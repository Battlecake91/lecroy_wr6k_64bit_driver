# CFDC2194 status-latch provenance investigation

Status: **source analysis in progress; no new kernel handler or scope-side
test**. Based on selected original x86 Ghidra exports available on
`main`, 2026-09-30. Treat address equivalences below as candidates
until the original dispatch receiver is verified in assembly.

## What the original handler actually returns

Original `FUN_00012BAE` is the `0xCFDC2194` DeviceControl handler.
The selected C and `asm_asm_12bae.txt` agree:

- Output length must be **exactly 0x1D (29) bytes**.
- The response is initialized to all zeros.
- DWORD at response offset `+0x04` is `2`.
- DWORD at response offset `+0x08` is read from the original
  object at `this+0x116A`.
- The entire response is copied to the caller; the original latch is
  **then cleared**, and `Information = 0x1D` with success.
- An unexpected output size returns `0xC0000206` and zero information.

Key x86 instructions:

```text
00012bc6 MOV ESI,ECX
00012bd9 LEA EAX,[ESI + 0x116a]
00012bdf MOV ECX,dword ptr [EAX]
00012be1 MOV dword ptr [EBP + -0x18],ECX
...
00012bf4 AND dword ptr [EAX],0x0
00012bfb MOV dword ptr [EDX + 0x1c],0x1d
```

The observed read-then-clear is not an atomic XCHG. Nothing in this
handler identifies the producer of a nonzero value or independently
proves synchronization around that field. Do not treat its absence in
literal-displacement scan output as proof that the field is always
zero; a constant-success stub would be invented behavior.

## Additional object-base alias to investigate

`FUN_000115C4` initializes the register/hardware subobject with:

```c
FUN_00014847((void *)((int)this + 0x1e0),
             (int)this + 0x14b5,
             (int)this + 0x14d5,
             (int)this + 0x14f5);
```

Inside `FUN_00014847`, this subobject's `+0x138` member is
initialized as the BAR0 base register pointer. The original
`0x00223044` `FUN_00012D24` uses the same `this+0x138` field
to read the START/FVER DWORD; this replacement path has also passed
a live comparison against BAR0+0x000. The `0xCFDC2194` and
`0x00223044` handlers are in the recovered same DeviceControl
dispatch family.

**Under the common hardware-subobject receiver assumption:**

```text
hardware_subobject = main + 0x01E0
CFDC2194_latch   = hardware_subobject + 0x116A
                 = main + 0x134A
affected bytes   = subobject + 0x116A..0x116D
                 = main + 0x134A..0x134D
```

This is a meaningful alternate way to reach the field. A
`field:116a` scan alone may miss a writer that uses `main+0x134A`,
a precomputed base such as `main+0x1340` followed by `+0x0A`,
or an aliased/indirect destination pointer.

The original existing `field_0x116a.refs.txt` currently reports
only the `0x12BD9` `LEA` in the consumer. It is an **exact assembly
text scan**, not a whole-program pointer/dataflow proof.

## Related control path, but not yet a demonstrated latch writer

The companion `0xCFDC2190` handler `FUN_00013A40` accepts a
29-byte type-2 *input* record, manipulates global
`DAT_0001CE18`, writes `~input_dword_at_+8` through the register
wrapper at `subobject+0x188`, and registers `FUN_00012EAE` as a
callback. `FUN_00014847` binds that wrapper to BAR0+0x008.

The callback may invoke `FUN_00011E46`, which propagates the global
value to `DAT_0001CE20` (BAR0+0x084, INTEN, as initialized in
`FUN_00014847`). These are distinct register/global paths. No
currently reviewed selected source directly assigns them to the
software latch `subobject+0x116A`.

## Repeatable, read-only original-binary investigation

`ghidra_scripts/targets.txt` now requests additional exports using
the **existing** `ExportSelected.java` syntax:

- `asm:115c4`, `asm:13a40`, `asm:12eae`,
  `asm:11e46`, and the incoming `xref` reports for
  `12bae` and `12d24`.
- `field:1167` through `field:116d` (the `116a` scan already
  exists) and `field:1347` through `field:134d`, to catch direct
  starts of byte/WORD/DWORD accesses overlapping the four latch bytes,
  depending on the receiver base.
- `field:1160` and `field:1340` to locate possible nearby
  `LEA` bases later indexed by an additional `0x0A`.

Run this **on the Ghidra PC**, not on the live scope:

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
& ".\scripts\run-ghidra-analysis.ps1" -CommitMessage "analysis: trace CFDC2194 status latch aliases"
```

The established script pulls the current targets, exports the selected
original analysis, commits changed derived exports, and pushes them.
This new target set has been committed but **the Ghidra run has not
been executed by the assistant**. Never claim a scan result before
its generated reports exist.

Interpret results in the following order:

1. Confirm the common `main+0x1E0` dispatch receiver for
   `FUN_00012BAE` and `FUN_00012D24` from original calling
   conventions and available dispatch reference context.
2. Inspect direct `116x` and `134x` hits and the instruction
   widths; rule in only stores that actually overlap the four bytes.
3. For `LEA` or a saved field pointer, follow register/stack
   propagation and callers. Include constructor zeroing, memory
   copies and any ISR/DPC/callback write paths.
4. Establish the status-bit meanings, write/clear ordering and
   synchronization before implementing native `0xCFDC2194`.
   If static analysis cannot resolve the producer, plan a controlled
   **original x86** runtime data watchpoint, with the actual object
   instance address measured first. Do not probe guessed live
   kernel addresses.

The scan does not program registers, access the scope PCI hardware,
write Dallas memory or alter the x64 kernel driver. Protect the
currently working XStream, DMA/interrupt and AP015 baseline.

## Source locations

- `ghidra_exports/selected/000115c4_FUN_000115c4.c`:
  hardware subobject construction.
- `ghidra_exports/selected/00014847_FUN_00014847.c`:
  hardware register-wrapper initialization.
- `ghidra_exports/selected/00012d24_FUN_00012d24.c`:
  START/FVER handler and its `this+0x138` field.
- `ghidra_exports/selected/00012bae_FUN_00012bae.c` and
  `asm_asm_12bae.txt`: exact 29-byte read-and-clear semantics.
- `ghidra_exports/selected/field_0x116a.refs.txt`:
  earlier limited direct field scan.
- `ghidra_exports/selected/00013a40_FUN_00013a40.c`,
  `00012eae_FUN_00012eae.c`,
  `00011e46_FUN_00011e46.c`: related control/callback path.
