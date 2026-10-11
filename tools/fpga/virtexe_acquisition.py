"""Offline acquisition-controller cuts using the existing Virtex-E decoder.

Reports contain private firmware-derived networks. Keep them outside Git.
Command qualifiers are named by coordinates, not presumed ACK semantics.
"""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import re
from pathlib import Path

from virtexe_mmio import functional_expression
from virtexe_routing import (COMMIT, Architecture, LogicAnalyzer, Router,
    compact_report, data_support, evaluate_data, expression_nodes, verify_checkout)
from virtexe_xc2s200e_decode import Xc2s200eBitstream


IMAGE_SHA256 = "227cba1ae543cf78b5b6d5700f719340e18df63850ec270eea47b5855c404337"
RX_PINS = ("B19", "B18", "B17", "E16", "D16", "B16", "D15", "B15",
           "D14", "B14", "D13", "B13", "C12")
FAST = "X24,Y29 BUFGCE[0].O"
CLOCK = "X24,Y0 BUFGCE[1].O"
RESET = "X40,Y4 SLICE[1].YQ"
V, Q = "X32,Y19 SLICE[1].YQ", "X29,Y19 SLICE[1].YQ"
E = "X24,Y16 SLICE[1].YQ"
D1, D2 = "X13,Y13 SLICE[1].YQ", "X13,Y13 SLICE[1].XQ"
H, P = "X13,Y13 SLICE[0].YQ", "X13,Y12 SLICE[0].YQ"
START = "X3,Y0 IOI[1].PAD_O"  # FG456 AA5, not the B8 pulse output.
GATES = {"MTTCTL": (0x080, ("X26,Y16 SLICE[1].X",)),
         "MTTRGO": (0x084, ("X27,Y18 SLICE[0].Y",)),
         "MAMRGO": (0x064, ("X26,Y16 SLICE[0].Y", "X28,Y17 SLICE[0].Y"))}
ADDRESS = dict(zip(("X26,Y17 SLICE[0].YQ", "X26,Y17 SLICE[0].XQ",
    "X26,Y19 SLICE[1].YQ", "X31,Y19 SLICE[0].YQ", "X31,Y19 SLICE[0].XQ",
    "X31,Y19 SLICE[1].YQ", "X26,Y18 SLICE[1].YQ", "X26,Y18 SLICE[1].XQ",
    "X26,Y19 SLICE[0].YQ"), range(2, 11)))
ADDRESS.update({"X26,Y19 SLICE[1].XQ": 5, "X25,Y17 SLICE[1].YQ": 6,
                "X25,Y17 SLICE[1].XQ": 7})
CA, CB = "X37,Y22 SLICE[1].X", "X38,Y19 SLICE[0].X"
LINK = "X42,Y26 SLICE[0].YQ"
FRAME, CAPTURE = "X44,Y25 SLICE[0].Y", "X44,Y26 SLICE[1].X"
FLAG = "X31,Y21 SLICE[0].XQ"
EVEN, ODD = "X41,Y21 SLICE[1].YQ", "X40,Y23 SLICE[0].YQ"
PARITY_COMPARE = "X43,Y22 SLICE[0].X"
PARITY_ERROR, ERROR = "X43,Y22 SLICE[0].YQ", "X44,Y16 SLICE[1].YQ"
RX_SELECTORS = ("X43,Y24 SLICE[0].XQ", "X43,Y24 SLICE[0].YQ", "X44,Y24 SLICE[0].YQ")
TX_PINS = ("U12", "AA13", "AA14", "W14", "V14", "Y15", "V15", "Y16",
           "V16", "Y17", "V17", "W18")
TX_FAST = "X24,Y0 BUFGCE[0].O"
TX_P, TX_E, TX_KICK = "X38,Y3 SLICE[0].XQ", "X38,Y4 SLICE[1].XQ", "X43,Y2 SLICE[0].YQ"
TX_P_COPIES = (TX_P, "X38,Y3 SLICE[1].YQ", "X38,Y3 SLICE[0].YQ")
TX_E_COPIES = (TX_E, "X38,Y3 SLICE[1].XQ", "X38,Y4 SLICE[1].YQ")
COMPARE = {"MTTRGO": "X10,Y18 SLICE[0].YB", "MAMRGO": "X15,Y22 SLICE[1].YB"}
TIMEBASE_SHA256 = "e3a679c4fa4304ca4d9e7387c099e1245d28c9db56b3d436f6e2a14095fed68b"
TB_START = "X0,Y12 IOI[3].I"  # Revision-B U38 N2; M1 is SACQ output.
TB_LEVEL = ("X2,Y7 SLICE[1].YQ", "X6,Y7 SLICE[0].YQ", "X6,Y3 SLICE[1].YQ")
TB_SYNC = ("X2,Y11 SLICE[1].YQ", "X2,Y11 SLICE[0].YQ", "X3,Y12 SLICE[1].YQ")
TB_TOGGLE = "X3,Y10 SLICE[0].YQ"


def intel_hex(raw):
    """Validate checksums/overlaps and preserve sparse MCU addresses."""
    memory, base, eof = {}, 0, False
    for line in raw.decode("ascii").splitlines():
        if not line.startswith(":") or eof:
            raise ValueError("invalid Intel HEX framing")
        record = bytes.fromhex(line[1:])
        if len(record) < 5 or len(record) != record[0] + 5 or sum(record) % 256:
            raise ValueError("invalid Intel HEX length/checksum")
        address, kind, data = int.from_bytes(record[1:3], "big"), record[3], record[4:-1]
        if kind == 0:
            for i, value in enumerate(data):
                if base + address + i in memory:
                    raise ValueError("overlapping Intel HEX address")
                memory[base + address + i] = value
        elif kind == 1 and address == 0 and not data:
            eof = True
        elif kind in (2, 4) and address == 0 and len(data) == 2:
            base = int.from_bytes(data, "big") << (4 if kind == 2 else 16)
        else:
            raise ValueError("unsupported Intel HEX record")
    if not eof:
        raise ValueError("missing Intel HEX EOF")
    return memory


def package_locations(text):
    blocks = re.findall(r"(?m)^bond BOND85 \{\s*(.*?)^\}", text, re.S)
    devices = re.findall(r"(?m)^device xc2s200e \{\s*(.*?)^\}", text, re.S)
    if len(blocks) != 1 or len(devices) != 1 or not re.search(
            r"\bbond fg456 = BOND85;", devices[0]):
        raise ValueError("expected pinned xc2s200e-fg456 BOND85")
    seen, locations = set(), {}
    for ball, site in re.findall(r"(?m)^\s*pin (\w+) = ([A-Z0-9_]+);\s*$", blocks[0]):
        if ball in seen:
            raise ValueError("duplicate FG456 ball")
        seen.add(ball)
        match = re.fullmatch(r"IOB_([NSEW])(\d+)_(\d+)", site)
        if not match:
            continue  # Dedicated GCK/configuration/power are not fabric IOI.
        side, coordinate, index = match.groups()
        coordinate, index = int(coordinate), int(index)
        x, y = {"W": (0, coordinate), "E": (47, coordinate),
                "S": (coordinate, 0), "N": (coordinate, 29)}[side]
        if not (0 <= x < 48 and 0 <= y < 30 and 0 <= index < 4):
            raise ValueError("invalid FG456 coordinate")
        locations[ball] = (x, y, index)
    return locations


def payload_bit(lane, delay):
    if not (0 <= lane < 12 and 0 <= delay < 3):
        raise ValueError("invalid lane/phase")
    if lane == 0 or (lane == 1 and delay == 0):
        return None
    return 2 - delay if lane == 1 else 3 * lane - 2 - delay


def command_frame(word):
    """Synthetic command-mode wire positions, not a hardware transmitter.

    A=0/B=1 are the qualified receive control fields. An address marker is
    payload bit31, not an extra 33rd payload bit. Parity follows PCI TX.
    """
    if not isinstance(word, int) or not 0 <= word <= 0xFFFFFFFF:
        raise ValueError("32-bit payload required")
    even = sum(bool(word & (1 << bit)) for bit in range(0, 32, 2)) % 2
    odd = 1 ^ (sum(bool(word & (1 << bit)) for bit in range(1, 32, 2)) % 2)
    return tuple(tuple(bool(word & (1 << bit)) if bit is not None else
                       bool((0, 1, even)[depth] if lane == 0 else odd)
                       for lane in range(12) for bit in [payload_bit(lane, depth)])
                 for depth in range(3))


def sampling_origins(report):
    """Track registered RX histories without presuming word acceptance."""
    logic = report["logic"]
    origin = {report["roots"][pin]["IQ"]: (lane, 0, ())
              for lane, pin in enumerate(RX_PINS)}
    for _ in range(5):
        changed = False
        for name, node in logic.items():
            if name in origin or node["kind"] != "register" or node["controls"].get(
                    "CLK", {}).get("source") != FAST:
                continue
            try:
                used = data_support(node["data"], logic)
                if len(used) != 1 or not used <= origin.keys():
                    continue
                source = next(iter(used))
                if evaluate_data(node["data"], logic, {source: False}) or not evaluate_data(
                        node["data"], logic, {source: True}):
                    continue
            except (KeyError, ValueError):
                continue
            lane, delay, enables = origin[source]
            ce = functional_expression(node["controls"]["CE"])
            if ce.get("constant") == 1 and not ce.get("inv", False):
                origin[name] = lane, delay + 1, enables
            else:
                origin[name] = lane, delay, enables + (json.dumps(ce, sort_keys=True),)
            changed = True
        if not changed:
            break
    return origin


def receiver_fields(report, include_combinational=False):
    """Recognize exact two-bank muxes; delays start at the registered pad.

    Combinational control A/B fields are optional. Parity/framing acceptance
    is a separate predicate, not implied by finding a payload mux.
    """
    logic, origin = report["logic"], sampling_origins(report)
    fields = {}
    for name, node in logic.items():
        if node["kind"] not in (("register", "combinational") if include_combinational else ("register",)):
            continue
        if node.get("controls", {}).get("CLK", {}).get("source") == FAST:
            continue
        try:
            used = data_support(node["data"], logic)
            banks, other = sorted(used & origin.keys()), sorted(used - origin.keys())
            if len(banks) != 2 or len(other) != 1:
                continue
            lane, delay, enables = origin[banks[0]]
            if origin[banks[1]][:2] != (lane, delay) or not enables or not origin[banks[1]][2]:
                continue
            if lane >= 12 or delay >= 3:
                continue
            selector = other[0]
            orientation = []
            for selected in (False, True):
                candidates = [bank for bank in banks if all(evaluate_data(node["data"], logic,
                    {selector: selected, **dict(zip(banks, values))}) == values[banks.index(bank)]
                    for values in itertools.product((False, True), repeat=2))]
                if len(candidates) != 1:
                    break
                orientation.append(candidates[0])
            if len(orientation) != 2 or len(set(orientation)) != 2:
                continue
            fields[name] = {"lane": lane, "delay": delay, "payload_bit": payload_bit(lane, delay),
                            "selector": selector, "banks_zero_one": orientation}
        except (KeyError, ValueError):
            continue
    return fields


def sequential_leaves(expression, logic):
    """Stop at registers, memory outputs and explicit unknown boundaries."""
    leaves, seen = set(), set()
    def visit(expr):
        for part in expression_nodes(expr):
            name = part.get("source")
            if name is None or name in seen:
                continue
            seen.add(name)
            node = logic.get(name, {})
            if node.get("kind") == "combinational":
                visit(node["data"])
            else:
                leaves.add(name)
    visit(expression)
    return leaves


def register_consumers(report, target):
    """Find actual sequential consumers through combinational logic only.

    Unlike a fully expanded leaf inventory, retain a selected carry/mux node
    as a boundary. This is connectivity evidence, not proof of sensitization.
    """
    logic = report["logic"]
    def uses(expression, seen):
        for part in expression_nodes(expression):
            name = part.get("source")
            if name == target:
                return True
            if name in seen or name not in logic:
                continue
            seen.add(name)
            if logic[name]["kind"] == "combinational" and uses(logic[name]["data"], seen):
                return True
        return False
    return {name: [part for part in ("data", "controls") if uses(node.get(part, {}), set())]
            for name, node in logic.items() if node["kind"] == "register"
            and any(uses(node.get(part, {}), set()) for part in ("data", "controls"))}


def transmit_fields(report):
    """Recover each actual TX mux cofactor, including registered snapshots.

    Uses the configured phase replicas consistently; their sequential
    equivalence must be checked separately. Does not identify a response ACK.
    """
    logic, fields = report["logic"], {}
    phases = (("A", True, True), ("B", False, True), ("C", True, False))
    copies = set(TX_P_COPIES + TX_E_COPIES)
    for lane, ball in enumerate(TX_PINS):
        output = report["roots"][ball]["O"]
        mux = logic[output]["data"].get("source")
        if mux is None or logic[mux]["kind"] != "register":
            raise ValueError("unresolved TX output register")
        expression = logic[mux]["data"]
        leaves = sorted(data_support(expression, logic) - copies)
        if len(leaves) != 3:
            raise ValueError("unexpected TX mux leaves")
        for phase, p, e in phases:
            fixed = {**dict.fromkeys(TX_P_COPIES, p), **dict.fromkeys(TX_E_COPIES, e)}
            candidates = [name for name in leaves if all(
                evaluate_data(expression, logic, {**fixed, **dict(zip(leaves, values))}) == values[leaves.index(name)]
                for values in itertools.product((False, True), repeat=3))]
            if len(candidates) != 1:
                raise ValueError("TX phase is not a single data source")
            source = candidates[0]
            word = source if phase == "C" else logic[source]["data"].get("source")
            if word is None or logic[word]["kind"] != "register":
                raise ValueError("unresolved controller TX word source")
            fields[lane, phase] = {"ball": ball, "output": output, "mux": mux,
                                   "phase_source": source, "word_register": word,
                                   "payload_bit": payload_bit(lane, "ABC".index(phase))}
    return fields


def comparison_pairs(report, command, bank):
    """Recognize equality LUT groups against a command's load bank.

    The final carry/mux predicate still requires independent whole-cone
    verification; finding local equalities alone is not a completion proof.
    """
    logic, pairs, seen = report["logic"], {}, set()
    bank_bits = {name: bit for bit, name in bank.items()}
    def visit(expr):
        for part in expression_nodes(expr):
            if part.get("op") == "lut4":
                leaves = data_support(part, logic)
                loads = sorted(leaves & bank_bits.keys())
                other = sorted(leaves - bank_bits.keys())
                if loads and len(loads) == len(other) and len(leaves) <= 4:
                    for ordered in itertools.permutations(other):
                        matching = list(zip(loads, ordered))
                        if all(evaluate_data(part, logic, dict(zip(sorted(leaves), values))) == all(
                            dict(zip(sorted(leaves), values))[left] == dict(zip(sorted(leaves), values))[right]
                            for left, right in matching)
                            for values in itertools.product((False, True), repeat=len(leaves))):
                            for left, right in matching:
                                bit = bank_bits[left]
                                if bit in pairs and pairs[bit] != right:
                                    raise ValueError("inconsistent comparison pairing")
                                pairs[bit] = right
            name = part.get("source")
            if name and name not in seen and logic[name]["kind"] == "combinational":
                seen.add(name)
                visit(logic[name]["data"])
    visit(logic[COMPARE[command]]["data"])
    if set(pairs) != set(bank):
        raise ValueError("incomplete comparison pairing")
    return pairs


def command_decode(report, gate, address):
    """Check consistent duplicate address cuts across all 2^11 assignments."""
    logic, expression = report["logic"], report["logic"][gate]["data"]
    used = data_support(expression, logic)
    if used - ADDRESS.keys() != {V, Q}:
        raise ValueError("unexpected command qualifier/boundary")
    for word in range(512):
        value = word << 2
        for v, q in itertools.product((False, True), repeat=2):
            inputs = {name: bool(value & (1 << bit)) for name, bit in ADDRESS.items()}
            inputs.update({V: v, Q: q})
            if evaluate_data(expression, logic, inputs) != (v and q and value == address):
                return False
    return True


def command_banks(report, fields):
    result = {}
    for command, (_, gates) in GATES.items():
        bank = {}
        for name, node in report["logic"].items():
            if node["kind"] != "register" or node["controls"].get("CE", {}).get("source") not in gates:
                continue
            source = node["data"].get("source")
            if node["data"].get("inv", False) or source not in fields:
                raise ValueError("command bank has unresolved payload")
            bit = fields[source]["payload_bit"]
            if bit is None or bit in bank:
                raise ValueError("command bank has nonpayload/duplicate bit")
            bank[bit] = name
        result[command] = bank
    return result


def symbolic_cut(report, registers):
    """Keep actual transitions; other sequential/architecture state is symbolic.

    The cut is an overapproximation, not full receiver/protocol reachability.
    """
    logic, pending, cut = report["logic"], list(registers), {}
    while pending:
        name = pending.pop()
        if name in cut:
            continue
        node = logic.get(name, {"kind": "architecture_boundary", "reason": "missing source"})
        if node["kind"] == "register" and name not in registers:
            cut[name] = {"kind": "architecture_boundary", "reason": "unconstrained sequential cut"}
        elif node["kind"] == "memory_output":
            cut[name] = {"kind": "architecture_boundary", "reason": "unconstrained runtime-memory output cut"}
        else:
            cut[name] = node
            if node["kind"] in ("register", "combinational", "tristate_driver"):
                for part in ("data", "controls", "disable"):
                    pending.extend(x["source"] for x in expression_nodes(node.get(part, {})) if "source" in x)
    return {"logic": cut, "memories": {}, "scope": "conditional sequential cut; not end-to-end DMA"}


def build_report(binary, checkout, adapter):
    source = verify_checkout(checkout)
    locations = package_locations((checkout / "databases/virtex.txt").read_text())
    architecture = Architecture(adapter, checkout / "databases/virtex.zstd")
    try:
        analyzer = LogicAnalyzer(Router(Xc2s200eBitstream(binary), architecture), 18000)
        roots, pads = {}, {}
        for ball, (x, y, index) in locations.items():
            attributes = analyzer.router.bel(x, y, f"IOB[{index}]")["attributes"]
            pads[ball] = {"location": (x, y, index), "attributes": attributes}
            if attributes.get("IBUF_MODE", {}).get("selections") != ["NONE"]:
                roots[ball] = {pin: analyzer.output({"x": x, "y": y, "bel": f"IOI[{index}]", "pin": pin})
                               for pin in ("I", "IQ")}
            if attributes.get("NDRIVE", {}).get("value") or attributes.get("PDRIVE", {}).get("value"):
                roots.setdefault(ball, {}).update({pin: analyzer.iob_output(x, y, index, pin) for pin in ("O", "T")})
        report = analyzer.report(roots)
        compact_report(report)
        report.update({"pads": pads, "image_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                       "project_combine_commit": COMMIT, "source": str(source),
                       "scope": "private configured acquisition image, not running firmware"})
        return report
    finally:
        architecture.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--project-combine", required=True, type=Path)
    parser.add_argument("--adapter", required=True, type=Path)
    parser.add_argument("--private-output", required=True, type=Path)
    args = parser.parse_args()
    output = args.private_output.resolve()
    if output.is_relative_to(Path(__file__).resolve().parents[2]):
        parser.error("full private network output must be outside the repository")
    report = build_report(args.binary, args.project_combine, args.adapter)
    output.write_text(json.dumps(report), encoding="utf-8")
    print(json.dumps({"image_sha256": report["image_sha256"], "logic_nodes": len(report["logic"]),
                      "boundaries": len(report["boundaries"]), "configured_only": True}))


if __name__ == "__main__":
    main()
