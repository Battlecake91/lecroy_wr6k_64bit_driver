# AGENTS.md

This file is the persistent hand-off and operating guide for this repository.
Every agent/chat working on this project should read it first and keep it current.

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

1. Hardware-test the BAR1 CLRIRQ source-specific interrupt acknowledge added
   after `xstream_trace_20260928_004553.jsonl`.
2. Trace 004553 proves calibration now completes after the opcode-0x88 local
   acknowledgement fix, and every captured IOCTL returns success.
3. Immediately after calibration, x64 enters an abnormal high-rate loop:
   standalone 85FB/0x01 reports pending `0x0080`, local opcode-0x88 clears
   it, `CFDC2184` runs, and pending `0x0080` is asserted again almost
   immediately. The trace ring records about 91k calls while sequence numbers
   exceed 1.5 million, proving a severe interrupt/event retrigger loop.
4. Static ISR recovery shows the missing hardware acknowledge:
   INTST 0x04/0x08/0x10/0x20 must write 1/2/4/8 respectively to BAR1 CLRIRQ
   (offset 0x008) before the common INTST write-back. This is now implemented.
5. Keep the proven CFDC2138 and family-1 0x51 DMA paths intact. Keep CFDD219F
   and unobserved multi-channel CFDC2138 gated, and preserve the below-4-GiB
   descriptor safety check.


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
