"""Bounded offline verification of the existing decoder's expression network.

No bitstream decoder lives here. Unknown architecture paths are fresh symbolic
inputs, and any resulting SAT witness is explicitly an abstract candidate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

try:
    import z3
    if not hasattr(z3, "Solver"):
        raise ImportError("z3-solver package is missing or inaccessible")
except ImportError as exc:
    raise ImportError("Install tools/fpga/requirements-symbolic.txt for offline SMT analysis") from exc

from virtexe_routing import register_contract


def clock_key(expression):
    """Route identifiers are evidence, not independent clock domains."""
    return json.dumps({k: v for k, v in expression.items() if k != "route"}, sort_keys=True)


class System:
    """Event-boundary model: simultaneous pre-edge sampling, explicit clocks.

Each step may contain selected effective clock edges and input/reset changes.
Analog settling, metastability and clock generation are not silently modeled.
"""
    def __init__(self, report, bound=4, timeout_ms=10000, powerup=True, schedule=None):
        if bound < 0 or timeout_ms < 1:
            raise ValueError("nonnegative bound and positive timeout required")
        self.report, self.logic = report, report["logic"]
        self.bound, self.powerup = bound, powerup
        self.solver = z3.Solver()
        self.solver.set(timeout=timeout_ms, random_seed=0)
        self.timeout_ms = timeout_ms
        self.schedule = schedule
        if schedule is not None and len(schedule) != bound:
            raise ValueError("one explicit clock-domain set per transition required")
        self.variables, self.cache, self.unknowns = {}, {}, {}
        self.hazards, self.assumptions, self.watched_buses = [], [], {}
        self.registers = {n: register_contract(n, d) for n, d in self.logic.items() if d["kind"] == "register"}
        self.memories = report.get("memories", {})
        self.outputs = {n: d for n, d in self.logic.items() if d["kind"] == "memory_output"}
        self.domains = sorted({clock_key(c.get("clock", {})) for c in self.registers.values()} |
            {clock_key(p["clock"]) for m in self.memories.values() for p in
             ([m] if m["kind"] == "ram16x1d" else m.get("ports", {}).values()) if "clock" in p})
        for domain in self.domains:
            c = json.loads(domain)
            source = self.logic.get(c.get("source"), {})
            if "unknown" in c or ("source" in c and source.get("kind") != "pad_input"):
                self.unknown("effective clock generation/phase not modeled: " + domain, 0)
        if schedule is not None:
            if any(set(step) - set(self.domains) for step in schedule):
                raise ValueError("schedule contains unknown clock domain")
            for step in schedule:
                clocks = [json.loads(c) for c in step]
                if any("constant" in c for c in clocks) or any("source" in a and a.get("source") == b.get("source") and a.get("inv", False) != b.get("inv", False)
                    for i, a in enumerate(clocks) for b in clocks[i + 1:]):
                    raise ValueError("impossible clock-edge schedule")
        self._initialize()
        for t in range(bound):
            self._transition(t)

    def var(self, category, name, t, sort=None):
        key = (category, name, t)
        if key not in self.variables:
            symbol = category + ":" + name + "@" + str(t)
            self.variables[key] = z3.Const(symbol, z3.BoolSort() if sort is None else sort)
        return self.variables[key]

    def unknown(self, reason, t, sort=None):
        key = hashlib.sha256(reason.encode()).hexdigest()[:20]
        self.unknowns[key] = reason
        return self.var("unknown", key, t, sort)

    def state(self, name, t):
        return self.var("state", name, t)

    def memory(self, name, t):
        return self.var("memory", name, t, z3.ArraySort(z3.BitVecSort(12), z3.BoolSort()))

    def edge(self, expression, t):
        key = clock_key(expression)
        if "constant" in expression:
            return z3.BoolVal(False)
        if self.schedule is not None:
            return z3.BoolVal(key in self.schedule[t])
        return self.var("edge", key, t)

    def signal(self, name, t, stack=()):
        if name in self.registers or name in self.outputs:
            return self.state(name, t)
        if (name, t) in self.cache:
            return self.cache[name, t]
        if name in stack:
            return self.unknown("combinational cycle: " + name, t)
        node = self.logic.get(name, {})
        kind = node.get("kind")
        if kind == "pad_input":
            value = self.var("input", name, t)
        elif kind == "combinational":
            value = self.expr(node["data"], t, stack + (name,))
        else:
            value = self.unknown(name + ": " + node.get("reason", "unmodeled source"), t)
        self.cache[name, t] = value
        return value

    def address(self, pins, t, stack=()):
        value = z3.BitVecVal(0, 12)
        for i, pin in pins.items():
            value = value | z3.If(self.expr(pin, t, stack), z3.BitVecVal(1 << int(i), 12), z3.BitVecVal(0, 12))
        return value

    def expr(self, expression, t, stack=()):
        if "unknown" in expression:
            identity = {k: v for k, v in expression.items() if k != "inv"}
            value = self.unknown("expression: " + json.dumps(identity, sort_keys=True), t)
        elif "constant" in expression:
            value = z3.BoolVal(bool(expression["constant"]))
        elif "source" in expression:
            value = self.signal(expression["source"], t, stack)
        elif expression.get("op") == "mux":
            value = z3.If(self.expr(expression["select"], t, stack), self.expr(expression["one"], t, stack), self.expr(expression["zero"], t, stack))
        elif expression.get("op") in ("and", "or", "xor"):
            args = [self.expr(p, t, stack) for p in expression["args"]]
            if expression["op"] == "xor":
                value = z3.BoolVal(False)
                for arg in args:
                    value = z3.Xor(value, arg)
            else:
                value = (z3.And if expression["op"] == "and" else z3.Or)(*args)
        elif expression.get("op") == "lut4":
            pins = {int(i): self.expr(p, t, stack) for i, p in expression["inputs"].items()}
            # Unused LUT inputs are set to zero only after lut_support proved independence.
            value = z3.Or(*[z3.And(*[v if a & (1 << i) else z3.Not(v) for i, v in pins.items()])
                for a in range(16) if expression["init"] & (1 << a) and all(i in pins or not a & (1 << i) for i in range(4))])
        elif expression.get("op") == "ram_read":
            name = expression["memory"]
            if self.memories.get(name, {}).get("kind") != "ram16x1d":
                value = self.unknown("unmodeled runtime memory: " + name, t)
            else:
                value = z3.Select(self.memory(name, t), self.address(expression["address"], t, stack))
        elif expression.get("op") == "bus":
            bus = self.bus(expression, t, stack)
            if expression.get("resolution") == "spartan2e_wired_and":
                value = z3.And(*[z3.Implies(e, d) for e, d in bus["drivers"]])
            else:
                invalid = z3.Or(bus["floating"], bus["conflicting"])
                self.hazards.append(("generic bus floating/conflicting", invalid))
                value = z3.If(invalid, self.unknown("generic bus has no defined level: " + str(stack), t),
                              z3.Or(*[z3.And(e, d) for e, d in bus["drivers"]]))
        else:
            value = self.unknown("unsupported expression: " + json.dumps(expression, sort_keys=True), t)
        return z3.Not(value) if expression.get("inv", False) else value

    def bus(self, expression, t, stack=()):
        drivers = []
        for item in expression.get("drivers", []):
            name = item["driver"]
            node = self.logic.get(name, {})
            if node.get("kind") != "tristate_driver" or name in stack:
                drivers.append((self.unknown("unresolved bus enable: " + name, t), self.unknown("unresolved bus data: " + name, t)))
            else:
                drivers.append((z3.Not(self.expr(node["disable"], t, stack + (name,))), self.expr(node["data"], t, stack + (name,))))
        if "unknown" in expression:
            drivers.append((self.unknown("unresolved bus topology: " + expression["unknown"], t), self.unknown("unresolved bus topology data", t)))
        count = z3.Sum(*[z3.If(e, 1, 0) for e, _ in drivers])
        conflict = z3.Or(*[z3.And(a, b, da != db) for i, (a, da) in enumerate(drivers) for b, db in drivers[i + 1:]])
        return {"drivers": drivers, "floating": count == 0, "single": count == 1, "multiple": count > 1,
                "agreeing": z3.And(count > 1, z3.Not(conflict)), "conflicting": conflict}

    def watch_bus(self, name, expression):
        self.watched_buses[name] = [self.bus(expression, t) for t in range(self.bound + 1)]

    def _initialize(self):
        if not self.powerup:
            self.assumptions.append("arbitrary runtime initial state; SAT is not power-up reachability")
        for name, c in self.registers.items():
            if c["status"] == "unknown":
                self.unknown("register contract: " + name + ": " + c["reason"], 0)
            elif self.powerup:
                self.solver.add(self.state(name, 0) == bool(c["init"]))
        for name, m in self.memories.items():
            if m["kind"] not in ("ram16x1d", "ramb4"):
                self.unknown("unsupported memory: " + name, 0)
                continue
            if self.powerup:
                init = m.get("initial")
                if init is None:
                    self.unknown("memory INIT unavailable: " + name, 0)
                else:
                    array = z3.K(z3.BitVecSort(12), z3.BoolVal(False))
                    for i in range(16 if m["kind"] == "ram16x1d" else 4096):
                        if init & (1 << i):
                            array = z3.Store(array, z3.BitVecVal(i, 12), z3.BoolVal(True))
                    self.solver.add(self.memory(name, 0) == array)
        # BRAM output-latch startup state is not inferred from memory INIT.
        if self.powerup and self.outputs:
            self.unknown("BRAM output-latch startup values not independently established", 0)
        for t in range(self.bound + 1):
            for name, c in self.registers.items():
                if c["status"] != "unknown" and c["flags"]["SR_ENABLE"] and not c["flags"]["SR_SYNC"]:
                    self.solver.add(z3.Implies(self.expr(c["sr"], t), self.state(name, t) == bool(c["init"])))

    def _transition(self, t):
        for name, c in self.registers.items():
            if c["status"] == "unknown":
                continue
            edge = self.edge(c["clock"], t)
            value = z3.If(z3.And(edge, self.expr(c["ce"], t)), self.expr(c["data"], t), self.state(name, t))
            if c["flags"]["SR_ENABLE"]:
                sr = self.expr(c["sr"], t)
                reset = z3.And(edge, sr) if c["flags"]["SR_SYNC"] else z3.Or(sr, self.expr(c["sr"], t + 1))
                value = z3.If(reset, z3.BoolVal(bool(c["init"])), value)
            self.solver.add(self.state(name, t + 1) == value)
        # Opposite polarities of one effective clock cannot both have rising edges.
        if self.schedule is None:
            for i, domain in enumerate(self.domains):
                a = json.loads(domain)
                for other in self.domains[i + 1:]:
                    b = json.loads(other)
                    if "source" in a and a.get("source") == b.get("source") and a.get("inv", False) != b.get("inv", False):
                        self.solver.add(z3.Not(z3.And(self.edge(a, t), self.edge(b, t))))
        for name, m in self.memories.items():
            if m["kind"] == "ram16x1d":
                old = self.memory(name, t)
                write = z3.And(self.edge(m["clock"], t), self.expr(m["enable"], t))
                new = z3.If(write, z3.Store(old, self.address(m["write_address"], t), self.expr(m["data"], t)), old)
                self.solver.add(self.memory(name, t + 1) == new)
            elif m["kind"] == "ramb4":
                self._bram_transition(name, m, t)

    def _bram_transition(self, name, memory, t):
        ports = {}
        for p, c in memory["ports"].items():
            if c.get("status") != "modeled":
                self.unknown("unmodeled BRAM port: " + name + p, t)
                # Unknown write side can alter any bit, not merely its output.
                self.solver.add(self.memory(name, t + 1) == self.unknown("unmodeled BRAM contents: " + name, t + 1, self.memory(name, t).sort()))
                return
            active = z3.And(self.edge(c["clock"], t), self.expr(c["enable"], t))
            reset = self.expr(c["reset"], t)
            # The available application note establishes output reset, not a
            # same-port RST+WE write-priority truth table. Retain both outcomes.
            write_allowed = z3.Or(z3.Not(reset), self.unknown("BRAM same-port reset/write priority: " + name + p, t))
            ports[p] = {"active": active, "write": z3.And(active, self.expr(c["write"], t), write_allowed),
                "reset": reset, "base": self.address(c["address"], t) << c["address_shift"],
                "data": [self.expr(c["data"][str(i)], t) for i in range(c["width"])], "width": c["width"]}
        old, new = self.memory(name, t), self.memory(name, t)
        a, b = ports["A"], ports["B"]
        # Timing windows are not known: a symbolic near-edge flag covers collisions
        # even when the other port's edge occurs in an adjacent modeled step.
        near = self.var("input", name + ":cross_port_setup_window_overlap", t)
        self.assumptions.append("BRAM cross-port setup-window overlap is unconstrained: " + name) if t == 0 else None
        for p, c in ports.items():
            other = b if p == "A" else a
            for i, data in enumerate(c["data"]):
                address = c["base"] + i
                overlap = (address & z3.BitVecVal(4096 - other["width"], 12)) == other["base"]
                conflict = z3.And(c["write"], other["write"], overlap)
                # An unknown write in the setup window can corrupt this write.
                uncertain = z3.And(c["write"], near)
                value = z3.If(z3.Or(conflict, uncertain), self.unknown("BRAM colliding write: " + name + p + str(i), t), data)
                new = z3.If(c["write"], z3.Store(new, address, value), new)
        self.solver.add(self.memory(name, t + 1) == new)
        for signal, output in self.outputs.items():
            if output["memory"] != name:
                continue
            c = ports[output["port"]]
            other = b if output["port"] == "A" else a
            i = output["bit"]
            address = c["base"] + i
            overlap = (address & z3.BitVecVal(4096 - other["width"], 12)) == other["base"]
            invalid_read = z3.Or(near, z3.And(other["write"], overlap))
            read = z3.If(invalid_read, self.unknown("BRAM read collision: " + signal, t), z3.Select(old, address))
            value = z3.If(c["write"], c["data"][i], read)
            value = z3.If(c["reset"], z3.BoolVal(False), value)
            self.solver.add(self.state(signal, t + 1) == z3.If(c["active"], value, self.state(signal, t)))

    def check(self, predicate, kind="reachability", trace_names=()):
        """Check an explicit bad-state/reachability formula, never an implicit goal."""
        if kind not in ("reachability", "invariant", "deadlock", "livelock"):
            raise ValueError("unsupported verification query")
        observed = [{n: self.signal(n, t) for n in trace_names} for t in range(self.bound + 1)]
        base = self.solver.check()
        self.solver.push()
        try:
            self.solver.add(predicate)
            answer = self.solver.check()
            result = {"query": kind, "solver": str(answer), "bound": self.bound, "timeout_ms": self.timeout_ms,
                "base_consistency": str(base),
                "solver_version": z3.get_version_string(), "unbounded_proof": False,
                "initial_state": "configuration power-up" if self.powerup else "arbitrary runtime",
                "clock_domains": self.domains, "assumptions": list(dict.fromkeys(self.assumptions)),
                "unknowns": sorted(self.unknowns.values())}
            if base == z3.unsat:
                result.update(status="Inconclusive", reason="Inconsistent initial state/environment constraints; no reachability conclusion")
            elif answer == z3.unknown:
                result.update(status="Inconclusive", reason=self.solver.reason_unknown())
            elif answer == z3.unsat:
                result.update(status="Inconclusive", reason="No witness within this bound; not an unbounded invariant or termination proof")
            else:
                model = self.solver.model()
                hazards = [n for n, p in self.hazards if z3.is_true(model.eval(p, model_completion=True))]
                abstract = bool(self.unknowns or hazards or not self.powerup)
                result.update(status="Inconclusive" if abstract else "Proven" if kind == "reachability" else "Counterexample",
                    witness="abstract_candidate" if abstract else "valid_model_execution", hazards=sorted(set(hazards)),
                    interpretation="A solver execution is not evidence of actual oscilloscope behavior")
                result["trace"] = [{"step": t, "signals": {n: bool(z3.is_true(model.eval(v, model_completion=True))) for n, v in observed[t].items()},
                    "buses": {n: {"active_drivers": [i for i, (e, _) in enumerate(samples[t]["drivers"]) if z3.is_true(model.eval(e, model_completion=True))],
                        "driver_data": [z3.is_true(model.eval(d, model_completion=True)) for _, d in samples[t]["drivers"]],
                        "conflicting": z3.is_true(model.eval(samples[t]["conflicting"], model_completion=True))} for n, samples in self.watched_buses.items()},
                    "variables": {category + ":" + name: str(model.eval(v, model_completion=True)) for (category, name, step), v in self.variables.items() if step == t}}
                    for t in range(self.bound + 1)]
            return result
        finally:
            self.solver.pop()

    def invariant(self, property_at, trace_names=()):
        return self.check(z3.Or(*[z3.Not(property_at(t)) for t in range(self.bound + 1)]), "invariant", trace_names)

    def deadlock(self, enabled_at, goal_at, trace_names=()):
        # Enabled actions must be supplied; absence of one sampled clock edge is
        # not by itself deadlock in an asynchronous environment.
        return self.check(z3.Or(*[z3.And(z3.Not(goal_at(t)), z3.Not(enabled_at(t))) for t in range(self.bound + 1)]), "deadlock", trace_names)

    def livelock(self, goal_at, trace_names=()):
        # Exact lasso, including arrays and BRAM output state. No progress/fairness
        # is invented. A finite no-progress prefix alone is not a livelock.
        names = sorted(set(self.registers) | set(self.outputs))
        loops = []
        for start in range(self.bound):
            equal = [self.state(n, start) == self.state(n, self.bound) for n in names]
            equal += [self.memory(n, start) == self.memory(n, self.bound) for n in self.memories]
            loops.append(z3.And(*equal, *[z3.Not(goal_at(t)) for t in range(start, self.bound + 1)]))
        return self.check(z3.Or(*loops), "livelock", trace_names)

    def coverage(self):
        return {"registers": len(self.registers), "modeled_registers": sum(c["status"] != "unknown" for c in self.registers.values()),
            "ram16x1d": sum(m["kind"] == "ram16x1d" for m in self.memories.values()),
            "ramb4": sum(m["kind"] == "ramb4" for m in self.memories.values()), "bram_output_bits": len(self.outputs),
            "clock_domains": len(self.domains), "unknowns": len(self.unknowns)}


def pci_environment(system, profile="electrical-only"):
    """Bind shared sampled pins separately from FPGA O/T and other agents.

    Optional target-response rules constrain only external target inputs. They
    do not require FPGA grant acceptance, termination, or DMA acknowledgements.
    """
    if profile not in ("electrical-only", "target-response"):
        raise ValueError("unknown PCI environment profile")
    roots = system.report.get("roots", {})
    result = {}
    for t in range(system.bound + 1):
        at = {}
        for name in ("REQ#", "GNT#", "FRAME#", "IRDY#", "TRDY#", "STOP#", "DEVSEL#"):
            root = roots.get(name)
            if not isinstance(root, dict) or "input" not in root:
                at[name] = system.unknown("PCI input correlation missing: " + name, t)
                continue
            sampled = system.signal(root["input"], t)
            at[name] = sampled
            if "O" not in root or "T" not in root:
                continue
            data = system.signal(root["O"], t)
            enabled = z3.Not(system.signal(root["T"], t))
            other = system.var("input", name + ":other_agent_enable", t)
            external = system.var("input", name + ":other_agent_data", t)
            conflict = z3.And(enabled, other, data != external)
            system.hazards.append((name + ": external electrical contention", conflict))
            level = z3.If(enabled, data, z3.If(other, external, z3.BoolVal(True)))
            system.solver.add(z3.Implies(z3.Not(conflict), sampled == level))
        result[t] = at
        if profile == "target-response":
            # Only apply to lines released by the FPGA; its own target outputs
            # must not be constrained to satisfy the property being investigated.
            released = [system.signal(roots[n]["T"], t) for n in ("TRDY#", "STOP#") if n in roots and "T" in roots[n]]
            system.solver.add(z3.Implies(z3.And(*released, z3.Or(z3.Not(at["TRDY#"]), z3.Not(at["STOP#"]))), z3.Not(at["DEVSEL#"])))
    system.assumptions.extend(["PCI profile: " + profile,
        "Unowned external PCI lines have pull-up High; external owner/data are independent environmental inputs",
        "No fairness, bounded target response, grant latency, producer stop or DMA acknowledgement is assumed",
        "External contention leaves sampled level unconstrained; internal BUFT wired-AND is NOT used for PCI pads"])
    return result


def analyze(report, bound=4, timeout_ms=10000, profile="electrical-only"):
    system = System(report, bound, timeout_ms)
    pins = pci_environment(system, profile)
    results, cut_results = {}, {}
    bus_name = "X4,Y2 TBUS.OUT"
    expression = report["logic"].get(bus_name, {}).get("data")
    trace = [root[p] for root in report.get("roots", {}).values() if isinstance(root, dict) for p in ("O", "T") if p in root]
    if expression:
        system.watch_bus(bus_name, expression)
        for condition in ("floating", "single", "agreeing", "conflicting"):
            results["tbus_" + condition] = system.check(z3.Or(*[system.bus(expression, t)[condition] for t in range(bound + 1)]), trace_names=trace)
        results["tbus_mutual_exclusion"] = system.invariant(lambda t: z3.Not(system.bus(expression, t)["multiple"]), trace)
        cuts = System(report, 0, timeout_ms, powerup=False)
        cuts.watch_bus(bus_name, expression)
        for condition in ("floating", "single", "agreeing", "conflicting"):
            cut_results[condition] = cuts.check(cuts.bus(expression, 0)[condition])
    # Pin observations are deliberately not named transaction FSM states.
    for name in ("REQ#", "FRAME#", "IRDY#"):
        root = report.get("roots", {}).get(name, {})
        if "O" in root and "T" in root:
            asserted = lambda t: z3.And(z3.Not(system.signal(root["T"], t)), z3.Not(system.signal(root["O"], t)))
            results[name + "_driven_low"] = system.check(z3.Or(*[asserted(t) for t in range(bound + 1)]), trace_names=trace)
    results["sampled_ready_pair"] = system.check(z3.Or(*[z3.And(z3.Not(pins[t]["IRDY#"]), z3.Not(pins[t]["TRDY#"])) for t in range(bound + 1)]), trace_names=trace)
    return {"coverage": system.coverage(), "queries": results, "tbus_arbitrary_cut_queries": cut_results,
        "not_attempted": {"transaction_deadlock_livelock": "No established terminal/progress state or enabled-action predicate; API is synthetic-tested, not a hardware liveness proof"},
        "dma_quiescence": {n: {"status": "Inconclusive", "reason": reason} for n, reason in {
            "producer_stop": "No identified software stop-to-producer acknowledgement",
            "descriptor_stop": "No identified descriptor-consumption disable/acknowledgement",
            "no_new_transactions": "No identified acknowledgement to use as invariant antecedent",
            "outstanding_complete": "No correlated DEVSEL timeout/abort/terminal-state contract",
            "buffers_empty": "Runtime memory values do not establish occupancy/pointer meaning",
            "host_completion": "FPGA model cannot establish Windows DMA/host-bridge retirement"}.items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path, help="private existing decoder JSON (never commit it)")
    parser.add_argument("--bound", type=int, default=4)
    parser.add_argument("--timeout-ms", type=int, default=10000)
    parser.add_argument("--environment", choices=("electrical-only", "target-response"), default="electrical-only")
    parser.add_argument("--output", type=Path, required=True, help="private machine-readable results and traces")
    args = parser.parse_args()
    data = json.loads(args.report.read_text(encoding="utf-8"))
    result = analyze(data.get("pci_analysis", data), args.bound, args.timeout_ms, args.environment)
    result["network_sha256"] = hashlib.sha256(args.report.read_bytes()).hexdigest()
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(args.output), "coverage": result["coverage"], "results": {
        n: {k: r[k] for k in ("status", "solver", "bound")} for n, r in result["queries"].items()}}))


if __name__ == "__main__":
    main()
