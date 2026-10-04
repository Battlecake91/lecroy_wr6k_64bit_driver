# Legacy PE32 executable coverage

This page records **the current verified static byte/function coverage**
for the original `LecS65AcqDrv.sys` image as analyzed by the checked-in
Ghidra project. It does not claim source-code equivalence or dynamic
behavioral coverage. The original 420-function inventory and subsequent
recovery history are traceable in Git rather than preserved as competing
current-state summaries.

## Current function inventory

| Measurement | Verified result |
|---|---:|
| Recognized internal functions | **527** |
| External function-manager entries | **87** |
| Inclusive Ghidra function-manager count | **614** |
| Internal functions with selected pseudocode and semantic map | **527** |
| Instruction bytes inside recognized functions | **48,086** |
| Decoded instruction bytes outside recognized functions | **25** |
| Instruction count outside recognized functions | **13** |
| Distinct unowned instruction clusters | **3** |

Ghidra initially recognized 420 internal functions. Reviewed
function-boundary recovery added **25** entries (commit
`9d3ac5b10245d9c9098c3fca2642bf259d7ef507`) and then **82**
(79 existing decoded thunks plus three opcode-checked new code functions;
commit `7bdbd3cf797a991ab8ab3dc686e069fcc419e343`).
All function descriptions reside in the
[canonical function map](legacy-driver-function-map.md).
Recovered compiler-generated virtual tail dispatches sometimes display
a misleading `Could not recover jumptable` warning; raw
`JMP [vtable+slot]` assembly establishes their control flow.

## Complete code-unit census of executable memory blocks

The final targeted Ghidra export commit
`ee9f4d86abd467927ac0985041aefacb654d75c7` produces
`CODE_COVERAGE.txt`, `EXECUTABLE_BYTE_CLASSIFICATION.txt`,
`UNDEFINED_EXECUTABLE_RANGES.txt` and `SEH_FRAGMENT_000180c1.txt`.

| Executable block | Total | Decoded instructions | Defined data | Undefined |
|---|---:|---:|---:|---:|
| `.text` | 49,024 | 45,559 | 2,615 | 850 |
| `PAGE` | 2,560 | 1,967 | 508 | 85 |
| `INIT` | 2,944 | 585 | 2,254 | 105 |
| **All executable blocks** | **54,528** | **48,111** | **5,377** | **1,040** |

The decoded-instruction total is **48,086 + 25 = 48,111**
bytes. The executable blocks total
**48,111 + 5,377 + 1,040 = 54,528** bytes.

### Three legitimate unowned SEH instruction clusters

| Region | Decoded bytes | Incoming metadata |
|---|---:|---|
| `0x18067..0x1806D` | 7 | references from `0x1C9D4`, `0x1C9D8` |
| `0x180BD..0x180C3` | 7 | references from `0x1C9E4`, `0x1C9E8` |
| `0x1814C..0x18156` | 11 | references from `0x1C9F0`, `0x1C9F4` |

All six relevant incoming references are **DATA** references from
compiler x86 structured exception handling (SEH) scope records,
not ordinary calls. These fragments are associated with the
transfer/MDL probe/lock and cleanup handlers and do **not**
constitute three ordinary missing C++ functions. The
`0x180C1..0x180C3` bytes `8B 65 E8` were successfully
disassembled to `MOV ESP,[EBP-0x18]` in the final targeted run.
No artificial function was created. These instructions are
part of exception filter/cleanup and frame restoration.

### Remaining 1,040 undefined bytes

The 423 undefined memory ranges from Ghidra have been reviewed
by contents and grouped as follows:

| Byte pattern/class | Bytes | Interpretation |
|---|---:|---|
| `CC` bytes | 282 | break/padding/alignment |
| Zero-filled bytes | 281 | zero padding, reserved storage |
| Printable strings or small inline constants | 458 | e.g. BAR0/BAR1/BAR2, ERRS/ERRM, format strings and integer constants |
| Mixed `CC` plus `0A 00 00 00` | 19 | mixed alignment and literal decimal 10 |
| **Total** | **1,040** | |

This is a **byte-pattern classification**, not an instruction by
instruction proof that every undefined byte is noncode. No
additional plausible self-contained ordinary function was found
in the checked ranges; any new evidence of an indirect entrypoint
must still be investigated. Three earlier suspicious undefined
sequences were explicitly disassembled and recovered as functions:

- `0x18E58`: PnP minor-code name lookup, 24 slots (0..23);
- `0x18EDB`: Power minor-code name lookup, four slots (0..3);
- `0x1C280`: compiler SEH exception-frame/status check.

## WDM dispatch table and source evidence

The [architecture map](legacy-driver-architecture.md) records the
complete **28-entry IRP_MJ dispatch table** at `0x1CD10..0x1CD7F`,
including each indexed trampoline, device vtable slot and concrete
handler. The two subsequent DWORDs, `0x00000004` at `0x1CD80`
and zero at `0x1CD84`, are **not** dispatch entries.
The PnP minor-name pointer table begins at `0x1CD88` and has
24 entries; the four Power minor-name pointers start at
`0x1CDE8`.

The original complete 27-case device-control dispatcher is
`0x11018..0x1138F`. The original raw assembly was captured in
`UNOWNED_CODE_ASM.txt` at commit `d5e08317e515ea46cb525b5628ea3e67de9c5e46`,
and the recovered pseudocode is in
`ghidra_exports/selected/00011018_FUN_00011018.c`.
The independently recovered
[original IOCTL map](ioctl-map.md) includes the legacy
unknown-IOCTL success-status overwrite defect and all existing
Dallas/FPGA writer dispatch paths.

## Scope of completion

**Static Ghidra executable-instruction ownership is accounted for**:
527 internal functions plus three explicitly identified
compiler SEH-scope instruction clusters. The remaining undefined
bytes have plausible padding/data signatures, but have not been
automatically forced into false code or data types.

This does **not** mean a bit-identical original source
reconstruction or fully verified x64-driver replacement. Open
behavioral work includes:

- matching original IRP completion, queuing, cancel, power and
  PnP lifetime semantics with safe modern x64 implementations;
- verifying actual acquisition, probing, IOCTL ABI and application
  behavior on hardware with controlled regressions;
- proving FPGA/GPIO and Dallas write sequencing only using safe,
  recoverable conditions, not the production licensed chip;
- revisiting any yet-undiscovered dynamic/indirect target
  indicated by traces or additional binary evidence.

No physical WR6k hardware or modern x64 driver was exercised or
modified by this static coverage effort.
