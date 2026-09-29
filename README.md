# LeCroy WR6k native 64-bit acquisition driver

An open reverse-engineering project providing a native Windows x64 replacement for the legacy 32-bit LeCroy `LecS65AcqDrv.sys` (S65 / WaveRunner 6000 acquisition hardware). The goal is to run the original XStream user-mode software on 64-bit Windows while preserving its device interfaces, IOCTL ABI and hardware behavior.

> **Status (2026-09-30): Experimental, running on a real WaveRunner scope.** XStream starts and acquires live waveforms using the replacement driver. This is an ongoing compatibility project, not a fully validated production driver or an official LeCroy release.

## Current functionality

| Area | Verified or implemented state |
| --- | --- |
| PCI and Windows interfaces | Native x64 WDM/PnP binding for `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00`, mapped BARs, recovered legacy interface GUIDs and common register/IOCTL access. |
| XStream acquisition | Real waveform display and recurring acquisitions on scope hardware. The user has tested timebase, vertical scale, coupling, bandwidth, trigger changes and 2-channel / 10-GS/s operation. Following the `0x00223044` register-query change, the owner reported no observed regression in the exercised XStream/AP015 baseline. |
| DMA and interrupts | Working, hardware-tested acquisition/transfer paths for the observed command forms, with locked buffers, chained legacy descriptors, completion events and recovered ISR/DPC acknowledgement. Unsupported transfer forms remain gated. |
| ProBus / AP015 | Probe identification and physical unplug/replug work. XStream detects the AP015 jaw state and displays the unlocked-jaw warning. The host response serializer and pending-command notification path are hardware-tested. |
| PCI Dallas DS2433 | ROM-ID read with CRC8 verification, 512-byte EEPROM read and private double-verified binary backup work on real hardware. Physical EEPROM writing is not implemented in the x64 driver. |
| Diagnostics | `lecdiag` provides PCI/register diagnostics, Dallas operations and IOCTL trace capture. The legacy four-byte START/FVER query (`0x00223044`) passed an independent read comparison against BAR0+0x000 on real scope hardware (both `0x00000002`). Newly rebuilt trace exports redact direct Dallas ROM/read responses and write-request data; older raw traces remain sensitive. |

These statements describe the tested hardware and observed XStream paths, not universal compatibility with every WR6k configuration, probe or service function. The original 27 top-level DeviceControl values have been identified; this does not mean every original control or nested command is implemented.

## Hardware and protocol boundaries

The scope contains distinct acquisition/front-panel hardware and a PCI interface board. The PCI board includes a Spartan-IIE (`U3 XC2S200E`), a local DS2433 (`U11`, connected to the FPGA through `ID_DATA`) and a separate `XC18V02` FPGA configuration PROM. Its differential connections to the acquisition board use separate 40-pin RX/TX headers.

All **five physical front ProBus sockets use I2C for probe communication**. A probe is initially classified by an analog identification value; the front-panel/probe EEPROM is subsequently identified over I2C, and probe control also uses I2C. A separate internal SPI path exists for board-level functions and must not be mistaken for probe-side SPI.

The **PCI-card DS2433 is independent of the front ProBus I2C EEPROM**. According to the hardware information supplied for this project, its writable 512-byte 1-Wire memory holds XStream license data.

### Dallas identity and licensing

- The DS2433 has a factory-programmed eight-byte ROM ID (family `0x23`, six serial bytes, CRC8) and a **separate** writable 512-byte EEPROM (16 pages of 32 bytes).
- The principal six-hex-digit component of the observed scope ID matches ROM serial bytes 1–3 interpreted as a little-endian 24-bit value. The displayed two-digit suffix has not yet been decoded.
- Transferring an EEPROM image to a replacement DS2433 does **not** transfer the original factory ROM identity. Whether every XStream license is bound to that identity remains unverified.
- The replacement driver implements `GET_DALLAS_ID` (`0x00223080`) and `READ_DALLAS_MEMORY` (`0x00223084`). **`WRITE_DALLAS_MEMORY` (`0x00223088`) is still missing** and currently returns `STATUS_INVALID_DEVICE_REQUEST` when XStream attempts to modify a license.
- The original x86 write behavior has been recovered from Ghidra: up to 32-byte scratchpad writes, scratchpad read/compare, copy authorization, a 100 ms post-copy wait and full-memory readback/retry. This is source analysis, **not** a tested x64 hardware write/restore implementation.

Existing private backups consist of two matching 512-byte raw EEPROM images; the original full eight-byte ROM ID has also been saved separately. New recovery tooling should associate both values in one private, integrity-checked backup container. Never put real license images, ROM IDs or raw startup traces in the public repository.

Further detail: [PCI and acquisition-board topology](docs/pci-card-acquisition-board-topology.md), [ProBus ADC/I2C architecture](docs/probus-detection-i2c-architecture.md), [Dallas backup and write validation](docs/dallas-license-memory-test-plan.md).

## Known limitations

- The x64 driver is an experimental test build; installation, test signing and hardware changes should be performed on a backed-up reference system.
- The Dallas EEPROM is **read-only through the current replacement driver**. The physical write/restore path needs implementation and independent validation on a disposable chip before use with a working licensed card.
- Compatibility is established for the observed XStream transfer forms. The WOW64-sensitive `0xCFDD219F` path, unobserved multi-channel/transfer variants and some legacy service/diagnostic operations remain gated or incomplete.
- Original `CFDC2194` error-status provenance is recovered from the legacy ISR. A matching 29-byte read-and-clear path plus corrected paired `CFDC2190` error-mask programming has been **built, test-signed, installed and positively queried on the real x64 scope** (2026-09-30, owner-provided build/install log: 0 warnings/0 errors, PnP restart success, idle `lecdiag error-status` `0x00000000`, 29-byte-format PASS). After installation, the owner completed the requested practical XStream waveform/control/two-channel/AP015 regression and reported **no observable malfunction**. This establishes the latest owner-confirmed working practical baseline, although real nonzero error-IRQ accumulation and persistent-error status bit 31 remain unexercised. See [error status investigation](docs/cfdc2194-status-latch-investigation.md).
- `CFDC2400` is now **source-implemented, not yet Windows-built or scope-tested** (2026-09-30). New Ghidra vtable evidence proves original handler ORs an exact 4-byte input into software pending interrupts and then directly invokes the DPC dispatcher via derived hardware-subobject slot `0x1C62C+0x24 -> 0x114F2 -> 0x11390`. The x64 port atomically adds the mask to its existing per-device pending field and synchronously invokes the existing DPC core at the required IRQL. Do not inject nonzero pending-mask values as an exploratory test on the only working scope. With this staged addition, 24/27 top-level original dispatch values have native cases (one of those is still gated), and three remain absent. The prior `CFDC2194` release remains the last owner-confirmed working XStream baseline. See [CFDC2400 investigation](docs/cfdc2400-software-pending-investigation.md).
- Some AP015 calibration/control response details remain to be independently characterized. Normal identification, connector hotplug and the unlocked-jaw indication are operational.
- XStream's Developer **Run Link Tests** page rejects the S65/WaveRunner family in its own user-mode DLL before issuing a link-test IOCTL. That vendor diagnostic limitation is not a kernel-driver regression.

## Build and diagnostic workflow

Use an administrative PowerShell on the development/test system. The project uses Visual Studio/MSBuild and the Windows Driver Kit. Keep a recoverable image of the working scope OS before installing an experimental driver. See the installation instructions for test-signing prerequisites and exact procedures.

```powershell
# Build the x64 driver and diagnostic executable
.\scripts\build-driver.ps1 -BuildLecdiag

# On the configured scope test machine: build, test-sign, reload and check
.\scripts\build-sign-load-driver.ps1

# Capture XStream activity locally for compatibility analysis
.\scripts\capture-xstream-trace.ps1
```

**Private Dallas backup** with XStream closed and a compatible `lecdiag` build:

```powershell
New-Item -ItemType Directory -Force '.\license-backups' | Out-Null
.\tools\lecdiag\build\lecdiag.exe dallas-id
.\tools\lecdiag\build\lecdiag.exe dallas-backup '.\license-backups\card-backup.bin'
```

`dallas-backup` checks the ROM ID before/after, reads the entire 512-byte image twice, refuses to overwrite an existing file and verifies the saved bytes. The separate `dallas-id` output must also be kept private. For redacted local page statistics and before/after offset comparisons use [`scripts/inspect-dallas-image.ps1`](scripts/inspect-dallas-image.ps1). **No command in this example writes the physical EEPROM.**

Raw XStream traces may include card identity and license contents because XStream reads the Dallas memory during startup. Store them only in private locations, including `trace-captures/` and `license-backups/` (both ignored by Git). Do not assume older exports are sanitized.

More build details: [Build/test/install](docs/build-test-install.md), [x64 bring-up reference](docs/x64-bringup.md) and [runtime trace documentation](docs/runtime-trace.md).

## Development backlog

Active work covers broader XStream compatibility/regression testing, remaining proven ABI variants, and a source-accurate Dallas write/restore path with disposable-device verification. Planned maintenance tooling includes a private backup container and an optional Dallas manager / Device Manager property page.

**Virtual Dallas ROM/EEPROM emulation for a dead or replaced chip, and any physical chip-isolation test, are explicitly deferred.** Neither virtual emulation nor a hardware recovery GUI is implemented. See the maintained [TODO](docs/TODO.md) and [Dallas recovery design](docs/dallas-device-manager-recovery-design.md).

## Repository guide

| Path | Purpose |
| --- | --- |
| [`driver/`](driver/) | Native x64 kernel driver, INF and Visual Studio project. |
| [`include/`](include/) | Reconstructed legacy interfaces and IOCTL ABI definitions. |
| [`tools/lecdiag/`](tools/lecdiag/) | Windows diagnostics and read-only Dallas backup. |
| [`scripts/`](scripts/) | Build, signing/reload, Ghidra export and private diagnostics. |
| [`ghidra_scripts/`](ghidra_scripts/) and [`ghidra_exports/selected/`](ghidra_exports/selected/) | Repeatable analysis scripts and selected, derived original-driver function exports. |
| [`docs/`](docs/) | Detailed hardware, protocol, ABI, runtime and development documentation. |

Recommended technical starting points: [IOCTL map](docs/ioctl-map.md), [hardware register map](docs/hardware-register-map.md), [ABI analysis](docs/abi-analysis.md), [device interfaces](docs/device-interfaces.md) and [runtime results](docs/runtime-trace.md). Contributors should also read [`AGENTS.md`](AGENTS.md) and the [current engineering handoff](docs/next-chat-handoff.md). The handoff and specialized documents retain investigation history; this README deliberately describes only the current consolidated state.

## Proprietary reference and data handling

The reference binary analyzed for this project is `LecS65AcqDrv.sys` (size **64,384 bytes**; SHA-256 `5f53de1dea6a58f201290e79a60fab587c039423322faa884bf4c15f0fd89087`). The proprietary original driver and supplied vendor schematics are not redistributed here. The public documentation contains derived analysis only.

The project is independent, community-driven work and is not affiliated with or endorsed by Teledyne LeCroy.
