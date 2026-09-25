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

The first two static-analysis passes are complete enough to expose the core shape of the driver:

- all 27 DeviceControl dispatch values have been recovered;
- the generic raw register read/write ABI is known;
- the driver-build query is known;
- Dallas/1-Wire buffer contracts are known;
- the named BAR0/BAR1/BAR2 register map has been reconstructed;
- the legacy `METHOD_NEITHER` transfer path has been identified as the main x64/WOW64 ABI risk.

Documentation:

- [docs/original-driver-analysis.md](docs/original-driver-analysis.md)
- [docs/ioctl-map.md](docs/ioctl-map.md)
- [docs/abi-analysis.md](docs/abi-analysis.md)
- [docs/hardware-register-map.md](docs/hardware-register-map.md)

No replacement driver code has been written yet. This is intentional: the externally visible ABI and hardware interface are being reconstructed first.

## Reference binary

Analysed binary:

```text
Filename: LecS65AcqDrv.sys
SHA-256: 5f53de1dea6a58f201290e79a60fab587c039423322faa884bf4c15f0fd89087
Size:     64384 bytes
```

The proprietary reference binary is not stored in this public repository.

## Important current finding

The legacy driver exposes generic register access:

```text
0xCFDC21C0  generic register read
0xCFDC21C4  generic register write
0xCFDC21C8  driver build query -> 1002
```

This gives the future x64 driver a very useful incremental bring-up route: BAR mapping and basic hardware access can be validated independently of the acquisition/DMA implementation.
