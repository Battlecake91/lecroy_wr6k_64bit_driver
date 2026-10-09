#!/usr/bin/env python3
"""Offline XC2S200E Virtex-E bitstream checks for the LeCroy WR6k PCI FPGA.

The tool intentionally accepts the private firmware image as an input path and
does not embed or generate proprietary LeCroy data.  The implemented geometry
is the Project Combine CHIP18/XC2S200E frame order needed to validate the
current PCI-FPGA control findings.
"""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


PROJECT_COMBINE_COMMIT = "234343d23e737e57f2727630e19008b509d7d522"

ROWS = 30
COLUMNS = 48
FRAME_BITS = ROWS * 18
FRAME_WORDS = (FRAME_BITS + 31) // 32
VIRTEX_TRANSFER_WORDS = FRAME_WORDS + 1

COLS_BRAM = {1, 14, 33, 46}
COL_CLK = COLUMNS // 2


@dataclass(frozen=True)
class FeatureBit:
    minor: int
    local_bit: int
    inverted: bool = False


def reverse_byte_bits(data: bytes) -> bytes:
    return bytes(int(f"{byte:08b}"[::-1], 2) for byte in data)


def chip18_col_frames() -> list[int]:
    """Return Project Combine's main-frame base for each CHIP18 column."""

    col_frame = [0] * COLUMNS
    frame_index = 8  # spine frames in public/virtex/src/expand.rs
    for dx in range(COLUMNS // 2):
        for side in ("R", "L"):
            col = COL_CLK + dx if side == "R" else COL_CLK - 1 - dx
            col_frame[col] = frame_index
            if col == 0 or col == COLUMNS - 1:
                width = 54
            elif col in COLS_BRAM:
                width = 27
            else:
                width = 48
            frame_index += width
    if frame_index != 2240:
        raise AssertionError(f"unexpected CHIP18 frame count {frame_index}")
    return col_frame


COL_FRAME = chip18_col_frames()


def virtex_frame_bits(slot_words: Sequence[int]) -> list[int]:
    """Decode one 18-DWORD Virtex transfer slot like insert_virtex_frame."""

    if len(slot_words) != VIRTEX_TRANSFER_WORDS:
        raise ValueError(f"expected 18 transfer words, got {len(slot_words)}")

    bits: list[int] = []
    final_word = slot_words[FRAME_WORDS - 1]
    pad = FRAME_WORDS * 32 - FRAME_BITS
    for bit in range(pad, 32):
        bits.append((final_word >> bit) & 1)

    for word_index in range(FRAME_WORDS - 2, -1, -1):
        word = slot_words[word_index]
        for bit in range(32):
            bits.append((word >> bit) & 1)

    if len(bits) != FRAME_BITS:
        raise AssertionError(f"decoded frame has {len(bits)} bits")
    return bits


def find_first_fdri_payload(packet_bytes: bytes) -> tuple[int, int]:
    """Locate the first Type-2 FDRI payload in the bit-reversed stream."""

    for offset in range(0, len(packet_bytes) - 4, 4):
        word = int.from_bytes(packet_bytes[offset : offset + 4], "big")
        # The WR6k PCI FPGA image uses Type-2 FDRI at byte 68.  The count is
        # encoded in the low 27 bits by the Virtex packet format.
        if word == 0x50009D92:
            return offset + 4, word & 0x07FF_FFFF
    raise ValueError("Type-2 FDRI header 0x50009D92 was not found")


class Xc2s200eBitstream:
    def __init__(self, path: Path):
        raw = path.read_bytes()
        self.packet_bytes = reverse_byte_bits(raw)
        self.fdri_payload_offset, self.fdri_word_count = find_first_fdri_payload(
            self.packet_bytes
        )
        if self.fdri_word_count % VIRTEX_TRANSFER_WORDS != 0:
            raise ValueError("FDRI word count is not an 18-word Virtex multiple")
        payload_len = self.fdri_word_count * 4
        payload = self.packet_bytes[
            self.fdri_payload_offset : self.fdri_payload_offset + payload_len
        ]
        if len(payload) != payload_len:
            raise ValueError("truncated Type-2 FDRI payload")
        words = [
            int.from_bytes(payload[pos : pos + 4], "big")
            for pos in range(0, len(payload), 4)
        ]

        # Project Combine stores the final frame of this first FDRI block for
        # a following MFWR duplicate.  The 2240 inserted frames are enough for
        # the main-fabric checks used here.
        transfer_frames = self.fdri_word_count // VIRTEX_TRANSFER_WORDS
        self.frames = [
            virtex_frame_bits(words[index : index + VIRTEX_TRANSFER_WORDS])
            for index in range(
                0,
                (transfer_frames - 1) * VIRTEX_TRANSFER_WORDS,
                VIRTEX_TRANSFER_WORDS,
            )
        ]
        if len(self.frames) != 2240:
            raise ValueError(f"expected 2240 main frames, got {len(self.frames)}")

    def bit(self, col: int, row: int, feature: FeatureBit) -> int:
        frame_index = COL_FRAME[col] + feature.minor
        bit_index = row * 18 + feature.local_bit
        value = self.frames[frame_index][bit_index]
        return value ^ 1 if feature.inverted else value

    def bit_string(self, col: int, row: int, features: Sequence[FeatureBit]) -> str:
        return "".join(str(self.bit(col, row, feature)) for feature in features)

    def bit_vector(self, col: int, row: int, features: Sequence[FeatureBit]) -> int:
        value = 0
        for index, feature in enumerate(features):
            value |= self.bit(col, row, feature) << index
        return value


def feature_range(
    minors: Iterable[int], local_bit: int, inverted: bool = False
) -> list[FeatureBit]:
    return [FeatureBit(minor, local_bit, inverted) for minor in minors]


SLICE0_F = feature_range(range(32, 48), 14, True)
SLICE0_G = feature_range(range(32, 48), 15, True)
SLICE1_G = feature_range(range(15, -1, -1), 15, True)

X2Y14_SLICE0_CONTROLS: dict[str, Sequence[FeatureBit]] = {
    "SLICE[0].BX inversion": [FeatureBit(38, 13)],
    "SLICE[0].DXMUX": [FeatureBit(46, 16)],
    "SLICE[0].FXMUX": [FeatureBit(29, 15), FeatureBit(31, 16)],
    "SLICE[0].FF_SR_SYNC": [FeatureBit(43, 16)],
    "SLICE[0].FF_SR_ENABLE": [FeatureBit(29, 14)],
    "SLICE[0].FFX_INIT": [FeatureBit(41, 16)],
    "IMUX_CLB_CLK[0]": [
        FeatureBit(23, 11),
        FeatureBit(22, 12),
        FeatureBit(21, 12),
        FeatureBit(21, 10),
        FeatureBit(23, 9),
        FeatureBit(22, 10),
    ],
    "IMUX_CLB_CE[0]": [
        FeatureBit(2, 11),
        FeatureBit(1, 11),
        FeatureBit(0, 12),
        FeatureBit(0, 10),
        FeatureBit(2, 9),
        FeatureBit(1, 9),
    ],
    "IMUX_CLB_SR[0]": [
        FeatureBit(25, 11),
        FeatureBit(26, 11),
        FeatureBit(24, 12),
        FeatureBit(26, 9),
        FeatureBit(25, 9),
        FeatureBit(24, 10),
    ],
    "OMUX[0]": [
        FeatureBit(39, 17),
        FeatureBit(45, 17),
        FeatureBit(43, 17),
        FeatureBit(40, 17),
        FeatureBit(44, 17),
        FeatureBit(41, 17),
        FeatureBit(42, 17),
    ],
    "OMUX[7]": [
        FeatureBit(8, 17),
        FeatureBit(2, 17),
        FeatureBit(4, 17),
        FeatureBit(7, 17),
        FeatureBit(3, 17),
        FeatureBit(6, 17),
        FeatureBit(5, 17),
    ],
}

KNOWN_VALUES = {
    "X2,Y14 SLICE[1].G": (2, 14, SLICE1_G, 0xF0FF),
    "X2,Y12 SLICE[0].G": (2, 12, SLICE0_G, 0xFFFA),
    "X2,Y3 SLICE[0].G": (2, 3, SLICE0_G, 0xAFAF),
    "X2,Y14 SLICE[0].F": (2, 14, SLICE0_F, 0xDDDD),
    "X2,Y14 SLICE[0].G": (2, 14, SLICE0_G, 0xD0F1),
}

# Listed-bit order, matching the pinned human-readable database (MSB first).
KNOWN_ROUTING_CONTROLS = {
    "PCILOGIC.I1": (0, 13, feature_range([51, 50, 5, 4, 2, 3, 49], 3), "1010000"),
    "PCILOGIC.I2": (0, 13, feature_range([7, 6, 48, 0, 1, 8, 9], 3), "1000001"),
    "PCILOGIC.I3": (0, 13, [FeatureBit(m, 4) for m in [9, 5, 6, 48]], "0001"),
    "REQ IOI[2].MUX_O": (0, 18, [FeatureBit(25, 16)], "1"),
    "REQ IOI[2].MUX_T": (0, 18, [FeatureBit(30, 16)], "1"),
    "GNT IOI[2].MUX_O": (0, 16, [FeatureBit(25, 16)], "0"),
    "GNT IOI[2].MUX_T": (0, 16, [FeatureBit(30, 16)], "0"),
    "X2,Y14 OMUX[0] to SINGLE_S[1]": (2, 14, [FeatureBit(47, 5)], "1"),
    "X0,Y11 SINGLE_E[19] to HEX_V3[3]": (0, 11, [FeatureBit(9, 8)], "0"),
}


def validate_knowns(bitstream: Xc2s200eBitstream) -> dict[str, object]:
    checks: dict[str, object] = {
        "project_combine_commit": PROJECT_COMBINE_COMMIT,
        "fdri_payload_offset": bitstream.fdri_payload_offset,
        "fdri_word_count": bitstream.fdri_word_count,
        "chip18_col_frame_x2": COL_FRAME[2],
        "known_luts": {},
        "x2y14_slice0_controls": {},
        "routing_and_iob_controls": {},
    }

    failures: list[str] = []
    known_luts = checks["known_luts"]
    assert isinstance(known_luts, dict)
    for name, (col, row, features, expected) in KNOWN_VALUES.items():
        actual = bitstream.bit_vector(col, row, features)
        known_luts[name] = {
            "actual": f"0x{actual:04X}",
            "expected": f"0x{expected:04X}",
        }
        if actual != expected:
            failures.append(f"{name}: expected 0x{expected:04X}, got 0x{actual:04X}")

    controls = checks["x2y14_slice0_controls"]
    assert isinstance(controls, dict)
    for name, features in X2Y14_SLICE0_CONTROLS.items():
        controls[name] = bitstream.bit_string(2, 14, features)

    expected_controls = {
        "SLICE[0].BX inversion": "0",
        "SLICE[0].DXMUX": "0",
        "SLICE[0].FXMUX": "10",
        "SLICE[0].FF_SR_SYNC": "0",
        "SLICE[0].FF_SR_ENABLE": "1",
        "SLICE[0].FFX_INIT": "1",
        "IMUX_CLB_CLK[0]": "001000",
        "IMUX_CLB_CE[0]": "000000",
        "IMUX_CLB_SR[0]": "000010",
        "OMUX[0]": "0011011",
        "OMUX[7]": "0011011",
    }
    for name, expected in expected_controls.items():
        actual = controls[name]
        if actual != expected:
            failures.append(f"{name}: expected {expected}, got {actual}")

    for name, (col, row, features, expected) in KNOWN_ROUTING_CONTROLS.items():
        actual = bitstream.bit_string(col, row, features)
        checks["routing_and_iob_controls"][name] = {"actual": actual, "expected": expected}
        if actual != expected:
            failures.append(f"{name}: expected {expected}, got {actual}")

    checks["failures"] = failures
    checks["status"] = "PASS" if not failures else "FAIL"
    return checks


def run_self_test() -> None:
    assert COL_FRAME[2] == 2030
    assert COL_FRAME[0] == 2186
    assert COL_FRAME[24] == 8

    slot = [0] * VIRTEX_TRANSFER_WORDS
    slot[16] = 1 << 4
    slot[15] = 1
    bits = virtex_frame_bits(slot)
    assert bits[0] == 1
    assert bits[28] == 1
    assert sum(bits) == 2


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "image", nargs="?", type=Path, help="private BINARY_205_decoded.bin path"
    )
    parser.add_argument(
        "--validate-knowns",
        action="store_true",
        help="validate current known control values",
    )
    parser.add_argument(
        "--self-test", action="store_true", help="run public-data-free unit checks"
    )
    parser.add_argument("--project-combine", type=Path, help="clean pinned Project Combine checkout")
    parser.add_argument("--build-adapter", action="store_true", help="build the public architecture adapter")
    cargo_mode = parser.add_mutually_exclusive_group()
    cargo_mode.add_argument("--cargo-offline", action="store_true", help="build using cached Rust dependencies only (default)")
    cargo_mode.add_argument("--allow-dependency-download", action="store_true", help="explicitly allow Cargo to download public build dependencies")
    parser.add_argument("--adapter", type=Path, help="previously built prjcombine_graph executable")
    parser.add_argument("--trace", help="upstream routing tree: X,Y,WIRE (e.g. 2,14,IMUX_CLB_F1[0])")
    parser.add_argument("--bel", help="decode BEL inputs and attributes: X,Y,BEL")
    parser.add_argument("--analyze-pci", action="store_true", help="reconstruct XQ and PCI control cones")
    parser.add_argument("--validate-architecture", action="store_true", help="validate native database and configured upstream routes")
    parser.add_argument("--register-table", help="D-path truth table for X,Y,BEL.PIN")
    parser.add_argument("--local-table", action="store_true", help="treat immediate register D-path sources as symbolic variables")
    parser.add_argument("--max-logic", type=int, default=200, help="maximum logic network nodes")
    parser.add_argument("--max-nodes", type=int, default=500, help="maximum routing nodes per trace")
    parser.add_argument("--max-depth", type=int, default=100, help="maximum routing depth")
    parser.add_argument("--output", type=Path, help="write private analysis JSON locally")
    parser.add_argument("--compact", action="store_true", help="omit disabled PIP candidates from JSON, retaining counts")
    args = parser.parse_args(argv)

    if args.build_adapter:
        if args.project_combine is None:
            parser.error("--build-adapter requires --project-combine")
        from virtexe_routing import build_adapter
        print(build_adapter(args.project_combine, not args.allow_dependency_download))
        return 0

    if args.self_test:
        run_self_test()
        print("self-test PASS")
        return 0

    if args.image is None:
        parser.error("image path is required unless --self-test is used")

    bitstream = Xc2s200eBitstream(args.image)
    if args.trace or args.bel or args.analyze_pci or args.validate_architecture or args.register_table:
        if args.adapter is None or args.project_combine is None:
            parser.error("routing requires --adapter and --project-combine")
        from virtexe_routing import (Architecture, Router, LogicAnalyzer, analyze_pci,
                                    validate_architecture, verify_checkout, compact_report, state_table)
        if (args.analyze_pci or args.register_table) and args.max_logic < 1:
            parser.error("--max-logic must be positive")
        checks = validate_knowns(bitstream)
        if checks["status"] != "PASS":
            print(json.dumps(checks, indent=2))
            return 1
        source = verify_checkout(args.project_combine)
        architecture = Architecture(args.adapter, source / "databases" / "virtex.zstd")
        try:
            router = Router(bitstream, architecture)
            native_checks = validate_architecture(router)
            if native_checks["status"] != "PASS":
                print(json.dumps(native_checks, indent=2))
                return 1
            result = {"frame_validation": checks, "architecture_validation": native_checks}
            if args.trace:
                x, y, name = args.trace.split(",", 2)
                result["trace"] = router.trace([int(x), int(y), name], args.max_nodes, args.max_depth)
            if args.bel:
                x, y, name = args.bel.split(",", 2)
                result["bel"] = router.bel(int(x), int(y), name)
            if args.analyze_pci:
                result["pci_analysis"] = analyze_pci(router, args.max_logic)
            if args.register_table:
                x, y, name = args.register_table.split(",", 2)
                bel, pin = name.rsplit(".", 1)
                analyzer = LogicAnalyzer(router, args.max_logic)
                key = analyzer.output({"x": int(x), "y": int(y), "bel": bel, "pin": pin})
                report = analyzer.report([key])
                result["register_analysis"] = report
                result["register_table"] = state_table(report, key, local=args.local_table)
            if args.compact:
                compact_report(result)
            encoded = json.dumps(result, indent=2, sort_keys=True)
            if args.output:
                args.output.write_text(encoded + "\n", encoding="utf-8")
                print(json.dumps({"output": str(args.output), "validation": "PASS"}))
            else:
                print(encoded)
            return 0
        finally:
            architecture.close()
    if args.validate_knowns:
        checks = validate_knowns(bitstream)
        print(json.dumps(checks, indent=2, sort_keys=True))
        return 0 if checks["status"] == "PASS" else 1

    print(
        json.dumps(
            {
                "project_combine_commit": PROJECT_COMBINE_COMMIT,
                "fdri_payload_offset": bitstream.fdri_payload_offset,
                "fdri_word_count": bitstream.fdri_word_count,
                "decoded_main_frames": len(bitstream.frames),
                "chip18_col_frame_x2": COL_FRAME[2],
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
