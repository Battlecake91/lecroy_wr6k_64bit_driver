# CFDC2194: source-proven ISR error-status latch (2026-09-30)

## Verification state

**The original latch producer is identified from freshly exported x86
instructions and the already selected decompilation.** Original
`FUN_000108D6` accumulates BAR0 ERRS into the latched DWORD at
`main+0x134A`, and separately sets its high bit for a persistent
error after acknowledgement. The original `0xCFDC2194` handler
`FUN_00012BAE` reads and clears exactly that DWORD via the hardware
subobject-relative coordinate `this+0x116A`.

The native x64 implementation has now been **source-committed**, not
Windows-built, loaded or hardware-tested by the assistant. Do not
confuse original source proof with native x64 runtime validation.

## Decisive alias and original function chain

`FUN_000115C4` calls the hardware subobject constructor
`FUN_00014847` with `ECX = main+0x1E0` (assembly
`asm_asm_115c4.txt` at 0x1167B..0x11681). The original
DeviceControl dispatch similarly calls `FUN_00012BAE` with
`LEA ECX,[ESI+0x1E0]` at 0x11322, followed by
`CALL 0x12BAE` at 0x11328
(`raw_11300.asm.txt`; ESI is the main-object receiver).
Therefore the latch has two exactly equivalent addresses:

```text
hardware-subobject + 0x116A
= main + 0x01E0 + 0x116A
= main + 0x134A       (DWORD, bytes 0x134A..0x134D)
```

The earlier `field_0x116a.refs.txt` saw only the reader's
`LEA [ESI+0x116A]`, since the original interrupt handler uses
`main` directly rather than the hardware subobject. The newly
exported reports resolve the missing producer:

```text
field_0x134a.refs.txt:
0x00010958 FUN_000108d6  OR dword ptr [ESI + 0x134a],EAX

field_0x134d.refs.txt:
0x00010A67 FUN_000108d6  OR byte ptr [ESI + 0x134d],0x80
```

Both are within the **original ISR**. Existing
`000108d6_FUN_000108d6.c` and
`000108d6_FUN_000108d6.refs.txt` confirm that it is called
from wrapper 0x10B30; `raw_asm_10b30.asm.txt` shows the
wrapper calls 0x108D6. The DPC is queued by that ISR.

## Exact original hardware behavior reconstructed

From `FUN_000108D6` and register wrappers established in
`FUN_00014847`:

- ISR first reads BAR0 `INTST` (+0x080). Its ownership gate
  compares it to the global interrupt-enable shadow.
- On **INTST bit 1 (0x02)**, it reads BAR0 `ERRS` (+0x004)
  as a DWORD into a local `errorState`. It ORs that *raw*
  DWORD into the sticky software status at `main+0x134A`.
- ERRS bits 10..14 (`0x0400, 0x0800, 0x1000, 0x2000,
  0x4000`) map to bits 0..4 of BAR1 `CLRERR` (+0x004),
  respectively. Nonzero mapped bits are written to CLRERR.
- The original writes the raw `errorState` back to BAR0
  ERRS (hardware acknowledgment), processes other IRQ sources,
  and acknowledges BAR0 INTST.
- For an accepted 0x02 source it then waits **1 microsecond**,
  re-reads INTST, and, if bit 1 remains set and a fresh ERRS
  read equals the original `errorState`, ORs those error bits
  into the cached ERRM mask, writes BAR0 `ERRM` (+0x008),
  and sets **latch bit 31 (`0x80000000`)** via the
  byte operation at `main+0x134D`.
- It then queues the normal DPC.

This confirms the latch is not an arbitrary interrupt snapshot:
its normal contents are OR-accumulated hardware error status,
with a distinct high-bit persistent/reassertion marker.

### Companion control IOCTL CFDC2190: corrected semantics

Original `FUN_00013A40` requires input length 29 and no output.
It accepts **DWORD type 2 at offset +4**; input DWORD at +8
controls global INTEN bit 1 based on whether it is zero or
nonzero. It writes the **bitwise complement of that same DWORD**
to BAR0 ERRM through `FUN_000107FE`, caching the actual
programmed ERRM value for the ISR's retry branch.

The prior x64 code treated the type field as its enable toggle and
wrote the +8 input directly without inversion. This has been
corrected **in source only** as part of the new implementation.
For a trace input of `0x00007FFF`, the original programming rule
yields ERRM `0xFFFF8000`. Actual x64 hardware behavior after
this correction is not yet tested.

### Original read-and-clear ABI

The separate handler `FUN_00012BAE` (`asm_asm_12bae.txt`)
requires **exactly 29 output bytes** and constructs the following
little-endian response:

| Offset | Bytes | Contents |
|---:|---:|---|
| +0x00 | 4 | zero |
| +0x04 | 4 | DWORD `2` |
| +0x08 | 4 | accumulated `main+0x134A` status |
| +0x0C | 17 | zero |

After constructing the response, the original clears the
software status DWORD and returns `STATUS_SUCCESS`,
`Information=29`. A wrong output size returns
`STATUS_INVALID_BUFFER_SIZE` (`0xC0000206`) with zero
information. The original handler's explicit load/copy/clear is
not atomic; the x64 code uses an `InterlockedExchange` to
avoid losing an ISR update between load and clear.

## Native x64 implementation committed, pending build/hardware test

| File | Source change |
|---|---|
| `driver/LecS65Drv.h` | Declare `0xCFDC2194`, sticky LONG error latch and cached ERRM LONG. |
| `driver/Driver.c` | Initialize cached ERRM to original constructor default `0xFFFFFFFF`; latch is zero-initialized with the device extension. |
| `driver/Acquisition.c` | Handle accepted INTST 0x02: read/OR/ack ERRS, CLRERR mapping, original 1-us reassertion check, ERRM update and sticky bit 31. Reset software shadows when disconnecting IRQ. |
| `driver/Ioctl.c` | Correct CFDC2190 type/enable/complement semantics and implement exact 29-byte CFDC2194 read-and-clear. |
| `tools/lecdiag/lecdiag.c` | New `error-status` command requests the full 29-byte readback, validates type/reserved zeros and displays its consumed latch DWORD. |

The preexisting x64 ISR gates and accumulates *enabled* INTST bits
rather than running every raw source bit after a shared interrupt
passes the original global gate; this is a preexisting replacement
design difference. The new ERRS path is activated for accepted
`status & 0x02`. Do not claim blanket byte-for-byte ISR
equivalence for uncommon simultaneous masked interrupts.

No Dallas write/emulation, probe command, DMA descriptor or
unrelated acquisition handler was intentionally changed. The
updated kernel is **not yet an observed working baseline**.

## Controlled next test on the actual x64 scope

1. Retain the previously known-good driver/OS recovery path. Close
   XStream and use an elevated PowerShell to build the new source.
   Do not interpret a thrown PowerShell script as a valid
   `$LASTEXITCODE` result.
2. Build driver and diagnostic executable:
   `.\scripts\build-driver.ps1 -BuildLecdiag`.
   If this fails, **stop**, supply the full error, and do not load.
3. After a successful build and review, use the established
   `.\scripts\build-sign-load-driver.ps1` procedure. Stop on
   any sign, install, PnP or verification problem.
4. With **XStream still closed**, run
   `.\tools\lecdiag\build\lecdiag.exe error-status`.
   Its 29-byte reply must have type 2, reserved bytes zero,
   and may have a zero or nonzero status depending on recent
   accepted hardware errors. The read **consumes** the latch.
   Calling it concurrently with XStream may steal a status
   the application expects. An immediate second call will
   usually return zero unless a new error IRQ occurs.
5. Start XStream and repeat only the established waveform/control/
   AP015 regression checklist. Verify especially that normal
   startup does not encounter a new error-IRQ/retry storm.
6. If checking original CFDC2190's actual hardware mask, the
   safe generic `lecdiag read 0 0x8` is read-only, but the
   interpretation depends on the last programmed control value.
   Do not inject guessed ERRM/ERRS writes or deliberately force
   an acquisition-board hardware fault on the only working scope.
7. Record whether genuine XStream ever calls CFDC2194 in
   routine use. A passing idle zero reply proves the positive ABI
   path, **not** that nonzero ISR capture and the bit-31 branch
   were exercised.

Reference exports: `field_0x134a.refs.txt`,
`field_0x134d.refs.txt`, `raw_asm_10b30.asm.txt`,
`raw_11300.asm.txt`, `000108d6_FUN_000108d6.c`,
`000115c4_FUN_000115c4.c`,
`00014847_FUN_00014847.c`,
`00013a40_FUN_00013a40.c`,
`00012bae_FUN_00012bae.c` and `asm_asm_12bae.txt`.
