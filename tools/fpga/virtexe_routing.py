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
    return {"kind": "private_firmware_native_architecture", "checks": checks,
            "status": "PASS" if all(c["pass"] for c in checks) else "FAIL"}


def lut_support(value):
    return [i for i in range(4) if any(((value >> address) ^ (value >> (address ^ (1 << i)))) & 1
                                     for address in range(16))]


class LogicAnalyzer:
    """Emit a bounded register/combinational network without assigning PCI states."""
    def __init__(self, router, max_cells=200):
        self.router, self.max_cells = router, max_cells
        self.logic, self.routes, self.boundaries = {}, {}, []
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
            result["unknown"] = route["driver_status"]
            self.boundaries.append({"route": key, "reason": route["driver_status"]})
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
            self.slice(entry, pin, attrs, inputs)
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
                entry.update(kind="register", data={"pad": [x, y, name]},
                             controls={p: self.input(inputs[p]) for p in ("ICLK", "ICE", "SR")},
                             mode={n: a for n, a in attrs.items() if n.startswith("FFI_")})
            else:
                entry.update(kind="unknown", reason="unsupported IOB output")
        else:
            entry.update(kind="architecture_boundary", reason="hard block semantics not modeled",
                         inputs={n: self.input(p) for n, p in inputs.items()})
            self.boundaries.append({"signal": key, "reason": entry["reason"]})

    def slice(self, entry, pin, attrs, inputs):
        def selection(name):
            values = attrs.get(name, {}).get("selections", [])
            return values[0] if len(values) == 1 else None

        def lut(letter):
            if any(attrs.get(letter + suffix, {}).get("value", 1) for suffix in ("_RAM_ENABLE", "_SHIFT_ENABLE")):
                return {"unknown": f"{letter} is configured as RAM or shift register"}
            value = attrs[letter]["value"]
            return {"op": "lut4", "init": value,
                    "inputs": {str(i): self.input(inputs[f"{letter}{i + 1}"]) for i in lut_support(value)}}

        def combinational(axis):
            mode = selection("FXMUX" if axis == "X" else "GYMUX")
            if mode == "F":
                return lut("F")
            if mode == "G":
                return lut("G")
            if mode == "F5":
                return {"op": "mux", "select": self.input(inputs["BX"]), "zero": lut("G"), "one": lut("F")}
            return {"unknown": f"unmodeled {axis} data path {mode}"}

        if pin in ("X", "Y"):
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
            pending = [node.get("data", {})]
            while pending:
                value = pending.pop()
                if isinstance(value, dict):
                    if "unknown" in value:
                        self.boundaries.append({"signal": name, "reason": value["unknown"]})
                    pending.extend(value.values())
        return {"project_combine_commit": COMMIT, "roots": roots, "logic": self.logic,
                "routes": self.routes, "boundaries": self.boundaries,
                "interpretation": "Configured logic network; protocol states and DMA quiescence are not assumed."}


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
    else:
        raise ValueError("unmodeled combinational expression")
    return value ^ expression.get("inv", False)


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
            pending = [node["data"]]
            while pending:
                value = pending.pop()
                if isinstance(value, dict):
                    if "unknown" in value:
                        raise ValueError(value["unknown"])
                    if "source" in value:
                        inputs.add(value["source"])
                    pending.extend(value.values())
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
    return analyzer.report(roots)
