"""Synthetic MMIO analysis tests and opt-in private configuration regressions."""
import hashlib
import json
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
from virtexe_mmio import (BOARD_PINS, LINK_PINS, address_projection, capture_candidates,
                         cone_inventory, functional_expression, package_locations, signal)
from virtexe_routing import COMMIT, data_support, register_contract, register_step, state_table


class SyntheticMmio(unittest.TestCase):
    def test_public_matrix_keeps_unknown_safety_and_explicit_connections(self):
        matrix = json.loads((Path(__file__).resolve().parents[2] /
                            "docs/pci-mmio-register-evidence.json").read_text(encoding="utf-8"))
        self.assertEqual(matrix["schema_version"], 1)
        self.assertIn("UnknownActive", matrix["safety"])
        names = {r["name"] for r in matrix["registers"]}
        self.assertTrue({"IIMCL", "IIMST", "MTTCTL", "INTST", "INTEN", "START", "SGTA", "IIMTC"} <= names)
        for row in matrix["registers"]:
            for field in ("mmio", "decoded_address", "destination", "status_source", "dma_safety"):
                self.assertIn(row[field]["status"], matrix["labels"])
            self.assertEqual(row["dma_safety"]["status"], "Unknown")
            self.assertEqual(row["mmio"]["width_bits"], 32)

    def test_package_edges(self):
        text = "bond BOND87 {\n pin P1 = IOB_N12_1;\n pin P2 = IOB_W17_1;\n pin P3 = IOB_S7_2;\n pin P4 = IOB_E9_3;\n}\n"
        loc = package_locations(text, dict(zip("abcd", range(1, 5))))
        self.assertEqual([(loc[k]["x"], loc[k]["y"], loc[k]["index"]) for k in "abcd"],
                         [(12, 29, 1), (0, 17, 1), (7, 0, 2), (47, 9, 3)])

    def test_package_rejects_missing_ambiguous_and_non_io(self):
        for text in ("", "bond BOND87 {\n}\nbond BOND87 {\n}\n",
                     "bond BOND87 {\n pin P1 = GND;\n}\n",
                     "bond BOND87 {\n pin P1 = IOB_W2_0;\n pin P1 = IOB_W3_0;\n}\n"):
            with self.assertRaises(ValueError):
                package_locations(text, {"a": 1})

    def test_projection_preserves_gate_and_unaligned_bits(self):
        logic = {n: {"kind": "register"} for n in ("a0", "a2", "gate")}
        logic["decode"] = {"data": {"op": "and", "args": [
            {"source": "a0", "inv": True}, {"source": "a2"}, {"source": "gate"}]}}
        p = address_projection({"logic": logic}, "decode", {"a0": 0, "a2": 2})
        self.assertEqual(p["address_mask"], 5)
        self.assertEqual(p["cases"], [{"other_assignments": [1], "address_values": [4]}])

    def test_projection_fails_closed_on_boundary(self):
        with self.assertRaises(ValueError):
            address_projection({"logic": {"a": {"data": {"unknown": "routing gap"}}}}, "a", {})

    def test_projection_bounds_other_state(self):
        logic = {"g": {"kind": "register"}, "d": {"data": {"source": "g"}}}
        with self.assertRaises(ValueError):
            address_projection({"logic": logic}, "d", {}, max_other_inputs=0)

    def test_capture_group_ignores_route_provenance_only(self):
        loc = {"AD[0]": {"x": 0, "y": 1, "index": 1}}
        logic = {signal(loc["AD[0]"], "IQ"): {"kind": "register"}}
        for i, inv in enumerate((False, False, True)):
            logic[f"q{i}"] = {"kind": "register", "data": {"source": signal(loc["AD[0]"], "IQ")},
                              "controls": {"CE": {"source": "gate", "inv": inv, "route": str(i)}}}
        self.assertEqual([len(g["members"]) for g in capture_candidates({"logic": logic}, loc)], [2, 1])
        self.assertEqual(functional_expression({"route": "x", "inv": True}), {"inv": True})

    def test_cone_retains_bus_alternatives_and_unknowns(self):
        report = {"logic": {"bus": {"kind": "combinational", "data": {"op": "bus", "drivers": [
            {"driver": "d"}]}}, "d": {"kind": "tristate_driver", "disable": {"unknown": "gate gap"},
            "data": {"op": "ram_read", "memory": "ram", "address": {"0": {"source": "q"}}}},
            "q": {"kind": "register", "data": {"source": "pad"}}, "pad": {"kind": "pad_input"}}}
        cut = cone_inventory(report, {"source": "bus"})
        self.assertEqual(cut["memories"], ["ram"])
        self.assertTrue(any(b["reason"] == "gate gap" for b in cut["boundaries"]))
        self.assertNotIn("pad", cut["signals"])
        self.assertIn("pad", cone_inventory(report, {"source": "bus"}, True)["signals"])


@unittest.skipUnless(os.environ.get("WR6K_MMIO_REPORT"), "private MMIO report not configured")
class PrivateMmio(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = Path(os.environ["WR6K_MMIO_REPORT"])
        cls.report = json.loads(path.read_text(encoding="utf-8"))
        if cls.report.get("project_combine_commit") != COMMIT:
            raise ValueError("private report architecture version mismatch")
        image = Path(os.environ["WR6K_FPGA_BINARY_205"])
        if cls.report.get("image_sha256") != hashlib.sha256(image.read_bytes()).hexdigest():
            raise ValueError("private report/image mismatch")
        if cls.report.get("frame_validation", {}).get("status") != "PASS":
            raise ValueError("missing successful frame calibration")
        cls.locations = package_locations((Path(os.environ["WR6K_PRJCOMBINE"]) /
            "databases/virtex.txt").read_text(encoding="utf-8"), {**BOARD_PINS, **LINK_PINS})
        cls.groups = capture_candidates(cls.report, cls.locations)
        cls.address = {m["signal"]: int(m["input_nets"][0][3:-1]) for g in cls.groups
            if g["controls"].get("CE", {}).get("source") == "X7,Y11 SLICE[1].Y"
            for m in g["members"] if len(m["input_nets"]) == 1 and m["input_nets"][0].startswith("AD[")}

    def test_complete_address_bank(self):
        self.assertEqual(set(self.address.values()), set(range(32)))
        for n, bit in self.address.items():
            self.assertEqual(data_support(self.report["logic"][n]["data"], self.report["logic"]),
                             {signal(self.locations[f"AD[{bit}]"], "IQ")})

    def test_configuration_offsets(self):
        for n, offset in {"X7,Y4 SLICE[0].YQ": 0x18, "X7,Y6 SLICE[0].YQ": 0x3C,
                          "X7,Y6 SLICE[1].YQ": 0x10, "X8,Y5 SLICE[0].YQ": 0x0C,
                          "X8,Y6 SLICE[0].YQ": 0x04, "X8,Y6 SLICE[1].YQ": 0x14}.items():
            with self.subTest(offset=offset):
                inputs = {signal(self.locations[f"AD[{i}]"], "IQ"): i for i in range(8)}
                p = address_projection(self.report, n, inputs)
                self.assertEqual((p["address_mask"], p["other_inputs"], p["cases"]),
                    (255, [], [{"other_assignments": [0], "address_values": [offset]}]))

    def test_address_phase_enable_and_config_command(self):
        alias = {signal(v, p): k for k, v in self.locations.items() for p in ("I", "IQ")}
        for n in ("X7,Y11 SLICE[1].Y", "X12,Y15 SLICE[1].X", "X7,Y12 SLICE[0].X"):
            t = state_table(self.report, n)
            self.assertEqual(t["status"], "verified_combinational_table")
            for row in t["rows"]:
                v = {alias.get(k, k): bool(row["address"] & (1 << i)) for i, k in enumerate(t["inputs"])}
                expected = not v["FRAME#"] and v["X12,Y12 SLICE[1].YQ"] and not v["X5,Y5 SLICE[0].YQ"]
                self.assertEqual(row["d"], expected)
        p = address_projection(self.report, "X7,Y25 SLICE[1].YQ", {
            signal(self.locations[f"CBE#[{i}]"], "IQ"): i for i in range(4)})
        self.assertEqual(p["cases"], [{"other_assignments": [0], "address_values": [11]}])

    def test_dma_local_write_predicates(self):
        for n, offset in {"X17,Y13 SLICE[0].Y": 0x40, "X17,Y13 SLICE[1].X": 0x40,
                          "X15,Y12 SLICE[0].X": 0x44, "X16,Y12 SLICE[0].Y": 0x44,
                          "X17,Y13 SLICE[1].Y": 0x48, "X12,Y13 SLICE[0].X": 0x84,
                          "X16,Y8 SLICE[0].X": 0x80, "X17,Y12 SLICE[0].X": 0x0C}.items():
            with self.subTest(offset=offset, signal=n):
                p = address_projection(self.report, n, self.address)
                self.assertEqual(p["address_mask"], 0x1CC)
                self.assertEqual(p["other_inputs"], ["X6,Y16 SLICE[1].YQ", "X7,Y12 SLICE[0].YQ"])
                self.assertEqual(p["cases"], [{"other_assignments": [3], "address_values": [offset]}])

    def test_config_bar_byte_enable_qualification(self):
        for name, offset, byte in (
            ("X7,Y17 SLICE[1].X", 0x18, 1), ("X7,Y17 SLICE[1].Y", 0x10, 1),
            ("X7,Y19 SLICE[0].X", 0x18, 2), ("X7,Y19 SLICE[0].Y", 0x10, 2),
            ("X8,Y19 SLICE[1].X", 0x18, 3), ("X8,Y21 SLICE[1].X", 0x14, 3),
            ("X8,Y21 SLICE[1].Y", 0x10, 3), ("X8,Y21 SLICE[0].Y", 0x14, 2)):
            predicate = {0x10: "X7,Y6 SLICE[1].YQ", 0x14: "X8,Y6 SLICE[1].YQ",
                         0x18: "X7,Y4 SLICE[0].YQ"}[offset]
            byte_input = signal(self.locations[f"CBE#[{byte}]"], "IQ")
            t = state_table(self.report, name, local=True)
            self.assertEqual(t["status"], "verified_combinational_table")
            for row in t["rows"]:
                v = {k: bool(row["address"] & (1 << i)) for i, k in enumerate(t["inputs"])}
                self.assertEqual(row["d"], v["X7,Y25 SLICE[1].YQ"] and
                    v["X6,Y15 SLICE[0].X"] and v[predicate] and not v[byte_input])

    def test_iimcl_replicas_are_write_latches_not_idle_ack(self):
        names = ["X23,Y14 SLICE[1].YQ", "X23,Y20 SLICE[1].YQ", "X23,Y25 SLICE[1].YQ"]
        for n in names:
            node = self.report["logic"][n]
            self.assertEqual(node["controls"]["CE"]["source"], "X17,Y13 SLICE[1].Y")
            self.assertEqual(node["data"]["source"], "X21,Y1 TBUS.OUT")
            self.assertEqual(register_contract(n, node)["status"], "modeled_async_ff")
            for old in (0, 1):
                for ce in (0, 1):
                    for data in (0, 1):
                        for reset in (0, 1):
                            values = {n: old, "X17,Y13 SLICE[1].Y": ce,
                                      "X21,Y1 TBUS.OUT": data, "X12,Y29 IOI[1].I": not reset}
                            step = register_step(self.report, n, values, clock_edge=True)
                            self.assertEqual(step["status"], "resolved")
                            self.assertEqual(step["next"], 0 if reset else data if ce else old)
        # BAR0's bit-20 mixed route must remain unresolved, not tied to AD20.
        self.assertTrue(cone_inventory(self.report, {"source": "X7,Y25 SLICE[1].XB"})["boundaries"])

    def test_readback_retains_runtime_memory_and_remote_clock(self):
        for i in range(32):
            root = self.report["roots"][f"AD[{i}]"]["O"]
            cut = cone_inventory(self.report, {"source": root}, expand_registers=True)
            self.assertTrue(cut["memories"], f"AD[{i}] runtime array must not become INIT constant")
        node = self.report["logic"]["X11,Y8 SLICE[0].YQ"]
        self.assertEqual(node["controls"]["CLK"]["source"], "X24,Y0 BUFGCE[0].O")

    def test_completion_source_and_post_clear_state_equations(self):
        for name, expected in (
            ("X22,Y11 SLICE[0].YQ", lambda v: (v["X23,Y14 SLICE[1].YQ"] and
                v["X22,Y11 SLICE[0].YQ"]) or (v["X26,Y18 SLICE[0].YQ"] and
                v["X22,Y21 SLICE[1].X"])),
            ("X25,Y20 SLICE[1].Y", lambda v: not v["X23,Y14 SLICE[1].YQ"] and
                (v["X22,Y11 SLICE[0].YQ"] or v["X25,Y20 SLICE[1].XQ"]))):
            t = state_table(self.report, name, local=True)
            self.assertEqual(t["status"], "verified_combinational_table")
            for row in t["rows"]:
                v = {k: bool(row["address"] & (1 << i)) for i, k in enumerate(t["inputs"])}
                self.assertEqual(row["d"], expected(v))

    def test_transmit_pairs_have_complementary_registered_data(self):
        for i in (0, 1, 2, 3, 4, 10):
            p, n = [self.report["logic"][self.report["roots"][f"TX_D{i}_{pol}"]["O"]]
                    for pol in ("P", "N")]
            self.assertEqual(p["data"]["source"], n["data"]["source"])
            self.assertFalse(p["data"]["inv"])
            self.assertTrue(n["data"]["inv"])
            self.assertEqual(p["controls"]["OCLK"]["source"], "X24,Y0 BUFGCE[1].O")

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC") == "1", "optional Z3 regression")
    def test_conditional_clear_is_not_an_abstract_bus_idle_certificate(self):
        from virtexe_symbolic import System, z3
        s = System(self.report, bound=1, powerup=False, timeout_ms=30000)
        pci = {"source": "X24,Y29 BUFGCE[1].O", "inv": False}
        s.solver.add(s.edge(pci, 0))
        for t in (0, 1):
            s.solver.add(s.signal("X12,Y29 IOI[1].I", t))
        for n in ("X23,Y14 SLICE[1].YQ", "X23,Y20 SLICE[1].YQ", "X23,Y25 SLICE[1].YQ"):
            s.solver.add(s.state(n, 0), z3.Not(s.state(n, 1)))
        s.solver.add(s.signal("X17,Y13 SLICE[1].Y", 0))
        s.solver.add(z3.Not(s.signal("X21,Y1 TBUS.OUT", 0)))
        roots = self.report["roots"]["REQ#"]
        s.solver.add(z3.Not(s.signal(roots["O"], 1)), z3.Not(s.signal(roots["T"], 1)))
        result = s.solver.check()
        print(f"Private conditional IIMCL clear/REQ-active: {result}; abstract unknowns={len(s.unknowns)}")
        self.assertEqual(result, z3.sat)
        self.assertTrue(s.unknowns, "do not report this abstract witness as hardware reachability")


if __name__ == "__main__":
    unittest.main()
