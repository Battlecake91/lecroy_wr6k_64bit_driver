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

Owner-reported Windows dry run on 2026-10-04 at 18:29:
the expanded revision **built successfully** (0 errors, 2 pre-existing
`LNK4075` warnings), `lecdiag` built for x64, and **16/17** source
contracts passed. The one failed `START_DEVICE` contract used an outdated
regular expression expecting direct adjacency between `Started=TRUE`
and `LecEnableInterfaces`; correct source now inserts
`LecSetIoctlAdmission(devExt, TRUE)` between these lines.
The test regex was updated in commit `07653b9`.

Owner-reported rerun on 2026-10-04 at 18:34:52 after the regex fix:
`scripts/test-driver.ps1 -Mode Dry` **PASS**, x64 driver build
**success** (incremental, 0 warnings, 0 errors), x64 `lecdiag`
build **success**, and all source/ABI contracts **17/17 PASS**.
The zero-warning result describes the incremental build, not a clean
rebuild (which previously reported two `LNK4075` warnings).

**The revised implementation has a verified Dry pass, but no PnP fault
injection or hardware validation yet.** The prior 12/12 result belongs to
the earlier IRQ gating commit only. The added source contracts are
assertions about call presence and ordering, not proofs of concurrency safety.

## DMA ownership staging (not yet verified)

The latest draft revision, after the owner's 17/17 Dry PASS, introduces
fail-closed DMA ownership: physical-ISR completion tracking,
`CFDC2400` software bit-0 rejection, a latched unknown-DMA fault
and permanent quarantine of potentially bus-mastered MDLs and descriptor
memory across CLOSE/STOP/REMOVE. A latched fault blocks new IOCTLs and
START within the same FDO. It deliberately leaks locked memory until
system restart because safe abort/idle has **not** been proven. Read
[detailed DMA evidence](dma-lifetime-and-timeout.md).

Owner-reported Windows 10 x64 Dry run on 2026-10-04 at 19:38:25:
driver Debug|x64 compiled and linked successfully with **0 errors**
and the same **2 LNK4075** configuration warnings; x64 `lecdiag`
built successfully. **19/20 source contracts passed**. The one
failure was a stale regex requiring exactly the old
`Started || InterruptConnected` gate without the newly added
`DmaUnknownActive` guard. The test was updated in commit
`87bea88` to require all three checks in both launch paths.

**The updated regex has not yet been Windows-retested**, and the new
DMA containment has not been PCI hardware-tested. Regression requirements include rejecting software
completion without affecting the existing `CFDC2400` zero-mask
contract, 5-second simulated timeout, poisoned-transfer unregister
refusal, process-CLOSE, STOP/REMOVE quarantine ownership and
subsequent DMA/IOCTL admission rejection. Driver Verifier and
recoverable hardware tests remain mandatory before release.

### Staged DMA logical descriptor encoder

The owner confirmed the previous DMA-quarantine revision on Windows 10 x64:
`scripts/test-driver.ps1 -Mode Dry`, 2026-10-04 at 19:46,
**20/20 PASS**, successful incremental driver build (0 errors, 0 warnings)
and x64 `lecdiag` build. The prior clean build reported only the
two known LNK4075 linker-option warnings.

**After that verified checkpoint**, the draft gained a new
hardware-independent `driver/DmaLayout.c` +
`driver/DmaLayout.h` module, compiled into the driver project but
not yet called by the active acquisition path. It converts already
adapter-mapped 32-bit **device logical** segments to WR6k descriptor
entries, with page splits, chain-link slots, an end marker and bounds
checking. It does not call `IoGetDmaAdapter`, map pages or program PCI.
The existing legacy PFN-based acquisition path remains unchanged.

`test-driver.ps1 -Mode Dry` now also invokes a native C unit test executable
(`tests/dry/test-dma-layout.c`) via
`tests/dry/test-dma-layout.ps1`, with cases for table-page boundaries,
mapping length/misalignment, 32-bit range overflow and capacity.
Owner-verified Windows 10 x64 full Dry run on 2026-10-04 at 19:55:
- Debug|x64 driver build **PASS**, 0 errors, 2 existing LNK4075
  linker-option warnings; the new DmaLayout.c compiled and linked.
- x64 lecdiag build **PASS**.
- Source/ABI Dry contracts **21/21 PASS**.
- Native DMA descriptor layout tests **13/13 PASS**.
- Runner: `REGRESSION SUITE PASS: Dry`.

These are actual Windows native test results for the staged encoder,
**not** successful WDM DMA adapter use, IOMMU compatibility, verified
DMA bus idle, STOP/REMOVE stress or PCI hardware validation.

### Staged WDM DMA adapter ownership (unverified)

After the 21/21 + 13/13 owner-verified checkpoint, the draft now builds
an **inactive** `DmaAdapterStage.c` module. This acquires a WDM DMA
adapter, provides common-buffer allocation with a checked 32-bit
device-logical address, and refuses to return mappings without a
proven-idle predicate. A Dry source contract asserts that the WDM
adapter stage is not invoked by live PnP/acquisition/IOCTL paths.

Owner-verified Windows 10 x64 full Dry run on 2026-10-04 at 20:18:24:
- WDK Debug|x64 compile and link **PASS**, 0 errors, 2 known LNK4075
  linker-option warnings; `DmaAdapterStage.c` compiled and linked.
- x64 lecdiag **PASS**.
- Source/ABI contracts **22/22 PASS**.
- Native DMA logical layout unit tests **13/13 PASS**.
- Overall `REGRESSION SUITE PASS: Dry`.

This is an owner-reported source/build and synthetic-layout result,
**not** a runtime DMA mapping, kernel fault-injection or PCI hardware test. The adapter is
not yet used for transfer mappings and the PCI hardware path is
unchanged. Do not install the branch.

### Asynchronous SG ownership state model (pending Windows verification)

After the owner's 22/22 source contract and 13/13 DMA-layout Dry pass,
the source now contains an **inactive** pure mapping-ownership model,
`driver/DmaMappingOwner.c/.h`, compiled into the WDK project. It
enforces that pending callbacks, device-active DMA, and unknown
hardware ownership cannot return mappings. Synthetic completion alone
never proves idle. A separate MSVC unit test suite exercises lifecycle
and callback timing without making DMA DDI calls; the Dry runner invokes it.

Owner-verified Windows 10 x64 Dry run on 2026-10-04 at 20:55:02:
Debug|x64 build successful (0 errors, 2 known LNK4075 warnings),
`lecdiag` x64 built, source/ABI contracts **23/23 PASS**, native
DMA-layout tests **13/13 PASS**, and mapping-ownership transition
tests **17/17 PASS**. Overall runner: `REGRESSION SUITE PASS: Dry`.
These are software-only results; no mapped PCI DMA or callback
scheduling was exercised.

The existing active acquisition still derives 32-bit addresses from
CPU PFNs. WDM scatter/gather mapping and actual recovery remain unimplemented.

### WDM SG callback bridge (new, unverified)

The current draft additionally compiles `DmaScatterGatherStage.c`,
an **inactive** per-MDL `GetScatterGatherList` callback bridge.
It records the returned SG mapping under a spin lock and holds a device
reference until mapping release. `PutScatterGatherList` is guarded
against missing callback or unknown DMA idle. The synthetic state
model remains separate from real physical bus-stop proof.
A source contract asserts that PnP, IOCTL and acquisition do not
call the bridge. Owner-reported Windows Dry execution on 2026-10-04 at 21:26:23:
WDK x64 build **PASS** (0 errors, 2 pre-existing `LNK4075` warnings),
`lecdiag` x64 **PASS**, Source/ABI contracts **24/24 PASS**,
descriptor-layout tests **13/13 PASS**, mapping-ownership tests
**17/17 PASS**, overall `REGRESSION SUITE PASS: Dry`. This validates
compilation and software-only invariants, **not** live WDM DMA callbacks
or PnP/REMOVE runtime safety.

### SG callback ownership analysis integrated, mock test added (unverified)

The focused WDM SG read-only analysis established critical races in the
inactive bridge: a borrowed `Peek` pointer could be invalidated by
concurrent `Release`, `CallbackComplete` was published before the
callback returned, and adapter/MDL/callback/PnP lifetime ownership was
not unified. A failed mapping also wrote shared fields outside its lock.

The new isolated bridge revision replaces `Peek` with a bounded,
spin-lock-protected `LecSgStageCopySegments`; synchronizes failed
submission state and callback publication; and makes
`LecSgStageRelease` **always fail closed** pending a real callback
rundown and bus-idle protocol. `LecSgStageMarkLaunched` also
returns FALSE unconditionally to prevent accidental DMA activation. The latter deliberately leaks the
stage, device-object reference and any DMA mapping, even after
no-hardware-start mapping, instead of assuming retirement.
The live driver still cannot call this staging layer.

`test-sg-stage.c` and `sg-stage-mock.h` now execute the actual
bridge against a synthetic host-only WDM API, including inline/delayed
and racing callback/REMOVE, late callback after failure, and copied
list lifetime. The full Dry runner invokes `test-sg-stage.ps1`.
Owner-verified Windows 10 x64 full Dry regression on 2026-10-04 at
22:40:42 (after fixing the Win32-only test shim):
- Debug|x64 driver and x64 `lecdiag` builds **PASS**,
  0 warnings and 0 errors in the incremental WDK build.
- Source/ABI contracts **24/24 PASS**.
- DMA descriptor layout native tests **13/13 PASS**.
- DMA mapping ownership state native tests **17/17 PASS**.
- Actual inactive WDM SG bridge linked against fake DDIs:
  **27/27 PASS**, including inline/delayed callbacks, racing REMOVE,
  concurrent SG copying/closing, failed submission, late callbacks,
  duplicate callbacks and malformed/beyond-4GiB mappings.
- Overall **REGRESSION SUITE PASS: Dry**; 81/81 checks passed.

The preceding 22:32 run failed **only** to compile the new host test
shim due to Windows type/name collisions. The 22:40 test supersedes
that failure. The 0-warning result is an incremental build, not a
separate clean-build warning audit.

The successful host simulation **does not prove** WDM callback retirement,
adapter/MDL/PnP rundown or live DMA safety. The SG bridge still
rejects every launch and every release and is not called by the active
PFN-derived acquisition path.


### Staged WDM v3 synchronous no-launch mapping (Windows Dry verified)

The current draft includes `DmaSyncStage.c/.h`, an entirely **inactive** adapter-mapped
mapping path using `GetScatterGatherListEx` with
`DMA_SYNCHRONOUS_CALLBACK` and NULL callback. The adapter staging
layer requests DMA_OPERATIONS v3 and checks its entry points.

The sync stage owns each successful mapping until
`FreeAdapterObject(DeallocateObject)` without ever providing a
hardware-launch operation. The central adapter context holds the PDO,
sets the required v3 DMA address width to 32 bits, validates the v3
operations-table size and allows only one sync owner **per context**.
That owner reserves an outstanding count before each WDM call, blocks
submissions after STOP, validates the adapter map-register budget and
derives WR6k descriptor capacity from the allocated common-buffer length.
It permits teardown only after its no-launch
mappings have drained. Monotonic, nonreused tokens and parent-locked
copying/removal prevent borrowed-pointer use-after-free and duplicate
release. A separate one-time construction step keeps the spin lock stable;
repeated init and init/destroy transitions cannot clear a live owner. It
cannot be used by real PnP yet, and its pinned MDL chain is deliberately
borrowed from an external owner.

The fake-WDM test suite `test-sg-sync.c` runs the actual PnP parent, sync and
adapter stage sources against synchronous v3 DDI mocks. It covers adapter and
common-buffer ownership, allocation failure, context initialization and
GetEx resource failure, malformed-success cleanup, repeated/concurrent init,
APIs during init/destroy, sequential/concurrent double release, copy/release
and map/release races, multiple adapter contexts (including two contexts for
one fake adapter), 48-MiB MDL chains, cyclic/unlocked MDLs, invalid mappings,
map-register limits, absent/inconsistent descriptor backing, exact page-chain
and terminator capacity, START/STOP/START, failed START rollback, STOP during
GetEx, REMOVE with an outstanding mapping, repeated STOP/REMOVE, surprise
removal, concurrent release/teardown, parent-call rundown, stale generation
tokens, ordered IRQ/DPC/timer software quiescence and unknown-active retention.
Actual blocked fake DDIs reproduce quarantine during GetEx, committed mapping
release, Finish drain and failed START cleanup. Concurrent Begin, duplicate
notification, post-STOP retention, factory allocation failure and parent
destruction are checked against adapter, common-buffer, mapping, PDO and
parent-allocation counters.
`scripts/test-driver.ps1 -Mode Dry` now invokes this suite.
A source contract ensures the live PCI code cannot invoke this stage.

Current Windows Dry regression on 2026-10-05:
- Debug|x64 clean driver rebuild **PASS** (0 warnings, 0 errors),
  x64 `lecdiag` **PASS**.
- Source/ABI contracts **33/33 PASS**.
- DMA descriptor layout **13/13 PASS**.
- Asynchronous mapping ownership **17/17 PASS**.
- Asynchronous fake-WDM SG bridge **27/27 PASS**.
- Synchronous WDM v3 no-launch and PnP-parent fake-DDI suite
  **149/149 PASS**.
- Live PnP publication lifetime suite **38/38 PASS**.
- Live PnP IRP/remove-lock suite **8/8 PASS**.
- Live DMA completion-state suite **17/17 PASS**.
- Overall **REGRESSION SUITE PASS: Dry** (**302/302** checks).

`test-pnp-publication.c` compiles the actual publication, PnP parent, sync and
adapter-stage sources against the fake WDM surface. It verifies publication
and parent allocation rollback, the single held PDO reference, zero adapter or
mapping allocation, STOP reuse, surprise/remove cleanup, concurrent
unpublication versus an active wrapper user, stale/sequential duplicate
release rejection and independent retention of a synthetic unknown-active
legacy transfer. The retained fixture deliberately leaves the wrapper,
parent, PDO reference and transfer link outstanding, matching the restart-only
quarantine contract.

The expanded publication suite additionally injects parent-notification
failure after transfer ownership has committed, blocks notification while
REMOVE unpublishes and waits, rejects duplicate publication and transfer-list
insertion, checks ownership rollback when retention loses the unpublish race,
and covers duplicate surprise removal plus STOP-after-surprise. Allocation,
PDO references and retained-object counts are checked at each boundary.

`test-pnp-irp-lifetime.c` compiles the production
`PnpIrpLifetime.c` forwarding helpers with a fake lower stack. Deterministic
synchronous, pending, failed and power completions verify that ordinary
forwarding releases its remove lock exactly once in the completion routine,
marks a propagated pending IRP and never releases early. The START/REMOVE
forward-and-wait helper instead returns `STATUS_MORE_PROCESSING_REQUIRED` from
its completion routine, waits for pending completion and retains both IRP and
remove-lock ownership for its caller.

`test-dma-completion.c` compiles the production `DmaCompletion.c` tracker.
It deterministically verifies an IRQ immediately before timeout, a completion
event without IRQ evidence, early/repeated/stale/wrong-generation completion,
completion racing STOP/REMOVE uncertainty, quarantine racing attempted
release and no-launch versus launched cleanup. `CompletionObserved` never
becomes `IdleProved` implicitly and cannot authorize WDM mapping release.
Source contracts also require the generation-checked DPC/transfer-selection
binding and the post-serialization MAM fault recheck before setup MMIO.

These tests are software-only: no real OS SG mapping, actual PnP rundown
or physical DMA idle has been proven.
The active PFN descriptor/address behavior is unchanged; only software
completion attribution, selected-transfer rundown and terminal-fault admission
were hardened. No hardware tests are authorized.

Remaining hard blockers before production/HLK readiness:
- A DMA timeout or error may leave real bus-master activity running after
  software pointers and descriptors are released. Establish a proven
  hardware abort/reset/idle sequence or safe OS DMA-mapping lifecycle.
- Test remove-lock callbacks, Start/Stop/Remove, rearm races and fault
  injection under Driver Verifier/controlled hardware.
- Review whether power transitions, DMA-remapping/IOMMU and unexpected
  removals need a stronger hardware ownership model.
- The live lifetime wrapper now publishes one independently resident parent
  per AddDevice FDO/PDO and prohibits staged adapter entry from live sources.
  Before activation, join every staged borrowed MDL/request to remove-lock and
  wrapper rundown, and validate real repeated/malformed PnP, failed START,
  power and fault-injection paths under Driver Verifier.

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
