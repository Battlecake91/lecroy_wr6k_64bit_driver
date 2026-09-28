# LeCroy WR6k 64-bit driver reconstruction

Reverse-engineering project for the legacy LeCroy S65 acquisition kernel driver used by the WaveRunner 6000-series software.

## Goal

Reconstruct the externally visible behaviour of the original 32-bit `LecS65AcqDrv.sys` and implement a native x64 replacement suitable for modern 64-bit Windows.

The priority is compatibility with the existing LeCroy user-mode software:

- preserve device / interface behaviour where required;
- preserve IOCTL values and buffer layouts;
- preserve hardware access semantics;
- replace the obsolete x86 DriverWorks implementation with maintainable WDK code.

**Current engineering handoff:** [AP015 raw-reply fix and jaw-event follow-up, 2026-09-28](docs/next-chat-handoff.md). Original HWInt/0x0200 notifications passed real-hardware tests (`230614`, `231656`). The subsequent source-proven raw 85FB reply-length/FF-padding fix (`3490709`) was verified for the legacy `47 00` response on hardware in `233125`. This last run, however, recorded **no spontaneous jaw-change pending 0x0200 or family-1/0x82** despite reported clamp movements. A controlled jaw-only-before/after-calibration trace is next; no speculative interrupt or DMA modifications.

## Current state

The reconstruction has progressed well beyond the initial outer-interface pass:

- all 27 DeviceControl dispatch values have been recovered;
- the generic raw register read/write ABI and build query are known;
- Dallas/1-Wire buffer contracts and low-level access paths are known;
- the named BAR0/BAR1/BAR2 register map has been reconstructed and cross-checked against the register-object initializer;
- interrupt, DPC, event-signalling, BAR1 message transport and MAM register programming paths have been decoded;
- the acquisition-buffer path is confirmed to use locked user pages, MDL chains and chained 4 KiB descriptor pages built directly from PFNs; descriptor counts are DWORDs and slot 511 links to the next table page;
- the packed `0xCFDC2110` command parser and all three A5FB command families are substantially decoded;
- local CFDC2110 operations are now tied to JTAG, SPI, clock/divider, firmware-version, ITMODE/LEDCTL, MTTCTL and MTTRGO actions; the remaining opaque startup commands are those forwarded verbatim to board firmware;
- the exact DeviceControl branch `0xCFDC2110 -> 0x13AE2` is now confirmed from raw dispatch instructions;
- the late DeviceControl dispatcher and its common completion/error path are now recovered, including `0xCFDC2190/2194`, raw register I/O, build query, `0xCFDC2400`, and `0xCFDD219F`;
- serial-trigger FPGA programming is now confirmed to bit-bang BAR1 `GPIODAT`, while `0xCFDC2190/2194` form an error-mask/control status pair;
- kernel event handles are now confirmed to be referenced with `EVENT_MODIFY_STATE`, tied to process registration, and immediately signalled when an enabled status bit is already pending;
- transfer registration and removal calls are now tied directly to `0x1731C` and `0x172A2`; the synchronous transfer hooks are now decoded as enable/disable of global transfer mask bit 0 via the synchronized interrupt-mask commit path;
- the indexed MAM protocol is decoded at the host side, including the five per-channel fields, MAMSEQ channel encoding, and `0x105` as mode 1 plus five words;
- the synchronous transfer helper is mapped to concrete registers: acquisition IOCTLs launch through `MAMRGO`, while CFDC2110 family-1 opcodes `0x50/0x51` launch through `MTTRGO`; both share `SGTA`, `IIMTC`, `IIMCL`, and the completion wait;
- the acquisition completion interrupt is now tied end-to-end: interrupt bit 0 is consumed by the DPC branch that signals the selected transfer entry's `+0x24` event, which is exactly what `0x171DE` waits on;
- the auxiliary side effect of the millisecond-delay IOCTL has been identified as a BAR2 `BUZZER` pulse around the delay;
- the acquisition front-ends are mapped to `0xCFDC2138` and the WOW64-sensitive `0xCFDD219F` METHOD_NEITHER path, including its packed channel pairs, transient MDL registration, trailing result DWORD and exact size equation;
- persistent transfer registration/removal and process-close cleanup are decoded; the legacy four-byte token is a leaked kernel pointer and must become an owned opaque ID on x64;
- the native x64 compatibility driver now includes owner-checked 32-bit transfer tokens, MDL locking, chained legacy DMA descriptor construction, recovered interrupt/DPC plumbing and process-close transfer cleanup;
- active MAMRGO/MTTRGO acquisition launch remains deliberately gated until the 32-bit DMA-address constraint and launch-count units are validated on hardware;
- passive tracing now has a JSONL live-capture workflow with timestamps, WOW64 state, bounded input previews, METHOD_NEITHER input capture and buffered output previews;
- on the current x64 reference bring-up, the driver loads and the PCI device binds correctly, but all three tested MMIO regions currently read as `0xFFFFFFFF`; passive PCI diagnostics now confirm `PCI Command = 0x0006`, so both Memory Space Enable and Bus Master Enable are active, and the BAR config-space values exactly match the translated Windows resources;
- the confirmed legacy START/ITMODE startup probe from `FUN_00012FDE` is now reproduced during x64 PnP start, including its exact 100 us / 500 us timing and buzzer pattern; on the reference scope this fixes the previous all-ones MMIO state, yielding FVER = `0x00000002`, ACQFVER = `0x00000003`, ONEWIRE = `0x00000000`, and a valid Dallas ID `23 F0 47 37 00 00 00 AC` with the expected two-beep startup pattern;
- the first real XStream startup trace has now been captured on working hardware; early startup uses only four distinct `CFDC2110` request shapes: RESET, family-1/opcode-`0x99`, family-0/opcode-`0x88`, and family-1/opcode-`0x42` JTAG, with no C5FB record observed;
- `CFDC2110` hardware execution remains byte-exact and trace-driven; RESET, opcodes `0x99`, `0x88`, `0x85`, the first JTAG form, and the next family-1 startup forms (`0x42` mode 0 / 83-bit, `0x90`, `0x42` mode 1 / 58-bit, `0x42` mode 1 / 256-bit, and the exact mode-1 / 83-bit form) are admitted, while the newly exposed mode-2 / 256-bit form and every other unconfirmed packet remain rejected before hardware side effects.

Documentation:

- [docs/original-driver-analysis.md](docs/original-driver-analysis.md)
- [docs/ioctl-map.md](docs/ioctl-map.md)
- [docs/abi-analysis.md](docs/abi-analysis.md)
- [docs/hardware-register-map.md](docs/hardware-register-map.md)
- [docs/device-interfaces.md](docs/device-interfaces.md)
- [docs/legacy-inf-analysis.md](docs/legacy-inf-analysis.md)
- [docs/reference-system.md](docs/reference-system.md)
- [docs/user-mode-components.md](docs/user-mode-components.md)
- [AGENTS.md](AGENTS.md) - current operating rules and hand-off state for future agents/chats

Reusable reconstructed ABI definitions:

- [include/LecS65LegacyIoctl.h](include/LecS65LegacyIoctl.h)
- [include/LecS65LegacyInterfaces.h](include/LecS65LegacyInterfaces.h)

A native x64 replacement prototype is in-tree and development now happens on `main`. Bring-up currently covers the recovered PCI/PnP surface, BAR mapping, selected legacy IOCTLs, Dallas/1-Wire access, event registration and extensive tracing. The remaining high-risk work is acquisition/DMA compatibility, the packed command semantics and WOW64-sensitive pointer paths.

## Reference binary

Analysed binary:

```text
Filename: LecS65AcqDrv.sys
SHA-256: 5f53de1dea6a58f201290e79a60fab587c039423322faa884bf4c15f0fd89087
Size:     64384 bytes
```

The proprietary reference binary is not stored in this public repository.

## Important current findings

The legacy driver exposes generic register access:

```text
0xCFDC21C0  generic register read
0xCFDC21C4  generic register write
0xCFDC21C8  driver build query -> 1002
```

It registers five PnP device-interface classes:

```text
{958695A4-693A-435E-8297-66F805D8E46A}  acquisition/control
{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}  Dallas / board ID
{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}  trace control
{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}  three-event registration
{7AC34BE9-F766-4F15-9E88-854BA5E2146E}  delay / flag helper
```

Passive tracing also established an important endpoint-selection detail:
original XStream probes `\\??\\ALADDINAcqDriver0`, expects that open to fail
with `STATUS_OBJECT_NAME_NOT_FOUND`, and then uses the
`{958695A4-...}` PnP interface for acquisition/control traffic. The x64
replacement deliberately does not publish that DOS alias.

The installed legacy INF has now been recovered as well. Windows publishes it as `oem18.inf`, while the file identifies itself as the original `LecS65AcqDrv.inf`. It confirms `PCI\\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00`, service name `LecS65AcqDrv`, device class `DataAcquisition`, class GUID `{BA5FE95F-EE73-4113-8121-F38CC4FF0095}`, and binary name `LecS65AcqDrv.sys`.

Together, those findings give the future x64 driver a useful incremental bring-up route: enumerate the same interfaces, bind to the confirmed PCI ID, map the PCI BARs, validate raw register access, then move on to Dallas, interrupts and acquisition/DMA.


## Native x64 bring-up prototype

Development originally started on `prototype/x64-bringup`, but that work has been merged and active development now happens on `main`.

The x64 prototype implements PCI/PnP bring-up, all five recovered interface GUIDs, BAR mapping, build query, raw register access, Dallas/1-Wire access, event-registration compatibility, interrupt delivery, the observed CFDC2124/CFDC2138 DMA path, family-1 MTTRGO transfers and detailed IOCTL tracing. It deliberately omits the legacy DOS alias because the original XStream startup expects that probe to fail. As of 2026-09-28, XStream displays real waveforms on the replacement x64 driver; active work has moved from basic acquisition bring-up to compatibility and stability validation.

See [docs/x64-bringup.md](docs/x64-bringup.md) and
[docs/runtime-trace.md](docs/runtime-trace.md).

Recommended passive XStream capture:

```powershell
.\scripts\capture-xstream-trace.ps1
```


## Command-line build

The native x64 driver can be built without opening Visual Studio:

```powershell
.\scripts\build-driver.ps1
```

Build the driver and `lecdiag` together with:

```powershell
.\scripts\build-driver.ps1 -BuildLecdiag
```


For the reference test machine, the normal one-command rebuild/reload workflow is:

```powershell
.\scripts\build-sign-load-driver.ps1
```

It builds the x64 driver and `lecdiag`, signs the SYS with the existing
`CN=LecS65 x64 Test` certificate, reloads the installed service, verifies
build 1002, and runs the passive `lecdiag pci` diagnostic.


The test-machine reload workflow installs updates through the Windows Driver
Store rather than overwriting `System32\drivers` directly.
Its generated `DriverVer` uses the UTC calendar date so Inf2Cat does not reject
builds made shortly after local midnight as postdated packages.


Current staged-runtime note:

- the original-style interface startup path is active;
- `CFDC2190`, six-byte 85FB host framing and generic family-0 opcode
  `0x88` handling are implemented;
- the corrected 85FB length rule now lets the 120 family-1 opcode-`0x96`
  selector reads advance exactly like the legacy trace instead of looping on
  selector `0x20`;
- trace `xstream_trace_20260927_235712.jsonl` proves the first real
  one-channel `CFDC2138` MAM DMA completes successfully and returns the
  requested 0x0C00-byte count;
- restoring the recovered `CFDC2180` / `CFDC218C` event delivery and
  standalone 85FB/0x01 command-status state now advances XStream into real
  transfer registration and buffered acquisition;
- the first successful x64 `CFDC2124` runtime registration proves that the
  selected source and descriptor-table pages fit the board's legacy 32-bit
  DMA address format;
- `CFDC2138` is now enabled only for the exact one-channel 15-byte ABI shape
  observed in both complete original runtime captures. It reproduces the
  recovered MAM setup, SGTA/IIMTC launch, interrupt-bit-0 completion and
  five-second timeout path;
- family-1 opcodes `0x50/0x51` now execute the recovered MTTRGO path; trace
  `xstream_trace_20260928_000706.jsonl` shows all 114 observed opcode-0x51
  transfers succeeding;
- trace `xstream_trace_20260928_002537.jsonl` confirms the ISR-side
  IIMCL fix: all 1261 CFDC2138 acquisitions and all 114 observed opcode-0x51
  MTTRGO transfers succeed, with no failed IOCTL in the capture;
- calibration still loops because family-0 opcode `0x88 / mask 0x0080` was
  incorrectly being forwarded to firmware. The original handles masks 0x0080
  and 0x0800 locally and returns `{0,2,0}`; the x64 driver now mirrors that
  split while keeping other masks firmware-forwarded;
- `CFDD219F` and unobserved multi-channel acquisition remain intentionally
  gated rather than guessed.


### 2026-09-28 runtime milestone

The latest x64 trace (`xstream_trace_20260928_004553.jsonl`) exits the
application's calibration phase with no failed IOCTLs, but then exposes a
post-calibration interrupt-event storm. Static recovery shows that the legacy
ISR performs source-specific BAR1 `CLRIRQ` writes for INTST bits
`0x04/0x08/0x10/0x20` before the common INTST acknowledge. The x64 ISR now
mirrors those writes. The next hardware test is aimed at reaching normal
waveform acquisition after calibration.


### First confirmed x64 waveforms

`xstream_trace_20260928_005808.jsonl` is the first captured run where XStream
visibly displayed waveforms on the replacement 64-bit driver.

The trace contains no failed IOCTLs. All 3,134 captured CFDC2138 acquisitions
complete successfully, including repeated 167,936-byte transfers, and the
post-calibration interrupt storm seen in the previous milestone trace is gone.

The working baseline includes the recovered endpoint selection, CFDC2190,
85FB framing/length rules, opcode-0x96 selector progression, legacy user-event
delivery, command-status handling, one-channel CFDC2138 MAM DMA, family-1
opcode-0x51 MTTRGO transfers, local opcode-0x88 acknowledgement, immediate
IIMCL completion acknowledge and source-specific BAR1 CLRIRQ writes.

The next phase is regression testing of normal oscilloscope behavior rather
than further speculative startup/DMA changes.


### Normal-operation regression status

A broader regression run,
`xstream_trace_20260928_011938.jsonl`, keeps every captured IOCTL successful
while the scope is used normally. The user has verified correct-looking
waveform amplitude/frequency plus working timebase, vertical scale, coupling,
bandwidth, 2-channel/10-GS/s mode switching and trigger-type changes.

The remaining probe-side uncertainty is ProBus communication. The recovered
family-0 opcode-0x90 SPI/probe path is heavily exercised and succeeds in the
trace, but the driver does not expose a direct I2C interface. Physical ProBus
I2C behavior therefore still needs a real probe-level validation.


### Service Revision diagnostics

The Service -> AladdinAcqBoard -> Revision page exposed two additional local
legacy commands: family-1 opcodes `0xA1` and `0xA2`. Static recovery maps
them to BAR1 `ACQFVER` and BAR0 `FVER` reads respectively. The replacement
now returns the original 12-byte revision response format instead of rejecting
the requests.

The XStream literal `WaveRunner Driver Not Supported` has also been located
in `lecaladdinhwaccesspcisvr.dll`; that user-mode component is the next
reverse-engineering target for the Developer -> Run Link Tests support gate.


### Developer Link Test limitation is vendor-defined

The XStream Developer -> Run Link Tests message
`WaveRunner Driver Not Supported` has been reverse engineered in the installed
`lecaladdinhwaccesspcisvr.dll`.

The DLL explicitly marks the `S65` driver family as a WaveRunner driver and
refuses to run this developer test for that family before any link-test IOCTL is
sent. This is therefore expected vendor behavior, not a missing feature of the
replacement x64 kernel driver.

The same driver-family flag affects other ABI choices inside XStream, so
patching or falsifying it would risk switching XStream onto the wrong hardware
path. The current project intentionally leaves this vendor diagnostic gate
unchanged.


### Calibration A/B comparison and remaining ProBus issue

The previous concern about excessive recalibration was corrected by a
controlled Ch2 20 mV/div to 100 V/div sweep. The replacement appears to
calibrate each not-yet-calibrated voltage step once and reuse its calibration.
The legacy and x64 captures have closely matching calibration command
distributions and no failed relevant driver calls. Excessive calibration is
therefore not currently a confirmed compatibility regression.

ProBus hotplug is a confirmed remaining issue. In the working original trace,
probe insertion produces 85FB command-pending status `0x0200`, followed by
family-1 opcode `0x82` identification and family-0/1 opcode `0x4A` traffic
with metadata including `AP015`. The corresponding x64 trace never reports
pending `0x0200`, so those downstream commands are never issued by XStream.
The known-good waveform/DMA baseline has deliberately not been changed.

See `docs/probus-calibration-ab-comparison.md` for the four-trace comparison.


### AP015 ProBus startup recognition validated

Follow-up trace `xstream_trace_20260928_193741.jsonl` shows that when the
AP015 probe is connected **before** starting x64 XStream, it is recognized:
the normal family-0/1 opcode-0x4A initialization/metadata exchange succeeds,
and the captured metadata prefix matches the legacy AP015 response byte for
byte. Degauss and Auto Zero were invoked in this session and generate 0x4A
traffic, although one probe-control reply differs from legacy and needs
separate result verification.

Historical pre-patch x64 captures did not report pending 0x0200 for AP015
hotplug. This specific recognition fault was addressed in commit `70716ba`
and its first real-hardware unplug/replug validation (trace `230614`)
now observes two genuine pending-0x0200 transitions, corresponding opcode
0x88 acknowledgements, family-1/0x82 handshakes and successful AP015
reidentification by opcode 0x4A. No synthetic probe presence or
speculative DMA changes were needed.


### Quantified replacement-driver interface coverage

A 2026-09-28 audit of the original driver's 27 identified top-level
DeviceControl IOCTLs against `driver/Ioctl.c` found 20 whose behavior is
represented, one deliberately gated METHOD_NEITHER transfer (`CFDD219F`)
and six original dispatch entries not yet ported. The 20 include
`CFDC212C`, which correctly reproduces the original
`STATUS_NOT_IMPLEMENTED` response. This is approximately 74% dispatch
coverage, **not** 74% functional completeness: CFDC2110 alone contains many
additional recovered subcommands, while some currently implemented IOCTLs
remain limited to observed ABI variants.

Estimated normal oscilloscope usability is around 90%; broader
original-driver compatibility is around 75-80%. Remaining observed issues
include a separate probe-control response discrepancy, while some
rare service/diagnostic paths remain unimplemented or untested. The
previously missing AP015 hotplug notifications were verified in trace
`230614`. See
`docs/ioctl-map.md` for the exact remaining original IOCTL values.


### ProBus HWInt hotplug restoration: first hardware validation passed

A third controlled ProBus trace,
`xstream_trace_20260928_213834.jsonl`, confirmed that probe removal is not
detected and reinsertion makes the displayed waveform disappear even while
DMA IOCTLs continue to return success.

The original DPC raw assembly has now established the missing bridge:
receive interrupt source INTST `0x08` reads/clears BAR1 HWInt `0x410`,
latches its low 16-bit value against the enabled command mask, and wakes
the status event. Legacy firmware receive setup also enables INTEN bit
`0x08`, which the synchronous x64 polling implementation had omitted.
These recovered behaviors are restored in the replacement driver without
inventing probe-state bits or changing waveform DMA. The **first post-patch
real-hardware test** `xstream_trace_20260928_230614.jsonl` captured 19,288
IOCTLs, no observed failing NTSTATUS results, two spontaneous standalone
pending-0x0200 events (seq 14951 and 16639), their 0x88 mask-0x0200
acknowledgements, two family-1/0x82 handshakes, and re-established AP015
metadata through family-0/1 opcode 0x4A after reinsertion. All 3,633 captured
CFDC2138 operations return the requested byte count; waveform DMA continues
after hotplug. XStream visibly recognized probe changes according to the
user. The trace has snapshot gaps and does not independently contain
CPU-usage, ISR/DPC counters or a screen recording. Degauss / Auto Zero reply
parity is a separate, still-open protocol question.


### AP015 Degauss / Auto Zero and jaw-switch trace (231656)

After the now-verified HWInt restoration, the user's next real-scope
session exercised Degauss with automatic Auto Zero and the jaw sequence
open / closed / open / closed-locked. The 68.178-second JSONL contains
35,458 captured IOCTLs with no observed failing NTSTATUS, 6,701
CFDC2138 calls with byte-exact return counts, and five spontaneous
ProBus status notifications containing bit `0x0200`. All five are
acknowledged and followed by family-1/0x82. Its extracted status values
alternate `0x0058` (correlated with open) and `0x00A7`
(correlated with closed); the first opening simultaneously reports
pending `0x0280`. The capture has snapshot gaps and has no independent
CPU or screen recording.

The original x86 and this pre-fix x64 trace show byte-identical
family-0/0x4A calibration requests but different 85FB reply framing:
legacy `...02000000FFFF` versus x64 `...040000000000`.
In this x64 run Degauss `47 00` is issued five times and `47 12`
(Auto Zero-associated request) appears in five bursts of five calls,
including after the mechanical state changes. This repetition does
not establish successful physical calibration.

A renewed static read of original `FUN_000167F4` identified a concrete
host-side serialization difference: it pre-fills the solicited response
with 0xFF and writes the actual received byte count into header WORD+4,
rather than advertising the requested capacity and leaving zero padding
as the replacement previously did. This also affects short family-1/0x82
reply formatting. A narrowly scoped correction in `driver/Ioctl.c`
(commit `3490709`) changes only the raw 85FB result packaging, retaining
the existing explicit opcode-0x99 override. No interrupt, transport,
PCI or DMA behavior has changed. **First real-hardware regression `233125`:** the identical
`47 00` request now returns the full original x86 response
`0000000000000000000002000000FFFF`, rather than
`...040000000000`, and the old fivefold repetition disappears.
Two such requests occur ~20.64 s apart, correlating with Degauss
and manual Auto Zero. The separate family-1/0x4A status still reports
F3 instead of reference F2. Zero 47 12 requests appear in this manual
run, so 47 12 must not universally be identified as every Auto Zero
invocation. Physical calibration parity remains unproven.
See `docs/probus-calibration-ab-comparison.md` and the current handoff.


### Follow-up: AP015 jaw notification not observed in trace 233125

Trace `xstream_trace_20260928_233125.jsonl` contains 27,947 captured
IOCTLs across ~55.628 seconds, zero captured NTSTATUS errors, and 5,219
successful CFDC2138 transfers whose returned byte counts all match the
requested byte counts. Startup AP015 metadata remains identical across
the captured 128-byte output prefix to the reference and preceding runs.
The last DMA runs at trace end; 1,666 DMA entries are after the second
manual control packet.

Unlike pre-framing run `231656`, where five genuine pending-0x0200
events tracked calibration and open/close jaw transitions, all **1,135
standalone 85FB/0x01 status replies** in this run report enabled
`0x02BF` and pending **only `0x0080`**. No 0x82/0x88
mask-0x0200 follow-up occurs, despite the user's described physical
jaw movements. There are 29 capture-snapshot gaps, all ending by
t~28.646 seconds; the 447 observed status reads after t=40 seconds
are not interrupted by a snapshot gap. The trace does not contain
visible XStream jaw-state changes, manually timestamped actions,
CPU metrics or raw interrupt latch counts. It is not yet established
whether the behavior depends on probe calibration state or another
runtime condition, or is indirectly related to host reply framing.

For the next controlled run, test one jaw open/close pair **before**
Degauss/Auto Zero, then, if stable, another pair after Degauss and
manually triggered Auto Zero. Keep the verified IRQ, transport, PCI,
DMA and 32-bit descriptor safety paths unchanged until the event
source is discriminated; CPU limit <=5%. See
`docs/next-chat-handoff.md` and the trace comparison for the exact
procedure and signal markers.
