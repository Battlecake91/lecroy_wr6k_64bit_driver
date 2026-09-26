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
- the acquisition-buffer path is confirmed to use locked user pages, MDL chains and a board-facing descriptor table built directly from PFNs;
- the packed `0xCFDC2110` command parser and all three A5FB command families are substantially decoded;
- the exact DeviceControl branch `0xCFDC2110 -> 0x13AE2` is now confirmed from raw dispatch instructions;
- the acquisition front-ends are now mapped to `0xCFDC2138` and the WOW64-sensitive `0xCFDD219F` METHOD_NEITHER path, both converging on the same acquisition orchestrator and synchronous transfer/wait path;
- the legacy `METHOD_NEITHER` path remains the main x64/WOW64 ABI risk;
- a native x64 compatibility driver exists and is being brought up incrementally on `main`; unsafe partial `CFDC2110` execution is intentionally disabled until semantics are complete.

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

It also registers four PnP device-interface classes:

```text
{7AC34BE9-F766-4F15-9E88-854BA5E2146E}
{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}
{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}
{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}
```

The installed legacy INF has now been recovered as well. Windows publishes it as `oem18.inf`, while the file identifies itself as the original `LecS65AcqDrv.inf`. It confirms `PCI\\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00`, service name `LecS65AcqDrv`, device class `DataAcquisition`, class GUID `{BA5FE95F-EE73-4113-8121-F38CC4FF0095}`, and binary name `LecS65AcqDrv.sys`.

Together, those findings give the future x64 driver a useful incremental bring-up route: enumerate the same interfaces, bind to the confirmed PCI ID, map the PCI BARs, validate raw register access, then move on to Dallas, interrupts and acquisition/DMA.


## Native x64 bring-up prototype

Development originally started on `prototype/x64-bringup`, but that work has been merged and active development now happens on `main`.

The x64 prototype implements PCI/PnP bring-up, the recovered legacy DOS device path, BAR mapping, build query, raw register access, Dallas/1-Wire access, event-registration compatibility and detailed IOCTL tracing. Interrupt and acquisition behaviour are being reconstructed from the original x86 driver before more hardware execution is enabled.

See [docs/x64-bringup.md](docs/x64-bringup.md).
