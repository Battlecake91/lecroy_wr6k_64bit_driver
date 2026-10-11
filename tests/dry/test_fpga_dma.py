"""Synthetic and private PCI initiator/descriptor evidence regressions."""
import hashlib
import itertools
import json
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
from virtexe_dma import (IIMTC_GATES, NAMES, RELATIONS, SGTA_GATES, ad_origin,
                        capture_map, check_local, focused_evidence, phase_observation, sl)
from virtexe_mmio import functional_expression
from virtexe_routing import COMMIT, evaluate_data, register_contract, register_equivalence


class SyntheticDma(unittest.TestCase):
    def test_public_matrix_preserves_safety_and_query_scope(self):
        path = Path(__file__).resolve().parents[2] / "docs" / "pci-mmio-register-evidence.json"
        matrix = json.loads(path.read_text(encoding="utf-8"))
        dma = matrix["pci_dma"]
        self.assertEqual(set(matrix["release_obligations"].values()), {"Unknown"})
        self.assertEqual(matrix["stable_until_explicit_restart"], "Unknown")
        self.assertEqual(dma["local_equations"]["count"], len(RELATIONS))
        self.assertEqual(dma["completion_source"]["retirement_implication"], "Unknown")
        q = dma["bounded_queries"]
        self.assertEqual(q["status"], "Inconclusive")
        self.assertEqual(q["bound"], 1)
        self.assertEqual([r["number"] for r in q["results"]], list(range(1, 11)))
        self.assertEqual(sum(r["solver"] == "sat" for r in q["results"]), q["sat"])
        self.assertEqual(sum(r["solver"] == "unsat" for r in q["results"]), q["unsat"])
        self.assertEqual(q["results"][4]["solver"], "unsat")
        self.assertIn("not eventual retry retirement", q["results"][4]["query"])

    def test_final_frame_does_not_mean_final_data_accepted(self):
        for trdy in (False, True):
            r = phase_observation(True, False, trdy, True, False, False, True)
            self.assertEqual(r["final_accepted"], not trdy)
            self.assertFalse(r["bus_idle_observed"])

    def test_retry_and_disconnect_are_distinguished_by_history(self):
        for first, trdy in itertools.product((False, True), repeat=2):
            r = phase_observation(False, False, trdy, False, False, first, True)
            self.assertEqual(r["retry"], first and trdy)
            self.assertEqual(r["disconnect"], not first or not trdy)
            self.assertFalse(r["target_abort"])

    def test_abort_requires_claim_history_and_is_not_retry(self):
        for claimed in (False, True):
            r = phase_observation(False, False, True, False, True, True, claimed)
            self.assertEqual(r["target_abort"], claimed)
            self.assertFalse(r["retry"])

    def test_capture_origin_does_not_assume_one_hot_bus(self):
        p = {"roots": {"AD[3]": {"IQ": "iq"}}, "logic": {
            "bus": {"kind": "combinational", "data": {"op": "bus", "drivers": [{"driver": "d"}, {"driver": "other"}]}},
            "d": {"kind": "tristate_driver", "data": {"source": "iq"}, "disable": {"source": "gate"}},
            "other": {"kind": "tristate_driver", "data": {"unknown": "other producer"}, "disable": {"constant": 0}},
            "iq": {"kind": "register"}, "gate": {"kind": "register"}}}
        r = ad_origin(p, {"source": "bus"})
        self.assertEqual(r["bit"], 3)
        self.assertEqual(r["disable"], {"source": "gate"})

    def test_missing_or_ambiguous_origin_fails_closed(self):
        for drivers in ([], [{"driver": "a"}, {"driver": "b"}]):
            p = {"roots": {"AD[0]": {"IQ": "iq"}}, "logic": {
                "bus": {"kind": "combinational", "data": {"op": "bus", "drivers": drivers}},
                "a": {"kind": "tristate_driver", "data": {"source": "iq"}, "disable": {"constant": 0}},
                "b": {"kind": "tristate_driver", "data": {"source": "iq"}, "disable": {"constant": 0}},
                "iq": {"kind": "register"}}}
            with self.assertRaises(ValueError):
                ad_origin(p, {"source": "bus"})

    def test_wrong_architecture_pin_cannot_pass(self):
        with self.assertRaises(ValueError):
            focused_evidence({"project_combine_commit": "wrong", "logic": {}})

    def test_irq_lut_matches_independent_sticky_clear_equation(self):
        variables = ("data0", "done", "clear", "irq")
        p = {"logic": {NAMES["irq"]: {"data": {"op": "lut4", "init": 0xDFCC,
            "inputs": {str(i): {"source": NAMES[n]} for i, n in enumerate(variables)}}}}}
        p["logic"].update({NAMES[n]: {"kind": "register"} for n in variables if n != "irq"})
        p["logic"][NAMES["irq"]]["kind"] = "register"
        self.assertEqual(check_local(p, "irq", variables, RELATIONS[0][2])["rows"], 16)
        p["logic"][NAMES["irq"]]["data"]["init"] ^= 1
        with self.assertRaises(ValueError):
            check_local(p, "irq", variables, RELATIONS[0][2])


@unittest.skipUnless(os.environ.get("WR6K_MMIO_REPORT"), "private PCI report not configured")
class PrivateDma(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = json.loads(Path(os.environ["WR6K_MMIO_REPORT"]).read_text(encoding="utf-8"))
        if cls.p.get("project_combine_commit") != COMMIT:
            raise ValueError("architecture pin mismatch")
        if cls.p.get("image_sha256") != hashlib.sha256(Path(os.environ["WR6K_FPGA_BINARY_205"]).read_bytes()).hexdigest():
            raise ValueError("private image/report mismatch")
        if cls.p.get("frame_validation", {}).get("status") != "PASS":
            raise ValueError("private frame validation missing")

    def test_sgta_has_exact_bits_2_through_31(self):
        m = capture_map(self.p, SGTA_GATES)
        self.assertEqual(set(m), set(range(2, 32)))
        self.assertEqual(m[2]["signal"], sl(25, 20, 0, "YQ"))

    def test_iimtc_has_all_32_bits(self):
        self.assertEqual(set(capture_map(self.p, IIMTC_GATES)), set(range(32)))

    def test_two_distinct_word_banks_have_complete_bit_mapping(self):
        a = capture_map(self.p, [NAMES["word_a_lo_ce"], NAMES["word_a_hi_ce"]], stages=1)
        b = capture_map(self.p, [NAMES["word_b_lo_ce"], NAMES["word_b_hi_ce"]], stages=1)
        self.assertEqual(set(a), set(range(32)))
        self.assertEqual(set(b), set(range(32)))
        self.assertFalse({d["signal"] for d in a.values()} & {d["signal"] for d in b.values()})
        for m in (a, b):
            self.assertTrue(all(d["disable"]["source"] == sl(3, 14, 0, "X") for d in m.values()))

    def test_busmaster_command_enable_is_configuration_bit_two(self):
        name = sl(5, 10, 1, "YQ")
        n = self.p["logic"][name]
        self.assertEqual(ad_origin(self.p, n["data"])["bit"], 2)
        self.assertEqual(n["controls"]["CE"]["source"], sl(8, 10, 0, "Y"))

    def test_all_ad_and_cbe_output_registers_retain_pci_ce_boundary(self):
        for name, root in self.p["roots"].items():
            if not name.startswith(("AD[", "CBE#[")):
                continue
            c = register_contract(root["O"], self.p["logic"][root["O"]])
            self.assertEqual(c["ce"]["source"], "X0,Y13 PCILOGIC.PCI_CE")

    def test_receive_buffer_has_independent_remote_and_pci_ports(self):
        for name in ("X46,Y9 BRAM", "X46,Y13 BRAM"):
            ports = self.p["memories"][name]["ports"]
            self.assertEqual(ports["A"]["clock"]["source"], "X24,Y0 BUFGCE[0].O")
            self.assertEqual(ports["B"]["clock"]["source"], "X24,Y29 BUFGCE[1].O")
            self.assertEqual(ports["A"]["write"]["source"], sl(43, 10, 1, "YQ"))
            self.assertEqual(functional_expression(ports["B"]["write"]), {"constant": 1, "inv": True})

    def test_descriptor_load_flag_replicas_have_matched_register_contracts(self):
        for n in (sl(22, 19, 1, "XQ"), sl(22, 20, 0, "YQ"), sl(23, 20, 0, "YQ")):
            register_equivalence(self.p, sl(22, 19, 1, "YQ"), n)


def add_relation(name, variables, predicate):
    def test(self):
        self.assertEqual(check_local(self.p, name, variables, predicate)["status"], "Verified")
    setattr(PrivateDma, "test_equation_" + name, test)


for relation in RELATIONS:
    add_relation(*relation)


@unittest.skipUnless(os.environ.get("WR6K_MMIO_REPORT") and os.environ.get("WR6K_FPGA_SYMBOLIC") == "1",
                     "private PCI SMT not configured")
class PrivateDmaSmt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        PrivateDma.setUpClass()
        cls.p = PrivateDma.p

    def test_word_a_zero_detector_exact_boolean_equivalence(self):
        from virtexe_symbolic import System, z3
        s = System(self.p, bound=0, powerup=False, timeout_ms=30000)
        m = capture_map(self.p, [NAMES["word_a_lo_ce"], NAMES["word_a_hi_ce"]], stages=1)
        zero = z3.And(*[z3.Not(s.signal(d["signal"], 0)) for d in m.values()])
        s.solver.add(s.signal(self.p["roots"]["RST#"]["I"], 0))
        self.assertEqual(s.solver.check(), z3.sat)
        s.solver.add(s.signal(NAMES["zero"], 0) != zero)
        self.assertEqual(s.solver.check(), z3.unsat)

    def test_iimtc_counter_d_is_reload_or_decrement_modulo_32_bits(self):
        from virtexe_symbolic import System, z3
        s = System(self.p, bound=0, powerup=False, timeout_ms=30000)
        names = [sl(29, y, 1, pin) for y in range(13, 29) for pin in ("XQ", "YQ")]
        m = capture_map(self.p, IIMTC_GATES)
        def word(values):
            return sum((z3.If(v, z3.BitVecVal(1 << i, 32), z3.BitVecVal(0, 32)) for i, v in enumerate(values)), z3.BitVecVal(0, 32))
        old = word([s.signal(n, 0) for n in names])
        data = word([s.expr(self.p["logic"][n]["data"], 0) for n in names])
        load = word([s.signal(m[i]["signal"], 0) for i in range(32)])
        expected = z3.If(z3.And(s.signal(NAMES["cl"], 0), s.signal(NAMES["reload"], 0)), load, old - 1)
        s.solver.add(s.signal(self.p["roots"]["RST#"]["I"], 0))
        self.assertEqual(s.solver.check(), z3.sat)
        s.solver.add(data != expected)
        answer = s.solver.check()
        detail = ""
        if str(answer) == "sat":
            model = s.solver.model()
            detail = str({"old": model.eval(old), "actual": model.eval(data),
                          "expected": model.eval(expected)})
        self.assertEqual(answer, z3.unsat, detail)

    def test_descriptor_counter_d_loads_word_b_or_two_or_decrements(self):
        from virtexe_symbolic import System, z3
        s = System(self.p, bound=0, powerup=False, timeout_ms=30000)
        names = [sl(20, y, 0, pin) for y in range(10, 26) for pin in ("XQ", "YQ")]
        m = capture_map(self.p, [NAMES["word_b_lo_ce"], NAMES["word_b_hi_ce"]], stages=1)
        def word(values):
            return sum((z3.If(v, z3.BitVecVal(1 << i, 32), z3.BitVecVal(0, 32)) for i, v in enumerate(values)), z3.BitVecVal(0, 32))
        old = word([s.signal(n, 0) for n in names])
        data = word([s.expr(self.p["logic"][n]["data"], 0) for n in names])
        load = s.signal(sl(22, 19, 1, "YQ"), 0)
        for n in (sl(22, 19, 1, "XQ"), sl(22, 20, 0, "YQ"), sl(23, 20, 0, "YQ")):
            s.solver.add(s.signal(n, 0) == load)
        expected = z3.If(load, word([s.signal(m[i]["signal"], 0) for i in range(32)]),
                         z3.If(s.signal(NAMES["pending"], 0), z3.BitVecVal(2, 32), old - 1))
        s.solver.add(s.signal(self.p["roots"]["RST#"]["I"], 0))
        self.assertEqual(s.solver.check(), z3.sat)
        s.solver.add(data != expected)
        answer = s.solver.check()
        detail = ""
        if answer == z3.sat:
            model = s.solver.model()
            detail = str({"old": model.eval(old), "actual": model.eval(data), "expected": model.eval(expected),
                          "load": model.eval(load), "pending": model.eval(s.signal(NAMES["pending"], 0))})
        self.assertEqual(answer, z3.unsat, detail)


if __name__ == "__main__":
    unittest.main()
