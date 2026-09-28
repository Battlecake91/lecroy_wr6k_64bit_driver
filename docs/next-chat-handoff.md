# Active handoff: 85FB parity validated; two no-jaw-event runs; isolate physical hotplug (2026-09-28)

**Read this file and `AGENTS.md` before changing the driver.**
Conversation in German, repository documentation and source comments in English.
Repository: https://github.com/Battlecake91/lecroy_wr6k_64bit_driver;
active branch: `main`.

**LATEST ENGINEERING STATE:** Hardware patch `70716ba` restored
authentic AP015 `0x0200` events in `230614` (physical unplug/replug)
and `231656` (Degauss/jaw state). Host-only raw 85FB formatter fix
`3490709` now reproduces original `47 00` and startup `0x99`
response padding in actual scope captures. However, **both post-formatter
runs `233125` and `235314` have zero `0x0200` events and zero
family-1/0x82 despite physical jaw movements**. Crucially, `235314`
followed a deliberate jaw open/close BEFORE Degauss, then manual Auto
Zero and another open/close: the absence is present in both phases.
Next discriminator is ONE physical AP015 unplug/replug on unchanged current
driver, without calibration/jaw movements. Do not change IRQ, PCI or DMA
on correlation alone.

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

## Immediate next task: one physical AP015 hotplug with current driver

Do not make another code change first. We now know that a deliberate
jaw pair before calibration and another afterward both yielded zero
notifications on the corrected host formatter. Distinguish a **general
missing 0x0200/INTST-0x08 path** from a probe jaw-specific event:

1. Start a short normal XStream Debug trace using the unchanged scope
   helper below, with AP015 **preconnected**. Wait for correct AP015
   metadata and waveform. Do not touch Degauss or Auto Zero.
2. **Physically unplug AP015 once**, leave disconnected for at least
   five seconds, and record whether XStream visually reports removal
   and the approximate elapsed seconds.
3. **Physically reconnect AP015 once**, hold at least five seconds,
   note whether XStream reports insertion/identifies AP015 and whether
   the waveform is visible; close XStream normally.
4. Preserve <=5% CPU; abort if acquisition degrades. Inspect
   standalone software-enabled 0x02BF vs pending 0x0200, acks
   family-0/0x88 mask 0x0200, family-1/0x82 and post-insertion
   family-0/1 opcode 0x4A metadata; verify DMA returned sizes and
   snapshot omissions. Compare to **pre-formatter successful physical
   hotplug baseline `230614`**.

Interpretation:
- Hotplug `0x0200` present now, but jaw still absent in `235314`:
  hardware notification broadly works; investigate AP015 mechanical
  sensor/firmware mode separately.
- Hotplug `0x0200` absent now too: investigate actual physical BAR0
  INTEN `0x084`, its 0x08 bit, BAR0 INTST `0x080` and BAR1 HWInt
  `0x410` by **bounded passive diagnostics**. Do not infer hardware
  INTEN from the standalone 85FB software-enable word 0x02BF. Do not
  write speculative register values or fabricate command-pending bits.
  The existing LecCommitLegacyInterruptMask path commits full DWORD
  updates from InterruptEnableShadow; any lost-mask/concurrency theory
  requires evidence and review of existing serialization.
- Successful IOCTL/DMA completion alone is never visual acquisition
  proof. User-visible jaw/hotplug state and relative hand timings
  should accompany the next JSONL.

No reverted driver or additional interrupt patch is requested at this
stage. Keep original physical IRQ/CLRIRQ, normal PCI/MAM/DMA, <4-GiB
descriptor safety and gated unsupported IOCTLs intact.

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
