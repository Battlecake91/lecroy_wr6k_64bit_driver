#!/usr/bin/env python3
"""Compare passive legacy XStream and x64 replacement-driver JSONL traces.

The legacy user-mode tracer emits type=nt_ioctl records. The x64 driver tracer
emits type=ioctl records. This tool filters to the recovered LeCroy IOCTL space,
normalizes both formats, aligns request signatures, and reports early control-
flow and response differences.
"""

from __future__ import annotations

import argparse
import difflib
import json
from pathlib import Path
from typing import Any, Iterable


def load_jsonl(path: Path) -> tuple[list[dict[str, Any]], list[int]]:
    rows: list[dict[str, Any]] = []
    malformed: list[int] = []
    with path.open("r", encoding="utf-8", errors="replace") as stream:
        for line_number, line in enumerate(stream, 1):
            line = line.strip()
            if not line:
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError:
                malformed.append(line_number)
                continue
            if isinstance(value, dict):
                rows.append(value)
    return rows, malformed


def is_lecroy_ioctl(code: str) -> bool:
    try:
        value = int(code, 16)
    except (TypeError, ValueError):
        return False
    top = value & 0xFFFF0000
    return top in (0x00220000, 0xCFDC0000, 0xCFDD0000)


def normalize(rows: Iterable[dict[str, Any]]) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    for row in rows:
        row_type = row.get("type")
        if row_type not in ("ioctl", "nt_ioctl"):
            continue

        code = row.get("ioctl")
        if not isinstance(code, str) or not is_lecroy_ioctl(code):
            continue

        status = row.get("status")
        if status is None:
            status = row.get("ntstatus")

        information = row.get("information")
        if information is None:
            information = row.get("bytes_returned", 0)

        result.append(
            {
                "source_seq": row.get("seq"),
                "ioctl": code.upper().replace("0X", "0x"),
                "input_length": int(row.get("input_length", 0) or 0),
                "output_length": int(row.get("output_length", 0) or 0),
                "input_hex": str(row.get("input_hex", "") or "").upper(),
                "output_hex": str(row.get("output_hex", "") or "").upper(),
                "status": str(status or ""),
                "information": int(information or 0),
            }
        )
    return result


def request_signature(row: dict[str, Any]) -> str:
    code = row["ioctl"]

    # Event handles and transfer pointers are process-specific. Keep the ABI
    # shape in the signature but do not use raw pointer/handle bytes to align.
    if code in {
        "0xCFDC2180",
        "0xCFDC218C",
        "0x00223100",
        "0xCFDC2124",
        "0xCFDC2128",
        "0xCFDC2138",
        "0xCFDD219F",
    }:
        payload = f"<dynamic:{row['input_length']}>"
    else:
        payload = row["input_hex"]

    return (
        f"{code}|in={row['input_length']}|out={row['output_length']}|{payload}"
    )


def short_hex(value: str, limit: int = 96) -> str:
    if len(value) <= limit:
        return value
    return value[:limit] + f"...(+{(len(value)-limit)//2} bytes)"


def describe(label: str, index: int, row: dict[str, Any]) -> str:
    return (
        f"{label}[{index}] seq={row['source_seq']} {row['ioctl']} "
        f"in={row['input_length']} out={row['output_length']} "
        f"status={row['status']} info={row['information']}\n"
        f"  input : {short_hex(row['input_hex'])}\n"
        f"  output: {short_hex(row['output_hex'])}"
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("legacy", type=Path)
    parser.add_argument("x64", type=Path)
    parser.add_argument("--context", type=int, default=6)
    parser.add_argument("--max-differences", type=int, default=30)
    args = parser.parse_args()

    legacy_raw, legacy_bad = load_jsonl(args.legacy)
    x64_raw, x64_bad = load_jsonl(args.x64)
    legacy = normalize(legacy_raw)
    x64 = normalize(x64_raw)

    print(f"Legacy LeCroy IOCTLs: {len(legacy)}")
    print(f"x64 LeCroy IOCTLs:    {len(x64)}")
    if legacy_bad:
        print(f"Legacy malformed JSONL lines ignored: {legacy_bad}")
    if x64_bad:
        print(f"x64 malformed JSONL lines ignored: {x64_bad}")

    legacy_sig = [request_signature(row) for row in legacy]
    x64_sig = [request_signature(row) for row in x64]

    matcher = difflib.SequenceMatcher(
        None, legacy_sig, x64_sig, autojunk=False
    )
    opcodes = matcher.get_opcodes()

    print("\nFirst request-stream divergence:")
    divergence = next(
        (op for op in opcodes if op[0] != "equal"),
        None,
    )
    if divergence is None:
        print("  none")
    else:
        tag, i1, i2, j1, j2 = divergence
        print(f"  {tag}: legacy[{i1}:{i2}] vs x64[{j1}:{j2}]")
        for i in range(max(0, i1 - args.context), min(len(legacy), i2 + args.context)):
            prefix = ">" if i1 <= i < i2 else " "
            print(prefix + " " + describe("L", i, legacy[i]).replace("\n", "\n    "))
        print("  ---")
        for j in range(max(0, j1 - args.context), min(len(x64), j2 + args.context)):
            prefix = ">" if j1 <= j < j2 else " "
            print(prefix + " " + describe("X", j, x64[j]).replace("\n", "\n    "))

    print("\nResponse differences for aligned identical requests:")
    shown = 0
    for tag, i1, i2, j1, j2 in opcodes:
        if tag != "equal":
            continue
        span = min(i2 - i1, j2 - j1)
        for delta in range(span):
            left = legacy[i1 + delta]
            right = x64[j1 + delta]
            status_diff = left["status"].upper() != right["status"].upper()
            info_diff = left["information"] != right["information"]
            output_diff = left["output_hex"] != right["output_hex"]

            # x64 kernel trace may intentionally contain only an output preview.
            # If its preview is a prefix of the complete legacy output, do not
            # call that a data mismatch solely because the capture is shorter.
            if output_diff and right["output_hex"]:
                if left["output_hex"].startswith(right["output_hex"]):
                    output_diff = False

            if not (status_diff or info_diff or output_diff):
                continue

            print(
                f"\n#{shown + 1}: aligned request {left['ioctl']} "
                f"legacy seq={left['source_seq']} x64 seq={right['source_seq']}"
            )
            if status_diff:
                print(f"  status: legacy={left['status']} x64={right['status']}")
            if info_diff:
                print(
                    f"  information: legacy={left['information']} "
                    f"x64={right['information']}"
                )
            if output_diff:
                print(f"  legacy output: {short_hex(left['output_hex'], 160)}")
                print(f"  x64 output:    {short_hex(right['output_hex'], 160)}")

            shown += 1
            if shown >= args.max_differences:
                return 0

    if shown == 0:
        print("  none")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
