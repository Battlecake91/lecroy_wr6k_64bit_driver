"""Deterministic synthetic SMT tests; no vendor data or hardware access."""
import itertools
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
import z3
from virtexe_routing import bram_contract, bram_width, evaluate_data, resolve_bus
from virtexe_symbolic import System, clock_key, pci_environment


def c(value):
    return {"constant": int(value)}


def s(name, inv=False):
    return {"source": name, "inv": inv}


def reg(data, init=0, sync=0, ce=None, sr=None, clock=None):
    return {"kind": "register", "data": data,
        "controls": {"CLK": clock or s("clk"), "CE": ce or c(1), "SR": sr or c(0)},
        "mode": {k: {"value": v} for k, v in {
            "FF_LATCH": 0, "FF_SR_ENABLE": 1, "FF_SR_SYNC": sync,
            "FF_REV_ENABLE": 0, "FFX_INIT": init}.items()}}


def report(logic=None, memories=None):
    return {"logic": {"clk": {"kind": "pad_input"}, **(logic or {})}, "memories": memories or {}}


def bram(width_a=4, width_b=4):
    attrs = {"DATA_WIDTH_A": {"selections": ["_" + str(width_a)]},
             "DATA_WIDTH_B": {"selections": ["_" + str(width_b)]}, "INIT": {"value": 0}}
    pins = {p + port: s(p.lower() + port) for port in "AB" for p in ("CLK", "EN", "RST", "WE")}
    pins.update({f"ADDR{p}[{i}]": c(0) for p in "AB" for i in range(12)})
    pins.update({f"DI{p}[{i}]": c(1) for p in "AB" for i in range(16)})
    return bram_contract(attrs, pins, lambda pin: pin)


class SymbolicTests(unittest.TestCase):
    def assertImpossible(self, system, predicate):
        system.solver.push()
        system.solver.add(predicate)
        self.assertEqual(system.solver.check(), z3.unsat)
        system.solver.pop()

    def test_initial_register_state(self):
        m = System(report({"q.XQ": reg(c(0), init=1)}), 0)
        self.assertImpossible(m, z3.Not(m.state("q.XQ", 0)))

    def test_unknown_pcilogic_stays_independent_of_fabric_inputs(self):
        for bits in range(8):
            logic = {f"i{i}": {"kind": "pad_input"} for i in range(3)}
            logic["PCI_CE"] = {"kind": "architecture_boundary", "inputs": {
                f"I{i+1}": s(f"i{i}") for i in range(3)}, "configuration": {}}
            logic["AD.XQ"] = reg(c(0), init=1, ce=s("PCI_CE"))
            m = System(report(logic), 1, schedule=[{clock_key(s("clk"))}])
            m.solver.add(*[m.signal(f"i{i}",0) == bool(bits & (1 << i)) for i in range(3)])
            for old in (False, True):
                m.solver.push()
                m.solver.add(m.state("AD.XQ",1) == old)
                self.assertEqual(m.solver.check(), z3.sat)
                m.solver.pop()
            self.assertTrue(any("PCI_CE" in reason for reason in m.unknowns.values()))

    def test_read_output_load_hold_and_reset_obey_preedge_ce(self):
        logic = {n: {"kind": "pad_input"} for n in ("data", "ce", "rst")}
        logic["AD.XQ"] = reg(s("data"), init=0, ce=s("ce"), sr=s("rst"))
        m = System(report(logic), 4, schedule=[{clock_key(s("clk"))}] * 4)
        for t, (data, ce, reset) in enumerate(((1,1,0), (0,0,0), (0,0,0), (1,0,1), (1,0,1))):
            m.solver.add(m.signal("data",t) == bool(data), m.signal("ce",t) == bool(ce),
                         m.signal("rst",t) == bool(reset))
        self.assertEqual(m.solver.check(), z3.sat)
        self.assertImpossible(m, z3.Or(z3.Not(m.state("AD.XQ",1)),
                                      z3.Not(m.state("AD.XQ",2)), m.state("AD.XQ",3), m.state("AD.XQ",4)))

    def test_response_capture_needs_remote_edge_then_two_pci_edges(self):
        logic = {n: {"kind": "pad_input"} for n in ("remote", "valid", "payload")}
        logic.update({"R.XQ": reg(s("payload"), ce=s("valid"), clock=s("remote")),
                      "B.XQ": reg(s("R.XQ")), "D.XQ": reg(s("B.XQ"))})
        schedule = [{clock_key(s("clk"))}, {clock_key(s("remote"))},
                    {clock_key(s("clk"))}, {clock_key(s("clk"))}]
        m = System(report(logic), 4, schedule=schedule)
        for t in range(5):
            m.solver.add(m.signal("valid",t), m.signal("payload",t))
        self.assertEqual(m.solver.check(), z3.sat)
        self.assertImpossible(m, z3.Or(m.state("R.XQ",1), z3.Not(m.state("R.XQ",2)),
                                      m.state("B.XQ",2), z3.Not(m.state("B.XQ",3)),
                                      m.state("D.XQ",3), z3.Not(m.state("D.XQ",4))))

    def test_simultaneous_preedge_feedback(self):
        m = System(report({"a.XQ": reg(s("b.XQ")), "b.XQ": reg(s("a.XQ"), init=1)}), 1)
        m.solver.add(m.edge(s("clk"), 0))
        self.assertImpossible(m, z3.Or(z3.Not(m.state("a.XQ", 1)), m.state("b.XQ", 1)))

    def test_no_edge_holds(self):
        m = System(report({"q.XQ": reg(c(1))}), 1)
        m.solver.add(z3.Not(m.edge(s("clk"), 0)))
        self.assertImpossible(m, m.state("q.XQ", 1))

    def test_ce_holds(self):
        m = System(report({"q.XQ": reg(c(1), ce=c(0))}), 1)
        m.solver.add(m.edge(s("clk"), 0))
        self.assertImpossible(m, m.state("q.XQ", 1))

    def test_sync_reset_overrides_ce(self):
        m = System(report({"q.XQ": reg(c(1), ce=c(0), sr=c(1), sync=1)}), 1, powerup=False)
        m.solver.add(m.state("q.XQ", 0), m.edge(s("clk"), 0))
        self.assertImpossible(m, m.state("q.XQ", 1))

    def test_sync_reset_needs_edge(self):
        m = System(report({"q.XQ": reg(c(0), sr=c(1), sync=1)}), 1, powerup=False)
        m.solver.add(m.state("q.XQ", 0), z3.Not(m.edge(s("clk"), 0)))
        self.assertImpossible(m, z3.Not(m.state("q.XQ", 1)))

    def test_async_reset_without_clock(self):
        m = System(report({"rst": {"kind": "pad_input"}, "q.XQ": reg(c(1), sr=s("rst"))}), 1)
        m.solver.add(m.signal("rst", 1), z3.Not(m.edge(s("clk"), 0)))
        self.assertImpossible(m, m.state("q.XQ", 1))

    def test_multiclock_schedule(self):
        r = report({"clk2": {"kind": "pad_input"}, "a.XQ": reg(c(1)), "b.XQ": reg(s("a.XQ"), clock=s("clk2"))})
        m = System(r, 2, schedule=[{clock_key(s("clk"))}, {clock_key(s("clk2"))}])
        self.assertImpossible(m, z3.Or(z3.Not(m.state("a.XQ", 1)), m.state("b.XQ", 1), z3.Not(m.state("b.XQ", 2))))

    def test_opposite_clock_edges_excluded(self):
        m = System(report({"a.XQ": reg(c(1)), "b.XQ": reg(c(1), clock=s("clk", True))}), 1)
        self.assertImpossible(m, z3.And(m.edge(s("clk"), 0), m.edge(s("clk", True), 0)))

    def test_constant_clock_cannot_edge(self):
        m = System(report({"q.XQ": reg(c(1), clock=c(1))}), 1)
        self.assertImpossible(m, m.state("q.XQ", 1))

    def test_invalid_schedule_rejected(self):
        r = report({"a.XQ": reg(c(1)), "b.XQ": reg(c(1), clock=s("clk", True))})
        with self.assertRaises(ValueError):
            System(r, 1, schedule=[{clock_key(s("clk")), clock_key(s("clk", True))}])

    def test_lut_matches_concrete_exhaustively(self):
        m = System(report({str(i): {"kind": "pad_input"} for i in range(4)}), 0)
        for init in (0, 0xFFFF, 0x6996, 0x8000, 0xAAAA, 0xABCD):
            expression = {"op": "lut4", "init": init, "inputs": {str(i): s(str(i)) for i in range(4)}}
            value = m.expr(expression, 0)
            for bits in itertools.product((False, True), repeat=4):
                actual = z3.simplify(z3.substitute(value, *[(m.signal(str(i), 0), z3.BoolVal(v)) for i, v in enumerate(bits)]))
                expected = evaluate_data(expression, m.logic, {str(i): v for i, v in enumerate(bits)})
                self.assertEqual(z3.is_true(actual), expected)

    def test_unknown_is_not_constant(self):
        m = System(report(), 0)
        value = m.expr({"unknown": "unrecovered pin"}, 0)
        self.assertEqual(m.check(value)["status"], "Inconclusive")
        self.assertEqual(m.check(z3.Not(value))["solver"], "sat")

    def test_unknown_alias_inversion_correlated(self):
        m = System(report(), 0)
        a = m.expr({"unknown": "x", "route": "r"}, 0)
        b = m.expr({"unknown": "x", "route": "r", "inv": True}, 0)
        self.assertImpossible(m, a == b)

    def test_unsupported_register_remains_unknown(self):
        r = reg(c(0))
        r["mode"]["FF_LATCH"]["value"] = 1
        m = System(report({"q.XQ": r}), 1)
        result = m.check(m.state("q.XQ", 1))
        self.assertEqual(result["solver"], "sat")
        self.assertEqual(result["witness"], "abstract_candidate")

    def test_distributed_ram_write_and_hold(self):
        memory = {"kind": "ram16x1d", "initial": 0, "clock": s("clk"), "enable": c(1), "data": c(1), "write_address": {str(i): c(i == 0) for i in range(4)}}
        m = System(report(memories={"ram": memory}), 2, schedule=[{clock_key(s("clk"))}, set()])
        read = {"op": "ram_read", "memory": "ram", "address": memory["write_address"]}
        self.assertImpossible(m, z3.Or(m.expr(read, 0), z3.Not(m.expr(read, 1)), z3.Not(m.expr(read, 2))))

    def test_distributed_ram_runtime_not_init(self):
        memory = {"kind": "ram16x1d", "initial": 0, "clock": s("clk"), "enable": c(0), "data": c(0), "write_address": {"0": c(0)}}
        m = System(report(memories={"ram": memory}), 0, powerup=False)
        value = m.expr({"op": "ram_read", "memory": "ram", "address": {"0": c(0)}}, 0)
        self.assertEqual(m.check(value)["solver"], "sat")

    def test_bram_address_shift_and_width(self):
        memory = bram(4, 16)
        self.assertEqual(memory["ports"]["A"]["address_shift"], 2)
        self.assertEqual(len(memory["ports"]["A"]["address"]), 10)
        self.assertEqual(memory["ports"]["B"]["address_shift"], 4)
        self.assertEqual(len(memory["ports"]["B"]["data"]), 16)
        self.assertIsNone(bram_width({}, "A"))

    def bram_system(self):
        memory = bram(4, 16)
        # Write one bit in the last word: exercise 12-bit address wrap boundaries.
        memory["ports"]["A"]["address"] = {str(i): c(1) for i in range(10)}
        memory["ports"]["B"]["address"] = {str(i): c(1) for i in range(8)}
        logic = {"out": {"kind": "memory_output", "memory": "ram", "port": "B", "bit": 12}}
        for p in "AB":
            for n in ("clk", "en", "rst", "we"):
                logic[n + p] = {"kind": "pad_input"}
        m = System(report(logic, {"ram": memory}), 1)
        m.solver.add(m.edge(s("clkA"), 0), m.edge(s("clkB"), 0), m.signal("enA", 0), m.signal("enB", 0),
            z3.Not(m.signal("rstA", 0)), z3.Not(m.signal("rstB", 0)), m.signal("weA", 0), z3.Not(m.signal("weB", 0)),
            z3.Not(m.var("input", "ram:cross_port_setup_window_overlap", 0)))
        return m

    def test_bram_write_uses_shared_cells(self):
        m = self.bram_system()
        self.assertImpossible(m, z3.Not(z3.Select(m.memory("ram", 1), z3.BitVecVal(4092, 12))))

    def test_bram_read_collision_symbolic(self):
        m = self.bram_system()
        self.assertEqual(m.check(m.state("out", 1))["solver"], "sat")
        self.assertEqual(m.check(z3.Not(m.state("out", 1)))["solver"], "sat")

    def test_bram_reset_clears_output_not_cells(self):
        m = self.bram_system()
        # Override the existing no-reset constraint in a separate fresh model.
        r = m.report
        r["memories"]["ram"]["ports"]["B"]["reset"] = c(1)
        m = System(r, 1)
        m.solver.add(m.edge(s("clkB"), 0), m.signal("enB", 0), m.signal("weB", 0))
        self.assertImpossible(m, m.state("out", 1))

    def test_bram_disabled_output_holds(self):
        r = self.bram_system().report
        r["memories"]["ram"]["ports"]["B"]["enable"] = c(0)
        m = System(r, 1)
        self.assertImpossible(m, m.state("out", 0) != m.state("out", 1))

    def test_bram_writeback_output_during_other_port_write(self):
        r = self.bram_system().report
        r["logic"]["out"]["port"] = "A"
        r["logic"]["out"]["bit"] = 0
        m = System(r, 1)
        m.solver.add(m.edge(s("clkA"), 0), m.signal("enA", 0), m.signal("weA", 0), z3.Not(m.signal("rstA", 0)))
        self.assertImpossible(m, z3.Not(m.state("out", 1)))

    def test_bram_unknown_port_cannot_freeze_memory(self):
        memory = bram()
        memory["ports"]["A"] = {"status": "unknown", "reason": "missing width"}
        m = System(report(memories={"ram": memory}), 1)
        self.assertEqual(m.check(z3.Select(m.memory("ram", 1), z3.BitVecVal(1, 12)))["witness"], "abstract_candidate")

    def test_unknown_bus_enable_does_not_imply_onehot(self):
        logic = {"a": {"kind": "tristate_driver", "data": c(0), "disable": {"unknown": "enable route"}},
                 "b": {"kind": "tristate_driver", "data": c(1), "disable": c(0)}}
        m = System(report(logic), 0)
        expression = {"op": "bus", "resolution": "spartan2e_wired_and", "drivers": [{"driver": "a"}, {"driver": "b"}]}
        m.watch_bus("bus", expression)
        result = m.check(m.bus(expression, 0)["conflicting"])
        self.assertEqual(result["status"], "Inconclusive")
        self.assertEqual(result["trace"][0]["buses"]["bus"]["active_drivers"], [0, 1])

    def test_bus_ownership_cases(self):
        logic = {"d0": {"kind": "tristate_driver", "data": c(0), "disable": s("t0")},
                 "d1": {"kind": "tristate_driver", "data": s("data"), "disable": s("t1")},
                 **{n: {"kind": "pad_input"} for n in ("t0", "t1", "data")}}
        expression = {"op": "bus", "resolution": "spartan2e_wired_and", "drivers": [{"driver": "d0"}, {"driver": "d1"}]}
        m = System(report(logic), 0)
        for condition in ("floating", "single", "agreeing", "conflicting"):
            self.assertEqual(m.check(m.bus(expression, 0)[condition])["solver"], "sat")
        for t0, t1, data in itertools.product((0, 1), repeat=3):
            values = dict(t0=t0, t1=t1, data=data)
            expected = resolve_bus(expression, logic, values)["value"]
            value = z3.simplify(z3.substitute(m.expr(expression, 0), *[(m.signal(n, 0), z3.BoolVal(bool(v))) for n, v in values.items()]))
            self.assertEqual(z3.is_true(value), bool(expected))

    def test_reachable_counterexample_trace(self):
        m = System(report({"q.XQ": reg(c(1))}), 2, schedule=[{clock_key(s("clk"))}] * 2)
        result = m.invariant(lambda t: z3.Not(m.state("q.XQ", t)), ["q.XQ"])
        self.assertEqual(result["status"], "Counterexample")
        self.assertFalse(result["trace"][0]["signals"]["q.XQ"])
        self.assertTrue(result["trace"][1]["signals"]["q.XQ"])

    def test_unsat_bound_is_not_unbounded_proof(self):
        m = System(report({"q.XQ": reg(c(0))}), 2)
        result = m.invariant(lambda t: z3.Not(m.state("q.XQ", t)))
        self.assertEqual(result["solver"], "unsat")
        self.assertEqual(result["status"], "Inconclusive")
        self.assertFalse(result["unbounded_proof"])

    def test_inconsistent_environment_is_not_a_proof(self):
        m = System(report(), 0)
        m.solver.add(z3.BoolVal(False))
        result = m.invariant(lambda t: z3.BoolVal(True))
        self.assertEqual(result["status"], "Inconclusive")
        self.assertEqual(result["base_consistency"], "unsat")
        self.assertIn("Inconsistent", result["reason"])

    def test_solver_timeout_is_inconclusive(self):
        m = System(report(), 0)
        with patch.object(type(m.solver), "check", return_value=z3.unknown), patch.object(type(m.solver), "reason_unknown", return_value="timeout"):
            result = m.check(z3.BoolVal(True))
        self.assertEqual(result["status"], "Inconclusive")
        self.assertEqual(result["reason"], "timeout")

    def test_no_progress_prefix_not_livelock(self):
        m = System(report({"q.XQ": reg(c(1))}), 1, schedule=[{clock_key(s("clk"))}])
        result = m.livelock(lambda t: z3.BoolVal(False))
        self.assertEqual(result["solver"], "unsat")

    def test_lasso_and_explicit_deadlock(self):
        m = System(report({"q.XQ": reg(c(0))}), 1)
        self.assertEqual(m.livelock(lambda t: m.state("q.XQ", t))["status"], "Counterexample")
        self.assertEqual(m.deadlock(lambda t: z3.BoolVal(False), lambda t: m.state("q.XQ", t))["status"], "Counterexample")

    def pci_system(self, profile="electrical-only"):
        r = report()
        r["roots"] = {}
        for name in ("REQ#", "GNT#", "FRAME#", "IRDY#", "TRDY#", "STOP#", "DEVSEL#"):
            r["logic"][name] = {"kind": "pad_input"}
            r["logic"][name + "O"] = {"kind": "combinational", "data": c(0)}
            r["logic"][name + "T"] = {"kind": "combinational", "data": c(1)}
            r["roots"][name] = {"input": name, "O": name + "O", "T": name + "T"}
        m = System(r, 0)
        return m, pci_environment(m, profile)

    def test_pci_released_pin_is_not_intended_output(self):
        m, pins = self.pci_system()
        m.solver.add(z3.Not(m.var("input", "FRAME#:other_agent_enable", 0)))
        self.assertImpossible(m, z3.Not(pins[0]["FRAME#"]))

    def test_pci_external_target_rule(self):
        m, pins = self.pci_system("target-response")
        self.assertImpossible(m, z3.And(z3.Not(pins[0]["TRDY#"]), pins[0]["DEVSEL#"]))

    def test_pci_has_no_response_fairness(self):
        m, pins = self.pci_system("target-response")
        self.assertEqual(m.check(z3.And(*[pins[0][n] for n in ("GNT#", "TRDY#", "DEVSEL#", "STOP#")]))["solver"], "sat")

    def test_pci_missing_correlation_not_guessed(self):
        m = System(report(), 0)
        pci_environment(m)
        self.assertTrue(any("DEVSEL#" in n for n in m.unknowns.values()))


if __name__ == "__main__":
    unittest.main()
