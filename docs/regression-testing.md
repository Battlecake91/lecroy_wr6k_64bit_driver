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
- the public reconstructed ABI still documents the three recovered hardware-write controls;
- the now-implemented SetOneRegister path remains hardened to an exact 266-byte request, zero output, index `< 43`, and known-table-only resolution;
- Dallas WRITE `0x00223088` and serial FPGA/GPIO writer `0xCFDC2130` remain absent from the native driver header and Ioctl.c;
- packed public ABI structs retain their compile-time size guards;
- private debug IOCTLs remain in their separate private CTL_CODE range;
- `lecwatch` remains a read-only observer with exactly one
  `DeviceIoControl` call site, targeting only `DEBUG_GET_TRACE`, with Dallas
  redaction retained and no private trace-clear command.

For a fast source-only pass without compiling:

    .\scripts\test-driver.ps1 -Mode Dry -SkipBuild

The first P0 IRQ-safety source contracts additionally enforce that a failed
`IoConnectInterrupt` prevents START_DEVICE success, leaves interfaces
unpublished and rolls back BAR mappings, and that both the buffered acquisition
and CFDC2110 MTT DMA launch paths refuse operation without a started device
and a connected interrupt.

Owner-reported Windows 10 x64 Dry execution on 2026-10-04
(`scripts/test-driver.ps1 -Mode Dry`) confirmed:

- Driver MSBuild Debug|x64: **success**, 0 errors, 2 `LNK4075`
  linker-option warnings (INCREMENTAL/EDITANDCONTINUE ignored by
  the driver link configuration).
- `lecdiag.exe`: built as x64 (PE machine `0x8664`).
- Source/ABI contracts: **12/12 PASS**, including IRQ-required START
  and both DMA-launch admission gates.
- Regression runner: `REGRESSION SUITE PASS: Dry`.

This confirms build and source contracts, **not** loaded-driver behavior,
fault-injected IRQ failure, safe Stop/Remove or hardware acquisition.

## P0 request/removal lifecycle staging

A later revision of this branch (after the 12/12 owner-verified Dry run)
adds `IO_REMOVE_LOCK` coverage for CREATE, CLOSE, DEVICE_CONTROL, PnP,
Power and generic forwarded IRPs. Forwarded IRPs release their references
on lower-stack completion, not at the initial dispatch return.
STOP/SURPRISE_REMOVE/REMOVE now close IOCTL admission and wait for
synchronous operations before releasing transfer/event/BAR resources.
The quiesce sequence masks a known INTEN register only when mapped hardware
is accessible, disconnects the ISR, removes/drains the kernel DPC,
cancels the embedded timer, and invalidates the acquisition shadows.
Surprise Removal skips MMIO writes.

**No WDK build, Dry regression, PnP fault injection or real hardware test
has validated this later revision yet.** The prior 12/12 result belongs to
the earlier IRQ gating commit only. The added source contracts are
assertions about call presence and ordering, not proofs of concurrency safety.

Remaining hard blockers before production/HLK readiness:
- A DMA timeout or error may leave real bus-master activity running after
  software pointers and descriptors are released. Establish a proven
  hardware abort/reset/idle sequence or safe OS DMA-mapping lifecycle.
- Test remove-lock callbacks, Start/Stop/Remove, rearm races and fault
  injection under Driver Verifier/controlled hardware.
- Review whether power transitions, DMA-remapping/IOMMU and unexpected
  removals need a stronger hardware ownership model.

The IRQ failure path has **not** been fault-injected or hardware-tested.
A future controlled test must simulate `IoConnectInterrupt` failure before
any PCI device testing, verify the returned PnP failure status, disabled device
interfaces, BAR cleanup and zero DMA launches, and confirm normal START after
a clean retry. Passing a source contract does not prove asynchronous PnP,
DPC, timer or DMA removal safety.

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

It does not build/install/reload the driver and it does not perform SetOneRegister, Dallas-write or serial-trigger-FPGA programming paths. SetOneRegister is now represented in source, but remains excluded from the default hardware regression until a deliberate same-value validation is performed.

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

The dry suite later gained the `lecwatch` source contract and now also contains a hardened SetOneRegister source contract, so the current source tree contains **10** dry checks. These newer checks have not yet been rerun by the owner. Keep the historical 8/8 result as valid evidence for the earlier suite, but do not silently promote it to 10/10.

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


### lecwatch action-correlation mode

The XStream layer now has an optional observability mode:

```powershell
.\scripts\test-driver.ps1 -Mode XStream -TraceXStreamActions
```

This requires the current native `lecwatch.exe` to already be running.
The regression process sends synchronous `WM_COPYDATA` messages to
`LecWatchMainWindow`; lecwatch records the markers with its own QPC and keeps
using its existing read-only `DEBUG_GET_TRACE` path.

Current automatically delimited actions are forced acquisition and reversible
C1 vertical scale, horizontal scale, coupling and bandwidth-limit changes,
including separate restore windows. A 100-ms post-action settle interval lets
the 10-ms monitor reader consume the end of each driver burst before the action
summary is closed.

Saved sessions can be reduced with:

```powershell
python .\tools\lecwatch\summarize-actions.py <session.jsonl> --markdown <summary.md>
```

This is an analysis mode, not a new hardware test primitive: it drives only the
same XStream COM actions already present in the E2E regression and adds no
kernel-control capability to lecwatch.


## First XStream E2E runtime attempt exposed startup-readiness race

Owner run on 2026-10-01 with `-Mode XStream -TraceXStreamActions` proved COM
activation itself works: **XStream COM automation connection PASS**. The script
then immediately queried `app.Acquisition` while the scope/XStream hardware
initialization was still in progress. At that instant the COM dispatch object
did not yet expose `Acquisition`, causing one real startup-timing failure and
eight meaningless dependent follow-on failures (`VerScale`, `HorScale`,
`Acquire`, `Samples`, `DataArray`, `Coupling`, `BandwidthLimit`).

This is not evidence that those XStream functions or driver paths are broken.
The owner explicitly confirmed the scope was not yet ready.

Commits `2e7582e` and `7c36d37` replace the immediate access with an actual
readiness gate:

- default readiness timeout: 180 seconds;
- configurable range: 10..600 seconds via
  `-XStreamReadyTimeoutSeconds` on `scripts/test-driver.ps1`;
- poll once per second;
- require `Acquisition`, `C1`, `Horizontal`, `C1.Out.Result`,
  positive `VerScale` and positive `HorScale`;
- print the most recent not-ready reason every five attempts;
- only after the complete automation chain is usable do dependent E2E actions
  begin;
- if readiness really times out, the required readiness check terminates the
  dependent run instead of generating a cascade of false failures;
- when action tracing is enabled, readiness itself is timestamped into
  `lecwatch`.

The corrected readiness path has not yet received the owner's follow-up runtime
result.


## XStream PowerShell COM requires collection-based hierarchy access

Follow-up owner runtime evidence on 2026-10-01 showed that waiting longer did
not make the direct PowerShell expression `$app.Acquisition` appear. The COM
server itself connects successfully, but PowerShell reports that the
`Acquisition` property does not exist on the root RCW.

This matches Teledyne LeCroy's COM-client guidance: the Browser/VBScript path
`app.Acquisition.C1.VerScale` is a convenient automation alias, while
hierarchical COM clients should traverse the object collections, e.g.
`app.Objects.Item("Acquisition")`, then
`acq.Objects.Item("C1")`, and access CVARs through
`c1.Item("VerScale").Value`. Some older LeCroy examples use singular
`Object.Item`, so the PowerShell helper now tries both plural and singular
collection names.

Commits `557c2ff` and `70f8897` convert the XStream E2E suite accordingly:

- root and nested folders use `Objects.Item(name)` with
  `Object.Item(name)` fallback;
- C1/Horizontal controls use `Item(name).Value`;
- VerScale, HorScale, Coupling, BandwidthLimit and ProbeName no longer rely on
  PowerShell exposing VBScript convenience aliases;
- optional Measure/P1 amplitude and frequency setup uses the same collection
  traversal;
- waveform results still use the documented `Out.Result` interface, with an
  object-collection fallback for `Out`.

The readiness loop remains useful, but it now waits on the real collection
hierarchy rather than repeatedly probing the unsupported direct alias.

This corrected collection-based PowerShell path has not yet received the
owner's runtime result.


## Read-only XStream COM object scanner

The owner asked for direct discovery of what the old WaveRunner/XStream COM
server actually exposes instead of continuing to guess the hierarchy from
newer examples.

A read-only scanner now exists at:

```powershell
.\tests\xstream\dump-xstream-automation.ps1
```

It connects to `LeCroy.XStreamDSO` / `.1`, prints `Get-Member -Force`
metadata for the root and discovered objects, inspects `Object` / `Objects`
collections, tries collection Count/numeric/foreach enumeration, and probes
known object names using `Item(name)`. It does not set CVARs, invoke actions,
perform acquisitions or issue driver IOCTLs.

Default output:

```text
xstream-automation-dump.txt
```

The file is ignored by Git because it is machine/runtime-specific.

The same evidence also justified adding direct `Parent.Item(name)` as the
first child-resolution path in the E2E helper before trying
`Parent.Objects.Item(name)` or `Parent.Object.Item(name)`.
The scanner has not yet been owner-run; do not claim its observed hierarchy
until the resulting dump is supplied.
