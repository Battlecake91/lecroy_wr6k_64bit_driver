"""Acquisition-side synthetic and opt-in private configured-logic tests."""
import hashlib
import itertools
import json
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/fpga"))
import virtexe_acquisition as a
from virtexe_mmio import functional_expression
from virtexe_routing import data_support, evaluate_data, register_contract


def ihex(kind, address=0, data=b""):
    record = bytes((len(data), address >> 8, address & 255, kind)) + data
    return b":" + (record + bytes((-sum(record) & 255,))).hex().encode() + b"\n"


class SyntheticAcquisition(unittest.TestCase):
    def test_sparse_mcu_hex_preserves_addresses(self):
        raw = ihex(0, data=b"ab") + ihex(4, data=b"\x00\x20") + ihex(0, data=b"cd") + ihex(1)
        self.assertEqual(a.intel_hex(raw), {0: 97, 1: 98, 0x200000: 99, 0x200001: 100})

    def test_hex_fails_closed(self):
        valid = ihex(0, data=b"a")
        for raw in (valid, valid + valid + ihex(1), ihex(1) + valid,
                    b":0100000061FF\n" + ihex(1), ihex(3) + ihex(1), b"bad\n"):
            with self.subTest(raw=raw), self.assertRaises(ValueError):
                a.intel_hex(raw)

    def test_segment_address_and_empty_image(self):
        self.assertEqual(a.intel_hex(ihex(2, data=b"\x12\x34") + ihex(0, 2, b"a") + ihex(1)), {0x12342: 97})
        self.assertEqual(a.intel_hex(ihex(1)), {})

    def test_fg456_not_pq208_and_dedicated_clock_not_iqi(self):
        text = "bond BOND85 {\n pin AA5 = IOB_S3_1;\n pin B8 = IOB_N13_2;\n pin A11 = CLK2;\n}\n" \
               "device xc2s200e {\n bond fg456 = BOND85;\n}\n"
        self.assertEqual(a.package_locations(text), {"AA5": (3, 0, 1), "B8": (13, 29, 2)})
        for bad in (text.replace("fg456", "pq208"), text + text,
                    text.replace("pin B8 = IOB_N13_2;", "pin AA5 = IOB_N13_2;"),
                    text.replace("IOB_S3_1", "IOB_S48_1")):
            with self.assertRaises(ValueError):
                a.package_locations(bad)

    def test_payload_mapping_is_bijection_not_36_payload_bits(self):
        bits = [a.payload_bit(lane, delay) for lane in range(12) for delay in range(3)]
        self.assertEqual(sorted(b for b in bits if b is not None), list(range(32)))
        self.assertEqual(bits.count(None), 4)
        self.assertEqual([a.payload_bit(1, d) for d in range(3)], [None, 1, 0])
        matrix = json.loads((Path(__file__).resolve().parents[2] /
            "docs/pci-mmio-register-evidence.json").read_text(encoding="utf-8"))
        acq = matrix["acquisition_controller"]
        self.assertEqual(acq["firmware"]["stream_0_sha256"], a.IMAGE_SHA256)
        self.assertEqual({row["name"] for row in acq["command_matrix"]}, set(a.GATES))
        self.assertEqual(set(matrix["release_obligations"].values()), {"Unknown"})
        self.assertEqual(acq["receiver"]["acceptance_status"], "Unknown")
        self.assertTrue(all(row["response_status"] == "Unknown" for row in acq["command_matrix"]))
        for lane, delay in ((12, 0), (0, 3), (-1, 0)):
            with self.assertRaises(ValueError):
                a.payload_bit(lane, delay)

    def test_symbolic_cut_does_not_turn_unknown_state_into_idle(self):
        r = {"logic": {"keep": {"kind": "register", "data": {"source": "cut"}},
                       "cut": {"kind": "register", "data": {"constant": 0}}}}
        cut = a.symbolic_cut(r, {"keep"})
        self.assertEqual(cut["logic"]["keep"], r["logic"]["keep"])
        self.assertEqual(cut["logic"]["cut"]["kind"], "architecture_boundary")
        self.assertNotIn("data", cut["logic"]["cut"])

    def test_missing_command_qualifier_is_not_verified(self):
        with self.assertRaises(ValueError):
            a.command_decode({"logic": {"gate": {"data": {"unknown": "no route"}}}}, "gate", 0x80)

    def test_synthetic_frames_keep_control_parity_and_marker_distinct(self):
        for word in [0, 0xFFFFFFFF, 0xA5A55A5A] + [1 << i for i in range(32)]:
            frame = a.command_frame(word)
            self.assertEqual(tuple(len(phase) for phase in frame), (12, 12, 12))
            self.assertFalse(frame[0][0])
            self.assertTrue(frame[1][0])
            reconstructed = sum(int(frame[d][lane]) << bit for lane in range(12)
                for d in range(3) for bit in [a.payload_bit(lane, d)] if bit is not None)
            self.assertEqual(reconstructed, word)
            self.assertEqual(frame[2][0], bool((word & 0x55555555).bit_count() % 2))
            self.assertEqual(frame[0][1], not bool((word & 0xAAAAAAAA).bit_count() % 2))
        for word in (-1, 1 << 32, "0"):
            with self.assertRaises(ValueError):
                a.command_frame(word)

    def test_leaf_inventory_preserves_memory_and_unknown_boundaries(self):
        logic = {"mux": {"kind": "combinational", "data": {"op": "xor", "args": [
            {"source": "ram"}, {"source": "ff"}, {"source": "unknown"}]}},
            "ram": {"kind": "memory_output"}, "ff": {"kind": "register"},
            "unknown": {"kind": "architecture_boundary"}}
        self.assertEqual(a.sequential_leaves({"source": "mux"}, logic), {"ram", "ff", "unknown"})

    def test_unresolved_tx_or_compare_is_rejected(self):
        with self.assertRaises((KeyError, ValueError)):
            a.transmit_fields({"logic": {}, "roots": {}})
        with self.assertRaises((KeyError, ValueError)):
            a.comparison_pairs({"logic": {}}, "MAMRGO", {0: "unknown"})


@unittest.skipUnless(os.environ.get("WR6K_ACQUISITION_REPORT"), "private acquisition report not configured")
class PrivateAcquisition(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.r = json.loads(Path(os.environ["WR6K_ACQUISITION_REPORT"]).read_text(encoding="utf-8"))
        cls.l = cls.r["logic"]
        cls.fields = a.receiver_fields(cls.r)
        cls.banks = a.command_banks(cls.r, cls.fields)

    def test_private_image_identity(self):
        self.assertEqual(self.r["image_sha256"], a.IMAGE_SHA256)
        path = os.environ.get("WR6K_ACQUISITION_BINARY")
        if path:
            self.assertEqual(hashlib.sha256(Path(path).read_bytes()).hexdigest(), a.IMAGE_SHA256)

    def test_package_start_and_pulse_are_different(self):
        self.assertEqual(self.r["roots"]["AA5"]["O"], a.START)
        self.assertNotEqual(self.r["roots"]["B8"]["O"], a.START)
        self.assertEqual(self.r["pads"]["AA5"]["location"], [3, 0, 1])

    def test_twelve_differential_lanes_and_sync(self):
        for pin in a.RX_PINS:
            self.assertEqual(self.r["pads"][pin]["attributes"]["IBUF_MODE"]["selections"], ["DIFF"])
            self.assertIn("IQ", self.r["roots"][pin])

    def test_all_32_payload_positions(self):
        self.assertEqual({f["payload_bit"] for f in self.fields.values()} - {None}, set(range(32)))
        self.assertEqual(len(self.fields), 79)
        self.assertEqual(self.fields["X35,Y20 SLICE[1].YQ"]["payload_bit"], 0)

    def test_synthetic_payloads_through_actual_rx_muxes(self):
        words = [0, 0xFFFFFFFF, 0xA5A55A5A] + [1 << i for i in range(32)]
        for word, selected in itertools.product(words, (False, True)):
            for name, field in self.fields.items():
                bit = field["payload_bit"]
                if bit is None:
                    continue
                expected = bool(word & (1 << bit))
                banks = field["banks_zero_one"]
                inputs = {field["selector"]: selected,
                          banks[int(selected)]: expected, banks[int(not selected)]: not expected}
                self.assertEqual(evaluate_data(self.l[name]["data"], self.l, inputs), expected)

    def test_exact_command_decodes_with_no_implicit_ack(self):
        for command, (value, gates) in a.GATES.items():
            for gate in gates:
                with self.subTest(command=command, gate=gate):
                    self.assertTrue(a.command_decode(self.r, gate, value))
        for command, gate in (("MTTRGO", "X27,Y18 SLICE[0].YQ"), ("MAMRGO", "X28,Y17 SLICE[0].YQ")):
            self.assertTrue(a.command_decode(self.r, gate, a.GATES[command][0]))

    def test_separate_load_bank_widths(self):
        for command, width in (("MTTCTL", 1), ("MTTRGO", 19), ("MAMRGO", 24)):
            self.assertEqual(set(self.banks[command]), set(range(width)))
            for name in self.banks[command].values():
                c = register_contract(name, self.l[name])
                self.assertNotEqual(c["status"], "unknown")
                self.assertEqual(c["clock"].get("source"), a.CLOCK)
        self.assertEqual(self.banks["MTTCTL"][0], a.E)
        self.assertFalse(set(self.banks["MTTRGO"].values()) & set(self.banks["MAMRGO"].values()))

    def test_control_reset_and_clock(self):
        for name in (a.E, a.D1, a.D2, a.H, a.P, a.START):
            c = register_contract(name, self.l[name])
            self.assertEqual(c["status"], "modeled_async_ff")
            self.assertEqual(c["init"], 0)
            self.assertEqual(c["clock"].get("source"), a.CLOCK)
            self.assertEqual(c["sr"].get("source"), a.RESET)

    def test_actual_control_equations_exhaustive(self):
        for e, d1, d2, h in itertools.product((False, True), repeat=4):
            values = {a.E: e, a.D1: d1, a.D2: d2, a.H: h}
            expected = {a.D1: e, a.D2: d1, a.H: e and (d2 or h),
                        a.START: e and (d2 or h), a.P: e and not d1}
            for name, value in expected.items():
                self.assertEqual(evaluate_data(self.l[name]["data"], self.l, values), value)

    def test_address_capture_not_execution_ack(self):
        gate = "X29,Y19 SLICE[1].X"
        flag = "X31,Y21 SLICE[0].XQ"
        for v, q, f in itertools.product((False, True), repeat=3):
            self.assertEqual(evaluate_data(self.l[gate]["data"], self.l,
                {a.V: v, a.Q: q, flag: f}), v and not q and f)

    def equation(self, name, inputs, expected, part="data"):
        for values in itertools.product((False, True), repeat=len(inputs)):
            self.assertEqual(evaluate_data(self.l[name][part], self.l, dict(zip(inputs, values))), expected(*values))

    def test_control_fields_are_actual_lane_zero_banks(self):
        fields = a.receiver_fields(self.r, True)
        for name, depth in ((a.CA, 0), (a.CB, 1)):
            self.assertEqual((fields[name]["lane"], fields[name]["delay"]), (0, depth))

    def test_actual_valid_and_address_data_sequence_equations(self):
        self.equation(a.V, (a.LINK, a.CA, a.CB, a.FRAME), lambda link, ca, cb, frame: link and not ca and cb and frame)
        self.equation(a.Q, (a.V, a.Q, a.FLAG), lambda v, q, flag: (not q and flag) if v else q)
        self.equation("X34,Y21 SLICE[1].X", (a.CA, a.CB, a.CAPTURE), lambda ca, cb, cap: not ca and cb and cap)

    def test_controller_framing_and_bank_selector_equations(self):
        aa, bb = "X44,Y26 SLICE[0].XQ", "X44,Y26 SLICE[0].YQ"
        cc, dd, tt = "X44,Y24 SLICE[1].YQ", "X44,Y24 SLICE[1].XQ", "X44,Y25 SLICE[1].XQ"
        kk = "X44,Y25 SLICE[0].X"
        self.equation(kk, (tt, bb), lambda t, b: t or not b)
        self.equation(a.FRAME, (cc, kk, aa, dd), lambda c, k, a0, d: d or not k or (a0 and not c))
        self.equation(a.CAPTURE, (dd, kk, aa, cc), lambda d, k, a0, c: d or not k or (a0 and not c))
        self.equation(cc, (aa, a.LINK), lambda a0, link: a0 and link)
        self.equation(dd, (aa, cc, a.LINK, kk), lambda a0, c, link, k: a0 and not c and link and not k)
        self.equation(tt, (bb, a.LINK), lambda b, link: b and link)
        self.equation(a.RX_SELECTORS[0], (a.LINK, "X45,Y24 SLICE[1].YQ"), lambda link, s: link and not s)
        self.equation("X43,Y26 SLICE[0].X", (a.LINK, a.CAPTURE), lambda link, cap: not link or cap)

    def test_fast_sync_bank_enable_equations(self):
        h, s, sync = "X32,Y28 SLICE[0].YQ", "X42,Y26 SLICE[1].YQ", self.r["roots"]["C12"]["IQ"]
        self.equation(h, (s, h), lambda link, old: link and not old)
        for sv, hv, fv in itertools.product((False, True), repeat=3):
            inputs = {s: sv, h: hv, sync: fv}
            ce = evaluate_data(functional_expression(self.l[h]["controls"]["CE"]), self.l, inputs)
            self.assertEqual(ce, not sv or fv)
            for n, value in (("X35,Y28 SLICE[0].XQ", sv and hv and fv),
                             ("X36,Y27 SLICE[0].XQ", sv and not hv and fv)):
                self.assertEqual(evaluate_data(self.l[n]["data"], self.l, inputs), value)

    def test_parity_comparator_and_sticky_error_not_ack(self):
        received_even, received_odd = "X44,Y20 SLICE[0].YQ", "X42,Y25 SLICE[0].YQ"
        self.equation(a.PARITY_COMPARE, (a.EVEN, a.ODD, received_even, received_odd),
                      lambda e, o, re, ro: e != re or o != ro)
        self.equation(a.PARITY_ERROR, (a.LINK, "X44,Y23 SLICE[1].YQ", a.PARITY_COMPARE),
                      lambda link, enabled, mismatch: link and enabled and mismatch)
        self.equation(a.ERROR, (a.PARITY_ERROR, "X44,Y23 SLICE[0].YQ", a.ERROR),
                      lambda parity, other, old: parity or other or old)
        for ball, inverted in (("AA20", True), ("AB20", False)):
            self.equation(self.r["roots"][ball]["O"], (a.ERROR,), lambda error: error != inverted)
        for node in (a.V, a.Q, *[g for _, gates in a.GATES.values() for g in gates]):
            self.assertFalse({a.ERROR, a.PARITY_ERROR, a.PARITY_COMPARE} & a.sequential_leaves(self.l[node]["data"], self.l))

    def test_tx_phase_machine_all_states_and_replicas(self):
        self.equation(a.TX_P, (a.TX_P, a.TX_E, a.TX_KICK), lambda p, e, kick: e or (not p and kick))
        self.equation(a.TX_E, (a.TX_P, a.TX_E), lambda p, e: p != e)
        for family in (a.TX_P_COPIES, a.TX_E_COPIES):
            reference = register_contract(family[0], self.l[family[0]])
            for name in family:
                contract = register_contract(name, self.l[name])
                for key in ("clock", "ce", "sr", "init", "status"):
                    self.assertEqual(functional_expression(contract[key]), functional_expression(reference[key]))
                for p, e, kick in itertools.product((False, True), repeat=3):
                    inputs = {a.TX_P: p, a.TX_E: e, a.TX_KICK: kick}
                    self.assertEqual(evaluate_data(self.l[name]["data"], self.l, inputs),
                                     evaluate_data(self.l[family[0]]["data"], self.l, inputs))

    def test_tx_all_32_word_fields_and_real_buffer_sources(self):
        fields = a.transmit_fields(self.r)
        payload = {v["payload_bit"]: v for v in fields.values() if v["payload_bit"] is not None}
        self.assertEqual(set(payload), set(range(32)))
        self.assertEqual(payload[0]["word_register"], "X27,Y6 SLICE[1].XQ")
        self.assertEqual(fields[0, "A"]["word_register"], "X29,Y4 SLICE[0].YQ")
        f6 = self.l["X27,Y13 SLICE[1].YQ"]["data"]
        self.assertEqual(f6["op"], "mux")
        self.assertEqual(f6["zero"]["source"], "X27,Y13 SLICE[0].F5")
        self.assertEqual(f6["one"]["source"], "X27,Y13 SLICE[1].F5")
        for field in fields.values():
            for name in (field["output"], field["mux"]):
                self.assertEqual(register_contract(name, self.l[name])["clock"]["source"], a.TX_FAST)
        memories = set()
        for field in payload.values():
            node = self.l[field["word_register"]]
            self.assertEqual(node["controls"]["CLK"]["source"], a.CLOCK)
            memories.update(n for n in a.sequential_leaves(node["data"], self.l) if self.l[n]["kind"] == "memory_output")
        self.assertEqual({self.l[n]["memory"] for n in memories}, {"X1,Y1 BRAM", "X1,Y5 BRAM"})
        for name in ("X1,Y1 BRAM", "X1,Y5 BRAM"):
            memory = self.r["memories"][name]
            self.assertEqual(memory["kind"], "ramb4")
            self.assertEqual(memory["ports"]["B"]["width"], 16)
            self.assertEqual(memory["ports"]["B"]["clock"]["source"], a.CLOCK)
        left, right = [self.r["memories"][name]["ports"] for name in ("X1,Y1 BRAM", "X1,Y5 BRAM")]
        for port in ("A", "B"):
            for key in ("address", "write", "clock"):
                self.assertEqual(functional_expression(left[port][key]), functional_expression(right[port][key]))

    def test_tx_control_fields_depend_on_transport_stable_not_mtt_e(self):
        self.assertEqual(self.r["roots"]["E14"]["IQ"], "X32,Y29 IOI[1].IQ")
        self.assertEqual(self.l["X32,Y21 SLICE[1].YQ"]["data"]["source"], "X32,Y29 IOI[1].IQ")
        self.assertEqual(functional_expression(self.l["X37,Y3 SLICE[0].YQ"]["data"]),
                         {"source": "X32,Y21 SLICE[1].YQ", "inv": True})
        n, large, mode = "X31,Y3 SLICE[0].Y", "X29,Y4 SLICE[0].X", "X27,Y8 SLICE[0].XQ"
        stable = "X32,Y4 SLICE[1].XQ"
        self.equation("X29,Y4 SLICE[0].YQ", (n, stable, mode, large),
                      lambda nz, s, md, lg: not s or not nz or (lg and not md))
        self.equation("X27,Y5 SLICE[1].YQ", ("X28,Y5 SLICE[1].Y", large, mode, "X32,Y4 SLICE[0].YQ"),
                      lambda nz, lg, md, s: not s or (nz and (lg or md)))
        for node in ("X29,Y4 SLICE[0].YQ", "X27,Y5 SLICE[1].YQ"):
            self.assertNotIn(a.E, a.sequential_leaves(self.l[node]["data"], self.l))

    def test_independent_full_width_comparison_pairing(self):
        for command, width in (("MTTRGO", 19), ("MAMRGO", 24)):
            pairs = a.comparison_pairs(self.r, command, self.banks[command])
            self.assertEqual(set(pairs), set(range(width)))
            self.assertEqual(len(set(pairs.values())), width)
            self.assertFalse(set(pairs.values()) & set(self.banks[command].values()))

    def test_status_event_uses_mampgo_strobe_not_mtt_stop(self):
        self.assertTrue(a.command_decode(self.r, "X26,Y18 SLICE[0].YQ", 0x044))
        self.assertEqual(self.fields["X32,Y20 SLICE[1].YQ"]["payload_bit"], 2)
        for name in ("X20,Y4 SLICE[1].Y", "X29,Y4 SLICE[1].X", "X19,Y24 SLICE[1].YQ"):
            self.assertNotIn(a.E, a.sequential_leaves(self.l[name]["data"], self.l))

    @unittest.skipUnless(os.environ.get("WR6K_ACQUISITION_SECOND_REPORT"), "second private stream not configured")
    def test_second_stream_matches_corrected_start_sacq_and_clock_pin_fingerprint(self):
        report = json.loads(Path(os.environ["WR6K_ACQUISITION_SECOND_REPORT"]).read_text(encoding="utf-8"))
        self.assertEqual(report["image_sha256"], a.TIMEBASE_SHA256)
        self.assertEqual(report["pads"]["M1"]["attributes"]["IBUF_MODE"]["selections"], ["NONE"])
        self.assertNotIn("I", report["roots"]["M1"])
        self.assertNotIn("IQ", report["roots"]["M1"])
        self.assertEqual(report["roots"]["M1"]["O"], "X0,Y14 IOI[1].PAD_O")
        self.assertEqual(self.r["pads"]["AA5"]["location"], [3, 0, 1])
        self.assertEqual(report["pads"]["N2"]["location"], [0, 12, 3])
        self.assertEqual(report["pads"]["N2"]["attributes"]["IBUF_MODE"]["selections"], ["CMOS"])
        self.assertEqual(report["roots"]["N2"]["I"], a.TB_START)
        for ball, inv in (("D1", True), ("D2", False)):
            data = report["logic"][report["roots"][ball]["O"]]["data"]
            self.assertEqual(data["source"], "X6,Y25 SLICE[1].Y")
            self.assertEqual(data["inv"], inv)
        # START=N2, SACQ=M1. Pin-role agreement is not a live-revision/loader proof.


@unittest.skipUnless(os.environ.get("WR6K_ACQUISITION_REPORT") and os.environ.get("WR6K_FPGA_SYMBOLIC") == "1",
                     "private acquisition SMT not configured")
class PrivateAcquisitionSmt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import z3
        from virtexe_symbolic import System, clock_key
        cls.z3, cls.System, cls.clock_key = z3, staticmethod(System), staticmethod(clock_key)
        cls.r = json.loads(Path(os.environ["WR6K_ACQUISITION_REPORT"]).read_text(encoding="utf-8"))
        cls.banks = a.command_banks(cls.r, a.receiver_fields(cls.r))
        names = {a.E, a.D1, a.D2, a.H, a.P, a.START} | set(cls.banks["MAMRGO"].values()) | set(cls.banks["MTTRGO"].values())
        cls.cut = a.symbolic_cut(cls.r, names)

    def model(self, bound=6, powerup=False):
        m = self.System(self.cut, bound, powerup=powerup,
                        schedule=[{self.clock_key({"source": a.CLOCK, "inv": False})}] * bound)
        for t in range(bound + 1):
            m.solver.add(self.z3.Not(m.signal(a.RESET, t)))
        return m

    def command(self, m, t, address, data=0, qualified=True):
        for name, bit in a.ADDRESS.items():
            m.solver.add(m.signal(name, t) == bool(address & (1 << bit)))
        m.solver.add(m.signal(a.V, t) == qualified, m.signal(a.Q, t) == qualified)
        fields = a.receiver_fields(self.r)
        for bank in self.banks.values():
            for bit, name in bank.items():
                source = self.r["logic"][name]["data"]["source"]
                self.assertEqual(fields[source]["payload_bit"], bit)
                m.solver.add(m.signal(source, t) == bool(data & (1 << bit)))

    def impossible(self, m, expression):
        self.assertEqual(m.solver.check(), self.z3.sat)
        m.solver.push()
        m.solver.add(expression)
        self.assertEqual(m.solver.check(), self.z3.unsat)
        m.solver.pop()

    def test_zero_clears_start_with_one_controller_edge_after_e_clears(self):
        m = self.model(4)
        m.solver.add(m.state(a.E, 0), m.state(a.START, 0))
        for t in range(4):
            self.command(m, t, 0x80, 0)
        self.impossible(m, self.z3.Or(m.state(a.E, 1), m.state(a.START, 2), m.state(a.H, 2)))

    def test_one_sets_start_after_three_delays_and_repeated_one_no_pulse(self):
        m = self.model(6, True)
        for t in range(6):
            self.command(m, t, 0x80, 1)
        self.impossible(m, self.z3.Or(m.state(a.START, 3), self.z3.Not(m.state(a.START, 4)),
                                     m.state(a.P, 3), m.state(a.P, 6)))

    def test_zero_does_not_clear_independent_mam_or_mtt_load_banks(self):
        m = self.model(4)
        retained = [self.banks[c][0] for c in ("MAMRGO", "MTTRGO")]
        m.solver.add(*[m.state(n, 0) for n in retained])
        for t in range(4):
            self.command(m, t, 0x80, 0)
        self.impossible(m, self.z3.Or(*[self.z3.Not(m.state(n, 4)) for n in retained]))

    def test_mam_load_can_follow_zero_without_mtt_restart(self):
        m = self.model(3, True)
        self.command(m, 0, 0x80, 0)
        self.command(m, 1, 0x64, 1)
        self.command(m, 2, 0x84, 1)
        self.impossible(m, self.z3.Or(m.state(a.E, 3),
            self.z3.Not(m.state(self.banks["MAMRGO"][0], 2)),
            self.z3.Not(m.state(self.banks["MTTRGO"][0], 3))))

    def test_unqualified_write_holds_and_cannot_restart(self):
        m = self.model(5, True)
        for t in range(5):
            self.command(m, t, 0x80, 1, qualified=False)
        self.impossible(m, self.z3.Or(m.state(a.E, 5), m.state(a.START, 5)))

    def test_async_reset_returns_control_to_init(self):
        m = self.System(self.cut, 0, powerup=False)
        m.solver.add(m.signal(a.RESET, 0))
        self.impossible(m, self.z3.Or(*[m.state(n, 0) for n in (a.E, a.D1, a.D2, a.H, a.P, a.START)]))

    def test_unknown_ack_does_not_prove_six_obligations(self):
        m = self.model(3)
        for t in range(3):
            self.command(m, t, 0x80, 0)
        m.solver.add(m.unknown("unidentified remote stop ACK", 2))
        for obligation in ("producer activity", "queued LVDS payload", "new PCI REQ/FRAME",
                           "outstanding PCI transfer", "restart path", "unretired host write"):
            m.solver.push()
            m.solver.add(m.unknown(obligation, 3))
            self.assertEqual(m.solver.check(), self.z3.sat)  # Abstract cut witness, not a hardware fault.
            m.solver.pop()

    def combinational_model(self):
        return self.System(self.r, 0, powerup=False)

    def equivalent(self, actual, expected, constraints=()):
        s = self.z3.Solver()
        s.set(timeout=20000)
        s.add(*constraints)
        self.assertEqual(s.check(), self.z3.sat)
        s.add(actual != expected)
        self.assertEqual(s.check(), self.z3.unsat)

    def test_parity_matches_independent_selected_banks_all_payloads(self):
        z3, m = self.z3, self.combinational_model()
        fields = a.receiver_fields(self.r, True)
        words = {}
        for name, field in a.receiver_fields(self.r).items():
            if field["payload_bit"] is not None:
                words.setdefault(field["payload_bit"], m.expr(self.r["logic"][name]["data"], 0))
        def xor(values):
            result = z3.BoolVal(False)
            for value in values:
                result = z3.Xor(result, value)
            return result
        selectors = {field["selector"] for field in fields.values()}
        constraints = [m.signal(name, 0) == m.signal(a.RX_SELECTORS[0], 0) for name in selectors]
        even = z3.Not(xor([words[i] for i in range(0, 32, 2)] + [m.signal(a.CA, 0), m.signal(a.CB, 0)]))
        odd = z3.Not(xor([words[i] for i in range(1, 32, 2)]))
        # Both banks remain independent symbolic values; no identical-bank shortcut.
        self.equivalent(m.expr(self.r["logic"][a.EVEN]["data"], 0), even, constraints)
        self.equivalent(m.expr(self.r["logic"][a.ODD]["data"], 0), odd, constraints)

    def test_rx_selector_replicas_have_identical_transition_contracts(self):
        m = self.combinational_model()
        reference = self.r["logic"][a.RX_SELECTORS[0]]
        for name in (*a.RX_SELECTORS, "X45,Y24 SLICE[1].YQ"):
            node = self.r["logic"][name]
            rc, nc = register_contract(a.RX_SELECTORS[0], reference), register_contract(name, node)
            for key in ("status", "clock", "ce", "sr", "init"):
                self.assertEqual(functional_expression(rc[key]), functional_expression(nc[key]))
            self.equivalent(m.expr(node["data"], 0), m.expr(reference["data"], 0))

    def test_tx_parity_computes_actual_word_mux_data(self):
        m, z3 = self.combinational_model(), self.z3
        fields = a.transmit_fields(self.r)
        payload = {f["payload_bit"]: m.expr(self.r["logic"][f["word_register"]]["data"], 0)
                   for f in fields.values() if f["payload_bit"] is not None}
        def xor(values):
            result = z3.BoolVal(False)
            for value in values:
                result = z3.Xor(result, value)
            return result
        stable = ("X32,Y4 SLICE[1].XQ", "X32,Y4 SLICE[0].YQ", "X32,Y4 SLICE[1].YQ")
        constraints = [m.signal(name, 0) == m.signal(stable[0], 0) for name in stable]
        cb = m.expr(self.r["logic"][fields[0, "B"]["word_register"]]["data"], 0)
        ca = m.expr(self.r["logic"][fields[0, "A"]["word_register"]]["data"], 0)
        for field, expected in ((fields[0, "C"], z3.Not(xor([payload[i] for i in range(0, 32, 2)] + [ca, cb]))),
                                (fields[1, "A"], z3.Not(xor([payload[i] for i in range(1, 32, 2)])))):
            self.equivalent(m.expr(self.r["logic"][field["word_register"]]["data"], 0), expected, constraints)

    def test_whole_carry_comparisons_equal_full_programmed_counts(self):
        m, z3 = self.combinational_model(), self.z3
        for command in a.COMPARE:
            pairs = a.comparison_pairs(self.r, command, self.banks[command])
            expected = z3.And(*[m.signal(self.banks[command][bit], 0) == m.signal(name, 0) for bit, name in pairs.items()])
            self.equivalent(m.signal(a.COMPARE[command], 0), expected)

    def test_mam_counter_increment_clear_and_enable_equations(self):
        m, z3 = self.combinational_model(), self.z3
        pairs = a.comparison_pairs(self.r, "MAMRGO", self.banks["MAMRGO"])
        vector = lambda bits: z3.Concat(*[z3.If(bit, z3.BitVecVal(1, 1), z3.BitVecVal(0, 1)) for bit in reversed(bits)])
        old = vector([m.signal(pairs[i], 0) for i in range(24)])
        nxt = vector([m.expr(self.r["logic"][pairs[i]]["data"], 0) for i in range(24)])
        load, previous, advance = [m.signal(n, 0) for n in
            ("X28,Y17 SLICE[0].YQ", "X13,Y17 SLICE[0].YQ", "X17,Y16 SLICE[1].YQ")]
        clear = z3.And(load, z3.Not(previous))
        self.equivalent(nxt, z3.If(clear, z3.BitVecVal(0, 24), old + 1))
        ce = m.expr(self.r["logic"][pairs[0]]["controls"]["CE"], 0)
        self.equivalent(ce, z3.Or(clear, advance))
        self.assertNotIn(a.E, a.sequential_leaves(self.r["logic"][pairs[0]]["controls"]["CE"], self.r["logic"]))

    def test_mtt_counter_normal_increment_and_separate_admission(self):
        m, z3 = self.combinational_model(), self.z3
        pairs = a.comparison_pairs(self.r, "MTTRGO", self.banks["MTTRGO"])
        vector = lambda bits: z3.Concat(*[z3.If(bit, z3.BitVecVal(1, 1), z3.BitVecVal(0, 1)) for bit in reversed(bits)])
        old = vector([m.signal(pairs[i], 0) for i in range(19)])
        nxt = vector([m.expr(self.r["logic"][pairs[i]]["data"], 0) for i in range(19)])
        edge0, edge1, advance = [m.signal(n, 0) for n in
            ("X11,Y19 SLICE[1].XQ", "X11,Y19 SLICE[1].YQ", "X7,Y12 SLICE[1].YQ")]
        self.equivalent(nxt, old + 1, (z3.Not(edge0), z3.Not(edge1)))
        self.equivalent(m.expr(self.r["logic"][pairs[0]]["controls"]["CE"], 0), z3.Or(edge0, advance))

    def test_address_then_data_load_uses_actual_q_and_address_transitions(self):
        names = {a.Q, a.E, a.D1, a.D2, a.H, a.P, a.START} | set(a.ADDRESS)
        cut = a.symbolic_cut(self.r, names)
        m = self.System(cut, 4, powerup=False,
                        schedule=[{self.clock_key({"source": a.CLOCK, "inv": False})}] * 4)
        m.solver.add(self.z3.Not(m.state(a.Q, 0)), m.state(a.E, 0))
        fields = a.receiver_fields(self.r)
        for t, word in enumerate((0x80000020, 0, 0, 0)):
            m.solver.add(self.z3.Not(m.signal(a.RESET, t)), m.signal(a.V, t) == (t < 2))
            for source in {self.r["logic"][n]["data"].get("source") for n in a.ADDRESS} | {"X35,Y20 SLICE[1].YQ", a.FLAG}:
                bit = fields[source]["payload_bit"]
                m.solver.add(m.signal(source, t) == bool(word & (1 << bit)))
        m.solver.add(self.z3.Not(m.signal(a.RESET, 4)))
        self.impossible(m, self.z3.Or(self.z3.Not(m.state(a.Q, 1)), m.state(a.Q, 2),
                                     m.state(a.E, 2), m.state(a.START, 3)))
        for n, bit in a.ADDRESS.items():
            self.assertEqual(fields[self.r["logic"][n]["data"]["source"]]["payload_bit"], bit - 2)
            self.impossible(m, m.state(n, 1) != bool(0x80 & (1 << bit)))

    def test_tx_buffered_word_can_remain_after_mtt_zero(self):
        payload = {f["payload_bit"]: f["word_register"] for f in a.transmit_fields(self.r).values() if f["payload_bit"] is not None}
        retained = payload[0]
        cut = a.symbolic_cut(self.r, {a.E, retained})
        m = self.System(cut, 3, powerup=False,
                        schedule=[{self.clock_key({"source": a.CLOCK, "inv": False})}] * 3)
        for t in range(4):
            m.solver.add(self.z3.Not(m.signal(a.RESET, t)))
        m.solver.add(m.state(a.E, 0), m.state(retained, 0))
        for t in range(3):
            for name, bit in a.ADDRESS.items():
                m.solver.add(m.signal(name, t) == bool(0x80 & (1 << bit)))
            m.solver.add(m.signal(a.V, t), m.signal(a.Q, t), self.z3.Not(m.signal("X35,Y20 SLICE[1].YQ", t)),
                         self.z3.Not(m.signal("X29,Y6 SLICE[1].YQ", t)))
        self.impossible(m, self.z3.Or(m.state(a.E, 1), self.z3.Not(m.state(retained, 3))))
        # Conditional retention, not proof that a remote FIFO is nonempty on hardware.

    def test_return_bram_pointer_increments_follow_separate_admission(self):
        m, z3 = self.combinational_model(), self.z3
        vector = lambda bits: z3.Concat(*[z3.If(bit, z3.BitVecVal(1, 1), z3.BitVecVal(0, 1)) for bit in reversed(bits)])
        ports = self.r["memories"]["X1,Y1 BRAM"]["ports"]
        write = m.signal("X20,Y4 SLICE[1].Y", 0)
        read = m.signal("X29,Y4 SLICE[1].X", 0)
        write_names = [ports["A"]["address"][str(i)]["source"] for i in range(8)]
        read_names = [f"X21,Y{y} SLICE[1].{axis}Q" for y in range(1, 5) for axis in ("X", "Y")]
        for names, enable in ((write_names, write), (read_names, read)):
            old = vector([m.signal(n, 0) for n in names])
            nxt = vector([m.expr(self.r["logic"][n]["data"], 0) for n in names])
            self.equivalent(nxt, old + z3.If(enable, z3.BitVecVal(1, 8), z3.BitVecVal(0, 8)))
            for name in names:
                self.assertNotIn(a.E, a.sequential_leaves(self.r["logic"][name]["controls"], self.r["logic"]))
        ready = m.signal("X23,Y5 SLICE[1].YQ", 0)
        address = vector([m.expr(ports["B"]["address"][str(i)], 0) for i in range(8)])
        lookahead = vector([m.expr(self.r["logic"][n]["data"], 0) for n in read_names])
        self.equivalent(address, z3.If(ready, lookahead, z3.BitVecVal(0, 8)))

    def test_return_occupancy_and_class_boundary_are_not_global_idle(self):
        m, z3 = self.combinational_model(), self.z3
        vector = lambda bits: z3.Concat(*[z3.If(bit, z3.BitVecVal(1, 1), z3.BitVecVal(0, 1)) for bit in reversed(bits)])
        names = ["X30,Y1 SLICE[0].YQ"] + [f"X30,Y{y} SLICE[0].{axis}Q" for y in range(2, 6) for axis in ("X", "Y")]
        block = [f"X31,Y{y} SLICE[1].{axis}Q" for y in (4, 5) for axis in ("X", "Y")] + ["X31,Y6 SLICE[1].XQ"]
        old = vector([m.signal(n, 0) for n in names])
        nxt = vector([m.expr(self.r["logic"][n]["data"], 0) for n in names])
        write = m.signal("X20,Y4 SLICE[1].Y", 0)
        read = m.signal("X29,Y4 SLICE[1].X", 0)
        ready = m.signal("X30,Y2 SLICE[1].YQ", 0)
        delta = old + z3.If(write, z3.BitVecVal(1, 9), z3.BitVecVal(0, 9)) - z3.If(read, z3.BitVecVal(1, 9), z3.BitVecVal(0, 9))
        self.equivalent(nxt, z3.If(ready, delta, z3.BitVecVal(0, 9)))
        boundary = m.signal("X29,Y4 SLICE[0].X", 0)
        self.equivalent(boundary, z3.Or(old == 0, vector([m.signal(n, 0) for n in block]) == 31))
        self.assertNotIn(a.E, a.sequential_leaves(self.r["logic"][names[0]]["data"], self.r["logic"]))
        # Modular bookkeeping alone does not prove no overflow, all producers stopped, or all queues drained.

    def test_status_candidate_is_event_latch_with_explicit_clear(self):
        m, z3 = self.combinational_model(), self.z3
        constraints = []
        for bit in set(a.ADDRESS.values()):
            names = [n for n, b in a.ADDRESS.items() if b == bit]
            constraints.extend(m.signal(n, 0) == m.signal(names[0], 0) for n in names[1:])
        address = z3.And(*[m.signal(n, 0) == bool(0x004 & (1 << bit)) for n, bit in a.ADDRESS.items()])
        clear = z3.And(address, m.signal(a.V, 0), m.signal(a.Q, 0), m.signal("X32,Y20 SLICE[1].YQ", 0))
        event = z3.And(m.signal("X26,Y18 SLICE[0].YQ", 0), m.signal("X16,Y19 SLICE[0].YQ", 0))
        node = self.r["logic"]["X19,Y24 SLICE[1].YQ"]
        self.equivalent(m.expr(node["data"], 0), event)
        self.equivalent(m.expr(node["controls"]["CE"], 0), z3.Or(event, clear), constraints)
        self.assertEqual(register_contract("X19,Y24 SLICE[1].YQ", node)["clock"]["source"], a.CLOCK)

    def test_terminal_conditions_reach_separate_registered_consumers(self):
        self.assertEqual(a.register_consumers(self.r, a.COMPARE["MTTRGO"]), {
            "X9,Y16 SLICE[0].YQ": ["data"], "X10,Y16 SLICE[1].YQ": ["data"]})
        self.assertEqual(a.register_consumers(self.r, a.COMPARE["MAMRGO"]), {
            "X16,Y19 SLICE[0].YQ": ["data", "controls"], "X16,Y19 SLICE[1].YQ": ["data"]})
        self.assertIn("controls", a.register_consumers(self.r, "X9,Y16 SLICE[0].YQ")["X7,Y12 SLICE[1].YQ"])
        self.assertIn("controls", a.register_consumers(self.r, "X16,Y19 SLICE[0].YQ")["X27,Y8 SLICE[0].XQ"])
        self.assertEqual(register_contract("X9,Y16 SLICE[0].YQ", self.r["logic"]["X9,Y16 SLICE[0].YQ"])["clock"]["source"], a.TX_FAST)

    def test_actual_write_read_admission_and_event_toggle_equations(self):
        m, z3 = self.combinational_model(), self.z3
        signal = lambda n: m.signal(n, 0)
        b, external, toggle = map(signal, ("X17,Y14 SLICE[1].YQ", "X18,Y1 SLICE[1].YQ", "X19,Y7 SLICE[0].YQ"))
        self.equivalent(signal("X20,Y4 SLICE[1].Y"), z3.Or(external, z3.And(b, toggle)))
        self.equivalent(m.expr(self.r["logic"]["X19,Y7 SLICE[0].YQ"]["data"], 0), z3.Xor(b, toggle))
        self.equivalent(signal("X29,Y4 SLICE[1].X"), z3.And(
            z3.Not(signal("X29,Y4 SLICE[0].X")), signal("X32,Y4 SLICE[0].YQ"),
            z3.Not(signal("X28,Y3 SLICE[1].Y")), signal("X29,Y6 SLICE[0].XQ")))
        m.solver.add(z3.Not(signal(a.E)), external, z3.Not(signal("X29,Y4 SLICE[1].X")))
        self.assertEqual(m.solver.check(), z3.sat)  # Constructed cut, not boot reachability.

    def test_mam_terminal_event_polarity_and_sticky_status_can_coexist_with_write(self):
        m, z3 = self.combinational_model(), self.z3
        equal = m.signal(a.COMPARE["MAMRGO"], 0)
        mode = m.signal("X13,Y20 SLICE[0].YQ", 0)
        self.equivalent(m.expr(self.r["logic"]["X16,Y19 SLICE[0].YQ"]["data"], 0), z3.Or(z3.Not(equal), z3.Not(mode)))
        self.equivalent(m.expr(self.r["logic"]["X16,Y19 SLICE[1].YQ"]["data"], 0), z3.And(z3.Not(equal), mode))
        m.solver.add(z3.Not(m.signal(a.E, 0)), equal, m.state("X19,Y24 SLICE[1].YQ", 0),
                     m.signal("X20,Y4 SLICE[1].Y", 0))
        self.assertEqual(m.solver.check(), z3.sat)
        # Comparison/event does not account for independent return producers.

    def test_zero_bookkeeping_does_not_inhibit_next_write(self):
        m, z3 = self.combinational_model(), self.z3
        count = ["X30,Y1 SLICE[0].YQ"] + [f"X30,Y{y} SLICE[0].{axis}Q" for y in range(2, 6) for axis in ("X", "Y")]
        m.solver.add(*[z3.Not(m.state(n, 0)) for n in count], z3.Not(m.state(a.E, 0)),
                     m.signal("X18,Y1 SLICE[1].YQ", 0), m.signal("X29,Y4 SLICE[0].X", 0),
                     m.signal("X20,Y4 SLICE[1].Y", 0))
        self.assertEqual(m.solver.check(), z3.sat)

    def test_mtt_terminal_state_equation_and_independent_return_write(self):
        m, z3 = self.combinational_model(), self.z3
        state = "X10,Y16 SLICE[1].YQ"
        launch, reload, old, equal = [m.signal(n, 0) for n in
            ("X12,Y18 SLICE[0].YQ", "X5,Y11 SLICE[1].YQ", state, a.COMPARE["MTTRGO"])]
        self.equivalent(m.expr(self.r["logic"][state]["data"], 0),
                        z3.And(launch, z3.Or(old, z3.And(reload, z3.Not(equal)))))
        m.solver.add(equal, z3.Not(m.state(a.E, 0)), m.signal("X20,Y4 SLICE[1].Y", 0))
        self.assertEqual(m.solver.check(), z3.sat)

    def test_tx_phase_can_start_after_stop_with_independent_kick(self):
        m = self.System(a.symbolic_cut(self.r, {a.E, a.START, a.TX_P, a.TX_E}), 2,
                        powerup=False, schedule=[{self.clock_key({"source": a.TX_FAST, "inv": False})}] * 2)
        z3 = self.z3
        m.solver.add(z3.Not(m.state(a.TX_P, 0)), z3.Not(m.state(a.TX_E, 0)))
        for t in range(3):
            m.solver.add(z3.Not(m.signal(a.RESET, t)), z3.Not(m.state(a.E, t)),
                         z3.Not(m.state(a.START, t)), m.signal(a.TX_KICK, t) == (t == 0))
        self.impossible(m, z3.Or(z3.Not(m.state(a.TX_P, 1)), m.state(a.TX_E, 1),
                                m.state(a.TX_P, 2), z3.Not(m.state(a.TX_E, 2))))
        # Actual TX transitions, arbitrary independent kick; no claim that PCI completed.


@unittest.skipUnless(os.environ.get("WR6K_ACQUISITION_SECOND_REPORT") and os.environ.get("WR6K_FPGA_SYMBOLIC") == "1",
                     "private timebase SMT not configured")
class PrivateTimebaseSmt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import z3
        from virtexe_symbolic import System, clock_key
        cls.z3, cls.System, cls.clock_key = z3, staticmethod(System), staticmethod(clock_key)
        cls.r = json.loads(Path(os.environ["WR6K_ACQUISITION_SECOND_REPORT"]).read_text(encoding="utf-8"))

    def model(self, names, bound, clock, powerup=False):
        return self.System(a.symbolic_cut(self.r, set(names)), bound, powerup=powerup,
                           schedule=[{self.clock_key({"source": clock, "inv": False})}] * bound)

    def impossible(self, m, expression):
        self.assertEqual(m.solver.check(), self.z3.sat)
        m.solver.push(); m.solver.add(expression)
        self.assertEqual(m.solver.check(), self.z3.unsat)
        m.solver.pop()

    def test_start_low_asynchronously_clears_first_level_stage_only(self):
        m = self.model(a.TB_LEVEL, 0, a.CLOCK)
        m.solver.add(self.z3.Not(m.signal(a.TB_START, 0)), m.state(a.TB_LEVEL[1], 0), m.state(a.TB_LEVEL[2], 0))
        self.impossible(m, m.state(a.TB_LEVEL[0], 0))
        self.assertEqual(register_contract(a.TB_LEVEL[0], self.r["logic"][a.TB_LEVEL[0]])["flags"]["SR_SYNC"], 0)

    def test_level_pipeline_clears_after_two_local_edges_not_pci_edges(self):
        m = self.model(a.TB_LEVEL, 2, a.CLOCK)
        for t in range(3):
            m.solver.add(self.z3.Not(m.signal(a.TB_START, t)), self.z3.Not(m.signal("X6,Y11 SLICE[0].YQ", t)))
        self.impossible(m, self.z3.Or(*[m.state(n, 2) for n in a.TB_LEVEL]))

    def test_three_stage_fast_sync_and_falling_edge_toggle(self):
        m = self.model((*a.TB_SYNC, a.TB_TOGGLE), 4, a.FAST)
        m.solver.add(*[m.state(n, 0) for n in a.TB_SYNC], self.z3.Not(m.state(a.TB_TOGGLE, 0)))
        for t in range(5):
            m.solver.add(self.z3.Not(m.signal(a.TB_START, t)))
        self.impossible(m, self.z3.Or(*[m.state(n, 3) for n in a.TB_SYNC],
            self.z3.Not(m.state(a.TB_TOGGLE, 3)), self.z3.Not(m.state(a.TB_TOGGLE, 4))))

    def test_start_rise_does_not_toggle_falling_edge_register(self):
        m = self.model((*a.TB_SYNC, a.TB_TOGGLE), 4, a.FAST, True)
        for t in range(5): m.solver.add(m.signal(a.TB_START, t))
        self.impossible(m, self.z3.Or(m.state(a.TB_TOGGLE, 4), self.z3.Not(m.state(a.TB_SYNC[-1], 3))))

    def test_start_level_can_clear_while_other_local_clock_has_not_sampled(self):
        m = self.model((*a.TB_LEVEL, *a.TB_SYNC), 2, a.CLOCK)
        m.solver.add(*[m.state(n, 0) for n in a.TB_SYNC])
        for t in range(3):
            m.solver.add(self.z3.Not(m.signal(a.TB_START, t)), self.z3.Not(m.signal("X6,Y11 SLICE[0].YQ", t)))
        self.impossible(m, self.z3.Or(m.state(a.TB_LEVEL[-1], 2), self.z3.Not(m.state(a.TB_SYNC[-1], 2))))

    def test_sacq_is_delayed_runtime_state_not_start_low_ack(self):
        z3 = self.z3
        output = self.r["roots"]["M1"]["O"]
        memory = "X4,Y14 SLICE[0].SRL16E"
        cut = a.symbolic_cut(self.r, {output, "X18,Y1 SLICE[0].X"})
        cut["memories"] = {memory: self.r["memories"][memory]}
        m = self.System(cut, 4, powerup=False,
                        schedule=[{self.clock_key({"source": a.CLOCK, "inv": False})}] * 4)
        self.assertEqual(self.r["memories"][memory]["kind"], "srl16e")
        for t in range(5):
            m.solver.add(z3.Not(m.signal(a.TB_START, t)), z3.Not(m.signal("X18,Y1 SLICE[0].YQ", t)),
                         z3.Not(m.signal("X15,Y2 SLICE[0].XQ", t)))
        for i in range(16): m.solver.add(z3.Select(m.memory(memory, 0), z3.BitVecVal(i, 12)))
        self.impossible(m, z3.Or(z3.Not(m.state(output, 1)), z3.Not(m.state(output, 3)), m.state(output, 4)))
        # Writer states and clocks are independent cut inputs, not a claim of boot reachability.


if __name__ == "__main__":
    unittest.main()
