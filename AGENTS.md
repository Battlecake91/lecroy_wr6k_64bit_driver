# AGENTS.md

This file is the persistent hand-off and operating guide for this repository.
Every agent/chat working on this project should read it first and keep it current.

**Current new-chat starting point:** [`docs/next-chat-handoff.md`](docs/next-chat-handoff.md).

**LECWATCH LIVE IOCTL MONITOR IMPLEMENTED (2026-09-30, NOT YET RUNTIME-VERIFIED):**
Native x64 `tools/lecwatch/lecwatch.c` now reads only the existing private
`DEBUG_GET_TRACE` ring on a 100-ms worker loop. It provides recent-activity
indicators, rate/count/status, live log, confidence/CFDC2110 decoding, Hide known,
5-s idle baseline + request-shape novelty, filters, QPC markers, Start/End Action
summaries, payload details, sequence-gap accounting and redacted JSONL export.
Dallas payloads are never displayed/saved. No arbitrary legacy IOCTL/MMIO/FPGA/
Dallas-write path exists in the GUI. Build with `scripts/build-lecwatch.ps1`;
no driver reload is required. **Source/static review only so far:** do not claim
MSVC build or real-scope PASS until owner output is supplied. See
`tools/lecwatch/README.md` and `docs/live-ioctl-monitor-design.md`.


**LONG-TERM RELEASE GOAL (owner decision, 2026-09-30):** this project is explicitly
intended to end with a **WHQL/WHCP-certified Microsoft-signed Windows x64 production
driver**, not merely a working test-signed replacement. Treat local HLK readiness as
a required release milestone before spending money on the EV certificate. The final
target is normal installation on supported Windows systems without test-signing mode.
Do not describe WHQL/WHCP certification as already achieved. See README project goals,
`docs/TODO.md`, and `docs/driver-signing-and-funding.md`.


**DRY REGRESSION VERIFIED (owner, 2026-09-30):** after the PowerShell 5.1
hex-literal fix in commit `95be1ba4ebaf229b58d115fca3b5b9e12d0a267b`, the owner
reran `scripts/test-driver.ps1 -Mode Dry`. Driver build completed with
**0 warnings / 0 errors**, `lecdiag` built as x64 PE machine 0x8664, and the
hardware-independent contracts finished **8/8 PASS, 0 failed** with final
`REGRESSION SUITE PASS: Dry`. Dry mode is now runtime-verified on the Windows
development machine. The separate real-PCI safe ABI baseline remains 9/9 PASS.

**XSTREAM E2E IMPLEMENTED (2026-09-30, NOT YET RUNTIME-VERIFIED):**
`tests/xstream/test-xstream-e2e.ps1` is integrated via `test-driver.ps1 -Mode XStream`.
Current checks: COM activation, C1/Horizontal/result access, forced acquisition,
waveform Samples/DataArray, reversible VerScale/HorScale/Coupling/BandwidthLimit.
Optional probe name, amplitude and frequency assertions are supported. `-Mode All`
now means Dry -> Hardware -> XStream. Do not claim XStream PASS until owner output.
Need exact legacy XStream Browser paths before adding explicit trigger, two-channel /
10-GS/s, and AP015 jaw/hotplug assertions.

**HARDWARE REGRESSION VERIFIED (owner, 2026-09-30):**
`scripts/test-driver.ps1 -Mode Hardware` completed **11/11 PASS, 0 failed** on
the real WR6k PCI device with XStream closed. Passive baseline: stats v1/build
1002/unknown IOCTLs 0; logical BAR lengths 0x200, 0x40000, 0x200; PCI 1570:0005
at BDF 4:1.0, command 0x0006, IRQ line 19 pin 1; START/FVER 0x00000002 matching
BAR0+0. CFDC2400 zero-mask and malformed-length checks plus CFDC2194 malformed
output checks all behaved as expected. No hazardous indexed-register, Dallas-write,
serial-trigger-FPGA or nonzero IRQ-injection path was exercised.

**NEW REGRESSION HARNESS (2026-09-30, SOURCE-SIDE ONLY):**
`scripts/test-driver.ps1` is now the unified test entry point with
`-Mode Dry|Hardware|All`. Dry builds driver+lecdiag by default and then runs
`tests/dry/test-source-contracts.ps1` without device access; `-SkipBuild`
performs only the fast source/ABI contracts. Hardware delegates to the existing
safe 9-check PCI ABI batch. XStream E2E is deliberately the next layer, not yet
implemented in the runner. The dry contracts freeze build 1002, selected IOCTL
numeric values/dispatch references, public ABI size guards, debug-control
separation, and continued native absence of hazardous original writers
0x0022303C, 0x00223088 and 0xCFDC2130. See
`docs/regression-testing.md`. **Do not claim the new Dry/All runner passed:** it
has not yet been executed on a Windows WDK machine. The separately owner-reported
hardware batch remains 9/9 PASS.

**CURRENT AUTHORITATIVE ORIGINAL-DRIVER ANALYSIS (2026-09-30,
owner Ghidra commit `00eb49db5efe98042df47ba07a570017cd37419d`):**
THIRD read-only Ghidra batch has now COMPLETED and
its findings are integrated in
[original 43-register list and writer ABI](
docs/original-register-list-and-write-abi.md).
Do NOT re-request already completed Ghidra batches.
Original hardware-subobject `CKeRegisterList` at
`+0x11EE` owns TWO independent dynamic arrays:
wrapper pointer table (at object +0x10,
capacity+0, grow step+4=1, lastIndex+0x0C=-1)
and 266-byte-record array (second array object
begins at list+0x1C, record buffer ptr list+0x2C).
`FUN_00012184` grows the pointer array,
copying old pointers/freeing previous allocation;
`FUN_000121FA` does the same with 0x10A-byte
records. Both return 0xC000009A on allocation failure.
The second record append's failure is NOT separately
checked by `FUN_00013FA6` before it sets its
local success byte (potential but UNOBSERVED mismatch).

The original common list receiver to
`FUN_000159E2 -> FUN_0001785B` is the SAME
hardware-subobject+0x11EE list, confirmed by
the 14847 original ASM argument stack.
Registration order: six transport entries 0..5,
15 common entries 6..20, then 22 entries 21..42
if START/ITMODE setup `FUN_00012FDE` succeeds.
The successful steady-state original list intends
43 entries, exactly matching the EXISTING native
`g_LecLegacyRegisterList[43]` ordering.
The original list-required-size helper computes
`(recordLastIndex+1)*266` = 11,438 bytes for 43.

**NEW CRITICAL RECORD-ABI DISCOVERY:** original list
metadata record offset `+0x101..+0x104` contains
the PHYSICAL REGISTER OFFSET (from wrapper+0x1C),
with BAR id byte at +0x100, type byte at +0x105,
readback DWORD at +0x106. Existing x64
`LecFillLegacyRegisterEntry` already implements
this layout correctly. But original missing setter
`0x0022303C` -> `FUN_0001259A` interprets the
INCOMING DWORD at `+0x101` as the **ARRAY INDEX**
and dereferences `pointerTable[index]` WITHOUT
a local range check before `FUN_000107FE`
physically writes requested data at +0x106.
For example GPIODAT's list offset is 0xC4,
yet its full-init array index is 42. Do NOT
reuse a list-query 266-byte record unchanged as
a setter input; reject all unknown/unvalidated
write inputs. The wrapper `FUN_00012CAC`
only checks `DAT_0001CD08==0` and exact
266-byte length, then reports success.

**GPIODAT SHARED WRITER:** original `CFDC2130`
`FUN_00011CFF` reads BAR1+0xC4 once and
per input byte updates bits 15:13 using 0xE000,
writing a full DWORD every time.
`FUN_000120DC` reads the SAME wrapper +0x318,
clears bit 16 via `& 0xFFFEFFFF`, then writes
it; original xrefs prove callers
`FUN_00012D6A` and `FUN_00012F30` invoke it
before regular transfer paths. The serial stream
therefore must NOT be implemented blindly during
active acquisition; no original loop delay is
visible, and original pin meanings/synchronization
remain unresolved. Other generic register-list
writers can access same pointer without literal
field+0x318 references.

**Decision after this analysis:** NO new x64 driver
source/installation changes; original coverage
remains 24/27 represented (one gated), with
0x0022303C indexed hardware write,
0xCFDC2130 serial FPGA programming and
0x00223088 licensed Dallas WRITE all ABSENT.
The last owner-reported low-impact on-scope ABI
batch is 9/9 PASS; no comprehensive post-CFDC2400
XStream/AP015 regression has yet been reported.
The owner explicitly prefers a SINGLE practical
regression after a meaningful combined milestone.
Further work should prioritize real original
XStream user-mode evidence for 0x0022303C
setter-record construction or GPIO ownership,
not invent a write test on the only working scope.
Do not claim that 43/43 register-map understanding
implies full x86 source-byte or functional parity.

**Prior third-export pending instructions below are superseded.**

**LATEST static Ghidra checkpoint (2026-09-30,
owner commit `92f8a7f67c751c764190206f79cd4bf9ec493753`):**
Second PC-only writer batch has been completed.
`CKeRegisterList` at hardware-subobject +0x11EE
contains a pointer array and a separate 0x10A-byte
record array. Constructor `FUN_00013434` sets
pointer capacity=0, growth quantum=1 and
lastIndex=-1. `FUN_00013F3C -> FUN_0001326E ->
FUN_00012184` appends/grows the pointer array;
`FUN_00013F70 -> FUN_00013230 -> FUN_000121FA`
appends/grows the per-register 266-byte record
array. The original `FUN_0001259A` writer
itself still has NO evident bounds check before
`pointerArray[index]` (index from caller record
+0x101; value +0x106) and immediate physical
`FUN_000107FE` write. Do NOT port raw original
index semantics without explicit validation.

`FUN_00014847` contains 15 common and 22
conditional `FUN_00013FA6` insertion calls;
`FUN_0001785B` adds six calls via
`FUN_000159E2`, but table receiver identity
must be established before assigning indices.
`FUN_000120DC` reads/bar-writes the same
BAR1 GPIODAT wrapper +0x318 (phys +0xC4)
and clears bit 16, distinct from `CFDC2130`
stream's 0xE000 bits 15:13. These are concrete
hardware writers, not arbitrary safe test IOCTLs.

**THIRD read-only Ghidra target batch (23 new
targets) is ALREADY in
`ghidra_scripts/targets.txt`; NOT YET RUN.**
It asks for `FUN_00012184` growth logic,
`FUN_00012166` free, `159E2 -> 1785B`
conditional register insertion, `120DC`
and additional GPIODAT writer traces,
ASM/XREFs and object fields. NEXT on separate
Ghidra PC (not on the scope):

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
.\scripts\run-ghidra-analysis.ps1 -CommitMessage "analysis: resolve register-list growth and shared GPIODAT writers"
```

No kernel driver source changes, no driver install
and NO full XStream regression requested now.
Prior real-scope low-impact ABI suite 9/9 PASS;
one combined practical XStream/AP015 regression
is still deferred. See
[second-batch proof and third-batch request](
docs/safe-abi-batch-and-missing-ioctls-2026-09-30.md).

**Earlier static export checkpoint (historical):**

**Newest static export checkpoint (2026-09-30):**
Owner already pushed first write-path Ghidra export as
`2071cfa7d44199d7e0cde3a7e3cc77e209ffc414`.
Its `asm_asm_1259a.txt` confirms the 266-byte
`0x0022303C` request supplies raw index DWORD
`record+0x101` into unguarded
`[registerTable+index*4]`, and value DWORD
`record+0x106` flows into direct physical write
`FUN_000107FE`. The register-list object is
`hardware-subobject+0x11EE`, constructed in
`FUN_00014212` via `FUN_00013434`;
`FUN_00014847` contains 37 source-level
`FUN_00013FA6` insertion CALL SITES, NOT a
verified runtime capacity/length. Do not port an
unbounded physical indexed writer from this alone.

Nineteen additional STATIC-ONLY PC Ghidra targets
were queued after this result in
`ghidra_scripts/targets.txt`. They include function
and ASM export for 1326E (dynamic list append),
13230 (element copy), 13434 (constructor), 134AE/
133C4 (destructor), 14212/14847 (object/hardware
initialization), plus xrefs for 107FE (other physical
MMIO write sites), 13FA6 (list registration),
13434/1326E/13230/134AE/14847.

**NEXT PC Ghidra action, not scope:**
```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
.\scripts\run-ghidra-analysis.ps1 -CommitMessage "analysis: resolve indexed register-list bounds and GPIO ownership"
```
The script handles pull, analysis export, commit and push.
No full XStream regression is due until a useful
combined milestone. See
[second-batch details](docs/safe-abi-batch-and-missing-ioctls-2026-09-30.md).

**Newest completed grouped test (owner result, 2026-09-30):**
The owner ran `scripts/test-safe-ioctl-batch.ps1` on the
actual x64 PCI scope and supplied the final summary:

```text
SAFE ABI BATCH: 9/9 passed; 0 failed.
All requested safe ABI checks passed.
```

The originally committed harness also printed a misleading
red PowerShell 5.1 `NativeCommandError` / `RemoteException`
at its former `& $diag @Command 2>&1 | Out-String`
capture line: intentionally negative `raw-ioctl`
buffer-size tests cause `lecdiag` to write an expected
error to native stderr, which Windows PowerShell 5.1
additionally materializes as an ErrorRecord. The nine
actual checks still returned PASS. A SOURCE-ONLY
`scripts/test-safe-ioctl-batch.ps1` correction now
uses `Start-Process -Wait -PassThru` with separate
redirected stdout/stderr temp files, reads native
output and exit code, and removes temp files in finally.
**Corrected harness has not yet been rerun**; do not
invent a second 9/9 test. No driver, firmware,
`tools/lecdiag/lecdiag.c` or signed package was
changed for this presentation-only fix.

Per the owner's test preference do NOT request another
complete XStream/AP015 regression now, or require
a same-nine-check rerun solely to validate cosmetic
log formatting. Continue meaningful STATIC analysis
of the two remaining high-risk write controls
(`0x0022303C` register index & bounds;
`0xCFDC2130` FPGA GPIO bit-banging). The twelve
static Ghidra targets are already in
`ghidra_scripts/targets.txt`; NEXT PC-only command:

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
.\scripts\run-ghidra-analysis.ps1 -CommitMessage "analysis: inspect remaining register and serial-trigger write paths"
```

The Ghidra script pulls, runs analysis exports and
commits/pushes new evidence. It does NOT touch the
running x64 scope driver. Do NOT fire artificial
nonzero CFDC2400 pending bits, issue a 266-byte
indexed register write or program BAR1 GPIODAT
with arbitrary serial-trigger data on the only
working scope. Preserve the 24/27 source-level
top-level IOCTL count (one gated), and defer a
single XStream regression to a later combined milestone.
See [grouped ABI result and safety review](
docs/safe-abi-batch-and-missing-ioctls-2026-09-30.md).

**Historical immediately preceding queued batch plan:**

**Current owner-requested test cadence (2026-09-30): GROUP REGRESSIONS.**
The owner explicitly does **not** want to spend time doing a complete
XStream waveform/AP015 regression after every small development step.
Collect a useful, safe test/implementation milestone first, then request
**one** full XStream practical regression for the combined changes.
This supersedes the historical "immediate next XStream regression"
wording below as a scheduling instruction; the post-CFDC2400
XStream result is still objectively PENDING, not implicitly PASS.

The owner already demonstrated installed `CFDC2400` zero-mask
positive ABI success on real PCI. A new scope-side nine-case script
`scripts/test-safe-ioctl-batch.ps1` is now committed but **has
NOT yet been executed**. It checks driver build 1002, PCI 1570:0005,
passive START/FVER-vs-BAR0 comparison, CFDC2400 four-byte ZERO
mask with output capacities 0 and 4, 3-byte and 5-byte rejected
CFDC2400 input (Win32 ERROR_INVALID_PARAMETER = 87), and CFDC2194
wrong output capacities 28/30 (rejected before consuming latch).
With optional `-IncludeErrorStatus` it also consumes one
valid 29-byte CFDC2194 status read; OMIT that switch by default.
The script refuses to run if XStream process is active and requires
only the already-built `tools/lecdiag/build/lecdiag.exe`; it does
NOT sign/load/reboot, inject any nonzero software pending bits,
issue Dallas writes or program serial-trigger FPGA/GPIO.
Note that even CFDC2400 ZERO mask still invokes existing DPC
processing, per original semantics; do not mislabel that action
as entirely passive.

NEXT useful user action on the x64 scope, with XStream closed
(no driver reload necessary):

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }
.\scripts\test-safe-ioctl-batch.ps1
```

Expected only after actual execution: `SAFE ABI BATCH: 9/9
passed; 0 failed.` Report the complete output/any failure;
expected DeviceIoControl failures for invalid buffer sizes
are successes in this test suite. Do NOT claim 9/9 in advance.
Read [grouped safety/test and remaining original IOCTL analysis](
docs/safe-abi-batch-and-missing-ioctls-2026-09-30.md).

STATIC original-handler progress:
- `0x0022303C` (`FUN_00012CAC`) requires an exact
  0x10A-byte (266B) buffer when global gate `DAT_1CD08==0`.
  `FUN_0001259A` reads a signed/unchecked DWORD register
  wrapper-table index at record offset +0x101 and a DWORD
  value at +0x106, then `FUN_000107FE` caches and writes
  the selected physical register. No safe bounds guard
  visible in the helper; DO NOT port/call with fabricated
  record until actual table/index mapping is known.
- `0xCFDC2130` (`FUN_00011CFF`) requires a non-null,
  nonempty byte stream; it reads BAR1 GPIODAT (+0xC4)
  through hardware-subobject wrapper +0x318 and on EACH
  input byte replaces only 0xE000 with
  `((byte << 8) & 0xE000)`, then commits a real MMIO
  DWORD via `FUN_000107FE`. Preserve byte-stream
  timing/order; not a safe arbitrary live scope probe.
- Licensed Dallas `0x00223088` remains deliberately
  deferred until a disposable chip is available.
Twelve additional static-only Ghidra export targets for
these two write paths have been queued in
`ghidra_scripts/targets.txt`, NOT yet executed.
No extra driver code was added for them. Native top-level
representation stays **24/27** with one gated, three absent.
Finish safe ABI batch and further static analysis before
the ONE deferred XStream regression.

**Historical pre-batching immediate test request (superseded):**

**Newest on-scope positive result (2026-09-30; CFDC2400):**
The owner supplied actual `lecdiag` output after running the
newly installed native `CFDC2400` code on their real x64
LeCroy PCI interface (VEN_1570, DEV_0005):

```text
.\tools\lecdiag\build\lecdiag.exe raw-ioctl 0xCFDC2400 00000000 0
Opened device interface: [actual PCI 1570:0005; unique instance omitted]
IOCTL 0xCFDC2400 succeeded: input=4 output-capacity=0 returned=0
Output:
```

This **confirms the zero-mask positive IOCTL ABI in real hardware**
(input exactly four bytes, request success, returned bytes zero),
including the installed driver's acceptance of the new case.
The separate complete build/sign/load transcript was not pasted in
this particular turn; do not invent the warning count, exact package
ID or unrelated scope results. A four-byte zero mask leaves the OR
unchanged, but the original derived-vtable method (and this port)
runs existing DPC processing unconditionally. It is not a claim
that nonzero synthetic interrupts were tested.

**IMMEDIATE NEXT GATE: post-CFDC2400 practical XStream regression.**
Start normal XStream with currently installed new driver; verify
live waveforms/amplitude/frequency, V/div/timebase,
coupling/bandwidth, trigger, two channels/10 GS/s as applicable,
and AP015 preattached recognition, physical unplug/replug,
unlocked-jaw warning. Watch for startup or IRQ/event regressions
after the new immediate software DPC path. No new build/reload is
needed before this regression. Do not run latch-consuming
`lecdiag error-status` while XStream is active or inject nonzero
software pending bits merely for coverage. After this regression,
document actual owner observations and revise the verified
working baseline accordingly.

**Historical immediately preceding CFDC2400 source-only state:**

**Newest source milestone (2026-09-30, after PC Ghidra export
commit `b7b31c8bf5a06e9621a3636776673b486f72bfe5`):**
The previously missing real derived hardware-subobject virtual
`+0x24` has been **resolved** by raw vtable dump:
`0x1C62C+0x24 -> 0x1C650 -> 0x114F2`.
The thunk at 0x114F2 adjusts receiver by -0x1E0, calls
original `FUN_00011390` (DPC dispatcher) DIRECTLY and
synchronously, and ends in `RET 0x8`. The earlier
base-class no-op `0x104A0` was from vtable `0x1C8BC`,
not the active derived vtable. Original complete CFDC2400
therefore: validate exact 4-byte buffered input, synchronize/
OR caller DWORD into `DAT_0001CE10`, immediately run the
existing DPC dispatcher, report Information=0/STATUS_SUCCESS.

New x64 **SOURCE-ONLY, NOT YET BUILT/LOADED/TESTED** port:
- `driver/LecS65Drv.h`: `LECS65_IOCTL_CFDC2400` and
  `LecInjectLegacyPendingAndDispatch` prototype.
- `driver/Acquisition.c`: use `InterlockedOr` on existing
  `InterruptPendingShadow`, temporarily raise to
  `DISPATCH_LEVEL` (existing DPC spinlock contract),
  call `LecInterruptDpc` DIRECTLY, restore caller IRQL.
  No new hardware registers, artificial IRQ or queued DPC.
  Even zero mask runs dispatcher, matching original code.
- `driver/Ioctl.c`: exact four-byte input with non-null
  system buffer; STATUS_INVALID_PARAMETER otherwise,
  positive status from helper and Information=0.
- `docs/ioctl-map.md` now counts **24/27** original
  top-level values represented (one gated), **three**
  absent: `0x0022303C`, `0x00223088`,
  `0xCFDC2130`.

**Next on REAL SCOPE: build only first**, with XStream
closed, at
`C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`:

```powershell
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Pull failed; STOP" }
& ".\scripts\build-driver.ps1" -BuildLecdiag
if (-not $?) { throw "New CFDC2400 build failed; STOP" }
```

If successful and with previous working driver recoverable,
use established elevated `scripts/build-sign-load-driver.ps1`.
Then with XStream still closed run ONLY
`tools\lecdiag\build\lecdiag.exe raw-ioctl 0xCFDC2400 00000000 0`.
Expected: 4-byte accepted input, Information/returned 0.
Avoid nonzero injected pending bits on the working scope.
Then perform normal live XStream waveform/control/AP015
regression. Do not report the new patch as verified before
the owner supplies actual Windows/scope output. See
[CFDC2400 investigation](docs/cfdc2400-software-pending-investigation.md).
The preceding CFDC2194/CFDC2190 owner-confirmed runtime
is still the last verified hardware baseline.

**Historical pre-export CFDC2400 hypothesis, superseded below:**

**Newest open source-analysis gate: original CFDC2400 (2026-09-30).**
Existing `ghidra_exports/selected/raw_12ec2.asm.txt` proves
its synchronized callback `LAB_00012EC2` does
`DAT_0001CE10 |= DAT_0001CE1C` on a non-null receiver.
The original user input is a required exact FOUR-byte DWORD;
`DAT_0001CE10` is the ISR/DPC software pending bitmap.
The callback itself performs no MMIO or DPC insertion.

**Critical correction to historical vtable prose below:** the
`CFDC2400` dispatch at original 0x112EA passes
`main+0x1E0`, NOT the main object, to `FUN_00013A2E`
and `FUN_00012EDE`. The hardware subobject's active
derived vtable is original `0x0001C62C` (constructor
`FUN_00010B3C` at 0x10B5C); the method called after
the callback is its slot `+0x24`, i.e. pointer DWORD
at **0x0001C650**. The earlier attribution of this
call to `0x000104A0` (`XOR EAX,EAX; RET`)
is **NOT VERIFIED and ABI-inconsistent**: original
`FUN_00012EDE` pushes TWO DWORDs before calling the
virtual method and has no caller-side cleanup. A bare RET
would unbalance the stack. Do not treat this old
main-vtable mapping as proof of a no-op for CFDC2400.
Actual derived slot and target must be exported first.

To preserve the working scope baseline, the provisional
new x64 `CFDC2400` constant/case drafted during analysis
was REVERTED. `driver/Ioctl.c` and `LecS65Drv.h`
have no CFDC2400 port; **23/27** original top-level
codes remain represented in x64 source (one gated),
four absent including CFDC2400. No native scope/driver
installation is requested now.

NEW Ghidra exporter support for `dwords:<hexbase>:<count>`
has been committed in `ghidra_scripts/ExportSelected.java`,
with literal table and slot +0x24 instruction preview.
`ghidra_scripts/targets.txt` now requests
`dwords:1c62c:16` (correct derived subobject),
`dwords:1c500:16` (main object), `dwords:1c8bc:16`
(base subobject), and original callback/dispatch ASM.
These targets have NOT YET BEEN EXECUTED on the PC Ghidra
environment. Next step: the owner runs
`scripts/run-ghidra-analysis.ps1` on their Ghidra PC
with commit message `analysis: resolve CFDC2400 derived-vtable slot`.
Then inspect `ghidra_exports/selected/dwords_dwords_1c62c_16.txt`,
especially `+0x24`; decompile actual target if needed.
Full proof, ABI caveat and command:
[CFDC2400 pending callback investigation](
docs/cfdc2400-software-pending-investigation.md).
Leave the previously verified waveform/AP015/PCI/IRQ/DMA
driver untouched until actual virtual method semantics are known.

**Historical checkpoint (CFDC2194 validation):**

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

**Historical pre-regression checkpoint (superseded for XStream):**

**Latest verified source-patch hardware checkpoint (2026-09-30):**
The owner's `build-sign-load-driver.ps1` succeeded: 0 build
warnings/0 errors, x64 lecdiag built, SYS/CAT signed, package
installed as `oem99.inf`, PCI device restarted, build query
1002, PCI diagnostic passed, and new
`lecdiag error-status` returned `0x00000000` with
`PASS: original 29-byte response layout verified.`
This supersedes the earlier **UNBUILT / UNTESTED** note
immediately below for build, load and CFDC2194 positive ABI
only. **XStream acquisition/AP015 regression on this NEW
CFDC2194/CFDC2190 driver remains unreported.**
The idle zero error word does not test nonzero ISR status
latching or the persistent-error branch.
See the timestamped evidence in
[CFDC2194 investigation](docs/cfdc2194-status-latch-investigation.md)
and [quick handoff](docs/quick-handoff-2026-09-29.md).

**Historical development checkpoint, now superseded for build/install/idle positive test:**

**Newest engineering state (2026-09-30, after owner Ghidra export):**
The new selected `field_0x134a.refs.txt` and
`field_0x134d.refs.txt` unequivocally find the original
`CFDC2194` software-latch **producer** in the x86
interrupt handler `FUN_000108D6`: original VA 0x10958
ORs raw BAR0 ERRS into `main+0x134A`, and VA 0x10A67
sets byte `main+0x134D` bit 7 (= status DWORD bit 31)
on a persistent/reasserted error. Original dispatch
0x11322 sets the handler receiver to `main+0x1E0`,
so `FUN_00012BAE`'s `this+0x116A` is EXACTLY the same
DWORD at `main+0x134A`. The prior literal `field:116a`
scan was incomplete because the ISR used the main-object
coordinate. This is now a **proven source result**, not a
hypothesis; do not request the owner to rerun the same scan.

New **source-only, UNBUILT / UNTESTED on Windows/scope**
native x64 work is committed:
- `driver/LecS65Drv.h`, `Driver.c`: sticky atomic ERRS latch
  and cached BAR0 ERRM mask.
- `driver/Acquisition.c`: accepted INTST `0x02`
  accumulates BAR0 ERRS (+0x004), maps ERRS bits 10..14
  onto BAR1 CLRERR bits 0..4, writes ERRS back, and
  implements the original 1-us persistent/reasserted
  check including status bit 31. Does not rewrite
  unrelated established acquisition IRQ/DPC handling.
- `driver/Ioctl.c`: new exact 29-byte
  `LECS65_IOCTL_CFDC2194` (DWORD 2 at +4, sticky
  status snapshot at +8, otherwise zeros; atomic read/clear).
  The adjacent `CFDC2190` is corrected to require
  type DWORD 2 at +4, use DWORD +8 for INTEN bit 1
  enable/disable, and program **bitwise-complemented**
  DWORD +8 to physical BAR0 ERRM. This corrects a
  preexisting x64 semantic divergence from `FUN_00013A40`;
  startup behavior after correction MUST be regression-tested.
- `tools/lecdiag/lecdiag.c`: `error-status` reads
  and consumes the original 29-byte status reply and verifies
  layout with XStream closed. Do not race this diagnostic
  against XStream's own potential latch consumption.

**Immediate next action:** perform Windows **build-only**
(`.\scripts\build-driver.ps1 -BuildLecdiag`) with XStream
closed. Stop on compiler errors. Only after successful build,
use the established elevated signed reload procedure with a
known-good recovery driver available; run
`.\tools\lecdiag\build\lecdiag.exe error-status` while
XStream stays closed. Then repeat XStream waveform,
vertical/timebase, trigger and AP015 regression. Idle
zero status is acceptable but proves only ABI/read-clear
execution, NOT that a nonzero error IRQ or sticky bit 31
occurred. Never inject a hardware fault or write Dallas
on the one licensed card for this validation.
Full provenance and commands:
[CFDC2194 status latch](docs/cfdc2194-status-latch-investigation.md);
[quick handoff](docs/quick-handoff-2026-09-29.md);
[TODO](docs/TODO.md). Existing pre-change
`start-register` real-scope PASS and the owner's
XStream/AP015 regression remain the last VERIFIED hardware
baseline; the new source patch has not replaced it yet.

**Historical checkpoint below (before the 2026-09-30 Ghidra
producer discovery; no longer the next task):**

**Latest verified real-scope feedback (2026-09-29 late):**
The owner ran `lecdiag start-register` on the real PCI device. New
read-only `0x00223044` returned START/FVER `0x00000002`; separate
generic BAR0+0x000 reference read returned `0x00000002`, and the
diagnostic reported PASS. This live positive-path result supersedes
the previous `ERROR_INVALID_FUNCTION` from an older loaded driver
after an elevated-build attempt was blocked by missing admin rights.
The complete later build/sign/load transcript and scope Git HEAD
were not supplied as independent evidence.

**Post-change XStream regression: owner reports all exercised checks
working (2026-09-29 late).** Following the PASS, the owner was asked
about live waveforms, ordinary vertical/timebase/coupling/bandwidth/
trigger behavior, two-channel 10-GS/s where practical, and AP015
detection, physical hotplug and jaw-unlock indication. Reply:
`Ja klappt soweit alles.` This closes the immediate regression,
but is an owner-reported baseline confirmation, not a full
instrumented per-feature trace matrix. No new regression reported.

**Next source-analysis target: `0xCFDC2194`.** Original
`FUN_00012BAE` produces an exact 29-byte read-and-clear response;
its status field comes from software latch `this+0x116A`.
A literal-displacement scan found only the handler's own read/clear,
not a nonzero producer. Related original `0xCFDC2190` handler
`FUN_00013A40` accepts a 29-byte type-2 command, updates
`DAT_0001CE18`, invokes `FUN_000107FE` on register helper
`this+0x188`, and registers callback `FUN_00012EAE`.
That callback can invoke `FUN_00011E46`, which writes the global
register. These are a verified adjacent control path, **not** a
proven writer of `this+0x116A`. Investigate indirect aliases,
callback/IRQ and initialization before implementing `0xCFDC2194`.
Do not fabricate a permanent-zero success. Preserve working
PCI/IRQ/DMA/ProBus. Virtual Dallas recovery, physical chip isolation
and writes to the licensed DS2433 remain deferred.

See [quick handoff](docs/quick-handoff-2026-09-29.md),
[current handoff](docs/next-chat-handoff.md) and [TODO](docs/TODO.md).

**Active CFDC2194 static-analysis advance (2026-09-30):**
Original `FUN_000115C4` invokes hardware initializer
`FUN_00014847(main+0x1E0,...)`. Original `FUN_00012D24` consumes
the `this+0x138` BAR0 START/FVER member that initializer sets.
For `FUN_00012BAE` (`CFDC2194`), its latched DWORD
`this+0x116A` is therefore also a **candidate**
`main+0x134A` under the shared dispatcher receiver
(`0x1E0 + 0x116A = 0x134A`); verify receiver flow in
the original DeviceControl wrapper, not just the C labels.
The previous `field:116a` direct scan alone cannot rule out
a writer using `main+0x134A`, overlapping stores, or an
indirect pointer. Added `asm:10b30` wrapper context,
`asm:115c4`, neighboring callback instructions and direct
`field:` targets for overlapping starts at `1167..116D`
and `1347..134D`, plus nearby candidate LEA bases, in
`ghidra_scripts/targets.txt`. **Targets committed; Ghidra
has not yet rerun them.** No real status producer or correct
synchronization established, and no x64 kernel patch made.
Detailed evidence, conditional address arithmetic and safe
PC export command:
[CFDC2194 provenance investigation](docs/cfdc2194-status-latch-investigation.md).

**Current original/x64 dispatch audit (2026-09-29 late):**
All **27** original top-level IOCTL codes are identified, but that
does NOT imply full original behavior is understood or ported.
The current `driver/Ioctl.c` switch covers **22/27** of those codes,
including explicitly gated WOW64-sensitive `0xCFDD219F`;
the five lacking x64 cases after the successful START/FVER addition
are `0x0022303C` (original register-write record),
`0x00223088` (recovered original Dallas writer, not safely ported),
`0xCFDC2130` (serial-trigger FPGA programming),
`0xCFDC2194` (29-byte status read-and-clear, latch producer unknown)
and `0xCFDC2400` (internal four-byte control/callback path).
`0x00222400` in x64 is outside the 27-value captured original
dispatch. Original `0xCFDC2110` nested commands and alternate
`0xCFDC2138` transfer forms are not all covered merely because
their top-level code is dispatched. See latest
[IOCTL map](docs/ioctl-map.md) and [TODO](docs/TODO.md).
Prioritize source analysis of `this+0x116A` next; do not enable a
permanent-zero fake handler or perform destructive EEPROM/FPGA
write tests just to raise a numerical coverage count.

**Documentation and priority policy (2026-09-29):**
Keep `README.md` a concise, consolidated statement of **verified
current project state**, not a development diary. Keep full investigation
history and prior superseded failed attempts in specialist `docs/`,
`docs/runtime-trace.md`, and this/handoff file. Maintain outstanding
tasks in `docs/TODO.md`. The user explicitly deferred virtual Dallas
ROM/EEPROM emulation and any physical DS2433 disconnection test.
They are backlog items, not immediate implementation authorization.

**2026-09-29 latest confirmed Dallas exports:** User completed both
separate original ROM-ID preservation
(privately, do not publish bytes)
and the Ghidra rerun. New exports
`00016d90` and `00016f2c`
with ASM now exist on main;
`16d90` concretely implements
SKIP ROM, WRITE SCRATCHPAD,
READ SCRATCHPAD and data verification,
COPY SCRATCHPAD and 100-ms delay.
`16f2c` performs a complete
READ MEMORY operation from zero.
Original 11f54 provides
<=32-byte chunks, final full
readback/compare and retry.
Use the now-recovered protocol
when designing x64 writer;
a still separate virtual
diagnostic mode should be
validated before physically
isolating the DS2433. No
kernel modification yet.
**2026-09-29 physical Dallas test sequencing:**
User offers to isolate the DS2433 physically to validate
recovery if the chip fails. This is a useful eventual
test, **not** the first action. Complete same-card ROM
ID must be separately backed up (existing `dallas-backup`
only saves 512 writable memory bytes). Implement
an explicit virtual host-facing Dallas ID/READ
path and separate coherent shadow WRITE state first,
with no actual 1-Wire transactions in emulated
handlers. Test with real chip still connected,
then consider reversible ID_DATA isolation
**only with power removed and electrical
topology verified**. No hot unplug, shorts
or destructive PCB modification on assumptions.
With device absent, assess PCI enumeration
and FPGA/driver startup separately from
XStream license behavior. If PCI/FPGA
depends on physical 1-Wire before driver
IOCTLs, host emulation alone cannot
recover a dead chip. Restore original
wiring and verify original ROM/image afterward.
Test plan:
`docs/dallas-device-manager-recovery-design.md`.

User has now rerun Ghidra successfully:
both helper decompilation and ASM exports
`FUN_00016d90` and `FUN_00016f2c`
are on main and have been reviewed.
16d90 shows the actual DS2433
`CC 0F` scratchpad write,
`CC AA` scratchpad read/data
comparison, `CC 55` copy using
TA1/TA2/E/S and 100-ms post-copy
delay, up to three per-chunk tries.
16f2c reads the full requested
length from `CC F0 00 00`;
the outer handler 11f54 compares
the final readback and retries.
The user also confirms the original
8-byte ROM was backed up privately.
Exact technical details:
`docs/dallas-device-manager-recovery-design.md`.
No emulation or native x64
EEPROM write code was implemented
in this Ghidra/docs update.
**Failed/replaced DS2433 nuance (2026-09-29):**
A factory-new chip has a different immutable eight-byte
ROM ID even if original 512-byte EEPROM contents are
programmed. The leading six-digit displayed Scope-ID
component matches ROM serial bytes 1..3 LE; the
remaining display suffix is still not decoded.
License binding to that ROM has NOT been proven,
so universal license invalidation must remain a
conditional risk, not a fact. Current
`lecdiag dallas-backup` exports only the 512
memory bytes; separately save complete original
`dallas-id` before loss of the original chip.
For a physically dead/replaced chip, an optional
virtual Dallas source could conceptually return
the archived ROM and image through host ID/READ
(and coherent separate shadow-write behavior),
but this does not program hardware, and it is
not yet known if PCI FPGA/firmware boot also
requires the actual device. Distinguish this
from genuine in-place EEPROM restoration.
Documented design:
`docs/dallas-device-manager-recovery-design.md`.
No virtual/restore writer has been implemented.
**New UI feature request (2026-09-29):** User requests a
Device Manager Dallas EEPROM maintenance page with backup,
compare, recovery and optional offline hex editing. A native
x64 device-specific property-page extension DLL is possible
using `EnumPropPages32`; avoid deprecated co-installers.
Prefer a standalone maintenance application plus optional
lightweight Device Manager page. Current INF installs only
the kernel SYS. Scope identity note: confirmed primary
display identifier derives from the DS2433's factory ROM
serial bytes 1..3 (24-bit little-endian), NOT the separate
512-byte data memory; display suffix remains undecoded.
A backup cannot alter a replacement chip's factory ROM.
Current x64 supports ROM/read but not write. Recovery
requires original writer reconstruction, test-chip
validation and full post-write verification.
Design: [`docs/dallas-device-manager-recovery-design.md`](docs/dallas-device-manager-recovery-design.md).
The UI and hardware writer are not implemented yet.
**Latest trace and original-code review (2026-09-29):**
Private 011858 captured the same valid Dallas
family-0x23 ROM response at seq 1 and 607.
ROM bytes 1..3, viewed as little-endian 24-bit
serial, match the primary portion of the user's
displayed scope identifier. The display suffix
is still unassigned; the exact formatted ID
is not a literal IOCTL preview string.
Recently pushed Ghidra `FUN_00011f54`
and asm establish <=32-byte write steps,
full requested-length read-back/compare
and up to three passes. The underlying
`FUN_00016d90` and `FUN_00016f2c`
implementations have since been
exported and reviewed. Native x64 still
returns `STATUS_INVALID_DEVICE_REQUEST`
for WRITE_DALLAS_MEMORY. Distinguish the
factory ROM ID from the separate 512-byte
EEPROM data, and keep card-specific
raw traces private. Full notes:
`docs/dallas-license-memory-test-plan.md`.
**NEW DECISIVE WRITE FAILURE (trace
`xstream_trace_20260929_011858.jsonl`,
2026-09-29, uploaded private):
User deleted one XStream key on current
x64 replacement, but it still appeared
after restart. In the 608-entry
gap-free 63.0224566-s IOCTL capture,
the sole NTSTATUS failure is genuine
XStream WOW64
`WRITE_DALLAS_MEMORY`
`0x00223088`, seq **522**,
t~57.303140 s, input **512**,
output **0**, Information **0**,
returned **`0xC0000010` /
STATUS_INVALID_DEVICE_REQUEST**.
Current x64 `driver/Ioctl.c` has
only ID `0x80` and read `0x84`
Dallas handlers and defaults to
this unsupported status for
missing WRITE `0x88`. XStream
immediately requests another
512-byte READ, seq **523**,
SUCCESS; captured first 128
READ bytes match baseline
seq **3**. The intended
full-512-byte write's captured
256-byte preview has 93 differing
offsets within common first 128
bytes vs baseline READ, pages
0..3. No raw memory/key values
in public docs. This is the
direct explanation for the
nonpersistent XStream deletion:
NOT an observed DS2433 physical
write failure, since the driver
rejected the request before
writing. Stop repeated x64 UI
delete tests until the writer
is source-recovered/implemented.

**Confidentiality:** This uploaded
legacy-format JSONL contains real
or candidate license content in
READ output and WRITE input
hex previews! Never commit,
reshare or quote its hex; treat
raw kernel debugger logs alike.
Updated `tools/lecdiag/lecdiag.c`
to redact the three Dallas
ROM/READ/WRITE payload directions
from FUTURE rebuilt JSONL
exports while retaining IDs,
length/status and setting
`sensitive_payload_redacted`.
Original x86 write handler
VA `0x11F54` targets
`11f54, asm:11f54,
xref:11f54` now added to
`ghidra_scripts/targets.txt`
for the PC Ghidra export via
`scripts/run-ghidra-analysis.ps1`.
Review true original 32-byte
scratchpad/copy/readback sequence
before adding any x64 write; do
NOT return fabricated success.
No native kernel writer code
was modified in this step.
Full details:
`docs/dallas-license-memory-test-plan.md`
and
`docs/next-chat-handoff.md`.
**Latest private Dallas structural result (2026-09-29):**
User ran `scripts/inspect-dallas-image.ps1` on one
of two previously matching 512-byte backups.
The data has **many 0x00-filled pages, NOT
an FF-erased free-page convention**:
0x0C0..0x15F (pages 6..10, 160 zero bytes)
and 0x180..0x1DF (pages 12..14, 96
zero bytes) are fully 0x00, with
partially mixed pages 0..5, 11 and 15.
The no-wholly-FF-page abort of the offline
dummy-image helper was correct but does
NOT prove no free application license slot.
Zero-filled pages are also **not** proven
safe scratch space: record layout,
checksums and allocation are unknown.
User can try a native XStream license
deletion/readdition A/B, but only for
ONE legitimately owned, independently
recorded/re-enterable key, ideally after
a no-op UI control and with original
x86 XStream/driver supporting Dallas
write 0x00223088 (still unimplemented
in x64). Take separate private 512-byte
read-only before/no-op/after-delete/
after-readd snapshots and compare
redacted offset ranges via
`scripts/inspect-dallas-image.ps1`.
A double backup is NOT a tested
restore capability; do not casually
delete the only licensed key or publish
raw license memory/IOCTL input. See
`docs/dallas-license-memory-test-plan.md`
and current `docs/next-chat-handoff.md`.
**Latest Dallas result (2026-09-29):** Both
independent private 512-byte DS2433 backups
match, but running the **offline**
`create-dallas-dummy-image.ps1` found NO
fully-FF 32-byte memory page and safely
created no test image. This does NOT
mean no application-level free license
record exists. Do not remove its
protective check or rewrite an arbitrary
non-FF area. User offers a better
format-discovery experiment using native
XStream's **Add License** dialog, but
invalid fake input may be rejected
before any write. The original x86
driver has `WRITE_DALLAS_MEMORY`
0x00223088; current replacement
x64 driver does NOT implement it.
For private structure/diff analysis
without leaking keys, new
[`scripts/inspect-dallas-image.ps1`](scripts/inspect-dallas-image.ps1)
offline utility outputs only per-page
FF/zero/printable counts, or changed
offset ranges and page numbers between
two independent pre/post images.
No actual new EEPROM write, erase,
license addition or restore occurred.
If trying original x86 XStream,
capture only IOCTL codes, buffer
lengths/status and order; raw write
input may expose active license keys.
Recovery should first be exercised
on a spare DS2433. See current
[`docs/dallas-license-memory-test-plan.md`](docs/dallas-license-memory-test-plan.md).
**Latest Dallas licensing test (2026-09-29):** user ran
the read-only 512-byte `lecdiag dallas-backup`
twice on actual scope and reports both saved
images have IDENTICAL SHA-256 digests. The
specific private hash and memory image must
not be published. User next requests a
**fictional test license**, not a working
entitlement. Committed
[`scripts/create-dallas-dummy-image.ps1`](scripts/create-dallas-dummy-image.ps1)
which purely OFFLINE creates a private
candidate 512-byte copy by checking that two
backups match bytewise, finding the last
fully `0xFF`-filled 32-byte page
(if one exists), and placing the invalid
30-byte marker
`FAKE-XSTREAM-LICENSE-TEST-ONLY`.
It uses CREATE_NEW and verifies the
result. No installed EEPROM is written;
a 0xFF page is NOT proven to be a
valid vacant license slot or excluded
from checksums. **Current native x64
driver has NO original 0x00223088
WRITE_DALLAS_MEMORY implementation.**
Do not claim current write/restore
compatibility. A spare DS2433 must
first validate the recovered
scratchpad/copy/verification algorithm
and safe power-cycle restore. Script
source was committed but not run by
assistant on Windows; the candidate
will be created when user runs it.
Procedure:
[`docs/dallas-license-memory-test-plan.md`](docs/dallas-license-memory-test-plan.md).
**Important 2026-09-29 user hardware correction:**
ALL FIVE front-facing ProBus sockets communicate **entirely via
I2C, not SPI**. A probe-class ADC value comes first;
I2C reads the front identification EEPROM and
controls the physical probe. The existing original
family-0/0x90 BAR1 SPICTL/SPIDAT/SPIDIN
helper is a separate internal board serial path,
believed by the user to program ADCs, references
and related circuitry. Exact target/selector
mapping is not yet proven, but **do not describe
the AP015 physical bus as SPI or hedge its I2C
nature**. The PCI card's `U11 DS2433`
1-Wire memory stores **XStream license keys**,
per user, rather than only a generic board-ID
payload; plaintext storage is their hypothesis,
not yet privately verified. The distinct eight-byte
DS2433 ROM ID and writable 512-byte licensing
EEPROM must not be confused, nor should either
be confused with front ProBus I2C EEPROM or
PCI FPGA `U6 XC18V02` configuration PROM.
Detailed architecture and safe licensing plan:
[`docs/probus-detection-i2c-architecture.md`](docs/probus-detection-i2c-architecture.md),
[`docs/dallas-license-memory-test-plan.md`](docs/dallas-license-memory-test-plan.md).
**The x64 driver implements Dallas ROM/read but
NOT the original `0x00223088` write IOCTL.**
New read-only `lecdiag dallas-backup` was
source-committed, not Windows-built or tested
on the scope. It compares two full 512-byte
reads and two ROM-ID reads, saves through
CREATE_NEW and verifies the persisted file,
without printing secrets or writing EEPROM.
Only test writer/erase on a spare DS2433
after verifying private backups and a viable
recovery procedure. Never commit raw license
images; `license-backups/` is Git-ignored.
**New original hardware schematics reviewed (2026-09-29):**
the user's `PCI Card.pdf` shows PCI-side Spartan-IIE
`U3 XC2S200E` with PI5C3861 PCI bus switches,
on-card `U11 DS2433` ID/memory via net `ID_DATA`,
`U6 XC18V02` FPGA configuration PROM, and separate
40-pin receive `J1` / transmit `J2` differential
link headers (CLOCK, D0..D11, SYNC, RESET_ERR and
stable signals). `Overview.pdf` is a separate
LeCroy acquisition-board top-level drawing with
`UP Control (UP)`, `Timebase (TB)`, two
`ADC+MAM` blocks `AM/AM2`, `FPGA's (FP)`,
four channel front ends plus EXT, and separately
named `I2C(0:5)`, `SPI_IO(0:40)` and other
control/data buses. **The Dallas/1-Wire BAR2 path
has a concrete PCI-card DS2433 endpoint; this
is NOT the front-panel I2C probe EEPROM or the
XC18V02 configuration PROM. PCI Spartan U3 is
NOT the acquisition-board AM/AM2/FP FPGA.**
The schematics show no FPGA RTL, exact BAR-to-net
mapping, link encoding, ADC probe-ID values or
front EEPROM address. Canonical derived architecture:
[`docs/pci-card-acquisition-board-topology.md`](docs/pci-card-acquisition-board-topology.md).
Do NOT commit original proprietary schematic PDFs
to this public repo.
**New front-end hardware context, provided by the user:** probe connection is
classified as ProBus by an **ADC identification value**; the front-panel
**EEPROM is then read over I2C**; subsequent physical probe control also
uses **I2C**. This is a probe/front-panel layer, **not** proof that the
Windows driver exposes I2C directly. Our original-driver family-0/0x90
BAR1 `SPICTL/SPIDAT/SPIDIN` helper is a distinct host-to-board layer;
do not misidentify that SPI register path as the probe's physical bus.
Canonical details and open mapping questions:
[`docs/probus-detection-i2c-architecture.md`](docs/probus-detection-i2c-architecture.md).
**Important 2026-09-29 correction:** The user's XStream clearly warns that
the opened AP015 clamp is **not locked and measurements may be inaccurate**.
Earlier assistant statements interpreting missing recorded `0x0200` in
two specific trace captures (`233125`, `235314`) as **failure of
XStream to recognize jaw opening were wrong**. New scope trace
`xstream_trace_20260929_002051.jsonl`, with the same unchanged driver,
records actual genuine `0x0200 -> 0x88 -> 0x82` jaw-state transitions:
`0x0058` at t~24.623 s correlates with open and family-1/0x4A status
**F7**, while `0x00A7` at t~49.861 s correlates with closed and
status **F3**. The same open/F7, closed/F3 pairing was captured twice
in earlier trace `231656`. Physical hotplug also works after 85FB
fix `3490709` (trace `001152`). **No demonstrated general
jaw-recognition, HWInt or DMA regression; do not rollback patches
or invent a new probe-state event.**

## Repository and communication

- Repository: `https://github.com/Battlecake91/lecroy_wr6k_64bit_driver`
- Public repository.
- Repository documentation and code comments are written in English.
- Chat with the user is in German.
- Active development happens on `main`.
- New confirmed findings must be reflected in the repository documentation.
- Keep `README.md`, this file, and the relevant detailed docs synchronized with the actual state.

## Project goal

Reconstruct the externally visible behavior and hardware protocol of the legacy
32-bit LeCroy `LecS65AcqDrv.sys` used by WaveRunner 6000 / S65 hardware and
provide a maintainable native x64 Windows replacement that remains compatible
with the existing 32-bit XStream software through WOW64.

Reference binary:

- SHA-256: `5F53DE1DEA6A58F201290E79A60FAB587C039423322FAA884BF4C15F0FD89087`
- Size: 64,384 bytes
- Version/build: `6.1.1.1002` / build `1002`
- PCI ID: `VEN_1570&DEV_0005&SUBSYS_00000000&REV_00`

The proprietary reference binary is not stored in this public repository.

## Safety rule

Do not re-enable or invent hardware execution for partially understood command
paths.

In particular:

- `0xCFDC2110` hardware execution is enabled only for command classes whose
  host-side semantics or verbatim board-forwarding behavior have been recovered
  and validated. Unknown/unproven packet classes must remain rejected; do not
  broaden the whitelist merely to make XStream advance.
- Do not recommend arbitrary raw `CFDC2110` replay against hardware.
- Do not recommend arbitrary BAR reads/writes outside already understood
  registers. A previous incorrect BAR access could freeze the device.
- Prefer static analysis and passive runtime tracing until the hardware semantic
  is understood.

A truthful unsupported result is preferred over a structurally valid but
semantically wrong hardware response.

## Authoritative BAR map

Resource order used by the legacy driver:

- BAR0: first memory resource, length `0x200`
- BAR1: second memory resource, length `0x40000`
- BAR2: third memory resource, length `0x200`

Important BAR0 offsets:

- FVER `0x000`
- ERRS `0x004`
- ERRM `0x008`
- START `0x00C`
- SGTA `0x040`
- IIMTC `0x044`
- IIMCL `0x048`
- IIMST `0x04C`
- INTST `0x080`
- INTEN `0x084`

Important BAR1 offsets:

- ITMODE `0x000`
- CLRERR `0x004`
- CLRIRQ `0x008`
- ACQFVER `0x00C`
- JTAGNUM/JTAGDAT/JTAGDIN `0x20/0x24/0x28`
- MAMDAT/MAMPGO/MAMSEQ/MAMRGO `0x40/0x44/0x60/0x64`
- MTTCTL/MTTRGO/MTTNUM `0x80/0x84/0x90`
- SPICTL/SPIDAT/SPIDIN `0xA0/0xA4/0xA8`
- GPIODIR/GPIODAT `0xC0/0xC4`
- LEDCTL/RMIDIV/RMICUM/ACQDIV/ACQCUM/PFREG
  `0xE0/0xE4/0xE8/0xEC/0xF0/0xF4`
- SetIRQ `0x100`
- TxControl/RxControl/TxCount/RxCount/HWInt
  `0x400/0x404/0x408/0x40C/0x410`
- TX window `0x420 + 4*n`
- RX window `0x600 + 4*n`

BAR2:

- BUZZER `0x000`
- ONEWIRE `0x040`

See `docs/hardware-register-map.md` for the maintained map.

## High-value IOCTLs

- `0xCFDC21C0`: generic register read
- `0xCFDC21C4`: generic register write
- `0xCFDC21C8`: driver build query -> 1002
- `0xCFDC2110`: packed command/response path, main reverse-engineering focus
- `0xCFDC212C`: always `STATUS_NOT_IMPLEMENTED`
- `0xCFDC2130`: serial-trigger FPGA programming
- `0xCFDC2184`: success, zero information
- `0xCFDD219F`: METHOD_NEITHER path, primary WOW64 ABI risk
- `0x00223080/84/88`: Dallas ID/read/write

See `docs/ioctl-map.md` and `docs/abi-analysis.md`.

## Current reverse-engineering state

### CFDC2110

Packed record:

```c
#pragma pack(push, 1)
typedef struct {
    uint16_t output_length;
    uint16_t payload_length;
    uint16_t type;
    uint16_t signature;
    uint8_t  payload[];
} LECS65_TRANSFER_RECORD;
#pragma pack(pop)
```

Type 3 signatures:

- `0x85FB` -> response/fetch path
- `0xA5FB` -> command path
- `0xC5FB` -> third related direct-status path

A5FB payload byte at record +9 selects family 0/1/2 and byte +10 is the opcode.
The family dispatch tables are documented in `docs/ioctl-map.md`.

### Board message transport

- TX helper `0x176E6`: 16-bit words, chunks up to `0x78` words, BAR1 TX
  window at `0x420`, TxCount/TxControl submission, continuation bit handling.
- RX helper `0x17578`: BAR1 RX window at `0x600`, RxControl continuation
  handling and event waits.
- `0x1619A`: synchronous request/response wrapper.
- `0x15DB8` and `0x16168`: command-side transmit wrappers.

### Interrupt/event path

Confirmed chain:

```text
hardware interrupt
 -> ISR 0x108D6
 -> status/ack
 -> KeInsertQueueDpc
 -> deferred processing 0x11390
 -> KeSetEvent
 -> waiting transport/acquisition path resumes
```

`0x1860E` wraps `IoConnectInterrupt`.

### Acquisition / MDL path

Confirmed:

- `0x1731C`: allocates a 0x40-byte transfer entry and registers a user buffer.
- `0x1807A`: builds MDL chains and locks user pages.
- `0x17F8C`: allocates a fixed 0x33000-byte nonpaged descriptor buffer + MDL.
- `0x18194`: walks the 32-bit MDL/PFN layout and builds board-facing
  address/word-count descriptors.
- `0x17F60` / `0x17FD6`: unlock/free MDL chains and transfer structures.
- No symbol-table evidence for `MmGetPhysicalAddress`, `IoMapTransfer`,
  `GetScatterGatherList`, or `PutScatterGatherList`; the legacy driver
  appears to build its descriptor table directly from MDL PFNs.

### Acquisition execution path

Current connected path:

```text
IOCTL-near front-end 0x13C84 / 0x13DC6
 -> resolve registered transfer entry through 0x18168
 -> build/validate temporary channel configuration
 -> 0x12D6A acquisition orchestrator
 -> 0x17D20 / 0x17CDC indexed MAM programming
 -> 0x17478
 -> 0x171DE synchronous hardware transfer
 -> event wait / timeout
```

`0x17BA6` wires MAMDAT/MAMPGO/MAMSEQ/MAMRGO into the MAM helper object.

### Packed command front-end

`0x13AE2` is the IOCTL-side packed command processor. It walks concatenated
records and distinguishes:

- type 3 through `0x16C14`;
- type 1/2 through `0x12F30`.

Latest helper results:

- `0x15710`: true when record type == 3.
- `0x15720`: true when record type == 1 or 2.
- `0x16C14`: type-3 signature dispatcher for A5FB / 85FB / C5FB.
- `0x153A2`: parses a concatenated record stream, counts records and total
  response size, using span `8 + payload_length`.
- `0x15364`: advances to the next packed record.
- `0x15422` and related 0x153xx/0x154xx helpers manage the temporary output
  buffer used by `0x13AE2`.

The raw instruction window around `0x1115B` now confirms the exact dispatch `0xCFDC2110 -> 0x13AE2`. The surrounding DeviceControl tree also reconfirms neighboring Dallas and control IOCTL branches.

## Ghidra workflow

The checked-in Ghidra project is under:

`ghidra_reverse_engineering_lecroy/LeCroy_Alladin_Driver.gpr/.rep`

The reusable exporter is:

`ghidra_scripts/ExportSelected.java`

The target list is:

`ghidra_scripts/targets.txt`

The operator runs only:

```powershell
cd C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver
.\scripts\run-ghidra-analysis.ps1
```

The runner pulls, executes headless Ghidra, stages exported results, commits and
pushes them. It also writes `ghidra_exports/selected/EXPORT_MANIFEST.txt`
containing the source commit, requested targets and exported file list, then
prints the resulting export commit SHA and a GitHub commit URL that can be
shared directly. The local Ghidra installation path is stored in ignored
`.ghidra-local.ps1`.

`ExportSelected.java` now exports a raw instruction window when an address is
not contained in a defined function. This is specifically intended to recover
undefined dispatch code such as the reference at `0x1115B`.

Do not ask the user to manually edit the target list. Update
`ghidra_scripts/targets.txt` in the repository, then tell the user to run the
same runner script.

## Git/Ghidra database handling

Ghidra rotates internal `db.N.gbf` files. Some snapshots are required for the
checked-in project, while local rotations must not continuously dirty Git.

The runner marks tracked Ghidra snapshot files `skip-worktree`; transient
rotated snapshots and lock files are ignored through `.gitignore`.

Do not blindly delete the checked-in Ghidra database snapshots. Doing so can
make the project report `LecS65AcqDrv.sys not found`.

## Documentation policy

After every meaningful analysis round:

1. update the relevant detailed doc, usually `docs/ioctl-map.md`,
   `docs/abi-analysis.md`, or `docs/hardware-register-map.md`;
2. update `README.md` when the public project status or major milestone has
   changed;
3. update this `AGENTS.md` whenever the current state, workflow, safety rules,
   or next-step assumptions change;
4. update `ghidra_scripts/targets.txt` with the next analysis set.

Do not leave new established findings only in chat.

## Current priority

1. **Correct the diagnostic interpretation before touching the driver.**
   The user explicitly observes that XStream displays a not-locked
   warning and measurement-accuracy caution when the AP015 jaw opens.
   Earlier `233125` and `235314` did have ZERO captured
   pending-0x0200/family-1/0x82 events, but it was an invalid leap
   to conclude XStream could not recognize the mechanical state.
   Those captures are evidence about that particular IOCTL stream,
   not proof about the application's warning/UI state.
2. **Latest post-`3490709` trace `002051` confirms real jaw events.**
   31,200 captured IOCTLs over 58.3684694 s (seq 1..35955),
   32 snapshot gaps / 4,755 omitted entries, final gap ends
   t~23.382490 s (before **all** key events). No observed
   failing NTSTATUS. CFDC2110=23,896, CFDC2138=5,839;
   all 5,839 return exactly requested bytes (CFDC2138 input
   offset 11), last 1,024/1,024 at t~58.367987 s.
   Standalone 85FB/0x01=1,292, enabled 0x02BF, pending:
   0x0080=1,287, 0x0200=3, 0x0280=1, 0x0000=1.
   Four actual 0x0200-bearing events each with matching 0x88
   and actual-length+FF-padded family-1/0x82:
   - t~24.623 s: status seq 14888 / ack 14889 /
     0x82 seq 14891 raw `000012005800`, status WORD **0x0058**;
     one family-0/0x4A 47 12 seq 14913 returns original-x86
     `...02000000FFFF`, status family-1/0x4A seq 14916 **F7**.
     This corresponds to opened jaw and XStream's not-locked warning.
   - t~35.038 s: status 21435 combined pending 0x0280,
     ack 21436 and 0x82 seq 21448 raw `00001200FE03`
     (WORD **0x03FE**); an apparent connector removal-like state.
   - t~37.458 s: status 22833/ack 22834/0x82 seq 22843
     raw `000012005700` (WORD **0x0057**). Later AP015
     reidentification family-0/1 0x4A seq 22877/22878,
     Information=270; the captured first 128 bytes exactly match
     startup metadata seq 506.
   - t~49.861 s: status 30553/ack 30554/0x82 seq 30563
     raw `00001200A700` (WORD **0x00A7**);
     one 47 12 seq 30580 -> original `...02000000FFFF`;
     0x4A status seq 30581 **F3**; jaw closed correlation.
   This reproduces earlier `231656` open 0x0058/F7 and
   closed 0x00A7/F3 twice. F7 XOR F3 = bit `0x04`, but no
   vendor-defined status-bit semantics are yet proven.
3. **Retain the verified source changes.** Driver `70716ba` recovered
   original BAR0 INTEN/INTST 0x08 and BAR1 HWInt 0x410 handling.
   Driver `3490709` corrected original `FUN_000167F4`
   actual received-length and FF-padded short raw 85FB records.
   Earlier `230614`, `231656`, `233125`, `235314`,
   `001152` and newest `002051` together establish multiple
   functioning hotplug, jaw-event, short 0x82, 47 00/47 12 and
   0x99 compatibility paths. No proven reason to revert either
   driver commit, synthesize 0x0200 or change stable PCI/DMA/ISR.
4. **Remaining optional protocol research (not an identified UI
   regression):** F7/F3 versus historical x86 F2 after 0x4A
   controls; exact state-word bits 0x0058/0x00A7, 0x03FE/0x03FF,
   0x0057/0x0058; transient wrong-probe first reinsertion in
   `001152` (0x028C then 0x0058 without full AP015 metadata).
   Never assign literal "1/2 clamp" semantics to 0x028C without
   static/dependent evidence. Record actual displayed behavior
   and physical action times with any future focused experiment.
5. Keep <=5% scope CPU, below-4-GiB DMA descriptor safety, existing
   IIMCL/CLRIRQ, family-1 0x51; `CFDD219F` and unobserved
   multi-channel CFDC2138 remain gated. Full handoff in
   `docs/next-chat-handoff.md`, A/B in
   `docs/probus-calibration-ab-comparison.md`.


## Latest dispatch recovery

The raw DeviceControl windows through `0x1138D` now recover the entire late dispatch section and common completion tail. Unknown IOCTLs become `STATUS_INVALID_PARAMETER`; `0xCFDC2184` is inline success with zero information; pending requests bypass normal completion logging. The function ends immediately before `0x11390`, the known deferred/event helper.


## Latest handler semantics

The remaining DeviceControl handlers are now substantially decoded:

- `0xCFDC2124`: persistent user-buffer registration; returns the 32-bit transfer-entry pointer as the legacy token.
- `0xCFDC2128`: removes and frees the transfer entry selected by that four-byte token.
- `0xCFDC2130`: serial-trigger FPGA stream bit-banged through BAR1 `GPIODAT` masked field `0xE000`.
- `0xCFDC2180` / `0xCFDC218C`: event registration tied to current process; the former can signal immediately when status is already active.
- `0xCFDC2190`: 29-byte error/interrupt mask control; field 2 updates global bit 1 and BAR0 `ERRM`.
- `0xCFDC2194`: paired 29-byte status readback and clear.
- `0x00222C00`: auxiliary hardware line is definitively BAR2 `BUZZER`; `0x1557C` asserts it around the delay.
- `0xCFDC2400`: synchronized global pending-bit update; its final virtual method is a no-op in this build.

Diagnostic helpers `0x10750`, `0x1076E`, and `0x1919A` are logging-only; `0x10798` is the common IRP completion helper.


## Latest event/control findings

- `0x11B18` is the shared event-reference helper: `ObReferenceObjectByHandle(..., EVENT_MODIFY_STATE, ExEventObjectType, RequestorMode, ...)`.
- `0x157B8` returns `pending_bits & enabled_bits`; `0xCFDC2180` can therefore signal a freshly registered event immediately when relevant status is already pending.
- `0x12EDE` (`0xCFDC2400`) stores a caller DWORD in global `DAT_1CE1C`, performs a synchronized callback, then calls main-object vtable method `+0x24` with `(0,0)`.
- The main-object vtable is resolved: `+0x08` removes a transfer entry,
  `+0x0C` registers one, and `+0x24` is the no-op used by `0xCFDC2400`.


## Latest synchronous transfer hook result

The main-device virtual methods used by `0x171DE` are now decoded:

- `0x13914`: set global transfer/interrupt mask bit 0, then commit through the synchronized `0x12EAE -> 0x11E46` path.
- `0x13934`: clear the same bit and commit it identically.
- `0x104A0`: no-op method returning zero. Therefore the final virtual call made by `0xCFDC2400` has no hardware side effect in this build.
- `0x12E18`: process-owned transfer cleanup. It removes all registered transfer entries belonging to a supplied process and frees them through `0x17FD6`.
- `0x14122` / `0x134F6`: deleting destructor / main-object teardown.
- `0x170EE`: thunk into subobject helper `0x1829A`, still unresolved.


## Current transfer register map

The synchronous acquisition helper `0x171DE` is now tied to concrete registers:

- object `+0x04` -> BAR0 `IIMCL` (`0x048`)
- object `+0x2C` -> BAR1 `MAMRGO` (`0x064`)
- object `+0x58` -> BAR0 `SGTA` (`0x040`)
- object `+0x80` -> BAR0 `IIMTC` (`0x044`)
- object `+0xD0` -> BAR1 `MTTRGO` (`0x084`)

The sequence is: program `SGTA`, program `IIMTC`, reset the transfer-entry event, enable global mask bit 0, write `IIMCL=1`, launch through `MAMRGO` or `MTTRGO`, wait up to five seconds, then disable mask bit 0.

`0x1829A` is transfer-list cleanup reached via the base-object thunk at `0x170EE`; it is not transfer execution logic.


## Current MAM/MTT and descriptor result

- MAMDAT uses bits 23:16 as an 8-bit index and bits 15:0 as the value.
- MAMPGO encodes a two-bit mode at bits 9:8 and a WORD count at bits 7:0;
  `0x105` is mode 1 with five words.
- The five per-channel words are `0x0E00 | channel`, configuration low/high,
  and low/high halves of `min(0x400, total_bytes/channel_count)`; the FPGA unit
  of the last field is not statically named.
- MAMSEQ encodes sequence index in bits 24:16, final-entry in bit 6, and
  channel identifier in bits 5:0.
- Acquisition IOCTLs always launch through MAMRGO. CFDC2110 family-1 opcodes
  `0x50/0x51` use MTTRGO through `0x160DC`.
- DMA entries are `{DWORD count_dwords, DWORD physical_address}`. Table pages
  have 512 entries, with slot 511 linking to the next page using count zero;
  a zero/zero entry terminates the chain.
- SGTA is the physical address of the first descriptor-table page and IIMTC is
  the total DWORD count returned by `0x18194`.


## Current CFDC2110 and WOW64 boundary

- Local A5FB helpers are mapped to JTAG, SPI, divider/cumulative registers,
  `FVER`, `ACQFVER`, `PFREG`, `ITMODE`, `LEDCTL`, `MTTCTL`, and `MTTRGO`.
- Remaining startup/runtime commands that use `0x15DB8` or `0x16168` are
  firmware-defined packets forwarded verbatim over BAR1; static driver analysis
  cannot assign their payload semantics.
- `0xCFDC2124` takes `{uint32 user_buffer, uint32 total_bytes, uint32 reserved}`
  and returns the transfer-entry kernel pointer as a four-byte token.
- `0xCFDC2128` removes that token. The legacy helper has no current-process
  ownership check; the x64 driver should use an opaque 32-bit owned token.
- Create/Close maintain a 16-byte per-process node. Last Close frees owned
  transfers for flag 1 and dereferences owned events for flag 2.
- `0xCFDD219F` consumes packed two-byte channel pairs plus two DWORDs from
  `Type3InputBuffer`, transiently locks `Irp->UserBuffer`, reserves its final
  DWORD for the completed byte count, and unregisters immediately afterwards.
- The original does not capture/probe `Type3InputBuffer` and does not include
  the trailing result DWORD in its `ProbeForWrite` range. The x64 replacement
  must correct both issues while preserving the external packed ABI.


## Current completion-path result

`0x108D6` is the ISR and `0x11390` the deferred/DPC dispatcher. `0x115C4`
initializes the DPC with DeferredContext equal to the main device object. The
`0x10872` callback calls `0x11DC2`, which consumes pending interrupt bit 0
from `DAT_1CE10`; if set, `0x11390` executes
`KeSetEvent(*(main+0x2E0)+0x24)`.

`main+0x2E0` is now tied to the selected transfer entry: acquisition handlers
run on the acquisition subobject at `main+0x1E0`, and both `0x13C84` and
`0x13DC6` store the `0x18168`-resolved transfer entry at subobject `+0x100`.
Since `(main+0x1E0)+0x100 == main+0x2E0`, the DPC signals the same
`entry+0x24` event that `0x171DE` resets and waits on. The acquisition-transfer
completion interrupt source is therefore confirmed as interrupt bit 0, gated by
the transfer mask enable/disable methods `0x13914` and `0x13934`.


## Current x64 implementation state

The native x64 driver has been advanced beyond the original bring-up prototype:

- `0xCFDC2124` persistent transfer registration is implemented with an
  owner-checked opaque 32-bit token instead of exposing a kernel pointer.
- `0xCFDC2128` unregisters only a token owned by the current process.
- registered data buffers are probed/locked through MDLs;
- board-facing 8-byte `{count_dwords, physical_address}` descriptors are built
  into chained 4 KiB table pages with the legacy slot-511 link format;
- the x64 implementation rejects table/source physical addresses above 4 GiB
  rather than truncating them into the legacy 32-bit descriptor format;
- process Close and PnP stop/remove release registered transfers;
- line-interrupt connection, ISR acknowledgement for explicitly owned sources,
  DPC dispatch and transfer completion-event signalling are implemented;
- the interrupt path is passive until `InterruptEnableShadow` explicitly owns
  a source;
- the exact observed one-channel `0xCFDC2138` path is active: it reuses a
  successfully registered below-4-GiB descriptor chain, programs the recovered
  MAM/SGTA/IIMTC sequence, launches through MAMRGO and waits up to five seconds
  for interrupt-bit-0 completion;
- `0xCFDD219F` and unobserved multi-channel `0xCFDC2138` remain
  `STATUS_NOT_SUPPORTED`;
- recovered `0xCFDC2110` classes execute their proven local or board-forwarded
  behavior, while unknown/unproven classes remain rejected;
- family-1 opcodes `0x50/0x51` now execute the statically recovered local
  MTT transfer path: resolve an owner-checked CFDC2124 token, validate
  `launch_word << 3 >= DataBytes`, program SGTA/IIMTC, enable transfer
  interrupt bit 0, write IIMCL=1, launch through BAR1 MTTRGO and wait up to
  five seconds for the same completion event used by CFDC2138.

Passive runtime tracing has been upgraded:

- 256-entry kernel ring;
- boot-relative 100 ns timestamps;
- PID/WOW64/method/status/information;
- Type3InputBuffer and UserBuffer pointer values for correlation;
- up to 256 input bytes;
- up to 128 METHOD_BUFFERED output bytes;
- bounded probed METHOD_NEITHER input preview;
- live JSONL collection every 250 ms through
  `scripts/capture-xstream-trace.ps1`.

Trace files are written below ignored `trace-captures/` and are designed to
be handed back to an analysis chat directly. See `docs/runtime-trace.md`.


## Dallas runtime timeout on x64

On the reference x64 system the replacement service is RUNNING, the PnP device
is present as `LeCroy Acquisition Device (S65)`, and `lecdiag build`
successfully returns legacy build 1002. The first direct Dallas test currently
fails with Win32 error 121, corresponding to the driver's
`STATUS_IO_TIMEOUT` from the ONEWIRE polling loop.

This means the driver is loaded and reachable; the immediate bring-up problem
is the BAR2 ONEWIRE controller path, not service installation or device binding.

Next safe diagnostics are limited to already-known non-destructive reads:

- `lecdiag bars`
- `lecdiag read 2 0x40` (ONEWIRE idle state)
- `lecdiag read 0 0x0` (FVER)
- `lecdiag read 1 0x0C` (ACQFVER)

Do not compensate by inventing writes. If ONEWIRE bit 0 is already set while
idle, investigate missing legacy startup/FPGA initialization before changing
the Dallas transaction semantics.


## Current MMIO bring-up blocker

All three tested safe MMIO reads currently return `0xFFFFFFFF` on the x64
reference system, despite correct PnP binding and BAR resource assignment:

- BAR0 FVER
- BAR1 ACQFVER
- BAR2 ONEWIRE

The service is RUNNING, the PnP device reports OK, and the build query returns
1002, so this is not a driver-load or DOS-link problem.

A private passive `lecdiag pci` diagnostic now reads the PCI configuration
header and reports PCI Command/Status, BDF, BAR config values and IRQ metadata.
Use that before considering any startup-register write. If Memory Space Enable
is clear, fix PCI decode first. If it is set while MMIO remains all ones,
investigate the recovered legacy START/ITMODE/FPGA initialization path.


## Diagnostic build workflow note

Private diagnostic changes require rebuilding both the user-mode `lecdiag.exe`
and the x64 driver when the private IOCTL ABI changes. A stale `lecdiag.exe`
is immediately visible because its usage text lacks newly added commands such
as `pci`. Use `scripts/build-lecdiag.ps1` after pulling tool changes, then
rebuild/reload the driver before exercising a new private diagnostic IOCTL.


## Command-line build workflow

Use `scripts/build-driver.ps1` as the normal driver build entry point. It
locates MSBuild through Visual Studio/vswhere and builds `Debug|x64` by
default. Add `-BuildLecdiag` to rebuild the user-mode diagnostic tool in the
same step.

The historical test-machine workflow also used command-line service/package
operations (`pnputil`, `sc.exe`) after signing. Do not assume a newly built
SYS is loadable until the test-signing/catalog state matches the installed
package.


## Build toolset

The x64 driver project currently targets `PlatformToolset=v143` for Visual
Studio 2022. A temporary `v145` setting caused MSB8020 on the reference build
machine and has been removed.


## Trace timestamp build fix

The trace ABI version 3 uses `KeQueryPerformanceCounter` and exposes
`timestamp_ticks`. Do not switch back to `KeQuerySystemTime` or
`KeQueryInterruptTime` without first verifying declaration and linkage in the
actual WDK project; both caused build/link failures on the reference system.


## Reference-machine reload script

For the already-installed test driver, use
`scripts/build-sign-load-driver.ps1` from elevated PowerShell. It builds the
driver and lecdiag, signs the SYS with `CN=LecS65 x64 Test`, stops the
service, replaces the installed SYS, restarts it, verifies build 1002, and runs
`lecdiag pci`. This mirrors the earlier command-line test-signing workflow and
avoids accidentally testing a stale loaded driver.


## Driver reload through Driver Store

The reference system denies direct replacement of
`System32\drivers\LecS65AcqDrv.sys` even after the PCI device is disabled.
The maintained `build-sign-load-driver.ps1` therefore stages a fresh INF/SYS,
generates/signs a CAT, gives the staged INF a newer DriverVer, installs through
`pnputil /add-driver /install`, and restarts the device. Use the Driver Store
rather than direct System32 copies.


## Inf2Cat DriverVer traps

Inf2Cat 22.9.6 occurred because PowerShell date formatting used the German
culture's dot separator for `DriverVer`. The package script now uses
InvariantCulture with literal slashes and prints the generated DriverVer line.

Inf2Cat 22.9.7 also occurred shortly after local midnight because the local
calendar date was one day ahead of UTC and therefore appeared postdated. The
package script now uses the UTC calendar date for the DriverVer date field but
keeps the local build timestamp in the numeric version for monotonic ordering.


## Confirmed PCI configuration on x64 reference system

The passive PCI diagnostic now confirms:

- BDF `4:1.0`
- VEN/DEV `1570:0005`
- PCI Command `0x0006`
- PCI Status `0x0208`
- BAR0 `0xF7CBFE00`
- BAR1 `0xF7CC0000`
- BAR2 `0xF7CBFC00`
- IRQ line 19, pin 1

PCI Command `0x0006` proves Memory Space Enable and Bus Master Enable are
active. BAR config values exactly match Windows translated resources. The
persistent `0xFFFFFFFF` MMIO reads are therefore not currently explained by
disabled PCI decoding or an obvious BAR mapping error.

Next step: statically close and then reproduce only the confirmed legacy
`0x12FDE` START/ITMODE startup sequence. Avoid speculative writes.


## Scope/PC role split

Keep the two machines distinct in instructions:

- **Scope**: Windows scope machine, user `LeCroyUser`, contains the installed
  x64 driver and the working checkout used for build/sign/load and runtime
  `lecdiag` tests. Current repo path:
  `C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`.
- **PC**: development/reverse-engineering PC, contains Ghidra and the Git
  checkout used for `scripts\run-ghidra-analysis.ps1`. Do not give Ghidra
  commands as if they run on the scope.

The two checkouts synchronize through GitHub.


## FUN_00012FDE exact startup sequence

The existing Ghidra export confirms the legacy startup probe exactly:

- BAR0 START <- 1
- wait 100 us
- read START bit 0
- clear -> 1200 ms buzzer pulse and STATUS_UNSUCCESSFUL
- set -> 300 ms buzzer pulse, ITMODE=7, wait 500 us, 300 ms buzzer pulse,
  ITMODE=3, STATUS_SUCCESS

The x64 driver now reproduces only this confirmed sequence during PnP start.
Do not expand it with guessed initialization writes.


## Startup probe confirmed on hardware

The x64 reproduction of legacy `FUN_00012FDE` is confirmed on the reference
scope and fixes the prior all-ones MMIO state.

Runtime after driver reload:

- two startup beeps, matching the legacy success branch;
- BAR0 FVER = `0x00000002`;
- BAR1 ACQFVER = `0x00000003`;
- BAR2 ONEWIRE = `0x00000000`;
- Dallas ID succeeds: `23 F0 47 37 00 00 00 AC`.

Conclusion: PCI configuration and BAR mapping were already correct. The FPGA /
register fabric required the exact START/ITMODE startup sequence before normal
MMIO access.

Next step: capture a normal XStream startup with
`scripts/capture-xstream-trace.ps1` on the scope and analyze the resulting
JSONL, especially remaining firmware-forwarded `0xCFDC2110` records.


## First real XStream startup trace

A 120-second trace from the reference scope contains 35 IOCTLs and 11
`CFDC2110` calls from a WOW64 XStream process.

Distinct `CFDC2110` startup request forms:

- one RESET: A5FB payload `40 02 40 01 52 45 53 45 54 00`
- two family-1 opcode `0x99`: payload `40 01 99 00`
- six family-0 opcode `0x88`: payload `40 00 88 00 DF FF`
- two family-1 opcode `0x42` JTAG requests
- each non-RESET request includes a following 85FB `40 00` fetch record
- no C5FB record observed

All 11 currently return `STATUS_INVALID_DEVICE_REQUEST` because the x64
driver keeps `CFDC2110` trace-only. Do not re-enable the whole path at once.
The next implementation step should be narrow and staged against these exact
observed startup packets.

A sanitized fixture is committed under
`testdata/traces/xstream_startup_20260926_cfcd2110_sanitized.jsonl`.
The raw trace stays local/private.


## Byte-exact CFDC2110 runtime gate

Commit `ab70d0a920712b780cdb0dc15792d03a62e54091` enables hardware execution
only for the four complete `CFDC2110` input buffers observed in the first
XStream startup trace. Every other buffer remains rejected before hardware side
effects.

Next test on the scope:

1. pull latest main;
2. run `scripts/build-sign-load-driver.ps1`;
3. start a fresh `capture-xstream-trace.ps1` capture;
4. launch XStream normally;
5. preserve the new raw JSONL locally and analyze new packet shapes before
   widening the whitelist.


## Second whitelisted XStream trace

The staged runtime gate now proves that RESET, family-1 `0x99`, and family-0
`0x88` execute successfully on the reference scope.

The next new startup packet is:

```text
A5FB 40 00 85 00 A0 00
+ 85FB 40 00 fetch
```

It occurs five times.

Do not enable opcode `0x85` yet. Legacy `FUN_00016962` performs host-side
state changes before generic transmit. Ghidra targets `1573e` and `15772`
were added to close those remaining side effects.

The captured 94-byte family-1 opcode-`0x42` JTAG whitelist entry had a manual
byte-transcription error. Commit
`bdfc91a969981de464b1430ac700b43972b1ccbb` corrects the exact packet match.


## Opcode 0x85 implementation complete

Ghidra commit `c4800ac03da8d1cc510ce89deeaaa47a8d7db0c6` resolves the two remaining
helpers behind family-0 opcode `0x85`:

- `0x1573E`: toggle global INTEN bit `0x10`, then commit;
- `0x15772`: toggle global INTEN bit `0x20`, then commit.

Together with `0x16074` (INTEN bit `0x04`), captured control WORD
`0x00A0` yields mask updates `+0x04, -0x10, -0x20`. The x64 driver now
applies those exact changes, writes BAR0 INTEN, forwards the captured request,
and accepts only the exact byte sequence seen in the trace.

Next scope test: pull, build/sign/load, capture XStream startup again, and
inspect the next rejected CFDC2110 packet before widening the whitelist.


## Third staged XStream trace

The latest capture contains 109 IOCTLs and 62 CFDC2110 calls:

- 21 success;
- 41 rejected by the exact runtime gate;
- startup repeats three times after later rejections, matching the observed
  XStream startup exceptions/errors;
- the heavy retry is a family-1 opcode-`0x42`, mode-0 83-bit JTAG request
  (33 rejected calls);
- two more new exact forms appear: family-1 opcode-`0x90` and a mode-1
  58-bit opcode-`0x42` JTAG request.

Commit `4b33b22e656ca6c008b56203c2b756c68df9e3b1` admits those three exact
buffers. `0x90` uses the already-decoded generic transport path; both new
`0x42` packets use the existing JTAG implementation.

The reported Auto-trigger failure is not yet evidence of an acquisition-path
bug because the trace never gets past repeated startup retries. Re-test startup
first after this commit before touching acquisition or trigger/DMA code.


## Fourth staged XStream trace

Latest capture: 46 IOCTLs, 21 CFDC2110 calls.

All 15 rejected CFDC2110 calls are the same already-known family-1 opcode
`0x42`, mode-0, 83-bit JTAG request. No new opcode appears.

Root cause: the manually entered whitelist array was 52 bytes, but the actual
XStream request is 54 bytes. Two zero bytes before the `C0 00 00 07` tail
were missing/misplaced. Commit
`9f49a04d298c087c5f9c934dcb18f55d8816c50d` replaces the entry with the exact
54-byte captured buffer.

Also, `lecdiag` capture mode still emitted trace header version 2 despite the
trace ABI being version 3. Commit
`58592348770215d80ede4041b5e66f3666eef0b2` fixes the remaining literal.

Next scope action: pull, build/sign/load, capture XStream startup again. No
Ghidra work is needed for this step.


## Latest staged trace: 2026-09-26 23:50

The raw capture `xstream_trace_20260926_235015.jsonl` remains outside Git.

Observed totals:

- 38 IOCTL records.
- 21 `0xCFDC2110` calls.
- 10 CFDC2110 successes.
- 11 CFDC2110 rejections.

The corrected 54-byte family-1 opcode-`0x42`, mode-0, 83-bit JTAG request now
succeeds. This confirms that its previous rejection was solely a whitelist
transcription error.

The only newly rejected request is an exact 94-byte family-1 opcode-`0x42`,
mode-1, 256-bit JTAG form. It repeats 11 times. It is identical to the already
admitted 256-bit mode-0 request except for the mode byte at the JTAG request
payload changing from `0x00` to `0x01`.

This form is safe to admit byte-exactly because:

- `LecJtagExecute` already accepts modes 0 and 1;
- mode 1 is already exercised by the admitted 58-bit startup JTAG transaction;
- mode 1 only adds bit `0x100` to BAR1 `JTAGNUM` in the reconstructed
  implementation;
- all data words, bit-count handling, and response handling otherwise use the
  same decoded JTAG path.

The 94-byte mode-1 256-bit request is therefore now added to the byte-exact
startup whitelist. Do not generalize this to arbitrary opcode-`0x42` or
arbitrary JTAG packets.

After the rejected block, the capture still reaches the known opcode-`0x90`
transport request and the known mode-1 58-bit JTAG transaction. No new opcode
family is present.

Next step: rebuild/reload on the scope, capture another 120-second XStream
startup, and inspect what appears after the newly admitted mode-1 256-bit JTAG
stage. Do not change trigger or DMA paths yet.


## Latest staged trace: 2026-09-27 00:49

The raw capture `xstream_trace_20260927_004923.jsonl` remains outside Git.

Observed totals:

- 40 IOCTL records.
- 23 `0xCFDC2110` calls.
- 12 CFDC2110 successes.
- 11 CFDC2110 rejections.

The admitted 94-byte family-1 opcode-`0x42`, mode-1, 256-bit request now
succeeds, so startup progressed beyond the previous byte-exact gate.

The first and only newly rejected request shape is an exact 54-byte family-1
opcode-`0x42`, mode-1, 83-bit JTAG form. It repeats 11 times and differs from
the admitted mode-0, 83-bit request at exactly byte offset 11 (`0x00` to
`0x01`). The existing `LecJtagExecute` implementation supports both modes;
mode 1 only adds bit `0x100` to BAR1 `JTAGNUM`, and mode 1 is already exercised
by the admitted 58-bit and 256-bit requests.

This exact buffer may therefore be proposed for a future byte-exact admission,
but it remains rejected after this analysis round. Do not replace the gate with
an opcode-, family- or mode-wide rule. No acquisition launch appears in the
trace, and trigger/DMA paths remain unchanged.


## Latest staged trace: 2026-09-27 00:59

The raw capture `xstream_trace_20260927_005905.jsonl` remains outside Git.

Observed totals:

- 46 IOCTL records.
- 23 `0xCFDC2110` calls.
- 12 CFDC2110 successes.
- 11 CFDC2110 rejections.

The newly admitted exact 54-byte family-1 opcode-`0x42`, mode-1, 83-bit
request succeeds on hardware. The first and only newly rejected request shape
is a 94-byte family-1 opcode-`0x42`, mode-2, 256-bit form, repeated 11 times.
It differs from the admitted mode-1, 256-bit request only at byte offset 11
(`0x01` to `0x02`).

Legacy `FUN_00015C7E` enters the JTAG register loop only for modes 0 and 1.
Mode 2 produces local protocol status 4 and installs a pending response through
`FUN_00015A26` without JTAG hardware access. The current x64 helper also blocks
mode 2 before register access, but its following 85FB fetch response is not yet
proven byte-exact. Keep this packet rejected until that response path is closed.
Do not change trigger or DMA paths.


## Mode-2 JTAG response closed

Ghidra export commit `5cd684608217d7371f32bc93b3e1ab9efd7865dc`
resolves `FUN_00010380` as a non-zeroing
`ExAllocatePoolWithTag(NonPagedPool, size, 'Wdm ')` wrapper.

For the captured family-1 opcode-`0x42`, mode-2, 256-bit request, legacy
`FUN_00015C7E` does not access JTAG hardware. It allocates 40 bytes and
defines only the first 8 bytes as:

```text
00 00 00 00 02 00 04 00
```

The remaining 32 bytes are undefined legacy pool contents. The x64 replacement
zero-fills those bytes deliberately rather than reproducing a kernel memory
disclosure, while preserving the 40-byte response length and protocol status 4.
The exact observed 94-byte request is admitted byte-for-byte. Do not generalize
this to arbitrary mode-2 requests.

Next step: scope pull/build/sign/load and capture another 120-second XStream
startup. Inspect the first request after the mode-2 local-response stage before
touching acquisition, trigger, or DMA launch.

## Latest staged trace: 2026-09-27 01:35

Raw capture `xstream_trace_20260927_013513.jsonl` remains outside Git.

Observed:

- 93 IOCTL records total;
- 71 `CFDC2110`;
- 60 successful, 11 rejected;
- mode-2 / 256-bit local-response path succeeds;
- mode 3 and mode 4 variants of the same 94-byte packet each succeed 16 times;
- mode 5 is the first rejected variant and repeats 11 times;
- modes 2/3/4 return the same status-4 local response and do not imply JTAG
  hardware execution;
- no acquisition-launch IOCTL is present.

The earlier apparent runtime/source mismatch is resolved. The scope checkout
was at commit `f119e4df0df5a1906016ec7a1a56fcb8a78a2705` with local,
uncommitted `driver/Ioctl.c` changes that already contained exact mode-2,
mode-3 and mode-4 long-packet whitelist entries plus the local
`mode > 1` status-4 response path. The signed package SYS hash matched the
installed `System32\drivers\LecS65AcqDrv.sys` exactly, so the trace reflects
that dirty local build.

Main now contains the exact observed mode-3 and mode-4 94-byte packet entries
as well. These packets remain local protocol-error paths with no JTAG MMIO.
Mode 5 is the next rejected exact packet and remains blocked pending a separate
decision. Do not generalize the runtime gate to arbitrary `mode >= 2`
requests.

## Latest staged trace: 2026-09-27 01:53

This run used a clean `main` checkout at
`9b8cf11555b88ee49d65ab94fd08f5c1a4fb4ca6`. The signed package SYS and the
installed driver matched exactly:
`085AF5586FEA3D56215B4FA6FFE3AC2CE57F7B74CE63FDB1C052248382493CF4`.

Trace `xstream_trace_20260927_015327.jsonl` contains 92 IOCTL records and 71
CFDC2110 calls: 60 success and 11 rejected. Modes 2, 3 and 4 of the 94-byte
family-1 opcode-`0x42` 256-bit shape each succeed 16 times with the local
status-4 response. The only rejected shape is the same request with mode 5,
repeated 11 times. No acquisition-launch IOCTL appears.

Mode 5 is now admitted as one exact observed 94-byte packet. It follows the
already proven legacy `mode > 1` local-error path and therefore performs no
JTAG MMIO. Do not replace the exact packet gate with a broad mode rule. Next
scope run should identify the request immediately after mode 5.

## Latest staged trace: 2026-09-27 01:57

Raw capture `xstream_trace_20260927_015757.jsonl` contains 176 IOCTL records
and 155 CFDC2110 calls: 144 successful and 11 rejected.

The mode-5 / 256-bit local-response packet now succeeds. XStream then enters a
new stage using family-1 opcode `0x81`. The rejected exact forms are:

```text
06 00 04 00 03 00 FB A5 40 01 81 00 0A 00 02 00 03 00 FB 85 40 00
06 00 04 00 03 00 FB A5 40 01 81 01 0A 00 02 00 03 00 FB 85 40 00
06 00 04 00 03 00 FB A5 40 01 81 02 0A 00 02 00 03 00 FB 85 40 00
```

Counts are 5, 5 and 1 respectively. Legacy `FUN_000165A6` routes opcode
`0x81` through the same generic BAR1 board-message transport used by
family-1 `0x90`/`0x99`, with no extra host-side register action. These
three exact observed packets are now admitted and routed through the existing
generic transport implementation. Their board-firmware meaning remains
unknown; keep all uncaptured `0x81` forms rejected. No acquisition-launch
IOCTL appears yet.

## Latest staged trace: 2026-09-27 02:01

Raw trace `xstream_trace_20260927_020125.jsonl` contains 180 IOCTL records and
158 CFDC2110 calls: 147 success and 11 rejected.

The exact family-1 opcode-`0x81` selectors 0, 1 and 2 now succeed. The only
rejected packet shapes are the same 22-byte request with selectors 3, 4 and 5,
with counts 5, 5 and 1. These three exact observed forms are now admitted
through the same generic board-message transport. No broad opcode-`0x81`
rule is introduced.

The user reports a reproducible XStreamDSO startup error at the same point on
essentially every test run and manually skips it. Treat that error as relevant
until disproven: it may be the application-visible consequence of the still
incomplete startup command sequence. Correlate whether the dialog disappears
or moves once the remaining startup rejections are removed.

## Startup trace handling after XStreamDSO error dialog

The reproducible XStreamDSO startup error dialog should no longer be skipped
during protocol discovery. For future captures, terminate XStream from the
dialog so the trace ends at the real startup failure point. Traffic after a
manually skipped dialog may belong to retry/recovery logic and should not be
used as evidence for the normal startup sequence.

Latest trace `xstream_trace_20260927_020927.jsonl` exposes the next exact
family-1 opcode-`0x81` selector forms: 6, 7 and 8. These are now admitted as
byte-exact packets through the existing generic board-message transport.
Unknown selectors remain blocked.

## Latest staged trace: 2026-09-27 02:25

This capture was terminated from the XStreamDSO startup error dialog. Treat it
as the clean reference pattern for future protocol-discovery runs: stop at the
dialog rather than skipping it.

Trace `xstream_trace_20260927_022530.jsonl` contains 172 IOCTL records and
161 CFDC2110 calls: 150 success and 11 rejected. The final 11 calls are exactly:

- family-1 opcode-`0x81` selector 9: 5 times;
- selector 10 (`0x0A`): 5 times;
- selector 11 (`0x0B`): 1 time.

No recovery traffic follows. These three exact packet forms are now admitted
through the existing generic board-message transport. Unknown selectors remain
blocked.

## Latest staged trace: 2026-09-27 02:29

Trace `xstream_trace_20260927_022954.jsonl` was terminated at the XStreamDSO
startup error dialog and ends on the true startup failure boundary.

The final 11 rejected family-1 opcode-`0x81` packets use selectors:

- `0x0C`: 5 times;
- `0x0D`: 5 times;
- `0x17`: 1 time.

The selector jump from 13 to 23 means this byte is not merely an incrementing
mode counter. Treat it as an opaque board selector/index until board semantics
are known. These three exact packet forms are now admitted through the existing
generic BAR1 transport. Unknown `0x81` selectors remain blocked.

## Latest staged trace: 2026-09-27 02:31

Trace `xstream_trace_20260927_023142.jsonl` was terminated at the XStreamDSO
startup error dialog. It contains 181 IOCTL records and 170 CFDC2110 calls; the
final 11 rejected calls are three exact family-0 opcode-`0x84` packet forms
with counts 5, 5 and 1.

Legacy `FUN_00016A66` routes family-0 opcode `0x84` directly through
`FUN_00016168`, the generic BAR1 board-message transport, with no additional
host-side register action. These three exact captured 54-byte packets are now
admitted through the existing generic transport implementation. Board-firmware
semantics remain unknown and uncaptured opcode-`0x84` forms stay rejected.

## Latest staged trace: 2026-09-27 02:34

Trace `xstream_trace_20260927_023415.jsonl` was terminated at the XStreamDSO
startup error dialog. It contains 200 IOCTL records and 188 CFDC2110 calls:
177 successful and 11 rejected.

The final rejected block is family-0 opcode `0x84` with selectors:

- `0x03`: 5 times;
- `0x04`: 5 times;
- `0x05`: 1 time.

These exact observed 54-byte packet forms are now admitted through the existing
generic BAR1 board-message transport. No broader opcode-`0x84` rule is
introduced. Unknown forms remain blocked.

## Latest staged trace: 2026-09-27 02:35

Trace `xstream_trace_20260927_023559.jsonl` was terminated at the XStreamDSO
startup error dialog. It contains 206 CFDC2110 calls: 195 successful and 11
rejected.

The final rejected block is family-0 opcode `0x84`:

- selector `0x06`: 5 times, with payload words `0x0036`, `0x0069`;
- selector `0x07`: 5 times, with payload words `0x00A5`, `0x00D5`;
- selector `0x08`: 1 time, with payload words `0x00A5`, `0x00D5`.

The complete 54-byte request buffer is significant. These three exact captured
forms are now admitted through the existing generic BAR1 transport. Do not
generalize admission by selector alone.

## Latest staged trace: 2026-09-27 02:37

Trace `xstream_trace_20260927_023753.jsonl` was terminated at the XStreamDSO
startup error dialog. It contains 224 CFDC2110 calls: 213 successful and 11
rejected.

The final rejected family-0 opcode-`0x84` forms are:

- selector `0x09`: 5 times, payload words `0x00A5`, `0x00D5`;
- selector `0x0A`: 5 times, payload words `0x0043`, `0x0075`;
- selector `0x0B`: 1 time, payload words `0x0000`, `0x0000`.

These three complete 54-byte buffers are now admitted through the existing
generic BAR1 transport. Keep matching byte-exactly; the selector alone is not
sufficient because the payload also varies.

## CFDC2110 gate strategy change

Do not keep adding byte-exact entries for command classes whose legacy path is
statically confirmed to be a pure board forwarder.

Current semantic forward-only set:

- family 0 / opcode `0x84`;
- family 1 / opcode `0x81`;
- family 1 / opcode `0x90`;
- family 1 / opcode `0x99`.

`LecIsConfirmedForwardOnlyCfDc2110` validates the packed type-3 record list,
record bounds, A5FB payload prefix, family/opcode pair, and companion 85FB
fetch prefix before admitting the request. The old accumulated byte-exact
arrays for opcode `0x81` and `0x84` were removed.

Keep byte-exact or special handling for commands with host-side effects or
incomplete semantics. In particular, family-0 opcode `0x85` must remain
special because the legacy driver changes host interrupt-mask state before
forwarding.

The last pre-change trace, `xstream_trace_20260927_023951.jsonl`, ended with
five rejected opcode-`0x84` selector-`0x0C` requests, five selector-`0x0D`
requests, and one family-1 opcode-`0x81` selector-`0x0E` request. These no
longer require individual whitelist entries.

## Latest staged trace: 2026-09-27 02:50

The semantic forwarding gate worked as intended: the next trace no longer
stopped on additional opcode-`0x81`/`0x84` payload variants.

Trace `xstream_trace_20260927_025005.jsonl` contains 427 CFDC2110 calls:
416 successful and 11 rejected. The final blocked requests are:

- family 0 / opcode `0x42`: 5 identical requests;
- family 0 / opcode `0x92`, selector 1, offset `0xC0`, value
  `0xFFFFFFFF`: 5 requests;
- family 0 / opcode `0x92`, selector 1, offset `0xC4`, value
  `0x00000460`: 1 request.

Static legacy semantics:
- `FUN_00015ACC` implements family-0 opcode `0x42` as a local JTAG write
  loop through BAR1 JTAGNUM/JTAGDAT and creates a local status response.
- `FUN_0001600E` implements family-0 opcode `0x92` as direct MMIO write.
  Selector 1 is confirmed as BAR1 because the observed offsets are exactly
  GPIODIR `0xC0` and GPIODAT `0xC4`.

The x64 driver now implements opcode `0x42` structurally and opcode `0x92`
selector 1 as an aligned BAR1 write with BAR resource bounds checked at runtime.
Selectors 0 and 2 remain gated until their legacy base mappings are confirmed.

## Latest staged trace: 2026-09-27 02:56

Trace `xstream_trace_20260927_025600.jsonl` contains 421 CFDC2110 calls:
420 successful and exactly one rejected. The previous family-0 opcode-`0x42`
JTAG and opcode-`0x92` BAR1-write implementations therefore pass this startup
stage.

The sole blocker is family 2 / opcode `0x02`, observed with body byte
`0x00`.

Legacy `FUN_000163B2` accepts body 0 or 1, optionally waits on an active
internal transfer, and writes 0/1 to BAR1 offset `0x80` (MTTCTL), then
creates a local status response. The x64 implementation now maps the optional
wait to `CurrentTransfer->CompletionEvent` and implements both valid body
values structurally.

## Latest runtime state: startup succeeds, probe/arm failures remain

Trace `xstream_trace_20260927_025927.jsonl` corresponds to the first run where
XStream reaches its main UI without the previous startup error dialog.

Application-visible failures:
- "Problems reading probe Ch1-Ch5"
- "unable to arm acquisition board" when selecting Auto or Single trigger.

Trace correlation:
- five consecutive rejected family-0 opcode-`0x90` requests strongly align
  with the five probe-channel errors;
- repeated family-0 opcode-`0xA0` requests occur in the later trigger/arm
  sequence;
- the final rejected opcode-`0x85` uses control word `0x02A0`;
- there is no CFDC2138 or CFDD219F call, so the arm failure currently happens
  before the gated DMA acquisition launch path.

Static legacy paths:
- opcode `0x90` -> `FUN_00015E80` -> helper object at `this+0x19`,
  control/data MMIO helper chain; exact BAR mapping still unproven.
- opcode `0xA0` -> `FUN_00015FD8` -> direct register object at
  `this+0x17A`; exact MMIO target still unproven.
- opcode `0x85` -> `FUN_00016962` host interrupt-mask update + generic
  board transport. The x64 gate now accepts structurally valid decoded
  opcode-`0x85` requests, not only the earlier exact packet.

`ghidra_scripts/targets.txt` now includes the opcode-`0x90`/`0xA0` helper
functions needed for the next mapping pass.

## Opcode 0xA0 mapping resolved

The latest Ghidra export proves the family-0 opcode-`0xA0` register mapping.

The CFDC2110 command dispatcher object is located at board-object offset
`+0xEA8`. `FUN_00015FD8` accesses dispatcher field `+0x17A`, which lands
at board-object offset `+0x1022`. `FUN_00014847` explicitly stores the
PFREG register object at `+0x1022`; PFREG is BAR1 offset `0xF4`.

Therefore opcode `0xA0` is now implemented as a direct 16-bit PFREG write
with a local legacy status response.

The remaining probe path, family-0 opcode `0x90`, uses dispatcher field
`+0x19`. Its helper layout matches the SPI helper initialized by
`FUN_0001340C` (active byte, CTL/DAT/DIN pointers, control shadow), but the
pointer assignment still needs a direct constructor proof before enabling MMIO
writes. Ghidra targets now include the command-object constructor region around
`0x159E2` and neighboring raw addresses.

## Probe path resolved

The constructor proof is now complete for family-0 opcode `0x90`.

- CFDC2110 dispatcher object = board `+0xEA8`.
- `FUN_000158EE` stores its second constructor parameter at dispatcher
  `+0x19`.
- `FUN_00014212` passes board `+0x106A` as that parameter.
- `FUN_00014847` initializes board `+0x106A` with `FUN_0001340C` using
  BAR1 SPICTL `0xA0`, SPIDAT `0xA4`, SPIDIN `0xA8`.
- Initial legacy SPI control shadow is `0x001FF000`.

Therefore family-0 opcode `0x90` is the local SPI helper path used by the
probe sequence. The x64 driver now implements the decoded selector handling,
SPI control-shadow updates, 16-bit-word bit reversal and SPIDAT writes.

The captured probe request (selector `0x0E`, 144 bits, nine words) matches the
legacy loop exactly.

Family-0 opcode `0xA0` is independently proven as PFREG BAR1+`0xF4` and is
already implemented.

Next runtime validation should focus on whether "Problems reading probe
Ch1-Ch5" disappears and where Auto/Single arm proceeds after these two paths.

## Latest runtime trace: 2026-09-27 09:48

Trace `xstream_trace_20260927_094813.jsonl` contains 460 CFDC2110 calls:
449 success and 11 rejected.

Important validation:
- family-0 opcode `0x90` SPI/probe path: 6 calls, all successful;
- family-0 opcode `0xA0` PFREG path: 5 calls, all successful;
- the previous probe/PFREG blockers are therefore cleared at driver-protocol
  level.

Remaining blockers:
- one family-1 opcode-`0x42` mode-1 58-bit JTAG request;
- ten family-1 opcode-`0x96` requests.

Legacy `FUN_000165A6` proves the pure family-1 forwarder set:
`4A,81,82,90,91,96,97,99`. The x64 semantic gate now admits the complete set.

Legacy `FUN_00015C7E` may read JTAG chunks across the current packed record
boundary into the following record. The x64 JTAG path now reproduces this
behavior only within the bounds of the fully validated complete IOCTL buffer.
This removes the remaining payload-specific mode-0/1 JTAG whitelist behavior
without introducing out-of-buffer reads.

Next runtime test should show whether Auto/Single reaches the acquisition IOCTL
path after these arm-stage commands.

## Latest runtime trace: 2026-09-27 09:58

Trace `xstream_trace_20260927_095816.jsonl` contains 535 CFDC2110 calls:
524 success and 11 rejected. Every rejection is family 0 / opcode `0x4A`
(selector 0 x5, selector 1 x5, selector 2 x1).

No CFDC2138 or CFDD219F acquisition launch is reached before this blocker.

Legacy `FUN_00016A66` proves family-0 opcode `0x4A` is a pure generic
board forwarder via `FUN_00016168(...,1)`. The complete statically confirmed
family-0 forward-only set is now admitted semantically:
`4A,84,86,87,96,97,A1,A2`.

Previously implemented family-0 opcode `0x90` SPI/probe and opcode `0xA0`
PFREG paths continue to succeed in this trace.

## Latest runtime trace: 2026-09-27 10:09

Trace `xstream_trace_20260927_100938.jsonl` contains 790 CFDC2110 calls:
779 successful and 11 rejected.

The rejected set is now:
- family 2 / opcode `0x10`: 1;
- family 2 / opcode `0x05`: 1;
- family 2 / opcode `0x01`: 3;
- CFDC2110 type-1 records: 6.

No CFDC2138 or CFDD219F is reached yet.

Newly implemented:
- family2/0x10 -> BAR1 LEDCTL 0xE0, value from two boolean request bytes;
- family2/0x05 -> BAR1 ITMODE pulse 7 then 3;
- CFDC2110 type 1/2 records -> clear GPIODAT bit16, stream 16-bit payload
  values to MAMDAT 0x40, then MAMPGO 0x44 with
  `((type & 3) << 8) | count`.

Captured type-1 payload length is 42 bytes = 21 words, so MAMPGO=0x115.

Family2/0x01 remains gated. Legacy `FUN_0001621A` controls an internal
restartable timer object at dispatcher+0x186 and is observed with 1 ms and
100 ms requests. `ghidra_scripts/targets.txt` now includes `157c4` so the
timer object's implementation can be exported next.

## Family-2 opcode 0x01 resolved; MAM write format corrected

The latest Ghidra export adds `FUN_000157C4`, proving that the object used by
family-2 opcode `0x01` is a Windows notification KTIMER wrapper.

`FUN_0001621A` behavior:
- command body byte0 must be 1;
- next DWORD is requested duration in milliseconds;
- zero-duration is normalized to 1 ms;
- poll timer with zero timeout;
- if signaled, arm a new relative one-shot timer;
- if still pending, compute elapsed/remaining time and re-arm only when the new
  requested duration exceeds the remaining interval;
- protocol response is local and immediate.

The x64 driver now reproduces this behavior with a device-owned KTIMER.

Important MAM correction:
`FUN_00012F30` creates temporary entries containing both payload value and
entry index. `FUN_00017BC8`/`FUN_000179E2` write the complete DWORD to
MAMDAT:
`(index << 16) | value`.
The previous raw-16-bit write implementation was corrected before hardware
testing.

Next hardware test may now progress beyond family2/0x01 and the six type-1 MAM
configuration records.

## Build fix after timer implementation

The first WDK build after adding family-2 opcode-`0x01` failed because
`KeQuerySystemTime` was not available as a linkable symbol in the current
WDK environment.

The timer implementation only needs a monotonic elapsed-time source, not wall
clock time. It now uses `KeQueryInterruptTime()`, which returns 100-ns units
suitable for the legacy remaining-time calculation.

The unrelated C4456 warning in the CFDC2110 validator was also cleaned up by
renaming the inner opcode-`0x92` register offset variable.

## Second timer build fix

The current WDK also did not expose `KeQueryInterruptTime` as a linkable symbol in this project configuration. The family-2 opcode-`0x01` timer helper now uses `KeQueryPerformanceCounter(&frequency)` for monotonic elapsed-time measurement. The legacy timer due time itself still uses the normal relative 100-ns `KeSetTimer` interval.


## Latest runtime state: board arms, measurement data absent

Trace `xstream_trace_20260927_104718.jsonl` is the first capture where the
application no longer reports "unable to arm acquisition board".

- 1033 CFDC2110 calls, all successful.
- Only three IOCTL failures remain in the entire trace:
  - `0x00222400`, out=4;
  - `0x00223004` QUERY_BUFFER_A, out=4;
  - `0x00223040` QUERY_BUFFER_B, out=4.
- Both buffer queries occur immediately after event registration.
- No CFDC2124 transfer registration, CFDC2138 acquisition, or CFDD219F
  acquisition call occurs afterward.

Current hypothesis, pending static proof: the missing four-byte buffer/query
values prevent XStream from entering the registered transfer/acquisition path,
which explains why arming succeeds but no waveform data appears.

Ghidra targets now include raw DeviceControl windows around 0x10FC0-0x11080 and
candidate handlers 0x128BC, 0x12ADA, 0x12A5E, 0x12CAC, 0x12C18, 0x12D24 to map
these IOCTLs exactly before implementing them.

## Buffer-query IOCTL dispatch resolved

The DeviceControl export after the 10:47 runtime trace resolves the two
four-byte query IOCTLs:

- 0x00223004 -> FUN_00012A5E;
- 0x00223040 -> FUN_00012C18.

For a 4-byte output buffer, both return a size DWORD:
- 0x223004 reads the first DWORD from a driver-owned buffer pointer at
  board-object offset +0x11EA;
- 0x223040 calls FUN_00012386 on the object at +0x11EE and returns that
  serialized size.

These are not user pointers/handles. Their concrete values are produced by
legacy buffer/list helper code and must still be exported before implementation.
Ghidra targets now include 12386, 123B4, 124C2, 1259A, 12290, 17A98 and 120FA
to resolve those size producers and serialization helpers.

## Buffer-query values implemented

The final Ghidra helper exports resolve the exact four-byte query values:

- 0x00223004 -> 0x00000110
  - FUN_0001329E allocates a 0x110-byte trace-control block and stores 0x110
    in its first DWORD.
  - FUN_00012A5E returns that first DWORD for a four-byte output query.

- 0x00223040 -> 0x00002CAE
  - FUN_00012386 returns (maxInserted + 1) * 0x10A.
  - Static insertion references show 43 distinct register objects are added to
    the legacy register list.
  - 43 * 0x10A = 0x2CAE.

The x64 driver now returns these exact values for the two startup queries.

The remaining unsupported 0x00222400 is still intentionally unmapped. The next
runtime trace should determine whether satisfying the two size queries causes
XStream to proceed to CFDC2124 transfer registration and the acquisition
IOCTLs.

## Latest runtime state: calibration passes, acquiring waits

Trace `xstream_trace_20260927_112352.jsonl`:
- 924 CFDC2110 calls, all successful.
- XStream now progresses past "Calibrating".
- UI then remains at "Acquiring" with no waveform data.

The four-byte query stage is fixed:
- 0x223004 -> 0x110;
- 0x223040 -> 0x2CAE.

XStream then performs second-stage full-buffer reads:
- 0x223004 with out=272 -> currently STATUS_INVALID_BUFFER_SIZE;
- 0x223040 with out=11438 -> currently STATUS_INVALID_BUFFER_SIZE.

No CFDC2124 / CFDC2138 / CFDD219F follows. These second-stage buffers are
therefore the current leading blocker before transfer registration/acquisition.

Static payload facts:
- 0x223004 full response is a 0x110-byte CKeTraceControl descriptor block.
- 0x223040 full response is 43 serialized register entries at 0x10A bytes per
  entry, total 0x2CAE bytes.
- serialized register entry layout is:
  name[256], BAR byte, offset DWORD, type byte, data DWORD.

Ghidra targets now also include 11A00, 13F70 and 19362 to close the remaining
register-list/trace-control serialization details before implementing the full
buffers.

## Full legacy query payload stage implemented

The x64 driver now implements both stages of 0x00223004 and 0x00223040.

0x223004:
- out=4 -> 0x110;
- out=0x110 -> exact reconstructed CKeTraceControl block:
  size=0x110, enabled=1, name="CKeTraceControl", type=2, trailing DWORD=0.

0x223040:
- out=4 -> 0x2CAE;
- out=0x2CAE -> 43 entries x 0x10A bytes.
- entry layout: name[256], BAR byte, DWORD offset, type byte, DWORD data.
- order and type bytes are reconstructed from FUN_00014847, FUN_00014212,
  FUN_000174F2 and FUN_0001785B.
- type-2 entries use initial shadow value 0; types 0/1/4 are refreshed from
  MMIO; INTEN uses InterruptEnableShadow.

Relevant implementation commits:
- ca931925... full payload serializers;
- d12f0633... remove CRT strlen dependency for kernel linking.

Next trace should show whether CFDC2124 / CFDC2138 / CFDD219F finally appear
after the full payload queries succeed.

## Latest trace: full query buffers pass; two new ABI forms implemented

Trace `xstream_trace_20260927_114152.jsonl`:
- 1102 IOCTL events;
- 1072 CFDC2110 calls, all successful;
- 0x223004 size/full payload reads all succeed;
- 0x223040 size/full-list reads all succeed.

Only three failures remain:
- 0x00222400 out=4 (still unmapped);
- 0x00223000 in=0x108 out=0;
- 0x00223040 in=4 out=0x10A.

New static mappings:
- 0x223000 -> FUN_00012ADA -> FUN_00012290(traceControl,index,level).
  This is software trace verbosity only, no hardware side effect.
- 0x223040 indexed form -> FUN_000124C2; input DWORD is register index and
  output is exactly one refreshed 0x10A-byte serialized register entry.

Both new forms are implemented in commit 766d6c896208279cc69d777cb534d47657da4446.

No CFDC2124 / CFDC2138 / CFDD219F is reached in this trace yet. The next
hardware run will determine whether the only remaining startup failure,
0x00222400, is still gating acquisition.

## Latest trace: one remaining failure

Trace `xstream_trace_20260927_114916.jsonl`:
- 959 IOCTL events;
- 937 CFDC2110 calls, all successful;
- 0x223000 succeeds;
- all 0x223004 forms succeed;
- all 0x223040 forms succeed, including indexed in=4/out=0x10A.

Exactly one IOCTL still fails:
- 0x00222400, in=0, out=4 -> STATUS_INVALID_DEVICE_REQUEST.

No CFDC2124 / CFDC2138 / CFDD219F appears after it.

The current DeviceControl raw export has a gap between 0x10F8F and 0x11018.
Because 0x222400 is lower than the already mapped 0x222C00 branch, its
dispatch must be in or before this missing region. Ghidra targets 11008,
11010 and 11014 were added in commit
3621ffa4b7e2c1800a1fb86aae1410955a9cc67e to close this final dispatcher gap.

## Current blocker corrected: repeated family-1 opcode 0x42 JTAG poll

Do not treat 0x00222400 as the primary blocker anymore. The complete legacy
DeviceControl switch also lacks an explicit 0x222400 branch, so this is likely
a tolerated probe.

The actual visible stall in trace `xstream_trace_20260927_114916.jsonl` is an
endless CFDC2110 family-1 opcode-0x42 JTAG scan:
- mode 1;
- requested data bytes 10;
- 76 bits;
- five chunks.

Current x64 response data words:
`0000 4014 0030 5000 0020`

XStream immediately repeats the same transaction.

Legacy FUN_00015C7E calls FUN_0001586E, whose raw assembly proves it reads
JTAGDIN and writes the DWORD through a caller-provided pointer. Ghidra loses
that data flow in the C decompilation, so the exact extraction into 16-bit
response words still needs raw assembly from inside FUN_00015C7E.

Targets 15CC0, 15CF0, 15D20, 15D50 and 15D70 were added in commit
5160c65886d9e179af2986496d1f742db2ed86d1.

## Current fix before next trace: MTTCTL must wait on command timer

Full asm for FUN_00015C7E proves the JTAG response extraction itself matches
legacy:
- 16-bit chunk -> JTAGDIN >> 16;
- final partial chunk -> JTAGDIN >> (32 - bits).

The repeated mode-1 76-bit JTAG poll is therefore waiting on a hardware state
that should have been established earlier.

Critical mismatch found in family-2 opcode 0x02:
- legacy FUN_000163B2 waits on the KTIMER armed by opcode 0x01 before writing
  MTTCTL;
- previous x64 code incorrectly waited on CurrentTransfer->CompletionEvent;
- before transfer registration this effectively removed the required delay.

Fixed in commit 45ca142cce4b25f18eccd029251726314d9c6759:
- opcode 0x02 now waits on LegacyTimer before MTTCTL;
- packed type-1/type-2 MAM records now always use mode 1, matching FUN_00012F30.

Next trace should show whether the 76-bit JTAG status finally changes and
XStream advances toward CFDC2124 / acquisition IOCTLs.

## Latest fix: suppress duplicate MAMDAT writes

Trace `xstream_trace_20260927_121112.jsonl` still stalls in the same
family-1 opcode-0x42 76-bit JTAG poll after the timer-barrier fix.

Immediately before the poll:
- seq 893-898: six type-1 MAM records;
- seq 899-904: exact duplicate of those six records;
- seq 905: MTTCTL=1;
- seq 906+: repeated JTAG status scan.

Legacy FUN_000179E2 keeps a per-index 16-bit shadow and does not write MAMDAT
again when the indexed value is unchanged. Previous x64 code always rewrote
the values.

Implemented:
- device extension now has LegacyMamShadow[256] and valid flags;
- LecApplyMamConfigRecord writes MAMDAT only when index data changes;
- MAMPGO is still written every record;
- type1/type2 records remain fixed to legacy mode 1.

Relevant commits:
- 46fc30c660724e3c1cd7c5c172c3bb1d9b62ef82
- 6c84c1860f376cc51bb2a1a5ae38f40269f75cdf

Next trace should show whether avoiding the second identical MAMDAT programming
block allows the post-MTTCTL JTAG status to advance.

## Exact MAM shadow constructor state recovered

Trace `xstream_trace_20260927_121601.jsonl` is still stuck in the same
post-MTTCTL family-1 opcode-0x42 JTAG poll.

Important static correction:
- MAMDAT register object is constructed by FUN_00011A00 at board +0x368.
- FUN_00011A00 initializes 256 shadow DWORDs to 0xFFFFFFFF.
- FUN_000179E2 compares only the low 16-bit value.
- Effective initial shadow for every MAM index is therefore 0xFFFF.

Previous x64 code used a separate invalid state and always emitted the first
write. That is not legacy-compatible for initial value 0xFFFF.

Fixed in:
- 393f6c10b066898b2f42a348e448bbc95b0e235c
- 8d48f6b13c1b9a541b338de23486015191eb63ad

The driver now initializes the MAM shadow lazily to 0xFFFF for all 256 indices
and suppresses writes exactly as legacy does.

## Current primary fix: continuous JTAG scan sequencing

Trace `xstream_trace_20260927_122739.jsonl` remains stuck in the same
family-1 opcode-0x42 76-bit JTAG scan. The MAM 0xFFFF constructor correction
does not affect the six pre-poll records because they contain no 0xFFFF values.

Critical mismatch found in JTAG implementation:

Legacy FUN_00015C7E / FUN_00015ACC:
- write JTAGNUM once for all complete 16-bit chunks;
- stream JTAGDAT for each chunk without rewriting JTAGNUM;
- only reprogram JTAGNUM for the final partial chunk.

Previous x64 implementation:
- rewrote JTAGNUM before every 16-bit chunk.

This can restart the hardware shift state and break a continuous 76-bit scan.

Fixed in commit:
- 3d0070e5328e3ae19412728cd644fb53bd1161bb

Both LecJtagExecute and LecJtagWriteOnly now match legacy chunk sequencing.
Next trace should show whether the repeated 76-bit status response finally
changes and XStream advances.

## Latest trace and legacy reference strategy

Trace `xstream_trace_20260927_123731.jsonl`:
- 962 IOCTLs;
- 961 success;
- only 0x00222400 fails;
- 937 CFDC2110;
- still no CFDC2124 / CFDC2138 / CFDD219F.
- repeated family-1 opcode-0x42 76-bit response is unchanged:
  `0000 4014 0030 5000 0020`.

XStream polls several times, calls DELAY_MS(10), and resumes the same scan.
This is an intentional status wait.

Need a legacy-driver reference response instead of continuing blind host-side
changes.

New lecdiag commands:
- raw-ioctl <code> <input-hex> <output-bytes>
- legacy-jtag-poll

`legacy-jtag-poll` sends the exact current CFDC2110 76-bit request and prints
the 24-byte result. It uses only the real legacy ABI, not x64 private debug
IOCTLs.

Build helper now accepts:
`./scripts/build-lecdiag.ps1 -Architecture x86`
and produces an x86 binary under tools/lecdiag/build/x86 for use with the
original 32-bit driver.

Relevant commits:
- ecd1ab6b087ca3deb60736208553b7e76ab189cb
- e306a28cf8190ec0b73b13347e7ee422119c0687

## lecdiag x86 build verification

A user observed that running `./scripts/build-lecdiag.ps1 -Architecture x86`
printed an output path under `tools/lecdiag/build/lecdiag.exe` and the resulting
binary failed on 32-bit Windows with "not a valid application for this OS
platform". The current repository script should instead emit
`tools/lecdiag/build/x86/lecdiag.exe` for x86 builds.

To make architecture mistakes explicit, commit
`9080db419d654cac8816f8c86002fabf17d55bb0` adds PE-header verification after
the build. The script now checks the Machine field and requires:
- x86: 0x014C
- x64: 0x8664

It also prints the verified architecture and PE machine value. If a user still
sees the old output path, their local checkout/script is stale or locally
modified; pull/reset before rebuilding.


## Legacy lecdiag device opening

On the original 32-bit system, the x86 lecdiag binary ran correctly but
`CreateFile(\\\\.\\ALADDINAcqDriver0)` returned ERROR_FILE_NOT_FOUND.
This confirms the fixed DOS compatibility link used by the replacement driver
is not necessarily published by the legacy driver.

Commit `fffd5209ab993e3162a4f503c899629ab2d0efe1` changes lecdiag to enumerate
all four recovered LeCroy PnP interface GUIDs through SetupAPI first and open
the first present interface. The fixed `\\\\.\\ALADDINAcqDriver0` path remains
as a fallback for the replacement driver.

Rebuild the x86 diagnostic after pulling and retry `lecdiag.exe build` then
`lecdiag.exe legacy-jtag-poll` on the original driver system.


## Legacy reference JTAG response obtained

The x86 lecdiag probe now works with the original 32-bit driver.

Exact repeated 76-bit poll response:
- legacy full output:
  `000000000000000000000C00000000144020000002002000`
- current x64 full output:
  `000000000000000000000C00000000144030000050002000`

Five little-endian JTAG words:
- legacy: 1400 2040 0000 0002 0020
- x64:    1400 3040 0000 0050 0020

Matching: words 1, 3, 5.
Different: word 2 (2040 vs 3040), word 4 (0002 vs 0050).

This is strong evidence that JTAG framing/extraction is now correct and that
upstream device state differs before the poll. Do not spend more time changing
JTAG response packing without new evidence.

Next investigation should identify the state-setting operations before this
poll, ideally by observing the original driver's complete startup IOCTL stream
or by controlled direct probes.

## Legacy XStream state transition

Direct legacy poll is stable before XStream:
- 1400 2040 0000 0002 0020

After original XStream starts:
- 1400 2040 0000 0000 0020

Current x64 stalled state:
- 1400 3040 0000 0050 0020

So original XStream clears legacy word4 bit 0x0002 while word2 stays 0x2040.
The x64 path has three additional differing status bits: word2 0x1000 and
word4 0x0010/0x0040.

This proves the remaining issue is upstream hardware state, not response
packing.

New diagnostic in commit 7ab94af4ed7216e68270d5220e68174c6935f252:
- `lecdiag legacy-prepoll-replay`
- replays the exact immediate pre-poll x64 CFDC2110 sequence;
- after every step, sends the known 76-bit legacy JTAG poll and prints five
  words;
- intended to run on the original 32-bit driver from a fresh board state before
  XStream.

Use this to identify the first command that changes legacy status bits.

## Pre-poll replay isolates current divergence to MTTCTL transition

Fresh-board legacy replay results:

Initial:
- 1400 2040 0000 0002 0020

Step 8, first family0/op42 JTAG write:
- word2 changes 2040 -> 3040
- resulting state: 1400 3040 0000 0002 0020

Steps 9-32:
- unchanged

Step 33, family2/op02 MTTCTL=1:
- word4 changes 0002 -> 0000
- resulting state: 1400 3040 0000 0000 0020

Current x64 stalled state:
- 1400 3040 0000 0050 0020

Therefore word2=3040 is NOT itself a bug. The first JTAG write legitimately
sets that bit under the original driver too.

The actual current divergence is the MTTCTL transition:
- legacy after MTTCTL=1: word4=0000
- x64 after MTTCTL=1 / poll: word4=0050

User heard relays switch during the replay, confirming real hardware state
changes.

Focus next on exact family2/op02 implementation and preconditions. Stop spending
time on JTAG response packing unless new evidence appears.

## x64 replay was not from a fresh board baseline

The x64 `legacy-prepoll-replay` run started already at:
- 1400 3040 0000 0050 0020

and stayed there for all 33 steps.

Do NOT infer from this run that MTTCTL in the x64 handler causes 0x0050. The
board was already in the divergent state before replay and may retain FPGA/JTAG
state across driver reload/restart.

Static re-check confirms LecRunLegacyStartupProbe matches legacy FUN_00012FDE
for START, delays, buzzer pulses and ITMODE 7->3.

Next required discriminator:
1. fully power-cycle the scope/hardware;
2. ensure XStream never starts;
3. load x64 driver;
4. immediately run `lecdiag legacy-jtag-poll` only.

If cold x64 baseline is 1400 2040 0000 0002 0020, the bad state is created by
later XStream commands and persisted into the previous replay.
If cold x64 baseline is already 1400 3040 0000 0050 0020, investigate driver
start / board reset state before any XStream IOCTLs.

## True cold x64 baseline is all-ones JTAG readback

After full power-off/power-on, before XStream, the first x64
`legacy-jtag-poll` returned full output:
`000000000000000000000C00000000FFFFFFFFFFFFFFFFFF0F`

So the fresh x64 board does NOT begin at the legacy fresh baseline
`1400 2040 0000 0002 0020`.

This means the replacement driver's startup path does not establish all of the
legacy board/JTAG state. The previously observed x64 state
`1400 3040 0000 0050 0020` must arise later.

Next test must be clean:
1. full hardware power cycle;
2. no XStream;
3. do not run `legacy-jtag-poll` or other diagnostic first;
4. immediately run `lecdiag legacy-prepoll-replay`.

Use the replay output to identify the first command that changes the x64
all-ones cold state.

## Cold x64 replay reaches legacy final status exactly

After full power cycle, x64 replay initial state:
- 1400 2000 0000 0000 0020

Legacy replay initial:
- 1400 2040 0000 0002 0020

But after step 8 x64 reaches:
- 1400 3040 0000 0000 0020

and remains there through step 33.

Legacy after step 33 is exactly:
- 1400 3040 0000 0000 0020

This is a major discriminator: the controlled x64 replay can establish the
correct final pre-poll board state. Therefore the basic GPIO/SPI/JTAG/MAM/
MTTCTL implementations are not sufficient to explain the real XStream stall at:
- 1400 3040 0000 0050 0020

Focus next on what differs between controlled replay and real XStream startup:
earlier commands/state, packed-record batching, timing/timer interactions,
other IOCTLs/events, or ordering outside the extracted immediate pre-poll
sequence.

## 2026-09-27 trace after correct manual board state

Trace: xstream_trace_20260927_151006.jsonl.

Before XStream, controlled x64 replay had reached the correct final state:
- 1400 3040 0000 0000 0020

During real XStream startup it is driven back to:
- 1400 3040 0000 0050 0020

The known 76-bit poll repeats 46 times with exactly that response.

Trace stats:
- 962 IOCTLs;
- CFDC2110 = 933;
- only failure remains seq2 0x00222400 / C0000010;
- no CFDC2124, CFDC2138 or CFDD219F.

Conclusion: the immediate pre-poll block itself can work correctly, but some
earlier part of real XStream startup establishes the bad word4=0x0050 state.
Investigate pre-sequence commands before seq879.

UI correction: the old startup "Beenden" error dialog has not appeared for some
time. Current visible warning says Channel 1-5 probes cannot be read and only
offers OK. Treat that separately; it may be ProbeBus/ProBus-related, but the
trace does not yet prove a causal link to the JTAG poll.

## Preferred next step: passive legacy XStream IOCTL hook

Instead of more active probes, capture the original XStream<->driver traffic in
user mode on the 32-bit legacy system.

Recommended implementation:
- x86 instrumentation DLL injected into 32-bit XStream;
- hook DeviceIoControl, optionally CreateFileA/W and CloseHandle;
- forward every call unchanged;
- log exact input bytes before the call and exact output bytes / bytesReturned /
  GetLastError after it;
- include sequence, timestamp, thread id, IOCTL code, sizes and duration;
- use a JSONL format compatible enough with current x64 trace tooling to allow
  direct diffing.

This is preferable to ProcMon (insufficient payload visibility) and to manual
WinDbg breakpoints (awkward for hundreds of calls). It leaves the original
kernel driver untouched and provides ground-truth legacy traffic.



## x86 original-XStream IOCTL tracer implemented

New tool:
- `tools/xstream-ioctl-trace/xstream_trace_launcher.c`
- `tools/xstream-ioctl-trace/xstream_io_hook.c`
- `scripts/build-xstream-ioctl-trace.ps1`

Purpose: capture original 32-bit XStream's real user-mode DeviceIoControl
traffic while it talks to the untouched legacy kernel driver.

The x86 launcher starts XStream suspended, injects `xstream_io_hook.dll`, then
resumes the process. The hook patches import tables for DeviceIoControl,
CreateFileA/W, CloseHandle, LoadLibraryA/W and GetProcAddress and forwards calls
unchanged.

JSONL records include exact input bytes before each call and returned output
bytes for synchronous calls, plus IOCTL code, sizes, thread id, duration,
GetLastError, handle, and pending/overlapped state. Buffers are capped at 1 MiB.

Build:
`.\scripts\build-xstream-ioctl-trace.ps1`

Run on legacy system:
`.\xstream_trace_launcher.exe "C:\Program Files\LeCroy\XStream\lecroyxstreamdso.exe"`

Use the resulting legacy trace as the primary source for locating the first
semantic difference versus current x64 startup before sequence ~879.

## Legacy XStream user-mode IOCTL tracer implemented

A dependency-free x86 tracer now exists in `tools/xstream-ioctl-trace/`:
- `xstream_trace_launcher.exe`: starts 32-bit XStream suspended, injects the hook DLL, explicitly initializes it, then resumes XStream;
- `xstream_io_hook.dll`: IAT-hooks DeviceIoControl, CreateFileA/W, CloseHandle, LoadLibraryA/W and GetProcAddress and writes JSONL.

Build:
`./scripts/build-xstream-ioctl-trace.ps1`

Output:
- `tools/xstream-ioctl-trace/build/x86/xstream_trace_launcher.exe`
- `tools/xstream-ioctl-trace/build/x86/xstream_io_hook.dll`

The DLL no longer performs heavy initialization from DllMain. Commit
`f05fda631065f5c546d03e5e05e89bc85ef78f30` exports
`InitializeXStreamTrace`; commit
`b0ca90b4b9d1180c6049215497bd776c1a2b102d` makes the launcher invoke that
initializer in the suspended target before resuming XStream.

Captured ioctl records include exact input/output bytes, sizes, success,
GetLastError, bytes returned, thread id, duration and pending state. Async
completion output is not captured yet. The goal is passive ground-truth tracing
of the original XStream<->legacy-driver startup path, especially before the
x64 trace's seq879 pre-poll block.


## x86 tracer initializer export fix

First legacy-system launch failed with:
`GetProcAddress(InitializeXStreamTrace) failed: 127`.

Cause: x86 `WINAPI` / `__stdcall` name decoration exported the initializer as
`_InitializeXStreamTrace@4` instead of the undecorated name expected by the
launcher.

Fixes:
- commit `78fc712932139bcf6646af07e5d0e7b4bb021330` adds an x86 linker export alias so the DLL exports stable name `InitializeXStreamTrace`;
- commit `a0482af7c375578d02b3d1ff1d72b1d1eddd817d` makes the launcher fall back to decorated name `_InitializeXStreamTrace@4` for robustness.

Rebuild with `./scripts/build-xstream-ioctl-trace.ps1` and copy both rebuilt x86 files to the legacy system before retrying.


## First passive legacy trace did not see LeCroy IOCTLs through DeviceIoControl

Trace: `legacy_xstream_trace_20260927_165243.jsonl`.

Observed:
- 2014 Win32 DeviceIoControl calls;
- five distinct codes, all 0x004708xx;
- payloads contain HID device paths;
- zero CFDCxxxx / 0022xxxx LeCroy IOCTLs.

So injection/IAT logging works, but the LeCroy path bypasses the hooked Win32
DeviceIoControl surface.

Commit `994a875641e88adb9f3bcec5a3b8c1ed91fbca8d` adds interception of
`ntdll!NtDeviceIoControlFile` and logs native request/response data as
`type=nt_ioctl` records. Rebuild the tracer and recapture before considering
more invasive methods.

## CPU-frequency sensitivity is potentially relevant

Reference 32-bit system: Core i5-3450. XStreamDSO becomes unreliable when CPU
clock rises above the minimum setting, with acquisition-board / trigger-level /
driver errors. The 32-bit Windows install is therefore normally capped at 5%
maximum processor state. The x64 environment has been running at 100%.

Treat this as an important timing clue. Before concluding that a remaining x64
startup mismatch is semantic, repeat a controlled x64 run with the processor
maximum state matched to the legacy system.

Tracer overhead was also real. The initial hook flushed every JSONL record with
FILE_FLAG_WRITE_THROUGH + FlushFileBuffers, while duration_us measured only the
kernel call before logging. Commit
668133737be58adc7f209aa3d126ee5aaaad9c9b switches to buffered logging and a
flush every 256 records plus detach.

Native legacy trace 20260927_185222 is successful and contains real LeCroy
traffic: about 9158 CFDC2110, 1984 CFDC2138, 90 CFDC2124, and 74 CFDC2128 calls.


## Passive trace overturned early startup assumptions

Ground-truth legacy trace: `legacy_xstream_trace_20260927_185222.jsonl`.
Comparator added: `tools/trace-diff/compare_xstream_traces.py`, commit
`6619c9527511c1cb5abd1177a0c0327427cadbd7`.

Earliest proven divergences vs x64 trace 20260927_151006:
- 0x00222400 succeeds on original (STATUS_SUCCESS, Information=0); x64 used to
  return C0000010.
- x64 then shows three CFDC21C4 BAR0+0x0C writes values 4,2,1 which are absent
  from original stream.
- 0x00223004 legacy name is exactly "CKeTraceControl: ".
- 0x00223040 original list order begins TxControl, RxControl, TxCount, RxCount,
  SetIRQ, HWInt, then FVER...; x64 previously placed these six at the end.
- original indexed 0x00223040 index 0 returns TxControl.
- INTEN list entry type is 2 in live original output, not 0.
- same family1/op99 request already returns different board payload in x64, but
  retest after correcting earlier control flow before changing transport logic.

Implemented:
- 0x00222400 success no-op;
- exact trace-control name;
- live register-list order and INTEN type.
Driver commit: `9ec5723466f06d3d60d8846b89b919715e79c010`.
ABI constants: `4608e757...` and `d800ecf8...`.

Next: build/load x64 with CPU maximum processor state matched to legacy 5%,
capture a fresh XStream trace, then compare again before touching op99 transport.

## Critical new finding: original LeCroy stack uses multiple handles

Legacy native trace groups LeCroy IOCTL families by distinct handles:
- 0x690: main CFDC2110/acquisition/register path;
- 0x6A4: 0x00223004 / 0x00223000 trace-control;
- 0x698: Dallas + 0x00222400 + Dallas-memory;
- 0x6C4: 0x00223100 three-event registration;
- 0x308 / 0xAF0 / 0xD88: delay/flag 0x00222C00 / 0x00222C04.

This means the earlier conclusion "0x00222400 belongs to the acquisition
DeviceControl switch and should succeed there" is not proven. Static analysis
still found no 0x00222400 branch in that switch. The runtime call succeeds on a
different legacy handle.

Do not collapse these families conceptually until device-path mapping is known.

Commit c3af835647f749564a18c9c589c10b53da349da7 adds NtCreateFile tracing to
xstream_io_hook.dll. Rebuild and recapture legacy XStream. The next trace should
contain type=nt_create_file records mapping native paths to handles, allowing us
to identify whether these are separate device objects/interfaces/drivers.

The live 0x00223040 register-list correction remains valid because it is on the
main handle 0x690 together with CFDC2110.

## NtCreateFile tracer build fix

The first NtCreateFile tracer revision failed to compile because the new
`HookNtCreateFile` body appeared before helper-function declarations. MSVC then
implicitly assumed `int` return types and later reported C2371/C2040 conflicts.

Commit `adaad84fb6ea1dddc76533c43e8163197c03805a` adds explicit forward
declarations for all helper functions used by the early native hook and removes
the redundant misplaced prototype block. Rebuild with
`./scripts/build-xstream-ioctl-trace.ps1`.


## Recovered legacy interface-role map

The format-version-3 passive trace `legacy_xstream_trace_20260927_195608.jsonl`
maps the original native device opens to five interface GUIDs:

- `958695A4-693A-435E-8297-66F805D8E46A`: main acquisition/control endpoint;
- `8D1103B8-5BF4-4B5C-B21E-EEAACE97D418`: Dallas/board identification;
- `9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5`: trace control;
- `FC5DF040-D6CD-4BA0-B5E0-2561972963A2`: three-event registration;
- `7AC34BE9-F766-4F15-9E88-854BA5E2146E`: delay/flag helper.

The x64 driver had only the latter four interface GUIDs. The main
`958695A4-...` acquisition endpoint was missing. It is now registered by
commits `45a4abe9cf08ec055b6b631881ec139575d80ca1` and
`65525e75c557c0e58b685b6c64c1a5d340c8c6aa`.

Test this corrected five-interface configuration before making more JTAG or
transport changes.


## Five-interface retest did not resolve startup loop

Fresh x64 trace `xstream_trace_20260927_201339.jsonl` was captured after
registering the recovered 958695A4 main acquisition interface and with maximum
processor state still at 5%.

Result:
- early corrected ABI behavior is active;
- 930 CFDC2110 calls;
- startup still reaches the same 76-bit family1/op42 poll;
- seq 906 onward repeats 43 times;
- response remains exactly `1400 3040 0000 0050 0020`.

Conclusion: missing 958695A4 registration was a real defect but not the sole
root cause.

Next: use the x86 injected XStream user-mode tracer on the x64 system too. Its
NtCreateFile records can directly prove which GUID/interface paths XStream opens
with the replacement driver and permit an apples-to-apples user-mode comparison
with legacy trace 20260927_195608.

## Correction: main XStream IOCTL stream uses ALADDINAcqDriver0

x64 injected user-mode trace `legacy_xstream_trace_20260927_203017.jsonl`
shows all five PCI interface GUIDs open successfully, including 958695A4.

However, the actual x64 startup CFDC2110/register stream is sent through the
legacy DOS path `\\??\\ALADDINAcqDriver0`, not through the 958695A4 handle.
XStream opens that DOS path at seq 584 (handle 0x5CC) and again at seq 5056
(handle 0xBF4). Those handles carry ~922 CFDC2110, ~512 CFDC21C0, 3 CFDC21C4,
4 00223040 and event controls.

The helper interface roles remain confirmed:
- 8D1103B8 = Dallas;
- 9007C2BC = trace control;
- FC5DF040 = three-event registration;
- 7AC34BE9 = delay/flag.

Treat the previous statement that 958695A4 is itself the main DeviceIoControl
endpoint as superseded. It is definitely a real interface and is opened by
XStream, but may serve discovery/classification while the DOS symbolic link is
the actual acquisition/control path.

Adding 958695A4 therefore could not by itself fix the JTAG startup loop. The
next work should compare byte-for-byte CFDC2110 request/response behavior over
ALADDINAcqDriver0 against the original 32-bit driver.

## Critical endpoint-selection fix: do not expose ALADDINAcqDriver0

Comparing legacy trace 20260927_195608 with x64 user trace 20260927_203017
revealed the first direct control-flow mismatch caused by our replacement:

Original 32-bit XStream probes `\\??\\ALADDINAcqDriver0`, but that open
fails with `STATUS_OBJECT_NAME_NOT_FOUND (0xC0000034)`. XStream then opens
interface GUID `958695A4-693A-435E-8297-66F805D8E46A` and uses that handle for
CFDC2110/acquisition traffic.

Our x64 driver created a DOS symbolic link for ALADDINAcqDriver0, so the probe
succeeded and XStream stayed on a different endpoint path. This explains why
adding the 958695A4 GUID did not help: XStream never needed to fall back to it.

Commit `0534482a6e00463b12420e03a28c83e9465801a6` removes publication of the
DOS alias. Retest x64 before making any further JTAG/transport changes.

Expected next trace: ALADDINAcqDriver0 open fails; main IOCTL stream moves to
958695A4 handle.

## Current blocker after DOS-alias fix: CFDC2190

Trace `xstream_trace_20260927_204139.jsonl` proves that removing the
ALADDINAcqDriver0 DOS alias materially changed startup into the original-style
interface path. The old x64 START writes 4,2,1 disappear.

The next hard divergence is seq 20:
- IOCTL CFDC2190
- input exactly 29 bytes
- input = 0000000002000000FF7F00000000000000000000000000000000000000
- x64 previously returned C0000010 because no handler existed.

Recovered legacy semantics:
- +0x04 DWORD controls global interrupt-mask bit 1;
- +0x08 DWORD is BAR0 ERRM;
- no output.

Implemented:
- 3d393570ee35c1b014c5093b167581c3091d7615
- 803670576886117541848b81c867872814e39a10

Retest this before touching JTAG/transport logic again.

## Current next test: 85FB hardware-response framing fix

Trace `xstream_trace_20260927_210024.jsonl` has no failing IOCTLs; CFDC2190
now succeeds. The earliest remaining semantic mismatch is the first family-1
opcode-0x99 CFDC2110 response.

Original output starts:
`0000000000000000000002000200FFFFFFFF...`

x64 output starts:
`0000000000000200000000000000...`

This exposed a generic framing bug: original 85FB hardware fetches prepend a
6-byte host header (DWORD 0, WORD 2) before raw firmware bytes. x64 copied raw
BAR1 RX bytes directly into the record.

Fixed in commit `33e24638d72bd5ae587ff80d6bebea4702a112cb`.

Retest x64 before changing transport or JTAG logic. If opcode-0x99 still differs
after the framing shift is corrected, focus next on why its raw 256-byte payload
is zeros on x64 versus 0xFF on the original reference.

## Current next retest: generic family-0 opcode 0x88 admission

Trace `xstream_trace_20260927_211141.jsonl` reaches a new XStream state:
"Stopped the Acquisition, Go to service Menu, Internals to Reset the Link".

Root cause in trace:
- seq 334..343: ten identical CFDC2110 calls fail C0000010;
- request = `060006000300FBA5400088001F00080002000300FB854000`;
- this is family0/opcode88, mask 0x001F.

Original legacy trace has the exact same request at seq 3375 and succeeds,
returning `0000000000000000000002000000`.

The opcode88 execution handler already existed. Only the structural safety gate
rejected non-literal mask variants. Commit
`cf90d85d3ee6d13710251ffc4edd6a2735ada579` admits generic recovered opcode88
records with payloadLength >= 6.

Retest x64 before any further JTAG/transport changes.

## Current next retest: 85FB payload-length field

Trace `xstream_trace_20260927_212113.jsonl` is the first run where:
- probe-failure messages are gone;
- link/acquisition error is gone;
- trigger can be started;
- no waveform data appears.

There are still zero CFDC2124 / CFDC2138 / CFDD219F calls. XStream polls
family1/opcode96 2473 times instead.

Exact op96 request:
`06000A000300FBA540019604002000000002080202000300FB854000`

Original vs x64 first 128 response bytes differ at exactly one byte:
original 85FB header length = 0x0202 (514), x64 = 0x0002.

Legacy rule recovered from multiple exact matches:
- 85FB host length normally equals fetch recordOutput - 6;
- op81: 4, op90: 6, op96: 514;
- JTAG follows same rule;
- op99 is a known exception with length 2.

Commit `d51dbe2f5098c2797094b7c1681bf7a2e65d8acd` implements this generic rule
for raw hardware responses while preserving op99's length-2 override.

Retest before touching DMA/acquisition code.

## Current state: trigger starts, probes work, still no acquisition data

After commit `cf90d85d3ee6d13710251ffc4edd6a2735ada579`, fresh trace
`xstream_trace_20260927_212113.jsonl` reaches the best state so far:

- no acquisition-link error;
- no channel probe read failures;
- trigger/acquisition can be started in XStream;
- no waveform data is returned.

Important discriminator: this trace still contains no CFDC2124, CFDC2138, or
CFDD219F calls. Therefore do not start debugging DMA launch yet. XStream has
not reached transfer registration.

The late runtime path is dominated by CFDC2110 family-1 opcode 0x96. Next work
should compare original-vs-x64 opcode-0x96 request/response behavior and identify
the first semantic mismatch that prevents transition into the acquisition
buffer path.

Do not undo the following now-proven fixes:
- no ALADDINAcqDriver0 DOS alias;
- fifth interface GUID 958695A4 registered;
- CFDC2190 implemented;
- 85FB hardware-response host framing fixed;
- generic family-0 opcode 0x88 masks admitted.

## 2026-09-27 runtime checkpoint: Family 1 opcode 0x96

Latest x64 kernel trace: `xstream_trace_20260927_212113.jsonl`.

- XStream now starts acquisition without the previous link-reset or probe-reading failures, but no waveform data appears.
- The trace contains 3,125 `CFDC2110` calls; 2,473 are the identical Family-1 opcode-`0x96` request `06000A000300FBA540019604002000000002080202000300FB854000`.
- Each observed opcode-`0x96` call succeeds at the NTSTATUS layer and reports 526 output bytes.
- The kernel trace records only a 128-byte output preview, not the complete 526-byte response. Full legacy-vs-x64 semantic comparison therefore requires the user-mode XStream trace (or another complete response capture).
- `CFDC2124`, `CFDC2138`, and `CFDD219F` remain absent. Do not debug DMA/MDLs yet.
- Preserve the established baseline: no `ALADDINAcqDriver0` DOS alias, register `958695A4-693A-435E-8297-66F805D8E46A`, keep `CFDC2190`, keep the 85FB six-byte host header, and keep generic Family-0 opcode `0x88` acceptance.

Next action: obtain complete original-32-bit and x64 Family-1 opcode-`0x96` responses and compare them byte-for-byte plus the subsequent request sequence.


## Trace evidence retention and reuse

Recorded traces are durable project evidence. Do not ask for a trace to be
recorded again merely because it is not attached to the current chat.

Before requesting a new capture:

1. Check this file and `docs/runtime-trace.md` for an existing trace that
   contains the required information layer.
2. If the trace already exists but is not attached in the current chat, ask
   for that exact filename.
3. Request a new capture only when testing a newer driver state, when the old
   tracer did not capture the required field, or when a materially different
   hardware/application state is required. State which of those reasons
   applies.

Important trace-format distinction:

- x64 `xstream_trace_*.jsonl` files produced from the in-kernel diagnostic
  ring store only 128 output bytes per IOCTL. Traces captured **before**
  commit `ca75f36d17ba0789f818e3f9f2bd599370b58ba1` also intentionally suppress
  successful `CFDC21C0` / `LECS65_IOCTL_REGISTER_READ` entries. Their
  absence in those older traces is not evidence that XStream did not issue
  them. Newer traces retain successful register reads for control-flow
  comparison.
- legacy/user-mode `legacy_xstream_trace_*.jsonl` captures can contain full
  `NtDeviceIoControlFile` buffers. Format-version-3 captures also contain
  `NtCreateFile` records and therefore preserve interface/handle mapping.

### High-value trace registry

- `legacy_xstream_trace_20260927_165243.jsonl`
  - first successful injected user-mode capture;
  - Win32-only capture proved the LeCroy path bypasses the hooked
    `DeviceIoControl` surface and motivated Native-API interception.
- `legacy_xstream_trace_20260927_185222.jsonl`
  - large original 32-bit Native-API runtime/acquisition reference;
  - contains real `CFDC2124`, `CFDC2128`, and `CFDC2138` traffic;
  - use this when comparing the functional acquisition path after startup.
- `legacy_xstream_trace_20260927_195608.jsonl`
  - primary original 32-bit format-v3 reference;
  - includes `NtCreateFile` and maps IOCTL handles to the five recovered
    interfaces;
  - contains complete `CFDC2110` outputs, including the opcode-`0x90` and
    opcode-`0x96` response framing used for the current comparison.
- `legacy_xstream_trace_20260927_203017.jsonl`
  - x86 user-mode tracer running against the x64 replacement driver;
  - proves that publishing `\\??\\ALADDINAcqDriver0` made XStream select a
    different main-control endpoint even though all five PnP interfaces were
    available.
- `xstream_trace_20260927_204139.jsonl`
  - first x64 kernel trace after removing the DOS alias;
  - exposes missing `CFDC2190` as the next startup gate.
- `xstream_trace_20260927_210024.jsonl`
  - first x64 trace after `CFDC2190` succeeds;
  - exposes missing six-byte 85FB host framing.
- `xstream_trace_20260927_211141.jsonl`
  - x64 trace after the first 85FB framing fix but before generic
    family-0 opcode-`0x88 / 0x001F` admission;
  - contains the ten link-reset-triggering `0x88` failures.
- `xstream_trace_20260927_212113.jsonl`
  - best x64 trace after the opcode-`0x88` fix and before commit
    `d51dbe2f5098c2797094b7c1681bf7a2e65d8acd`;
  - trigger starts and probe/link errors are gone, but XStream repeats
    family-1 opcode-`0x96` selector `0x20` and never reaches
    `CFDC2124/CFDC2138`;
  - this trace contains the pre-fix hard-coded 85FB length value `2` and must
    not be used to judge the post-`d51dbe2` driver state.
- `xstream_trace_20260928_000706.jsonl`
  - post-MTT calibration trace;
  - proves family-1 opcode 0x51 works and exposes intermittent CFDC2138
    completion timeouts before the ISR-side IIMCL clear fix.
- `xstream_trace_20260928_002537.jsonl`
  - proves all captured DMA/MTT IOCTLs succeed after the IIMCL fix;
  - exposes the incorrect firmware forwarding of runtime opcode-0x88 /
    mask 0x0080.
- `xstream_trace_20260928_004553.jsonl`
  - first x64 trace where calibration visibly completes;
  - exposes the post-calibration INTST-0x04 / pending-0x0080 event storm caused
    by missing BAR1 CLRIRQ acknowledgement.
- `xstream_trace_20260928_005808.jsonl`
  - **first confirmed x64 waveform-acquisition trace**;
  - captured after commit `6cc5a238684c217f1238aae2069950ec99a39672`
    added the recovered BAR1 CLRIRQ ISR acknowledgement;
  - user visibly observed waveforms;
  - 16,827 traced IOCTL records have NTSTATUS success;
  - 3,134 CFDC2138 acquisitions all succeed and every four-byte result equals
    the requested transfer byte count;
  - successful CFDC2138 sizes include 0x0C00, 0x5400, 0x0400, 0x1400,
    0x2000, 0x29000 (167,936 bytes) and others;
  - all 678 captured family-0 opcode-0x88 calls return the legacy zero response;
  - the previous million-call post-calibration event storm is absent.

The older milestone traces from `xstream_trace_20260927_023951.jsonl` through
`xstream_trace_20260927_151006.jsonl` remain indexed chronologically in
`docs/runtime-trace.md`. Consult that history before asking the user to
reproduce an already captured startup stage.

### 2026-09-27 corrected opcode-0x96 comparison and next retest

The apparent opcode-`0x96` mismatch in `xstream_trace_20260927_212113.jsonl`
was initially misidentified as firmware state. Full record framing shows that
output offset 10/11 belongs to the **second record's 85FB host header**, because
the preceding A5FB record contributes the first six output bytes.

Legacy reference values:

- family-1 opcode-`0x90`: second-record 85FB length = `0x0006`;
- family-1 opcode-`0x96`: second-record 85FB length = `0x0202` (514);
- family-1 opcode-`0x99`: legacy special case, length = `0x0002`.

The 21:21 x64 trace wrote `0x0002` for the opcode-`0x90` and opcode-`0x96`
fetch records as well. Commit
`d51dbe2f5098c2797094b7c1681bf7a2e65d8acd` (created after that trace)
already fixes this by using `recordOutput - 6` for normal raw-hardware 85FB
responses while preserving opcode-`0x99` as a length-2 exception.

Do not infer from pre-`ca75f36d` kernel traces that the legacy `CFDC21C0`
polling block is missing: those traces deliberately omitted successful
register-read IOCTLs. Commit `ca75f36d17ba0789f818e3f9f2bd599370b58ba1`
removes that diagnostic suppression for new captures.

Next hardware test: build/install current `main` including `d51dbe2` and run
XStream. A useful success discriminator is that legacy performs 120 contiguous
family-1 opcode-`0x96` reads beginning with selectors `0x20, 0x22, 0x24,
0x26, ...`, rather than repeating `0x20`. The original traces later reach
`CFDC2124` followed immediately by `CFDC2138`. Only a post-`d51dbe2` trace
can determine the next blocker if acquisition still does not start.


## Preferred x64 scope test workflow

The user keeps a scope-local PowerShell helper named
`Run-LeCroy-XStream-Trace.ps1`. For normal x64 hardware retests, prefer this
script over manually asking for separate pull/build/load/capture/start steps.

The supplied script performs the following sequence itself:

1. Requires an elevated PowerShell session.
2. Uses the scope checkout at
   `C:\\Users\\LeCroyUser\\Git\\lecroy_wr6k_64bit_driver`.
3. Runs `git pull`.
4. Runs `scripts\\build-sign-load-driver.ps1 -Configuration <Debug|Release>`.
5. Uses `tools\\lecdiag\\build\\lecdiag.exe trace-capture` with a one-hour
   maximum window.
6. Stops an already-running XStream instance before the test.
7. Starts the kernel trace, then launches
   `C:\\Program Files (x86)\\LeCroy\\XStream\\lecroyxstreamdso.exe`.
8. Keeps tracing until XStream is closed normally, then stops the trace helper.
9. Saves the trace as
   `trace-captures\\xstream_trace_YYYYMMDD_HHMMSS.jsonl` in the repo checkout.
10. Attempts to copy the finished trace to the configured NAS trace directory;
    if the NAS copy fails, the local trace remains intact.

The helper is located at the fixed scope path:

`C:\\Users\\LeCroyUser\\Desktop\\Run-LeCroy-XStream-Trace.ps1`

Always give the user the complete path. Do not use a relative
`.\\Run-LeCroy-XStream-Trace.ps1` invocation, even if the current directory
would make it work.

Default command for an ordinary x64 trace run:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& "C:\\Users\\LeCroyUser\\Desktop\\Run-LeCroy-XStream-Trace.ps1" -Configuration Debug
```

When asking the user to test a new x64 driver state, always provide the exact
commands they should execute. Do not merely say "retest" or describe the steps
abstractly. Prefer the helper script whenever the required evidence is available
from the kernel `lecdiag` trace. Tell the user what visible XStream action to
perform during the run and when to close XStream.

Do not use this helper when the investigation specifically requires complete
user-mode buffers, `NtCreateFile` handle/interface mapping, or another field
that the kernel trace cannot capture. In that case explicitly state why the
user-mode XStream tracer is required and give its exact command sequence.


### 2026-09-27 post-d51dbe2 trace: xstream_trace_20260927_222010.jsonl

This is the first x64 kernel trace captured after the corrected 85FB payload-length
rule in commit `d51dbe2f5098c2797094b7c1681bf7a2e65d8acd`.

Confirmed progress:

- Family-1 opcode `0x96` no longer stalls on selector `0x20`.
- Exactly 120 opcode-`0x96` requests are observed and the selector advances
  through the legacy-style sequence beginning `0x20, 0x22, 0x24, 0x26, ...`.
- The first opcode-`0x96` output now contains the legacy 85FB length
  `0x0202` (514). The earlier `212113` blocker is resolved.
- No `CFDC2124`, `CFDC2138`, or `CFDD219F` calls are reached yet.

The late trace reaches the known family-1 opcode-`0x42` 76-bit JTAG status
request at seq 819. It is repeated 47 times with response payload state
`1400 3040 0000 0050 0020` (host framing omitted here), with a 10 ms delay
after the first six polls.

Legacy comparison changes the interpretation of that status loop:

- the original 32-bit trace also returns the same `...0050...` state for its
  first 46 observed polls;
- the original later changes to `...0002...`, but it already performs real
  `CFDC2124` transfer registrations and `CFDC2138` acquisitions while the
  JTAG status is still `...0050...`;
- therefore the `...0050...` JTAG value itself is not sufficient to explain
  why current x64 never starts acquisition.

A representative original active cycle around the same status state is:

```text
family2/op02 = 1
85FB status
family0/op88
CFDC2184(1)
family2/op02 = 0
family1/op42 JTAG status -> ...0050...
CFDC2124
CFDC2138
...
```

Current x64 instead reaches family2/op02 = 1 and then remains in repeated JTAG
status requests; it never enters that host-side acquisition sequence.

Do not invent a side effect for `CFDC2184`: static recovery already proves the
legacy DeviceControl branch completes it with STATUS_SUCCESS and zero
information, which matches the current x64 handler. The absence of repeated
`CFDC2184` calls is evidence of user-mode control-flow divergence, not evidence
that the no-op handler is wrong.

The next useful discriminator is the successful `CFDC21C0` register/status
traffic around the late JTAG/acquisition transition. Commit
`ca75f36d17ba0789f818e3f9f2bd599370b58ba1` changes only diagnostics: it
stops suppressing successful register reads in the kernel trace. No hardware
or IOCTL semantics are changed. Retest with the normal scope desktop helper
before escalating to another injected user-mode trace. If the kernel ring
proves insufficient, a fresh user-mode Native-API trace on the current
post-d51dbe2 endpoint state is the fallback; the older
`legacy_xstream_trace_20260927_203017.jsonl` predates DOS-alias removal and
is not an equivalent current-state reference.


### Current diagnostic retest after ca75f36d

Commit `ca75f36d17ba0789f818e3f9f2bd599370b58ba1` retains successful
`CFDC21C0` register reads in the in-kernel trace. This is a tracing-only
change. The next scope run should use the standard desktop helper at its exact
path:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& "C:\\Users\\LeCroyUser\\Desktop\\Run-LeCroy-XStream-Trace.ps1" -Configuration Debug
```

During the run, let XStream reach the no-waveform/acquiring state, leave it
there long enough to execute the late JTAG polling block, then close XStream
normally. The resulting trace should reveal the CFDC21C0 offsets and returned
values that were hidden in earlier x64 captures.


## 2026-09-27 trace 223910: trigger restart confirms missing legacy event wakeups

Trace `xstream_trace_20260927_223910.jsonl` is the first x64 kernel capture
after commit `ca75f36d17ba0789f818e3f9f2bd599370b58ba1` retained successful
`CFDC21C0` reads.

The newly visible register reads do **not** occur in the late Acquiring loop.
All 11 successful reads are part of earlier initialization:

- BAR0 offset `0x000` returns `2`;
- BAR1 offset `0x00C` (ACQFVER) returns `3`.

Therefore hidden `CFDC21C0` polling is not the missing transition.

The user's Auto -> Stop -> Auto trigger change is visible in the trace. XStream
leaves the JTAG poll, sends family-2 opcode `0x02 = 0`, reconfigures the
board, re-arms with opcode `0x02 = 1`, and returns to the same
`...0050...` JTAG state. No `CFDC2124`, `CFDC2138`, or `CFDD219F`
appears. The stall is reproducible across a trigger restart.

Static comparison then exposed a concrete x64 interrupt/event defect:

- original ISR `FUN_000108D6` accumulates all enabled INTST sources and queues
  its DPC for every accepted source;
- previous x64 ISR queued its DPC only for INTST bit `0x01`;
- original DPC `FUN_00011390` maps INTST bit `0x02` to the event registered
  by `CFDC218C`;
- INTST bits `0x04`, `0x10`, and `0x20` wake the event registered by
  `CFDC2180`;
- INTST bit `0x01` is the selected acquisition transfer completion event;
- previous x64 DPC only signalled the transfer completion event and never
  signalled either registered XStream event.

The event registration mapping itself is statically confirmed:

- `CFDC2180 -> FUN_000128F8 -> main+0x12DE`;
- `CFDC218C -> FUN_00012B34 -> main+0x12EE`.

The x64 driver now mirrors these proven event-delivery semantics. Event pointer
replacement/release is protected by `LegacyEventLock`, and ISR sources are
coalesced in `InterruptPendingShadow`. INTST bit `0x08` remains intentionally
unmapped because the original uses it for an internal RX transport event while
the replacement transport currently polls RX_CONTROL synchronously.

This change does **not** enable DMA. `CFDC2138` and `CFDD219F` remain
gated. The next test should determine whether restoring the legacy wakeup path
causes XStream to issue `CFDC2124` and then reach the already-gated
acquisition IOCTL.


## 2026-09-27 trace 225338: standalone 85FB status is the next gate

Trace `xstream_trace_20260927_225338.jsonl` is the first hardware run after
commit `21238b8fe94c02b42bf04ce84e98ceffd1dd393b` restored the proven
CFDC2180/CFDC218C interrupt-to-event delivery.

The behavioral change proves that the event fix wakes a previously blocked
XStream control path. Instead of remaining indefinitely in the late JTAG poll,
XStream immediately issues the standalone ten-byte CFDC2110 request:

```text
0A0002000300FB854001
```

The pre-fix x64 parser rejected this request eleven times with
`STATUS_INVALID_DEVICE_REQUEST (0xC0000010)`, after which XStream reported
"Unable to arm acquisition board" and "PCI Communication failed!".

Both independent original traces already contain this exact request hundreds
of times. They return the identical ten-byte response:

```text
000000000400BF028000
```

Static legacy recovery explains every field. `FUN_000169B4`, 85FB
subcommand 1, returns:

```text
DWORD 0
WORD  4
WORD  command enable mask
WORD  sticky command pending mask
```

At this acquisition transition the enable mask is `0x02BF`, programmed by
the preceding family-0 opcode-`0x85` request. The pending mask is `0x0080`.
Original `FUN_00011390` latches command-status bits from hardware interrupts:

- INTST `0x04` -> pending `0x0080`
- INTST `0x10` -> pending `0x0800`
- INTST `0x20` -> pending `0x0100`

Only bits present in the command enable mask are latched. Family-0 opcode
`0x88` clears requested sticky pending bits before forwarding the command.

The x64 driver now implements this stateful local status path. It does not
hard-code `BF02 8000`: opcode `0x85` stores the current enable mask, the
DPC latches the proven pending bits, standalone 85FB/0x01 reports both masks,
and opcode `0x88` clears the requested pending bits.

Legacy sequencing shows the immediate expected next path after a successful
status query:

```text
85FB/0x01 -> enabled 0x02BF, pending 0x0080
family0/0x88 mask 0x0080
CFDC2184
family2/0x02 = 0
family1/0x42 JTAG status
CFDC2124
CFDC2138
```

Active DMA remains intentionally gated. The next retest is expected to expose
`CFDC2124` and then reach `CFDC2138`; a controlled failure at the still
unsupported acquisition handler is acceptable evidence and must not be hidden
by fabricating DMA success.


## 2026-09-27 trace 230317: first real acquisition call and one-channel DMA enablement

Trace `xstream_trace_20260927_230317.jsonl` is the first x64 run after the
stateful standalone 85FB/0x01 status implementation. The previous arm failure
is gone. XStream reaches its acquisition-memory builder and reports:

```text
CAAcqDescBuilder::Init
FAILED to Initialize Memory! NumSeg: 40
```

The trace exposes the direct cause. `CFDC2124` succeeds, then the deliberately
gated `CFDC2138` returns `STATUS_NOT_SUPPORTED (0xC00000BB)` twice.

First x64 pair:

```text
CFDC2124
  input  = 48D6BB1F040C000000000000
  output = 01000000
  total bytes = 0x0C04
  data bytes  = 0x0C00

CFDC2138
  input = 0100000001010100000000000C0000
  token       = 1
  count       = 1
  pair marker = 1
  channel     = 1
  config      = 0
  data bytes  = 0x0C00
```

The working original performs the same 15-byte CFDC2138 shape and returns the
requested byte count as its four-byte output. The 19:56 complete legacy trace
contains 1902 CFDC2138 calls; all have input length 15, channel count 1,
pair-marker byte 1, output length/information 4 and successful completion.
Observed channel IDs are 0, 1, 2, 0x30, 0x31 and 0x32; observed config DWORDs
are 0, 0x200 and 0xC00.

The static path is now sufficiently closed for that exact one-channel ABI:

```text
CFDC2138 / 0x141DC
 -> 0x13C84 validation
 -> 0x12D6A acquisition orchestrator
 -> clear BAR1 GPIODAT bit 16
 -> 0x17D20/0x17C16 MAMDAT + MAMPGO=0x105
 -> 0x17CDC/0x17EE0 MAMSEQ
 -> SGTA = descriptor-table PA
 -> IIMTC = transfer dword count
 -> reset transfer completion event
 -> enable INTEN bit 0
 -> IIMCL = 1
 -> MAMRGO = requested/min(requested,0x400)
 -> wait <= 5 s
 -> disable INTEN bit 0
 -> IIMST/IIMCL cleanup + ERRS read
 -> return requested byte count
```

For one channel the five MAMDAT values are `0x0E00|channel`, config
low/high, and `min(requested,0x400)` low/high. MAMSEQ is
`0x40|channel`. For the first current 0xC00-byte transfer MAMRGO is 3.

DMA address-width safety remains enforced before launch: CFDC2124 locks the
pages, builds the legacy 8-byte descriptor chain and rejects every source or
descriptor-table physical address above 4 GiB. Therefore the successful
CFDC2124 in trace 230317 proves that this exact current transfer is
representable by the board's 32-bit DMA ABI.

The x64 driver now executes only this observed CFDC2138 one-channel form
(input 15, output 4, count 1, pair marker 1, channel <= 0x3F). Timeout remains
real and maps to STATUS_IO_TIMEOUT; no fake completion is returned.

`CFDD219F` remains gated. Multi-channel CFDC2138 remains gated.


## 2026-09-27 CFDC2138 build fix

The first build of commit `124785ccd65e190fe73494394ea0ebc0502be8c5`
failed before driver loading. The new CFDC2138 helper called `LecReadU32`,
but only `LecReadU16` and the write helpers existed in `Ioctl.c`. MSVC
therefore emitted C4013 and the linker failed with LNK2019 for the unresolved
`LecReadU32` symbol.

The fix adds the missing unaligned-safe `LecReadU32` helper using
`RtlCopyMemory`, matching the existing 16-bit helper style. The same change
initializes the family-2 opcode-0x02 `mttCtl` pointer to NULL to remove the
existing C4701/C4703 maybe-uninitialized warnings without changing control-flow
semantics.

The failed build never reached signing or driver reload, so no DMA-capable
driver from commit 124785c was loaded during that attempt. Retest using the
normal `Run-LeCroy-XStream-Trace.ps1 -Configuration Debug` workflow after
pulling the fix.


## 2026-09-27 trace 235712: CFDC2138 succeeds; family-1 opcode 0x51 is next blocker

`xstream_trace_20260927_235712.jsonl` proves that the first enabled x64
one-channel CFDC2138 DMA path completes successfully:

```text
CFDC2124 seq 739
  input  1076BC1C040C000000000000
  output 01000000
  status SUCCESS

CFDC2138 seq 740
  input  0100000001010100000000000C0000
  output 000C0000
  Information = 4
  status SUCCESS
```

Therefore the unchanged XStream dialog
`CAAcqDescBuilder::Init / FAILED to Initialize Memory! NumSeg: 40` no longer
indicates failure of the first MAM acquisition. The failure has moved to the
next memory-builder step.

Immediately after the successful 0x0C00-byte CFDC2138, XStream registers a
second transfer:

```text
CFDC2124 seq 741
  total bytes = 0x404
  data bytes  = 0x400
  output token = 2
  status SUCCESS
```

It then sends:

```text
06000A000300FBA540015100020000008000
080002000300FB854000
```

This is family 1 opcode `0x51`, payload layout after the opcode:

```text
BYTE  control/unused = 0
DWORD transfer token = 2
WORD  launch value   = 0x0080
```

The pre-fix x64 gate rejected it with `STATUS_INVALID_DEVICE_REQUEST`.

Both complete original traces contain this exact semantic operation repeatedly:

- 19:56 trace: 279 opcode-0x51 calls;
- 18:52 trace: 286 opcode-0x51 calls;
- every captured call succeeds with 14 output bytes
  `0000000000000000000002000000`;
- every runtime request has payload length 10 and control byte 0;
- 278/279 and 285/286 calls respectively use launch value `0x0080`;
- one call in each trace uses `0x0180`;
- all referenced CFDC2124 entries have 0x400 data bytes.

Static dispatch in `FUN_000165A6` sends both family-1 opcodes `0x50` and
`0x51` to `FUN_000160DC`. That helper resolves the DWORD transfer token,
rejects the request when `launch_word << 3 < registered_data_bytes`, then
calls `FUN_00017478 -> FUN_000171DE` with the final selector choosing
MTTRGO rather than MAMRGO.

The x64 implementation now mirrors that proven path and uses the same
owner-checked below-4-GiB descriptor object already built by CFDC2124.


## 2026-09-28 trace 000706: calibration progresses; intermittent transfer completion timeouts remain

`xstream_trace_20260928_000706.jsonl` is the first x64 capture after
family-1 opcode 0x50/0x51 MTTRGO support.

High-level result:

- XStream no longer raises the CAAcqDescBuilder memory-initialization fatal
  error.
- The application remains active in `Calibrating...` for a long interval and
  continues issuing acquisition/front-end traffic through the end of the
  capture.
- The user changed CH1 coupling from DC50 toward DC 1 Meg during the run. The
  trace continues through front-end configuration traffic; there is no
  unsupported/rejected IOCTL associated with that interaction.
- Relays were audibly observed once during the run, consistent with the
  application finally reaching front-end/calibration control rather than being
  blocked in memory setup.

Trace counts:

```text
CFDC2110  10070
CFDC2184   2798
CFDC2138    804
CFDC2124    766
CFDC2128    756
family1/0x51 114
```

All 114 family-1 opcode-0x51 calls succeed with the legacy 14-byte combined
response. No CFDC2110 call is rejected. Of 804 CFDC2138 calls, 798 succeed and
six return `STATUS_IO_TIMEOUT (0xC00000B5)`.

The six failures occur at approximately 52.38, 70.97, 80.09, 103.42, 121.38
and 142.54 seconds after the first traced IOCTL. Every failed request is channel
`0x30`, config `0x00000C00`; requested sizes are 0x20, 0x20, 0x24, 0x28,
0x54 and 0x10 bytes. In every case XStream retries the same token/request and
the retry succeeds about 30 ms after the five-second timeout completes.

This is not observed on the original driver: the complete 19:56 legacy trace
contains 1902 CFDC2138 calls and the complete 18:52 trace contains 1984; every
captured call returns NTSTATUS success with Information=4.

Static comparison identifies a missing immediate ISR action. The acquisition
subobject is constructed at `main+0x1E0`. `FUN_00014847` stores BAR0 IIMCL
(offset `0x048`) at subobject offset `0x1D8`, therefore the raw pointer is
at `main+0x3B8`. Original ISR `FUN_000108D6` explicitly writes zero through
`main+0x3B8` whenever INTST bit 0 is set, before it queues the DPC that later
signals the transfer completion event.

The previous x64 ISR acknowledged INTST bit 0 but did not perform this immediate
IIMCL clear; IIMCL was only conditionally cleaned up after the waiting thread
resumed. The x64 ISR now mirrors the original bit-0 behavior and writes
`IIMCL=0` immediately before pending-bit/DPC processing.

Trace 000706 is now the primary evidence file for the post-MTT calibration
stage. Do not ask for memory-initialization or opcode-0x51 traces again unless a
newer driver state specifically needs regression comparison.


## 2026-09-28 trace 002537: DMA timeouts gone; opcode-0x88 local special case identified

`xstream_trace_20260928_002537.jsonl` is the first x64 capture after the
immediate ISR-side `IIMCL=0` completion acknowledge.

Observed runtime state:

- XStream no longer reports the earlier memory-initialization fatal dialog.
- It remains indefinitely in `Calibrating...`.
- The run contains 30,931 traced IOCTLs and no non-success NTSTATUS.
- `CFDC2138`: 1261 calls, all success.
- family-1 opcode `0x51`: 114 calls, all success.
- `CFDC2124`: 336 registrations; `CFDC2128`: 324 explicit removals.
- The previous six 5-second CFDC2138 timeouts are completely gone.

The user terminated XStream after the calibration hang. Relays were then heard
clicking roughly 10-20 seconds later. The trace does not prove whether those
clicks came from process-shutdown commands or autonomous board firmware state,
but the same run exposes a major command-routing error capable of perturbing
firmware state.

Family-0 opcode `0x88` distribution:

```text
mask 0x0080 : 5660 calls
mask 0xFFDF :    1 call
mask 0x001F :    1 call
```

The two complete original traces contain only about 280/289 runtime
`0x0080` calls because calibration completes. Every original opcode-0x88
combined response is:

```text
0000000000000000000002000000
```

The pre-fix x64 implementation forwarded every opcode-0x88 to firmware.
In trace 002537, 352 of the mask-0x0080 calls return:

```text
0000000000000000000002002C00
```

and trace 000706 previously showed mostly `...2D00`.

Static legacy code proves this forwarding is wrong. `FUN_00016A66` always
clears the local pending mask first, but when the opcode-0x88 mask is exactly
`0x0080` or `0x0800`, it calls `FUN_00015A88(this, 0)` and exits without
calling the firmware transport. `FUN_00015A88` installs the local response:

```text
DWORD 0
WORD  2
WORD  0
```

Only other masks continue through the board-forwarding helper.

The x64 implementation now mirrors that branch: masks 0x0080/0x0800 are local
and never reach firmware; other masks such as the startup 0xFFDF and 0x001F
remain firmware-forwarded.

A separate comparison found that original type-1/type-2 CFDC2110 output bytes
often contain nonzero pool residue while the x64 driver deliberately zeroes
them. Static `FUN_00013AE2 -> FUN_00012F30` shows those record outputs are not
initialized by the original host handler. Do not reproduce that kernel-pool
information leak or treat those bytes as a defined ABI unless future user-mode
evidence proves XStream depends on them.


## 2026-09-28 trace 004553: calibration completes; post-calibration CLRIRQ loop identified

`xstream_trace_20260928_004553.jsonl` is the first hardware trace after the
family-0 opcode-0x88 masks 0x0080/0x0800 were corrected to the original local
acknowledgement path.

User-visible result:

- `Calibrating...` still takes a long time but now eventually completes.
- No waveform is displayed afterwards.
- The application appears mostly idle from the UI perspective.

Trace result:

- 91,109 IOCTL records are captured and every one has NTSTATUS success.
- `CFDC2138`: 40 calls, all success.
- `CFDC2124`: 43 calls; `CFDC2128`: 41 calls.
- No unsupported or failed CFDC2110 command appears.
- After approximately 181.43 s, immediately after the final calibration DMA
  burst, XStream enters a repeated three-step cycle:
  1. standalone 85FB/0x01 -> enabled=0x02BF, pending=0x0080;
  2. local family-0 opcode 0x88 / mask 0x0080 -> success/zero response;
  3. CFDC2184(1) -> success.
- The same pending bit is then visible again immediately. The stored trace has
  only about 91k lines because the 256-entry kernel ring is being overrun;
  internal sequence numbers rise from roughly 30k at the start of the loop to
  above 1.53 million by 270 s. This is not normal low activity but an extreme
  host event loop.

The local opcode-0x88 implementation is now correct. The missing behavior is
one layer below it in the hardware ISR.

Original `FUN_000108D6` handles the interrupt sources as follows before the
common INTST write-back:

```text
INTST 0x01 -> IIMCL = 0
INTST 0x04 -> CLRIRQ = 1
INTST 0x08 -> CLRIRQ = 2
INTST 0x10 -> CLRIRQ = 4
INTST 0x20 -> CLRIRQ = 8
```

`FUN_00014847` proves that the pointer used for those 0x04..0x20 writes is
BAR1 CLRIRQ:

- acquisition object base = `main + 0x1E0`;
- CLRIRQ register object = acquisition + `0x200`;
- pointer location therefore = `main + 0x3E0`;
- register address = BAR1 base + `0x008`.

The previous x64 ISR performed the IIMCL completion clear and the common INTST
write-back, but omitted every CLRIRQ source acknowledge. Consequently INTST
0x04 could immediately reassert, the DPC re-latched command pending bit 0x0080,
and the CFDC2180 user event woke XStream again.

The x64 ISR now mirrors the four CLRIRQ writes exactly and still performs the
common INTST write-back afterwards.

Trace 004553 is the primary evidence file for the completed-calibration /
post-calibration event-loop stage. Do not ask the user to reproduce that stage
unless validating a newer ISR state.


## 2026-09-28 trace 005808: first confirmed x64 waveforms

`xstream_trace_20260928_005808.jsonl` is the first runtime capture where the
user visibly observed waveforms while XStream was running on the 64-bit
replacement driver.

This test uses current `main` after commit
`6cc5a238684c217f1238aae2069950ec99a39672`, which restored the original
BAR1 CLRIRQ source-specific ISR acknowledgement.

Trace summary:

```text
records parsed              16,828
IOCTL records               16,827
non-success IOCTLs               0
CFDC2110                     12,851
CFDC2138                      3,134
CFDC2184                        681
CFDC2124                         67
CFDC2128                         63
```

All 3,134 CFDC2138 calls return NTSTATUS success, Information=4 and a DWORD
equal to the requested byte count. Observed requested sizes include:

```text
0x00000400    1,024 bytes
0x00000C00    3,072 bytes
0x00001400    5,120 bytes
0x00002000    8,192 bytes
0x00004C00   19,456 bytes
0x00005400   21,504 bytes
0x00029000  167,936 bytes
```

The first CFDC2138 appears about 18.31 s into the capture. A 167,936-byte
transfer is already completing successfully about 18.33 s into the capture,
and successful acquisition traffic continues through the final trace records.

All 678 captured family-0 opcode-0x88 transactions return the original-style
combined response ending in `0000`; none exposes the former firmware values
0x002C/0x002D.

The trace sequence spans 1..18,546 with only modest ring loss, rather than the
>1.5-million sequence explosion seen in trace 004553. The previous
post-calibration pending-0x0080 event storm is therefore resolved by the
source-specific CLRIRQ writes.

This is now the primary known-good x64 runtime baseline. Do not request another
"does waveform acquisition work at all?" trace. Future captures should target
specific regression areas or remaining features.


## 2026-09-28 trace 011938: broad functional regression passes

`xstream_trace_20260928_011938.jsonl` is the first broad normal-operation
regression run after waveform acquisition was restored.

User-visible validation during this run:

- displayed waveform height is plausible/correct;
- displayed waveform frequency is plausible/correct;
- timebase adjustment works;
- input vertical scale adjustment works;
- coupling changes work;
- bandwidth changes work;
- switching to 2-channel / 10 GS/s mode works;
- trigger-type changes work.

Trace summary:

```text
records                    156,971
IOCTL records              156,970
non-success IOCTLs               0
CFDC2110                   106,365
CFDC2138                    43,157
CFDC2184                     6,296
CFDC2124                       494
CFDC2128                       486
```

All captured IOCTLs return NTSTATUS success. CFDC2138 traffic spans channel IDs
`0, 1, 2, 0x30, 0x31, 0x32`; the run therefore exercises substantially more
than the original one-channel bring-up path and remains stable while XStream
changes acquisition modes and front-end settings.

Probe-path evidence:

- family-0 opcode `0x90` occurs 1,700 times;
- selector `0x0E` occurs 1,436 times with the recovered 144-bit probe
  transaction shape;
- selector `0x0C` occurs 87 times;
- selectors `0..4` also occur repeatedly;
- every captured request succeeds.

Static recovery identifies family-0 opcode 0x90 as the local BAR1 SPI
helper using SPICTL/SPIDAT/SPIDIN. **Important later hardware clarification
(2026-09-29):** the user confirms that the electrical probe-side interface
uses an ADC signature for ProBus-class detection, then reads a front-panel
EEPROM through I2C, and conducts physical probe control via I2C.
The driver-visible local BAR1 SPI helper is a **different protocol layer**;
no demonstrated bridge mapping yet relates selector 0x0E to a physical
I2C transaction. The Windows driver IOCTL trace cannot reveal SDA/SCL
transitions or EEPROM addresses, but subsequent real AP015 tests do verify
user-visible identification and jaw-unlock warning on x64. Do not keep
describing those functions as awaiting their first probe-level test.
See `docs/probus-detection-i2c-architecture.md` for verified-versus-
user-supplied evidence and the remaining lower-layer questions.

Treat trace 011938 as the primary normal-operation functional-regression trace.


## Developer-menu compatibility: Run Link Tests

After normal waveform operation and the broad regression trace
`xstream_trace_20260928_011938.jsonl`, the user tested XStream's developer
menu item `Run Link Tests`. XStream logs:

```text
WaveRunner Driver Not Supported
```

This is currently an application-visible compatibility gap, but it is not yet
known whether the message is caused by a missing driver capability or by an
XStream-side support gate that rejects the replacement before any link-test
IOCTL is issued.

Important discriminator: the x64 replacement already returns the recovered
legacy driver build value `1002` through `CFDC21C8`. Therefore the message
must not be casually attributed to the public build-query value alone.

Next investigation must be passive:
1. run the normal x64 XStream trace helper on the working waveform baseline;
2. wait until normal waveform acquisition is active;
3. invoke Developer -> Run Link Tests exactly once;
4. wait a few seconds and close XStream normally;
5. compare IOCTL traffic immediately around the menu action.

If a new IOCTL/packet sequence appears and fails, recover that ABI from the
legacy driver/trace before implementing it. If no new driver traffic appears,
the "not supported" result is an XStream-side capability/identity gate and
should be investigated in the user-mode binary rather than by changing working
PCI/DMA behavior.

Do not modify the known-good acquisition path merely to make this developer
menu entry advance.


## 2026-09-28 trace 013644: Developer Run Link Tests is gated in XStream

Important test-procedure correction from the user:

- XStream's Developer -> Run Link Tests must be executed with acquisition
  stopped.
- Do not instruct future tests to invoke it while waveform acquisition is
  running.

Trace `xstream_trace_20260928_013644.jsonl` captures the developer link-test
attempt in the required stopped-acquisition state.

Result:

- XStream logs `WaveRunner Driver Not Supported`.
- 572 IOCTLs are captured and every IOCTL returns NTSTATUS success.
- No new/unknown IOCTL code appears.
- No new unsupported CFDC2110 opcode appears.
- `CFDC21C8` is not issued during the captured XStream session, so this
  rejection is not a runtime comparison of the public build-query result 1002.
- During the long stopped interval the only non-CFDC2110 traffic consists of
  already-known `SET_FLAG_BYTE (0x00222C04)` and
  `DELAY_MS (0x00222C00)` calls, all successful.
- The nearby CFDC2110 traffic is the already-known family-1 opcode-0x42 JTAG
  status poll and also succeeds.

This strongly indicates that `WaveRunner Driver Not Supported` is an
XStream/user-mode capability or driver-type gate that rejects the replacement
before an actual link-test transaction is sent to the acquisition driver.

Do not alter the known-good PCI/DMA path to address this message.

Next investigation should locate the literal string
`WaveRunner Driver Not Supported` in the installed XStream EXE/DLL set and
reverse the surrounding user-mode support check. If the check ultimately
depends on a driver-visible identity/capability field, reproduce that recovered
field in the replacement driver; otherwise document or patch only the XStream
developer diagnostic path as appropriate.

Trace 013644 is the canonical evidence file for this developer-menu gate. Do
not request another Run Link Tests driver trace unless a changed XStream-side
or identity implementation needs validation.


## 2026-09-28 trace 014500: Service/Revision page exposes family-1 A1/A2

The user opened XStream Service -> AladdinAcqBoard -> Revision and received:

```text
HardwarePCI Communication error!
```

The page nevertheless displays other revision information, including
`Device Driver Build Num: 1002`.

Trace `xstream_trace_20260928_014500.jsonl` isolates the remaining driver
error. It contains 590 IOCTL calls: 579 success and 11
`STATUS_INVALID_DEVICE_REQUEST (0xC0000010)` failures. Every failure is one
of two new CFDC2110 local requests:

```text
family 1 / opcode 0xA2
060004000300FBA54001A2000C0002000300FB854000

family 1 / opcode 0xA1
060004000300FBA54001A1000C0002000300FB854000
```

They alternate repeatedly while the Revision page refreshes. No other IOCTL
fails.

Static legacy dispatch closes both commands completely:

- `FUN_000165A6` routes family-1 `0xA1` to `FUN_00015BCE`.
- `FUN_000165A6` routes family-1 `0xA2` to `FUN_00015C26`.
- dispatcher base = board object `+0xEA8`;
- `0xA1` reads dispatcher `+0x17E` = board `+0x1026`;
- constructor `FUN_00014847` stores the BAR1 `ACQFVER` register wrapper
  (offset `0x00C`) at board `+0x1026`;
- `0xA2` reads dispatcher `+0x176` = board `+0x101E`;
- constructor stores the BAR0 `FVER` register wrapper (offset `0x000`) at
  board `+0x101E`.

Both legacy helpers construct the same 12-byte local pending response:

```text
DWORD 0
WORD  6
WORD  protocol status
DWORD register value
```

The x64 driver now implements both local reads exactly. They are not firmware
forwarders.

The same user session also located the literal
`WaveRunner Driver Not Supported` in:

```text
C:\Program Files (x86)\LeCroy\XStream\lecaladdinhwaccesspcisvr.dll
```

That DLL is therefore the primary user-mode reverse-engineering target for the
Developer -> Run Link Tests support gate once the Revision-page regression is
verified.


## 2026-09-28 user-mode analysis: Run Link Tests is intentionally disabled for S65/WaveRunner

The user supplied the installed proprietary
`lecaladdinhwaccesspcisvr.dll` for local reverse engineering only. Do not
commit or redistribute that DLL in this public repository.

Analyzed file:

```text
PE32 x86 DLL
timestamp: 2017-06-23 14:05:50
SHA-256:
4bdfcbe57fb76f40ca5d77f729e1a6662aa3b5b4e3dd2e12a7f13f84ecfc4f86
```

The literal UTF-16 string `WaveRunner Driver Not Supported` is at image VA
`0x1002FB18` (file offset `0x2EF18`). Its only direct code reference is in
the developer-link-test routine around `0x1001A121`.

Recovered gate:

```text
if (this->driverConnection.connected == false)
    log "Driver Not Connected";
else if (this->driverConnection.isS65WaveRunnerDriver != false)
    log "WaveRunner Driver Not Supported";
else
    continue with processor/build/PCI revision and gigabit-link tests;
```

The field mapping is recovered by following construction of the embedded
driver-connection object:

- outer hardware-access object constructs its connection object at
  `this + 0x10` through function `0x10019347`;
- connection `+0x20` therefore appears to the link-test routine as outer
  `+0x30`;
- connection `+0x22` appears as outer `+0x32`;
- link-test checks outer `+0x32` first for driver-connected state;
- it then checks outer `+0x30` for the WaveRunner/S65 rejection.

The connection constructor recognizes driver-family strings:

```text
"Null"
"S65"
"FE2"
"CENTAUR"
```

For `"S65"`, it selects the PnP-interface-backed handle and sets connection
byte `+0x20 = 1`. That exact byte is the condition producing
`WaveRunner Driver Not Supported`.

Therefore the developer-menu rejection is vendor-intended behavior for the
S65/WaveRunner driver family. It is not evidence that the x64 replacement is
missing a kernel IOCTL or returning a wrong build/version value. The original
S65 driver path would hit the same user-mode gate.

Do not "fix" this by forcing the S65 flag to zero in the DLL. The same flag is
used elsewhere to choose materially different driver ABI paths. For example,
code around `0x1001AE8D` selects between IOCTL values `0xCFDC219C` and
`0xCFDD219F` based on the same S65-family flag. Clearing it would make
XStream behave as though different acquisition hardware/driver semantics were
present and could regress a currently working scope.

Project conclusion for Developer -> Run Link Tests:

- treat `WaveRunner Driver Not Supported` as expected vendor behavior for
  S65/WaveRunner;
- no replacement-driver change is required;
- if equivalent diagnostics are desired, reconstruct them as a separate
  diagnostic tool/lecdiag path rather than falsifying the XStream driver family.


## 2026-09-28 calibration correction: Ch2 vertical-step A/B comparison

The user corrected the earlier excessive-calibration report: on the working
x64 driver XStream appears to calibrate each voltage step **once**, and then
reuses that step's calibration when it is selected again.

Four new A/B files were supplied:

- `legacy_xstream_trace_20260928_183122_x86_Caibration_original.jsonl`
- `xstream_trace_20260928_183640_x64_calibration.jsonl`
- `legacy_xstream_trace_20260928_183409_probus_original.jsonl`
- `xstream_trace_20260928_183807_probus_x64.jsonl`

The vertical-only pair steps CH2 from 20 mV/div to 100 V/div. Relevant
comparison (legacy uses `nt_ioctl`, not duplicated usermode `ioctl` entries):

```text
                                  Legacy x86   x64
relevant IOCTLs                       33,849   42,761
failed relevant IOCTLs                     0        0
family-1 opcode 0x96                     120      120
family-1 opcode 0x81                      61       61
family-0 opcode 0x90 selector 0x0E     1,147    1,181
   selector-0x0E idle frames              560      570
   selector-0x0E other frames             587      611
```

The 120 opcode-0x96 selector requests occur in the same set/order; their
captured first 128 output bytes match for identical input selectors. The x64
kernel trace caps output-hex at 128 bytes, whereas the legacy trace captures
526 bytes, so this is a prefix comparison, not a full 526-byte equality claim.

These traces do not demonstrate systematic redundant calibration on revisiting
a vertical step. Total IOCTL counts cannot directly measure calibration count
because the captures differ in duration. Do not alter the calibrated baseline
without a further confirmed discrepancy.

## 2026-09-28 ProBus comparison: missing pre-identification pending bit 0x0200

The two ProBus runs differ sharply:

- legacy workflow: launch XStream, plug a probe, wait for recognition, change
  sensitivity, execute Degauss and Auto Zero, exit;
- x64 workflow: connect the same probe; XStream shows no reaction.

Relevant packet counts:

```text
                                     Legacy x86    x64
relevant IOCTLs                          20,923  20,690
failed relevant IOCTLs                        0       0
family-0 SPI opcode 0x90 / selector 0x0C     75     195
85FB status pending 0x0080                  642     679
85FB status pending 0x0200                    3       0
family-1 opcode 0x82 (identification)        3       0
family-1 opcode 0x4A                         19       0
family-0 opcode 0x4A                         16       0
```

Legacy first observes `0x0200` at trace seq 14552 and again at 14588 and
14616:

```text
85FB/0x01:
  input  0A0002000300FB854001
  output 000000000400BF020002
                 enable=0x02BF, pending=0x0200

family-0/0x88 mask 0x0200:
  060006000300FBA5400088000002080002000300FB854000

family-1/0x82:
  060004000300FBA540018200960102000300FB854000
```

The legacy sequence proceeds into family-0/1 opcode 0x4A. The family-1/0x4A
metadata response at seq 14596 includes ASCII `AP015`. Other 0x4A
transactions follow during the probe-management operations.

Every one of the 679 x64 standalone 85FB/0x01 status requests instead reports
`000000000400BF028000`, with pending=0x0080 only. No 0x0200 producer is
implemented in the current x64 command-pending state machine: the DPC only
latches pending masks 0x0080, 0x0800 and 0x0100. Therefore downstream 0x82
and 0x4A commands are **not requested by XStream**, rather than requested and
rejected. Family-1/0x82 and 0x4A already belong to the existing recovered
firmware-forwarding whitelist.

The x64 probe run does exercise selector-0x0C SPI traffic, so this is not a
complete absence of host-to-board activity. It polls substantially more often,
but the overall capture durations differ; do not turn that count into a rate
claim without aligning timestamps.

An additional, independent, reproducible response mismatch exists for the
same family-1 JTAG opcode-0x42, mode-1, selector-0x4C input packet. In
steady-state samples the legacy output byte at index 17 is 0x20, whereas the
x64 output byte is 0x32. This occurs in both calibration and ProBus A/B
captures. It is not yet proven to be the probe-detection signal.

Next safe discriminator: start XStream on x64 **with the probe already
connected**, rather than hot-plugging it. If recognition then occurs, prioritize
the asynchronous event producer. If not, compare probe-ring/SPI state and
the JTAG-status discrepancy. Read and instrument the actual board sources
before modifying interrupt or command-pending behavior.


## 2026-09-28 trace 193741: AP015 ProBus recognized with probe preconnected

The test recommended after hotplug failure was performed: the AP015 probe was
plugged in *before* launching XStream on x64. XStream recognized it. The user
then invoked Degauss and Auto Zero. New trace:
`xstream_trace_20260928_193741.jsonl` (21,004 captured IOCTL entries,
all NTSTATUS success; 16,151 CFDC2110 calls). The kernel trace sequence
extends through 23329, so some ring records are lost. Do not infer exact
uncaptured operation counts from this file.

A/B results across the controlled ProBus files:

```text
                              legacy hotplug  x64 hotplug  x64 preconnected
85FB pending 0x0200                3              0               0
family-1 / 0x82                    3              0               0
family-0 / 0x4A                   16              0              11
family-1 / 0x4A                   19              0               8
family-0 / 0x90                   879            692             541
failed CFDC2110                    0              0               0
```

At x64 seq 505 the startup probe-control family-0/0x4A request
`06000E000300FBA540004A02020001000001A000A100080002000300FB854000`
succeeds. At seq 508 the family-1/0x4A metadata request
`060004000300FBA540014A01080102000300FB854000` returns Information=270
and contains ASCII `AP015`. Its captured first 128 output bytes exactly
match legacy seq 14596 and 14812 for the same request. This proves the
startup probe metadata transport works without any pending-0x0200 event.

User-observed probe Degauss/Auto Zero activity is reflected in subsequent
0x4A control/response traffic. It is not yet fully response-equivalent:

```text
identical input suffix:   40004A020100000001004700
legacy seq 24205 output: 0000000000000000000002000000FFFF
x64 seq 12082..12087:  00000000000000000000040000000000
```

The x64 call repeats five times, after which a family-1/0x4A read returns
`...F700`; a corresponding legacy post-command read returned `...F200`.
These are actual protocol payload differences despite NTSTATUS success.
Do not call the physical Degauss/Auto Zero result verified without
correlating to device/UI completion indicators.

**Revised working hypothesis:** host-to-board probe communication and AP015
startup identification function; spontaneous recognition of a probe inserted
after XStream has already started remains broken. Legacy hotplug first
delivers 85FB pending 0x0200 and then initiates 0x82/0x4A. The x64 current
DPC latches only 0x80,0x800,0x100 and explicitly ignores the original RX
transport event INTST 0x08. Original `FUN_00011390` processes that source
through `FUN_000176A2`/`FUN_000176D0` and an internal event. This is a
concrete candidate for missing unsolicited probe/ring notification, **not
yet a proven 0x0200 producer**. Investigate causality before changing ISR,
DPC or masks.

Next controlled test if needed: launch XStream with probe recognized,
unplug it while running, observe whether the UI detects removal, then
reinsert and observe recognition, capturing one short passive trace. Do
not modify the proven DMA or substitute synthetic pending bits.


## 2026-09-28 x64 compatibility coverage audit (dispatch vs behavior)

The recovered original `CLecS65AcqDrvDevice::DeviceControl` dispatch table in
`docs/ioctl-map.md` contains **27 unique original IOCTL values**:
26 METHOD_BUFFERED and one METHOD_NEITHER. Comparing those exact values with
the current replacement driver's top-level switch in `driver/Ioctl.c`
shows:

```text
Original dispatch entries                              27
Represented by an x64 switch case                       21
  of which CFDD219F is explicitly STATUS_NOT_SUPPORTED   1
  remaining represented legacy cases                    20
No top-level x64 case yet                                 6
```

One of the 20 represented cases, `CFDC212C`, is *correctly* implemented
as `STATUS_NOT_IMPLEMENTED`, because that is exactly what the original
driver does. Other represented cases may cover only their observed ABI subset
(e.g. CFDC2138 enables only the proven one-channel transfer shape). This is
**dispatch coverage**, not functional, code-line, or full branch coverage.
20/27 = about 74.1% original dispatch behavior represented including the
intentional legacy-not-implemented case; 21/27 = about 77.8% with any
explicit switch case, including the gated METHOD_NEITHER request.

Exactly six original dispatch values currently have no x64 switch case:

- `0x0022303C` - 0x10A-byte trace/control structure;
- `0x00223044` - four-byte direct register-read/status helper;
- `0x00223088` - Dallas/1-Wire memory write;
- `0xCFDC2130` - serial-trigger FPGA programming;
- `0xCFDC2194` - 29-byte interrupt/error status readback paired with CFDC2190;
- `0xCFDC2400` - additional internal control helper.

The separate `0xCFDD219F` METHOD_NEITHER transfer is known and routed, but
intentionally gated because of its native user pointers, WOW64 requirements
and unverified active DMA. The additional successful `0x00222400` probe
implemented in x64 is an observed runtime compatibility case but **not
counted** among the 27 entries in the specific original 2008 dispatch table.

`CFDC2110` itself is a packed dispatcher for many type/signature/family
opcodes. Its broad command functionality is implemented well enough for real
waveforms, trigger/front-end control, calibration, SPI/JTAG, MTT, revision
reads and preconnected AP015 metadata, but its unobserved branches must not be
counted as fully reproduced. Original `0xC5FB` behavior and several other
rare states remain to be tested/recovered.

All 27 original top-level dispatch *values* are identified, so the missing
work is predominantly recovery/implementation/validation, not discovery of
another large unknown primary IOCTL table. There are 197 *selectively
exported* original Ghidra decompilation `.c` files in
`ghidra_exports/selected`; this is not an inventory of every original
machine-code function and cannot support an overall per-function percentage.

Engineering estimates (not objective coverage measures):

- everyday oscilloscope use: approximately 90% complete;
- broad original-driver compatibility including rare/service/probe branches:
  approximately 75-80% complete.

Primary observed gaps are ProBus hotplug during an existing XStream session
and payload differences in certain probe control replies, followed by untested
rare diagnostics and the intentionally gated METHOD_NEITHER transfer.


## 2026-09-28 trace 213834 and exact unsolicited-HWInt recovery

User test:

- AP015 attached before x64 XStream startup: recognized.
- AP015 unplugged while XStream runs: UI does not detect removal.
- AP015 reinserted: waveform disappears.
- Uploaded `xstream_trace_20260928_213834.jsonl`.

Trace facts:

```text
Capture duration              73.143 s
IOCTL records                 36,700
non-success IOCTLs                 0
CFDC2138                       6,987
CFDC2138 return-length errors      0
standalone 85FB/0x01            1,613
85FB enabled mask             0x02BF
85FB pending 0x0080            1,613
85FB pending 0x0200                0
family-1/0x82                       0
family-0/0x4A                       1  (startup only, seq 509)
family-1/0x4A                       1  (startup only, seq 511)
```

The last >=8192-byte DMA is seq 10912 at t=27.087873 s. The last observed
family-0/0x90 is seq 10937 at t=27.09982 s. After about 27.1 seconds,
CFDC2138 settles into recurrent channel 0 / 1,024-byte and channel 0x30..
0x32 / 2,048-byte transfers that continue successfully through trace end.
A byte-identical family-1/0x42 status query transitions from output
`...144030...` at seq 10886/t=27.08393 to `...144032...`
at seq 11016/t=27.21153. Do not infer a physical unplug/replug timestamp
or a specific JTAG bit meaning from that alone.

**Static root cause recovered after this trace:**

- `FUN_0001619A` calls `FUN_000160A8(this,1)` before transmitting the
  firmware response-fetch packet. `FUN_000160A8` permanently ORs `0x08`
  into global legacy INTEN and commits it via `FUN_00012EAE ->
  FUN_00011E46`. Our old bounded polling path never set that bit; the x64
  ISR's `status &= InterruptEnableShadow` would discard INTST 0x08.
- Original ISR `FUN_000108D6` acknowledges INTST 0x08 with BAR1 CLRIRQ=2,
  then records/acks INTST and queues the DPC. This CLRIRQ handling already
  existed in x64, but was masked off.
- `FUN_00011390` raw assembly at 0x114A2..0x114C8 passes the stack
  out-parameter `&hwIntWord` to `FUN_000176A2`. This reads BAR1
  `HWInt` offset 0x410 as a DWORD, returns its LOW WORD to the DPC, and
  writes zero back to HWInt only when nonzero. Constructor
  `FUN_0001785B` proves transport+0xD8 maps to BAR1+0x410.
- If HWInt was nonzero, original DPC does
  `FUN_000157A6(commandStatus, hwIntWord)` and wakes CFDC2180 event.
  `FUN_000157A6` performs `pending |= enabled & hwIntWord`.
  The Ghidra C decompiler previously rendered that call with argument
  `0` due to failing to detect the stack out-parameter. The raw
  `raw_114f2.asm.txt` establishes the correct path.
- The separate `FUN_000176D0` checks RX_CONTROL ready bit 15 and signals
  the internal synchronous RX event; x64 continues using bounded receive
  polling, so only the missing HWInt command-status action is reinstated.

The patch enables INTEN bit 0x08 once before the first real 85FB firmware
fetch (matching legacy ordering), and handles pending INTST 0x08 in DPC by
reading/clearing real HWInt and latching only the enabled bits plus event wake.
The original local 0x88 acknowledgement, immediate IIMCL and CLRIRQ
acknowledgements and all DMA handling are preserved. No synthetic 0x0200
injection or timer/polling loop was added.

**Not yet compiled or hardware-validated after this change.**


## 2026-09-28 successor-chat pre-build review (HWInt/ProBus)

The successor chat verified that the user's uploaded ZIP contains the four
required historical ProBus traces, not a trace from the new driver. Legacy
hotplug alone has the three standalone 85FB pending `0x0200` notifications
(seq 14552/14588/14616) and genuine corresponding opcode-0x88 mask 0x0200,
followed by 0x82 and 0x4A. The three x64 traces predate patch `70716ba`
and have no pending 0x0200; preconnected probe enumeration remains distinct.
See `docs/probus-calibration-ab-comparison.md` for the checked counts.

The patch in `driver/Ioctl.c` and `driver/Acquisition.c` was source-reviewed
against original `FUN_000160A8`, `FUN_0001619A`, `FUN_000176A2`,
`FUN_000157A6`, `raw_114f2.asm.txt`, and the x64 ISR/DPC. No obvious
source-level WDK symbol/type error was identified; this is not a real
compile result. BAR1 HWInt `0x410` is in the established mapped range; the
real low-16-bit pending latch is protected with the same `LegacyEventLock`
as standalone status and opcode-0x88 acknowledgement. No driver changes
were made during this review, to protect the currently working acquisition.

One unproven concurrency risk to observe during the first test: existing
`LecCommitLegacyInterruptMask` commits caller-computed full DWORD masks,
so a concurrent first-time receive-bit-0x08 enable and acquisition-bit-0x01
toggle could interleave. Do not change the established PCI/DMA code without
new trace evidence; check normal waveform startup, CFDC2138 completion, and
status/event flow before one controlled physical unplug/replug.

First perform the scope-side **Debug build-only check**, then use the existing
scope desktop `Run-LeCroy-XStream-Trace.ps1` helper for build/sign/load and
the single controlled AP015 hotplug test. Exact commands, stop criteria,
minimum idle intervals, and requested trace markers are in
`docs/next-chat-handoff.md`. No post-patch WDK build or real-hardware test
has yet been reported. CPU limit remains 5%.


## 2026-09-28 23:06: first real AP015 HWInt patch validation

The user's post-patch x64 session (file
`xstream_trace_20260928_230614.jsonl`, not checked into the public
repository) started with AP015 preconnected. The user raised AP015 A/div,
physically removed AP015, reinserted it once, and closed XStream. The user
reports all changes were recognized. Exact physical-action timestamps were
not recorded separately.

Observed, filtering the JSONL to `type=ioctl`:

```text
captured IOCTL records                       19,288
trace sequence range                      1..21,761
missing sequences / capture gaps         2,473 / 30
non-success NTSTATUS in captured calls              0
CFDC2110                                         14,742
CFDC2138                                          3,633
CFDC2138 returned byte-count mismatches               0
standalone 85FB/0x01                                720
  enabled mask                                    0x02BF
  pending 0x0080                                     718
  pending 0x0200                                       2
family-0/0x88 mask 0x0200                             2
family-1/0x82                                         2
family-0/0x4A                                         2
family-1/0x4A                                         2
```

At elapsed ~28.422 s, status seq 14951 returned
`000000000400BF020002`, then mask-0x0200 acknowledgement seq 14952 and
family-1/0x82 seq 14961. No follow-up 0x4A in this first event sequence,
consistent with probe removal. At ~31.275 s, seq 16639 reported pending
0x0200 again, ack seq 16640, 0x82 seq 16651, 0x4A setup seq 16678,
and 0x4A AP015 metadata seq 16679. Startup AP015 seq 509 and reinsertion
seq 16679 both return Information=270 and identical captured 128-byte
prefixes, also identical to the historical legacy metadata prefix.

All captured DMA IOCTLs return the requested byte count: 2,332 before
first event, 328 between, 973 after second event; nearest observed DMA
gaps around the event timestamps ~36 ms and ~397 ms. The last recorded
>=8192-byte transfer occurs at ~27.067 s, before the first 0x0200
notification, after which 1,024/2,048-byte transfers continue to capture
end (~39.58 s). This is compatible with the reported sequence of a
probe A/div change followed by unplug/replug, without asserting its exact
causal timestamp. JSONL alone cannot establish on-screen pixels, raw
interrupt rates, exact CPU consumption or the absence of unrecorded failed
IOCTLs in the snapshot gaps. No repeated 0x0200 status event storm is seen
in captured standalone status reads.

**The original missing asynchronous AP015 disconnect/reconnect recognition
is validated for the tested case.** The patch in commit 70716ba is now
hardware-exercised; no driver modification was required after source review.
Degauss/Auto Zero reply parity is still open and was not exercised by this
run. Complete context: `docs/next-chat-handoff.md`,
`docs/probus-calibration-ab-comparison.md` and `docs/runtime-trace.md`.


## 2026-09-28 23:16: Degauss/Auto Zero, clamp-state events and raw-reply correction

User procedure in `xstream_trace_20260928_231656.jsonl`:
Degauss (automatically followed by Auto Zero), open, close, open,
close-locked, with AP015 attached before XStream startup.
This is **post-70716ba / pre-3490709**.

The 35,458 captured IOCTLs span seq 1..39998 (~68.178 s),
with 39 snapshot gaps and 4,540 sequence numbers missing. All captured
NTSTATUS values are success. 27,100 CFDC2110; 6,701 CFDC2138;
every CFDC2138 return DWORD equals requested bytes from input offset 11.
There are 1,438 standalone 85FB/0x01 status reads with enable
`0x02BF`: pending 0x0080=1,432; exactly 0x0200=4; 0x0280=1;
exactly 0x0000=1. The five real 0x0200-bearing notifications are:

```text
~34.163 s  seq 19902 pending 0200 -> ack 19903 (0200) -> 0x82 19916 (0x00A7)
~41.812 s  seq 24430 pending 0280 -> ack 24431 (0280) -> 0x82 24443 (0x0058)
~57.832 s  seq 33777 pending 0200 -> ack 33778 (0200) -> 0x82 33787 (0x00A7)
~59.820 s  seq 34908 pending 0200 -> ack 34909 (0200) -> 0x82 34918 (0x0058)
~61.894 s  seq 36079 pending 0200 -> ack 36080 (0200) -> 0x82 36089 (0x00A7)
```

The first notification follows Degauss; the latter four align with
open/close/open/locked-close in that order. After combined 0x0280 ack,
seq 24437 returns pending 0 and 24438 acks mask 0. Both jaw state
words are empirical observations; a distinct locked bit was not proven.

Special-function packet comparison across old x86 and current x64:

- `...47 00`: x86 once at seq 24205 with result
  `0000000000000000000002000000FFFF`, x64 five times at
  seq 19883..19887 with `00000000000000000000040000000000`.
- `...47 12`: x86 two observed at seq 14895/14900 with the same
  0x0002/FFFF response; x64 five groups of five after Degauss/auto-zero
  and each jaw event, all with 0x0004/0000.
- Subsequent family-1/0x4A status: legacy `...F200`; current x64
  `...F700`, `...F100`, `...F300` depending on observed phase.
- Family-1/0x82, same request: x86 raw header reports received length
  0x0016 or 0x0006 with FF padding; x64 old raw header always reports
  0x0190 capacity with zero padding (despite aggregate Information=412).

**Static proof:** Original `ghidra_exports/selected/000167f4_FUN_000167f4.c`
allocates the requested response record, fills with 0xFF, receives via
`FUN_0001619A` into record+6 with requested capacity minus six, and
writes *actual_received* into WORD+4 on success. The old x64 raw branch
in `driver/Ioctl.c` instead zero-initialized result and wrote
`recordOutput-6` even when fewer bytes were received.

**New, unbuilt driver commit `3490709`:** only the raw 85FB formatter
now fills the record 0xFF, uses actual copied bytes in its length header,
retains family-1/0x99's established override, and leaves all actual
hardware-facing paths untouched. For identical two-byte hardware replies,
a post-patch 47 00 / 47 12 response is expected to match legacy
`...02000000FFFF`, but hardware's actual returned count is not independently
visible in the prepatch JSONL, so a new test is essential.
Preserve the successful hotplug and acquisition baseline.

Detailed comparisons and exact next test:
`docs/probus-calibration-ab-comparison.md`,
`docs/runtime-trace.md`, `docs/ioctl-map.md`, and
`docs/next-chat-handoff.md`.


## 2026-09-28 23:31: response-framing fix verified; jaw notifications absent

New user scope workflow: XStream with AP015 attached, Degauss, later
**manually trigger Auto Zero**, open/close clamp several times and exit.
Trace `xstream_trace_20260928_233125.jsonl` exercises host formatter
patch `3490709` on genuine hardware for the first time. Main result:
identical `47 00` requests at seq 8573/t~20.034 s and
seq 22156/t~40.675 s **both** return
`0000000000000000000002000000FFFF`, equal to original x86 legacy
seq 24205. Pre-fix trace `231656` had five closely repeated 47 00
requests with `...040000000000`; the fivefold 47 00 retry burst is
absent after correcting original `FUN_000167F4` 0xFF prefill and
actual-received-byte header. There are no `47 12` requests in this
manual-action trace, whereas `231656` had five five-request bursts
following automatic/probe-state notifications. Post-47-00 family-1/0x4A
`01 0A` response is `...04000000F300` both times; legacy gave
`...F200`. The bit-0 difference remains an undecoded board/firmware
status, not a host header problem. The same captured first 128 bytes
of 270-byte startup AP015 metadata match earlier x64 and legacy runs.

Trace counts: 27,947 IOCTLs, seq 1..30856, duration ~55.628 s,
29 snapshot gaps omitting 2,909 entries, none after ~28.646 s;
zero NTSTATUS failures observed. CFDC2110: 21,366. CFDC2138:
5,219, all exact returned vs requested length at input offset 11,
including 1,666 after the second 47 00. All 1,135 standalone status
queries report enable `0x02BF`, pending `0x0080`, and zero pending
`0x0200`. No family-1/0x82 appears; the five authentic events in
run `231656` do not appear after the formatter patch despite the
reported clamp movements. **447 status reads after t=40 s are recorded
without a subsequent trace-snapshot gap**. Whether physical jaw
movements were recognized in XStream was not described separately by
the user, and this IOCTL JSONL does not capture screen state or the
hardware ISR/HWInt latch itself. This is a meaningful unconfirmed
behavioral difference, not proof of a direct code-level interrupt
regression.

**No further code changes until a discriminating jaw-only test.**
Start with recognized AP015 and stable waveform, open/hold/close/hold
once *before* Degauss or Auto Zero; note UI display and relative
timestamps. Then, if normal, invoke Degauss and manual Auto Zero,
repeat one open/hold/close/hold pair and exit. Compare spontaneous
pending 0x0200 and follow-on 0x88/0x82 as well as actual 85FB reply
length/FF padding and DMA; keep <=5% CPU, stop on abnormal waveform.
Do not create fake pending bits or speculative BAR/DMA changes.

Source-detail and trace tables:
`docs/probus-calibration-ab-comparison.md`,
`docs/runtime-trace.md`, `docs/next-chat-handoff.md`.


## 2026-09-28 23:53: AP015 jaw open/close before/after calibration

Post-`3490709` trace `xstream_trace_20260928_235314.jsonl`
(user upload, **not** committed) implements the exact prior next-step
protocol confirmed by the user: AP015 attached and identified;
open/hold/close/hold once **before** any special function; Degauss
then **manual** Auto Zero; open/hold/close/hold once more; quit.
Actual hand-action seconds and user-visible jaw-state responses have
not been reported independently. This was the essential control
missing in previous post-fix run `233125`.

Duration 72.009226 s, 36,366 captured IOCTLs in seq 1..42227,
36 snapshot gaps omitting 5,861 sequence entries, last gap ends
t~48.479610 s. Zero recorded non-success NTSTATUS. CFDC2110=27,834;
CFDC2138=6,818, **all** return the requested count from input byte
offset 11, including last DMA at t~72.008570 s.

AP015 startup metadata at seq 508, Information=270, has the same
captured first 128 bytes as previous `231656`. The first startup
family-1/0x99 raw result also now has the same first 128 captured bytes
as the original x86, including `FFFFFFFF...` trailing bytes.
The two identical, user-action-correlated 47 00 queries at
seq **23386** (t~40.155959 s) and seq **31835** (t~54.336670 s)
both return full output
`0000000000000000000002000000FFFF`, identical to legacy x86.
The paired family-1/0x4A `...01 0A` statuses are F3 at
seq 23388 and 31836, whereas the historical x86 reference is F2.
No five-call 47 00 retry burst, no `47 12` in this manual case.

The source-reported **software event enable** WORD remains `0x02BF`
in all 1,517 captured standalone 85FB/0x01 calls, but the **pending
WORD is 0x0080 in every one**, with zero 0x0200/0x0280 and no
0x88-mask-0x0200 or family-1/0x82. Partitioned by 47 00 times:
622 status reads before Degauss, 381 between Degauss and manual
Auto Zero, 514 after Auto Zero, all zero 0x0200. The last 648
status reads after t~48.480 s are free of snapshot gaps. Therefore
calibration is not a *necessary* trigger of the missing jaw
notifications; the current driver failed to expose 0x0200 in the
pre-calibration jaw state test too. Do not misidentify software
enabled 0x02BF with hardware BAR0 INTEN +0x084/0x08.

The original HWInt restoration had worked with the old raw host
response serializer (`230614` and `231656`). The two newer
post-`3490709` captures show no 0x0200 despite jaw movement. That
temporal build correlation warrants additional hardware evidence
but does NOT establish that new 0xFF reply padding directly causes a
missing hardware interrupt.

**One minimal next hardware test, no further code change first:**
physically unplug/replug AP015 during otherwise quiet XStream under
the unchanged current driver. If notification `0x0200` is also absent
for hotplug, use bounded passive readout of BAR0 INTEN 0x084,
INTST 0x080 and BAR1 HWInt 0x410 (and related shadow/ISR evidence)
to discriminate missing enable, assertion and DPC latching; if
hotplug works, isolate the probe-jaw path. Preserve <=5% CPU and
stable acquisition. Detailed comparative records:
`docs/probus-calibration-ab-comparison.md`,
`docs/runtime-trace.md`, `docs/next-chat-handoff.md`.


## 2026-09-29 00:11: genuine hardware events still work with corrected 85FB serializer

The user performed several AP015 physical unplug/replug operations
with the unchanged post-`3490709` driver. They reported one
transient wrong-probe ("1/2 clamp") identification on first
reconnection, possibly due to plug seating; subsequent
reconnections recognized correctly. User trace (not public):
`xstream_trace_20260929_001152.jsonl`.

The trace contains **32,675** captured IOCTLs, seq 1..37623,
61.381146 s duration, 34 gaps omitting 4,948 entries, last gap
t~32.792085 s. All recorded NTSTATUS values are success.
24,976 CFDC2110 and 6,135 CFDC2138; **all** returned DMA DWORDs
equal requested DWORD at input offset 11, including last
1,024-byte DMA at ~61.380523 s. Standalone command status
85FB/0x01: 1,361 reads, enable always 0x02BF; pending
0x0080=1,348; 0x0200=11; 0x0280=1; 0x0000=1. Thus twelve
real 0x0200-bearing notifications and their matching 0x88 ack
and family-1/0x82 sequences; nine postdate last snapshot gap.
These are hard proof that the original 0x08 HWInt/INTST path is not
globally disabled by the FF-padding fix.

Four removal events at seq 16747 (~27.557 s), 25308 (~40.564 s),
27775 (~44.929 s), 30380 (~49.510 s) consistently yield 0x82
raw state WORD 0x03FF. Reconnection status clusters:
seq 19245/19249 (~31.662/31.718 s) produce 0x82 0x028C,
combined mask pending 0x0280, then 0x82 0x0058, but **no**
subsequent 270-byte AP015 metadata (likely associated with
the user's wrong first probe display; no proven meaning for
0x028C); seq 26308/26320 (~42.336/42.386 s) produce
0x00A9 -> 0x0058 and AP015 metadata seq 26343;
seq 29173/29187 (~47.306/47.367 s) 0x00AA -> 0x0058,
metadata seq 29208; seq 32018/32062 (~52.275/52.405 s)
0x00A9 -> 0x0058, metadata seq 32068.
Startup metadata seq 509 plus all three normal reinsertions have
Information=270 and **identical captured first 128 bytes**,
including literal AP015.

Post-formatter family-1/0x82 reply format is now hardware exercised:
first removal result actual raw length 14 (`0x000E`);
remaining eleven actual length 6 (`0x0006`); all
unused captured output bytes 0xFF. Three automatic/reidentification-
associated family-0/0x4A `47 12` requests at seq
26385, 29250, 32096 each return exact original x86
`0000000000000000000002000000FFFF` **once**, followed
by status F7 at seq 26386,29251,32099.
No prior five-attempt 47 12 bursts. Meaning of F7 vs
historical x86 F2 and actual physical Auto Zero quality is
not established by IOCTL data alone. The user's manual Auto Zero
in earlier tests correlated with a `47 00` request instead.

**Updated engineering decision:** Do not revert 3490709 or change
already functioning PCI, IRQ and DMA based on earlier missing
jaw-only 0x0200 in 233125/235314. The global HWInt
notification route is demonstrably alive for physical hotplug.
One test of jaw-only open/hold/close/hold before and after
ONE normal physical unplug/replug, without Degauss/Auto Zero,
will distinguish whether proper reidentification changes
jaw-event behavior. Preserve the <=5% CPU limit and keep
raw probe traces off the public repository.

See full A/B and timeline:
`docs/probus-calibration-ab-comparison.md`,
`docs/runtime-trace.md`, `docs/next-chat-handoff.md`.


## 2026-09-29 00:20: XStream unlock warning refutes previous jaw failure interpretation

The user challenged our previous characterization: **XStream actually
reports the AP015 not locked when the clamp is opened and cautions
that the measurement may be inaccurate.** We had interpreted
absence of captured `0x0200` in jaw-only traces `233125`/
`235314` too strongly. It meant only that those *recorded*
standalone status replies did not show a fresh 0x0200, not
that XStream lacked all jaw-state information or failed to warn.
Correct this language in any future handoff/answer.

Newest unchanged post-`3490709` trace
`xstream_trace_20260929_002051.jsonl` directly captures four
0x0200-bearing notifications, all after its final snapshot gap
(~23.382490 s), each acknowledged by family-0/0x88 and
followed by actual-length-0x0006, FF-padded family-1/0x82.
Status sequence:
```text
t24.623  seq14888 pending 0200 -> ack14889 -> 0x82 14891 state 0058
         -> family0/0x4A 47 12 seq14913 original FF-padded result
         -> family1/0x4A status seq14916 F7  (open/unlocked)
t35.038  seq21435 pending 0280 -> ack21436 -> 0x82 21448 state 03FE
         -> intermediate pending 0000 seq21442
t37.458  seq22833 pending 0200 -> ack22834 -> 0x82 22843 state 0057
         -> seq22877/22878 AP015 metadata (same captured 128-byte
            prefix as startup seq506, Information=270)
t49.861  seq30553 pending 0200 -> ack30554 -> 0x82 30563 state 00A7
         -> family0/0x4A 47 12 seq30580 original FF-padded result
         -> family1/0x4A status seq30581 F3  (closed)
```
This supports the same `0058/open -> F7`,
`00A7/closed -> F3` correlation as previous pre-formatter
`231656`. The two F statuses differ by `0x04`, but the formal
bit meaning is not yet recovered. Distinct 03FE/03FF and 0057/0058
word variants warrant descriptive comparison, not premature
per-bit interpretation. This real captured event stream and
the user's visible warning refute a blanket jaw detection
regression. No code changes made.

Capture metrics: 31,200 IOCTLs, sequence 1..35955, 32 gaps
omitting 4,755 entries, no recorded NTSTATUS failures;
1,292 standalone 85FB/0x01 (pending 0080=1,287,
0200=3, 0280=1, 0000=1), 23,896 CFDC2110,
5,839 CFDC2138 all requested/returned byte lengths equal.
Last DMA t~58.367987 s, 1,024 requested/returned.
Full data: `docs/probus-calibration-ab-comparison.md`,
`docs/runtime-trace.md`, `docs/next-chat-handoff.md`.


## 2026-09-29: user clarification of ADC -> front EEPROM/I2C -> probe-control I2C architecture

The user supplied an important **physical front-end** fact:
the probe is first detected through its analog ADC identification
value; based on that value XStream recognizes the electrical
classification as a ProBus probe, then reads an EEPROM located
at the scope front via I2C. Physical probe control is also by
I2C. Exact numerical ADC values, I2C address and transactions,
EEPROM bytes, and which board firmware or XStream layer issues
each electrical operation are not yet captured or statically
decoded.

**Do not conflate host register interfaces with external I2C:**
family-0/0x90 is source-recovered as BAR1 SPICTL/SPIDAT/SPIDIN
host-to-board serial transactions, including selector-0x0E
144-bit packets; their relationship (if any) to the actual
front-panel I2C controller is unknown. Similarly,
firmware-forwarded `0x4A` replies containing ASCII AP015 are
consistent with EEPROM-driven identification but the present
IOCTL trace does not prove which reply bytes are direct EEPROM
data. Authentic `0x0200` from BAR0 INTST bit 0x08 and BAR1
HWInt is a host notification channel, not automatically the
physical lock sensor or an I2C transaction. The observed jaw
open/closed `0x82=0x0058/F7` and `0x82=0x00A7/F3`
correlations and the user's XStream warning remain true.

**The transient first wrong "1/2 clamp" recognition in trace
`001152`** had 0x82 `028C -> 0058` and did not trigger the
usual AP015 270-byte host metadata query. ADC classification,
connector settling, EEPROM I2C, and board-state sequencing
are now *separate testable candidate stages*, not a diagnosed
I2C/EEPROM failure. Preserve the working host formatter and
HWInt/PCI/DMA; investigate low-level I2C/ADC only when an
actual unresolved probe behavior warrants it.

Canonical architecture document:
`docs/probus-detection-i2c-architecture.md`. Source-backed
host/board mappings: `docs/ioctl-map.md`,
`docs/hardware-register-map.md`. Runtime A/B:
`docs/probus-calibration-ab-comparison.md`.


## 2026-09-29: original PCI interface and acquisition-board schematics reviewed

**Source uploads reviewed as actual schematic images:** `PCI Card.pdf`
(single-sheet A2 Perigee LLC PCI card diagram dated 2003-03-12)
and `Overview.pdf` (single-sheet A2 LeCroy acquisition-board
top-level overview, model/drawing header `901586-XX`).
The original files are NOT in the public repository; the
latter visibly carries a proprietary-information notice.
Only detailed derived factual observations have been added
in [`docs/pci-card-acquisition-board-topology.md`](docs/pci-card-acquisition-board-topology.md).

**PCI card structure:**
- The normal conventional PCI AD/control groups cross multiple
  `PI5C3861` bidirectional bus-switch ICs into `U3 XC2S200E`
  Spartan-IIE FPGA. The design note explicitly describes the
  5 V to 3.3 V bus interface without driving 3.3 V back to
  5 V. The PCI-side FPGA and physical INTA# routing are
  visible in the one-page sheet; don't assume this U3
  is also the acquisition board's ADC+MAM FPGA.
- A PCI-card **`U11 DS2433`**, labelled `ID Chip`,
  is wired to the `ID_DATA` FPGA net. This strongly
  corroborates the original-driver BAR2+0x040 `ONEWIRE`
  and `IOCTL_GET_DALLAS_ID`, `READ_DALLAS_MEMORY`,
  `WRITE_DALLAS_MEMORY` path as **PCI-card-local
  identification/storage**. The sheet does not show
  U3's internal BAR2-to-pin RTL; preserve that limit.
- **`U6 XC18V02` is an FPGA configuration PROM**,
  separate from the DS2433 and the front-panel
  I2C EEPROM. The sheet shows assembly alternatives
  for PROM boot (`R60/R63/R66`) versus remote
  configuration over link (`R88/R89`); *which are
  actually fitted on this instrument is not known*.
- `J1` is a **40-pin Receive Header** and `J2`
  a **40-pin Transmit Header**, with separately named
  differential `CLOCK_P/N`, `D0..D11_P/N`,
  `SYNC_P/N`, `RESET_ERR_P/N`, and stable status
  lines. Receive and transmit resistor networks
  are separately drawn. Exact framing, clocking
  and acquisition-side link endpoint are NOT
  established by this schematic.

**Separate acquisition-board overview:**
- `Power Conv/Filters (PC)`, `UP Control (UP)`,
  `Timebase (TB)`, `ADC+MAM (AM)`,
  `ADC+MAM (AM2)`, `FPGA's (FP)`,
  four channel FE blocks and `EXT` appear as
  individually labeled functional areas.
- The drawing visibly shows a shared named
  `I2C(0:5)` interconnect between UP/front-end
  areas, while `SPI_IO(0:40)`, UP `UC_SPI(0:4)`,
  `Voltage_Monitor(0:32)`, `MTT_FPGA(0:35)`
  and `ADC_CNTL(0:25)` are separate named
  nets. These facts corroborate the user's earlier
  ADC-based ProBus class detection -> front-panel
  I2C EEPROM identification -> I2C probe control
  architecture without disclosing the specific
  EEPROM IC/address or ADC discriminator.
- The **two acquisition-board ADC+MAM FPGA blocks**
  and dedicated FPGA subsystem must not be
  collapsed into the PCI-card U3 simply because
  both systems contain Xilinx logic.

**Driver interpretation:** XStream talks to PCI
interface FPGA and its driver-visible BAR/IOCTL
abstractions; the separate hardware link and
acquisition FPGA architecture now explain why
host `CFDC2110` and `CFDC2138` functions
must be read at the host/bridge protocol boundary,
not mislabeled as direct physical probe I2C or
the whole ADC-board firmware. The local
family-0/0x90 `BAR1 SPICTL/SPIDAT/SPIDIN`
source mapping does not override the user's
physical front-panel I2C statement. `0x0200`
HWInt and `0x4A/0x82` replies remain host-level
status/metadata, with no direct I2C edge capture.
The architecture evidence does NOT warrant
new speculative PCI, ISR, DMA or probe patches.
Keep previous successful actual hardware validations.

Related:
`docs/hardware-register-map.md`,
`docs/ioctl-map.md`,
`docs/probus-detection-i2c-architecture.md`,
`docs/next-chat-handoff.md`.


## 2026-09-29: all five ProBus ports are I2C; DS2433 holds XStream licensing

The user confirms the **five front physical ProBus
probe sockets** use I2C exclusively for EEPROM
identity and all probe controls. ADC analog
classification first selects ProBus type.
There is **no external SPI to an attached
ProBus probe**. The board's SPI path is
believed to address internal ADC, voltage
reference and similar configuration devices.
Our original family-0/0x90 `BAR1
SPICTL/SPIDAT/SPIDIN` implementation is
still correctly called a host/board SPI helper;
do not claim it is the physical probe
protocol. Specific SPI selector-to-device
mapping is an open RE objective, rather
than a reason to dispute the user's
confirmed physical I2C topology. The
board Overview's `I2C(0:5)` notation
alone does not establish the number of
connectors: the count of five comes
from the user.

The user's further clarification:
the PCI-side `U11 DS2433` 1-Wire chip
contains XStream **license keys**.
Whether these are cleartext is presently
only the user's tentative understanding,
to be evaluated privately from an
owner-authorized image. The unique
DS2433 8-byte ROM ID is separate
from its 512-byte EEPROM. The
original x86 binary has
`0x00223080` ID, `0x00223084`
read and `0x00223088` write
(32-byte chunks/read-back verification).
Current native x64 source defines and
implements the first two but has **NO
write/erase IOCTL**. The previous
`lecdiag` tool printed an EEPROM
hex dump but did not persist a binary
backup. Source commit for new
`lecdiag dallas-backup <file.bin>`
adds a strictly read-only, private
backup that checks stable ROM ID,
two complete identical 512-byte
images, CREATE_NEW no-overwrite,
flush/reopen/byte-compare and exact
512-byte file length. This C source
has NOT been compiled or run on
the scope yet. `.gitignore` now
excludes `license-backups/` and
`*.ds2433.bin`.

Test sequence: (1) build lecdiag
on Windows; (2) collect two unique
full backups with XStream closed;
(3) ensure their SHA-256 hashes
match; (4) retain another copy
outside repo; (5) privately inspect
license data format only if needed;
(6) reconstruct and bench-test
the original DS2433 writer on
a **disposable spare device**, not
the installed licensing chip.
A backup is not proof that restore
or application licensing survives
a destructive edit. **Never start
by erasing the only licensed EEPROM.**
The user explicitly suggested backup,
delete and write functionality tests,
but no destructive test was executed
or authorized through this message.
No sensitive/license bytes enter
public logs or commits.

Detailed instructions:
[`docs/dallas-license-memory-test-plan.md`](docs/dallas-license-memory-test-plan.md).
Do not update `driver/Ioctl.c`
write behavior on guesswork, or alter
current working PCI/IRQ/DMA.


## 2026-09-29: two matching private backups and synthetic offline image

The user submitted two matching SHA-256
hash results from two independent private
full DS2433 license-memory backup files.
The previous `lecdiag dallas-backup`
source already performs two identical
512-byte reads, before/after 8-byte
ROM ID comparison, CREATE_NEW, flush,
on-disk byte-by-byte validation. The
matching independently saved images are
additional positive real-scope evidence
of the **read** path, not the existence
of a write/restore capability or proof
that licenses are plaintext. Do not add
user-specific fingerprints or keys to
this public repo.

User explicitly asks to add an invented
license for a future write/read test.
Source added:
`scripts/create-dallas-dummy-image.ps1`
(PowerShell 5.1-compatible syntax intended;
not yet run on the actual scope). The
script takes two paths to private
512-byte backup files and a
`.ds2433.bin` output path; checks
distinct source paths and exact byte
equality; chooses the last fully
FF-filled 32-byte DS2433 memory page,
or aborts with no output if none.
It writes only the 30-byte ASCII
`FAKE-XSTREAM-LICENSE-TEST-ONLY`
within an entire **copy** of the
original, preserving all other
482 bytes including the last two
FF bytes of that page. Target file
uses CREATE_NEW, is re-read/verified,
and its suffix is Git-ignored.
The script does not read or modify
hardware, does not print license
bytes, and is **NOT** a syntactically
valid, activatable XStream license.
Even fully FF pages may be reserved;
do not infer application validity.

**No live chip write was performed.**
The native x64 driver currently
supports `GET_DALLAS_ID` 0x00223080,
`READ_DALLAS_MEMORY` 0x00223084,
but does not define/dispatch original
x86 `WRITE_DALLAS_MEMORY`
0x00223088. On-card license
write/erase without tested recovery
could disable a working scope.
Recover original 32-byte scratchpad,
copy authorization, timing, readback
verification; validate on a spare
DS2433 first, including after power
cycle. Further procedure, exact
private commands:
`docs/dallas-license-memory-test-plan.md`.
Do not alter working IRQ, PCI, DMA,
ProBus or license fields on assumption.


## 2026-09-29: no full-FF page; native XStream license UI and private redacted diff

The user executed `scripts/create-dallas-dummy-image.ps1`
on the scope using two separate private original
DS2433 backups that previously yielded matching
SHA-256. Result:
`No entirely 0xFF-filled 32-byte page found.
No image created; license layout analysis required.`
Correct fail-closed behavior: no derived
fake image created, input backups untouched
and physical 1-Wire device not accessed by
that **offline** script. An all-FF page was
a *safety precondition for our naive marker*,
not evidence of the authentic XStream license
format or free-slot allocation. Therefore
it is wrong to conclude that all 512 bytes
are committed, and wrong to choose a
partly-used page by guesswork.

User suggests using XStream's supported
license-entry dialog as a better observation
of the intended format and validation.
Entering an obviously fictional key may be
rejected without touching DS2433;
observe message and, if running x86 original
passive trace, whether `0x00223088`
actually occurred. Current x64 driver
still does NOT dispatch `0x00223088`,
so an x64 GUI attempt is not a completed
writer regression. A legitimate key
entry under original x86, if user chooses
it, modifies the only actual licensed
card and needs independent risk/restore
consideration. Do not automatically
prompt a destructive live test or publish
raw trace input that may include keys.

Added `scripts/inspect-dallas-image.ps1`:
strictly read-only and offline, 512-byte
validation, 16 per-page occupancy rows
showing ONLY numeric counts of 0xFF,
0x00, ASCII printable, other; optional
`-After` file prints changed byte ranges,
page indexes and counts WITHOUT raw
EEPROM bytes/strings/license hashes.
Use existing `original-a.bin` as
baseline and a newly captured private
read-only `after-xstream.bin` only
after an expressly authorized native
XStream action. No new kernel driver
changes or hardware writes made in this
turn. Full instructions in
`docs/dallas-license-memory-test-plan.md`
and current `docs/next-chat-handoff.md`.


## 2026-09-29: original Dallas image structure uses zero padding, native XStream A/B proposed

The user supplied the **redacted 16-page structure**
from the newly committed local
`scripts/inspect-dallas-image.ps1`:
first 6 pages 0x000..0x0BF contain
many printable bytes and mixed
zero/other/FF data; pages 6-10
(0x0C0..0x15F) are entirely zero
(**160 bytes**); page 11
(0x160..0x17F) contains 28 zeros
and 4 other bytes; pages 12-14
(0x180..0x1DF) are all-zero
(**96 bytes**); page 15
(0x1E0..0x1FF) contains 29 zeros
and three FF bytes. User-specific
actual EEPROM bytes and license
keys were NOT published. This
explains why the preceding
`create-dallas-dummy-image.ps1`
appropriately declined to choose
an all-FF 32-byte page: the image
uses 0x00 fill, and that alone
does not tell us which positions
are application-free. Do not
degrade the script's guard or
guess a location to place
fictional license bytes.

User offers to delete and later
re-add one XStream license via the
application's native UI to help
recover original license storage.
This may expose a genuine
before/after record rather than
a made-up 32-byte marker, but
must only be a separately
considered user-controlled step
after ensuring the SAME legitimate
key is recorded independently
and can be typed again. Prefer
original x86 XStream/driver
because its Dallas write
0x00223088 exists, unlike
replacement x64. Start with
a no-op control (baseline
backup; merely open/close
XStream license UI; another
read-only backup), then
if the user accepts risk,
one delete, backup, legitimate
same-key re-add, backup;
analyze only changed byte
offsets with
`inspect-dallas-image.ps1
-Before ... -After ...`.
Do NOT leak raw write input:
the trace preview can contain
license material. If re-add
is rejected, stop rather
than issue manual guessed
EEPROM writes. Two valid
backups do not prove x64
write/restore correctness.
Detailed plan:
`docs/dallas-license-memory-test-plan.md`.
No source/driver/hardware
write was performed in this
exchange.


## 2026-09-29 01:18: XStream Dallas DELETE reached missing native WRITE dispatch

**New real-hardware forensic result:** the
user invoked Delete for a known XStream
license on the x64 replacement and
reported that it remained listed after
XStream restart. They uploaded
`xstream_trace_20260929_011858.jsonl`
(**PRIVATE: the trace contains
license-bearing hex previews**).
608 IOCTL entries seq 1..608, no
omissions, duration 63.0224566 s.
Initial GET_DALLAS_ID seq 1,
READ_DALLAS_MEMORY seq 3
(input0/output512/info512), both
SUCCESS. At seq 522 t57.303140 s
XStream WOW64 sends one
`0x00223088 WRITE_DALLAS_MEMORY`
with 512 input bytes and zero
output. The native x64 driver
returns `0xC0000010
STATUS_INVALID_DEVICE_REQUEST`,
Information=0: **the only non-success
in the entire capture**. Immediate
READ_DALLAS_MEMORY seq 523
(t57.824097 s) succeeds 512/
512; its first 128 preview
bytes are identical to seq 3.
Final GET_DALLAS_ID seq 607
also succeeds. A partial private
offset-only comparison between
initial READ and the intended
write input shows 93 changed
positions in their common
first 128 preview bytes
(EEPROM pages 0..3); full
write preview is 256/512,
so the complete planned
application image is NOT known
from JSONL. Never publish/quote
the preview itself.

**Root cause proven:** original x86
has writer 0x00223088 at Ghidra
VA 0x11F54. Replacement
`driver/Ioctl.c` initializes
status to STATUS_INVALID_DEVICE_REQUEST,
does not define/dispatch the
writer, and consequently
returns that same exact status.
XStream is already implementing
the whole-image update; we
are missing the compatible
kernel DS2433 write ABI.
The persisted key is therefore
unsurprising. Do not diagnose
EEPROM failure, nonexistent free
slot, or a defective XStream UI
from this failed call.

**Next source-backed step:**
Export and inspect original x86
0x11F54 handler: added
`11f54`, `asm:11f54`,
`xref:11f54` to persistent
`ghidra_scripts/targets.txt`.
Run PC-side
`scripts/run-ghidra-analysis.ps1`
and retrieve export from GitHub;
recover original scratchpad
WRITE/COPY, 32-byte chunking,
readback verification, power/
timing and retry semantics.
Only then implement gated
writer with verified backup/
recovery route on a disposable
spare DS2433 before live
licensed-card mutation.
NO synthetic STATUS_SUCCESS
or dangerous raw overwrite.

**Trace hardening in user-mode
diagnostic EXE:** modified
`write_trace_entry_jsonl` in
`tools/lecdiag/lecdiag.c`
to suppress input hex for
WRITE_DALLAS_MEMORY 0x88,
output hex for READ_DALLAS_MEMORY
0x84 and GET_DALLAS_ID 0x80.
Future rebuilt `lecdiag`
JSONL includes
`sensitive_payload_redacted:true`
for these entries, preserves
numeric status/length/timing;
old trace/driver ring/KD logs
stay sensitive. Rebuild lecdiag
before any other license-UI
trace if exporting JSONL.

**Missing PowerShell script:** user
tried `inspect-dallas-image.ps1`
and got CommandNotFoundException.
This is a local checkout/cwd issue,
not evidence of EEPROM trouble:
the file is on `main`;
`Set-Location` to scope repo,
`git pull --ff-only origin main`,
then `Test-Path
.\scripts\inspect-dallas-image.ps1`.
An `after-delete.bin` is
NOT automatically created
by either XStream or trace;
get optional fresh whole-image
read-only backup after closing
XStream to compare all 512.
The dispatcher failure alone
already proves the core bug.

