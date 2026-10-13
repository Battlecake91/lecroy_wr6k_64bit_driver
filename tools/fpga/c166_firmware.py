"""Sparse, read-only C166 control-flow inventory using an external decoder.

The GPL decoder is not vendored. Listings and source memory stay private.
Syntactic decoding is not a DPP/EXTS-aware execution or reachability proof.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path

DECODER_COMMIT = "35d4488ee6b35d246dfc0dc87e0667b385f9d43e"
DECODER_HASHES = {
    "c166_disasm.py": "f2922b0725c17d391cfbdf1f2b462f3471d93a4f26ffd35d2448a99428745104",
    "c166_is.txt": "edcac8e20991266a2fc436e40e6171b93dfb6916d595e67b6ae68c353ada77a4",
    "c166_cc.txt": "2f99cc288c57a0190d720dc64c9b175d6da8fe9a01e46cb7ab4b40e5818697ad",
    "c166_sfr.txt": "99224d2d1c73892bea2954f5807c1d264a56967e95e6395dcdcdf5a98bd774f8",
}
ISA = "https://www.keil.com/dd/docs/datashts/infineon/c166ism.pdf"


def spans(memory):
    result = []
    for address in sorted(memory):
        if not result or result[-1][1] != address:
            result.append([address, address + 1])
        else:
            result[-1][1] += 1
    return result


def direct_flow(raw, address):
    """Infineon C166 ISA v2.0: branch displacement is in words; IP wraps."""
    if len(raw) not in (2, 4):
        raise ValueError("instruction must contain two or four bytes")
    op, operand = raw[:2]
    if op in (0xfa, 0xda, 0xea, 0xca, 0xe2, 0x8a, 0x9a, 0xaa, 0xba) and len(raw) != 4:
        raise ValueError("truncated branch instruction")
    segment, following = address & ~0xffff, (address + len(raw)) & 0xffff
    rel = lambda byte: segment | ((following + 2 * (byte if byte < 128 else byte - 256)) & 0xffff)
    near = lambda: segment | int.from_bytes(raw[2:4], "little")
    if op in (0xfa, 0xda):
        return {"kind": "call" if op == 0xda else "jump", "targets": [operand << 16 | int.from_bytes(raw[2:4], "little")],
                "fallthrough": op == 0xda}
    if op in (0xea, 0xca):
        if operand & 15:
            return {"kind": "unknown", "reason": "reserved direct branch operand"}
        return {"kind": "call" if op == 0xca else "jump", "targets": [near()], "fallthrough": op == 0xca or operand != 0}
    if op & 15 == 13:
        return {"kind": "jump", "targets": [rel(operand)], "fallthrough": op != 0x0d}
    if op == 0xbb:
        return {"kind": "call", "targets": [rel(operand)], "fallthrough": True}
    if op == 0xe2:
        return {"kind": "call", "targets": [near()], "fallthrough": True}
    if op in (0x8a, 0x9a, 0xaa, 0xba):
        return {"kind": "bit_jump", "targets": [rel(raw[2])], "fallthrough": True}
    if op in (0xcb, 0xdb, 0xeb, 0xfb):
        return {"kind": "return", "targets": [], "fallthrough": False}
    if op in (0x9c, 0xab):
        return {"kind": "indirect", "targets": [], "fallthrough": op == 0xab or operand >> 4 != 0}
    if op == 0xb7:
        if raw != bytes.fromhex('b7 48 b7 b7'):
            raise ValueError("invalid protected SRST encoding")
        return {"kind": "reset", "targets": [], "fallthrough": False}
    if op == 0x9b:
        return {"kind": "unknown", "reason": "trap vector/segment context requires execution contract"}
    return {"kind": "linear", "targets": [], "fallthrough": True}


def load_decoder(directory):
    hashes = {name: hashlib.sha256((Path(directory) / name).read_bytes().replace(b"\r\n", b"\n")).hexdigest()
              for name in DECODER_HASHES}
    if hashes != DECODER_HASHES:
        raise ValueError("external C166 decoder does not match the reviewed revision (LF-normalized hashes)")
    path = Path(directory) / "c166_disasm.py"
    spec = importlib.util.spec_from_file_location("wr6k_external_c166", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.C166Disassembler(), hashes


def extended_operand(raw):
    """Decode EXT addressing metadata independently of incomplete display helpers."""
    if not raw or raw[0] not in (0xd7, 0xdc):
        return None
    if len(raw) != (4 if raw[0] == 0xd7 else 2):
        raise ValueError("truncated EXT instruction")
    mode = ("segment", "page", "segment_extended_sfr", "page_extended_sfr")[raw[1] >> 6]
    result = {"mode": mode, "instructions": ((raw[1] >> 4) & 3) + 1}
    if raw[0] == 0xdc:
        result["register"] = raw[1] & 15
    else:
        result["constant"] = int.from_bytes(raw[2:], "little")
        if raw[1] & 15 or result["constant"] >= (1024 if "page" in mode else 256):
            raise ValueError("reserved EXT encoding")
    return result


def table_targets(memory, address, count, segment):
    if address & 1 or not 1 <= count <= 256 or segment & 0xffff:
        raise ValueError("invalid explicitly reviewed jump table")
    return [segment | int.from_bytes(bytes(memory[address + 2 * i + j] for j in range(2)), "little")
            for i in range(count)]


def serial_word_loop(memory, address, clock_bit, din_bit=9, register=15):
    """Validate sixteen BSET/BMOV/BCLR triples independently of display text.

    Infineon ISA pp.50,54,57: clock rises BEFORE DIN changes. The first
    sampled bit is previous DIN, and the final assigned bit needs another edge.
    """
    def read(at, count):
        return bytes(memory[at + i] for i in range(count))
    for bit in range(16):
        at = address + bit * 8
        expected = bytes((clock_bit << 4 | 15, 0xe0, 0x4a, 0xf0 | register,
                          0xe0, bit << 4 | din_bit, clock_bit << 4 | 14, 0xe0))
        if read(at, 8) != expected:
            raise ValueError("serial word loop differs from reviewed P2 encoding")
    return {"clock": clock_bit, "din": din_bit, "source_register": register,
            "bits": list(range(16)), "order": "rise, assign DIN, fall",
            "sampling": "previous DIN at rising edge; not a complete configuration-success proof"}


def copy_records6(memory, address, limit=128):
    """Narrow format recovered at 0xE9E..0xEC0, not a general C startup ABI.

    Return consecutive tag6 (offset,page,offset,page,length) copies only.
    Other tags terminate with an explicit boundary, never guessed semantics.
    """
    def word(at):
        return memory[at] | memory[at + 1] << 8
    result = []
    for _ in range(limit):
        tag = word(address)
        if tag != 6:
            return result, {"address": address, "tag": tag}
        dest, dp, source, sp, length = (word(address + i) for i in (2, 4, 6, 8, 10))
        if dp >= 1024 or sp >= 1024 or (dest & 0x3fff) + length > 0x4000 or (source & 0x3fff) + length > 0x4000:
            raise ValueError("unsupported startup page/length")
        result.append({"record": address, "destination": dp << 14 | dest & 0x3fff,
                       "source": sp << 14 | source & 0x3fff, "length": length})
        address += 12
    raise ValueError("startup record limit")


def inventory(memory, decoder, entries, limit=40000, tables=None):
    """Conservative syntactic CFG. Does not fill holes or scan data as code."""
    pending, nodes, edges, boundaries = list(entries), {}, [], []
    occupied = {}
    while pending:
        address = pending.pop()
        if address in nodes:
            continue
        if len(nodes) >= limit:
            boundaries.append({"address": address, "reason": "instruction limit"})
            break
        if address & 1 or address not in memory:
            boundaries.append({"address": address, "reason": "unaligned or unmapped target"})
            continue
        length = decoder.clen.get(memory[address])
        if length not in (2, 4) or any(address + i not in memory for i in range(length)):
            boundaries.append({"address": address, "reason": "unknown opcode or sparse/truncated instruction"})
            continue
        raw = bytes(memory[address + i] for i in range(length))
        _, _, text = decoder.decode_one(raw, 0, address)
        if text.startswith(("(unknown", "(decode error")):
            boundaries.append({"address": address, "reason": text})
            continue
        if any(address + i in occupied and occupied[address + i] != address for i in range(length)):
            boundaries.append({"address": address, "reason": "overlapping instruction interpretation"})
            continue
        for i in range(length):
            occupied[address + i] = address
        flow = direct_flow(raw, address)
        if flow["kind"] == "indirect" and address in (tables or {}):
            table, count = tables[address]
            flow = {"kind": "reviewed_table", "targets": table_targets(memory, table, count, address & ~0xffff),
                    "fallthrough": flow["fallthrough"], "table_address": table, "table_count": count,
                    "assumption": "caller supplied bounds and effective data page from reviewed instructions"}
        nodes[address] = {"raw": raw.hex(), "assembly": text, "flow": flow,
                          "operand_boundary": "INCOMPLETE" in text or raw[0] in (0xd1, 0xd7, 0xdc)}
        if raw[0] in (0xd7, 0xdc):
            nodes[address]["extended_addressing"] = extended_operand(raw)
        if flow["kind"] in ("unknown", "indirect"):
            boundaries.append({"address": address, "reason": flow.get("reason", "indirect target/segment context")})
        for target in flow.get("targets", []):
            edges.append({"from": address, "to": target, "kind": flow["kind"]})
            pending.append(target)
        if flow.get("fallthrough"):
            following = (address & ~0xffff) | ((address + length) & 0xffff)
            edges.append({"from": address, "to": following, "kind": "fallthrough"})
            pending.append(following)
    return {"instructions": nodes, "edges": edges, "boundaries": boundaries,
            "scope": "syntactic direct-flow candidates only; no data-page propagation, indirect dispatch or hardware reachability"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sparse_json", type=Path)
    parser.add_argument("--decoder", type=Path, required=True)
    parser.add_argument("--entry", action="append", type=lambda x: int(x, 0))
    parser.add_argument("--table", action="append", default=[], help="Reviewed indirect-IP:physical-table:count, integers accept 0x")
    parser.add_argument("--private-output", type=Path, required=True)
    args = parser.parse_args()
    if args.private_output.resolve().is_relative_to(Path(__file__).resolve().parents[2]):
        parser.error("private firmware listing must be outside the repository")
    if args.private_output.resolve() == args.sparse_json.resolve():
        parser.error("private output must not overwrite source memory")
    memory = {int(k): v for k, v in json.loads(args.sparse_json.read_text()).items()}
    decoder, hashes = load_decoder(args.decoder)
    tables = {}
    for value in args.table:
        ip, table, count = (int(v, 0) for v in value.split(":"))
        tables[ip] = (table, count)
    result = inventory(memory, decoder, args.entry or [0], tables=tables)
    result.update(memory_spans=spans(memory), mapped_bytes=len(memory), decoder_files=hashes,
                  decoder_revision=DECODER_COMMIT, instruction_reference=ISA)
    args.private_output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print(json.dumps({"instructions": len(result["instructions"]), "boundaries": len(result["boundaries"]),
                      "operand_boundaries": sum(n["operand_boundary"] for n in result["instructions"].values()),
                      "mapped_bytes": len(memory), "spans": result["memory_spans"]}))


if __name__ == "__main__":
    main()
