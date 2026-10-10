"""Focused offline PCI target pin/capture/readback analysis; no register guesses.

Consumes the existing configuration-aware router. Full reports contain private
firmware-derived networks and must not be committed or written into the repo.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

from virtexe_routing import (Architecture, LogicAnalyzer, Router, COMMIT,
                            data_support, evaluate_data, expression_nodes,
                            verify_checkout)
from virtexe_xc2s200e_decode import Xc2s200eBitstream, validate_knowns


# Derived electrical connectivity, not vendor schematic reproduction.
# Package positions are independently resolved from pinned BOND87 below.
AD_PINS = [58, 57, 56, 55, 49, 48, 47, 46, 45, 44, 43, 42, 17, 16, 15, 11,
           10, 9, 8, 7, 6, 5, 4, 3, 206, 205, 204, 203, 202, 201, 200, 199]
BOARD_PINS = {**{f"AD[{i}]": pin for i, pin in enumerate(AD_PINS)},
              **{f"CBE#[{i}]": pin for i, pin in enumerate((36, 35, 34, 33))},
              "IDSEL": 20, "DEVSEL#": 21, "FRAME#": 29, "IRDY#": 24,
              "TRDY#": 27, "STOP#": 23, "GNT#": 22, "REQ#": 18, "RST#": 198}
LINK_PINS = {**{f"TX_D{i}_{pol}": pin for i, pair in
                {0: (126, 125), 1: (123, 122), 2: (121, 120),
                 3: (116, 115), 4: (112, 111), 10: (133, 132)}.items()
                for pol, pin in zip(("P", "N"), pair)},
             **{f"RX_D{i}_{pol}": pin for i, pair in
                {9: (152, 151), 10: (147, 146), 11: (141, 140)}.items()
                for pol, pin in zip(("P", "N"), pair)},
             "RX_SYNC_P": 139, "RX_SYNC_N": 138}


def address_projection(report, name, address_signals, max_other_inputs=6):
    """Exhaustively factor an address predicate without tying off other state.

    Result covers all assignments to the support, including non-DWORD-aligned
    addresses. The gate-state cuts remain explicit; this is not a BAR claim.
    """
    expression, logic = report["logic"][name]["data"], report["logic"]
    used = data_support(expression, logic)
    address = sorted(used & address_signals.keys())
    other = sorted(used - address_signals.keys())
    if len(other) > max_other_inputs or len(address) > 10:
        raise ValueError("address projection exceeds bounded input budget")
    groups = {}
    for av in range(1 << len(address)):
        fixed = {n: bool(av & (1 << i)) for i, n in enumerate(address)}
        active = tuple(ov for ov in range(1 << len(other)) if evaluate_data(
            expression, logic, {**fixed, **{n: bool(ov & (1 << i))
                                           for i, n in enumerate(other)}}))
        if active:
            value = sum(int(fixed[n]) << address_signals[n] for n in address)
            groups.setdefault(active, []).append(value)
    return {"signal": name, "status": "Verified", "address_mask": sum(
                1 << address_signals[n] for n in address), "other_inputs": other,
            "cases": [{"other_assignments": list(key), "address_values": values}
                      for key, values in groups.items()],
            "semantics": "D-path predicate only; other input cuts are unconstrained"}


def cone_inventory(report, expression, expand_registers=False):
    """Collect paths and explicit boundaries, including all bus alternatives."""
    seen, boundaries, memories, pending = set(), [], set(), [expression]
    while pending:
        value = pending.pop()
        for item in expression_nodes(value):
            if "unknown" in item:
                boundaries.append({"reason": item["unknown"], "route": item.get("route")})
            if item.get("op") == "ram_read":
                memories.add(item["memory"])
            names = ([item["source"]] if "source" in item else []) + [
                d["driver"] for d in item.get("drivers", [])]
            for name in names:
                if name in seen:
                    continue
                seen.add(name)
                node = report["logic"].get(name, {})
                kind = node.get("kind")
                if kind in ("combinational", "tristate_driver") or (
                        expand_registers and kind == "register"):
                    pending.extend(node[k] for k in ("data", "disable", "controls") if k in node)
                elif kind not in ("register", "pad_input"):
                    boundaries.append({"signal": name, "reason": node.get("reason", "unmodeled source")})
    return {"signals": sorted(seen), "memories": sorted(memories), "boundaries": boundaries}


def package_locations(text, pins=BOARD_PINS):
    """Strictly parse the pinned public package block, not schematic text/OCR."""
    blocks = re.findall(r"(?m)^bond BOND87 \{\s*(.*?)^\}", text, re.S)
    if len(blocks) != 1:
        raise ValueError("expected exactly one xc2s200e-pq208 BOND87")
    entries = re.findall(r"(?m)^\s*pin P(\d+) = ([A-Z0-9_]+);\s*$", blocks[0])
    bond = {}
    for number, site in entries:
        if int(number) in bond:
            raise ValueError("duplicate package pin")
        bond[int(number)] = site
    result = {}
    for name, number in pins.items():
        match = re.fullmatch(r"IOB_([NSEW])(\d+)_(\d+)", bond.get(number, ""))
        if not match:
            raise ValueError(f"missing or non-IOB package pin: {name}/P{number}")
        side, coordinate, index = match.groups()
        coordinate, index = int(coordinate), int(index)
        x, y = {"W": (0, coordinate), "E": (47, coordinate),
                "S": (coordinate, 0), "N": (coordinate, 29)}[side]
        if not (0 <= x < 48 and 0 <= y < 30 and 0 <= index < 4):
            raise ValueError("invalid package coordinate")
        result[name] = {"package_pin": number, "x": x, "y": y, "index": index}
    return result


def signal(location, pin):
    return f"X{location['x']},Y{location['y']} IOI[{location['index']}].{pin}"


def support(expression, logic):
    try:
        return {"status": "Verified", "signals": sorted(data_support(expression, logic))}
    except (ValueError, KeyError) as exc:
        return {"status": "Unknown", "reason": str(exc)}


def functional_expression(value):
    """Exclude route provenance from functional equality/grouping keys."""
    if isinstance(value, dict):
        return {k: functional_expression(v) for k, v in value.items() if k != "route"}
    if isinstance(value, list):
        return [functional_expression(v) for v in value]
    return value


def capture_candidates(report, locations):
    """Group input-fed FFs by exact decoded clock/CE/SR, not assumed purpose."""
    aliases = {signal(loc, pin): net for net, loc in locations.items() for pin in ("I", "IQ")}
    groups = {}
    for name, node in report["logic"].items():
        if node.get("kind") != "register" or "IOI[" in name:
            continue
        cut = support(node["data"], report["logic"])
        nets = sorted({aliases[s] for s in cut.get("signals", []) if s in aliases})
        if not nets:
            continue
        controls = functional_expression(node.get("controls", {}))
        key = json.dumps(controls, sort_keys=True)
        group = groups.setdefault(key, {"controls": controls, "members": []})
        group["members"].append({"signal": name, "input_nets": nets, "data_support": cut,
                                 "classification": "Inferred",
                                 "reason": "Input-fed register candidate; address/data/command role not assigned"})
    return sorted(groups.values(), key=lambda g: (-len(g["members"]), json.dumps(g["controls"], sort_keys=True)))


def summarize(report, locations):
    outputs = {}
    for net, root in report["roots"].items():
        if not isinstance(root, dict) or "O" not in root:
            continue
        outputs[net] = {path: {"signal": root[path], "kind": report["logic"].get(root[path], {}).get("kind"),
                               "support": support(report["logic"].get(root[path], {}).get("data", {}), report["logic"])}
                        for path in ("O", "T")}
    groups = capture_candidates(report, locations)
    return {"project_combine_commit": COMMIT, "locations": locations,
            "capture_groups": groups, "output_cuts": outputs,
            "logic_nodes": len(report["logic"]), "boundaries": report["boundaries"],
            "interpretation": "Configured pin/capture structure only; no BAR, register semantic or idle claim"}


def analyze_target(router, locations, max_cells=10000):
    analyzer = LogicAnalyzer(router, max_cells)
    roots = {}
    for net, loc in locations.items():
        x, y, index = (loc[k] for k in ("x", "y", "index"))
        roots[net] = {p: analyzer.output({"x": x, "y": y, "bel": f"IOI[{index}]", "pin": p})
                      for p in ("I", "IQ")}
        if net.startswith(("AD[", "CBE#", "TX_")) or net in (
                "DEVSEL#", "TRDY#", "STOP#", "REQ#", "FRAME#", "IRDY#"):
            roots[net].update({p: analyzer.iob_output(x, y, index, p) for p in ("O", "T")})
    report = analyzer.report(roots)
    return report, summarize(report, locations)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path)
    parser.add_argument("--project-combine", type=Path, required=True)
    parser.add_argument("--adapter", type=Path, required=True)
    parser.add_argument("--max-logic", type=int, default=10000)
    parser.add_argument("--output", type=Path, required=True, help="private output, never commit")
    parser.add_argument("--summary", type=Path, required=True, help="private focused evidence, never commit")
    args = parser.parse_args()
    if args.max_logic < 1:
        parser.error("--max-logic must be positive")
    repo = Path(__file__).resolve().parents[2]
    for output in (args.output, args.summary):
        if output.resolve().is_relative_to(repo) or output.resolve() == args.image.resolve():
            parser.error("private report must be outside the repository and must not overwrite the image")
    if args.output.resolve() == args.summary.resolve():
        parser.error("network and summary require distinct paths")
    source = verify_checkout(args.project_combine)
    locations = package_locations((source / "databases/virtex.txt").read_text(encoding="utf-8"),
                                  {**BOARD_PINS, **LINK_PINS})
    image = Xc2s200eBitstream(args.image)
    calibration = validate_knowns(image)
    if calibration["status"] != "PASS":
        raise ValueError("private frame calibration failed")
    architecture = Architecture(args.adapter, source / "databases/virtex.zstd")
    try:
        report, summary = analyze_target(Router(image, architecture), locations, args.max_logic)
        report["frame_validation"] = calibration
        report["image_sha256"] = hashlib.sha256(args.image.read_bytes()).hexdigest()
        args.output.write_text(json.dumps(report, sort_keys=True) + "\n", encoding="utf-8")
        args.summary.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(json.dumps({"logic_nodes": summary["logic_nodes"], "capture_groups": len(summary["capture_groups"]),
                          "boundaries": len(summary["boundaries"]), "output": str(args.output)}))
    finally:
        architecture.close()


if __name__ == "__main__":
    main()
