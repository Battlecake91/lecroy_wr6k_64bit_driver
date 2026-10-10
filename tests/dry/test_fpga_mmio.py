"""Synthetic MMIO analysis tests and opt-in private configuration regressions."""
import hashlib
import json
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
from virtexe_mmio import (BOARD_PINS, LINK_PINS, address_projection, capture_candidates,
                         conditional_mux_field, cone_inventory, functional_expression,
                         package_locations, pcilogic_contract, signal)
from virtexe_routing import (COMMIT, classify_pip, data_support, evaluate_data,
                            register_contract, register_equivalence, register_step, state_table)


class SyntheticMmio(unittest.TestCase):
    def test_complete_lvds_pin_inventory_and_payload_bijection(self):
        for direction in ("TX", "RX"):
            self.assertEqual({n for n in LINK_PINS if n.startswith(direction + "_D")},
                {f"{direction}_D{i}_{p}" for i in range(12) for p in ("P", "N")})
        self.assertEqual(len(set(LINK_PINS.values())), len(LINK_PINS))
        matrix = json.loads((Path(__file__).resolve().parents[2] /
            "docs/pci-mmio-register-evidence.json").read_text(encoding="utf-8"))["lvds_protocol"]
        self.assertEqual([x["lane"] for x in matrix["tx_lanes"]], list(range(12)))
        self.assertEqual(sorted(b for x in matrix["tx_lanes"] for b in x["payload_bits_ABC"]
                                if b is not None), list(range(32)))
        self.assertEqual(len(matrix["rx"]["active_inputs"]), 12)
        self.assertEqual(len({matrix["clocks"][c] for c in ("pci", "remote", "tx_fast", "rx_fast")}), 4)

    def test_pcilogic_ve_has_no_encoded_delay_not_a_zero_delay_model(self):
        for tile in ("PCI_W_VE", "PCI_E_VE"):
            node = {"architecture_class": tile, "configuration": {},
                    "inputs": {"I1": {"source": "a", "inv": True},
                               "I2": {"source": "b", "inv": False}}}
            contract = pcilogic_contract(node)
            self.assertEqual(contract["status"], "Unknown")
            self.assertEqual(contract["delay"]["encoding"], "absent_in_pinned_tile")
            self.assertTrue(contract["inputs"]["I1"]["inv"])
            self.assertFalse(contract["inputs"]["I2"]["inv"])
            self.assertNotIn("data", contract)

    def test_pcilogic_other_family_delay_and_unsupported_cases_remain_unknown(self):
        delay = {"value": 3, "bits": [{"frame": 0, "bit": 0}]}
        c = pcilogic_contract({"architecture_class": "PCI_W_V",
                              "configuration": {"PCI_DELAY": delay}})
        self.assertEqual(c["delay"]["configuration"], delay)
        self.assertEqual(c["delay"]["status"], "Unknown")
        for node in ({}, {"architecture_class": "PCILOGICSE"},
                     {"architecture_class": "PCI_W_VE", "configuration": {"PCI_DELAY": delay}}):
            self.assertEqual(pcilogic_contract(node)["delay"]["status"], "Unknown")

    def test_conditional_field_is_exhaustive_and_does_not_claim_other_branch(self):
        logic = {n: {"kind": "register"} for n in ("address", "payload", "phase", "outer")}
        logic["field"] = {"data": {"op": "mux", "select": {"source": "outer"},
            "zero": {"op": "mux", "select": {"source": "phase"},
                     "zero": {"source": "address"}, "one": {"source": "payload"}},
            "one": {"unknown": "unrelated outer branch"}}}
        r = {"logic": logic}
        c = conditional_mux_field(r, "field", {"address": 4}, {"payload": 2})
        self.assertEqual((c["status"], c["address_bit"], c["tbus_pci_ad_producer_bit"]),
                         ("Verified", 4, 2))
        self.assertEqual(c["condition"]["value"], False)
        logic["field"]["data"]["zero"]["one"]["inv"] = True
        self.assertEqual(conditional_mux_field(r, "field", {"address": 4}, {"payload": 2})["status"], "Unknown")

    def test_conditional_field_rejects_missing_and_nonmux_data(self):
        for data in ({"unknown": "missing route"}, {"source": "unknown"},
                     {"op": "mux", "select": {"source": "outer"},
                      "zero": {"unknown": "unrecovered branch"}, "one": {"constant": 0}}):
            result = conditional_mux_field({"logic": {"f": {"data": data}}}, "f", {}, {})
            self.assertEqual(result["status"], "Unknown")

    def test_public_matrix_keeps_unknown_safety_and_explicit_connections(self):
        matrix = json.loads((Path(__file__).resolve().parents[2] /
                            "docs/pci-mmio-register-evidence.json").read_text(encoding="utf-8"))
        self.assertEqual(matrix["schema_version"], 1)
        self.assertIn("UnknownActive", matrix["safety"])
        self.assertEqual(len(matrix["release_obligations"]), 6)
        self.assertEqual(set(matrix["release_obligations"].values()), {"Unknown"})
        self.assertEqual(matrix["pcilogic"]["status"], "Unknown")
        self.assertEqual(matrix["read_pipeline"]["status"], "Unknown")
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

    def test_pcilogic_hidden_ready_taps_and_no_ve_delay_encoding(self):
        node = self.report["logic"]["X0,Y13 PCILOGIC.PCI_CE"]
        self.assertEqual(node["architecture_class"], "PCI_W_VE")
        self.assertNotIn("PCI_DELAY", node["configuration"])
        for pin, net in (("IRDY", "IRDY#"), ("TRDY", "TRDY#")):
            expected = self.locations[net]
            self.assertEqual(node["dedicated"][pin], {"x": expected["x"], "y": expected["y"],
                "bel": f"IOI[{expected['index']}]", "pin": "PCI"})
        for pin, source in (("I1", "X2,Y14 SLICE[1].Y"), ("I2", "X2,Y12 SLICE[0].Y"),
                            ("I3", "X2,Y3 SLICE[0].Y")):
            self.assertEqual(functional_expression(node["inputs"][pin]), {"source": source, "inv": False})
        contract = pcilogic_contract(node)
        self.assertEqual(contract["status"], "Unknown")
        self.assertEqual(contract["delay"]["encoding"], "absent_in_pinned_tile")

    def test_pcilogic_fabric_input_equations_not_hardblock_function(self):
        equations = {
            "X2,Y14 SLICE[1].Y": lambda v: v["X2,Y14 SLICE[0].XQ"] or not v["X5,Y15 SLICE[0].YQ"],
            "X2,Y12 SLICE[0].Y": lambda v: v["X3,Y13 SLICE[1].Y"] or
                v["X4,Y9 SLICE[1].XQ"] or v["X2,Y21 SLICE[1].XQ"],
            "X2,Y3 SLICE[0].Y": lambda v: v["X2,Y15 SLICE[1].XQ"] or not v["X5,Y5 SLICE[0].YQ"],
        }
        for name, expected in equations.items():
            table = state_table(self.report, name, local=True)
            self.assertEqual(table["status"], "verified_combinational_table")
            for row in table["rows"]:
                values = {n: bool(row["address"] & (1 << i)) for i,n in enumerate(table["inputs"])}
                self.assertEqual(row["d"], expected(values))

    def test_complete_bar1_payload_topology_and_sixteen_conditional_fields(self):
        logic = self.report["logic"]
        address = {n: self.address[d["data"]["source"]] for n, d in logic.items()
            if d.get("kind") == "register" and d.get("controls", {}).get("CE", {}).get("source") ==
            "X16,Y11 SLICE[0].Y" and d.get("data", {}).get("source") in self.address}
        iq = {signal(self.locations[f"AD[{i}]"], "IQ"): i for i in range(32)}
        payload = {}
        for n, node in logic.items():
            if node.get("kind") != "register" or node.get("controls", {}).get("CE", {}).get("source") not in (
                    "X11,Y12 SLICE[1].X", "X8,Y12 SLICE[0].X"):
                continue
            drivers = logic[node["data"]["source"]]["data"]["drivers"]
            bits = [iq[logic[d["driver"]]["data"]["source"]] for d in drivers
                    if logic[d["driver"]]["data"].get("source") in iq]
            self.assertEqual(len(bits), 1)
            payload[n] = bits[0]
        self.assertEqual(len(payload), 32)
        self.assertEqual(set(payload.values()), set(range(32)))
        fields = [conditional_mux_field(self.report, n, address, payload) for n,d in logic.items()
                  if d.get("kind") == "register"]
        fields = [f for f in fields if f["status"] == "Verified"]
        self.assertEqual({(f["address_bit"], f["tbus_pci_ad_producer_bit"]) for f in fields},
                         {(bit + 2, bit) for bit in range(16)})
        self.assertEqual(len(fields), 16)
        matrix = json.loads((Path(__file__).resolve().parents[2] /
                            "docs/pci-mmio-register-evidence.json").read_text(encoding="utf-8"))
        self.assertEqual({(f["signal"], f["address_bit"], f["tbus_pci_ad_producer_bit"]) for f in fields},
                         {(f["stage"], f["address_bit"], f["tbus_pci_ad_producer_bit"])
                          for f in matrix["lvds_command_staging"]["fields"]})
        projection = matrix["lvds_command_staging"]["legacy_address_projection"]
        for key, offset in (("MTTCTL_080_stage_ones",0x80), ("MTTRGO_084_stage_ones",0x84),
                            ("MAMRGO_064_stage_ones",0x64)):
            self.assertEqual(set(projection[key]), {f["signal"] for f in fields
                             if offset & (1 << f["address_bit"])})
        for field in fields:
            self.assertEqual(logic[field["signal"]]["controls"]["CLK"]["source"], "X24,Y0 BUFGCE[0].O")

    def _lvds(self):
        return json.loads((Path(__file__).resolve().parents[2] /
            "docs/pci-mmio-register-evidence.json").read_text(encoding="utf-8"))["lvds_protocol"]

    def test_twelve_tx_lanes_and_checked_phase_shadows(self):
        protocol, logic = self._lvds(), self.report["logic"]
        controller = protocol["tx_controller"]
        for group in (controller["p"], controller["e"], protocol["command_phase"]):
            for n in group[1:]:
                register_equivalence(self.report, group[0], n)
        for lane in protocol["tx_lanes"]:
            ppad = logic[self.report["roots"][f'TX_D{lane["lane"]}_P']["O"]]
            npad = logic[self.report["roots"][f'TX_D{lane["lane"]}_N']["O"]]
            self.assertEqual(ppad["data"]["source"], npad["data"]["source"])
            self.assertNotEqual(ppad["data"]["inv"], npad["data"]["inv"])
            fast = logic[ppad["data"]["source"]]
            for node, pin in ((ppad, "OCLK"), (npad, "OCLK"), (fast, "CLK")):
                self.assertEqual(node["controls"][pin]["source"], protocol["clocks"]["tx_fast"])
            for bits in range(32):
                p, e = bool(bits & 1), bool(bits & 2)
                values = {**dict.fromkeys(controller["p"], p), **dict.fromkeys(controller["e"], e),
                    lane["A"]: bool(bits & 4), lane["B"]: bool(bits & 8), lane["C"]: bool(bits & 16)}
                expected = values[lane["A"]] if p == e else values[lane["C"]] if e else values[lane["B"]]
                self.assertEqual(evaluate_data(fast["data"], logic, values), expected)
            for slot, stage in zip("AB", lane["stages"]):
                node = logic[lane[slot]]
                self.assertEqual(node["data"]["source"], stage)
                gate = node["controls"]["CE"]
                for p in (False, True):
                    for e in (False, True):
                        values = {**dict.fromkeys(controller["p"], p), **dict.fromkeys(controller["e"], e)}
                        self.assertEqual(evaluate_data(gate, logic, values), not p and e)

    def test_tx_controller_equations_reset_and_sync_contract(self):
        protocol, logic = self._lvds(), self.report["logic"]
        c = protocol["tx_controller"]
        for p in (False, True):
            for e in (False, True):
                for kick in (False, True):
                    values = {**dict.fromkeys(c["p"], p), **dict.fromkeys(c["e"], e),
                              c["kick"]: kick, c["reset"]: False}
                    for name in c["p"] + c["e"]:
                        expected = (p != e) if name in c["p"] else p or (not e and kick)
                        self.assertEqual(register_step(self.report, name, values, clock_edge=True)["next"], expected)
                        self.assertEqual(register_step(self.report, name, dict(values, **{c["reset"]: True}),
                                                       clock_edge=False)["next"], 0)
        name = c["sync_register"]
        self.assertEqual(register_contract(name, logic[name])["status"], "modeled_sync_ff")
        self.assertEqual(logic[name]["controls"]["SR"]["source"], c["p"][0])
        self.assertEqual(logic[name]["data"]["source"], c["e"][0])

    def test_tx_control_fields_and_nine_word_counter_not_opcodes(self):
        logic = self.report["logic"]
        names = ["X27,Y4 SLICE[0].XQ", "X27,Y4 SLICE[0].YQ", "X27,Y5 SLICE[0].XQ",
                 "X27,Y5 SLICE[0].YQ", "X27,Y6 SLICE[0].XQ"]
        for n in names:
            c = register_contract(n, logic[n])
            self.assertEqual(c["init"], 0)
            self.assertEqual(c["ce"]["source"], "X27,Y6 SLICE[1].X")
            self.assertEqual(c["clock"]["source"], self._lvds()["clocks"]["remote"])
        for value in range(32):
            values = {n: bool(value & (1 << i)) for i,n in enumerate(names)}
            actual = sum(int(evaluate_data(logic[n]["data"],logic,values)) << i for i,n in enumerate(names))
            self.assertEqual(actual, 0 if value in (8,31) else value+1)
            self.assertEqual(evaluate_data(logic["X27,Y4 SLICE[1].X"]["data"],logic,values), value != 8)
        q, v, z = "X27,Y4 SLICE[1].X", "X39,Y3 SLICE[1].YQ", "X28,Y5 SLICE[1].X"
        for bits in range(8):
            values = {q: bool(bits&1), v: bool(bits&2), z: bool(bits&4)}
            self.assertEqual(evaluate_data(logic["X28,Y5 SLICE[1].YQ"]["data"],logic,values),
                             not (values[q] and values[v] and not values[z]))
            self.assertEqual(evaluate_data(logic["X27,Y4 SLICE[1].YQ"]["data"],logic,values),
                             values[q] or not values[v])
        # Position 31 in address mode is a control flag; do not call it W.
        data = logic["X28,Y7 SLICE[1].XQ"]["data"]["zero"]
        phase, payload, edge, flag = ("X25,Y1 SLICE[1].XQ", "X18,Y13 SLICE[1].XQ",
                                    "X25,Y5 SLICE[0].X", "X28,Y2 SLICE[1].XQ")
        for bits in range(16):
            values = {phase: bool(bits&1), payload: bool(bits&2), edge: bool(bits&4), flag: bool(bits&8)}
            self.assertEqual(evaluate_data(data,logic,values), values[payload] if values[phase]
                             else values[edge] or values[flag])

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC"), "SMT suite not enabled")
    def test_actual_tx_controller_bounded_three_slot_cycle_and_sync_alignment(self):
        import z3
        from virtexe_symbolic import System, clock_key
        protocol, logic = self._lvds(), self.report["logic"]
        c = protocol["tx_controller"]
        keep = set(c["p"] + c["e"] + [c["sync_register"]])
        for net in [f"TX_D{i}_{p}" for i in range(12) for p in ("P", "N")] + ["TX_SYNC_P", "TX_SYNC_N"]:
            pad = self.report["roots"][net]["O"]
            keep.add(pad)
            keep.add(logic[pad]["data"]["source"])
        for lane in protocol["tx_lanes"]:
            keep.update((lane["A"], lane["B"]))
        # Only the actual TX-clock registers transition. Remote snapshots/reset/kick
        # remain explicit external cuts; no assumed DLL ratio or command-valid bit.
        reduced = dict(self.report, logic={n: ({"kind": "pad_input"} if d["kind"] == "register"
            and n not in keep else d) for n,d in logic.items()})
        domain = clock_key({"source": protocol["clocks"]["tx_fast"], "inv": False})
        s = System(reduced, bound=8, powerup=False, schedule=[{domain}] * 8)
        for n in c["p"] + c["e"]:
            s.solver.add(z3.Not(s.state(n, 0)))
        for t in range(9):
            s.solver.add(z3.Not(s.signal(c["reset"], t)))
            s.solver.add(s.signal(c["kick"], t) == (t == 0))
        self.assertEqual(s.solver.check(), z3.sat)
        mismatches = []
        for t, (p,e) in enumerate([(0,0), (0,1), (1,0), (1,1), (0,1), (1,0), (1,1), (0,1), (1,0)]):
            mismatches.extend(s.state(n,t) != bool(p) for n in c["p"])
            mismatches.extend(s.state(n,t) != bool(e) for n in c["e"])
        sync_pad = self.report["roots"]["TX_SYNC_P"]["O"]
        for t in range(2,9):
            mismatches.append(s.state(sync_pad,t) != (t in (3,6)))
        s.solver.add(z3.Or(*mismatches))
        self.assertEqual(s.solver.check(), z3.unsat)
        print("Private actual TX controller: 8-edge cycle/shadows/SYNC mismatch UNSAT; external remote snapshots")

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC"), "SMT suite not enabled")
    def test_all_32_tx_fields_and_two_conditional_parity_functions(self):
        import z3
        from virtexe_symbolic import System
        protocol, logic = self._lvds(), self.report["logic"]
        iq = {signal(self.locations[f"AD[{i}]"], "IQ"): i for i in range(32)}
        payload = {}
        for name,node in logic.items():
            if node.get("controls", {}).get("CE", {}).get("source") not in (
                    "X11,Y12 SLICE[1].X", "X8,Y12 SLICE[0].X"):
                continue
            drivers = logic[node["data"]["source"]]["data"]["drivers"]
            bits = [iq[logic[d["driver"]]["data"]["source"]] for d in drivers
                    if logic[d["driver"]]["data"].get("source") in iq]
            self.assertEqual(len(bits), 1)
            payload[bits[0]] = name
        fields = {bit: stage for lane in protocol["tx_lanes"]
                  for bit,stage in zip(lane["payload_bits_ABC"], lane["stages"]) if bit is not None}
        self.assertEqual(set(fields), set(range(32)))
        # Cut only the named combinational mode selectors. The assertion is a
        # cofactor identity, not reachability or ownership of the TBUS snapshot.
        projected = dict(self.report, logic=dict(logic))
        cuts = protocol["pci_mode_condition"]["cuts_zero"]
        for name in cuts:
            projected["logic"][name] = {"kind": "pad_input"}
        s = System(projected, bound=0, powerup=False, timeout_ms=30000)
        s.solver.add(*[z3.Not(s.signal(n,0)) for n in cuts])
        by_address = {n: self.address[d["data"]["source"]] for n,d in logic.items()
            if d.get("controls", {}).get("CE", {}).get("source") == "X16,Y11 SLICE[0].Y"
            and d.get("data", {}).get("source") in self.address}
        address = {bit: n for n,bit in by_address.items()}
        for phase in (False, True):
            s.solver.push()
            s.solver.add(*[s.signal(n,0) == phase for n in protocol["command_phase"]])
            self.assertEqual(s.solver.check(), z3.sat)
            mismatches = []
            for bit,stage in fields.items():
                actual = s.expr(logic[stage]["data"],0)
                if phase:
                    expected = s.signal(payload[bit],0)
                elif bit < 16:
                    expected = s.signal(address[bit+2],0)
                elif bit < 31:
                    expected = z3.BoolVal(False)
                else:
                    continue  # Distinct control expression, not address bit 33.
                mismatches.append(actual != expected)
            s.solver.add(z3.Or(*mismatches))
            self.assertEqual(s.solver.check(), z3.unsat)
            s.solver.pop()
        for parity, bits, extra in (("X26,Y6 SLICE[0].YQ", range(1,32,2), None),
                                    ("X26,Y6 SLICE[1].YQ", range(0,32,2), "X27,Y4 SLICE[1].YQ")):
            expected = z3.BoolVal(True)
            for bit in bits:
                expected = z3.Xor(expected, s.expr(logic[fields[bit]]["data"],0))
            if extra:
                expected = z3.Xor(expected, s.expr(logic[extra]["data"],0))
            s.solver.push()
            s.solver.add(s.expr(logic[parity]["data"],0) != expected)
            self.assertEqual(s.solver.check(), z3.unsat)
            s.solver.pop()
        print("Private LVDS cofactors: 32 payload/31 address-mode positions and two parity mismatch queries UNSAT")

    def test_all_rx_lanes_three_positions_and_two_bank_structure(self):
        protocol, logic = self._lvds(), self.report["logic"]
        for net in protocol["rx"]["active_inputs"]:
            iq = self.report["roots"][net]["IQ"]
            self.assertEqual(logic[iq]["kind"], "register")
            self.assertEqual(logic[iq]["controls"]["ICLK"]["source"], protocol["clocks"]["rx_fast"])
            pipeline, pending, banks = {iq: 0}, [iq], []
            while pending:
                source = pending.pop()
                for name,node in logic.items():
                    if node.get("data", {}).get("source") != source or node.get("controls", {}).get(
                            "CLK", {}).get("source") != protocol["clocks"]["rx_fast"]:
                        continue
                    ce = node["controls"]["CE"]
                    if ce.get("constant") == 1 and not ce.get("inv", False):
                        if name not in pipeline:
                            pipeline[name] = pipeline[source]+1
                            pending.append(name)
                    else:
                        banks.append((name, pipeline[source]))
            self.assertEqual(sorted(pipeline.values()), [0,1,2], net)
            self.assertEqual(sorted(depth for _,depth in banks), [0,0,1,1,2,2], net)
        for n in ("X32,Y20 SLICE[0].XQ", "X32,Y20 SLICE[0].YQ"):
            register_equivalence(self.report, "X32,Y20 SLICE[1].YQ", n)
        for n in ("X36,Y15 SLICE[0].YQ", "X36,Y15 SLICE[1].YQ"):
            register_equivalence(self.report, "X36,Y15 SLICE[1].XQ", n)

    def test_rx_R_input_banks_mux_and_enable_not_dma_ack(self):
        logic = self.report["logic"]
        for name,source in (("X34,Y22 SLICE[0].XQ", self.report["roots"]["RX_D1_P"]["IQ"]),
                ("X36,Y21 SLICE[0].XQ", "X34,Y22 SLICE[0].XQ"),
                ("X36,Y21 SLICE[1].XQ", "X36,Y21 SLICE[0].XQ"),
                ("X38,Y20 SLICE[1].XQ", "X36,Y21 SLICE[0].XQ")):
            self.assertEqual(logic[name]["data"]["source"], source)
            self.assertEqual(logic[name]["controls"]["CLK"]["source"], self._lvds()["clocks"]["rx_fast"])
        selector, a, b = "X36,Y15 SLICE[1].XQ", "X36,Y21 SLICE[1].XQ", "X38,Y20 SLICE[1].XQ"
        for bits in range(8):
            values = {selector: bool(bits&1), a: bool(bits&2), b: bool(bits&4)}
            self.assertEqual(evaluate_data(logic["X36,Y20 SLICE[1].Y"]["data"], logic, values),
                             values[a] if values[selector] else values[b])
        gate, a, b = "X35,Y16 SLICE[0].Y", "X35,Y21 SLICE[1].X", "X35,Y22 SLICE[1].X"
        for bits in range(8):
            # Explicit local cuts; no response semantics assigned to these fields.
            values = {gate: bool(bits&1), a: bool(bits&2), b: bool(bits&4)}
            self.assertEqual(evaluate_data(logic["X32,Y20 SLICE[0].X"]["data"], logic, values),
                             values[gate] and not values[a] and values[b])
        self.assertEqual(logic["X27,Y27 SLICE[0].YQ"]["data"]["inputs"]["2"]["source"], "X46,Y29 DLL.LOCKED")
        self.assertIn("Unknown", self._lvds()["rx"]["acknowledgement_meaning"])
        g, h, sync = "X36,Y14 SLICE[0].YQ", "X40,Y16 SLICE[0].YQ", "X47,Y18 IOI[1].IQ"
        for bits in range(8):
            values = {g: bool(bits&1), h: bool(bits&2), sync: bool(bits&4)}
            self.assertEqual(evaluate_data(logic[h]["data"],logic,values), values[g] and not values[h])
            self.assertEqual(evaluate_data(logic[h]["controls"]["CE"],logic,values), not values[g] or values[sync])
            self.assertEqual(evaluate_data(logic["X40,Y16 SLICE[1].XQ"]["data"],logic,values),
                             values[g] and values[h] and values[sync])
            self.assertEqual(evaluate_data(logic["X40,Y17 SLICE[1].XQ"]["data"],logic,values),
                             values[g] and not values[h] and values[sync])

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC"), "SMT suite not enabled")
    def test_complete_rx_word_to_conditional_bar1_read_mux(self):
        import z3
        from virtexe_symbolic import System
        protocol, logic = self._lvds(), self.report["logic"]
        received = {n for n,d in logic.items() if d.get("controls", {}).get("CE", {}).get("source")
            in ("X32,Y20 SLICE[0].XQ", "X32,Y20 SLICE[0].YQ", "X32,Y20 SLICE[1].YQ")
            and d.get("data", {}).get("source")}
        self.assertEqual(len(received), 32)
        iq = {self.report["roots"][net]["IQ"]: int(net.split("_")[1][1:])
              for net in protocol["rx"]["active_inputs"]}
        positions = {bit: (lane["lane"], depth) for lane in protocol["tx_lanes"]
                     for bit,depth in zip(lane["payload_bits_ABC"], (0,1,2)) if bit is not None}
        s = System(self.report, bound=0, powerup=False, timeout_ms=30000)
        q = lambda n: s.signal(n,0)
        s.solver.add(*[q(n) == bool(0x80 & (1 << bit)) for n,bit in self.address.items()])
        s.solver.add(z3.Not(q("X7,Y12 SLICE[0].YQ")), q("X6,Y14 SLICE[0].YQ"),
            z3.Not(q("X6,Y12 SLICE[1].YQ")), q("X13,Y14 SLICE[1].YQ"), z3.Not(q("X9,Y19 SLICE[0].YQ")))
        self.assertEqual(s.solver.check(), z3.sat)
        used_received, mismatches = set(), []
        for bit in range(32):
            pad = logic[self.report["roots"][f"AD[{bit}]"]["O"]]
            candidates = []
            for n in cone_inventory(self.report, pad["data"])["signals"]:
                if logic[n].get("kind") != "tristate_driver":
                    continue
                sources = set(cone_inventory(self.report, logic[n]["data"])["signals"]) & received
                if sources:
                    self.assertEqual(len(sources), 1)
                    candidates.append((logic[n]["data"], next(iter(sources))))
            self.assertEqual(len(candidates), 1, bit)
            expression, capture = candidates[0]
            used_received.add(capture)
            mismatches.append(s.expr(expression,0) != q(capture))
            stage = logic[capture]["data"]["source"]
            self.assertEqual(logic[stage]["controls"]["CLK"]["source"], protocol["clocks"]["remote"])
            banks = data_support(logic[stage]["data"], logic) - {
                "X36,Y15 SLICE[0].YQ", "X36,Y15 SLICE[1].XQ", "X36,Y15 SLICE[1].YQ"}
            self.assertEqual(len(banks), 2)
            for bank in banks:
                current, depth = logic[bank]["data"]["source"], 0
                self.assertEqual(logic[bank]["controls"]["CLK"]["source"], protocol["clocks"]["rx_fast"])
                while current not in iq:
                    node = logic[current]
                    self.assertEqual(node["controls"]["CLK"]["source"], protocol["clocks"]["rx_fast"])
                    self.assertEqual(functional_expression(node["controls"]["CE"]), {"constant": 1, "inv": False})
                    current, depth = node["data"]["source"], depth+1
                    self.assertLessEqual(depth, 2)
                self.assertEqual((iq[current], depth), positions[bit])
        self.assertEqual(used_received, received)
        s.solver.add(z3.Or(*mismatches))
        self.assertEqual(s.solver.check(), z3.unsat)
        print("Private all 32 RX bits: lane/time-position mapping checked; BAR1 local read-mux mismatch UNSAT, not accepted PCI read")

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

    def test_bar1_read_write_capture_has_its_own_gate(self):
        logic = self.report["logic"]
        node = logic["X12,Y17 SLICE[1].YQ"]
        self.assertEqual(node["data"]["source"], "X6,Y16 SLICE[1].YQ")
        self.assertEqual(node["controls"]["CE"]["source"], "X11,Y15 SLICE[1].X")
        self.assertEqual(node["controls"]["CLK"]["source"], "X24,Y29 BUFGCE[1].O")
        table = state_table(self.report, "X11,Y15 SLICE[1].X", local=True)
        for row in table["rows"]:
            v = {n: bool(row["address"] & (1 << i)) for i,n in enumerate(table["inputs"])}
            self.assertEqual(row["d"], v["X6,Y14 SLICE[0].YQ"] and
                not v["X16,Y14 SLICE[1].YQ"] and not v["X10,Y14 SLICE[1].YQ"])

    def test_six_fast_transmit_muxes_are_exact_not_packet_validity(self):
        # Source groups A/B/C, not opcodes or temporal packet slots.
        groups = {
            0: ("X32,Y5 SLICE[1].YQ", "X35,Y5 SLICE[0].YQ", "X26,Y6 SLICE[1].YQ"),
            1: ("X36,Y5 SLICE[1].YQ", "X35,Y5 SLICE[0].XQ", "X28,Y5 SLICE[0].XQ"),
            2: ("X32,Y3 SLICE[1].YQ", "X34,Y3 SLICE[1].YQ", "X27,Y3 SLICE[0].XQ"),
            3: ("X32,Y3 SLICE[1].XQ", "X31,Y2 SLICE[0].YQ", "X25,Y2 SLICE[0].XQ"),
            4: ("X34,Y4 SLICE[0].YQ", "X31,Y4 SLICE[0].YQ", "X26,Y5 SLICE[1].XQ"),
            10: ("X31,Y6 SLICE[0].YQ", "X32,Y6 SLICE[1].YQ", "X26,Y10 SLICE[0].YQ"),
        }
        for bit,(a,b,c) in groups.items():
            root = self.report["roots"][f"TX_D{bit}_P"]["O"]
            pad = self.report["logic"][root]
            fast = pad["data"]["source"]
            self.assertEqual(functional_expression(pad["controls"]["OCE"]), {"constant": 1, "inv": False})
            if bit < 4:
                p,e,select = "X31,Y3 SLICE[1].YQ", "X36,Y2 SLICE[0].YQ", "X36,Y3 SLICE[1].XQ"
            else:
                p,e = "X31,Y3 SLICE[1].XQ", "X36,Y3 SLICE[1].YQ"
                select = "X36,Y3 SLICE[1].XQ" if bit == 4 else "X36,Y2 SLICE[1].YQ"
            table = state_table(self.report, fast)
            self.assertEqual(table["status"], "verified_combinational_table")
            self.assertEqual(set(table["inputs"]), {p,e,select,a,b,c})
            self.assertEqual(len(table["rows"]), 64)
            for row in table["rows"]:
                v = {n: bool(row["address"] & (1 << i)) for i,n in enumerate(table["inputs"])}
                self.assertEqual(row["d"], v[a] if v[p] == v[e] else v[c] if v[select] else v[b])

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

    def target_read_fixture(self, bar=0, offset=0x4C, wait_until=3):
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
                         q(roots["IRDY#"]["I"], t) == (t < wait_until or t > max(5, wait_until)))
            for i in range(4):
                s.solver.add(q(roots[f"CBE#[{i}]"]["I"], t) == bool((6 if t == 1 else 0) & (1 << i)))
            for i in range(32):
                pad = roots[f"AD[{i}]"]
                s.solver.add(q(pad["I"], t) == z3.If(q(pad["T"], t),
                    z3.BoolVal(bool(((0x10000000 if bar else 0) + offset) & (1 << i))), q(pad["O"], t)))
            for net in ("DEVSEL#", "TRDY#", "STOP#"):
                pad = roots[net]
                s.solver.add(q(pad["I"], t) == z3.If(q(pad["T"], t), z3.BoolVal(True), q(pad["O"], t)))
        return s

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC") == "1", "optional Z3 regression")
    def test_target_read_handshake_still_needs_pci_ce_behavior(self):
        from virtexe_symbolic import z3
        bound = 8
        s = self.target_read_fixture()
        q = lambda n, t: s.signal(n, t)
        roots = self.report["roots"]
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
    def test_four_local_reads_joint_decode_handshake_and_conditional_load(self):
        from virtexe_symbolic import z3
        roots, logic = self.report["roots"], self.report["logic"]
        commands = {m["signal"]: int(m["input_nets"][0][5:-1]) for g in self.groups
            if g["controls"].get("CE", {}).get("source") == "X12,Y15 SLICE[1].X"
            for m in g["members"] if len(m["input_nets"]) == 1 and m["input_nets"][0].startswith("CBE#[")}
        self.assertEqual(set(commands.values()), set(range(4)))
        for offset in (0x48, 0x4C, 0x80, 0x84):
            s = self.target_read_fixture(offset=offset)
            q = lambda n,t: s.signal(n,t)
            # CE=1 is a diagnostic assumption, not a recovered hardblock model.
            s.solver.add(*[q("X0,Y13 PCILOGIC.PCI_CE",t) for t in range(9)])
            self.assertEqual(s.solver.check(), z3.sat, hex(offset))
            expected = [q("X7,Y12 SLICE[0].YQ",3), z3.Not(q("X6,Y14 SLICE[0].YQ",3)),
                        z3.Not(q("X6,Y16 SLICE[1].YQ",3))]
            expected.extend(q(n,3) == bool(offset & (1 << bit)) for n,bit in self.address.items())
            expected.extend(q(n,3) == bool(6 & (1 << bit)) for n,bit in commands.items())
            expected.extend(z3.Not(q(roots[net][pin],5)) for net,pin in (
                ("DEVSEL#","O"),("DEVSEL#","T"),("TRDY#","O"),("TRDY#","T"),("AD[0]","T")))
            expected.extend((q(roots["FRAME#"]["I"],5), z3.Not(q(roots["IRDY#"]["I"],5)),
                             z3.Not(q(roots["STOP#"]["T"],5)), z3.Not(q(roots["STOP#"]["O"],5))))
            # STOP# is asserted with TRDY#: this fixture ends with data/disconnect,
            # not a proved ordinary target completion or burst-read contract.
            expected.extend(q(roots[net]["T"],7) for net in ("DEVSEL#","TRDY#","AD[0]"))
            bus = logic["X8,Y1 TBUS.OUT"]["data"]
            expected.extend(s.expr(logic[d["driver"]]["disable"],4) ==
                            (d["driver"] != "X10,Y1 TBUF[0].O") for d in bus["drivers"])
            expected.append(q(roots["AD[0]"]["O"],5) == q("X11,Y7 SLICE[0].Y",4))
            s.solver.push()
            s.solver.add(z3.Not(z3.And(*expected)))
            self.assertEqual(s.solver.check(), z3.unsat, hex(offset))
            s.solver.pop()

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC") == "1", "optional Z3 regression")
    def test_stalled_read_has_abstract_early_release_counterexample(self):
        from virtexe_symbolic import z3
        s = self.target_read_fixture(wait_until=7)
        q = lambda n,t: s.signal(n,t)
        roots = self.report["roots"]
        s.solver.add(*[q("X0,Y13 PCILOGIC.PCI_CE",t) for t in range(9)])
        self.assertEqual(s.solver.check(), z3.sat)
        # The fixture is not boot reachable and still contains unknown contracts.
        # Do not turn CE=1 into a claim of a complete PCI wait-state contract.
        s.solver.add(q(roots["IRDY#"]["I"],5), q(roots["IRDY#"]["I"],6),
                     z3.Not(q(roots["TRDY#"]["O"],5)),
                     z3.Not(q(roots["AD[0]"]["T"],5)), q(roots["AD[0]"]["T"],6))
        self.assertEqual(s.solver.check(), z3.sat)
        self.assertTrue(s.unknowns)

    @unittest.skipUnless(os.environ.get("WR6K_FPGA_SYMBOLIC") == "1", "optional Z3 regression")
    def test_bar1_read_has_no_completed_response_without_remote_edges(self):
        from virtexe_symbolic import z3
        s = self.target_read_fixture(bar=1, offset=0x80)
        q = lambda n,t: s.signal(n,t)
        roots = self.report["roots"]
        self.assertEqual(s.solver.check(), z3.sat)
        expected = [q("X6,Y14 SLICE[0].YQ",3), z3.Not(q("X7,Y12 SLICE[0].YQ",3))]
        expected.extend(q(n,3) == bool(0x10000080 & (1 << bit)) for n,bit in self.address.items())
        # No fast/receive edges in this fixture. A missing response is not an idle ack.
        expected.extend(z3.Not(q("X13,Y14 SLICE[1].YQ",t)) for t in range(9))
        expected.extend(q(roots["TRDY#"]["O"],t) for t in (5,6,7,8))
        s.solver.add(z3.Not(z3.And(*expected)))
        self.assertEqual(s.solver.check(), z3.unsat)

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
