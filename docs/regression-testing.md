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

It currently performs eleven low-impact checks: driver build identity, private
debug-stats ABI, three mapped logical BARs, PCI identity, START/FVER comparison,
CFDC2400 zero-mask boundary behavior and CFDC2194 invalid-size checks.

The two newly added checks are read-only diagnostics. They verify debug structure
version/build metadata and confirm that all three logical BAR resources are mapped
with non-zero lengths. They do not read arbitrary MMIO register contents.

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

The first owner-run Dry execution on Windows 10 / VS 2022 / WDK successfully built
the driver and lecdiag: driver build completed with 0 warnings and 0 errors, and
lecdiag was produced as x64 PE machine 0x8664.

The initial source-contract phase reported 6/8 PASS. Both failures were traced to
the test harness, not to a driver/ABI mismatch: Windows PowerShell 5.1 treats hex
literals in the 0x80000000..0xFFFFFFFF range as signed Int32 values. That broke
numeric comparisons for CFDC/CFDD IOCTLs and the hazardous CFDC2130 check.

Commit 95be1ba4ebaf229b58d115fca3b5b9e12d0a267b changed the dry checks to compare
canonical eight-digit uppercase hex strings instead of PowerShell numeric literals.

The owner reran the complete Dry suite after this fix on 2026-09-30. The driver
build again completed with 0 warnings and 0 errors, lecdiag was built as x64
(PE machine 0x8664), and all source/ABI contracts passed:

    DRY REGRESSION: 8/8 passed; 0 failed.
    All hardware-independent contracts passed.
    REGRESSION SUITE PASS: Dry

Dry mode is therefore now owner-verified on the Windows development machine.

The previous hardware suite separately retains its owner-reported 9/9 real-scope
result. The expanded eleven-check hardware suite is source-side only until the owner
executes it; do not claim 11/11 yet.
