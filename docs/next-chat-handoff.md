# Active handoff: first real 0x0022303C SetOneRegister call captured; caller ABI fully resolved (2026-09-30)

## Newest decisive evidence: KernelPCIRegisters write

Owner deliberately wrote back the SAME displayed register value through
`Service -> Development -> AladdinAcqBoard -> KernelPCIRegisters`.
Private original-x86 trace `legacy_xstream_trace_setregister_3.jsonl`
contains exactly ONE `0x0022303C` call, first ever seen by this project:

- input 266 bytes / output 0;
- STATUS_SUCCESS, Information=0;
- request is all zero except DWORD `+0x101 = 2`,
  DWORD `+0x106 = 2`;
- immediately before it, `0x00223040` reads index 2 = `TxCount`,
  BAR1+0x408, type 4, current value 2.

Therefore original setter semantics are proved:
`+0x101` = zero-based register-list INDEX,
`+0x106` = desired DWORD. The 266-byte query record is NOT echoed;
its physical offset at +0x101 is a different logical structure.

Owner's binary scan found `3C 30 22 00` exactly once in private
`lecaladdinhwaccesspcisvr.dll` (do not commit vendor binary).
Local x86 static analysis independently proves:
- routine image VA `0x10020B7C` zeroes 0x10A bytes;
- class member +0x310 = `RegisterList` / `m_cvEnumRegisterList`;
- selected index -> buffer +0x101;
- class member +0x314 = `PCIRegister` / `m_cvRegPCIRegister`;
- requested value -> buffer +0x106;
- then sends IOCTL `0x0022303C`.
Notification dispatcher calls this routine for PCIRegister change
notification flag 0x100. That is the exact GUI write path.

The uploaded DLL contains no literal `CFDC2130` and no literal
`0x00223088`; those are separate unresolved user-mode paths.

**NEXT engineering step:** decide/implement a HARDENED x64
`0x0022303C` compatibility handler rather than copying the original bug:
exact 266 bytes, index < active/known native count, resolve index only via
`g_LecLegacyRegisterList`, reject unknown/out-of-range values, perform
the mapped DWORD MMIO write, Information=0. Avoid exploratory writes;
the real same-value MMIO write already proved the path.

See [runtime trace](runtime-trace.md) and
[original register-list/write ABI](original-register-list-and-write-abi.md).

## Previous trace evidence (still valid, but setter is no longer unseen)

## Exact Service GUI page identified

Owner screenshot identifies the register diagnostics page as:

```text
Service -> Development -> AladdinAcqBoard -> KernelPCIRegisters
```

The visible list begins `TxControl, RxControl, TxCount, RxCount, SetIRQ,
HWInt, FVER, ...` in the same order as the reconstructed 43-entry
register list. Selecting TxControl shows `Adr: 400 H`, a hex value field
and a visible `Read` button. This directly links the page to the observed
`0x00223040` indexed register-read traffic.

There is NO visible Write button in the screenshot and the service trace
contains zero `0x0022303C`. Treat the value field as potentially
write-triggering on edit/Enter/focus-loss; do not experimentally modify it
on the real scope. Next safe step remains static user-mode analysis /
pending binary constant scan.

## Newest owner evidence: Developer/Service-menu original-x86 trace

Owner supplied private `legacy_xstream_trace_setregister_2.jsonl`
after traversing Developer/Service menus and reading many exposed
diagnostic/status pages. Parsed **13,076 valid JSON records**:
9,338 native IOCTL calls plus 2,020 Win32 IOCTL records.
Original main acquisition/control handle: **7,141 native calls,
all 7,141 STATUS_SUCCESS**.

Exact target result:
- ZERO `0x0022303C` SetOneRegister;
- ZERO `0xCFDC2130` serial-trigger FPGA;
- ZERO `0x00223088` Dallas WRITE;
- ZERO `0xCFDC21C4` raw register WRITE.

This is independent of the previous broad normal-UI trace, which also
contained zero target writers. The service traversal therefore strongly
supports leaving these hazardous top-level writers absent unless actual
user-mode caller evidence proves they are needed.

Service-only evidence relative to the previous normal trace:
- the only additional top-level LeCroy IOCTL code is `CFDC21C8`
  (two successful driver-build=1002 queries);
- eleven new CFDC2110 request shapes:
  eight family1/op42 JTAG query buffers (each twice),
  family1/A2 twice, family1/A1 twice, and one family0/op84
  selector 0x17 generic forwarded board request;
- A1/A2 are the already decoded Revision page:
  A1 -> BAR1 ACQFVER +0x0C, A2 -> BAR0 FVER +0x00;
- indexed `0x00223040` reads additionally request
  index 1 RxControl and index 2 TxCount;
- `CFDC21C0` reads BAR0 offsets 0x00,04,08,0C,10,14 twice.

Do NOT ask owner to deliberately click unknown service "write register"
or FPGA-programming actions just to force these dangerous IOCTLs.
The safest next evidence remains the already-running binary scan for
little-endian `3C 30 22 00` (0x0022303C). If scan hits an EXE/DLL,
inspect that binary statically and recover the setter request producer.

Sanitized evidence: [runtime trace](runtime-trace.md).
Raw trace remains private because it contains local/device-specific data.

## Previous broad normal-UI trace evidence (still valid)

## Newest owner evidence: private original XStream trace

The owner supplied `legacy_xstream_trace_setregister.jsonl` after
exercising: channel enable/disable, all channels to 20 mV/div,
coupling, bandwidth, timebase, sample rate, four-to-two-channel
10 GS/s, trigger CH2->CH1, positive->negative slope, Edge->Width,
and Width Less Than->Out Of Range.

Parsed file status:
- 199,741 valid JSON records + one incomplete final JSON line;
- 184,862 valid `nt_ioctl` records;
- main acquisition/control handle: **182,588 native calls, all
  `STATUS_SUCCESS`**;
- **ZERO `0x0022303C` SetOneRegister calls**;
- **ZERO `0xCFDC2130` serial-trigger FPGA calls**;
- ZERO `0x00223088` Dallas WRITE and ZERO `0xCFDC21C4`
  raw register-write calls.

This strongly indicates the two remaining dangerous register/FPGA
writers are NOT required for the exercised normal oscilloscope controls.
Do not implement them merely for a 27/27 headline.

The trace also contains a full original `0x00223040` register-list
reply of **11,438 bytes = 43 x 266**. Parsing all 43 public metadata
records (name, BAR, offset, type) gives **43/43 exact matches** to
the current native `g_LecLegacyRegisterList[43]` and the static
Ghidra ordering. Raw runtime values and the private raw trace are
NOT committed because the file also contains device-specific
Dallas/license and identifier material.

**NEXT evidence expected from owner:** pending binary scan of the
original XStream install for little-endian IOCTL constant
`3C 30 22 00` (= `0x0022303C`). If a matching EXE/DLL is found,
obtain that binary and statically trace the user-mode caller and
construction of the 266-byte setter request. The current trace alone
does not justify any new hardware-writing driver code or scope test.

Complete sanitized findings:
[runtime-trace 2026-09-30 section](runtime-trace.md) and
[original register-list/write ABI](original-register-list-and-write-abi.md).

## Previous static-analysis checkpoint (still valid)

## Live IOCTL monitor implemented in source; first Windows run pending

A native x64 user-mode monitor is now implemented at `tools/lecwatch/lecwatch.c`.
It consumes only the existing private `DEBUG_GET_TRACE` ring and therefore
requires **no kernel/driver change or reload**. Build helper:

```powershell
.\scripts\build-lecwatch.ps1
```

Then run `.\tools\lecwatch\build\lecwatch.exe` alongside XStream. Implemented
features: recent-activity LEDs, per-IOCTL rate/count/status, live log, semantic
confidence, focused CFDC2110 family/opcode decoding, Hide known, idle-baseline
suppression with request-shape novelty, errors/text filters, QPC user markers,
Start/End Action count summaries, double-click bounded payload details, gap
accounting and annotated JSONL save. Dallas payloads are redacted in details and
saved sessions. The GUI never sends legacy hardware controls.

**Verification status:** implementation/static review complete; **MSVC build and
real-scope live connection have not yet been run/reported**. Do not claim a
`lecwatch` PASS. First useful test is build-only, then launch with XStream and
verify connection, LEDs/log and one harmless UI action. See
[tool README](../tools/lecwatch/README.md) and
[live monitor design](live-ioctl-monitor-design.md).


## Long-term release target

The owner has made WHQL/WHCP certification an explicit end goal of the project.
The intended final state is a Microsoft-signed production driver that installs on
supported Windows x64 systems without test-signing mode. Before buying an EV
certificate, first perform local HLK/VHLK readiness testing on the actual WaveRunner
PCI hardware and resolve blocking test failures. Attestation/test signing is a
development step, not the intended final release state.




**DRY REGRESSION VERIFIED (owner, 2026-09-30):** after the PowerShell 5.1
hex-literal fix in commit `95be1ba4ebaf229b58d115fca3b5b9e12d0a267b`, the owner
reran `scripts/test-driver.ps1 -Mode Dry`. Driver build completed with
**0 warnings / 0 errors**, `lecdiag` built as x64 PE machine 0x8664, and the
hardware-independent contracts finished **8/8 PASS, 0 failed** with final
`REGRESSION SUITE PASS: Dry`. Dry mode is now runtime-verified on the Windows
development machine. The separate real-PCI safe ABI baseline remains 9/9 PASS.

## XStream E2E implementation added, runtime verification pending

`tests/xstream/test-xstream-e2e.ps1` is now integrated as
`scripts/test-driver.ps1 -Mode XStream`; `-Mode All` now runs Dry -> Hardware ->
XStream. The initial suite tests COM activation, C1/Horizontal automation access,
forced-trigger acquisition, waveform samples/DataArray, and reversible V/div,
timebase, coupling and bandwidth changes. Optional `-ExpectedProbeName`,
`-ExpectedAmplitudeVpp` and `-ExpectedFrequencyHz` assertions are supported.
AP015 basic identity uses legacy `Acquisition.C1.ProbeName`.

Do not claim XStream E2E PASS yet: this new script has not been executed on the
scope. Exact WaveRunner 6000 Browser paths are still needed for explicit trigger
configuration, two-channel/10-GS/s assertions and AP015 jaw/hotplug state.

## Hardware regression verified 11/11 on real scope

The owner executed `scripts/test-driver.ps1 -Mode Hardware` on 2026-09-30 with
XStream closed. Result: **11/11 PASS, 0 failed**, final
`REGRESSION SUITE PASS: Hardware`.

Key observed baseline values: stats version 1 / build 1002 / unknown IOCTLs 0;
logical BAR lengths 0x200 / 0x40000 / 0x200; PCI 1570:0005 at BDF 4:1.0,
command 0x0006, IRQ line 19 pin 1; START/FVER 0x00000002 matching BAR0+0.
CFDC2400 zero-mask boundary tests and CFDC2194 malformed-output rejection also
passed. No hazardous write path was exercised.

## New regression harness added (source-side, not yet runtime-verified)

A unified test entry point now exists at `scripts/test-driver.ps1` with
`-Mode Dry`, `-Mode Hardware`, and `-Mode All`.

- Dry: builds driver + lecdiag by default, then runs
  `tests/dry/test-source-contracts.ps1` without opening any device.
- Dry `-SkipBuild`: fast source-only ABI/contract checks.
- Hardware: delegates to the established safe 9-check real-PCI ABI suite.
- All: Dry first, then Hardware.
- XStream E2E is intentionally deferred to the next layer.

The dry suite freezes build 1002, selected IOCTL numeric values and dispatch
references, public packed ABI size guards, separation of private debug controls,
and continued absence of the three hazardous original writers
(`0x0022303C`, `0x00223088`, `0xCFDC2130`) from the native driver source.
It is intentionally NOT a fake hardware emulator.

Documentation: [regression test architecture](regression-testing.md).
The new runner/dry suite have NOT yet been executed on Windows and must not be
reported as PASS until the owner supplies actual output. The older hardware
safe-ABI result remains the separately reported 9/9 PASS baseline.

## Current authoritative state, no additional scope work needed now

The owner's THIRD separate-PC read-only Ghidra
run was pushed as
`00eb49db5efe98042df47ba07a570017cd37419d`.
It has ALREADY BEEN FETCHED AND ANALYZED,
alongside the first/second exports. See the
new comprehensive English documentation:
[original two-array 43-register architecture
and write-IOCTL ABI](original-register-list-and-write-abi.md).
Do not ask the owner to repeat those three runs.

Important new closure:

1. The original `CKeRegisterList` lives at
   hardware-subobject+0x11EE, initialized by
   `FUN_00013434`. It has separate dynamic
   register-wrapper pointer and 266-byte
   metadata arrays, each with capacity,
   grow increment=1, lastIndex initially=-1
   and pointer/status fields.
2. `FUN_00012184` is the newly proved dynamic
   pointer-array growth/copy/free routine;
   `FUN_000121FA` handles 266-byte metadata
   growth. Both return 0xC000009A on allocation
   failure. `FUN_00013FA6` checks first append
   success, but does not verify second metadata
   append success before returning its own bool.
3. Exact original common list receiver is passed
   from `FUN_00014847` through
   `FUN_000159E2 -> FUN_0001785B`: six
   transport regs first, 15 common direct
   regs next, then 22 conditional regs if
   `FUN_00012FDE` START/ITMODE init succeeds.
   Normal successful profile intends exactly
   **43 entries indices 0..42**, matching
   existing x64 `g_LecLegacyRegisterList[43]`.
   Query-format list size = `43*0x10A =
   11,438 bytes (0x2CAE)`.
4. **Important differing semantics in the
   same input byte positions:** a list-QUERY
   record at `+0x101` contains the register's
   PHYSICAL BAR OFFSET, while missing original
   setter `0x0022303C` interprets that DWORD
   as the POINTER-ARRAY INDEX, with its
   value DWORD at `+0x106`. The original
   setter itself performs no index/lastIndex
   validation and calls `FUN_000107FE` for
   immediate MMIO. A read-query record may
   not be echoed as a setter request.
   Example GPIODAT BAR1 offset 0xC4 vs
   full-profile list index 42.
5. `CFDC2130` bit-bangs BAR1 GPIODAT
   `+0xC4`, updating mask `0xE000`
   in one FULL-DWORD write for EACH input
   byte. Another original helper
   `FUN_000120DC` clears bit 16 of
   the SAME register before regular
   transfer handlers `FUN_00012D6A`
   and `FUN_00012F30`. Potential concurrent
   GPIO ownership and edge ordering require
   evidence/serialization; do not synthesize
   test programming on the real scope.
6. The third absent original value
   `0x00223088` is the licensed Dallas
   EEPROM WRITE and remains deferred until
   disposable hardware exists.

**No kernel driver source was changed by
this static-only third analysis.** Native
top-level inventory remains 24/27
(one gated), three high-risk cases absent.
Latest owner's real-scope low-impact ABI
batch `9/9 passed; 0 failed`; corrected
PowerShell stderr-only harness has not
separately been rerun. Complete
post-CFDC2400 practical XStream/AP015
regression stays deliberately DEFERRED,
per owner's preference to group compatible
work into one substantive milestone.

**Best next engineering step:** consolidate
this static result (done in linked doc),
review existing original XStream IOCTL traces
or original user-mode producer of the
266-byte `0x0022303C` setter record before
even considering a safe indexed writer.
If such caller evidence is unavailable,
further arbitrary Ghidra batches do not
justify risk to the only licensed scope.
No fresh scope test or Ghidra command is
required from the owner as a consequence
of this analysis alone.

## Historical second-batch handoff (superseded)

## Immediate current state and next step

The owner has ALREADY pushed the second original x86
static analysis in commit
`92f8a7f67c751c764190206f79cd4bf9ec493753`.
It establishes that the 266-byte original
`0x0022303C` register-list data structure has a
dynamic pointer array and parallel record array:
`FUN_0001326E -> FUN_00012184` grows/adds a
pointer, while `FUN_00013230 -> FUN_000121FA`
grows/copies corresponding 266-byte records.
The constructor `FUN_00013434` initializes
`lastIndex = -1`, pointer capacity zero,
increment quantum one. Original write routine
`FUN_0001259A` directly dereferences
`pointerArray[callerIndex]` without checking
capacity/lastIndex in that function. Exact caller
record field offsets: index DWORD `+0x101`,
write-value DWORD `+0x106`. No direct hardware
test of this risky IOCTL is authorized.

Original hardware init `FUN_00014847` has 15 common
and 22 conditional `FUN_00013FA6` registration
call sites, but that is NOT proof of runtime entry
count. Another `FUN_000159E2 -> FUN_0001785B`
path contains six calls whose receiver/table identity
is still to be resolved. Original `FUN_000120DC`
also manipulates the `+0x318` BAR1 GPIODAT
wrapper: clears bit 16 (`& 0xFFFEFFFF`),
while original `CFDC2130` controls bits 15:13
(`0xE000`) for EVERY byte. Native GPIO
ownership/timing must be characterized before
porting serial-trigger FPGA programming.

**A THIRD batch of 23 original-binary read-only
Ghidra targets is NOW committed to
`ghidra_scripts/targets.txt`, not yet executed.**
It includes `12184`, `12166`, `1785B`,
`16C92`, `179E2`, ASM for `121FA`,
`159E2`, `13FA6`, `120DC` and related
XREF/field scans.

NEXT on the **SEPARATE Ghidra PC**, not Scope:

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
.\scripts\run-ghidra-analysis.ps1 -CommitMessage "analysis: resolve register-list growth and shared GPIODAT writers"
```

Runner auto pulls/exports/commits/pushes; fetch
new files on its return. This does NOT touch the
x64 kernel driver or hardware. The owner has also
completed scope safe ABI batch **9/9 PASS**;
the PowerShell 5.1 stderr presentation issue
was fixed in the harness without requiring a
repeat. **Complete XStream/AP015 regression stays
DEFERRED per owner's explicit milestone preference.**
No new driver code was added for original hardware
writers or licensed Dallas WRITE. Native original
IOCTL top-level source representation remains 24/27
(one deliberately gated).

Detailed findings:
[grouped ABI and missing originals analysis](
safe-abi-batch-and-missing-ioctls-2026-09-30.md).

## Previous checkpoint after first writer export

## Newest owner-run x64 scope tests

The owner executed `scripts/test-safe-ioctl-batch.ps1`
on their real installed PCI 1570:0005 native driver.
They supplied the terminal result:

```text
SAFE ABI BATCH: 9/9 passed; 0 failed.
All requested safe ABI checks passed.
```

The original PowerShell 5.1 harness wrote an additional
misleading `NativeCommandError` at former line
`$combined = & $diag @Command 2>&1 | Out-String`.
This is the PowerShell treatment of `lecdiag` native
stderr for intentionally invalid request lengths:
it is not a kernel/IOCTL test failure. The
**nine checks still passed** according to the
reported summary. `scripts/test-safe-ioctl-batch.ps1`
has been updated to use `Start-Process -Wait -PassThru`
with separate temporary redirected stdout and stderr,
capture of actual ExitCode and finally cleanup.
**The corrected harness has NOT been rerun**;
do not claim a second result. This correction
changes NO driver source, `lecdiag.c` or kernel binary.

This complements the earlier positive real-scope
`CFDC2400` zero-mask result
`input=4 output-capacity=0 returned=0`.
The nine-case batch includes PCI/build identification,
passive START/FVER comparison, two positive CFDC2400
zero-mask forms, two invalid CFDC2400 input lengths
and two invalid (non-latch-consuming) CFDC2194
output lengths. No nonzero artificial pending IRQ
bits or destructive MMIO/Dallas write are included.

## First write-path Ghidra export already returned; second batch now queued

The owner ran and pushed the first read-only original
writer analysis in commit
`2071cfa7d44199d7e0cde3a7e3cc77e209ffc414`.
`asm_asm_1259a.txt` confirms
`[registerTable+callerIndex*4]` (index from record
`+0x101`, value from record `+0x106`) immediately
feeds `FUN_000107FE` physical register write, with
no index validation visible in that local function.

`FUN_00014212` constructs a CKeRegisterList at
hardware-subobject `+0x11EE`, and `FUN_00014847`
contains 37 STATIC `FUN_00013FA6` registration call
sites (not a proved runtime table length). In order
to determine insertion/index bounds and which other
code owns shared GPIO/MMIO wrappers, 19 more Ghidra
targets have NOW been committed to
`ghidra_scripts/targets.txt`, including 1326E/13230
table append/copy, 13434 list construction,
134AE/133C4 teardown, full 14212/14847 ASM
and xrefs for 107FE, 13FA6, 14847.

**NEXT PC-ONLY batch (second run not yet performed):**

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
.\scripts\run-ghidra-analysis.ps1 -CommitMessage "analysis: resolve indexed register-list bounds and GPIO ownership"
```

No scope testing or XStream regression necessary now.
See [grouped ABI result and original write-path
analysis](safe-abi-batch-and-missing-ioctls-2026-09-30.md).

## Previous Ghidra run instruction (completed by owner)

## Immediate next action: PC Ghidra export, no repeated XStream test

Per the owner's explicit preference, **batch useful source
analysis and low-impact checks before ONE later full
XStream/AP015 regression**. Do not demand a full
regression after every tiny patch, nor a cosmetic
rerun of the already-reported 9/9 solely to avoid
PowerShell stderr decoration.

The twelve STATIC Ghidra targets for original
`0x0022303C` (index-based register-write,
`FUN_00012CAC`/`FUN_0001259A`) and
`0xCFDC2130` (serial FPGA programming
via `FUN_00011CFF` and physical BAR1 GPIODAT writes)
are already queued in `ghidra_scripts/targets.txt`.
Next on the user's separate Ghidra development PC:

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"
.\scripts\run-ghidra-analysis.ps1 -CommitMessage "analysis: inspect remaining register and serial-trigger write paths"
```

This pulls newest main, runs the existing read-only
Ghidra export against the original x86 driver, then
commits/pushes exported evidence. It does NOT install
anything on the scope. Inspect resulting register-list
structure/index bounds and GPIO pin/timing details
before contemplating a port of either write handler.
Neither should be probed with fabricated write
data on the working scope; licensed Dallas WRITE
`0x00223088` stays deferred to disposable hardware.
Native top-level represented count remains 24/27,
including one gated case.

**The complete post-CFDC2400 practical XStream/AP015
regression remains DEFERRED, not reported PASS**, until
the next useful combined development milestone.
Full grouped test and remaining-control analysis:
[2026-09-30 grouped ABI / static write note](
safe-abi-batch-and-missing-ioctls-2026-09-30.md).

## Historical pre-batch instructions (now superseded)

## Current owner preference and immediate task

The owner explicitly wants **more development/testing in a useful
batch before doing one lengthy XStream waveform/control/AP015
regression**, rather than full XStream re-testing for every tiny
IOCTL change. Maintain test status honestly: the newly installed
CFDC2400 implementation passed its real-scope zero-mask
positive ABI test, but its comprehensive post-change
XStream regression is **not yet performed**.

A new source-controlled scope script
`scripts/test-safe-ioctl-batch.ps1` (not yet executed)
performs **nine low-impact checks** with XStream closed,
WITHOUT any driver build/sign/reload, nonzero pending
IRQ injection, FPGA/serial-trigger programming or Dallas
writing:

1. `lecdiag build`: build 1002 and exactly four response bytes.
2. `lecdiag pci`: vendor/device 1570:0005.
3. `lecdiag start-register`: compares 0x00223044 with
   passive BAR0+0x000 read.
4. `CFDC2400` four zero bytes, output capacity zero:
   success and zero bytes returned.
5. Same four zero bytes, output capacity four:
   success and still zero bytes returned.
6. Malformed `CFDC2400` three-byte input:
   expected DeviceIoControl failure, Win32 87.
7. Malformed `CFDC2400` five-byte input: same rejection.
8. `CFDC2194` output capacity 28: rejected before
   consuming its error-status latch.
9. `CFDC2194` output capacity 30: same rejection.

The script refuses to run when XStream is active.
Optional `-IncludeErrorStatus` adds a TENTH,
**consuming** valid CFDC2194 29-byte read/clear;
the default deliberately omits it. Even a zero
CFDC2400 mask invokes the original-style existing DPC
dispatcher; it is not strictly passive.

**NEXT on REAL x64 SCOPE, XStream closed:**

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }
.\scripts\test-safe-ioctl-batch.ps1
```

Expected summary IF all pass:
`SAFE ABI BATCH: 9/9 passed; 0 failed.`
The owner has not yet run this new script.
No extra WDK build or driver installation is necessary
for this script-only addition. Report actual batch
outcome, then continue grouped development. Full
XStream regression is deferred as requested until
the next useful combined milestone.

## New source-only analysis of the three missing originals

- `0x0022303C`: exactly 0x10A-byte buffered
  SetOneRegister path. Original `FUN_0001259A`
  reads record index DWORD at +0x101 and value
  DWORD at +0x106, indexes dynamic register-wrapper
  pointer table without an evident bounds check and
  `FUN_000107FE` actually writes the selected MMIO
  register. Do not port/test blindly.
- `0xCFDC2130`: input byte stream drives BAR1
  GPIODAT +0x0C4, masked upper field 0xE000,
  one *physical register write per input byte*.
  Reproducing it requires pin/timing/ownership safety,
  not a superficial success stub.
- `0x00223088`: Dallas DS2433 licensing WRITE
  remains intentionally deferred until disposable
  hardware is available.

New static read-only Ghidra targets are queued but
not run in `ghidra_scripts/targets.txt`.
No code was added for these risky original values;
24/27 source top-level representations (one gated)
remain. See [grouped tests and static remaining
handler analysis](
safe-abi-batch-and-missing-ioctls-2026-09-30.md).

## Previous handoff: CFDC2400 zero-mask positive path

## Latest actual PCI validation (owner-supplied)

The owner executed the newly implemented CFDC2400 request
against the physical x64 PCI device (VEN_1570, DEV_0005).
Reported diagnostic output, with the unique Windows PCI
instance string elided here:

```text
.\tools\lecdiag\build\lecdiag.exe raw-ioctl 0xCFDC2400 00000000 0
Opened device interface: [real PCI 1570:0005]
IOCTL 0xCFDC2400 succeeded: input=4 output-capacity=0 returned=0
Output:
```

**CFDC2400 zero-mask positive ABI is now verified on the
actual installed scope driver.** Exact 4-byte input was
accepted, NTSTATUS was successful and the returned byte
count was 0. The full separate build/sign/load transcript
was *not* pasted for this result, so do not assert exact
compiler warnings, INF/CAT identity or other unverifiable
details. No nonzero software IRQ mask was injected.
Since original `0x114F2` directly calls DPC `0x11390`,
the successful zero-mask invocation also reached the new
handler's intended immediate-dispatch path, but it is
not an instrumented proof of individual internal branches.

**NEXT:** Have the owner start XStream using the *currently
installed CFDC2400 driver* and perform practical
waveform/amplitude/frequency, V/div/timebase,
coupling/bandwidth/trigger, two-channel/10-GS/s
where applicable, and AP015 recognition + physical
unplug/replug/open-jaw checks. Note IRQ or startup
anomalies if any. No repeated build/load or deliberate
nonzero injected interrupt is needed. Do not consume
`lecdiag error-status` concurrently with XStream.
After the owner reports a clean check, update the
working baseline; until then, the previous CFDC2194/
CFDC2190 build is the last owner-confirmed practical
XStream regression PASS.

## Previous handoff before the positive scope diagnostic

## Latest decisive evidence: Ghidra export complete

The owner's new PC Ghidra exports were committed as
`b7b31c8bf5a06e9621a3636776673b486f72bfe5`.
Original derived hardware-subobject vtable at
`0x1C62C`, slot `+0x24` at `0x1C650`,
points to **0x114F2**. The 0x114F2 thunk adjusts
`ECX -= 0x1E0` (hardware subobject -> main),
passes through two caller zero DWORDs,
calls **`FUN_00011390` synchronously**, and
returns `RET 0x8`. The historical 0x104A0 bare
RET belongs only to base subobject vtable 0x1C8BC;
it was never the invoked derived virtual method.

Before the thunk, original `FUN_00012EDE` validates
exactly a non-null 4-byte METHOD_BUFFERED request and
via `LAB_00012EC2` ORs the request DWORD into original
software interrupt pending bitmap `DAT_0001CE10`
under synchronization. Original full semantics:
software pending OR, **immediate DPC dispatch**,
success / Information=0, no speculative register write.

## Native patch source is committed, NOT yet hardware verified

- `driver/LecS65Drv.h` declares `LECS65_IOCTL_CFDC2400`
  and `LecInjectLegacyPendingAndDispatch`.
- `driver/Acquisition.c` implements atomic pending-mask
  OR and calls existing `LecInterruptDpc` immediately
  (raise to DPC IRQL temporarily only when required to
  satisfy `KeAcquireSpinLockAtDpcLevel`, then restore).
- `driver/Ioctl.c` validates exactly 4 input bytes,
  calls the shared helper, returns zero information.
- `docs/ioctl-map.md` counts 24/27 original
  top-level cases now represented (one gated), three
  absent: `0x0022303C`, `0x00223088`,
  `0xCFDC2130`.

No Windows WDK build/sign/reload or user-reported
XStream regression on this **NEW CFDC2400** source
exists yet. Preserve known-good CFDC2194/CFDC2190
recovery driver. Do not inject nonzero software
pending bits on the only working PCI scope.

## Immediate next real-scope action (XStream CLOSED)

First, **build only**; do not combine the first
compilation with driver installation:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }

& ".\scripts\build-driver.ps1" -BuildLecdiag
if (-not $?) { throw "CFDC2400 build failed; STOP" }
```

On a successful WDK build and with a known-good recovery
version preserved, run the established
`build-sign-load-driver.ps1` from an elevated PowerShell.
With XStream still closed:

```powershell
& ".\tools\lecdiag\build\lecdiag.exe" raw-ioctl 0xCFDC2400 00000000 0
if ($LASTEXITCODE -ne 0) { throw "CFDC2400 zero-mask test failed; STOP" }
```

A success returns zero bytes and accepts 4-byte
little-endian zero input. It is a low-risk ABI test:
the original code STILL invokes the DPC dispatcher
with mask zero to process any already pending sources.
Do not try nonzero synthetic pending bits; after success
repeat the established live waveform/control/two-channel/
AP015 test and report actual observations.

Full exact addresses, Ghidra files and architectural
mapping: [CFDC2400 focused investigation](
cfdc2400-software-pending-investigation.md).

## Historical pre-export plan, now superseded

## Latest finding and current required action

Original `CFDC2400` is METHOD_BUFFERED; `FUN_00012EDE` accepts
exactly a four-byte DWORD and a non-null input pointer, otherwise
returns `STATUS_INVALID_PARAMETER`. Existing original
`ghidra_exports/selected/raw_12ec2.asm.txt` reveals
`LAB_00012EC2` does:

```asm
00012EC9 MOV EAX,[0x0001CE1C]       ; caller DWORD
00012ECE OR  dword ptr [0x0001CE10],EAX ; sticky pending IRQ bitmap
```

The old `docs/ioctl-map.md` statement that its last virtual
`+0x24` call is a no-op at `0x104A0` was identified as a
**wrong/unverified object-vtable attribution**. Original dispatch
at 0x112EA passes `main+0x1E0` (hardware subobject), whose
active derived vtable is `0x1C62C`, not the main-object
vtable at `0x1C500`. The target pointer must be obtained
from **0x1C650 = 0x1C62C+0x24**. Additionally the
original `FUN_00012EDE` pushes two zero DWORD arguments
and performs no cleanup after the virtual CALL;
`0x104A0` ends in a bare `RET`, so treating it as this
particular call target is ABI-inconsistent. The pending-bit
callback is source-proven; the final virtual side effect is
**not yet identified**.

A provisional x64 software-only case was REVERTED immediately
upon finding the discrepancy. No new driver code is to be
built/loaded on the scope; the established, working
CFDC2194/CFDC2190 XStream baseline remains intact.
The original top-level coverage stays **23/27**, one gated,
four cases missing including CFDC2400.

`ghidra_scripts/ExportSelected.java` now implements
literal `dwords:<hexbase>:<1..64>` dumps and previews the
`+0x24` target instructions; `ghidra_scripts/targets.txt`
requests `dwords:1c62c:16`, `dwords:1c500:16`,
`dwords:1c8bc:16` and the surrounding ASM/XREFs.

**Only next action is a PC Ghidra export**, not a scope driver test:

```powershell
Set-Location "C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver"

& ".\scripts\run-ghidra-analysis.ps1" -CommitMessage "analysis: resolve CFDC2400 derived-vtable slot"
```

It auto pulls, exports and pushes. Examine new
`ghidra_exports/selected/dwords_dwords_1c62c_16.txt`
slot `+0x24`; if the target is nontrivial, export its
full function next. Only then determine whether native
`CFDC2400` can safely use `InterlockedOr` on
`InterruptPendingShadow` with any additional original
post-callback effects. Do not test a nonzero injected mask
on the only working scope. Details:
[cfdc2400-software-pending-investigation.md](
cfdc2400-software-pending-investigation.md).

## Last confirmed scope baseline (no new patch installed)

## Current validated operating state

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

## Historical hardware checkpoint immediately before XStream regression

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
- **Historical request (now owner-confirmed without observed malfunctions):** run XStream with the newly installed
  driver and check live waveforms, ordinary controls, two-channel/
  10-GS/s where practical, AP015 preattached identification,
  physical unplug/replug, jaw warning and any new error IRQ storm.
  Do not run `lecdiag error-status` while XStream is consuming
  the same sticky latch, and do not artificially trigger hardware
  faults or access licensed Dallas memory destructively.

## Historical source-recovery and pre-build plan (completed through idle diagnostic)

# Active handoff: CFDC2194 ISR producer recovered; x64 source patch awaits Windows build (2026-09-30)

## New decisive original-driver result

The user executed the new PC-side Ghidra export, producing commit
`540bf87c374d3ea256896c49fdecbeacca8592da`. New literal
field scan reports identify the previously missing software-latch
writer in original ISR `FUN_000108D6`:

```text
0x10958  OR dword ptr [ESI+0x134A],EAX  ; accumulated BAR0 ERRS
0x10A67  OR byte ptr [ESI+0x134D],0x80  ; persistent-error marker bit 31
```

Original `FUN_000115C4` constructs the hardware subobject at
`main+0x1E0`; original DeviceControl dispatch at 0x11322
passes the same subobject to `FUN_00012BAE` (CFDC2194).
Thus `subobject+0x116A == main+0x134A` exactly. The old
`field:116a` search missed this **real ISR producer** because
the two functions use different object bases.

Original INTST bit 1 (0x02) triggers BAR0 ERRS (+0x004) read and
OR into the software status. The ISR translates ERRS bits
10..14 to BAR1 CLRERR bits 0..4, acknowledges ERRS, and on a
1-us persistent bit-1/unchanged-ERRS check widens ERRM and
sets status bit 31. Original CFDC2190 accepts 29 input bytes,
requires type 2 at +4, and uses **nonzero DWORD +8** to set
INTEN bit 1 while writing its complement to BAR0 ERRM.
Original CFDC2194 accepts exact 29-byte output, returns DWORD 2
at +4 and accumulated error status at +8, zero elsewhere,
then clears the original status latch (Information=29).

## Source staged on main, NOT hardware tested

The x64 `driver/Acquisition.c` now has the source-backed
accepted-INTST-0x02 ERRS accumulation/acknowledgment and
persistent-error flag path. `driver/LecS65Drv.h` and
`Driver.c` hold/initialize the software latch/ERRM cache.
`driver/Ioctl.c` implements CFDC2194's 29-byte
read-and-clear, and corrects the previous CFDC2190
type/enable and ERRM inversion divergence. New
`tools/lecdiag/lecdiag.c` command `error-status`
validates and consumes the status reply with XStream closed.

**There has been no Windows WDK build, signed install or
hardware test of this new patch in the assistant environment.**
The last actually verified scope baseline is the successful
`lecdiag start-register` comparison (`0x00000002` on
both read paths) and subsequent owner-reported working
XStream/AP015 regression. The newly changed ERRM programming
must receive its own regression before being called working.

## Immediate next real-scope action

With XStream closed and known-good driver recovery available,
start with build-only in the scope's elevated PowerShell:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "git pull failed; STOP" }

& ".\scripts\build-driver.ps1" -BuildLecdiag
if (-not $?) { throw "Windows build failed; STOP" }
```

If the build is successful and driver replacement is ready, use
the established `.\scripts\build-sign-load-driver.ps1` in the
same elevated PowerShell. STOP at installation/PnP errors.
With XStream still closed:

```powershell
& ".\tools\lecdiag\build\lecdiag.exe" error-status
if ($LASTEXITCODE -ne 0) { throw "CFDC2194 diagnostic failed; STOP" }
```

Expect an exactly 29-byte type-2 structure with zero reserved
bytes; status DWORD may be zero when no enabled error IRQ has
occurred. This test **consumes** the sticky latch, so never
run alongside XStream. A successful zero reply proves the
IOCTL positive ABI, not real nonzero error capture or the
bit-31 retry branch. Then check normal XStream waveform,
settings, two-channel where practical, AP015 recognition/
hotplug/jaw warning and lack of new error-IRQ storms.
Do not inject artificial hardware faults or resume Dallas work.

Detailed source and caveats:
[CFDC2194 status-latch investigation](
cfdc2194-status-latch-investigation.md). Updated
[IOCTL map](ioctl-map.md) now counts 23 of the 27
original dispatch codes represented in x64 source, including
one deliberately gated; four top-level cases are still absent.

## Historical handoff (superseded by the new source-backed producer)

**Project direction updated (2026-09-29):**
User explicitly postpones virtual Dallas ROM/EEPROM
emulation and physical DS2433 isolation testing.
These are [deferred TODO items](TODO.md), not the
next task. The device and kernel source remain
untouched by this documentation change.

`README.md` has been **rewritten as a concise
canonical current-state overview**; no historic
trial/error, outdated milestone claims or
per-trace diary entries belong there. Retain
detail and prior investigation in the dedicated
`docs/` pages and this engineering handoff.
Active backlog still includes broader XStream
regression work and source-guided native
Dallas WRITE 0x00223088 (after separate
spare-chip verification); other features
are tracked in `docs/TODO.md`.



**Conditional failed-chip recovery design (2026-09-29):**
User correctly points out that replacement of a physically
dead DS2433 changes its *factory* ROM identity, whereas the
existing raw `original-a.bin`/`original-b.bin` contain
only 512 writable-memory bytes. The user's main displayed
Scope-ID component matches factory ROM serial bytes 1..3
as 24-bit LE. We have **not** proven that every XStream
license is bound to this identity: do not claim universal
license invalidation without original-code or replacement
hardware evidence. Capture the **complete original eight-byte
ROM separately**, including CRC8, while it can still be
read. A full, private recovery container must preserve both
ROM identity and 512-byte contents.

Two different recovery cases: (1) if original ROM and
EEPROM writing still work, program original EEPROM
content with an independently verified native writer;
(2) if original chip is dead or replaced, consider an
**explicit, opt-in virtual Dallas diagnostic mode**
serving backed-up original ROM and EEPROM to the
host via Dallas ID/READ interfaces, and clearly
handling any write to a distinct shadow state.
Such a mode would NOT rewrite a replacement chip's
factory ROM and is NOT a physical repair.
We have not proved host IOCTL substitution suffices
if FPGA/board initialization needs physical 1-Wire.
Details:
`docs/dallas-device-manager-recovery-design.md`.
Current x64 driver still does not implement 0x00223088
WRITE; no virtual mode or chip write has been implemented.

**Physical Dallas absence test plan (2026-09-29):**
User proposes temporarily disconnecting the DS2433 to prove
a virtual Dallas host recovery mode can support a failed
physical chip. DO NOT make disconnection the first step.
Preserve complete original eight-byte ROM ID (the current
512-byte raw backups do not include it), two verified memory
images and current x64 operating state. First implement
explicit software-only Dallas IOCTL ID/READ handling from
the archived same-chip image with no actual 1-Wire transfer
in those handlers. Treat any virtual WRITE as a separate
shadow-image action, not physical chip programming; no
such emulator/writer has been implemented yet. Verify
the emulator with chip attached, including proving it
uses the virtual bytes and not a live hardware read.
Only then, with powered-off and electrically checked
hardware, consider reversible isolation of the PCI
card `U11` from FPGA `U3` on `ID_DATA`.
Never hot-disconnect, short the bus or cut a PCB trace
on guesswork. Check whether PCI enumeration, driver
initialization and subsequently XStream function
without hardware response. Failure before IOCTL startup
means host-level emulation alone is insufficient.
Finally power down, restore link, disable virtual
mode and compare original ROM/EEPROM again. See
`docs/dallas-device-manager-recovery-design.md`.

**NEW CONFIRMED Ghidra and backup state (2026-09-29):**
The user confirms the Ghidra export was rerun
and their original eight-byte ROM was saved
separately in the private `license-backups/`
directory; actual identifying bytes were NOT
shared or committed. GitHub main now has
`ghidra_exports/selected/00016d90_FUN_00016d90.c`
and `00016f2c_FUN_00016f2c.c`, their XREF
reports, and ASM exports. Reviewed both:
`16d90` resets/presence-checks, SKIP ROM
`CC`, WRITE SCRATCHPAD `0F`, two-byte
little-endian address, up to 32 payload
bytes; it resets, READ SCRATCHPAD
`CC AA`, obtains TA1/TA2/E/S,
checks data bytes, resets, COPY
SCRATCHPAD `CC 55` with address/E/S,
then sleeps using relative -1,000,000
100-ns ticks (**100 ms**). Its own
retry loop has three attempts.
`16f2c` performs full requested-length
READ MEMORY starting at address zero
via `CC F0 00 00`; original
`11f54` uses its readback plus
`RtlCompareMemory`, <=32-byte chunks,
and up to three outer retries.
These are source-backed hardware
protocol facts and allow planning
an accurate x64 writer, but **no
x64 writer, virtual mode, hardware
test or device-manager UI has been
implemented yet**. Do not ask the
user for another export of the
same two functions.

Next: independently implement
read-only virtual ID/READ and
private shadow-memory handling,
verify with original chip attached,
then consider only a reversible
electrically checked chip-isolation
test. The real writer is a distinct
development track requiring spare
DS2433 validation. Updated detailed
technical write sequence:
`docs/dallas-device-manager-recovery-design.md`.


**Read this file and `AGENTS.md` before changing the driver.**
Conversation in German, repository documentation and source comments in English.
Repository: https://github.com/Battlecake91/lecroy_wr6k_64bit_driver;
active branch: `main`.

**2026-09-29 completed handoff:** original DS2433
full eight-byte ROM ID has reportedly
been saved privately; the second
Ghidra run successfully exported
original low-level writer `16d90`
and reader `16f2c`, including ASM.
See latest confirmed Ghidra state
above for command sequence and
pending x64 development. Do not
publish private ROM/EEPROM data.

**2026-09-29 additional evidence:** private Dallas ROM reads
at trace 011858 seq 1/607 are identical and CRC-valid,
with family byte 0x23. Serial bytes at offsets 1..3
as a little-endian 24-bit value match the primary
six-digit displayed scope identifier. The display
suffix is not yet mapped. The pushed original
writer wrapper FUN_00011f54 was reviewed:
up to 512 input bytes, <=32-byte chunks, full
readback/compare, up to three passes; called helpers 16d90 and 16f2c were subsequently exported and reviewed, as recorded above.
A software-only test image is not a replacement
for actual programmed EEPROM contents. Keep
private identifiers and license bytes out of
public artifacts. See the Dallas test plan.

**LATEST FEATURE REQUEST (2026-09-29):** User
wants Device Manager custom Dallas
EEPROM Properties tab and standalone
maintenance/recovery UI: preserve
the installed PCI DS2433 license image,
open and compare a dump, recover a
corrupted but electrically responsive
DS2433 and optionally edit a private
copy. Windows supports native x64
device property-page extension DLL
registered with `EnumPropPages32`;
avoid deprecated co-installers.
The current INF copies only SYS,
so the DLL and its packaging are
future work. A full manager should
remain separate from Device Manager.
Factory 64-bit Dallas ROM (family
0x23/serial/CRC) is IMMUTABLE and
separate from the writable
512-byte EEPROM; the user's
six-digit main Scope-ID comes
from ROM serial bytes 1..3 as
24-bit little-endian, while
the display's two-digit suffix
remains unknown. Rewriting an
EEPROM image onto a different
chip cannot recreate the old
ROM identity. Current x64 Dallas
ROM/read works; write 0x00223088
does not. Source-guided implementation using
exported helpers 16d90/16f2c,
spare-chip validation, full
readback and then optional GUI
are required. See
`docs/dallas-device-manager-recovery-design.md`.
No UI or kernel writer was
implemented from this design.

**LATEST ENGINEERING STATE:** The user attempted
to Delete one XStream license under the
current x64 replacement driver; after XStream
restart the key was still present.
**New exact cause is now PROVEN** by uploaded
`xstream_trace_20260929_011858.jsonl`:
XStream's 32-bit WOW64 process issues
**`WRITE_DALLAS_MEMORY` 0x00223088**
once at seq **522**, t~57.303 s, with
**512 input bytes**, no output. The x64
driver responds **`0xC0000010` /
`STATUS_INVALID_DEVICE_REQUEST`**
(Information 0), because current
`driver/Ioctl.c` defines/dispatches Dallas
ID 0x80 and READ 0x84 but NO WRITE 0x88.
An immediate 512-byte READ at seq
**523** succeeds; its captured first
128 bytes match the initial READ
seq 3. Thus no need to blame EEPROM,
license layout or XStream's UI: the
actual unsupported write ABI is now
exercised and is our next concrete
compatibility gap.

The uploaded JSONL contains preview bytes
of the potentially real licensing image
(256-byte write input and 128-byte
read outputs); **NEVER copy this raw
trace or its keys into the public repo**.
It has 608 contiguous IOCTLs over
63.0224566 s, 607 successes, only
one failure (the write). XStream's
attempted full-512-byte image differs
from baseline within 93 byte offsets
in their common first-128-byte prefix,
affecting pages 0..3; remaining
contents are unobserved in preview.
Added original-driver Ghidra
export targets `11f54`,
`asm:11f54`, `xref:11f54`;
run `scripts/run-ghidra-analysis.ps1`
on the PC with Ghidra, recover
actual DS2433 scratchpad/copy/readback
before any native x64 writer is coded
or activated. The diagnostic EXE
`tools/lecdiag/lecdiag.c` now redacts
Dallas license-bearing IOCTL hex
previews from **future newly rebuilt
lecdiag JSONL exports only**; this
does not change the kernel or retroactively
redact the current private file.
Safety: matching private backups are
not a proven restore path. Do NOT
repeat delete on existing x64 writer-less
driver or fabricate a STATUS_SUCCESS stub.
Detailed current test:
`docs/dallas-license-memory-test-plan.md`.

**Current scope/driver regression state:** The user's own visible XStream behavior
corrects the previous diagnosis: **opening the AP015 generates an
explicit not-locked warning about measurement accuracy.** Absence of
pending-0x0200 in earlier particular IOCTL captures `233125` and
`235314` never proved XStream failed to recognize the mechanical
state. In the new user-supplied scope trace
`xstream_trace_20260929_002051.jsonl` (same unchanged post-`3490709`
driver), four authentic 0x0200-bearing events all have matching
0x88/0x82; state `0x0058` at ~24.623 s correlates with opened
jaw and family-1/0x4A `F7`, while state `0x00A7` at ~49.861 s
correlates with closed jaw and family-1/0x4A `F3`. That identical
open/F7 and closed/F3 pairing already appeared in `231656`.
The event stream also includes `0x03FE` (~35.038 s, an apparent
removal state) and `0x0057` (~37.458 s), followed by proper AP015
reidentification. No generic HWInt, jaw recognition, PCI or DMA
regression is established. **Do not request another routine
jaw/hotplug test or modify driver code because of the superseded
inference.** The formatter `3490709` remains working.

## Requested future feature: Device Manager Dallas EEPROM tab / recovery (2026-09-29)

The user requests a recoverability feature for other
LeCroy owners whose PCI-side Dallas license
EEPROM was accidentally corrupted and needed
external chip programming: a custom **Dallas
EEPROM** tab in the Windows Device Manager
device Properties with **Backup / Open / Compare
/ Restore / Advanced Edit**. A normal native x64
**property-page extension DLL** can be registered
using device-specific `EnumPropPages32` in
the INF (Microsoft's documented Win32 mechanism).
Do not introduce deprecated/co-installer-based
property pages; those affect current Microsoft
driver signing. Because full recovery deserves
a larger protected UI, propose shared user-mode
logic and a standalone
`LeCroy Dallas Manager.exe`, with the optional
Device Manager tab showing hardware status,
backup and a recovery-manager launcher.
The current INF `driver/LecS65AcqDrv.inf`
has custom DataAcquisition class, ClassInstall32
and ONLY the SYS copied: no current property
page DLL or manager is implemented. Keep
the current driver package working and
check INF/catalog/signing separately.

**Correct physical identity distinction:**
DS2433 ROM is factory-programmed
family 0x23 + 48-bit serial + Dallas
CRC8; 512-byte EEPROM is distinct
license-data storage. Primary six-digit
Scope-ID prefix in the user's display
matches ROM bytes 1..3 as a 24-bit LE
integer across seven private captures.
The two-digit display suffix and complete
formatted field are not yet decoded.
Writing a 512-byte backup to a
replacement DS2433 cannot change
its ROM or necessarily recreate the
old scope/license identity. If ROM
cannot be read at all, same-board
software restore may be impossible;
the hardware could still need an
external programmer/repair.

**Recommended recovery design:** private
versioned backup container includes
original full ROM, 512 EEPROM bytes,
SHA-256, version and timestamp;
support legacy raw 512-byte `.bin`
as unbound without its ROM metadata.
During restore, read actual ROM and
CRC, require matching identity, capture
another backup of current contents,
show page diffs, serialize against
XStream, use original 32-byte
scratchpad/copy/verification behavior,
stop on first failure and read back
all 512 bytes. Advanced hex editing
acts on a separate private file copy
by default, not directly on live
licensing memory. Do not invent
license formats or genuine keys.
Optional temporary *virtual shadow
READ* may aid diagnosis but does
not physically repair EEPROM and
is not a substitute for a writer.

**Implementation staging:** (1) standalone
read-only manager on existing working
0x80 ROM/0x84 READ/lecdiag backup;
(2) recover Ghidra dependencies
`FUN_00016d90`/`FUN_00016f2c`,
port original `0x00223088`,
exercise on spare DS2433 including
post-power-cycle readback and
interruption; (3) enable verified
restore/expert offline image editing;
(4) integrate optional x64
`EnumPropPages32` property-page DLL
after packaging/signing checks.
NO UI or new kernel writer has
actually been added yet.

Canonical design:
[`dallas-device-manager-recovery-design.md`](dallas-device-manager-recovery-design.md).
Manufacturer DS2433 datasheet confirms
separate factory 64-bit ROM, EEPROM
16 x 32 bytes, 32-byte scratchpad,
and 1-Wire copy requiring stable
bus supply. Windows docs:
https://learn.microsoft.com/en-us/windows-hardware/drivers/install/specific-requirements-for-device-property-page-providers--property-pag
https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/removing-coinstallers

## Decisive Dallas write trace: XStream DID try; x64 returned invalid request (011858) (2026-09-29)

The user deleted a visible key via current
x64 XStream, but it reappeared after
application restart. This is explained by
the **actual write attempt and failure**
found in their newly uploaded private
`xstream_trace_20260929_011858.jsonl`:

| Relative time / seq | Control and length | Actual returned status |
|---|---|---|
| t=0 / seq 1 | GET_DALLAS_ID 0x00223080, output 8 | SUCCESS |
| t~0.804498 / seq 3 | READ_DALLAS_MEMORY 0x00223084, output 512 | SUCCESS |
| **t~57.303140 / seq 522** | **WRITE_DALLAS_MEMORY 0x00223088**, input 512, output 0 | **0xC0000010 STATUS_INVALID_DEVICE_REQUEST**, Information 0 |
| t~57.824097 / seq 523 | READ_DALLAS_MEMORY, output 512 | SUCCESS |
| t~63.022424 / seq 607 | GET_DALLAS_ID | SUCCESS |

There were 608 IOCTLs seq 1..608,
no snapshot omissions, duration
63.0224566 s. 607 NTSTATUS successes,
**one single failure, the WRITE**.
The first 128 captured READ output bytes
at seq 3 and 523 match (do not claim
full 512-byte equality from 128-byte
preview). The attempted 512-byte WRITE
has only its first 256 input bytes
captured; 93 differing offsets
between intended image and initial
read in their common first 128
bytes (pages 0..3). No raw
sensitive bytes or exact key hashes
are in docs.

**Source-supported direct explanation:**
`driver/Ioctl.c` initializes
`status = STATUS_INVALID_DEVICE_REQUEST`,
and has no `LECS65_IOCTL_WRITE_DALLAS_MEMORY`
case; that unknown IOCTL falls into
default and returns C0000010.
Therefore XStream **does attempt a
full 512-byte write** and receives
the expected unsupported status:
the key remaining after restart is
consistent with the device being
unchanged, not a strange license
parser/UI caching behavior. Its
post-write read still succeeds.
Do not repeat current x64 deletion
or attempt to force raw overwrite
of zero-filled pages.

**NEXT actual task:** source-recover
original x86 Dallas write handler
(`0x00223088`, Ghidra VA
`0x11F54`) including 32-byte
scratchpad/copy/readback and retry.
Added `11f54`, `asm:11f54`,
`xref:11f54` into
`ghidra_scripts/targets.txt`.
Run established PC-side
`scripts/run-ghidra-analysis.ps1`
to export it, then use source
evidence to plan a bounded,
restore-tested native x64 writer.
No IOCTL writer or fake success
has been added in this turn.

**Trace confidentiality change:**
The unredacted current upload
contains a 256-byte WRITE input
preview potentially comprising
real license fields and 128-byte
READ output previews. KEEP IT PRIVATE.
Modified `tools/lecdiag/lecdiag.c`
so FUTURE rebuilt diagnostic
JSONL exports have empty
`input_hex` for WRITE 0x88,
empty `output_hex` for
READ 0x84 and GET_ID 0x80,
and set
`sensitive_payload_redacted=true`
while preserving code, lengths,
status, ordering. This is
diagnostic-source-only:
old files and in-kernel
debug/ring memory are not
automatically sanitized.

**PowerShell path confusion:**
`inspect-dallas-image.ps1`
exists on `main` from an earlier
commit. Ensure
`Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"`,
`git pull --ff-only origin main`,
`Test-Path .\scripts\inspect-dallas-image.ps1`.
The earlier user's
`after-delete.bin` was
NOT auto-created by XStream
or the trace; `lecdiag dallas-backup
.\license-backups\after-failed-delete.bin`
must be run independently with
XStream closed to get an exact
whole-memory after snapshot.
This optional snapshot is
not needed to prove the current
missing-writer root cause.

## New redacted license-memory finding: zero-filled pages (2026-09-29)

The user ran the newly added redacted
`scripts/inspect-dallas-image.ps1 -Before
.\\license-backups\\original-a.bin` and provided
page-level FF/00/printable/other counts.
The useful structural result is **substantial
0x00 fill rather than 0xFF fill**:

- Pages 0..5 (0x000..0x0BF): many printable
  bytes, possibly records/header but exact
  license format NOT decoded.
- Pages **6..10** (0x0C0..0x15F):
  five entirely zero-filled 32-byte pages
  (**160 bytes**).
- Page 11 (0x160..0x17F):
  28 zeros plus 4 other bytes.
- Pages **12..14** (0x180..0x1DF):
  three entirely zero-filled 32-byte pages
  (**96 bytes**).
- Last page 15 (0x1E0..0x1FF):
  29 zeros and 3 FF bytes.

The no-full-FF-page abort of
`create-dallas-dummy-image.ps1`
was therefore expected, but **no full-FF
page does NOT mean the EEPROM is full**.
Do not equate wholly zero-filled pages
with free XStream license slots either:
reserved bytes and/or a global checksum
may exist. Keep raw EEPROM content and
user-specific hashes private.

**User offers native XStream delete+re-add
of one existing key.** That could be a
valuable original-format A/B, but only
after the legitimate *same key* is
independently documented/re-enterable
and the user accepts the risk of losing
a licensed feature. Current x64 native
driver still has NO write IOCTL
`0x00223088`; use original 32-bit
XStream plus original x86 driver for
a real native-app EEPROM write if
proceeding. Prefer a **no-op control**
original-driver backup before and after
merely opening/closing XStream's
license UI, followed by separate private
read-only images after ONE native delete
and after re-adding the SAME original key.
Use `inspect-dallas-image.ps1 -Before
... -After ...` to compare byte offsets
and page counts, never publicly expose
raw keys. Matching double-backups are
not proof that a native x64 restore
would succeed. If native re-add fails,
stop rather than raw-writing guessed
EEPROM offsets. Complete staged plan:
`docs/dallas-license-memory-test-plan.md`.

## Update: no all-FF page; consider native XStream license dialogue (2026-09-29)

The user executed the previously added offline
`scripts/create-dallas-dummy-image.ps1` against
two identical 512-byte backups. The script
**correctly aborted** because **no entire
32-byte page is all `0xFF`**. Hence
`fake-test.ds2433.bin` was **NOT created**.
The two private source images and original
PCI licensing DS2433 were unchanged.
This observation does NOT prove no free
application-level license slot: possible
alternate allocation/padding/record structure
requires actual data-format evidence.
Do NOT remove the FF-page safety check
to overwrite unknown live license records.

The user offers to try the **XStream Add License**
UI. This may reveal the application's
validation/storage process more faithfully
than inventing arbitrary EEPROM offsets.
Key discriminator: original x86 implements
`0x00223088 WRITE_DALLAS_MEMORY`;
native replacement x64 `driver/Ioctl.c`
still has no write case. An invalid/fake
license may be rejected by user-mode
validation **before** a write IOCTL,
which proves nothing about EEPROM writes.
Do not claim a real write, install working
credentials or erase licensed data merely
because read-only backups are consistent.
If passive original-x86 trace is captured,
extract only IOCTL number, in/out LENGTH,
status, chronological order. Raw
`0x00223088` input could contain keys;
never publish full input buffers.

Added independent **redacted read-only local
image analyzer**:
`scripts/inspect-dallas-image.ps1`:
before-only mode prints each of 16 pages'
FF, 00, printable and other byte COUNTS;
before/after mode additionally prints
changed byte offsets/ranges and affected
page indices, no secret byte values.
Use it on original backups now and, if
a user-authorized native XStream action
actually changes storage, on a newly
read-only-captured post-image to learn
the real layout without sharing keys.
Because `lecdiag` uses original legacy
interface GUIDs, an x86 build can also
access the original-driver ABI; otherwise,
after running original x86 XStream
and closing it, boot current x64
and re-run read-only backup on the
same physical DS2433. Avoid
concurrent exclusive driver opens.

**No kernel code or EEPROM writes were
changed by this finding.** A protected
spare DS2433 writer/power-cycle-restore
validation remains mandatory before
deliberate low-level modifications
to only licensed card. Details:
`docs/dallas-license-memory-test-plan.md`.

## Latest Dallas test: two matching backups, offline dummy image only (2026-09-29)

The user has now **run `lecdiag dallas-backup` twice**
and supplied the two matching **SHA-256** results.
This is positive validation of the backup path on
their real scope, subject to the fact that hashes
do not identify license-record formatting or
establish a safe write/restore path. **Never
store the particular image hash or raw bytes in
the public repository.**

The user asks for an invented extra license to
test EEPROM writing. Do **not** confuse a test
marker with a real XStream license or immediately
overwrite the only functioning installed license
storage. Added a safe private **offline-only**
preparation script:
[`scripts/create-dallas-dummy-image.ps1`](../scripts/create-dallas-dummy-image.ps1).
It takes two independent 512-byte backups,
verifies exact byte equality, searches for the
last completely `0xFF`-filled 32-byte page,
and creates a new 512-byte copy in which
**only 30 bytes** on that page hold the ASCII
marker `FAKE-XSTREAM-LICENSE-TEST-ONLY`.
It refuses no-page/invalid-length/mismatched
inputs and existing output filenames,
requires the Git-ignored `.ds2433.bin`
suffix and verifies the persisted file.
No secret bytes are printed; NO
DeviceIoControl, chip write or erase is
performed by this offline script.

Sample private invocation from scope repo,
assuming previous user backups were named
`original-a.bin`/`original-b.bin`:

```powershell
git pull --ff-only origin main
& ".\scripts\create-dallas-dummy-image.ps1" `
  -ImageA ".\license-backups\original-a.bin" `
  -ImageB ".\license-backups\original-b.bin" `
  -OutputImage ".\license-backups\fake-test.ds2433.bin"
```

**Remaining hardware gate:** x64
`driver/Ioctl.c` does not implement
original x86 `WRITE_DALLAS_MEMORY`
0x00223088. Even an all-`FF`
page is not proven to be a safe
license slot or excluded from any
global checksum. Recover the actual
DS2433 scratchpad/copy algorithm and
test write/read/power-cycle restore on
a disposable chip before exposing
the only licensed card to a write.
Details:
[`docs/dallas-license-memory-test-plan.md`](dallas-license-memory-test-plan.md).
The new PowerShell script has been
committed but not executed in the user's
Windows environment by the assistant;
do not claim on-hardware test-image
creation or write success.

## New Dallas licensing and five-slot I2C facts (2026-09-29)

The user explicitly corrects the architecture: the actual five
front-facing probe connectors use **I2C exclusively**; neither
probe EEPROM reads nor physical probe controls use SPI.
The separate SPI networks are believed to be primarily
used for internal ADC/reference/configuration ICs. This
is user physical-hardware knowledge, independent of
the source-proven host family-0/0x90 BAR1
`SPICTL/SPIDAT/SPIDIN` helper. The full
mapping of that helper to internal devices is
still open. The Overview's `I2C(0:5)`
signal range should not be equated with an
exact socket count without a child sheet.

The user's second correction is that the PCI card's
`U11 DS2433` is the **XStream license EEPROM**.
It has an independently readable eight-byte
one-wire ROM ID and 512-byte memory. The
user expects license keys may appear in plaintext;
do not assert/print/publish actual contents
before obtaining and examining a **private dump**.
The DS2433 is distinct from front-probe EEPROM
(I2C) and FPGA configuration PROM XC18V02.

**Existing x64 read-only coverage:**
`0x00223080` Dallas ID with CRC-8,
`0x00223084` 512-byte memory read.
Original-driver x86 `0x00223088`
write is recovered but **not in current x64
driver/Ioctl.c**; the CLI also previously had
only `dallas-id` and console hex `dallas-read`.
A strictly read-only `dallas-backup` binary
output command has now been implemented in
`tools/lecdiag/lecdiag.c`, source only:
it reads ROM ID, 512-byte EEPROM image twice,
ROM ID again, compares and persists to a
CREATE_NEW binary path, then verifies actual
saved bytes/EOF. No secret bytes printed,
no write/erase. Must still compile and undergo
first actual scope test. The `license-backups/`
folder and `*.ds2433.bin` are now Git-ignored.
Backup instructions, distinct ROM/memory roles,
and future guarded spare-chip write tests:
[`docs/dallas-license-memory-test-plan.md`](dallas-license-memory-test-plan.md).
**Never test erase/restore first on the only
working XStream-license chip**: a full dump
does not by itself validate the unimplemented
writer or recovery procedure. Keep scope CPU
and DMA constraints as before.

## New primary hardware sources: supplied PCI schematic and acquisition top level

The user has now supplied two one-page A2 PDFs as actual hardware
reference material:
- `PCI Card.pdf`: Perigee LLC schematic, sheet 1, dated
  2003-03-12; this shows the **separate PCI interface card**.
- `Overview.pdf`: LeCroy Corporation acquisition board
  **top-level** drawing (model header `901586-XX`).

Both were inspected visually, including the schematic/vector
content, not just the PDF text extraction. The Overview has
no regular extractable words. **These files are user uploads,
NOT source artifacts to be copied into the public GitHub repo:**
the Overview carries an explicit proprietary-information notice.
Derived, carefully qualified hardware observations are now
documented in the canonical
[`pci-card-acquisition-board-topology.md`](pci-card-acquisition-board-topology.md).

**Critical new PCI-side topology:**

- `U3 XC2S200E` **Spartan-IIE FPGA** receives conventional
  PCI signals through several `PI5C3861` bidirectional
  bus switches (schematic explicitly describes 5-V/3.3-V
  interfacing). `INTA#` is within the PCI/FPGA signal group.
- `U11 DS2433` is a physical PCI-card
  **Dallas 1-Wire ID/memory** IC, marked `ID Chip`, on
  net `ID_DATA` wired to FPGA U3. This gives the
  recovered original-driver `BAR2+0x040 ONEWIRE`
  and GET/READ/WRITE Dallas IOCTLs a concrete
  **PCI-card-local device candidate**. BAR2-to-ID_DATA
  FPGA RTL is not depicted, so treat the exact mapping
  as source-plus-schematic inference.
- `U6 XC18V02` is separate PCI Spartan
  **configuration PROM**, neither Dallas 1-Wire
  nor front-panel/probe I2C EEPROM. The schematic
  has **optional**, mutually alternative PROM
  boot (`R60/R63/R66`) and remote configuration
  **via link** (`R88/R89`) resistor stuffing;
  actual stuffing was not determined.
- `J1` is a **40-pin Receive Header**, `J2` a
  **40-pin Transmit Header**. Each has differential
  CLOCK, twelve differential D0..D11 data pairs,
  SYNC and RESET_ERR plus status/ground connections;
  separately drawn RX termination and TX resistor
  networks. These define a distinct FPGA-to-
  acquisition-board **physical link**, not ordinary
  PCI continuing through the front ends. Link
  encoding/protocol and exact endpoint/RTL are
  not shown.

**Separate acquisition-board overview:**
`Power Conv/Filters (PC)`, `UP Control (UP)`,
`Timebase (TB)`, two different `ADC+MAM`
(`AM` and `AM2`), `FPGA's (FP)`, four
channel front ends plus `EXT` appear as
distinct blocks. It explicitly labels
`I2C(0:5)` in UP/front-end region,
plus **separate** `SPI_IO(0:40)`,
`UC_SPI(0:4)`, `Voltage_Monitor(0:32)`,
`MTT_FPGA(0:35)`, `ADC_CNTL(0:25)`,
`FE_CHx_ADC(0:1)`, FPGA/ADC and JTAG
bus groups. This independently corroborates
I2C distribution and hardware separation, **not**
the precise ADC channel for the user's ProBus
recognition value or the front EEPROM address.

**Three memories and multiple FPGAs MUST NOT be confused:**
PCI `U11 DS2433` = 1-Wire board ID;
PCI `U6 XC18V02` = configuration PROM;
front-panel/probe identification EEPROM = I2C after
ADC ProBus classification (user-provided).
PCI Spartan U3 is **not** acquisition-board
AM/AM2/FP. Family-0/0x90 host BAR1 SPI
register operations are not automatically the
external physical probe I2C bus. Nor do
0x0200 HWInt, 0x82 state or 0x4A metadata
directly reveal ADC/I2C electrical transaction bytes.
No driver source changes follow from this topology
alone; keep working verified HWInt, 85FB
framing and DMA untouched.

## Front-panel probe hardware architecture (user clarification, 2026-09-29)

The user has supplied **physical hardware/protocol information**
that changes how we should interpret the trace, not the working
driver implementation:

1. The first probe-class detection happens via an **analog ADC
   identification value**. XStream interprets that value as
   designating a **ProBus** probe.
2. **After** the ADC/ProBus decision, the **front-panel EEPROM**
   is read over **I2C** to identify the device.
3. Physical **probe control also uses I2C**.

These are separate stages. The exact ADC channel/value/threshold,
front I2C controller, EEPROM slave address/bytes, and any
host-to-I2C firmware bridge remain **unidentified in our data**.
XStream initiates the high-level identification, but there is no
proof that the Windows kernel driver itself drives SDA/SCL.

**Essential host-versus-probe-layer distinction:** original
family-0/0x90 is *statically proven* to use BAR1
`SPICTL/SPIDAT/SPIDIN`, including selector-0x0E 144-bit
host-to-board traffic. This is not the same assertion as
"the AP015 electrical bus is SPI", and must NOT override
the user's information that the physical probe-side
control is I2C. The exact relationship (if any) of BAR1
SPI to the physical I2C master remains to be recovered.
Firmware-forwarded 0x4A returns AP015 metadata (270-byte
aggregate Information, first 128 bytes captured); an
EEPROM origin is a hardware-informed candidate, not a
byte-proven mapping. Likewise, 0x0200 is the host-side
notification bit from the BAR0 INTST-0x08 / BAR1 HWInt
path; its presence/absence is not itself an ADC value
or a raw I2C transaction.

**First wrong "1/2 clamp" reinsertion in `001152`:**
0x82 state `028C -> 0058` appeared and the normal
270-byte AP015 reidentification packet did not follow.
The new ADC-before-EEPROM knowledge separates possible
physical contact, early analog classification,
EEPROM/I2C access and firmware-state timing hypotheses.
Do not assert that EEPROM contents were corrupt or that
a specific I2C failure occurred. The subsequent three
correct reinsertions produced normal AP015 metadata.

**Other current behavior stays validated:** the user-visible
unlocked-jaw warning and trace `002051` pair
0x82 `0058`/0x4A F7 for opened/unlocked and
0x82 `00A7`/0x4A F3 for closed. Neither 0x82 nor the
F7/F3 bit can yet be assigned to a particular physical
I2C register or EEPROM field. Refrain from further
generic IRQ, PCI or DMA changes.

Full canonical page:
[`docs/probus-detection-i2c-architecture.md`](probus-detection-i2c-architecture.md).
The source-backed host commands are documented in
`docs/ioctl-map.md`, and the runtime event comparisons
in `docs/probus-calibration-ab-comparison.md`.

## Current verified status

The targeted original-interrupt-path restoration in driver commit
`70716bace9ec874cf9b2f123a7946288215e1810` now has its first successful
reported real-hardware XStream regression. In the user's scope-side test,
AP015 was already attached at startup, was switched to a higher A/div range,
was disconnected once while XStream remained running, then was reconnected
once. The user reports that XStream detected the changes correctly and then
closed the application. The corresponding new kernel IOCTL capture is:

`xstream_trace_20260928_230614.jsonl` (user upload, **not** stored in the
public GitHub repository).

The pre-patch trace `xstream_trace_20260928_213834.jsonl` had missed probe
removal and lost the displayed waveform on reinsertion despite successful
DMA returns. The new test establishes the previously missing genuine
asynchronous command-status route. Do not claim that every ProBus special
function or every possible hotplug timing is now exhaustively validated.

### New capture: objective markers

| Measurement | Captured result |
|---|---:|
| IOCTL entries | 19,288 (seq 1..21761) |
| Missing sequence entries between trace snapshots | 2,473 across 30 gaps |
| Non-success NTSTATUS among captured calls | 0 |
| CFDC2110 | 14,742 |
| CFDC2138 | 3,633 |
| DMA returned DWORD differs from requested bytes | 0 |
| Standalone 85FB/0x01 status | 720 |
| Status enable mask | 0x02BF |
| Status pending 0x0080 | 718 |
| Status pending 0x0200 | **2** |
| Family-0/0x88 acknowledge with mask 0x0200 | **2** |
| Family-1/0x82 | **2** |
| Family-0/0x4A | 2 (startup / reinsertion) |
| Family-1/0x4A | 2 (startup / reinsertion) |

**Relative timeline** (10-MHz trace tick conversion, from first captured IOCTL;
precise physical-action timestamps were not separately recorded):

- ~13.908 s / seq 509: startup family-1/0x4A identifies AP015, response
  `Information=270`, captured output prefix contains ASCII `AP015`.
- ~28.422 s / seq 14951: standalone 85FB/0x01 returns
  `000000000400BF020002` (enabled 0x02BF, **pending 0x0200**).
- ~28.449 s / seq 14952: real family-0/0x88 mask-0x0200 acknowledge.
- ~28.480 s / seq 14961: family-1/0x82 (Information 412); no subsequent
  0x4A metadata query in this first event sequence. This is consistent
  with the user's first action: AP015 removal.
- ~31.275 s / seq 16639: standalone 85FB/0x01 again pending **0x0200**.
- ~31.299 s / seq 16640: corresponding 0x88 mask-0x0200 acknowledge.
- ~31.330 s / seq 16651: family-1/0x82 (Information 412).
- ~31.469 s / seq 16678: family-0/0x4A setup.
- ~31.515 s / seq 16679: family-1/0x4A again identifies AP015,
  `Information=270`; the first 128 captured response bytes are byte-identical
  to startup seq 509 and to the legacy AP015 metadata prefix in seq
  14596/14812. The uncaptured remaining 142 bytes are not compared.

All 3,633 captured CFDC2138 calls report NTSTATUS success,
`Information=4` and a returned requested byte count. Recorded DMA calls:
2,332 before event 1, 328 between the events, 973 after event 2; the
nearest cross-event DMA gaps are ~36 ms and ~397 ms. DMA resumes and
continues to trace end (~39.58 s). The last captured transfer >=8 KiB is
at ~27.067 s; the later 1,024-/2,048-byte pattern starts *before* the
first pending-0x0200 event. The user changed the probe's A/div setting
before disconnecting, but the exact trigger for transfer-size changes is
not independently timestamped. The kernel JSONL does not certify on-screen
waveform pixels, instantaneous CPU usage or raw ISR/DPC rate. The two
0x0200 events do not indicate a repeating probe-event storm; do not treat
snapshot omissions as proof that no unrecorded call ever failed.

The successful post-patch capture plus user-visible XStream behavior
supports closing the original **missing AP015 hotplug notification** bug
for the tested disconnect/reconnect scenario. Keep the patch intact.

## Second post-HWInt scope trace: 231656 (before 3490709)

The user ran another x64 session with AP015 preconnected, executed Degauss
(and the subsequent automatic Auto Zero), opened/closed the clamp, opened
it again, and closed it in the locked position. The uploaded capture is
`xstream_trace_20260928_231656.jsonl`, **not** committed publicly.
Its first and last captured IOCTL seq are 1..39998, 35,458 actual entries;
39 trace-snapshot gaps omit 4,540 sequence numbers. All captured calls
have `STATUS_SUCCESS`. There are 27,100 CFDC2110 calls, 6,701 CFDC2138
calls and zero returned-byte-count mismatches. CFDC2138's requested
DWORD is at **input offset 11**, not token DWORD offset 0.

The 1,438 standalone 85FB/0x01 status queries all report enable mask
`0x02BF`: pending exactly 0x0080 = 1,432; 0x0200 = 4; 0x0280 = 1;
0x0000 = 1. Therefore **five** real pending status events include 0x0200.
All five are acknowledged by family-0/0x88 and followed by family-1/0x82.
Timeline relative to first captured IOCTL (10-MHz timestamp):

| Event/time | Status seq/pending | 0x88 seq/mask | 0x82 seq/pair | User-action correlation |
|---|---|---|---|---|
| ~34.163 s | 19902 / 0200 | 19903 / 0200 | 19916 / `12 00 A7 00` | Post-Degauss; clamp still closed |
| ~41.812 s | 24430 / **0280** | 24431 / **0280** | 24443 / `12 00 58 00` | First open |
| ~57.832 s | 33777 / 0200 | 33778 / 0200 | 33787 / `12 00 A7 00` | First close |
| ~59.820 s | 34908 / 0200 | 34909 / 0200 | 34918 / `12 00 58 00` | Second open |
| ~61.894 s | 36079 / 0200 | 36080 / 0200 | 36089 / `12 00 A7 00` | Locked close |

The observed `0x0058` and `0x00A7` state words correlate with opened
and closed clamp respectively, but the bit-field and a separate lock bit
have **not** been statically decoded. After seq 24431 ack 0x0280,
seq 24437 returns pending 0 and seq 24438 acknowledges mask 0.
No trace snapshot gap falls immediately across any of these five events.
The last DMA IOCTL occurs near t=68.177 s; the 6,701 captured returns
all match the requested size. Do not confuse IOCTL return success with
pixel-level waveform proof, and do not claim CPU/ISR counts from JSONL.

### Persistent special-function difference versus original x86

In trace 231656, family-0/0x4A Degauss-associated request `...47 00`
appears **five times** (seq 19883..19887), all with response
`00000000000000000000040000000000`; the immediate family-1/0x4A
status is `...F700` (seq 19895). Automatically invoked Auto Zero
request `...47 12` occurs five times at seq 19938,19939,19941..19943
(status F100) and again in four five-request bursts after the jaw events
(status F700/F300/F700/F300), **25 occurrences total**.

The byte-identical legacy original 47 00 request at seq 24205 and the
legacy 47 12 requests at seq 14895 and 14900 return
`0000000000000000000002000000FFFF`; the comparable legacy 0x4A status
is `...F200`. Existing x64 prepatch trace 193741 already had the
same repeated 47 00 replies; the restoration of authentic HWInt 0x0200
alone does not change this response-format issue.

Original legacy family-1/0x82 answers use actual raw-response length
`0x0006` or `0x0016` and trailing 0xFF padding; the previous x64
host wrapper advertises full capacity `0x0190` and zero pads. All have
aggregate `IoStatus.Information=412`, so that number is **not** the
firmware reply count.

### Exact source-proven cause and targeted patch

Original `ghidra_exports/selected/000167f4_FUN_000167f4.c`:
allocates the entire requested output record and fills it `0xFF`;
then invokes `FUN_0001619A` with destination at result+6 and record
payload capacity (record size minus six) and obtains `actual_received`
as an out-param. On success it writes `actual_received` (not capacity)
as WORD at result+4 and retains FF padding. `FUN_000169B4` copies the
whole record. The x64 wrapper already has `pendingResponseLength` from
`LecTransportReceive`, but its old raw branch wrote `recordOutput-6`
and SystemBuffer was initialized to zeros.

**Driver commit `34907090820a582ac360d02120316c8792d7c888`** changes
only raw-hardware 85FB output serialization in `driver/Ioctl.c`: prefill
raw result record with 0xFF; use actual copied byte count
`min(pendingResponseLength, recordOutput-6)` for header WORD+4, leaving
unused bytes as FF. The established family-1/0x99 override stays in place.
There is no change to actual BAR access, device IRQ, HWInt, receive polling,
DMA, MAM, or the original firmware command packet. **Unbuilt/untested.**

## First test after raw 85FB host formatting patch: 233125

Scope trace: `xstream_trace_20260928_233125.jsonl` (private user upload,
**not committed**). User sequence: start XStream with AP015 attached,
Degauss, then manually trigger Auto Zero, then open/close the clamp
several times, exit. This is a hardware run of driver patch
`34907090820a582ac360d02120316c8792d7c888`, *not* the older
pre-fix trace `231656`.

Observed 55.628 seconds, 27,947 captured IOCTLs (seq 1..30856),
2,909 unobserved sequence entries in 29 trace-snapshot gaps, **none
after t=28.646 s**. Zero non-success NTSTATUS among captured IOCTLs.
21,366 CFDC2110, 5,219 CFDC2138; all DMA returns match requested byte
count from CFDC2138 input offset **11** and acquisition IOCTLs continue
until ~55.627 s. There are 1,666 successful DMA calls after the second
special `47 00` request (~40.675 s). AP015 startup metadata at seq 507,
Information 270, has the same captured first 128 bytes as `230614`,
`231656` and the original x86 legacy trace.

**Framing patch succeeds for the exact legacy `47 00` packet:**

| Trace | Same 47 00 input | Returned complete output |
|---|---|---|
| Original x86 183409, seq 24205 | one captured | `0000000000000000000002000000FFFF` |
| Pre-fix x64 231656, seq 19883..19887 | five near-duplicate requests | `00000000000000000000040000000000` |
| New x64 233125, seq **8573** (~20.034 s) | first user-action correlation | `0000000000000000000002000000FFFF` |
| New x64 233125, seq **22156** (~40.675 s) | second user-action correlation | `0000000000000000000002000000FFFF` |

The two new requests are ~20.64 s apart and correlate respectively
with Degauss and the user's later **manual** Auto Zero. Unlike the
earlier automatic follow-up/jaw-event run, new capture has **zero
`47 12` requests**. Hence a blanket claim that 47 12 is every Auto
Zero trigger is inaccurate. The old five-retry bursts do not recur
for 47 00 in this run. The actual full 16-byte reply parity establishes
the original `FUN_000167F4` actual-received-length and FF-tail fix
for this particular request, *not* the still-unobserved post-fix 47 12
or 0x82 reply shapes.

The following identical family-1/0x4A `...01 0A` status requests
occur at seq 8574 (~20.066 s) and 22158 (~40.706 s), both returning
`0000000000000000000004000000F300`. The legacy matched examples
gave `...F200`. The F3/F2 one-bit payload difference persists after
the host serializer correction. Firmware meaning, physical calibration
outcome and exact probe state are unproven. Do not falsify this board
status to force legacy parity.

**Independent missing-notification observation:** all **1,135**
standalone 85FB/0x01 status reads report enable mask 0x02BF and
pending **only 0x0080**; no 0x0200/0x0280, no family-1/0x82, no
0x88 acknowledgement for mask 0x0200. There are **447 status reads
after t=40 s and no snapshot gaps after t=28.646 s**, despite the
reported several physical clamp opening/closing operations. This is
different from trace 231656, which recorded five real 0x0200-bearing
notifications during an otherwise similar physical sequence. The
fix in 3490709 changes only host-side raw reply serialization, with no
direct IRQ, INTEN, HWInt, firmware send, DMA or PCI modifications.
Whether changed host control flow/calibration state indirectly relates
to the missing notifications is **unknown**. This capture has neither
raw ISR counters nor physical action timestamps/screenshots. Do not
patch interrupts or hardware registers on assumption alone.

Detailed A/B evidence:
`docs/probus-calibration-ab-comparison.md` and `docs/runtime-trace.md`.

## Second post-formatter scope capture: jaw-state before/after calibration (235314)

The user confirmed performing exactly the prior handoff's controlled
protocol: with AP015 connected and XStream running, **open/hold/close/hold
BEFORE any Degauss or Auto Zero**, then Degauss and manually trigger Auto
Zero, then repeat open/hold/close/hold, and close the session. File
`xstream_trace_20260928_235314.jsonl` (user upload, **not** public).
Exact hand-action seconds and independent UI outcome were not supplied.

Measured 72.009226 seconds, 36,366 captured IOCTLs (seq 1..42227),
36 trace-snapshot gaps / 5,861 omitted entries; the last gap ends at
t~48.480 s. Zero recorded failing NTSTATUS. 27,834 CFDC2110,
6,818 CFDC2138, every DMA output DWORD equals its requested byte count
(input DWORD at offset 11), and transfers continue to t~72.008570 s.

**All 1,517 standalone 85FB/0x01 replies contain software enable
0x02BF and pending ONLY 0x0080.** No 0x0200 / 0x0280, no family-1/0x82,
and no family-0/0x88 with ack mask 0x0200.

| Deliberate protocol phase (time relative to first IOCTL) | Status reads | Containing pending 0x0200 | Recorded DMA replies |
|---|---:|---:|---:|
| AP015 identified, **before Degauss** (~17.000..40.156 s) | **622** | **0** | 3,207 |
| Degauss to manually invoked Auto Zero (~40.156..54.337 s) | **381** | **0** | 1,622 |
| After manual Auto Zero (~54.337..72.009 s) | **514** | **0** | 1,989 |
| Gap-free ending (~48.480..72.009 s; overlaps two rows above) | **648** | **0** | 2,569 |

First user-action-correlated family-0/0x4A `...47 00`: seq
**23386**, t~40.155959, response
`0000000000000000000002000000FFFF`, follow-up 0x4A status
**F3** at seq 23388. Second `...47 00`: seq **31835**,
t~54.336670, identical **byte-for-byte original-x86-matching**
response, follow-up status again F3 at seq 31836. They are ~14.181 s
apart; no previous five-attempt burst. The first 128 captured bytes
of startup family-1/0x99 also now **match original x86 including all
0xFF tail bytes**; pre-formatter x64 had wrongly zero-filled them.
AP015 is detected at startup family-1/0x4A seq 508,
Information=270; its captured metadata prefix matches `231656`.
No `47 12` packet in this manual procedure.

**Inference:** missing jaw notifications are not *necessarily*
caused by the Degauss/Auto Zero action: the no-event behavior was
already present before any calibration, unlike earlier pre-formatter
`231656` which reported genuine 0x0200 events. Two consecutive runs
after `3490709` show the difference; this temporal correlation does
not prove that FF padding or actual-response-length directly disables
hardware. Software command-enable mask 0x02BF must NOT be confused
with hardware BAR0 INTEN offset 0x084 and its receive-interrupt bit
0x08. The IOCTL JSONL does not expose physical INTEN, INTST, HWInt,
raw ISR/DPC rate, or UI jaw-state pixels. See the full A/B table in
`docs/probus-calibration-ab-comparison.md` and exact runtime events
in `docs/runtime-trace.md`.

## New decisive scope trace: physical AP015 hotplug on post-formatter driver (001152)

The user repeatedly unplugged/replugged the AP015 in one XStream session,
using unchanged driver after `3490709`. They report the first
reconnection was temporarily displayed as a different "1/2 clamp"
(possibly seating), whereas later reinsertions were recognized
correctly. Private trace:
`xstream_trace_20260929_001152.jsonl`. Elapsed time ~61.381146 s;
32,675 IOCTL entries with seq 1..37623, 34 trace-snapshot gaps omitting
4,948 entries, last gap ends ~32.792085 s. No captured NTSTATUS
failure. CFDC2110 24,976; CFDC2138 6,135, all return exactly the
requested bytes from input DWORD offset 11. Last DMA at ~61.380523 s
is 1,024/1,024; 1,062 DMA calls occur after the final insertion
notification. IOCTL success is not an independent pixel-level waveform
or CPU/ISR measurement.

Standalone 85FB/0x01: 1,361 queries, enabled mask **0x02BF** in
all of them, pending `0x0080` x 1,348, `0x0200` x 11,
`0x0280` x 1, `0x0000` x 1. **Twelve real 0x0200-bearing
notifications**, matched by family-0/0x88
(11 mask-0200, one mask-0280) and 12 family-1/0x82;
NINE occur after the final snapshot gap.

| Apparent physical action | t (s) | Status seq/pending | family-1/0x82 seq/raw final state WORD |
|---|---:|---|---|
| Unplug 1 | 27.557 | 16747/0200 | 16754/**03FF** (special 14-byte raw data) |
| Replug 1 | 31.662 + 31.718 | 19245/0200; 19249/0280 | 19248/**028C**; 19257/**0058** |
| Unplug 2 | 40.564 | 25308/0200 | 25318/**03FF** |
| Replug 2 | 42.336 + 42.386 | 26308/0200; 26320/0200 | 26318/**00A9**; 26325/**0058** |
| Unplug 3 | 44.929 | 27775/0200 | 27782/**03FF** |
| Replug 3 | 47.306 + 47.367 | 29173/0200; 29187/0200 | 29183/**00AA**; 29193/**0058** |
| Unplug 4 | 49.510 | 30380/0200 | 30390/**03FF** |
| Replug 4 | 52.275 + 52.405 | 32018/0200; 32062/0200 | 32021/**00A9**; 32065/**0058** |

These are correlated with hand-action order, not individually
timestamped hand actions or proven field-by-field bit meanings.
`03FF` consistently follows a removal. **First reinsertion
differs materially**: transient `028C`, combined pending `0280`,
then `0058`, with intervening status pending 0 / ack 0; XStream
does not request any post-event 270-byte AP015 metadata. This
correlates with the reported wrong "1/2 clamp" display, but there
is no evidence that `028C` literally means "1/2". Subsequent
three reinsertions trigger normal family-0/1 0x4A metadata:
seq 26342/26343, 29207/29208 and 32067/32068.
Together with startup seq 509, all four family-1/0x4A
Information=270 metadata replies contain "AP015" and have
byte-identical **captured first 128 bytes** (rest not captured).

**Post-3490709 reply-format validation is now complete for the
previously untested short 0x82 and 47 12 examples:**
0x82 seq 16754 has actual raw payload length 0x000E (14 bytes),
and the other eleven 0x82 replies have actual raw length 0x0006
(6 bytes). All unused captured record bytes are 0xFF, not the
pre-fix zero padding, and none advertises the old full capacity
0x0190. One and only one identical `...47 12` setup follows each
successful reidentification: seq **26385, 29250, 32096**;
all return the full legacy-x86-matching
`0000000000000000000002000000FFFF`, with follow-on family-1/0x4A
firmware/status F7 at seq 26386, 29251, 32099. No old
five-attempt 47 12 burst. Do not conflate these reidentification-
associated 47 12 requests with manually pressed Auto Zero,
which was correlated with 47 00 in traces 233125 and 235314.

**Engineering conclusion:** the repeated absence of spontaneous
jaw-only pending 0x0200 in post-formatter traces 233125/235314 is
not a global HWInt/INTEN-0x08 failure, since the exact same
formatter code in trace 001152 successfully handles four physical
unplug/replug cycles and twelve genuine notifications. No reason
to rollback `3490709`, synthesize events, or modify PCI/DMA.
The first reinsertion's transient state remains a separate,
non-reproduced cause-unknown observation. The meaning of F7
versus historical F2 and any actual physical calibration quality
also remain open.

## Latest correction: jaw-open warning observed and traced (002051)

The user explicitly reports XStream displays a not-locked warning
when the physical AP015 jaw is opened, cautioning that measurement
accuracy may be affected. Earlier assistant statements equating zero
captured standalone pending-0x0200 with "XStream does not recognize
the jaw" were **incorrect**. Preserve the counts in prior captures
as historical IOCTL observations but do not repeat the alleged UI
failure or infer an additional polling/status mechanism without
evidence.

Private real-scope trace:
`xstream_trace_20260929_002051.jsonl`, on the same `3490709`
corrected-response build, with **no further driver change**.
31,200 captured IOCTLs (seq 1..35955), 58.3684694 seconds,
32 snapshot gaps omitting 4,755 sequence entries, last gap ends
~23.382490 s; every following relevant event is gap-free. No
observed NTSTATUS failures. 23,896 CFDC2110, 5,839 CFDC2138,
all exact requested-vs-returned DMA byte counts (requested DWORD
at input byte offset 11), last 1,024-byte transfer
t~58.367987 s.

Standalone 85FB/0x01: 1,292 reads, software enabled mask 0x02BF;
pending `0x0080` x 1,287, `0x0200` x 3,
`0x0280` x 1, and `0x0000` x 1. Thus four authentic
notifications containing 0x0200, each with family-0/0x88
acknowledgement and family-1/0x82. Every 0x82 result returns
actual raw length `0x0006`, with 0xFF unused bytes:

| Time | Pending status/ack seq | 0x82 seq and raw status | Correlation |
|---:|---|---|---|
| ~24.623 s | 14888 / 14889, mask 0200 | 14891: `000012005800`, **0x0058** | Jaw-open, XStream unlocked warning |
| ~35.038 s | 21435 / 21436, mask 0280 | 21448: `00001200FE03`, **0x03FE** | Apparent removal-like status; pending briefly zero at seq 21442 |
| ~37.458 s | 22833 / 22834, mask 0200 | 22843: `000012005700`, **0x0057** | Reconnect-like transition and normal AP015 0x4A metadata seq 22877/22878 |
| ~49.861 s | 30553 / 30554, mask 0200 | 30563: `00001200A700`, **0x00A7** | Jaw-closed state |

The first opening-like event is followed by one family-0/0x4A
`47 12` request at seq 14913 -> exact original-x86-matching
`0000000000000000000002000000FFFF`, then family-1/0x4A
status seq 14916 ends **F7**. The final closing-like event
similarly has `47 12` at seq 30580 with exact response, then
status seq 30581 ends **F3**. The **same pairing** was previously
captured in `231656`: 0x0058/open -> F7;
0x00A7/closed -> F3, each repeated twice.
F7 XOR F3 = 0x04; the meaning of this exact bit has **not**
been statically recovered, so document it as a correlated
candidate, not a formal vendor register definition.

AP015 startup metadata seq 506 and subsequent metadata seq 22878
each have Information=270, and their captured first 128 bytes
are identical, including literal "AP015". The two other raw
0x82 words 03FE and 0057 differ by one from earlier
removal 03FF and open 0058 observations; no proven per-bit
meaning and no independently provided action timestamps.

**Outcome:** the jaw-unlock UI warning is real and the same
corrected driver can process genuine 0x0200/0x88/0x82 for
jaw-state transitions as well as physical reconnect.
No demonstrated problem justifies reverting `3490709` or
altering BAR/ISR/PCI/DMA. Earlier `233125` and `235314`
remain factually zero captured pending-0x0200, but the
previous interpretation as failed XStream warning/display
must be retired. See `docs/probus-calibration-ab-comparison.md`
and `docs/runtime-trace.md` for supporting packet details.

## Recovered and now exercised original path

- `FUN_0001619A -> FUN_000160A8(1)` enables BAR0 INTEN bit `0x08`
  before a real 85FB firmware response fetch; new code in
  `driver/Ioctl.c` restores this missing enable.
- ISR `FUN_000108D6` handles `INTST 0x08`, acknowledges source via
  BAR1 `CLRIRQ = 2`, records/acks INTST and schedules DPC.
- Original `FUN_00011390` assembly at `0x114A2..0x114C8` proves the
  actual call `FUN_000176A2(transport, &hwIntWord)`. Its decompiled C
  incorrectly shows a zero argument for the subsequent
  `FUN_000157A6(commandStatus, hwIntWord)`; trust the assembly.
- `FUN_000176A2` reads the low WORD of BAR1 HWInt `0x410` and clears
  a nonzero value by writing zero. `FUN_000157A6` then performs
  `pendingMask |= enabledMask & hwIntWord`. CFDC2180 event is signalled
  on a nonzero source word. New `driver/Acquisition.c` DPC mirrors this.
- The existing standalone 85FB/0x01 returns the enable/pending WORDs and
  family-0/0x88 acknowledges the same sticky status under
  `LegacyEventLock`. The genuine `0x0200` bit now drives user-mode
  0x82/0x4A naturally, without synthetic probe state or another poller.
- Solicited RX remains bounded/synchronous; single-channel MAM/CFDC2138,
  family-1 MTT/0x51, immediate IIMCL completion acknowledgement and
  existing INTST/CLRIRQ handling remain unchanged.

No additional PCI/DMA changes were needed. The earlier source-only review
identified a theoretical interleaving of full interrupt-mask commits around
the initial enable of bit 0x08 versus acquisition bit 0x01; the successful
focused test supplied no concrete reason to redesign that working path.
Do not refactor it speculatively.

## Historical comparison assets

Four files in the previously uploaded ZIP
`lecroy_probus_handoff_20260928(1).zip` (none committed to GitHub):

1. `legacy_xstream_trace_20260928_183409_probus_original.jsonl`:
   original x86 hotplug, three pending-0x0200 notifications at seq
   14552/14588/14616; opcode-0x88 acks seq 14553/14589/14620;
   family-1/0x82 and family-0/1 opcode-0x4A AP015 metadata.
2. `xstream_trace_20260928_183807_probus_x64.jsonl`:
   pre-patch x64 hotplug, no pending-0x0200, no recognition.
3. `xstream_trace_20260928_193741.jsonl`:
   pre-patch x64 AP015 already connected at startup, recognized by 0x4A;
   Degauss and Auto Zero requests present, but one result differs.
4. `xstream_trace_20260928_213834.jsonl`:
   pre-patch AP015 connected, then removed/reinserted; no removal
   notification; no pending-0x0200; DMA continues yet waveform disappeared.

Full historical A/B data, including the new post-patch trace markers, are
in `docs/probus-calibration-ab-comparison.md`. The separate old Ch2
calibration traces are optional: each newly visited V/div setting calibrates
once and then caches, so this is not an active bug.

## Current engineering decision: no further jaw regression patch or repeat test

The user's XStream warning and trace `002051` **resolve the premise**
that the mechanical jaw was no longer recognized. The latest trace
has both genuine open-like 0x0058/F7 and closed-like 0x00A7/F3
sequences on the corrected host response serializer, along with
a separate physical reconnect and unchanged acquisition. Do not
demand another generic physical hotplug/jaw exercise to reproduce
an alleged UI malfunction which the user has contradicted.

Retain driver commits `70716ba` (HWInt recovery) and `3490709`
(actual received-length and 0xFF raw reply padding). Do not change
IRQ, PCI, MAM or DMA without a concrete new failing observation.
A separate protocol-research topic remains: decode the actual
meaning of the changing family-1/0x4A F7/F3 status byte,
0x82 probe-state words (0x0058/0x00A7; 0x03FE/0x03FF;
0x0057/0x0058), and the transient wrong-probe first insertion
in `001152`. Compare original x86 and x64 packets, avoid making
up firmware meanings, and log screen-observed effects when available.
Existing safety constraints: <=5% scope CPU, no synthetic pending
0x0200, preserve below-4-GiB descriptor checks and gated transfer
IOCTL variants.

## Reusable scope workflow

Scope repo: `C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`.
Use an **elevated** PowerShell window.

For a separate build-only check (do not install if build fails):

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git update failed." }
& ".\scripts\build-driver.ps1" -Configuration Debug
```

For an authorized scope-side XStream hardware retest, the user keeps the
following local helper. It pulls, builds, test-signs, installs/reloads the
driver, captures the IOCTL stream and attempts NAS trace copy; the local
trace remains available if NAS copy fails:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& "C:\Users\LeCroyUser\Desktop\Run-LeCroy-XStream-Trace.ps1" -Configuration Debug
```

User action timings are **not** automatically annotated in JSONL.
Request them when correlating physical transitions. Never commit proprietary
original EXE/DLL/SYS or the user's private raw scope traces into this public
repo. Keep `AGENTS.md`, this handoff file, README and relevant detailed docs
in sync with further verified findings.
