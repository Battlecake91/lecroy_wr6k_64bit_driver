#!/usr/bin/env python3
"""Summarize lecwatch ACTION START/END windows from an annotated JSONL session."""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


@dataclass
class Action:
    name: str
    start_qpc: int
    end_qpc: int | None = None
    ioctls: list[dict[str, Any]] = field(default_factory=list)


def load_rows(path: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    header: dict[str, Any] = {}
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8-sig", errors="replace") as stream:
        for line_number, line in enumerate(stream, 1):
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
            except json.JSONDecodeError as exc:
                raise SystemExit(
                    f"{path}:{line_number}: malformed JSON: {exc}"
                ) from exc
            if not isinstance(row, dict):
                continue
            if row.get("type") == "lecwatch_session":
                header = row
            else:
                rows.append(row)
    return header, rows


def event_qpc(row: dict[str, Any]) -> int | None:
    value = row.get("qpc")
    if value is None:
        value = row.get("timestamp_ticks")
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


def extract_actions(rows: list[dict[str, Any]]) -> list[Action]:
    ordered = sorted(
        enumerate(rows),
        key=lambda pair: (
            event_qpc(pair[1]) if event_qpc(pair[1]) is not None else 2**63 - 1,
            pair[0],
        ),
    )

    actions: list[Action] = []
    active: Action | None = None

    for _, row in ordered:
        qpc = event_qpc(row)
        if qpc is None:
            continue

        if row.get("type") == "marker":
            text = str(row.get("text", ""))
            if text.startswith("ACTION START: "):
                if active is not None:
                    actions.append(active)
                active = Action(text[len("ACTION START: "):], qpc)
            elif text.startswith("ACTION END: "):
                if active is not None:
                    active.end_qpc = qpc
                    actions.append(active)
                    active = None
            continue

        if row.get("type") == "ioctl" and active is not None:
            active.ioctls.append(row)

    if active is not None:
        actions.append(active)

    return actions


def short_name(row: dict[str, Any]) -> str:
    code = str(row.get("ioctl", "?"))
    detail = str(row.get("detail", "") or "")
    return f"{code} {detail}".rstrip()


def summarize_action(action: Action, frequency: int | None) -> list[str]:
    lines: list[str] = []
    if action.end_qpc is not None and frequency:
        duration = (action.end_qpc - action.start_qpc) / frequency
        duration_text = f"{duration:.6f} s"
    elif action.end_qpc is not None:
        duration_text = f"{action.end_qpc - action.start_qpc} ticks"
    else:
        duration_text = "open/incomplete"

    lines.append(f"### {action.name}")
    lines.append("")
    lines.append(
        f"Duration: {duration_text}; IOCTL records: {len(action.ioctls)}."
    )

    if not action.ioctls:
        lines.append("")
        lines.append("No IOCTL records were retained inside this action window.")
        return lines

    failures = [
        row for row in action.ioctls
        if str(row.get("status", "0x00000000")).upper() != "0X00000000"
    ]
    code_counts = Counter(str(row.get("ioctl", "?")) for row in action.ioctls)
    detail_counts = Counter(short_name(row) for row in action.ioctls)

    lines.append(f"Non-success status records: {len(failures)}.")
    lines.append("")
    lines.append("| IOCTL | Count |")
    lines.append("| --- | ---: |")
    for code, count in code_counts.most_common():
        lines.append(f"| `{code}` | {count} |")

    lines.append("")
    lines.append("Nested/detail breakdown:")
    lines.append("")
    lines.append("| IOCTL / detail | Count | Input variants |")
    lines.append("| --- | ---: | ---: |")

    variants: dict[str, set[str]] = defaultdict(set)
    for row in action.ioctls:
        variants[short_name(row)].add(str(row.get("input_hex", "") or ""))

    for key, count in detail_counts.most_common():
        safe = key.replace("|", "\\|")
        lines.append(f"| `{safe}` | {count} | {len(variants[key])} |")

    return lines


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("session", type=Path)
    parser.add_argument(
        "--markdown",
        type=Path,
        help="write the summary to this Markdown file instead of stdout",
    )
    args = parser.parse_args()

    header, rows = load_rows(args.session)
    frequency_value = header.get("qpc_frequency")
    try:
        frequency = int(frequency_value) if frequency_value is not None else None
    except (TypeError, ValueError):
        frequency = None

    actions = extract_actions(rows)

    output: list[str] = [
        "# lecwatch action summary",
        "",
        f"Session: `{args.session}`",
        "",
        f"Actions found: {len(actions)}.",
        f"Session dropped-record counter: {header.get('dropped', 'unknown')}.",
        "",
    ]

    if not actions:
        output.append(
            "No ACTION START/ACTION END marker pairs were found. "
            "Run XStream E2E with -TraceActions while lecwatch is open."
        )
    else:
        for action in actions:
            output.extend(summarize_action(action, frequency))
            output.append("")

    text = "\n".join(output).rstrip() + "\n"
    if args.markdown is not None:
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(text, encoding="utf-8")
        print(args.markdown)
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
