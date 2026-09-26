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

- `0xCFDC2110` parsing/transport code is preserved, but hardware execution is
  intentionally disabled in the x64 driver.
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
pushes them. The local Ghidra installation path is stored in ignored
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

1. Build/install the updated x64 driver and `lecdiag` on the reference system.
2. Capture a normal XStream startup with
   `scripts/capture-xstream-trace.ps1` and analyze the JSONL sequence,
   especially firmware-forwarded `0xCFDC2110` requests.
3. Verify whether Windows supplies all source and descriptor-table pages below
   4 GiB; the current x64 transfer registration deliberately rejects addresses
   that do not fit the proven legacy 32-bit descriptor ABI.
4. Determine MAM/MTT launch-count units from passive normal operation before
   enabling `0xCFDC2138` or `0xCFDD219F` acquisition launch.
5. Keep partial `0xCFDC2110` hardware execution disabled until the observed
   startup/runtime packets have confirmed semantics.


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
  a source. Active acquisition launch is still gated.
- `0xCFDC2138` and `0xCFDD219F` are currently trace-visible but return
  `STATUS_NOT_SUPPORTED`; do not enable MAMRGO/MTTRGO launch until DMA
  address-width and launch-count units have been validated.
- `0xCFDC2110` remains trace-only for safety.

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
