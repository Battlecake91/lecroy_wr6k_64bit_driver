"""Configuration-aware upstream routing using the pinned native grid resolver."""
from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

COMMIT = "234343d23e737e57f2727630e19008b509d7d522"


def verify_checkout(source: Path):
    source = source.resolve()
    root = subprocess.check_output(["git", "-C", str(source), "rev-parse", "--show-toplevel"], text=True).strip()
    if Path(root).resolve() != source:
        raise ValueError("Project Combine path must be the Git checkout root")
    head = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
    if head != COMMIT:
        raise ValueError(f"Project Combine must be at {COMMIT}, got {head}")
    status = subprocess.check_output(["git", "-C", str(source), "status", "--porcelain"], text=True)
    if status.strip():
        raise ValueError("Project Combine checkout must be clean")
    return source


def build_adapter(source: Path, offline: bool = True) -> Path:
    source = verify_checkout(source)
    rust = Path(__file__).with_name("prjcombine_graph.rs")
    key = hashlib.sha256(rust.read_bytes() + str(source).encode()).hexdigest()[:16]
    build = Path(tempfile.gettempdir()) / f"wr6k-routing-{key}"
    build.mkdir(exist_ok=True)
    shutil.copyfile(rust, build / "main.rs")
    # Rust 1.89 compatibility, isolated from the pinned Git checkout. Neither
    # change affects device definitions, routing, bit geometry or serialization.
    public = build / "public"
    shutil.copytree(source / "public", public, dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns("target"))
    replacements = {
        "emit.rs": ("stream.extend([Punct::new(';', Spacing::Alone)]);",
                    "stream.extend([TokenTree::Punct(Punct::new(';', Spacing::Alone))]);"),
        "eval.rs": ("n.strict_add_signed(*offset)",
                    'n.checked_add_signed(*offset).expect("template index overflow")'),
    }
    for filename, (old, new) in replacements.items():
        path = public / "tablegen" / "src" / filename
        content = path.read_text(encoding="utf-8")
        if content.count(old) != 1:
            raise ValueError(f"unexpected pinned tablegen compatibility site: {filename}")
        path.write_text(content.replace(old, new), encoding="utf-8")
    manifest = ['[package]', 'name = "wr6k-routing"', 'version = "0.1.0"', 'edition = "2024"',
                '[[bin]]', 'name = "wr6k-routing"', 'path = "main.rs"', '[dependencies]', 'serde_json = "1"']
    for crate in ("entity", "interconnect", "types", "virtex", "xilinx-bitstream"):
        path = (public / crate).as_posix()
        manifest.append(f'prjcombine-{crate} = {{ path = {json.dumps(path)} }}')
    (build / "Cargo.toml").write_text("\n".join(manifest) + "\n", encoding="utf-8")
    lock = Path(__file__).with_name("Cargo.lock")
    if lock.exists():
        shutil.copyfile(lock, build / "Cargo.lock")
    command = ["cargo", "build", "--manifest-path", str(build / "Cargo.toml")]
    if lock.exists():
        command.append("--locked")
    if offline:
        command.append("--offline")
    subprocess.run(command, check=True)
    exe = build / "target" / "debug" / ("wr6k-routing.exe" if os.name == "nt" else "wr6k-routing")
    return exe


class Architecture:
    def __init__(self, executable: Path, database: Path):
        self.process = subprocess.Popen([str(executable), str(database)], stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, text=True, encoding="utf-8")
        self.cache = {}

    def query(self, **query):
        key = json.dumps(query, sort_keys=True)
        if key not in self.cache:
            self.process.stdin.write(key + "\n")
            self.process.stdin.flush()
            response = self.process.stdout.readline()
            if not response:
                raise RuntimeError("Project Combine adapter exited without a response")
            value = json.loads(response)
            if "error" in value:
                raise ValueError(value["error"] + ": " + key)
            self.cache[key] = value
        return self.cache[key]

    def close(self):
        self.process.stdin.close()
        try:
            self.process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.process.stdout.close()


def read_bit(image, bit):
    if "unresolved" in bit or not (0 <= bit.get("frame", -1) < len(image.frames)):
        raise ValueError(f"bit outside decoded main frames: {bit}")
    if not (0 <= bit.get("bit", -1) < len(image.frames[bit["frame"]])):
        raise ValueError(f"bit outside decoded frame: {bit}")
    return bool(image.frames[bit["frame"]][bit["bit"]]) ^ bit.get("inv", False)


def classify_pip(image, pip):
    result = dict(pip)
    configs = pip["config"]
    if len(configs) != 1:
        result.update(status="unknown", reason="missing or ambiguous architecture encoding")
        return result
    config = configs[0]
    try:
        actual = [read_bit(image, b) for b in config["bits"]]
        if len(actual) != len(config["expected"]):
            raise ValueError("encoding length mismatch")
        result["actual"] = actual
        result["status"] = "active" if actual == config["expected"] else "disabled"
        if "inversion_bit" in config:
            result["inv"] = result.get("inv", False) ^ read_bit(image, config["inversion_bit"])
    except ValueError as exc:
        result.update(status="unknown", reason=str(exc))
    return result


def decode_bel(image, bel):
    result = dict(bel)
    result["attributes"] = {}
    for name, attr in bel["attributes"].items():
        try:
            actual = [read_bit(image, b) for b in attr["bits"]]
            decoded = {"bits": attr["bits"], "actual": actual,
                       "value": sum(int(b) << i for i, b in enumerate(actual))}
            if "values" in attr:
                decoded["selections"] = [n for n, v in attr["values"].items() if v == actual]
                decoded["status"] = "verified" if len(decoded["selections"]) == 1 else "unknown"
            result["attributes"][name] = decoded
        except ValueError as exc:
            result["attributes"][name] = {"status": "unknown", "reason": str(exc)}
    result["inputs"] = {}
    for name, pin in bel["inputs"].items():
        decoded = dict(pin)
        if pin["inversion_bit"]:
            decoded["inv"] ^= read_bit(image, pin["inversion_bit"])
        result["inputs"][name] = decoded
    return result


class Router:
    def __init__(self, image, architecture):
        self.image = image
        self.architecture = architecture
        self.bus_rows = {}

    def bus_row(self, y):
        if y not in self.bus_rows:
            row = self.architecture.query(x=0, y=y, bus_row=True)
            self.bus_rows[y] = dict(row, columns=[dict(c,
                bus=decode_bel(self.image, c["bus"]),
                tbufs=[decode_bel(self.image, b) for b in c["tbufs"]]) for c in row["columns"]])
        return self.bus_rows[y]

    def inspect(self, signal):
        x, y, name = signal
        node = dict(self.architecture.query(x=x, y=y, wire=name))
        node["pips"] = [classify_pip(self.image, p) for p in node["pips"]]
        node["disabled_terminals"] = []
        enabled = []
        for terminal in node["terminals"]:
            if terminal["bel"].startswith("IOI[") and terminal["pin"] == "I":
                pad = self.bel(terminal["x"], terminal["y"],
                               terminal["bel"].replace("IOI[", "IOB["))
                mode = pad["attributes"].get("IBUF_MODE", {})
                if mode.get("selections") == ["NONE"]:
                    node["disabled_terminals"].append({"terminal": terminal, "IBUF_MODE": mode})
                    continue
            enabled.append(terminal)
        node["terminals"] = enabled
        return node

    def trace(self, signal, max_nodes=500, max_depth=100):
        if max_nodes < 1 or max_depth < 0:
            raise ValueError("positive node limit and nonnegative depth required")
        nodes, cycles, limits = {}, [], []
        requested_signal = list(signal)
        pending = [(requested_signal, [], 0)]
        while pending:
            signal, ancestors, depth = pending.pop()
            key = json.dumps(signal)
            if key in ancestors:
                cycles.append(ancestors[ancestors.index(key):] + [key])
                continue
            if key in nodes:
                continue
            if depth > max_depth or len(nodes) >= max_nodes:
                limits.append(signal)
                continue
            node = self.inspect(signal)
            nodes[key] = node
            active = [p for p in node["pips"] if p["status"] == "active"]
            # Bidirectional switches can expose several active paths to one producer.
            # Report candidate fan-in here; resolve terminal ambiguity after traversal.
            node["active_fanin"] = len({tuple(p["source"]) for p in active})
            for pip in reversed(active):
                pending.append((pip["source"], ancestors + [key], depth + 1))
        terminals = {json.dumps(t, sort_keys=True): t for n in nodes.values() for t in n["terminals"]}
        unknown = [key for key, n in nodes.items() if any(p["status"] == "unknown" for p in n["pips"])]
        leaves = [n["root"] for n in nodes.values() if not n["terminals"] and
                  not any(p["status"] == "active" for p in n["pips"])]
        constants = [r for r in leaves if r and r[2] == "PULLUP"]
        inactive = [n["root"] for n in nodes.values() if n.get("disabled_terminals") and
                    not n["terminals"] and not any(p["status"] == "active" for p in n["pips"])]
        unresolved = [r for r in leaves if r not in constants and r not in inactive]
        polarities, seen = {}, set()
        pending = [(json.dumps(requested_signal), False)]
        while pending:
            key, inv = pending.pop()
            if (key, inv) in seen or key not in nodes:
                continue
            seen.add((key, inv))
            node = nodes[key]
            for terminal in node["terminals"]:
                identity = json.dumps(terminal, sort_keys=True)
                polarities.setdefault(identity, set()).add(inv)
            if node["root"] in constants:
                polarities.setdefault("PULLUP", set()).add(inv)
            for pip in node["pips"]:
                if pip["status"] == "active":
                    pending.append((json.dumps(pip["source"]), inv ^ pip.get("inv", False)))
        ambiguous = len(terminals) + bool(constants) > 1 or any(len(v) > 1 for v in polarities.values())
        resolved = not (limits or unknown or unresolved) and bool(terminals or constants) and not ambiguous
        return {"signal": requested_signal, "nodes": nodes, "terminals": list(terminals.values()),
                "cycles": cycles, "limits": limits, "unknown_pips": unknown,
                "constants": constants, "inactive_sources": inactive, "unresolved": unresolved,
                "driver_polarities": {k: sorted(v) for k, v in sorted(polarities.items())},
                "inversion": next(iter(next(iter(polarities.values())))) if resolved else None,
                "driver_status": "ambiguous" if ambiguous else "resolved" if resolved else "unknown"}

    def bel(self, x, y, name):
        return decode_bel(self.image, self.architecture.query(x=x, y=y, bel=name))


def validate_architecture(router):
    """Independent native-database checks; requires the local private image."""
    checks = []
    for x, y, name, attr, expected in (
        (2, 14, "SLICE[1]", "G", 0xF0FF), (2, 12, "SLICE[0]", "G", 0xFFFA),
        (2, 3, "SLICE[0]", "G", 0xAFAF), (2, 14, "SLICE[0]", "F", 0xDDDD),
        (2, 14, "SLICE[0]", "G", 0xD0F1),
        (0, 18, "IOI[2]", "MUX_O", "FFO"), (0, 18, "IOI[2]", "MUX_T", "FFT"),
        (0, 16, "IOI[2]", "MUX_O", "O"), (0, 16, "IOI[2]", "MUX_T", "T"),
    ):
        value = router.bel(x, y, name)["attributes"][attr]
        actual = value.get("selections", [None])[0] if isinstance(expected, str) else value.get("value")
        checks.append({"feature": f"X{x},Y{y} {name}.{attr}", "actual": actual,
                       "expected": expected, "pass": actual == expected})
    for x, y, name, source, field in (
        (0, 13, "IMUX_PCI_I1", "HEX_V5[3]", "1010000"),
        (0, 13, "IMUX_PCI_I2", "HEX_V1[3]", "1000001"),
        (0, 13, "IMUX_PCI_I3", "HEX_V4[1]", "0001"),
        (2, 14, "OMUX[0]", "OUT_CLB_XQ[0]", "0011011"),
        (2, 14, "OMUX[7]", "OUT_CLB_XQ[0]", "0011011"),
        (0, 26, "IMUX_IO_OCE[1]", "PCI_CE", None),
    ):
        active = [p for p in router.inspect([x, y, name])["pips"] if p["status"] == "active"]
        actual = [p["source_raw"][2] for p in active]
        raw = "".join(str(int(v)) for v in reversed(active[0]["actual"])) if len(active) == 1 else None
        checks.append({"feature": f"X{x},Y{y} {name}", "actual": actual, "field": raw,
                       "expected": source, "pass": actual == [source] and (field is None or raw == field)})
    pin = router.bel(2, 14, "SLICE[0]")["inputs"]["BX"]
    checks.append({"feature": "X2,Y14 SLICE[0].BX inversion", "actual": pin["inv"],
                   "expected": False, "pass": pin["inv"] is False})
    for name, expected in (
        ("F1", (2, 14, "SLICE[0]", "XQ")),
        ("F2", (3, 16, "SLICE[1]", "X")),
        ("G1", (2, 15, "SLICE[0]", "XQ")),
        ("G2", (2, 14, "SLICE[0]", "XQ")),
        ("G3", (3, 16, "SLICE[1]", "X")),
        ("G4", (0, 15, "IOI[3]", "I")),
        ("BX", (0, 13, "IOI[2]", "I")),
        ("SR", (12, 29, "IOI[1]", "I")),
        ("CLK", (24, 29, "BUFGCE[1]", "O")),
    ):
        trace = router.trace([2, 14, f"IMUX_CLB_{name}[0]"])
        actual = [tuple(t[k] for k in ("x", "y", "bel", "pin")) for t in trace["terminals"]]
        checks.append({"feature": f"X2,Y14 SLICE[0].{name} upstream",
                       "actual": actual, "expected": expected,
                       "pass": trace["driver_status"] == "resolved" and actual == [expected] and trace["inversion"] is False})
    for x, y, name, attr, expected in (
        (8, 27, "SLICE[1]", "YBMUX", "GCY"), (8, 27, "SLICE[1]", "CYINIT", "CIN"),
        (8, 27, "SLICE[1]", "CY0", "CONST_0"), (8, 27, "SLICE[1]", "CYSELF", "F"),
        (8, 27, "SLICE[1]", "CYSELG", "G"), (6, 26, "SLICE[1]", "CYSELG", "CONST_1"),
        (20, 10, "SLICE[0]", "FXMUX", "FXOR"), (20, 10, "SLICE[0]", "GYMUX", "GXOR"),
        (20, 10, "SLICE[0]", "CYINIT", "BX"), (20, 10, "SLICE[0]", "CY0", "CONST_1"),
    ):
        value = router.bel(x, y, name)["attributes"][attr]
        checks.append({"feature": f"X{x},Y{y} {name}.{attr}", "actual": value,
                       "expected": expected, "pass": value.get("selections") == [expected]})
    carry = router.bel(8, 27, "SLICE[1]").get("dedicated", {}).get("CIN")
    expected = {"x": 8, "y": 26, "bel": "SLICE[1]", "pin": "COUT"}
    checks.append({"feature": "dedicated CIN", "actual": carry, "expected": expected, "pass": carry == expected})
    row = router.bus_row(2)
    fixed = row["fixed"]
    topology = all(edge in fixed for edge in ([[0, 4], [2, 0]], [[13, 4], [15, 0]],
                                              [[32, 4], [34, 0]], [[45, 4], [47, 0]]))
    checks.append({"feature": "TBUS west/BRAM skips", "pass": topology,
                   "expected": "one lane rotation across each skipped BRAM column"})
    for x, expected in ((4, [(2, 0), (3, 1), (4, 0), (5, 1), (6, 0), (7, 1), (8, 0), (10, 0), (11, 1), (12, 0), (13, 1)]),
                        (7, [(2, 1), (3, 0), (4, 1), (5, 0), (6, 1), (7, 0), (8, 1), (10, 1), (11, 0), (12, 1), (13, 0)])):
        component = bus_component(row, (x, 2))
        actual = [(d["terminal"]["x"], int(d["terminal"]["bel"][5:-1])) for d in component["drivers"]]
        checks.append({"feature": f"X{x},Y2 TBUS configured taps", "actual": actual, "expected": expected,
                       "pass": actual == expected and "unknown" not in component})
    ram = router.bel(26, 11, "SLICE[0]")["attributes"]
    for name, expected in {"F_RAM_ENABLE": 1, "G_RAM_ENABLE": 1, "F_SHIFT_ENABLE": 0,
                           "G_SHIFT_ENABLE": 0, "WA4_ENABLE": 0, "DIF_MUX": "BY", "F": 0, "G": 0}.items():
        attr = ram[name]
        actual = attr.get("selections", [None])[0] if isinstance(expected, str) else attr.get("value")
        checks.append({"feature": "X26,Y11 SLICE[0]." + name, "actual": attr,
                       "expected": expected, "pass": actual == expected})
    return {"kind": "private_firmware_native_architecture", "checks": checks,
            "status": "PASS" if all(c["pass"] for c in checks) else "FAIL"}


def lut_support(value):
    return [i for i in range(4) if any(((value >> address) ^ (value >> (address ^ (1 << i)))) & 1
                                     for address in range(16))]


def bus_component(row, origin):
    """Dedicated lanes from the native verifier; never merge I/T into the bus."""
    columns = {c["x"]: c for c in row["columns"]}
    if origin[0] not in columns:
        return {"unknown": "dedicated bus BEL unavailable", "drivers": []}
    edges, gate_evidence = {}, []

    def connect(a, b):
        a, b = tuple(a), tuple(b)
        edges.setdefault(a, set()).add(b)
        edges.setdefault(b, set()).add(a)

    for a, b in row["fixed"]:
        connect(a, b)
    for gate in row["joiners"]:
        attr = columns[gate["owner"]]["bus"]["attributes"].get(gate["attribute"], {})
        value = attr.get("value")
        gate_evidence.append(dict(gate, configuration=attr))
        if value != 0:  # Include unknown connections conservatively, with an explicit boundary.
            connect(gate["a"], gate["b"])
    visited, pending = set(), [tuple(origin)]
    while pending:
        node = pending.pop()
        if node in visited:
            continue
        visited.add(node)
        pending.extend(edges.get(node, ()))
    gates = [g for g in gate_evidence if tuple(g["a"]) in visited or tuple(g["b"]) in visited]
    unknown = ["unavailable joiner encoding" for g in gates if g["configuration"].get("value") not in (0, 1)]
    drivers = []
    for x, column in sorted(columns.items()):
        for index, b in enumerate(column["tbufs"]):
            taps = []
            for name, lane in (("OUT_A", index), ("OUT_B", index + 2)):
                attr = b["attributes"].get(name, {})
                if (x, lane) not in visited:
                    continue
                if attr.get("value") not in (0, 1):
                    unknown.append("unavailable TBUF tap encoding")
                if attr.get("value") != 0:
                    taps.append({"lane": lane, "attribute": name, "configuration": attr})
            if taps:
                drivers.append({"terminal": {"x": x, "y": b["y"], "bel": b["bel"], "pin": "O"}, "taps": taps})
    result = {"members": sorted(visited), "joiners": gates, "drivers": drivers,
              "evidence": row["evidence"],
              "joiner_contract": "bidirectional single-switch model; reverse PIP claimed by pinned rdverify"}
    if unknown:
        result["unknown"] = "; ".join(sorted(set(unknown)))
    return result


class LogicAnalyzer:
    """Emit a bounded register/combinational network without assigning PCI states."""
    def __init__(self, router, max_cells=200):
        self.router, self.max_cells = router, max_cells
        self.logic, self.routes, self.boundaries = {}, {}, []
        self.memories = {}
        self.pending = []

    def input(self, pin):
        if pin["wire"] is None:
            return {"unknown": "blackhole architecture wire"}
        key = json.dumps(pin["wire"])
        if key not in self.routes:
            self.routes[key] = self.router.trace(pin["wire"])
        route = self.routes[key]
        result = {"route": key, "inv": pin.get("inv", False) ^ bool(route.get("inversion"))}
        if route["driver_status"] != "resolved":
            detail = {k: route[k] for k in ("unresolved", "limits", "unknown_pips", "terminals") if route.get(k)}
            result["unknown"] = route["driver_status"] + ": " + json.dumps(detail, sort_keys=True)
            self.boundaries.append({"route": key, "reason": result["unknown"]})
        elif route["constants"]:
            result["constant"] = 1
        else:
            result["source"] = self.output(route["terminals"][0])
        return result

    def output(self, terminal):
        x, y, name, pin = (terminal[k] for k in ("x", "y", "bel", "pin"))
        key = f"X{x},Y{y} {name}.{pin}"
        if key in self.logic:
            return key
        if len(self.logic) >= self.max_cells:
            self.boundaries.append({"signal": key, "reason": "logic node limit"})
            return key
        self.logic[key] = {"kind": "pending"}
        self.pending.append((terminal, key))
        return key

    def populate(self, terminal, key):
        x, y, name, pin = (terminal[k] for k in ("x", "y", "bel", "pin"))
        bel = self.router.bel(x, y, name)
        attrs, inputs = bel["attributes"], bel["inputs"]
        entry = self.logic[key]
        entry["configuration"] = attrs
        if name.startswith("SLICE["):
            entry["dedicated"] = bel.get("dedicated", {})
            self.slice(entry, pin, attrs, inputs, terminal, bel.get("dedicated", {}))
        elif name in ("TBUS", "TBUS_WE"):
            lane = 2 if name == "TBUS" and pin == "OUT" else int(pin[3:]) if pin.startswith("BUS") else None
            if lane not in (0, 1, 2, 3):
                entry.update(kind="architecture_boundary", reason="invalid dedicated bus tap")
                self.boundaries.append({"signal": key, "reason": entry["reason"]})
                return
            component = bus_component(self.router.bus_row(y), (x, lane))
            entry.update(kind="combinational", dedicated=component,
                         data={"op": "bus", "drivers": [{"driver": self.output(d["terminal"])}
                               for d in component["drivers"]]})
            if "unknown" in component:
                entry["data"]["unknown"] = component["unknown"]
        elif name.startswith("TBUF[") and pin == "O":
            entry.update(kind="tristate_driver", data=self.input(inputs["I"]),
                         disable=self.input(inputs["T"]))
        elif name.startswith("IOI["):
            try:
                pad = self.router.bel(x, y, name.replace("IOI[", "IOB["))["attributes"]
            except ValueError:
                pad = {}
            entry["pad_configuration"] = pad
            if pad.get("IBUF_MODE", {}).get("selections", []) not in (["CMOS"], ["VREF"], ["DIFF"]):
                entry.update(kind="architecture_boundary", reason="pad input disabled or unverified")
                self.boundaries.append({"signal": key, "reason": entry["reason"]})
                return
            if pin == "I":
                entry.update(kind="pad_input", pad=[x, y, name])
            elif pin == "IQ":
                entry.update(kind="register", data={"source": self.output(dict(terminal, pin="I"))},
                             controls={p: self.input(inputs[p]) for p in ("ICLK", "ICE", "SR")},
                             mode={n: a for n, a in attrs.items() if n.startswith("FFI_")})
            else:
                entry.update(kind="unknown", reason="unsupported IOB output")
        else:
            entry.update(kind="architecture_boundary", reason="hard block semantics not modeled",
                         inputs={n: self.input(p) for n, p in inputs.items()})
            self.boundaries.append({"signal": key, "reason": entry["reason"]})

    def slice(self, entry, pin, attrs, inputs, terminal, dedicated):
        def selection(name):
            values = attrs.get(name, {}).get("selections", [])
            return values[0] if len(values) == 1 else None

        def lut(letter):
            if (all(attrs.get(n, {}).get("value") == v for n, v in {
                    "F_RAM_ENABLE": 1, "G_RAM_ENABLE": 1, "F_SHIFT_ENABLE": 0,
                    "G_SHIFT_ENABLE": 0, "WA4_ENABLE": 0}.items()) and selection("DIF_MUX") == "BY"):
                memory = f"X{terminal['x']},Y{terminal['y']} {terminal['bel']}.RAM16X1D"
                if attrs["F"]["value"] != attrs["G"]["value"]:
                    return {"unknown": "dual-port RAM INIT copies disagree"}
                if memory not in self.memories:
                    self.memories[memory] = {"kind": "ram16x1d", "initial": attrs["F"]["value"],
                        "semantics": "INIT is power-up configuration, not current contents; no SR clear. Effective SR is write enable.",
                        "evidence": "pinned ise-hammer/virtex/clb.rs RAMCONFIG and v2xdl-verify/clb_lut4.rs::gen_ram16d",
                        "configuration": attrs}
                    self.memories[memory].update(
                        write_address={str(i): self.input(inputs[f"G{i + 1}"]) for i in range(4)},
                        data=self.input(inputs["BY"]), enable=self.input(inputs["SR"]), clock=self.input(inputs["CLK"]))
                return {"op": "ram_read", "memory": memory,
                        "address": {str(i): self.input(inputs[f"{letter}{i + 1}"]) for i in range(4)}}
            if any(attrs.get(letter + suffix, {}).get("value", 1) for suffix in ("_RAM_ENABLE", "_SHIFT_ENABLE")):
                return {"unknown": f"{letter} is configured as RAM or shift register"}
            value = attrs[letter]["value"]
            if value in (0, 0xFFFF):
                return {"constant": int(value == 0xFFFF)}
            return {"op": "lut4", "init": value,
                    "inputs": {str(i): self.input(inputs[f"{letter}{i + 1}"]) for i in lut_support(value)}}

        def internal(pin):
            return {"source": self.output(dict(terminal, pin=pin)), "inv": False}

        def carry_input():
            mode = selection("CYINIT")
            if mode == "BX":
                return self.input(inputs["BX"])
            if mode == "CIN" and dedicated.get("CIN"):
                return {"source": self.output(dedicated["CIN"]), "inv": False}
            return {"unknown": "dedicated CIN unavailable or CYINIT invalid"}

        def carry(letter):
            ci = internal("CI" if letter == "F" else "FCY")
            select = selection("CYSEL" + letter)
            if select == "CONST_1":
                return ci
            if select != letter:
                return {"unknown": "invalid CYSEL" + letter}
            s = lut(letter)
            if s.get("constant") == 1:
                return ci
            mode = selection("CY0")
            if mode in ("CONST_0", "CONST_1"):
                di = {"constant": int(mode == "CONST_1")}
            elif mode == "F1_G1":
                di = self.input(inputs[letter + "1"])
            elif mode == "PROD":
                di = {"op": "and", "args": [self.input(inputs[letter + n]) for n in ("1", "2")]}
            else:
                return {"unknown": "invalid CY0"}
            return di if s.get("constant") == 0 else {"op": "mux", "select": s, "zero": di, "one": ci}

        def combinational(axis):
            mode = selection("FXMUX" if axis == "X" else "GYMUX")
            if mode == "F":
                return lut("F")
            if mode == "G":
                return lut("G")
            if mode == "F5":
                return {"op": "mux", "select": self.input(inputs["BX"]), "zero": lut("G"), "one": lut("F")}
            if mode in ("FXOR", "GXOR"):
                return {"op": "xor", "args": [lut(mode[0]), internal("CI" if axis == "X" else "FCY")]}
            return {"unknown": f"unmodeled {axis} data path {mode}"}

        if pin in ("CI", "FCY", "GCY", "COUT", "XB", "YB"):
            if pin == "CI":
                data = carry_input()
            elif pin in ("FCY", "XB"):
                data = carry("F")
            elif pin in ("GCY", "COUT"):
                data = carry("G")
            elif selection("YBMUX") == "BY":
                data = self.input(inputs["BY"])
            elif selection("YBMUX") == "GCY":
                data = internal("GCY")
            else:
                data = {"unknown": "invalid YBMUX"}
            entry.update(kind="combinational", data=data)
        elif pin in ("X", "Y"):
            entry.update(kind="combinational", data=combinational(pin))
        elif pin in ("XQ", "YQ"):
            axis = pin[0]
            mode = selection("D" + axis + "MUX")
            data = self.input(inputs[mode]) if mode in ("BX", "BY") else combinational(axis) if mode == axis else {"unknown": "D mux"}
            entry.update(kind="register", data=data,
                         controls={p: self.input(inputs[p]) for p in ("CLK", "CE", "SR")},
                         mode={n: a for n, a in attrs.items() if n.startswith("FF")})
        else:
            entry.update(kind="architecture_boundary", reason=f"unmodeled slice output {pin}")
            self.boundaries.append({"reason": entry["reason"]})

    def iob_output(self, x, y, index, path):
        name = f"IOI[{index}]"
        key = f"X{x},Y{y} {name}.PAD_{path}"
        bel = self.router.bel(x, y, name)
        attrs, inputs = bel["attributes"], bel["inputs"]
        select = attrs["MUX_" + path].get("selections", [])
        pad = self.router.bel(x, y, f"IOB[{index}]")["attributes"]
        if pad.get("MUX_" + path, {}).get("selections", []) != select:
            self.logic[key] = {"kind": "architecture_boundary", "reason": "IOI/IOB mux disagreement",
                               "configuration": attrs, "pad_configuration": pad}
            self.boundaries.append({"signal": key, "reason": "IOI/IOB mux disagreement"})
            return key
        if select == ["FF" + path]:
            clock, enable = path + "CLK", path + "CE"
            self.logic[key] = {"kind": "register", "configuration": attrs,
                               "data": self.input(inputs[path]),
                               "controls": {p: self.input(inputs[p]) for p in (clock, enable, "SR")},
                               "mode": {n: a for n, a in attrs.items() if n.startswith("FF" + path + "_")}}
        elif select == [path]:
            self.logic[key] = {"kind": "combinational", "data": self.input(inputs[path]), "configuration": attrs}
        else:
            self.logic[key] = {"kind": "unknown", "reason": "invalid IOB output mux"}
        self.logic[key]["pad_configuration"] = pad
        return key

    def report(self, roots):
        index = 0
        while index < len(self.pending):
            terminal, key = self.pending[index]
            self.populate(terminal, key)
            index += 1
        for name, node in self.logic.items():
            for value in expression_nodes({k: node.get(k, {}) for k in ("data", "controls", "disable", "inputs")}):
                if "unknown" in value:
                    self.boundaries.append({"signal": name, "reason": value["unknown"]})
        return {"project_combine_commit": COMMIT, "roots": roots, "logic": self.logic,
                "routes": self.routes, "boundaries": self.boundaries, "memories": self.memories,
                "interpretation": "Configured logic network; protocol states and DMA quiescence are not assumed."}


def expression_nodes(value):
    pending = [value]
    while pending:
        node = pending.pop()
        if isinstance(node, dict):
            yield node
            pending.extend(node.values())
        elif isinstance(node, list):
            pending.extend(node)


def compact_report(result):
    """Keep every active/unknown PIP's evidence while omitting disabled candidates."""
    if isinstance(result, dict):
        if "pips" in result:
            result["disabled_pip_count"] = sum(p["status"] == "disabled" for p in result["pips"])
            result["pips"] = [p for p in result["pips"] if p["status"] != "disabled"]
        for value in result.values():
            compact_report(value)
    elif isinstance(result, list):
        for value in result:
            compact_report(value)
    return result


def evaluate_data(expression, logic, values, stack=()):
    """Evaluate configured combinational data; registers and pads are variables."""
    if "unknown" in expression:
        raise ValueError(expression["unknown"])
    if "constant" in expression:
        value = bool(expression["constant"])
    elif "source" in expression:
        source = expression["source"]
        if source in values:
            value = bool(values[source])
        else:
            if source in stack:
                raise ValueError("combinational cycle: " + source)
            node = logic.get(source, {})
            if node.get("kind") != "combinational":
                raise ValueError("unassigned or unresolved source: " + source)
            value = evaluate_data(node["data"], logic, values, stack + (source,))
    elif expression.get("op") == "mux":
        select = evaluate_data(expression["select"], logic, values, stack)
        value = evaluate_data(expression["one" if select else "zero"], logic, values, stack)
    elif expression.get("op") == "lut4":
        address = sum(int(evaluate_data(pin, logic, values, stack)) << int(index)
                      for index, pin in expression["inputs"].items())
        value = bool(expression["init"] & (1 << address))
    elif expression.get("op") == "ram_read":
        memory = expression["memory"]
        if memory not in values or not isinstance(values[memory], int) or not 0 <= values[memory] <= 0xFFFF:
            raise ValueError("runtime memory contents unavailable: " + memory)
        address = sum(int(evaluate_data(pin, logic, values, stack)) << int(i)
                      for i, pin in expression["address"].items())
        value = bool(values[memory] & (1 << address))
    elif expression.get("op") == "xor":
        value = False
        for arg in expression["args"]:
            value ^= evaluate_data(arg, logic, values, stack)
    elif expression.get("op") == "and":
        value = all(evaluate_data(arg, logic, values, stack) for arg in expression["args"])
    elif expression.get("op") == "bus":
        resolved = resolve_bus(expression, logic, values, stack)
        if resolved["status"] not in ("driven", "multiple_drivers_agree"):
            raise ValueError("bus " + resolved["status"] + ": " + resolved.get("reason", ""))
        value = bool(resolved["value"])
    else:
        raise ValueError("unmodeled combinational expression")
    return value ^ expression.get("inv", False)


def resolve_bus(expression, logic, values, stack=()):
    """Four-state TBUF resolution; unknown enables/data are never tied off."""
    active = []
    try:
        if "unknown" in expression:
            raise ValueError(expression["unknown"])
        for driver in expression["drivers"]:
            signal = driver["driver"]
            if signal in stack:
                raise ValueError("combinational bus cycle: " + signal)
            node = logic[signal]
            if node["kind"] != "tristate_driver":
                raise ValueError("unmodeled bus producer: " + signal)
            if not evaluate_data(node["disable"], logic, values, stack + (signal,)):
                active.append({"signal": signal, "value": int(evaluate_data(
                    node["data"], logic, values, stack + (signal,)))})
    except (ValueError, KeyError) as exc:
        return {"status": "unknown", "reason": str(exc), "active": active}
    if not active:
        return {"status": "floating", "reason": "no enabled TBUF; no pull/keeper assumed", "active": []}
    if len({d["value"] for d in active}) > 1:
        return {"status": "contention", "reason": "opposing enabled TBUF data", "active": active}
    return {"status": "driven" if len(active) == 1 else "multiple_drivers_agree",
            "value": active[0]["value"], "active": active}


def data_support(expression, logic, stack=()):
    if "unknown" in expression:
        raise ValueError(expression["unknown"])
    if "source" in expression:
        source = expression["source"]
        if source in stack:
            raise ValueError("combinational cycle: " + source)
        node = logic.get(source)
        if node is None:
            raise ValueError("missing logic node: " + source)
        if node["kind"] in ("register", "pad_input"):
            return {source}
        if node["kind"] == "combinational":
            return data_support(node["data"], logic, stack + (source,))
        raise ValueError("architecture boundary: " + source)
    if "constant" in expression:
        return set()
    if expression.get("op") == "mux":
        parts = [expression[n] for n in ("select", "zero", "one")]
    elif expression.get("op") == "lut4":
        parts = expression["inputs"].values()
    elif expression.get("op") == "ram_read":
        raise ValueError("truth table requires runtime memory contents: " + expression["memory"])
    elif expression.get("op") in ("xor", "and"):
        parts = expression["args"]
    elif expression.get("op") == "bus":
        support = set()
        for driver in expression["drivers"]:
            signal = driver["driver"]
            if signal in stack:
                raise ValueError("combinational bus cycle: " + signal)
            node = logic.get(signal, {})
            if node.get("kind") != "tristate_driver":
                raise ValueError("unmodeled bus producer: " + signal)
            support.update(data_support(node["disable"], logic, stack + (signal,)))
            if node["disable"].get("constant") != 1 or node["disable"].get("inv", False):
                support.update(data_support(node["data"], logic, stack + (signal,)))
        return support
    else:
        raise ValueError("unmodeled combinational expression")
    return set().union(*(data_support(part, logic, stack) for part in parts))


def state_table(report, signal, max_inputs=10, local=False):
    """Exhaustive D-path table, before CE/SR/clock; no protocol labels assumed."""
    node = report["logic"][signal]
    result = {"signal": signal, "controls": node.get("controls", {}),
              "mode": node.get("mode", {}), "semantics": "D-path only; apply CE, SR and clock separately"}
    try:
        if "data" not in node:
            raise ValueError("signal has no modeled D path")
        if local:
            inputs = set()
            for value in expression_nodes(node["data"]):
                if "unknown" in value:
                    raise ValueError(value["unknown"])
                if "source" in value:
                    inputs.add(value["source"])
            inputs = sorted(inputs)
            result["semantics"] += "; immediate sources treated as symbolic variables, including combinational outputs"
        else:
            inputs = sorted(data_support(node["data"], report["logic"]))
        result["inputs"] = inputs
        if len(inputs) > max_inputs:
            raise ValueError(f"truth table needs {len(inputs)} inputs; limit is {max_inputs}")
        result["rows"] = [{"address": address, "d": int(evaluate_data(node["data"], report["logic"],
                           {name: bool(address & (1 << i)) for i, name in enumerate(inputs)}))}
                          for address in range(1 << len(inputs))]
        result["status"] = "verified_combinational_table"
    except ValueError as exc:
        result.update(status="unknown", reason=str(exc))
    return result


def register_contract(signal, node):
    """Normalize Virtex FF modes (make_ffs FD[CPRS]E primitive contract)."""
    if node.get("kind") != "register":
        return {"status": "unknown", "reason": "not a register"}
    prefix = "FF" + signal[-2] if signal.endswith((".XQ", ".YQ")) else (
        "FFI" if signal.endswith(".IQ") else "FFO" if signal.endswith(".PAD_O") else "FFT")
    common = "FF" if prefix in ("FFX", "FFY") else prefix
    mode = node.get("mode", {})
    attr = lambda name: mode.get(name, {}).get("value")
    flags = {name: attr(common + "_" + name) for name in ("LATCH", "SR_ENABLE", "SR_SYNC")}
    init = attr(prefix + "_INIT")
    controls = node.get("controls", {})
    result = {"data": node.get("data", {"unknown": "missing D"}), "init": init, "flags": flags,
              "clock": next((controls[p] for p in ("CLK", "ICLK", "OCLK", "TCLK") if p in controls), {}),
              "ce": next((controls[p] for p in ("CE", "ICE", "OCE", "TCE") if p in controls), {}),
              "sr": controls.get("SR", {}), "evidence": mode,
              "clock_contract": "caller must provide an edge of the selected effective clock"}
    if init not in (0, 1) or any(v not in (0, 1) for v in flags.values()):
        result.update(status="unknown", reason="unavailable register mode")
    elif flags["LATCH"] or (common == "FF" and attr("FF_REV_ENABLE") != 0):
        result.update(status="unknown", reason="latch or reverse SR not modeled")
    else:
        result["status"] = "modeled_sync_ff" if flags["SR_SYNC"] else "modeled_async_ff"
        result["sr_contract"] = "single SR resets/sets to INIT and takes priority over CE; synchronous SR requires selected clock edge"
    return result


def register_step(report, signal, values, clock_edge=False):
    contract = register_contract(signal, report["logic"][signal])
    if contract["status"] == "unknown":
        return contract
    try:
        sync = contract["flags"]["SR_SYNC"]
        if contract["flags"]["SR_ENABLE"] and (not sync or clock_edge) and evaluate_data(contract["sr"], report["logic"], values):
            return {"status": "resolved", "next": contract["init"], "cause": "synchronous SR" if sync else "asynchronous SR"}
        if not clock_edge:
            cause, value = "no selected clock edge", values[signal]
        elif not evaluate_data(contract["ce"], report["logic"], values):
            cause, value = "CE hold", values[signal]
        else:
            cause, value = "D sampled", evaluate_data(contract["data"], report["logic"], values)
        return {"status": "resolved", "next": int(bool(value)), "cause": cause}
    except (ValueError, KeyError) as exc:
        return {"status": "unknown", "reason": str(exc)}


def state_model(report):
    registers = {n: register_contract(n, d) for n, d in report["logic"].items() if d["kind"] == "register"}
    return {"registers": registers,
            "semantics": "Simultaneous pre-edge evaluation within each identified clock domain; SR overrides CE, synchronous SR requires clock edge. Not a reachability or quiescence proof.",
            "buses": {n: d["data"] for n, d in report["logic"].items() if d.get("data", {}).get("op") == "bus"},
            "memories": report.get("memories", {})}


def reconstruction_summary(report):
    """Count evidence boundaries separately from successful local regressions."""
    routes = report["routes"]
    kinds = {}
    for node in report["logic"].values():
        kinds[node["kind"]] = kinds.get(node["kind"], 0) + 1
    statuses = {status: sum(r["driver_status"] == status for r in routes.values())
                for status in ("resolved", "unknown", "ambiguous")}
    limits = sum(bool(r.get("limits")) for r in routes.values())
    unknown_encodings = sum(bool(r.get("unknown_pips")) for r in routes.values())
    unique_boundaries = {json.dumps(b, sort_keys=True) for b in report["boundaries"]}
    buses = sum(n.get("data", {}).get("op") == "bus" for n in report["logic"].values())
    incomplete = bool(unique_boundaries or statuses["unknown"] or statuses["ambiguous"]
                      or report.get("memories") or buses)
    return {"status": "incomplete" if incomplete else "modeled_not_reachability_proved",
            "logic_nodes": len(report["logic"]), "logic_kinds": kinds,
            "routing_paths": len(routes), "routing_statuses": statuses,
            "routes_with_limits": limits, "routes_with_unknown_encodings": unknown_encodings,
            "unique_boundaries": len(unique_boundaries), "conditional_buses": buses,
            "runtime_memories": len(report.get("memories", {})),
            "dma_quiescence": "Unknown; local validation is not a stop/drain acknowledgement"}


def local_equivalence(left, right, logic, max_inputs=10):
    """Compare expressions at explicit immediate cuts, retaining their polarity."""
    def unwrap(expression):
        seen = set()
        expression = dict(expression)
        while "source" in expression and logic.get(expression["source"], {}).get("kind") == "combinational":
            source = expression["source"]
            if source in seen:
                raise ValueError("combinational alias cycle: " + source)
            seen.add(source)
            child = dict(logic[source]["data"])
            child["inv"] = child.get("inv", False) ^ expression.get("inv", False)
            expression = child
        return expression

    left, right = unwrap(left), unwrap(right)
    inputs = set()
    for part in expression_nodes([left, right]):
        if "unknown" in part:
            raise ValueError(part["unknown"])
        if "source" in part:
            inputs.add(part["source"])
        if part.get("op") in ("bus", "ram_read"):
            raise ValueError("stateful or conditional bus expression cannot be an unconditional alias")
    inputs = sorted(inputs)
    if len(inputs) > max_inputs:
        raise ValueError("local equivalence input limit exceeded")
    for address in range(1 << len(inputs)):
        values = {n: bool(address & (1 << i)) for i, n in enumerate(inputs)}
        if evaluate_data(left, logic, values) != evaluate_data(right, logic, values):
            raise ValueError(f"alias differs at assignment {address}")
    return {"inputs": inputs, "rows": 1 << len(inputs)}


def register_equivalence(report, left, right):
    """Check reset, edge, enable and D contracts before using a shadow equality."""
    a = register_contract(left, report["logic"].get(left, {}))
    b = register_contract(right, report["logic"].get(right, {}))
    if a["status"] == "unknown" or b["status"] == "unknown":
        raise ValueError("unmodeled register in shadow equality")
    if any(a[n] != b[n] for n in ("status", "init", "flags")):
        raise ValueError("register mode or initialization differs")
    return {part: local_equivalence(a[part], b[part], report["logic"])
            for part in ("data", "ce", "sr", "clock")}


def memory_step(report, memory, values, clock_edge=False):
    """Update one decoded RAM16X1D from pre-edge values; never assume INIT at runtime."""
    contract = report.get("memories", {}).get(memory, {})
    if contract.get("kind") != "ram16x1d":
        return {"status": "unknown", "reason": "unmodeled memory"}
    try:
        current = values.get(memory)
        if not isinstance(current, int) or not 0 <= current <= 0xFFFF:
            raise ValueError("runtime memory contents unavailable: " + memory)
        if not clock_edge or not evaluate_data(contract["enable"], report["logic"], values):
            return {"status": "resolved", "next": current, "cause": "write disabled or no selected clock edge"}
        address = sum(int(evaluate_data(pin, report["logic"], values)) << int(i)
                      for i, pin in contract["write_address"].items())
        data = evaluate_data(contract["data"], report["logic"], values)
        return {"status": "resolved", "next": (current & ~(1 << address)) | (int(data) << address), "cause": "write"}
    except (ValueError, KeyError) as exc:
        return {"status": "unknown", "reason": str(exc)}


def cone_summary(report, roots):
    """Walk data and non-clock controls, preserving feedback and precise boundaries."""
    logic = dict(report["logic"], **report.get("memories", {}))
    seen, pending, boundaries, external, buses = set(), list(roots), {}, set(), set()
    while pending:
        name = pending.pop()
        if name in seen:
            continue
        seen.add(name)
        node = logic.get(name)
        if node is None:
            boundaries[name] = ["missing node (traversal limit or invalid reference)"]
            continue
        if node["kind"] == "pad_input":
            external.add(name)
            continue
        if node["kind"] == "architecture_boundary":
            boundaries[name] = [node["reason"]]
            continue
        if node["kind"] == "ram16x1d":
            boundaries.setdefault(name, []).append("runtime memory contents unavailable; INIT is not a drain acknowledgement")
        if node["kind"] == "register":
            contract = register_contract(name, node)
            if contract["status"] == "unknown":
                boundaries.setdefault(name, []).append(contract["reason"])
        controls = {k: v for k, v in node.get("controls", {}).items() if not k.endswith("CLK")}
        for part in expression_nodes({"data": node.get("data", {}), "disable": node.get("disable", {}),
                                      "controls": controls, "write_address": node.get("write_address", {}),
                                      "enable": node.get("enable", {})}):
            if "unknown" in part:
                boundaries.setdefault(name, []).append(part["unknown"])
            if "source" in part:
                pending.append(part["source"])
            if "driver" in part:
                pending.append(part["driver"])
            if "memory" in part:
                pending.append(part["memory"])
            if part.get("op") == "bus":
                buses.add(name)
    return {"nodes": len(seen), "registers": sum(logic.get(n, {}).get("kind") == "register" for n in seen),
            "boundaries": boundaries, "external_inputs": sorted(external), "conditional_buses": sorted(buses),
            "status": "incomplete" if boundaries else "modeled_with_external_inputs_and_bus_conditions"}


# Board correlation already documented from private schematic and BOND87.
PCI_PADS = {"REQ#": (18, 2), "GNT#": (16, 2), "STOP#": (15, 2),
            "FRAME#": (13, 2), "IRDY#": (15, 3), "TRDY#": (14, 1)}


def analyze_pci(router, max_cells=200):
    analyzer = LogicAnalyzer(router, max_cells)
    roots = {"XQ": analyzer.output({"x": 2, "y": 14, "bel": "SLICE[0]", "pin": "XQ"})}
    pci = router.bel(0, 13, "PCILOGIC")
    for name, pin in pci["inputs"].items():
        roots["PCILOGIC." + name] = analyzer.input(pin)
    for name, (row, index) in PCI_PADS.items():
        roots[name] = {"input": analyzer.output({"x": 0, "y": row, "bel": f"IOI[{index}]", "pin": "I"}),
                       "O": analyzer.iob_output(0, row, index, "O"),
                       "T": analyzer.iob_output(0, row, index, "T")}
    for row, index in ((26, 1), (11, 2)):
        roots[f"PCI datapath X0,Y{row} IOI[{index}]"] = {
            "O": analyzer.iob_output(0, row, index, "O"), "T": analyzer.iob_output(0, row, index, "T")}
    report = analyzer.report(roots)
    report["state_model"] = state_model(report)
    report["state_model"]["pad_outputs"] = {
        name: {"data": roots[name]["O"], "disable": roots[name]["T"],
               "drive_condition": "effective disable = 0",
               "observation": "External pin is not equated to output data while disabled; other PCI agents are external inputs."}
        for name in PCI_PADS}
    report["control_cones"] = {name: cone_summary(report, [signal]) for name, signal in {
        "Q": roots["XQ"], "REQ": roots["REQ#"]["O"], "FRAME": roots["FRAME#"]["O"],
        "FRAME_T": roots["FRAME#"]["T"], "IRDY": roots["IRDY#"]["O"]}.items()}
    report["local_relation_checks"] = validate_pci_relations(report)
    report["summary"] = reconstruction_summary(report)
    return report


def validate_pci_relations(report):
    """Private-image regression equations at explicit symbolic cuts, not reachable states."""
    def sl(x, y, n, pin):
        return f"X{x},Y{y} SLICE[{n}].{pin}"
    names = {"Q": sl(2, 14, 0, "XQ"), "P": sl(2, 15, 0, "XQ"),
             "A": sl(3, 16, 1, "X"), "C": sl(3, 18, 0, "Y"), "D": sl(3, 17, 0, "Y"),
             "K": sl(2, 17, 0, "XQ"), "U": sl(2, 19, 1, "X"),
             "V": sl(2, 19, 0, "Y"), "W": sl(2, 19, 1, "Y"),
             "B": sl(2, 13, 0, "XQ"), "T": sl(3, 13, 0, "XQ"),
             "S": sl(4, 9, 1, "XQ"), "M": sl(5, 3, 0, "YQ"), "N": sl(5, 10, 1, "YQ"),
             "O": "X0,Y15 IOI[3].PAD_O", "h": sl(5, 8, 0, "YQ"), "d": sl(2, 3, 1, "YQ"),
             "v": sl(5, 5, 0, "YQ"), "k": sl(3, 4, 0, "X"), "l": sl(2, 12, 1, "X"),
             "j": sl(4, 8, 0, "X"), "p": sl(3, 7, 0, "Y"), "z": sl(4, 6, 0, "X"),
             "u": sl(3, 10, 1, "Y"), "b": sl(3, 11, 1, "XQ"), "a": sl(5, 10, 0, "Y"),
             "c": sl(5, 3, 0, "X"), "R": sl(2, 12, 1, "YQ"), "Rd": sl(2, 16, 1, "YQ"),
             "H": sl(2, 18, 1, "YQ"), "E": sl(2, 18, 0, "Y"), "J": sl(4, 20, 1, "YQ")}
    for symbol, pad in {"f": (13, 2), "i": (15, 3), "r": (14, 1), "s": (15, 2), "g": (16, 2)}.items():
        names[symbol] = f"X0,Y{pad[0]} IOI[{pad[1]}].I"
    checks = []
    def check(name, signal, variables, expected, control=None):
        node = report["logic"].get(signal, {})
        expression = node.get("controls", {}).get(control, {}) if control else node.get("data", {})
        result = {"feature": name, "signal": signal, "symbolic_inputs": variables,
                  "semantics": "Exhaustive local symbolic assignments; no reachability claim", "rows": 1 << len(variables)}
        try:
            for address in range(1 << len(variables)):
                v = {key: bool(address & (1 << i)) for i, key in enumerate(variables)}
                values = {names[key]: value for key, value in v.items()}
                if "j" in v:
                    values[sl(4, 8, 1, "X")] = v["j"]
                if "O" in v:
                    values[sl(2, 15, 1, "XQ")] = v["O"]
                if evaluate_data(expression, report["logic"], values) != bool(expected(v)):
                    raise ValueError(f"mismatching symbolic assignment {address}")
            result["pass"] = True
        except (ValueError, KeyError) as exc:
            result.update({"pass": False, "reason": str(exc)})
        checks.append(result)
    check("Q next D", names["Q"], "QPAfi", lambda v: v['A'] if v['Q'] else (
        True if v['f'] else (not v['P'] or not v['i']) if v['A'] else (not v['P'] and not v['i'])))
    check("P next D", names["P"], "PQfiUVW", lambda v: (
        (not v['U'] and not v['V']) if v['P'] else v['f']) if v['i'] else (
        not v['W'] or v['f'] and not (v['Q'] and v['P'])))
    check("A relation", names["A"], "CDKP", lambda v: not v['C'] and not v['D'] or v['K'] and not v['P'])
    check("FRAME data", names["B"], "gSslhjrp", lambda v: (v['g'] or not v['S']) and (
        (not v['l'] or v['h'] and v['j'] and (not v['r'] or v['p'])) if v['s'] else (v['h'] or not v['l'])))
    check("FRAME CE", names["B"], "dsrO", lambda v: not v['d'] or not v['s'] or v['r'] or v['O'], "CE")
    check("FRAME tristate data", names["T"], "BsraguTkb", lambda v: v['B'] and (not v['s'] or not v['r']) or (
        (v['g'] and (v['u'] or v['T']) or v['B'] and v['k']) if v['a'] else
        not (v['b'] and (not v['u'] or not v['g']) and (not v['k'] or not v['B']))))
    check("FRAME tristate CE", names["T"], "TSM", lambda v: not v['T'] or v['S'] and v['M'], "CE")
    check("IRDY data", names["O"], "hvzsBkdOr", lambda v: not (v['h'] and v['v'] and v['z']) or (
        (v['B'] and v['k'] or (v['B'] or v['d']) and not v['O'] and not v['r']) if v['s'] else v['B']))
    check("grant-qualified S pulse", names["S"], "figMNS", lambda v: v['f'] and v['i'] and not v['g'] and v['M'] and v['N'] and not v['S'])
    check("M retention", names["M"], "SMNc", lambda v: not v['S'] and (v['M'] or v['N'] and v['c']))
    check("REQ data", "X0,Y18 IOI[2].PAD_O", ("R", "Rd", "H", "E"), lambda v: v['R'] or v['Rd'] or not v['H'] and not v['E'])
    check("REQ E", names["E"], "NMJ", lambda v: v['N'] and (v['M'] or v['J']))
    check("REQ delayed R", names["Rd"], ("R",), lambda v: v['R'])
    check("IRDY delayed tristate", "X0,Y15 IOI[3].PAD_T", ("T",), lambda v: v['T'])
    check("h constant D", names["h"], (), lambda v: True)
    for name, left, right in (
        ("FRAME data shadow", names["B"], "X0,Y13 IOI[2].PAD_O"),
        ("FRAME tristate shadow", names["T"], "X0,Y13 IOI[2].PAD_T"),
        ("IRDY data shadow", names["O"], sl(2, 15, 1, "XQ")),
        ("j duplicate Boolean cone", names["j"], sl(4, 8, 1, "X")),
    ):
        result = {"feature": name, "signals": [left, right],
                  "semantics": "Local alias proof; register equality requires matched initialization and selected clock events"}
        try:
            if name == "j duplicate Boolean cone":
                evidence = local_equivalence(report["logic"][left]["data"], report["logic"][right]["data"], report["logic"])
            else:
                evidence = register_equivalence(report, left, right)
            result.update({"pass": True, "evidence": evidence})
        except (ValueError, KeyError) as exc:
            result.update({"pass": False, "reason": str(exc)})
        checks.append(result)
    return {"status": "PASS" if all(c["pass"] for c in checks) else "FAIL",
            "symbol_map": names, "checks": checks}
