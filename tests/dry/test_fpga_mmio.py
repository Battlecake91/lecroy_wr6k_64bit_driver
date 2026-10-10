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
from virtexe_routing import (COMMIT, classify_pip, data_support, evaluate_data,
                            register_contract, register_step, state_table)


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

    def test_ad20_and_devsel_are_resolved_by_tile_owned_hex_buffers(self):
        node = self.report["logic"]["X7,Y23 SLICE[0].YQ"]
        self.assertEqual(node["data"]["source"], signal(self.locations["AD[20]"], "IQ"))
        for name in ("X7,Y25 SLICE[1].XB", "X2,Y17 SLICE[0].X", "X4,Y15 SLICE[0].X"):
            self.assertFalse(cone_inventory(self.report, {"source": name})["boundaries"])
        route = self.report["routes"]['[7, 23, "IMUX_CLB_BY[0]"]']
        self.assertEqual(route["driver_status"], "resolved")
        self.assertEqual(route["terminals"], [{"x": 0, "y": 24, "bel": "IOI[1]", "pin": "IQ"}])

    def test_native_hex_feature_ownership_in_both_enabled_cases(self):
        from virtexe_xc2s200e_decode import Xc2s200eBitstream
        image = Xc2s200eBitstream(Path(os.environ["WR6K_FPGA_BINARY_205"]))
        route = self.report["routes"]['[7, 23, "IMUX_CLB_BY[0]"]']
        mux = route["nodes"]['[0, 24, "HEX_H3_MUX[0]"]']
        io = next(p for p in mux["pips"] if p["source_raw"][2] == "OUT_IO_IQ[1]")
        bram = next(p for p in mux["pips"] if p["source_raw"][2] == "BRAM_QUAD_DOUT[7]")
        self.assertEqual(io["tile"], [0, 24, "IO_W"])
        self.assertEqual(bram["tile"], [1, 21, "BRAM_W"])
        self.assertEqual(classify_pip(image, io)["status"], "active")
        self.assertEqual(classify_pip(image, bram)["status"], "disabled")
        for p, expected in ((io, (2226, 436, False)), (bram, (2119, 443, True))):
            bit = p["config"][0]["owner_buffers"][0]["bits"][0]
            self.assertEqual((bit["frame"], bit["bit"], bit["inv"]), expected)
            image.frames[bit["frame"]][bit["bit"]] ^= 1
        # In-memory mutation only: the real image is never changed or programmed.
        self.assertEqual(classify_pip(image, io)["status"], "disabled")
        self.assertEqual(classify_pip(image, bram)["status"], "active")

    def test_read_selection_and_tbus_gate_equations(self):
        equations = {
            "X11,Y10 SLICE[0].YQ": lambda v: v["X7,Y12 SLICE[0].YQ"] and not v["X6,Y16 SLICE[1].YQ"],
            "X11,Y12 SLICE[1].YQ": lambda v: v["X6,Y14 SLICE[0].YQ"] and not v["X6,Y16 SLICE[1].YQ"],
            "X11,Y11 SLICE[0].YQ": lambda v: v["X6,Y12 SLICE[1].YQ"] and not v["X6,Y16 SLICE[1].YQ"],
            "X11,Y10 SLICE[0].X": lambda v: not v["X5,Y15 SLICE[0].XQ"] or v["X7,Y12 SLICE[0].YQ"],
            "X11,Y12 SLICE[0].Y": lambda v: not v["X5,Y15 SLICE[0].XQ"] or v["X6,Y14 SLICE[0].YQ"],
            "X11,Y11 SLICE[0].X": lambda v: not v["X5,Y15 SLICE[0].XQ"] or v["X6,Y12 SLICE[1].YQ"],
            "X11,Y11 SLICE[1].X": lambda v: not v["X5,Y15 SLICE[0].XQ"] or not any(v[n] for n in (
                "X11,Y10 SLICE[0].YQ", "X11,Y12 SLICE[1].YQ", "X11,Y11 SLICE[0].YQ")),
        }
        for n, expected in equations.items():
            t = state_table(self.report, n, local=True)
            self.assertEqual(t["status"], "verified_combinational_table")
            for row in t["rows"]:
                v = {k: bool(row["address"] & (1 << i)) for i, k in enumerate(t["inputs"])}
                self.assertEqual(row["d"], expected(v), n)
        driver = self.report["logic"]["X10,Y1 TBUF[0].O"]
        self.assertEqual(driver["disable"]["source"], "X11,Y11 SLICE[1].X")
        self.assertEqual(driver["data"]["source"], "X11,Y7 SLICE[0].Y")

    def test_all_ad_output_registers_require_unmodeled_pci_ce(self):
        ce = "X0,Y13 PCILOGIC.PCI_CE"
        self.assertEqual(self.report["logic"][ce]["kind"], "architecture_boundary")
        for bit in range(32):
            name = self.report["roots"][f"AD[{bit}]"]["O"]
            node = self.report["logic"][name]
            self.assertEqual(node["controls"]["OCE"]["source"], ce)
            self.assertEqual(node["controls"]["OCLK"]["source"], "X24,Y29 BUFGCE[1].O")
            self.assertEqual(register_contract(name, node)["status"], "modeled_async_ff")
            for old in (0, 1):
                # Hold must not evaluate the unselected runtime TBUS/RAM data.
                step = register_step(self.report, name, {name: old, ce: 0,
                    "X12,Y29 IOI[1].I": 1}, clock_edge=True)
                self.assertEqual((step["status"], step["next"]), ("resolved", old))
        for net in ("DEVSEL#", "TRDY#"):
            for output in ("O", "T"):
                name = self.report["roots"][net][output]
                contract = register_contract(name, self.report["logic"][name])
                self.assertEqual(contract["status"], "modeled_async_ff")
                self.assertEqual(contract["clock"]["source"], "X24,Y29 BUFGCE[1].O")
        self.assertEqual(functional_expression(self.report["logic"][self.report["roots"]["DEVSEL#"]["T"]]["data"]),
                         functional_expression(self.report["logic"][self.report["roots"]["TRDY#"]["T"]]["data"]))

    def test_bar1_address_and_payload_staging(self):
        logic = self.report["logic"]
        staged = {n: self.address[d["data"]["source"]] for n, d in logic.items()
            if d.get("kind") == "register" and d.get("controls", {}).get("CE", {}).get("source") ==
            "X16,Y11 SLICE[0].Y" and d.get("data", {}).get("source") in self.address}
        self.assertEqual(set(staged.values()), set(range(2, 18)))
        self.assertEqual(len(staged), 16)
        for name in ("X16,Y11 SLICE[0].Y", "X11,Y12 SLICE[1].X", "X8,Y12 SLICE[0].X"):
            t = state_table(self.report, name, local=True)
            for row in t["rows"]:
                v = {k: bool(row["address"] & (1 << i)) for i, k in enumerate(t["inputs"])}
                expected = v["X6,Y14 SLICE[0].YQ"] and (
                    not v["X16,Y14 SLICE[1].YQ"] or v["X10,Y14 SLICE[1].YQ"])
                if name != "X16,Y11 SLICE[0].Y":
                    expected = expected and v["X6,Y16 SLICE[1].YQ"]
                self.assertEqual(row["d"], expected, name)
        for gate in ("X11,Y12 SLICE[1].X", "X8,Y12 SLICE[0].X"):
            bank = [d for d in logic.values() if d.get("kind") == "register" and
                    d.get("controls", {}).get("CE", {}).get("source") == gate]
            self.assertEqual(len(bank), 16)
            self.assertTrue(all(d["data"].get("source", "").endswith("TBUS.OUT") for d in bank))

    def test_response_pipeline_crosses_effective_clocks(self):
        logic = self.report["logic"]
        for name, source, clock in (
            ("X11,Y8 SLICE[0].YQ", "X32,Y19 SLICE[1].YQ", "X24,Y0 BUFGCE[0].O"),
            ("X13,Y14 SLICE[1].YQ", "X13,Y16 SLICE[0].YQ", "X24,Y29 BUFGCE[1].O"),
            ("X9,Y19 SLICE[0].YQ", "X13,Y14 SLICE[1].YQ", "X24,Y29 BUFGCE[1].O")):
            self.assertEqual(logic[name]["data"]["source"], source)
            self.assertEqual(logic[name]["controls"]["CLK"]["source"], clock)
        self.assertEqual(logic["X11,Y8 SLICE[0].YQ"]["controls"]["CE"]["source"], "X32,Y20 SLICE[1].YQ")
        t = state_table(self.report, "X13,Y16 SLICE[0].YQ", local=True)
        for row in t["rows"]:
            self.assertEqual(row["d"], bool(row["address"]))

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC") == "1", "optional Z3 regression")
    def test_three_bar_comparators_are_exact_for_all_runtime_values(self):
        from virtexe_symbolic import System, z3
        s = System(self.report, bound=0, powerup=False, timeout_ms=30000)
        q = lambda n: s.signal(n, 0)
        logic = self.report["logic"]
        iq_bits = {signal(self.locations[f"AD[{i}]"], "IQ"): i for i in range(32)}
        for compare, gates, first in (
            ("X7,Y25 SLICE[1].XB", ("X7,Y17 SLICE[1].Y", "X7,Y19 SLICE[0].Y", "X8,Y21 SLICE[1].Y"), 9),
            ("X8,Y27 SLICE[1].YB", ("X8,Y21 SLICE[0].Y", "X8,Y21 SLICE[1].X"), 18),
            ("X6,Y26 SLICE[1].XB", ("X7,Y17 SLICE[1].X", "X7,Y19 SLICE[0].X", "X8,Y19 SLICE[1].X"), 9)):
            bank = {n: iq_bits[d["data"]["source"]] for n, d in logic.items()
                if d.get("kind") == "register" and d.get("controls", {}).get("CE", {}).get("source") in gates}
            self.assertEqual(set(bank.values()), set(range(first, 32)))
            command = z3.Or(*[z3.And(*[q(signal(self.locations[f"CBE#[{i}]"], "IQ")) ==
                bool(cmd & (1 << i)) for i in range(4)]) for cmd in (6, 7, 12, 14, 15)])
            expected = z3.And(command, *[q(n) == q(signal(self.locations[f"AD[{i}]"], "IQ"))
                                        for n, i in bank.items()])
            s.solver.push()
            s.solver.add(q(compare) != expected)
            self.assertEqual(s.solver.check(), z3.unsat, compare)
            s.solver.pop()

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC") == "1", "optional Z3 regression")
    def test_conditional_status_muxes_not_complete_mmio_reads(self):
        from virtexe_symbolic import System, z3
        s = System(self.report, bound=0, powerup=False, timeout_ms=30000)
        q = lambda n: s.signal(n, 0)
        c, a, b, d = [q(n) for n in ("X22,Y11 SLICE[0].YQ", "X11,Y8 SLICE[0].YQ",
                                    "X13,Y14 SLICE[1].YQ", "X9,Y19 SLICE[0].YQ")]
        for bar, offset in ((0, 0x48), (0, 0x4C), (0, 0x80), (0, 0x84), (1, 0x80)):
            s.solver.push()
            s.solver.add(*[q(n) == bool(offset & (1 << i)) for n, i in self.address.items()])
            s.solver.add(q("X7,Y12 SLICE[0].YQ") == (bar == 0),
                         q("X6,Y14 SLICE[0].YQ") == (bar == 1), z3.Not(q("X6,Y12 SLICE[1].YQ")))
            if bar == 1:
                expected = z3.If(z3.And(b, z3.Not(d)), a, q("X11,Y7 SLICE[0].YQ"))
            elif offset < 0x80:
                expected = z3.If(b, z3.If(d, c, a), c)
            else:
                expected = q("X22,Y11 SLICE[0].XQ")
            s.solver.add(q("X11,Y7 SLICE[0].Y") != expected)
            self.assertEqual(s.solver.check(), z3.unsat, (bar, offset))
            s.solver.pop()

    def test_four_transmit_staging_fields_carry_address_or_payload(self):
        logic = self.report["logic"]
        for name, select, phase, address, payload, ad_bit in (
            ("X28,Y5 SLICE[0].XQ", "X28,Y6 SLICE[1].Y", "X25,Y1 SLICE[0].YQ", "X23,Y5 SLICE[0].YQ", "X20,Y3 SLICE[0].YQ", 0),
            ("X27,Y3 SLICE[0].XQ", "X28,Y6 SLICE[1].Y", "X25,Y1 SLICE[0].YQ", "X20,Y6 SLICE[1].YQ", "X23,Y3 SLICE[1].YQ", 2),
            ("X25,Y2 SLICE[0].XQ", "X28,Y6 SLICE[1].Y", "X25,Y1 SLICE[0].YQ", "X23,Y3 SLICE[0].XQ", "X24,Y2 SLICE[0].YQ", 5),
            ("X26,Y5 SLICE[1].XQ", "X28,Y5 SLICE[1].Y", "X25,Y1 SLICE[1].YQ", "X19,Y9 SLICE[0].YQ", "X26,Y5 SLICE[0].YQ", 8)):
            bus = logic[logic[payload]["data"]["source"]]["data"]
            producers = [logic[d["driver"]]["data"].get("source") for d in bus["drivers"]]
            self.assertIn(signal(self.locations[f"AD[{ad_bit}]"], "IQ"), producers)
            for assignment in range(8):
                p, a, v = [bool(assignment & (1 << i)) for i in range(3)]
                values = {select: False, phase: p, address: a, payload: v}
                actual = evaluate_data(logic[name]["data"], logic, values)
                self.assertEqual(actual, v if p else a, (name, assignment))

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
    def test_target_read_handshake_still_needs_pci_ce_behavior(self):
        from virtexe_symbolic import System, clock_key, z3
        bound = 8
        pci = {"source": "X24,Y29 BUFGCE[1].O", "inv": False}
        s = System(self.report, bound=bound, powerup=False, timeout_ms=60000,
                   schedule=[{clock_key(pci)}] * bound)
        q = lambda n, t: s.signal(n, t)
        roots = self.report["roots"]
        # Constructed runtime idle fixture, not boot/configuration reachability:
        # memory decode enabled; disjoint BAR1=0x10000000, BAR2=0x20000000;
        # BAR0=0 and old AD0 output=1. RAM/BRAM arrays remain arbitrary.
        ones = {"X7,Y10 SLICE[0].YQ", "X9,Y26 SLICE[0].YQ",
                "X6,Y27 SLICE[0].XQ", roots["AD[0]"]["O"]}
        for name, contract in s.registers.items():
            s.solver.add(s.state(name, 0) == bool(1 if name in ones else contract["init"]))
        for t in range(bound + 1):
            s.solver.add(q(roots["RST#"]["I"], t), q(roots["GNT#"]["I"], t),
                         z3.Not(q(roots["IDSEL"]["I"], t)))
            s.solver.add(q(roots["FRAME#"]["I"], t) == (t not in (1, 2)),
                         q(roots["IRDY#"]["I"], t) == (t < 3 or t > 5))
            for i in range(4):
                s.solver.add(q(roots[f"CBE#[{i}]"]["I"], t) == bool((6 if t == 1 else 0) & (1 << i)))
            for i in range(32):
                pad = roots[f"AD[{i}]"]
                s.solver.add(q(pad["I"], t) == z3.If(q(pad["T"], t),
                    z3.BoolVal(bool(0x4C & (1 << i))), q(pad["O"], t)))
            for net in ("DEVSEL#", "TRDY#", "STOP#"):
                pad = roots[net]
                s.solver.add(q(pad["I"], t) == z3.If(q(pad["T"], t), z3.BoolVal(True), q(pad["O"], t)))
        logic = self.report["logic"]
        bus = logic["X8,Y1 TBUS.OUT"]["data"]
        ownership = z3.And(*[s.expr(logic[d["driver"]]["disable"], 4) ==
                            (d["driver"] != "X10,Y1 TBUF[0].O") for d in bus["drivers"]])
        s.solver.push()
        s.solver.add(z3.Not(ownership))
        self.assertEqual(s.solver.check(), z3.unsat)
        s.solver.pop()
        for ce in (False, True):
            s.solver.push()
            s.solver.add(*[q("X0,Y13 PCILOGIC.PCI_CE", t) == ce for t in range(bound + 1)])
            s.solver.add(*[z3.Not(q(roots[net][pin], 5)) for net, pin in (
                ("DEVSEL#", "O"), ("DEVSEL#", "T"), ("TRDY#", "O"),
                ("TRDY#", "T"), ("AD[0]", "T"))])
            s.solver.add(q(roots["AD[0]"]["O"], 5) == (not ce))
            self.assertEqual(s.solver.check(), z3.sat)
            s.solver.pop()
        self.assertTrue(any("PCILOGIC.PCI_CE" in reason for reason in s.unknowns.values()))

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
