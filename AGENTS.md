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

The incoming reference to `0x13AE2` is at undefined address `0x1115B`.
This remains a high-priority target for recovering the exact IOCTL-dispatch
branch.

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

1. Recover the raw instruction window around `0x1115B` and identify the exact
   IOCTL-dispatch branch that invokes `0x13AE2`.
2. Decode the wrapper details at `0x141DC` and `0x141F8` so the exact buffered
   vs METHOD_NEITHER acquisition ABI is explicit.
3. Continue resolving MAM/acquisition control semantics far enough to reproduce
   the x86 behavior safely in the x64 driver.
4. Keep partial `0xCFDC2110` hardware execution disabled until the full
   startup/runtime command set is understood.
