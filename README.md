# LeCroy WR6k 64-bit driver reconstruction

Reverse-engineering project for the legacy LeCroy S65 acquisition kernel driver used by the WaveRunner 6000-series software.

## Goal

Reconstruct the externally visible behaviour of the original 32-bit `LecS65AcqDrv.sys` and implement a native x64 replacement suitable for modern 64-bit Windows.

The priority is compatibility with the existing LeCroy user-mode software:

- preserve device / interface behaviour where required;
- preserve IOCTL values and buffer layouts;
- preserve hardware access semantics;
- replace the obsolete x86 DriverWorks implementation with maintainable WDK code.

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
