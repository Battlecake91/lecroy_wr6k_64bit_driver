# LeCroy x64 driver: compact engineering handoff (2026-09-29)

This file describes the **latest actionable state**, not the historical investigation diary. Read [AGENTS.md](../AGENTS.md) first, then the [current handoff](next-chat-handoff.md), [TODO](TODO.md), and [README](../README.md). Repository and technical documentation are in English; converse with the owner in German.

**Public repository:** https://github.com/Battlecake91/lecroy_wr6k_64bit_driver (branch `main`).

## Current next step: grouped safe checks, one later XStream regression

Owner explicitly requests milestone-based XStream regressions,
not a full waveform/AP015 checklist after every tiny patch.
The real-PCI CFDC2400 zero-mask positive ABI already passed.
A new `scripts/test-safe-ioctl-batch.ps1` is committed
but NOT RUN. It performs nine low-impact checks (build
identity, PCI, START/FVER, CFDC2400 zero-input/output
bounds, CFDC2194 rejection of wrong output lengths)
without building/reloading or injected nonzero IRQ.
It refuses to run with XStream open.

On the scope, XStream closed:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Pull failed; STOP" }
.\scripts\test-safe-ioctl-batch.ps1
```

Expected 9/9 summary only after actual test. Optional
`-IncludeErrorStatus` consumes the error latch and
is deliberately omitted by default. Original missing
`0x0022303C` is a potentially unbounded indexed
hardware register write (+0x101 index, +0x106 data
in 266-byte record), `CFDC2130` performs one physical
BAR1 GPIODAT 0xE000-masked write per input byte.
Do not test either with fabricated payload. Full XStream
regression follows when a combined milestone is ready.
See [batch and static safety review](
safe-abi-batch-and-missing-ioctls-2026-09-30.md).

## Previous CFDC2400 milestone

## Newest result: CFDC2400 real-PCI zero-mask ABI PASS (2026-09-30)

The owner ran `lecdiag raw-ioctl 0xCFDC2400 00000000 0`
on the actual scope PCI device (1570:0005) and supplied:

```text
IOCTL 0xCFDC2400 succeeded: input=4 output-capacity=0 returned=0
Output:
```

The installed native case recognizes exactly four input bytes,
reports success, and returns no data. The unique device-instance
path is intentionally omitted here. A separate complete build/
sign/install transcript was not supplied with this specific test;
no nonzero pending IRQ bit or full XStream compatibility was
demonstrated. Zero-mask still triggers the original-style
immediate existing DPC processing, so it is not a passive query.

**Immediate next owner action:** start XStream on the current
installed CFDC2400 driver; run normal waveforms/controls/
two-channel 10 GS/s/AP015 preattached + unplug/replug +
jaw-warning regression. No further build or loading required
before this test. Avoid injecting a nonzero interrupt mask.
The preceding CFDC2194/CFDC2190 XStream baseline remains
the last practical XStream regression confirmed working.

## Historical prior CFDC2400 source-only checkpoint

## Latest actionable state: CFDC2400 source port awaits real-scope build (2026-09-30)

The owner's latest original-driver export
`b7b31c8bf5a06e9621a3636776673b486f72bfe5`
identifies derived subobject vtable `0x1C62C`,
slot `+0x24` at `0x1C650 -> 0x114F2`.
This thunk directly invokes the existing original DPC
dispatcher `FUN_00011390` (with `ECX-0x1E0`) and
returns `RET 0x8`, so the earlier base-vtable no-op
`0x104A0` hypothesis is conclusively superseded.

Complete original `CFDC2400`: validate exact 4-byte input,
OR its DWORD into software pending bitmap
`DAT_0001CE10` via synchronized `LAB_00012EC2`;
**immediately invoke DPC dispatcher**; return
STATUS_SUCCESS / Information=0. Native source now
contains `CFDC2400` case plus
`LecInjectLegacyPendingAndDispatch` helper that
atomically ORs the mask and synchronously calls the
existing x64 DPC at correct IRQL. No new MMIO/
artificial DPC queue. This is **NOT Windows-built,
signed/loaded or hardware-tested** yet. The last
owner-confirmed live-waveform/AP015 baseline is
still the earlier CFDC2194/CFDC2190 patch.
Source top-level coverage: 24/27, including one
gated case; three genuinely absent cases.

**Next on scope, XStream closed:**

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Pull failed; STOP" }
& ".\scripts\build-driver.ps1" -BuildLecdiag
if (-not $?) { throw "Build failed; STOP" }
```

Then only after successful build and known-good
recovery: elevated `scripts/build-sign-load-driver.ps1`;
with XStream still closed run
`tools\lecdiag\build\lecdiag.exe raw-ioctl 0xCFDC2400 00000000 0`.
Do NOT test nonzero injected interrupts on the sole
scope; do normal XStream/AP015 regression after zero.
Full rationale: [CFDC2400 investigation](
cfdc2400-software-pending-investigation.md).

## Historical CFDC2400 state before resolved vtable export (superseded)

## New current analysis: CFDC2400 requires correct derived vtable (2026-09-30)

Existing original `raw_12ec2.asm.txt` proves the
`CFDC2400` callback ORs the four-byte caller value into
global pending bitmap `DAT_0001CE10` under interrupt
synchronization. However the original dispatch passes
`main+0x1E0`, the hardware subobject with **derived vtable
0x1C62C**; final virtual slot `+0x24` is at `0x1C650`.
Older prose mapping that call to `0x104A0` no-op is
ABI-inconsistent (the call pushes two DWORDs, but
`0x104A0` is bare RET) and must not be relied upon.
A provisional CFDC2400 x64 case was reverted before
any Windows build. The last signed/verified CFDC2194
XStream baseline is unchanged, with 23/27 represented
top-level original codes.

New read-only Ghidra target `dwords:1c62c:16` plus
comparison vtables and ASM is committed. Next, the
owner runs on the *Ghidra PC*, **not scope**:

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
& ".\scripts\run-ghidra-analysis.ps1" -CommitMessage "analysis: resolve CFDC2400 derived-vtable slot"
```

Then inspect
`ghidra_exports/selected/dwords_dwords_1c62c_16.txt`
slot `+0x24`. All specifics:
[CFDC2400 investigation](cfdc2400-software-pending-investigation.md).

## Historical last scope result: post-CFDC2194 XStream regression PASS



**Owner-reported post-patch XStream regression PASS (2026-09-30):**
After the successful signed installation of the new CFDC2194
error-status ISR/read-and-clear path and correction of CFDC2190
ERRM programming, the owner completed the requested practical
XStream check and reported: *"Ich finde keine Fehlfunktionen."*
Thus no malfunction was observed in the exercised workflow,
following the previously documented waveform/control/two-channel/
AP015 regression checklist. This is an owner-reported practical
regression result, **not** a separately instrumented, item-by-item
capture or proof that every legacy feature has been tested.

Latest actual checkpoint: Windows x64 Debug build succeeded with
0 warnings/0 errors; x64 lecdiag built; SYS/CAT signed; PnP
installation `oem99.inf` and device restart succeeded;
driver build query 1002 and passive PCI query passed;
with XStream closed, `lecdiag error-status` returned
`0x00000000` and verified the exact 29-byte format; XStream
subsequently showed no owner-observed malfunctions.
**This is the current working owner-confirmed x64 baseline.**
Not exercised: a *nonzero* ERRS ISR latch, the persistent-error
bit-31 branch, concurrent status consumption and unobserved
acquisition forms. Do not manufacture hardware faults or disturb
the sole licensed Dallas device to force coverage.

## Previous hardware result: signed-load and idle CFDC2194 positive path PASS

**Owner-reported actual scope validation, 2026-09-30 00:23 local:**
The new CFDC2194/CFDC2190 source was built, signed, installed and queried
on the x64 LeCroy scope. The owner supplied the full
`.\\scripts\\build-sign-load-driver.ps1` transcript:
- MSBuild Debug|x64 succeeded with **0 warnings, 0 errors**; SYS output
  `x64\\Debug\\LecS65AcqDrv.sys`. The up-to-date inner build
  printed `ClCompile: All outputs are up-to-date`, which is expected
  after the preceding source build.
- `tools\\lecdiag\\build\\lecdiag.exe` built, x64 PE machine 0x8664.
- Test-signed SYS and CAT; Inf2Cat signability: no errors/warnings.
- PnP installation succeeded as `oem99.inf` on
  `PCI\\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00`;
  PnP device restart succeeded.
- Newly opened PCI device interface returned driver build
  **1002** (Information=4). Passive PCI read returned
  vendor/device 1570:0005, BDF 4:1.0, memory space and bus master
  enabled, IRQ line 19/pin 1.
- With XStream closed, new `lecdiag error-status` returned
  `0xCFDC2194 error status (read/clear): 0x00000000`
  followed by `PASS: original 29-byte response layout verified.`
  This confirms real-hardware acceptance and format of the
  exact 29-byte CFDC2194 reply, not a nonzero ISR error latch,
  bit-31 persistent-error branch or full XStream compatibility.
- `DriverVer=09/29/2026,0.2026.930.23` was generated on
  September 30 local because the script uses UTC for DriverVer
  date, while local clock supplies version components.
- **Historical next step, subsequently completed with no owner-observed malfunction:** run XStream with the newly installed
  driver and check live waveforms, ordinary controls, two-channel/
  10-GS/s where practical, AP015 preattached identification,
  physical unplug/replug, jaw warning and any new error IRQ storm.
  Do not run `lecdiag error-status` while XStream is consuming
  the same sticky latch, and do not artificially trigger hardware
  faults or access licensed Dallas memory destructively.

## Historical source-recovery status (subsequently built and installed)

## 2026-09-30 update: original CFDC2194 producer SOLVED; source-only port

The owner's PC Ghidra rerun committed fresh exports in
`540bf87c374d3ea256896c49fdecbeacca8592da`.
Original `FUN_000108D6` (ISR) **writes** the status consumed
by `CFDC2194`: the newly found `field_0x134a.refs.txt`
records `0x10958 OR DWORD [main+0x134A]` (raw BAR0 ERRS).
`field_0x134d.refs.txt` records
`0x10A67 OR BYTE [main+0x134D],0x80` (persistent-error bit 31).
The original dispatcher passes `main+0x1E0` to
`FUN_00012BAE`, therefore its `this+0x116A` is exactly
the same DWORD. The old `field:116a` scan missed the
main-object coordinate; this issue is now **resolved**.

New x64 **source-only** changes are committed on main:
`driver/Acquisition.c` latches/acknowledges the ERRS IRQ
and persistent error bit, `driver/LecS65Drv.h` and
`Driver.c` hold the software latch/ERRM cache,
`driver/Ioctl.c` implements the 29-byte CFDC2194 reply
and corrects CFDC2190's type/enable/ERRM inversion,
and `tools/lecdiag/lecdiag.c` adds `error-status`.

**No successful Windows build, signed reload or XStream
regression on this NEW source has yet been reported.**
The owner-confirmed waveform/AP015 baseline following the
earlier START/FVER fix is still the last verified hardware
state. The new physical ERRM inversion can change interrupt
behavior, so build and test carefully before calling it
verified. Do not inject fake errors or touch Dallas.

### Windows build feedback (2026-09-30, owner excerpt)

The owner supplied the following two MSVC LINK warnings from the
new source build (no compile error appeared in the supplied excerpt):

- `LNK4075`: `/INCREMENTAL` ignored due to `/RELEASE`.
- `LNK4075`: `/EDITANDCONTINUE` ignored due to `/DRIVER`.

Both are option-conflict/precedence warnings, not driver source
errors and not reasons to alter validated IRQ/DMA logic. A final
`0 errors` build summary and both actual output files still need
to be checked before treating the latest source as built
successfully. **No signed reload, `lecdiag error-status`
output, or post-change XStream regression was supplied yet.**
The existing `build-sign-load-driver.ps1` rebuilds driver
and lecdiag, signs SYS and CAT, updates PnP, and checks the
newly loaded driver; it **must run elevated** and may restart
the real PCI device.

### Immediate next command (scope, elevated PowerShell, XStream closed)

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }

& ".\scripts\build-driver.ps1" -BuildLecdiag
if (-not $?) { throw "Windows build failed; STOP" }
```

Only if build succeeds and the known-good driver is recoverable:
run the established signed `build-sign-load-driver.ps1` as
administrator. Then, **before starting XStream**:

```powershell
& ".\tools\lecdiag\build\lecdiag.exe" error-status
if ($LASTEXITCODE -ne 0) { throw "CFDC2194 diagnostic failed; STOP" }
```

The response must be 29 bytes, DWORD type 2 at +4 and
software-latched status DWORD at +8; the remaining bytes
are zero. A status of zero on an idle device is acceptable.
This query **clears/consumes** the latch; never race it with
XStream. Retest normal waveforms, controls and AP015 afterward.
For producer proof and detailed validation:
[CFDC2194 source investigation](
cfdc2194-status-latch-investigation.md).

## Historical 2026-09-29 validated baseline and investigation notes

## Latest actual scope result: read-only 0x00223044 PASS (2026-09-29 late)

The owner ran `tools/lecdiag/build/lecdiag.exe start-register` on the
real x64 scope and supplied this output (non-sensitive):

```text
Opened device interface: \\?\pci#ven_1570&dev_0005&subsys_00000000&rev_00#...#{7ac34be9-f766-4f15-9e88-854ba5e2146e}
0x00223044 START/FVER: 0x00000002
BAR0+0x000 reference: 0x00000002
PASS: legacy 4-byte output matches physical register read.
```

**Verified:** the newly implemented legacy 4-byte output request
`0x00223044` is accepted by the driver currently serving the real PCI
device. Its returned DWORD (`0x00000002`) equals the result of the
separate generic register-read IOCTL for physical BAR0+0x000,
also `0x00000002`. This is read-only, with no BAR write or Dallas access.
It verifies this request's live successful path and consistency with the
generic BAR0 read, not every edge case or a separate original-x86
side-by-side measurement. The user's current message does not include
the complete pure-build/signed-install log or the scope's Git HEAD;
do not infer those individual steps or claim a completed XStream
regression from the successful diagnostic alone.

**Prior blocked attempt, now superseded for this IOCTL:** the first
`scripts/build-sign-load-driver.ps1` invocation used a non-elevated
PowerShell and stopped at its administrator assertion, without installing
a driver. The subsequent diagnostic failed its first new IOCTL with
Win32 `ERROR_INVALID_FUNCTION` (1) before any reference comparison.
The new PASS establishes that this earlier error is not the current
result. The precise corrective installation commands used were not
included in the latest console excerpt. When running PowerShell scripts,
check immediate `$?` or use `try/catch`; a later `$LASTEXITCODE`
does not reliably catch a PowerShell `throw`.

### Follow-up XStream / AP015 regression: owner reports PASS

After `start-register` passed, the owner was asked to check genuine
waveforms, ordinary vertical/timebase/coupling/bandwidth/trigger
operations, 2-channel/10-GS/s where practical, preconnected AP015,
physical removal/reinsertion and the known open-jaw warning.
Reply: `Ja klappt soweit alles.` Record no observed regression in
the exercised existing baseline, not an automated exhaustive
per-feature acceptance matrix. The immediate `0x00223044`
hardware + XStream check is now complete.

### 2026-09-30 source advance (not a completed Ghidra rerun)

Original `FUN_000115C4` initializes the hardware subobject at
`main+0x1E0`. The START/FVER register member `this+0x138`
is initialized/read on that subobject by `FUN_00014847` and
`FUN_00012D24`. Thus the `CFDC2194` latch
`this+0x116A` could also be accessed as `main+0x134A`
if the same original dispatcher receiver is used. Its four
bytes span `116A..116D` or `134A..134D`. Added
dispatch-wrapper, overlapping field and callback export
targets in `ghidra_scripts/targets.txt` to search both
representations. Exact scans do not prove the absence of
indirect writers. Full analysis:
[CFDC2194 provenance](cfdc2194-status-latch-investigation.md).
The Ghidra PC rerun is still required; no kernel patch or
new hardware test performed.

### Next task: map original CFDC2194 latch producer

`FUN_00012BAE` returns exactly 29 bytes, including the saved
status DWORD from `this+0x116A` at +8, then clears the latch.
The direct field scan only found that read-and-clear site.
Paired original `FUN_00013A40` (CFDC2190) calls
`FUN_000107FE` on `this+0x188` and registers callback
`FUN_00012EAE` which can call `FUN_00011E46` for a
global register write. This adjacent path **does not prove**
the origin of `this+0x116A`. Inspect aliases/indirect writes,
callback/IRQ and initialization before any kernel implementation.
A constant-zero successful response is not a substitute.

Do not disturb working PCI/IRQ/DMA/AP015, or resume deferred
virtual Dallas, hardware isolation or licensed-chip writing.

## Source facts just recovered from the user's Ghidra push

New original x86 selected exports:

- `ghidra_exports/selected/asm_asm_12d24.txt`
- `ghidra_exports/selected/asm_asm_12bae.txt`
- `ghidra_exports/selected/field_0x138.refs.txt`
- `ghidra_exports/selected/field_0x116a.refs.txt`
- Already available corresponding C:
  `00012d24_FUN_00012d24.c`,
  `00012bae_FUN_00012bae.c`,
  and initializer `00014847_FUN_00014847.c`.

**Newly implemented x64 `0x00223044`:**
original `FUN_00012D24` checks only exact output
length four, calls `READ_REGISTER_ULONG` on
the register pointer held in main object `+0x138`,
returns a DWORD with `Information=4`.
Original `FUN_00014847` initializes that pointer to
`BAR0 base + 0x000`, the START/FVER register location.
Replacement code now defines
`LECS65_IOCTL_READ_START_REGISTER` in
`driver/LecS65Drv.h`, dispatches it in
`driver/Ioctl.c`, resolves BAR0 offset zero,
performs `READ_REGISTER_ULONG`, returns the
actual DWORD. It is a read-only change.

**`0xCFDC2194` is deliberately NOT implemented:**
original `FUN_00012BAE` requires exact 29-byte
output; zero-fills that buffer, places DWORD `2`
at response offset 4, copies the software status
latch at original `this+0x116A` to offset 8,
then clears the original latch. The new displacement
scan `field_0x116a.refs.txt` finds only
the handler's own `LEA [ESI+0x116A]` read/clear
access, not an identified writer. An indirect or
aliased writer is possible. Map where/when the
status latch becomes nonzero before implementing
`CFDC2194`; **never fabricate a permanently zero
success response** to inflate coverage.

Recent relevant source additions:
- `driver/LecS65Drv.h`: IOCTL definition `0x00223044`.
- `driver/Ioctl.c`: read-only dispatcher case.
- `tools/lecdiag/lecdiag.c`: `start-register` command and equality test.
- `docs/ioctl-map.md`: original reference and pending build/hardware validation.
- `docs/TODO.md`: remaining work.

The assistant did not build or run Windows binaries in its own environment.
The owner has now supplied the successful real-scope `start-register`
read comparison above. Only the XStream/AP015 post-change regression
remains unreported; do not treat it as passed.

## Stable verified baseline / boundaries

- Native x64 driver currently starts XStream and displays genuine
  waveforms on the real WaveRunner scope.
- Observed one-channel acquisition and family-1 transfer paths,
  DMA descriptor handling, legacy interrupts/DPC and event delivery
  have been hardware-tested. Do not casually change them.
- Normal timebase, vertical scale, coupling, bandwidth,
  trigger, two-channel/10-GS/s selection were tested.
- The AP015 identifies correctly, physical hotplug works and
  XStream displays the unlocked-jaw warning. All five front
  physical ProBus sockets are I2C for probe communications;
  PCI-board Dallas 1-Wire EEPROM is a separate device.
- U11 DS2433 has factory 64-bit ROM ID and separate writable
  512-byte EEPROM holding XStream license data. The user
  privately saved two byte-identical 512-byte backups and
  later separately saved the complete original eight-byte
  ROM identity. **No private data is present in this public
  handoff**. Current x64 ID/read/backup works; physical
  WRITE_DALLAS_MEMORY `0x00223088` is not yet implemented.
- A genuine XStream license-delete attempt sent the complete
  512-byte write to missing `0x00223088`, returned
  `0xC0000010 STATUS_INVALID_DEVICE_REQUEST`; no physical
  EEPROM write resulted from that attempt. The original
  32-byte DS2433 scratchpad/write/verify/copy and full
  readback code is recovered in Ghidra exports `11f54`,
  `16d90`, `16f2c` but has **not** been ported/tested
  on spare hardware yet. Do not experiment on the sole
  licensed card.
- The principal six-hex-digit displayed scope-ID component
  corresponds to factory Dallas ROM serial bytes 1..3
  (24-bit little-endian); two-digit suffix unresolved.
  Replacement DS2433 would have a new immutable ROM identity.
  Whether every actual license binds to this ID is unproven.
- User deliberately **DEFERRED** virtual Dallas ROM/EEPROM
  emulation and physical
  chip isolation tests. These remain in [TODO](TODO.md) and
  [Dallas recovery design](dallas-device-manager-recovery-design.md),
  NOT the next action.
- Any original or older replacement startup JSONL trace may
  expose unique Dallas ID and real license memory previews.
  Keep raw traces and backups private. Rebuilt user-mode
  `lecdiag` now suppresses direct Dallas hex data from
  newly generated JSONL; that does NOT sanitize old captures
  or arbitrary kernel debug streams.

## Two-machine layout

- PC with Ghidra and engineering checkout:
  `C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver`.
  Established export command:
  `& ".\scripts\run-ghidra-analysis.ps1" -CommitMessage "analysis: ..."`.
  The needed `12d24`/`12bae` and field scans are already
  **pushed**; do not ask the owner to repeat the same export.
- Real x64 scope, XStream and PCI device:
  `C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`.
  This is where driver build/sign/load and the new
  `lecdiag start-register` hardware comparison must run.
- Original x86 installation is available as a separate boot
  setup for comparing legacy-driver behavior, not needed for
  the immediate read-only test.

## Documentation hygiene

The owner's explicit request: README must describe the latest
**current, verified** state only, NOT chronological mistakes,
old milestones, superseded hypotheses or a per-trace diary.
Keep detailed evidence in `docs/`; keep `AGENTS.md`
and `docs/next-chat-handoff.md` current as the source evolves.
No real license keys, actual full ROM identifiers, raw EEPROM
images or private trace byte previews in public GitHub.
