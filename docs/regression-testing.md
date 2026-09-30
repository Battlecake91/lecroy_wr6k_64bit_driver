# Regression test architecture

Date: 2026-09-30

The replacement driver now has one front-end regression runner:

    .\scripts\test-driver.ps1 -Mode Dry
    .\scripts\test-driver.ps1 -Mode Hardware
    .\scripts\test-driver.ps1 -Mode XStream
    .\scripts\test-driver.ps1 -Mode All

XStream end-to-end automation is now implemented as the third regression layer.

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
- private debug IOCTLs remain in their separate private CTL_CODE range;
- `lecwatch` remains a read-only observer with exactly one
  `DeviceIoControl` call site, targeting only `DEBUG_GET_TRACE`, with Dallas
  redaction retained and no private trace-clear command.

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

## XStream mode

XStream mode uses the installed XStream application through its local COM automation
server. It performs acquisition/waveform checks plus reversible vertical, horizontal,
coupling and bandwidth control roundtrips. Optional expected probe name,
amplitude and frequency assertions are available.

    .\scripts\test-driver.ps1 -Mode XStream
    .\scripts\test-driver.ps1 -Mode XStream -ExpectedProbeName AP015
    .\scripts\test-driver.ps1 -Mode XStream -ExpectedAmplitudeVpp 1 -ExpectedFrequencyHz 1000

The first implementation is not yet runtime-verified on the scope.

## All mode

All now runs Dry, then Hardware, then XStream:

    .\scripts\test-driver.ps1 -Mode All

XStream must be closed when All starts because the Hardware layer deliberately
refuses to run concurrently with it. The XStream COM step starts or connects to
XStream only after the hardware checks complete.

Any terminating failure stops the runner.

## Three-layer regression architecture

The intended layering is:

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

Dry mode was therefore owner-verified on the Windows development machine for
the then-current **8-contract** suite.

The dry suite has since gained one additional `lecwatch` source contract, so
the current source tree contains **9** dry checks. That new ninth check has not
yet been rerun by the owner. Keep the historical 8/8 result as valid evidence
for the earlier suite, but do not silently promote it to 9/9.

The owner executed the expanded Hardware suite on the real WR6k PCI device on
2026-09-30. All eleven default checks passed:

    SAFE HARDWARE REGRESSION: 11/11 passed; 0 failed.
    All requested safe hardware checks passed.
    REGRESSION SUITE PASS: Hardware

Observed passive hardware metadata during that run:
- debug stats version 1, legacy build 1002, unknown IOCTL count 0;
- logical BAR0 = 0x200 bytes, BAR1 = 0x40000 bytes, BAR2 = 0x200 bytes;
- PCI vendor/device 1570:0005, BDF 4:1.0;
- PCI command 0x0006 (memory space and bus master enabled);
- IRQ line 19, pin 1;
- START/FVER remained 0x00000002 and matched BAR0+0x000;
- both valid CFDC2400 zero-mask forms returned success with zero output bytes;
- malformed CFDC2400 lengths and CFDC2194 output lengths were rejected as expected.

Hardware mode is therefore owner-verified on the physical scope.
