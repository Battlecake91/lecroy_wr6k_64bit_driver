# Grouped low-impact ABI validation and remaining write-only controls

Date: 2026-09-30. Scope: existing native x64 WR6k driver,
the newly owner-tested `CFDC2400` zero-mask positive path,
and the original x86 static exports.

## Test policy: one practical XStream regression per milestone

The owner explicitly prefers collecting multiple compatible
source changes and diagnostic checks before spending time
on a full XStream waveform/AP015 regression. **Do not demand
a complete XStream regression after every small IOCTL patch.**
Run focused low-impact ABI checks first, inspect their outcomes
and perform one practical XStream regression when a useful
development batch is ready. Keep each source change's
test status precise; deferring regression does not imply PASS.

The newest owner result remains:
`lecdiag raw-ioctl 0xCFDC2400 00000000 0` successfully
opened the real PCI 1570:0005 device and returned
`input=4 output-capacity=0 returned=0`, with empty output.
No owner-reported post-CFDC2400 XStream regression exists yet.

## Owner-reported grouped ABI run: 9/9 PASS (2026-09-30)

After the original nine-case script was committed, the owner ran it
on the x64 scope and supplied its terminal summary:

```text
SAFE ABI BATCH: 9/9 passed; 0 failed.
All requested safe ABI checks passed.
```

The shared excerpt also contained a Windows PowerShell 5.1
`NativeCommandError` referring to the old script line
`$combined = & $diag @Command 2>&1 | Out-String`.
That line merges a native program's stderr into PowerShell's
pipeline. Deliberately invalid `DeviceIoControl` buffer-size
tests cause `lecdiag` to write an EXPECTED error message to
stderr, which PowerShell 5.1 then renders as an additional
red `NativeCommandError` / `RemoteException`. The script
nevertheless evaluated the actual expected process exit and
message and reported all nine checks as PASS. This is a
diagnostic harness presentation issue, **not an observed
kernel IOCTL failure**.

The script has since been updated to use `Start-Process`
with separate temporary `-RedirectStandardOutput` and
`-RedirectStandardError` capture, `-Wait -PassThru`,
the actual `ExitCode`, system-native text decoding, and
`try/finally` temporary-file cleanup. This avoids exposing
expected stderr as PowerShell ErrorRecords while preserving
the same test cases and strict outcome matching.

**The corrected harness has NOT YET been rerun** on the
scope; the original harness's terminal 9/9 PASS is the
owner-reported result. No driver source, `lecdiag.c`,
signing or kernel binary was changed to fix the cosmetic
PowerShell behavior. The complete XStream/AP015 regression
remains intentionally deferred to the next combined milestone,
as requested by the owner.

## Original batch instructions (test completed, harness updated)

## Prepared single-command scope test (script committed, not yet executed)

Run with XStream **closed** on the x64 scope from the repo checkout:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }

.\scripts\test-safe-ioctl-batch.ps1
```

This script needs only the already built
`tools\lecdiag\build\lecdiag.exe`. It explicitly
checks for a running `XStream` process and refuses to
run if it is present. It does **not** build, sign,
install or reload the driver.

The default nine independent checks are:

| Check | Expected result | Kernel/hardware effect |
|---|---|---|
| `lecdiag build` | original build 1002, returned 4 bytes | build query |
| `lecdiag pci` | physical PCI vendor/device `1570:0005` | passive PCI config query |
| `lecdiag start-register` | native 4-byte output equals generic BAR0+0 read | passive MMIO register reads |
| `CFDC2400` input `00000000`, output capacity 0 | success; returned 0 | software OR with zero, immediate existing DPC processing |
| `CFDC2400` same zero input, output capacity 4 | success; returned 0 | same, validates output-independent ABI |
| `CFDC2400` input `000000` (3 bytes) | reject with Win32 87 / `STATUS_INVALID_PARAMETER` | fails before pending OR/DPC |
| `CFDC2400` input `0000000000` (5 bytes) | same rejection | fails before pending OR/DPC |
| `CFDC2194` output capacity 28 (dummy one-byte input) | rejected; no latch consumption | returns before read-and-clear |
| `CFDC2194` output capacity 30 (dummy one-byte input) | rejected; no latch consumption | returns before read-and-clear |

These malformed-size tests are **expected failures of
DeviceIoControl**; the script reports them as PASS only if
the expected negative result is observed. It prints a final
`SAFE ABI BATCH: 9/9 passed; 0 failed.` summary on a
fully successful run and throws otherwise.

An additional explicit `-IncludeErrorStatus` parameter
executes a valid `lecdiag error-status` at the end
(10 total tests). That optional success path **consumes
the software status latch** and is intentionally omitted
from the default suite. The owner already verified it
separately on the prior CFDC2194 patch.

The batch injects **no nonzero pending bit**,
does not call any Dallas write IOCTL,
does not program BAR1 GPIODAT,
and does not issue arbitrary indexed register writes.
Even a zero-mask `CFDC2400` call invokes the original-style
existing DPC dispatch; do not call it strictly passive.

**Historical pre-run checkpoint:** At the time this section was first written, no batch result was available. The subsequent owner-reported 9/9 result and PowerShell 5.1 stderr-capture correction are documented at the top.

## Static review of the remaining x86 handlers

The original dispatch inventory has 27 unique top-level IOCTL
values, 24 now represented in native x64 source (one gated).
The three absent ones are `0x0022303C`,
`0x00223088` (licensed Dallas WRITE, deliberately deferred)
and `0xCFDC2130`.

### 0x0022303C: SetOneRegister, original 266-byte record

Source:
`ghidra_exports/selected/00012cac_FUN_00012cac.c`,
`0001259a_FUN_0001259a.c`,
`000107fe_FUN_000107fe.c`,
`00013fa6_FUN_00013fa6.c`.

The original wrapper `FUN_00012CAC` initially sets
`STATUS_NOT_IMPLEMENTED (0xC0000002)` and checks its
global state gate `DAT_0001CD08 == 0`. Otherwise it
returns `STATUS_DEVICE_NOT_READY (0xC00000A3)`.
When available, it requires input length
**exactly 0x10A (266) bytes**; a different size returns
`STATUS_INVALID_BUFFER_SIZE (0xC0000206)`.
On the matching-size path, it copies all 266 bytes
into a local stack record, calls `FUN_0001259A`,
and reports `STATUS_SUCCESS, Information=0`.

`FUN_0001259A` reads a DWORD **index** from
record offset `+0x101` and a DWORD **value** from
`+0x106`. It looks up register-wrapper pointer
`table[index]` through register-list object `this+0x10`,
then calls `FUN_000107FE` on that selected wrapper.
The latter updates its software cached word
(`wrapper+0x24`) **and writes a physical MMIO DWORD**
through the wrapper's register pointer.

**A bounds/safety check is not visible in the exported
`FUN_0001259A` body.** This is a variable-index
hardware-register WRITE, not a harmless trace control.
Do not fake success or send a fabricated 266-byte request
to the working scope. Before a native port, establish the
actual register-list size, allowed indices,
index-to-BAR/register mapping and software gate semantics,
and harden against negative/out-of-range caller values
even if the old x86 binary did not.

### 0xCFDC2130: serial-trigger FPGA programming via GPIODAT

Sources:
`ghidra_exports/selected/00011cff_FUN_00011cff.c`,
`00014847_FUN_00014847.c`,
`000107fe_FUN_000107fe.c`.

The original named handler
`IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA_Handler`
requires a non-null METHOD_BUFFERED input pointer.
A null pointer returns `STATUS_INVALID_PARAMETER
(0xC000000D)`, and zero length returns
`STATUS_INVALID_BUFFER_SIZE (0xC0000206)`.
For a nonempty byte stream, it first reads the
original hardware-subobject `this+0x318` register
wrapper. Construction in `FUN_00014847` identifies
this as **BAR1 GPIODAT at offset 0x0C4**.

Each input byte triggers a masked register update,
which can be rewritten exactly as:

```c
uVar3 = (uVar3 & ~0xE000u)
      | ((((uint32_t)inputByte) << 8) & 0xE000u);
WRITE_REGISTER_ULONG(BAR1_GPIODAT, uVar3);
```

This selects the incoming byte's bits 7:5 and places
them in the GPIODAT register's bits 15:13, preserving
other bits. It repeats once per byte, including
successive physical writes and cached-wrapper updates
through `FUN_000107FE`. The exported helper contains
no separate maximum request length; the IRP input
length controls the loop.

The actual protocol depends on the ORDER and timing
of these writes, so collapsing the stream to its
last byte or treating this as a generic register
read/write would be incorrect. A future native port
must account for GPIODAT ownership and other users,
recovered physical pin meanings, stream/timing bounds
and safe isolation. **Do not test this handler with
a nonempty payload on the working scope merely to
raise an IOCTL implementation count.**

### 0x00223088: Dallas EEPROM write

Already separately documented and intentionally
deferred until a disposable DS2433 is available.
No synthetic write or destructive license testing
belongs in this compatibility batch.

## Recommended continuation

1. Obtain and review the nine-case batch output without
   interrupting the current working XStream baseline.
2. Continue original x86 register-table and
   FPGA-configuration *static* analysis where useful;
   only source-port actions whose MMIO effects can
   be strictly bounded and isolated.
3. Accumulate enough compatible changes before performing
   the one deferred XStream waveform/control/AP015
   regression. Record this as one milestone rather
   than assigning nonexistent results to each patch.
