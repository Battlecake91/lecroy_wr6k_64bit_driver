# CFDC2400: software interrupt-pending OR (2026-09-30)

## Scope and verification status

This is a **source-grounded analysis and native x64 source-only port**
of one original 2008 DeviceControl value. Original selected decompilation
and the existing raw instruction export already contain the critical
callback body, so a fresh Ghidra export is **not necessary** for the
basic semantics. The newly committed native code has **not** yet been
Windows-built, test-signed, installed or exercised by the owner.

The last owner-confirmed working XStream baseline remains the preceding
CFDC2194 / corrected CFDC2190 build, which passed idle status-query
format and a practical XStream regression with no observed malfunction.
This CFDC2400 patch is not part of that tested baseline yet.

## Exact original dispatch and input/output contract

Original `raw_11300.asm.txt`:

```asm
00011292 CMP EAX,0xCFDC2400
00011297 JZ  0x000112E9
...
000112E9 PUSH EDI
000112EA LEA  ECX,[ESI+0x1E0]
000112F0 CALL 0x00013A2E
```

`FUN_00013A2E` calls `FUN_00012EDE` and writes its returned NTSTATUS
to IRP `IoStatus.Status` at `IRP+0x18`. In the original helper,
the stack metadata's input length at `*(stack+0x60)+8` must be
**exactly four bytes**, and the input/system buffer at `stack+0x0C`
must be non-null. The control is METHOD_BUFFERED, one little-endian
32-bit DWORD. No output length requirement is evident in the selected
handler; success sets `IoStatus.Information=0` and returns
`STATUS_SUCCESS`. Invalid length or null input produces
`STATUS_INVALID_PARAMETER (0xC000000D)`. The request DWORD itself
is not range-checked or masked by the original.

## The previously unresolved callback is already exported

The existing `raw_12ec2.asm.txt` contains **all** the meaningful
instructions in original `LAB_00012EC2`:

```asm
00012EC2  CMP dword ptr [ESP+0x4],0
00012EC7  JZ 0x12ED8
00012EC9  MOV EAX,[0x0001CE1C]
00012ECE  OR  dword ptr [0x0001CE10],EAX
00012ED4  MOV AL,0x1
00012ED6  JMP 0x12EDA
00012ED8  XOR AL,AL
00012EDA  RET 0x4
```

The caller `FUN_00012EDE` copies the input DWORD to global
`DAT_0001CE1C` at original VA 0x12EF8, calls synchronization helper
`FUN_00010A88` and invokes its virtual callback dispatch with
`LAB_00012EC2` and the non-null hardware-subobject receiver. The
callback **ORs the full DWORD into global `DAT_0001CE10`**.
It is not a register write.

The same `DAT_0001CE10` receives enabled hardware interrupt bits
from original ISR `FUN_000108D6`; original individual DPC source
helpers `FUN_00011DC2`, `FUN_00011DD8`,
`FUN_00011DEE`, `FUN_00011E04`, `FUN_00011E1A`
and `FUN_00011E30` test and clear particular bits from that
global pending bitmap. The generic `FUN_00010636` interrupt
synchronization helper dispatches callbacks directly when no
interrupt object exists and uses `KeSynchronizeExecution` when
an interrupt object is connected.

After the synchronized OR, original `FUN_00012EDE` invokes the
active device-subobject vtable slot `+0x24` with `(0, 0)`.
The earlier vtable analysis already identifies its concrete target
as original `0x000104A0`, and existing `raw_104a0.asm.txt`
shows `XOR EAX,EAX; RET` at that address. Therefore this final
virtual call neither writes registers nor queues a synthetic DPC
in this driver build. Do not mistake it for a hardware operation
or replace it with a DPC insertion.

## Mapping to the existing native x64 architecture

`driver/Acquisition.c` already has a matching software pending
bitmap: `DevExt->InterruptPendingShadow`.

- x64 ISR atomically ORs accepted source bits into this field
  with `InterlockedOr`.
- x64 DPC atomically consumes its snapshot with
  `InterlockedExchange(..., 0)`.
- CFDC2400 can OR the caller's entire DWORD into **that same
  per-device field**, atomically. The legacy global-versus-native
  per-device lifetime difference is intentional; this driver binds
  each device instance independently.
- **Do not** filter injected bits through
  `InterruptEnableShadow`: the original software injection
  directly ORs the caller DWORD regardless of enabled hardware
  sources.
- **Do not** call `KeInsertQueueDpc`: the legacy subsequent
  virtual method is an explicit no-op. Like the original,
  this operation can remain pending until the usual subsequent
  DPC dispatch. Do not claim that an injected bit signals
  any event immediately.
- **Do not** add BAR reads/writes, interrupt-mask updates,
  transfer completion or AP015 emulation to this IOCTL.

The smallest implementation is a new `LECS65_IOCTL_CFDC2400`
constant in `driver/LecS65Drv.h` and one case in
`driver/Ioctl.c` that:

1. resets `information=0` and checks
   `systemBuffer != NULL && inputLength == sizeof(ULONG)`;
2. copies the DWORD via `RtlCopyMemory` (aligned-independent);
3. calls `InterlockedOr((volatile LONG*)&DevExt->InterruptPendingShadow,
   (LONG)mask)`;
4. returns `STATUS_SUCCESS` without MMIO or DPC insertion.

An input of zero is a pure positive-path ABI test that leaves
the pending bitmap unchanged. A nonzero input can synthesize
completion/event sources and must **not** be used casually on
the sole working scope, especially while XStream runs.

## Proposed first Windows test (not yet executed)

Only after a successful build and signed install, with XStream
**closed**, use the existing diagnostic's raw IOCTL mode:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }

& ".\scripts\build-driver.ps1" -BuildLecdiag
if (-not $?) { throw "Build failed; STOP" }
```

Use the established **elevated**, known-good
`scripts/build-sign-load-driver.ps1` procedure only if building
succeeds. Then, without starting XStream:

```powershell
& ".\tools\lecdiag\build\lecdiag.exe" raw-ioctl 0xCFDC2400 00000000 0
if ($LASTEXITCODE -ne 0) { throw "CFDC2400 zero-mask test failed; STOP" }
```

Expected successful diagnostic text includes:

```text
IOCTL 0xCFDC2400 succeeded: input=4 output-capacity=0 returned=0
Output:
```

The zero mask does not add software pending bits. An input of
two bytes (e.g. `0000`) should fail with invalid parameter, but
a failed `raw-ioctl` exits nonzero by design. Do not perform
a nonzero-mask functional test without identifying a safe,
isolated recipient of the synthesized event. After successful
positive-path testing, repeat only normal XStream waveform/control/
AP015 practical regression. The current source port and this
document are not evidence of such a test having occurred.

## Original source evidence

- `ghidra_exports/selected/raw_11300.asm.txt`: dispatch
  and receiver.
- `00013a2e_FUN_00013a2e.c`,
  `00012ede_FUN_00012ede.c`: exact input and IRP contract.
- `raw_12ec2.asm.txt`: callback and full pending OR.
- `00010636_FUN_00010636.c`: synchronized callback dispatch.
- `000108d6_FUN_000108d6.c`: original ISR accumulation.
- `00011dc2_FUN_00011dc2.c`, `00011dd8_FUN_00011dd8.c`
  and other `00011d*.c`: pending-bit consumers.
- `raw_104a0.asm.txt` and historical vtable analysis in
  `docs/ioctl-map.md`: final virtual-call no-op.
