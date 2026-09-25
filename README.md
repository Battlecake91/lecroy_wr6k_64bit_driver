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

The first static-analysis passes now expose most of the driver's outer interface:

- all 27 DeviceControl dispatch values have been recovered;
- the generic raw register read/write ABI is known;
- the driver-build query is known;
- Dallas/1-Wire buffer contracts are known;
- the named BAR0/BAR1/BAR2 register map has been reconstructed;
- all four device-interface GUIDs registered by the original driver have been recovered;
- the legacy `METHOD_NEITHER` transfer path has been identified as the main x64/WOW64 ABI risk.

Documentation:

- [docs/original-driver-analysis.md](docs/original-driver-analysis.md)
- [docs/ioctl-map.md](docs/ioctl-map.md)
- [docs/abi-analysis.md](docs/abi-analysis.md)
- [docs/hardware-register-map.md](docs/hardware-register-map.md)
- [docs/device-interfaces.md](docs/device-interfaces.md)
- [docs/legacy-inf-analysis.md](docs/legacy-inf-analysis.md)
- [docs/reference-system.md](docs/reference-system.md)
- [docs/user-mode-components.md](docs/user-mode-components.md)

Reusable reconstructed ABI definitions:

- [include/LecS65LegacyIoctl.h](include/LecS65LegacyIoctl.h)
- [include/LecS65LegacyInterfaces.h](include/LecS65LegacyInterfaces.h)

No replacement driver code has been written yet. This is intentional: the externally visible ABI and hardware interface are being reconstructed first.

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
