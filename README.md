# LeCroy WR6k native 64-bit acquisition driver

An independent open reverse-engineering project providing a native Windows x64 replacement for the legacy 32-bit LeCroy `LecS65AcqDrv.sys` used by WaveRunner 6000 / S65 acquisition hardware.

The goal is to run the original XStream software on 64-bit Windows while preserving the required device interfaces, driver ABI and hardware behavior.

> ⚠️ **Experimental project:** the replacement driver works on real WaveRunner hardware, but it is not yet a production-certified driver and is not an official Teledyne LeCroy release.

## 🎯 Project goal

| Goal | State |
| --- | --- |
| Native Windows x64 acquisition driver | ✅ Working |
| Original XStream compatibility | 🟢 Substantially working |
| Repeatable software and hardware regression tests | 🟢 Available |
| Remaining legacy interface coverage | 🟡 In progress |
| Local HLK readiness | 🔴 Pending |
| WHQL / WHCP production certification | 🔴 Long-term goal |
| Installation without test-signing mode | 🔴 Requires final Microsoft-signed release |

The intended end state is a maintainable, Microsoft-signed x64 production driver that can be installed on supported systems without enabling Windows test-signing mode.

## 📊 Current status

| Area | Status | Summary |
| --- | --- | --- |
| PCI / PnP driver binding | ✅ Verified | Native x64 driver binds to the supported LeCroy PCI device and exposes the recovered interfaces. |
| XStream acquisition | ✅ Verified | Real waveform acquisition works on WaveRunner hardware. |
| Normal scope controls | ✅ Verified | Timebase, vertical scale, coupling, bandwidth, trigger changes and tested multi-channel operation work. |
| DMA / interrupt path | ✅ Verified | The observed acquisition and transfer paths operate on real hardware. |
| ProBus / AP015 | 🟢 Working baseline | Probe identification, reconnect behavior and tested jaw-state handling work. |
| Legacy register ABI | 🟢 Mostly understood | The 43-entry register list and query/set ABI are documented and represented in the native implementation. |
| Diagnostics | 🟢 Available | `lecdiag`, regression scripts and runtime tracing support compatibility work. |
| Live IOCTL monitor | 🟡 Implemented | Native monitor source exists; final runtime validation is still pending. |
| Dallas EEPROM read | ✅ Verified | ROM identification and EEPROM backup work on real hardware. |
| Dallas EEPROM write | ⛔ Deferred | Intentionally not enabled on the licensed production device. |
| Serial FPGA/GPIO writer | ⛔ Deferred | Not implemented until its ownership and sequencing are sufficiently understood. |
| Production signing | 🔴 Pending | HLK / WHQL / WHCP work remains. |

Detailed implementation and verification state is maintained in the project documentation rather than duplicated here.

## 🚧 Current priorities

| Priority | Work |
| --- | --- |
| 🧹 Repository cleanup | Simplify documentation, remove duplicated historical state and keep current project information easy to navigate. |
| 🧪 Regression coverage | Continue consolidating safe Dry, Hardware and XStream validation. |
| 🔌 Compatibility | Close remaining proven XStream / legacy ABI gaps where safe and relevant. |
| 🛡️ Hardware safety | Keep hazardous Dallas and FPGA/GPIO write paths gated until independently verifiable. |
| 🧾 Release readiness | Prepare the project for HLK testing and eventual WHQL / WHCP certification. |

Open implementation work is tracked in [`docs/TODO.md`](docs/TODO.md).

## 🧪 Test layers

| Layer | Purpose | Current state |
| --- | --- | --- |
| Dry | Build and hardware-independent source / ABI contracts | ✅ Available |
| Hardware | Low-impact validation on the real PCI device | ✅ Verified baseline |
| XStream | End-to-end validation through the original application | 🟡 Expanding |
| HLK | Microsoft production-driver qualification | 🔴 Pending |

Unified regression entry point:

```powershell
.\scripts\test-driver.ps1 -Mode Dry
.\scripts\test-driver.ps1 -Mode Hardware
.\scripts\test-driver.ps1 -Mode XStream
.\scripts\test-driver.ps1 -Mode All
```

See [regression testing](docs/regression-testing.md) for test scope and safety notes.

## 🛠️ Build

The project targets Windows x64 and uses Visual Studio / MSBuild with the Windows Driver Kit.

```powershell
# Build driver and diagnostic utility
.\scripts\build-driver.ps1 -BuildLecdiag

# Build, test-sign and reload on the configured development scope
.\scripts\build-sign-load-driver.ps1
```

Installation and setup details are documented in [build, test and install](docs/build-test-install.md).

## 🧩 Repository layout

| Path | Purpose |
| --- | --- |
| [`driver/`](driver/) | Native Windows x64 kernel driver and INF |
| [`include/`](include/) | Reconstructed interfaces and ABI definitions |
| [`tools/lecdiag/`](tools/lecdiag/) | Diagnostic and compatibility utility |
| [`tools/lecwatch/`](tools/lecwatch/) | Live IOCTL activity monitor |
| [`scripts/`](scripts/) | Build, test and analysis helpers |
| [`ghidra_scripts/`](ghidra_scripts/) | Repeatable reverse-engineering scripts |
| [`docs/`](docs/) | Detailed architecture, ABI, hardware and testing documentation |

## 📚 Documentation

Detailed technical explanations live outside this README.

| Topic | Documentation |
| --- | --- |
| IOCTL compatibility | [IOCTL map](docs/ioctl-map.md) |
| Register ABI | [Original register list and write ABI](docs/original-register-list-and-write-abi.md) |
| Hardware registers | [Hardware register map](docs/hardware-register-map.md) |
| Device interfaces | [Device interfaces](docs/device-interfaces.md) |
| Regression strategy | [Regression testing](docs/regression-testing.md) |
| XStream automation | [XStream regression testing](docs/xstream-automation-regression-testing.md) |
| ProBus architecture | [ProBus detection and I2C architecture](docs/probus-detection-i2c-architecture.md) |
| PCI / acquisition-board topology | [Hardware topology](docs/pci-card-acquisition-board-topology.md) |
| Dallas safety and recovery | [Dallas test plan](docs/dallas-license-memory-test-plan.md) |
| Driver signing | [Signing and funding](docs/driver-signing-and-funding.md) |
| Open work | [TODO](docs/TODO.md) |

## ⚠️ Important limitations

- This remains an experimental reverse-engineered driver.
- Compatibility is based on the hardware and XStream paths tested so far, not every possible WR6k configuration.
- Dallas EEPROM writing is deliberately disabled until it can be validated safely on disposable or recoverable hardware.
- The serial FPGA/GPIO write path remains intentionally unported.
- Development builds currently rely on test signing.
- Private traces, Dallas contents, license data and proprietary vendor binaries are not part of this public repository.

## 📄 Project scope

The proprietary reference driver and vendor documentation are not redistributed. This repository contains independently written replacement code, tooling and derived technical documentation.

This project is community-driven and is not affiliated with or endorsed by Teledyne LeCroy.
