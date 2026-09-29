# CFDC2400: synchronized pending-bit injection and immediate DPC dispatch (2026-09-30)

## Verification state

The owner's PC Ghidra rerun committed decisive literal vtable exports
in **`b7b31c8bf5a06e9621a3636776673b486f72bfe5`**.
The original complete `CFDC2400` control flow can now be reconstructed
from exact instructions rather than an incorrect base-class-vtable
assumption. A native x64 source implementation is now committed in
`driver/LecS65Drv.h`, `driver/Acquisition.c` and `driver/Ioctl.c`.

**IMPORTANT: New source is not yet Windows-built, signed/reloaded, or
tested on the real oscilloscope.** The last owner-confirmed working
XStream/AP015 baseline remains the prior CFDC2194 / corrected CFDC2190
driver, whose practical regression had no observed malfunction.
Do not treat the original binary analysis or GitHub source update as
hardware runtime validation.

## Original request and dispatch contract

From `ghidra_exports/selected/raw_11300.asm.txt`,
`asm_asm_13a2e.txt` and `asm_asm_12ede.txt`:

```asm
00011292 CMP EAX,0xCFDC2400
00011297 JZ  0x000112E9
...
000112E9 PUSH EDI
000112EA LEA  ECX,[ESI+0x1E0] ; hardware subobject
000112F0 CALL 0x00013A2E
...
00012EE6 CMP DWORD PTR [EAX+0x8],0x4 ; exact input length
00012EEF MOV EAX,[ESI+0x0C]           ; METHOD_BUFFERED input
00012EF2 TEST EAX,EAX                 ; required non-null
00012EF6 MOV EAX,[EAX]                ; read full DWORD
00012EF8 MOV [0x0001CE1C],EAX
```

`FUN_00013A2E` invokes `FUN_00012EDE` and stores its
return as `IoStatus.Status` (IRP+0x18).
Exactly four buffered input bytes are required. There is no
observed output buffer length requirement. On success, both
`IoStatus.Status` and `IoStatus.Information` are set to zero.
Null input or any input length other than four returns
`STATUS_INVALID_PARAMETER (0xC000000D)`.
No bit filtering or range validation is performed on the DWORD.

## Stage 1: software pending-bit OR under interrupt synchronization

`ghidra_exports/selected/raw_12ec2.asm.txt`:

```asm
00012EC2 CMP DWORD PTR [ESP+0x4],0
00012EC7 JZ  0x12ED8
00012EC9 MOV EAX,[0x0001CE1C]
00012ECE OR  DWORD PTR [0x0001CE10],EAX
00012ED4 MOV AL,0x1
00012ED6 JMP 0x12EDA
00012ED8 XOR AL,AL
00012EDA RET 0x4
```

The helper `FUN_00010A88` loads the interrupt synchronization
framework at `0x1CCD8`. Original `FUN_00012EDE` invokes that
framework with callback `LAB_00012EC2` and a non-null receiver.
The callback therefore does exactly:

```c
DAT_0001CE10 |= DAT_0001CE1C;
```

The `xref_xref_1ce10.txt` report independently confirms
that `DAT_0001CE10` receives original ISR
`FUN_000108D6` pending interrupts, starts at zero in
`FUN_00014142`, and is consumed by individual DPC source
handlers `FUN_00011DC2`, `FUN_00011DD8`,
`FUN_00011DEE`, `FUN_00011E04`,
`FUN_00011E1A` and `FUN_00011E30`.
The callback itself makes no direct hardware access.

## Stage 2: the CORRECT derived-vtable slot calls the DPC synchronously

The earlier historical claim that original vtable `+0x24`
was a no-op at `0x000104A0` was **wrong for this call**.
The user's new exports now resolve the ambiguity conclusively:

| Object or entry | Source | Actual value |
|---|---|---|
| Main object vtable | `dwords_dwords_1c500_16.txt` | `0x0001C500` |
| Base hardware-subobject vtable | `dwords_dwords_1c8bc_16.txt` | `0x0001C8BC` |
| Base subobject slot `+0x24` | `0x1C8E0` | `0x000104A0`, bare `RET` |
| **Active derived hardware-subobject vtable** | `dwords_dwords_1c62c_16.txt` | **`0x0001C62C`** |
| **Derived slot `+0x24`** | **`0x1C650`** | **`0x000114F2`** |

The original dispatch constructs `this = main+0x1E0`.
The derived-subobject constructor assigns active vtable
`0x1C62C`, as corroborated by
`xref_xref_1c62c.txt` at `0x10B5C`.

`asm_asm_12ede.txt` proves the actual post-callback call:

```asm
00012F0E MOV EAX,[EDI]       ; derived subobject vtable
00012F10 PUSH 0
00012F12 PUSH 0
00012F14 MOV ECX,EDI
00012F16 CALL DWORD PTR [EAX+0x24]
00012F19 AND DWORD PTR [ESI+0x1C],0
00012F1D AND DWORD PTR [ESI+0x18],0
00012F21 XOR EAX,EAX
```

The exported slot preview shows complete thunk `0x114F2`:

```asm
000114F2 PUSH DWORD PTR [ESP+0x8]
000114F6 ADD ECX,0xFFFFFE20 ; subobject this - 0x1E0 => main
000114FC PUSH DWORD PTR [ESP+0x8]
00011500 PUSH ECX
00011501 CALL 0x00011390
00011506 RET 0x8
```

The `RET 0x8` perfectly matches the two arguments pushed
by `FUN_00012EDE`. Most importantly the thunk calls
`FUN_00011390` **directly and immediately**, not
`KeInsertQueueDpc`. The already-exported original
`00011390_FUN_00011390.c` is the DPC/source dispatcher
for completion, error/event notification and command/HWInt
sources. Thus CFDC2400's full observable operation is:

```text
validated caller DWORD
  -> synchronize; OR whole DWORD into software pending bits
  -> synchronous DPC processing of pending source bits
  -> NTSTATUS success; Information = 0
```

This corrects the earlier incomplete proposed port that only
ORed pending bits without processing them.

## Native x64 source mapping (new, untested)

`driver/Acquisition.c` already uses the per-device
`DevExt->InterruptPendingShadow` as the corresponding pending
bitmap; ISR atomically ORs bits and its established
`LecInterruptDpc` uses `InterlockedExchange(...,0)`
before processing events and the HWInt source. Rather than
duplicating six branch handlers, the newly added
`LecInjectLegacyPendingAndDispatch(DevExt, PendingMask)`:

1. Checks its caller IRQL is no higher than `DISPATCH_LEVEL`.
2. Atomically ORs **all 32 caller bits** into the pending
   bitmap; no speculative hardware-enable-mask filtering.
3. If necessary temporarily raises the caller IRQL to
   `DISPATCH_LEVEL` to satisfy the existing DPC core's
   `KeAcquireSpinLockAtDpcLevel` usage.
4. Directly invokes `LecInterruptDpc` **synchronously**.
5. Restores the original IRQL and returns `STATUS_SUCCESS`.

This reuse retains the x64 replacement's existing event,
transfer-completion and ProBus/HWInt handling. It does not
synthesize a new interrupt, insert a new DPC or write a new
hardware register. An input DWORD of zero still dispatches
any previously pending bits, just as the original virtual
call is unconditional; hence a zero-mask test is a
**low-risk positive-path ABI test**, not a purely passive
query.

`driver/Ioctl.c` has a new `LECS65_IOCTL_CFDC2400` case.
It requires `systemBuffer != NULL` and
`inputLength == sizeof(ULONG)`, copies the request with
`RtlCopyMemory`, calls the helper and reports zero output
information. `driver/LecS65Drv.h` declares the constant
and the shared helper.

The port deliberately reuses existing per-device pending
and DPC architecture instead of modeling the original
single global pending-state variable. Concurrent normal
queued DPC activity remains arbitrated by atomic snapshot/
clear. This is a source-backed behavioral mapping, not an
assertion of perfect concurrency equivalence for every
legacy interleaving.

## Safe next scope sequence

**Build-only first**, with XStream closed, the known-good
driver recovery available and an ordinary shell in the
scope repository directory:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }

& ".\scripts\build-driver.ps1" -BuildLecdiag
if (-not $?) { throw "New CFDC2400 build failed; STOP" }
```

Only if compilation succeeds, load using the established
**elevated** `scripts/build-sign-load-driver.ps1`
(signs SYS and CAT, installs updated PnP package and
restarts PCI). Stop on any build/sign/install/verification
error. With XStream still closed, exercise only the
**zero input mask**:

```powershell
& ".\tools\lecdiag\build\lecdiag.exe" raw-ioctl 0xCFDC2400 00000000 0
if ($LASTEXITCODE -ne 0) { throw "CFDC2400 zero-mask diagnostic failed; STOP" }
```

Expected summary (on success):

```text
IOCTL 0xCFDC2400 succeeded: input=4 output-capacity=0 returned=0
Output:
```

This confirms positive ABI acceptance and no crash during
the immediate empty/pending DPC processing. A nonzero
input can synthesize software completion/error/command
sources; **do not inject one** on the only known-good scope
just to test code coverage. An invalid two-byte input
(`0000`) should fail with `STATUS_INVALID_PARAMETER`
as expected, but is optional because the tool exits with
an error for intentional negative tests.

After the zero-mask check passes, start XStream and repeat
the established practical waveform/control/two-channel/
AP015 regression. Record any new startup, IRQ or event
behavior; no such post-CFDC2400 result exists yet.

## Evidence file list

- `ghidra_exports/selected/raw_11300.asm.txt`
- `ghidra_exports/selected/00013a2e_FUN_00013a2e.c`
- `ghidra_exports/selected/asm_asm_13a2e.txt`
- `ghidra_exports/selected/00012ede_FUN_00012ede.c`
- `ghidra_exports/selected/asm_asm_12ede.txt`
- `ghidra_exports/selected/raw_12ec2.asm.txt`
- `ghidra_exports/selected/dwords_dwords_1c62c_16.txt`
- `ghidra_exports/selected/dwords_dwords_1c500_16.txt`
- `ghidra_exports/selected/dwords_dwords_1c8bc_16.txt`
- `ghidra_exports/selected/xref_xref_1c62c.txt`
- `ghidra_exports/selected/xref_xref_1ce10.txt`
- `ghidra_exports/selected/00011390_FUN_00011390.c`
- `ghidra_exports/selected/raw_asm_104a0.asm.txt`
