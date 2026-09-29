# CFDC2400: pending-bit callback and unresolved derived virtual call (2026-09-30)

## Status and safety boundary

**Original callback behavior is proved; final virtual method is NOT
yet proved for the correct object type.** Original `CFDC2400`
copies a four-byte caller value into a temporary global, then its
synchronized callback ORs that value into the driver's pending
interrupt bitmap. A later virtual method is also invoked.

A minimal native x64 case was provisionally drafted while analyzing
the pending-bit callback, but **was reverted before any Windows build
or user deployment** after an ABI/vtable inconsistency was found.
The shipping native `driver/Ioctl.c` and
`driver/LecS65Drv.h` remain at the latest tested CFDC2194/
CFDC2190 code baseline with **no CFDC2400 case**. Do not
re-enable the provisional case merely to increase IOCTL coverage.

The user's last real-scope outcome after the CFDC2194 patch was
a successful install/idle ABI test and a practical XStream
regression with no observed malfunction. No new hardware test is
needed to investigate CFDC2400 yet.

## Verified original dispatch and request contract

`ghidra_exports/selected/raw_11300.asm.txt`:

```asm
00011292 CMP EAX,0xCFDC2400
00011297 JZ  0x000112E9
...
000112E9 PUSH EDI
000112EA LEA  ECX,[ESI+0x1E0] ; hardware SUBOBJECT, not main ESI
000112F0 CALL 0x00013A2E
```

Original `FUN_00013A2E` delegates to `FUN_00012EDE`
and puts its return value into IRP `IoStatus.Status`
(offset +0x18). `FUN_00012EDE` checks input length
**exactly 4** and requires the METHOD_BUFFERED system input
pointer to be non-null. It copies the little-endian input
DWORD, without a range/mask check, into `DAT_0001CE1C`.
On successful completion it sets `IoStatus.Information=0`
and `IoStatus.Status=0`. Bad input length or pointer returns
`STATUS_INVALID_PARAMETER (0xC000000D)`. No original output
length constraint is visible for this handler.

## The callback is already completely present in an older export

`ghidra_exports/selected/raw_12ec2.asm.txt`:

```asm
00012EC2 CMP dword ptr [ESP+0x4],0
00012EC7 JZ  0x12ED8
00012EC9 MOV EAX,[0x0001CE1C]
00012ECE OR  dword ptr [0x0001CE10],EAX
00012ED4 MOV AL,0x1
00012ED6 JMP 0x12EDA
00012ED8 XOR AL,AL
00012EDA RET 0x4
```

At original VA 0x12EF8 `FUN_00012EDE` writes the
incoming DWORD to global `DAT_0001CE1C`; it then calls
the synchronization framework (`FUN_00010A88`) with
`LAB_00012EC2` and its non-null receiver.

Thus the callback performs:

```c
DAT_0001CE10 |= requestedMask;
```

This is the same original pending-source bitmap that ISR
`FUN_000108D6` accumulates enabled interrupt sources into.
Original per-source DPC helpers `FUN_00011DC2`,
`FUN_00011DD8`, `FUN_00011DEE`,
`FUN_00011E04`, `FUN_00011E1A` and
`FUN_00011E30` test/consume individual bits. The
`FUN_00010636` synchronization helper invokes callbacks
directly without an interrupt object and via
`KeSynchronizeExecution` when one is attached.

**No FPGA register write occurs in this callback.**
However, that does not establish the behavior of the following
virtual method or justify declaring the whole IOCTL a no-op
apart from the pending-bit OR.

## Important vtable discrepancy discovered during this analysis

Earlier `AGENTS.md` / `docs/ioctl-map.md` call the final
`+0x24` virtual method a no-op at `0x000104A0`.
The method body at `raw_104a0.asm.txt` does indeed say
`XOR EAX,EAX; RET`, but its assignment to **this**
particular virtual call is not grounded in the correct
object receiver. In fact it is ABI-inconsistent:

1. Original main-device construction in
   `raw_asm_10b30.asm.txt` sets main-object VTable to
   **0x0001C500**. The embedded hardware subobject is
   at `main+0x1E0`, and constructor
   `FUN_00010B3C` sets its active derived VTable to
   **0x0001C62C** (after base constructor
   `FUN_00014212` initially assigns `0x0001C8BC`).
2. The original dispatch **passes the hardware subobject**
   to `FUN_00013A2E`, then `FUN_00012EDE`. It is not
   calling the main-device VTable.
3. `raw_12ec2.asm.txt` shows the exact virtual call:

   ```asm
   00012F0E MOV EAX,[EDI]       ; EDI = hardware subobject
   00012F10 PUSH 0
   00012F12 PUSH 0
   00012F14 MOV ECX,EDI
   00012F16 CALL dword ptr [EAX+0x24]
   00012F19 AND dword ptr [ESI+0x1C],0
   ```

   Therefore the concrete slot to read is
   `0x0001C62C + 0x24 = 0x0001C650`, not the
   main-object VTable's `0x0001C500 + 0x24`.
4. There is **no caller-side ADD ESP,8** after the
   two arguments are pushed. A target consisting solely
   of `XOR EAX,EAX; RET` (without stack-argument cleanup)
   cannot match that call convention if the exported assembly
   is complete. The actual target likely has a different
   return convention and possibly additional behavior.

We must inspect the raw DWORD stored at `0x0001C650` and
the instruction sequence at its target before porting any
remaining side effect. Do not confuse the no-op body at
`0x000104A0` with proof about the derived-subobject slot.

## Next **static-only** Ghidra export now prepared

`ghidra_scripts/ExportSelected.java` now understands
`dwords:<hexbase>:<count>`: it directly reads an
arbitrary little-endian DWORD pointer table, resolves any
entry recognized as a function, and previews instructions
at slot `+0x24` through the first RET or 24 instructions.
This is **read-only Ghidra analysis**.

The updated `ghidra_scripts/targets.txt` includes:

```text
dwords:1c62c:16     # actual derived hardware-subobject VTable
dwords:1c500:16     # main-device VTable, for contrast
dwords:1c8bc:16     # base hardware-subobject VTable
asm:13a2e
asm:12ede
asm:104a0
asm:10a88
xref:12ec2
xref:1ce10
xref:1ce1c
xref:1c62c
xref:1c500
```

After the user executes the existing PC-side analysis runner,
inspect particularly:

```text
ghidra_exports/selected/dwords_dwords_1c62c_16.txt
```

The exporter sanitizes `dwords:1c62c:16` as
`dwords_dwords_1c62c_16.txt`.
The slot `+0x24` should reveal the concrete function
address and its return convention. If the preview is
insufficient to establish all effects, add that newly
identified function address to targets for a complete
decompile/ASM run.

PC-only command:

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
& ".\scripts\run-ghidra-analysis.ps1" -CommitMessage "analysis: resolve CFDC2400 derived-vtable slot"
```

This runner pulls the current main, exports selected original
driver evidence and pushes the new export files. It does
**not** build/reload the scope's x64 kernel driver.

## Proposed x64 mapping after the unresolved virtual call is proved

The callback's pending-bit OR corresponds closely to native
`DevExt->InterruptPendingShadow`, already used by
`driver/Acquisition.c`: ISR uses `InterlockedOr` and
DPC uses `InterlockedExchange(..., 0)`. If no further
side effect invalidates the mapping, a native callback equivalent
can use `InterlockedOr` with the **entire** caller-supplied
32-bit mask. The original callback does not intersect that
injected mask with `InterruptEnableShadow`.

**Do not introduce a forced DPC, physical BAR access,
or synthetic signal until the derived method is resolved.**
A zero input would be a safe future positive-path test
once source is complete; nonzero values inject software
interrupt sources and may affect later acquisition/event
handling. Avoid deliberate nonzero-mask tests on the
working production scope.

## Source inventory

- `ghidra_exports/selected/raw_11300.asm.txt`
- `ghidra_exports/selected/00013a2e_FUN_00013a2e.c`
- `ghidra_exports/selected/00012ede_FUN_00012ede.c`
- `ghidra_exports/selected/raw_12ec2.asm.txt`
- `ghidra_exports/selected/00010636_FUN_00010636.c`
- `ghidra_exports/selected/000108d6_FUN_000108d6.c`
- `ghidra_exports/selected/00011dc2_FUN_00011dc2.c`,
  `00011dd8_FUN_00011dd8.c` and other DPC bit consumers
- `ghidra_exports/selected/raw_asm_10b30.asm.txt`
- `ghidra_exports/selected/raw_104a0.asm.txt`
