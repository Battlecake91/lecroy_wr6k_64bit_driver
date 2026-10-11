"""Public synthetic C166 control-flow tests, no proprietary firmware."""
import hashlib
import json
import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "fpga"))
from c166_firmware import (direct_flow, spans, inventory, extended_operand, table_targets,
                           serial_word_loop, copy_records6, load_decoder)


class C166FlowTests(unittest.TestCase):
    def test_truncated_inputs_fail_closed(self):
        for raw in (b'', b'\xfa', bytes.fromhex('fa 00')):
            with self.assertRaises(ValueError): direct_flow(raw, 0)
        with self.assertRaises(ValueError): extended_operand(bytes.fromhex('d7 00'))

    def test_pcall_and_reset_are_explicit(self):
        self.assertEqual(direct_flow(bytes.fromhex('e2 f0 34 12'), 0x200000),
                         {'kind': 'call', 'targets': [0x201234], 'fallthrough': True})
        self.assertEqual(direct_flow(bytes.fromhex('b7 48 b7 b7'), 0)['kind'], 'reset')
        with self.assertRaises(ValueError): direct_flow(bytes.fromhex('b7 48'), 0)

    def test_decoder_hash_checked_before_import(self):
        import tempfile
        from c166_firmware import DECODER_HASHES
        with tempfile.TemporaryDirectory() as directory:
            for name in DECODER_HASHES:
                (Path(directory) / name).write_text('not executable decoder', encoding='ascii')
            with self.assertRaises(ValueError): load_decoder(directory)

    def test_serial_loop_clock_precedes_assignment_and_preserves_word_order(self):
        raw = b''.join(bytes((0xaf, 0xe0, 0x4a, 0xf2, 0xe0, bit << 4 | 5, 0xae, 0xe0)) for bit in range(16))
        memory = dict(enumerate(raw))
        result = serial_word_loop(memory, 0, 10, 5, 2)
        self.assertEqual(result['bits'], list(range(16)))
        self.assertEqual(result['order'], 'rise, assign DIN, fall')
        memory[2] = 0x3a
        with self.assertRaises(ValueError): serial_word_loop(memory, 0, 10, 5, 2)

    def test_startup_copy_is_page_aware_and_stops_at_unknown_tag(self):
        words = (6, 0x1234, 3, 0x45, 7, 8, 7)
        memory = dict(enumerate(b''.join(w.to_bytes(2, 'little') for w in words)))
        records, boundary = copy_records6(memory, 0)
        self.assertEqual(records, [{'record': 0, 'destination': 0xd234, 'source': 0x1c045, 'length': 8}])
        self.assertEqual(boundary, {'address': 12, 'tag': 7})

    def test_ext_immediate_page_and_actual_range(self):
        self.assertEqual(extended_operand(bytes.fromhex("d7 50 81 00")),
                         {"mode": "page", "instructions": 2, "constant": 0x81})
        self.assertEqual(extended_operand(bytes.fromhex("dc 4d")),
                         {"mode": "page", "instructions": 1, "register": 13})
        self.assertEqual(extended_operand(bytes.fromhex("d7 00 20 00"))["constant"], 0x20)
        with self.assertRaises(ValueError):
            extended_operand(bytes.fromhex("d7 00 00 01"))

    def test_explicit_table_preserves_code_segment_not_data_page(self):
        self.assertEqual(table_targets({0x4000: 0x34, 0x4001: 0x12}, 0x4000, 1, 0x200000), [0x201234])
        with self.assertRaises(KeyError):
            table_targets({0: 1}, 0, 1, 0)

    def test_far_jump_and_call(self):
        self.assertEqual(direct_flow(bytes.fromhex("fa 12 34 56"), 0)["targets"], [0x125634])
        self.assertFalse(direct_flow(bytes.fromhex("fa 12 34 56"), 0)["fallthrough"])
        self.assertTrue(direct_flow(bytes.fromhex("da 12 34 56"), 0)["fallthrough"])

    def test_relative_word_displacement_and_segment_wrap(self):
        self.assertEqual(direct_flow(bytes.fromhex("0d ff"), 0x120000)["targets"], [0x120000])
        self.assertEqual(direct_flow(bytes.fromhex("2d 01"), 0x12fffe)["targets"], [0x120002])
        self.assertTrue(direct_flow(bytes.fromhex("2d 01"), 0)["fallthrough"])

    def test_near_branch_preserves_segment(self):
        self.assertEqual(direct_flow(bytes.fromhex("ea 00 34 56"), 0x120000)["targets"], [0x125634])
        self.assertEqual(direct_flow(bytes.fromhex("ea 01 34 56"), 0)["kind"], "unknown")

    def test_bit_branch_uses_third_byte(self):
        self.assertEqual(direct_flow(bytes.fromhex("9a 10 fe 70"), 0x100)["targets"], [0x100])

    def test_indirect_and_returns_are_not_guessed(self):
        self.assertFalse(direct_flow(bytes.fromhex("9c 02"), 0)["fallthrough"])
        self.assertEqual(direct_flow(bytes.fromhex("db 00"), 0)["kind"], "return")

    def test_reviewed_conditional_table_retains_fallthrough(self):
        class Decoder:
            clen = {0x9c: 2, 0xdb: 2}
            def decode_one(self, raw, offset, address):
                return None, None, 'synthetic branch'
        memory = dict(enumerate(bytes.fromhex('9c 12 db 00 db 00 04 00')))
        result = inventory(memory, Decoder(), [0], tables={0: (6, 1)})
        self.assertTrue(result['instructions'][0]['flow']['fallthrough'])
        self.assertEqual(set(result['instructions']), {0, 2, 4})

    def test_sparse_holes_are_preserved(self):
        self.assertEqual(spans({0: 1, 1: 2, 5: 3, 0x200000: 4}), [[0, 2], [5, 6], [0x200000, 0x200001]])
        class Decoder:
            clen = {0xfa: 4}
        result = inventory({0: 0xfa, 1: 0, 3: 0}, Decoder(), [0])
        self.assertEqual(result["instructions"], {})
        self.assertIn("sparse", result["boundaries"][0]["reason"])


@unittest.skipUnless(os.environ.get('WR6K_C166_MEMORY'), 'private sparse C161 memory not configured')
class PrivateC161Loader(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = {int(k): v for k, v in json.loads(Path(os.environ['WR6K_C166_MEMORY']).read_text()).items()}

    def raw(self, address, length):
        return bytes(self.m[address + i] for i in range(length))

    def word(self, address):
        return int.from_bytes(self.raw(address, 2), 'little')

    def test_reset_dispatch_and_separate_clock_loops(self):
        self.assertEqual(len(self.m), 56792)
        identity = b''.join(a.to_bytes(4, 'little') + bytes([v]) for a, v in sorted(self.m.items()))
        self.assertEqual(hashlib.sha256(identity).hexdigest(), 'c81c6023eda3ac9618f5a4934a4e25ccf39a4d06c7d44601b2c858300f91dcf3')
        self.assertEqual(direct_flow(self.raw(0, 4), 0)['targets'], [0xf38])
        for source, target in ((0x3ede, 0x6d6c), (0x6d78, 0x6f1c)):
            self.assertEqual(direct_flow(self.raw(source, 4), source)['targets'], [target])
        for address, clock in ((0x6dd0, 12), (0x6e72, 12), (0x6f7c, 13), (0x701e, 13)):
            self.assertEqual(serial_word_loop(self.m, address, clock)['din'], 9)

    def test_loader_source_offsets_lengths_and_extra_clock_bytes(self):
        for address, value in ((0x6dc2, 0x10), (0x6f6e, 0x18), (0x6e5a, 0x7f00),
                               (0x6e66, 0x58), (0x6efc, 0x5a64), (0x7006, 0x7f00),
                               (0x7012, 0x2c), (0x70a8, 0x2d20)):
            self.assertEqual(self.word(address), value)
        self.assertEqual((88 * 0x7f00 + ((0x5a64 + 15) // 16) * 16) // 8, 360526)
        self.assertEqual((44 * 0x7f00 + 0x2d20) // 8, 180260)
        # Acquisition reads six bytes beyond the two contiguous resource streams.

    def test_done_entry_gate_and_direction_cleanup_are_not_stop_ack(self):
        for address in (0x6d86, 0x6f32):
            raw = self.raw(address, 4)
            self.assertEqual((raw[0], raw[1], raw[3] >> 4), (0x9a, 0xfe, 11))
        self.assertEqual(self.word(0x6d9a), 0xfbff)
        self.assertEqual(self.word(0x6f46), 0xfbff)
        self.assertEqual(self.word(0x6f0e), 0xfdff)  # DIN becomes input.
        self.assertEqual(self.word(0x70ba), 0xdfff)  # PCI CCLK becomes input.

    def test_runtime_copy_initializes_loader_base_to_zero(self):
        records, boundary = copy_records6(self.m, 0x1628)
        record = next(r for r in records if r['destination'] == 0xe6fc)
        self.assertEqual((record['source'], record['length']), (0x6380, 0x78))
        self.assertEqual(self.raw(record['source'], 4), bytes(4))
        self.assertIsInstance(boundary['tag'], int)
        # Initializer value, not proof against every indirect write/installed bank.


if __name__ == "__main__":
    unittest.main()
