# Regression test architecture

Date: 2026-09-30

The replacement driver now has one front-end regression runner:

    .\scripts\test-driver.ps1 -Mode Dry
    .\scripts\test-driver.ps1 -Mode Hardware
    .\scripts\test-driver.ps1 -Mode All

XStream end-to-end automation is deliberately the next layer and is not part of
this first implementation.

## Dry mode

Dry is safe to run on a development PC without the LeCroy PCI hardware. By default
it first builds the x64 driver and lecdiag, then executes:

    tests\dry\test-source-contracts.ps1

It does not open a device, install/reload the driver, access PCI MMIO, touch Dallas
memory, invoke XStream or require an oscilloscope.

The initial dry contracts freeze facts that are particularly valuable before
refactoring:

- legacy build ABI remains 1002;
- the currently represented native legacy IOCTL constants retain their numeric
  values;
- the public reconstructed ABI still documents the three hazardous original
  controls that are intentionally not implemented;
- those hazardous controls remain absent from the native driver header and
  Ioctl.c;
- packed public ABI structs retain their compile-time size guards;
- private debug IOCTLs remain in their separate private CTL_CODE range.

For a fast source-only pass without compiling:

    .\scripts\test-driver.ps1 -Mode Dry -SkipBuild

The dry suite is intentionally not a fake hardware emulator. Source/ABI invariants
can be proven without hardware; acquisition, interrupt, DMA and MMIO behavior
cannot.

## Hardware mode

Hardware mode reuses the established scripts/test-safe-ioctl-batch.ps1 suite.
It requires the actual PCI device and installed replacement driver and requires
XStream to be closed.

It currently performs the owner-validated low-impact ABI batch: driver build
identity, PCI identity, START/FVER comparison, CFDC2400 zero-mask boundary behavior
and CFDC2194 invalid-size checks.

It does not build/install/reload the driver and it does not perform the known
hazardous indexed-register, Dallas-write or serial-trigger-FPGA programming paths.

The optional consuming error-status read remains explicit:

    .\scripts\test-driver.ps1 -Mode Hardware -IncludeErrorStatus

## All mode

All runs Dry first and Hardware second:

    .\scripts\test-driver.ps1 -Mode All

Any terminating failure stops the runner. This makes the same entry point usable
interactively now and by a future self-hosted hardware CI runner later.

## Planned third layer

XStream E2E will be added after these lower layers are established. Its design is
documented in docs/xstream-automation-regression-testing.md.

The intended final layering is:

1. Dry: build plus source/ABI contracts, no hardware.
2. Hardware: direct safe driver/PCI regression.
3. XStream: application-to-hardware end-to-end behavior.

Ordinary hosted CI can run Dry. Hardware and XStream require a machine attached to
the actual acquisition hardware.

## Current verification status

The files were added source-side on 2026-09-30. The existing hardware suite retains
its previously owner-reported 9/9 real-scope result. The new unified runner and dry
suite must not be called runtime-verified until they are actually executed on a
Windows development machine with the required Visual Studio/WDK toolchain.
