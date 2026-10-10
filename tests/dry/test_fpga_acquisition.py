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
        self.assertEqual(len(self.fields), 73)
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


if __name__ == "__main__":
    unittest.main()
