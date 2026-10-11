"""Focused PCI DMA evidence checks over the existing private decoder report.

Coordinates identify configured signals, not undocumented register semantics.
No hardware access or independent bitstream decoder is implemented here.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from virtexe_mmio import cone_inventory, functional_expression
from virtexe_routing import COMMIT, evaluate_data


def sl(x, y, n, pin):
    return f"X{x},Y{y} SLICE[{n}].{pin}"


NAMES = {
    "irq": sl(22, 11, 0, "XQ"), "done": sl(22, 11, 0, "YQ"),
    "clear": sl(16, 8, 0, "X"), "data0": "X21,Y1 TBUS.OUT",
    "cl": sl(23, 14, 1, "YQ"), "cl2": sl(23, 20, 1, "YQ"),
    "terminal": sl(26, 18, 0, "YQ"), "zero": sl(22, 21, 1, "X"),
    "segment_zero": sl(20, 10, 1, "X"), "segment_gate": sl(20, 25, 1, "X"),
    "segment_event": sl(22, 10, 0, "YQ"), "phase": sl(23, 21, 1, "YQ"),
    "accept": sl(19, 8, 0, "YQ"), "pci_accept": sl(4, 13, 1, "YQ"),
    "read_mode": sl(19, 7, 1, "YQ"), "v": sl(5, 5, 0, "YQ"),
    "i": sl(2, 15, 1, "XQ"), "trdy": "X0,Y14 IOI[1].I",
    "word_a_lo_ce": sl(23, 21, 0, "X"), "word_a_hi_ce": sl(24, 21, 1, "X"),
    "word_b_lo_ce": sl(23, 21, 0, "Y"), "word_b_hi_ce": sl(23, 22, 1, "X"),
    "incoming_phase": sl(23, 22, 1, "YQ"),
    "reload": sl(25, 20, 1, "XQ"), "pending": sl(26, 18, 1, "YQ"),
    "load_replica": sl(22, 20, 0, "YQ"), "next_phase": sl(24, 21, 1, "YQ"),
    "r": sl(2, 12, 1, "YQ"), "rd": sl(2, 16, 1, "YQ"),
    "b": sl(2, 13, 0, "XQ"), "k": sl(3, 4, 0, "X"),
    "stop": "X0,Y15 IOI[2].I",
}
SGTA_GATES = (sl(17, 13, 0, "Y"), sl(17, 13, 1, "X"))
IIMTC_GATES = (sl(15, 12, 0, "X"), sl(16, 12, 0, "Y"))


def bank(report, gates):
    return {n: d for n, d in report["logic"].items()
            if d.get("kind") == "register" and
            d.get("controls", {}).get("CE", {}).get("source") in gates}


def ad_origin(report, expression):
    """Identify the AD capture alternative, without assuming one-hot TBUS enables."""
    logic = report["logic"]
    iq = {v["IQ"]: int(k[3:-1]) for k, v in report["roots"].items()
          if k.startswith("AD[")}
    candidates = []
    for n in cone_inventory(report, expression)["signals"]:
        node = logic.get(n, {})
        if node.get("kind") == "tristate_driver":
            value = functional_expression(node["data"])
            if value.get("source") in iq and not value.get("inv", False):
                candidates.append({"bit": iq[value["source"]], "driver": n,
                                   "disable": functional_expression(node["disable"])})
    if len(candidates) != 1:
        raise ValueError("expected exactly one non-inverted AD input alternative")
    return candidates[0]


def capture_map(report, gates, stages=0):
    result = {}
    for name, node in bank(report, gates).items():
        data = node["data"]
        for _ in range(stages):
            value = functional_expression(data)
            if "source" not in value or value.get("inv", False):
                raise ValueError("capture pipeline is not a direct non-inverted register")
            node = report["logic"][value["source"]]
            if node.get("kind") != "register":
                raise ValueError("capture pipeline register missing")
            data = node["data"]
        origin = ad_origin(report, data)
        if origin["bit"] in result:
            raise ValueError("duplicate AD capture bit")
        result[origin["bit"]] = {"signal": name, **origin}
    return result


def check_local(report, name, variables, expected):
    """Exhaustive check at named local cuts; never a state reachability proof."""
    expression = report["logic"][NAMES[name]]["data"]
    rows = 1 << len(variables)
    for assignment in range(rows):
        values = {v: bool(assignment & (1 << i)) for i, v in enumerate(variables)}
        actual = evaluate_data(expression, report["logic"], {NAMES[k]: v for k, v in values.items()})
        if actual != bool(expected(values)):
            raise ValueError(f"{name}: local assignment {assignment} differs")
    return {"signal": NAMES[name], "rows": rows, "status": "Verified",
            "scope": "local symbolic cuts, not runtime reachability"}


RELATIONS = (
    ("irq", ("data0", "done", "clear", "irq"),
     lambda v: v["done"] or v["irq"] and not (v["clear"] and v["data0"])),
    ("done", ("cl", "terminal", "done", "zero"),
     lambda v: v["cl"] and v["done"] or v["terminal"] and v["zero"]),
    ("terminal", ("segment_event", "next_phase"),
     lambda v: v["segment_event"] and v["next_phase"]),
    ("segment_event", ("segment_zero", "segment_gate"),
     lambda v: v["segment_zero"] and v["segment_gate"]),
    ("segment_gate", ("load_replica", "pending"),
     lambda v: not v["load_replica"] and not v["pending"]),
    ("pci_accept", ("trdy", "v", "i"),
     lambda v: not v["trdy"] and v["v"] and not v["i"]),
    ("accept", ("pci_accept", "read_mode"),
     lambda v: v["pci_accept"] and v["read_mode"]),
    ("word_a_lo_ce", ("accept", "phase"), lambda v: v["accept"] and v["phase"]),
    ("word_a_hi_ce", ("accept", "phase"), lambda v: v["accept"] and v["phase"]),
    ("word_b_lo_ce", ("accept", "incoming_phase"), lambda v: v["accept"] and v["incoming_phase"]),
    ("word_b_hi_ce", ("accept", "incoming_phase"), lambda v: v["accept"] and v["incoming_phase"]),
    ("reload", ("done", "reload", "cl"),
     lambda v: not v["cl"] and (v["done"] or v["reload"])),
    ("r", ("v", "k", "stop", "b"),
     lambda v: v["b"] and v["v"] and (v["k"] or not v["stop"])),
)


def focused_evidence(report):
    if report.get("project_combine_commit") != COMMIT:
        raise ValueError("Project Combine pin mismatch")
    checks = {name: check_local(report, name, variables, expected)
              for name, variables, expected in RELATIONS}
    maps = {"sgta": capture_map(report, SGTA_GATES), "iimtc": capture_map(report, IIMTC_GATES),
            "word_a": capture_map(report, [NAMES["word_a_lo_ce"], NAMES["word_a_hi_ce"]], stages=1),
            "word_b": capture_map(report, [NAMES["word_b_lo_ce"], NAMES["word_b_hi_ce"]], stages=1)}
    for name, expected in (("sgta", set(range(2, 32))), ("iimtc", set(range(32))),
                           ("word_a", set(range(32))), ("word_b", set(range(32)))):
        if set(maps[name]) != expected:
            raise ValueError(name + " capture coverage differs")
    return {"project_combine_commit": COMMIT, "image_sha256": report.get("image_sha256"),
            "checks": checks, "capture_maps": maps,
            "safety": "UnknownActive unchanged; no DMA retirement or host completion certificate"}


def phase_observation(frame, irdy, trdy, stop, devsel, first, claimed):
    """Conventional PCI observations, conditional on this card owning the bus.

    These are protocol labels, not reconstructed internal FF state names.
    Master-abort detection additionally needs the missing response history.
    """
    accepted = not irdy and not trdy
    return {"accepted": accepted, "final_accepted": frame and accepted,
            "wait": irdy or trdy,
            "retry": first and not stop and trdy and not devsel,
            "disconnect": not stop and not devsel and (not first or not trdy),
            "target_abort": claimed and not stop and devsel,
            "bus_idle_observed": frame and irdy}


def bounded_queries(report, timeout_ms=30000):
    """Ten scoped queries. Unknown lifecycle meanings are never invented.

    Runtime initial states are arbitrary. Only a PCI edge is selected; the
    remote producer clocks have no edge in this one-transition window. There
    is no fairness assumption or power-up/valid-launch reachability claim.
    """
    from virtexe_symbolic import System, clock_key, z3
    pci = {"source": "X24,Y29 BUFGCE[1].O", "inv": False}
    s = System(report, bound=1, powerup=False, timeout_ms=timeout_ms,
               schedule=[{clock_key(pci)}])
    q = lambda n, t: s.signal(NAMES.get(n, n), t)
    rst = report["roots"]["RST#"]["I"]
    s.solver.add(q(rst, 0), q(rst, 1))
    req = report["roots"]["REQ#"]
    active = lambda t: z3.And(z3.Not(q(req["O"], t)), z3.Not(q(req["T"], t)))
    frame = report["roots"]["FRAME#"]
    driving = lambda t: z3.Not(q(frame["T"], t))
    results = []

    def run(number, title, predicate, scope, extra=()):
        s.solver.push()
        try:
            s.solver.add(*extra)
            base = s.solver.check()
            s.solver.add(predicate)
            answer = s.solver.check()
            results.append({"solver": str(answer), "base_consistency": str(base), "bound": 1,
                            "initial_state": "arbitrary runtime", "unbounded_proof": False,
                            "status": "Inconclusive"})
            results[-1].update(number=number, query=title, scope=scope,
                               unknown_count=len(s.unknowns),
                               assumptions=["RST# deasserted at both boundaries", "one selected PCI clock edge; no other domain edge",
                                            "arbitrary runtime FF/memory contents; no fairness", *scope.split("; ")])
            if answer == z3.unknown:
                results[-1]["solver_reason"] = s.solver.reason_unknown()
            elif answer == z3.sat:
                model = s.solver.model()
                results[-1]["cut_trace"] = [{n: z3.is_true(model.eval(q(n, t), model_completion=True))
                                            for n in ("cl", "irq", "done", "r", "rd", "b", "i")}
                                           for t in range(2)]
            print(f"PCI DMA query {number}: base={base}, result={answer}", flush=True)
        finally:
            s.solver.pop()

    clear = z3.And(q("cl", 0), z3.Not(q("cl", 1)), q(sl(17, 13, 1, "Y"), 0),
                   z3.Not(q("data0", 0)))
    replicas = [sl(23, y, 1, "YQ") for y in (14, 20, 25)]
    equal_clear = [q(n, 0) for n in replicas] + [z3.Not(q(n, 1)) for n in replicas]
    run(1, "IIMCL clear with active REQ#", z3.And(clear, active(1)),
        "actual clear strobe/TBUS bit/replicas; asserted driven REQ# after clear", equal_clear)
    run(2, "IRQ source before established retirement", z3.And(z3.Not(q("irq", 0)), q("done", 0), q("irq", 1),
         active(1), driving(1), z3.Not(q("b", 1))),
        "actual IRQ source latch rises; active FRAME output and driven REQ# coexist; final-transaction identity unknown")
    pointer = sl(25, 21, 0, "YQ")
    run(3, "Descriptor-address candidate changes after clear", z3.And(clear, q(pointer, 0) != q(pointer, 1)),
        "actual address-candidate FF bit changes on clear edge; full descriptor advance/reachability unknown", equal_clear)
    bram = report["memories"]["X46,Y9 BRAM"]
    run(4, "PCI activity with receive write disabled", z3.And(z3.Not(s.expr(bram["ports"]["A"]["write"], 0)),
        driving(1), z3.Not(q("b", 1)), active(1)),
        "actual receive BRAM write enable low; no remote edge; buffered payload write attribution unknown")
    run(5, "STOP#/TRDY# retry cut retains request state", z3.And(z3.Not(q("stop", 0)), q("trdy", 0),
        q("r", 0), q("rd", 1), active(1)),
        "STOP# low/TRDY# high and actual delayed request latch; initial-data-phase and eventual delayed-write semantics unknown")
    run(6, "Host timeout with pending PCI activity", z3.And(active(1), driving(1), z3.Not(q("b", 1))),
        "timeout is an independent host observation, not an FPGA reset/input; no cancellation transition assumed")
    run(7, "IIMCL restart with pending request", z3.And(z3.Not(q("cl", 0)), q("cl", 1),
        q(sl(17, 13, 1, "Y"), 0), q("data0", 0), active(0), active(1)),
        "actual IIMCL set strobe; pending driven REQ# at both boundaries; software launch admission not modeled")
    run(8, "Clear candidate with receive admission still enabled", z3.And(clear,
        s.expr(bram["ports"]["A"]["write"], 1)),
        "IIMCL-clear candidate is not a receive-port admission barrier; no physical producer-stop claim", equal_clear)
    run(9, "IRQ provenance across generations", q("irq", 1),
        "untagged actual IRQ latch; host generation labels cannot establish physical origin")
    run(10, "Accepted final phase without host completion certificate", z3.And(driving(1),
        z3.Not(q(report["roots"]["IRDY#"]["T"], 1)), q("b", 1),
        z3.Not(q("i", 1)), z3.Not(q("trdy", 1)),
        z3.Not(q(report["roots"]["IRDY#"]["I"], 1)),
        z3.Not(q(report["roots"]["DEVSEL#"]["I"], 1))),
        "enabled FRAME/IRDY outputs, FRAME# high/IRDY# low/TRDY# low/DEVSEL# low; prior ownership/address phase assumed; host bridge completion outside FPGA model")
    return {"queries": results, "hardware_idle": "Unknown", "stable_until_restart": "Unknown",
            "interpretation": "SAT results are local runtime candidates only; queries 3-5,9-10 do not resolve the named end-to-end lifecycle property"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path, help="private configured network")
    parser.add_argument("--queries", action="store_true", help="run bounded PCI DMA queries (requires z3-solver)")
    parser.add_argument("--output", type=Path, help="private report outside repository")
    args = parser.parse_args()
    if args.output and (args.output.resolve().is_relative_to(Path(__file__).resolve().parents[2]) or
                        args.output.resolve() == args.report.resolve()):
        parser.error("private output must be outside repository and not overwrite input")
    report = json.loads(args.report.read_text(encoding="utf-8"))
    evidence = focused_evidence(report)
    if args.queries:
        evidence["bounded"] = bounded_queries(report)
    if args.output:
        args.output.write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"local_relations": len(evidence["checks"]),
                      "capture_bits": {k: len(v) for k, v in evidence["capture_maps"].items()},
                      "queries": evidence.get("bounded", {}).get("queries", [])}, indent=2))


if __name__ == "__main__":
    main()
