# Regression test architecture

Date: 2026-10-13

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

Current Windows offline regression on 2026-10-17 used a clean isolated
worktree at development merge commit
`aba7ba35ab75cb3b1e20935806eb86376a790726`:

- `scripts/build-driver.ps1 -Configuration Debug -BuildLecdiag -BuildLecwatch`
  with SDK/WDK **10.0.28000.0** and MSVC tools **14.41.34120**: Debug|x64
  driver **PASS**, 0 warnings, 0 errors; `LecS65AcqDrv.sys`, x64
  `lecdiag.exe` and x64 `lecwatch.exe` generated successfully.
- `scripts/build-driver.ps1 -Configuration Release`: Release|x64 driver
  **PASS**, 0 warnings, 0 errors.
- `dumpbin /headers /imports /exports` on both SYS files reports AMD64 machine
  `0x8664`, native subsystem, `DriverEntry` as the PE entry point, and only
  `ntoskrnl.exe` and `HAL.dll` imports. No exports are present or required.
  Debug is 104,960 bytes with SHA-256
  `803E3A0FA0040F644410030126F01780C503C10619B260B891143BFE46D57602`;
  Release is 76,288 bytes with SHA-256
  `B858D8E1F8228617138626AF6FBF3099D41E4D0C8F80ED56048886C58E16C0FE`.
- `scripts/test-driver.ps1 -Mode Dry -SkipBuild`: **394/394 PASS**, comprising
  source/ABI and package-workflow **52/52**, descriptor layout **13/13**, mapping ownership
  **17/17**, asynchronous SG **27/27**, synchronous v3/PnP **149/149**,
  PnP publication **38/38**, PnP IRP/remove-lock **8/8**, DMA completion
  **39/39**, deterministic interleavings **2/2**, and production INTEN helper
  checks **49/49**. Source contracts verify the shared Debug/Release driver
  manifest/toolset policy, the exact five-interface compatibility set, and the
  package workflow's no-install, pinned-toolchain, explicit-identity,
  no-overwrite and opt-in signing boundaries.
- The default run passed **5,000/5,000** DMA race iterations and
  **1,000/1,000** INTEN RMW iterations.
- `tests/dry/test-dma-completion.ps1 -StressIterations 10000` passed
  **50,000/50,000** DMA race iterations and **10,000/10,000** INTEN RMW
  iterations, plus the same 39 completion and 2 deterministic checks.
  Default plus expanded runs reproduce the established **66,000/66,000**
  stress comparison.

`tests/dry/test-package-workflow-negative.ps1` is a separate fail-closed suite,
not part of the 394 functional total. Its current 16/16 cases require rejection
of wrong/dirty source state, missing tools and build pairs, invalid identity or
certificate metadata, an existing output directory, a real mismatched SYS/PDB
pair, mocked InfVerif/Inf2Cat/signing/catalog-membership failures, and an
incomplete manifest. It never uses a certificate private key or a device.

`tests/dry/test-package-workflow-compatibility.ps1` is also separate from the
394 functional total. Run it explicitly under both supported hosts:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\dry\test-package-workflow-compatibility.ps1
pwsh.exe -NoProfile -NonInteractive `
  -File .\tests\dry\test-package-workflow-compatibility.ps1
```

The suite passed 19/19 on Windows PowerShell 5.1.19041.6456 / CLR
4.0.30319.42000 and 19/19 on PowerShell 7.6.5 / CLR 10.0.11. It reproduces the
manifest-path stage that previously failed on Windows PowerShell 5.1 and then
completes a mocked package-artifact and manifest round trip. It verifies path
containment, different-drive rejection, missing-file rejection, deterministic
paths with spaces and special characters, manifest completeness, and mocked
signed/unsigned verification states without using a certificate or private
key.

`tests/dry/test-elevated-signing-launcher.ps1` qualifies the tracked
Windows PowerShell 5.1 launcher with an elevated token and the parameter-
compatible mock at `tests/dry/fixtures/mock-signing-workflow.ps1`. Supply new
external transcript/result paths and the absolute PowerShell 7 executable used
for the wrong-host rejection check:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass `
  -File .\tests\dry\test-elevated-signing-launcher.ps1 `
  -EvidenceLog C:\absolute\new-launcher-test.log `
  -ResultPath C:\absolute\new-launcher-test-result.json `
  -PowerShell7Path C:\absolute\pwsh.exe
```

The final elevated run on Windows PowerShell 5.1.19041.6456 / CLR
4.0.30319.42000 passed 24/24. A separate real non-elevated invocation was
rejected with exit code 1 before creating output, log or marker files. No test
accessed a certificate store or private key, and none invoked real signing,
package tools, driver build, hardware or installation operations. The existing
package compatibility tests remain 19/19 and fail-closed tests remain 16/16 on
both Windows PowerShell 5.1 and PowerShell 7.6.5.

These executions qualify both Debug and Release x64 compilation and the
hardware-independent source/native contracts at the merged development source.
They do **not** install or load the driver, exercise kernel scheduling, prove
physical DMA retirement, run Driver Verifier or HLK, or qualify hardware.

Completion tests compile the production tracker and cleanup decision with
MSVC `/W4 /WX`. New deterministic cases cover wait-success/mask-failure,
timeout on either side of IRQ observation, terminal retention, next-generation
rejection after cleanup failure, preserved normal success and NeverLaunched
cleanup, plus duplicate IRQs before/after consumption and concurrent first
observation. Source guards tie the decision to both MTT/MAM cleanup paths,
status propagation, poison-before-notification, DPC/event lifetime locking,
unregister list ownership and ordered PnP rundown. These guards do not execute
the full acquisition/unregister path or prove kernel race freedom. No hardware
access, driver loading, Verifier or live DMA mapping was performed.

`test-dma-completion-races.c` compiles the actual `DmaCompletion.c` with native
Windows interlocked instructions and MSVC `/O2 /W4 /WX`. A test-only wrapper pauses one chosen thread
after an instruction completes; it does not replace the atomic operation with
a model. It forces delayed marker publication after UnknownActive and an
already-validated consumer returning TRUE after failed finalization. Both cases
retain terminal ownership: neither the marker nor the return value is an idle
or release certificate. Stress covers simultaneous observations, observation
versus mask failure/timeout, consumption versus mask failure, and an old
recorded generation versus a fresh generation. Finite repeated schedules are
not exhaustive or a substitute for kernel synchronization/IRQL qualification.

`test-interrupt-control.c` compiles the production InterruptControl.c with
MSVC `/O2 /W4 /WX`. Array-backed MMIO and critical-section-backed kernel locks
are test doubles, not device accesses. Tests cover acquisition enable/cleanup,
generic full writes preserving or conflicting with bit 0, physical versus
cached reads, receive-enable idempotence, mapping/length and synchronization
failure, repeated acquisition, terminal quarantine, STOP closure, surprise
unavailability and absent BAR rejection. Six gated two-thread schedules pause
a writer inside the production critical section while another caller attempts
an absolute update, RMW, STOP, quarantine or surprise invalidation. Five added
checks execute the production atomic unavailable predicate/invalidation:
fresh availability, writer-before-invalidation ordering, latch publication,
rejection of reads/cleanup without MMIO, and irreversible restart rejection.
One added source guard binds that predicate to the ISR and HWInt DPC before
their hardware branches; it is not host execution of those kernel paths.
Expanded stress runs 10,000
two-writer RMW schedules and checks both shadow and issued register value.
No recoverable bus-error return is fabricated for WRITE_REGISTER_ULONG, whose
production DDI is void. Failed mask issue feeds the separately tested actual
completion tracker; no full acquisition/ISR/DPC or kernel IRQL execution is
claimed. Late-IRQ filtering is checked against the shared shadow and production
source contract, not by injecting a real IRQ. See the complete inventory and
compatibility exception in [DMA lifetime](dma-lifetime-and-timeout.md#device-wide-inten-ownership).

For a genuine driver build, use the project's SDK/WDK **10.0.28000.0** with
the VS 2022 x64 C++ toolchain. The current clean-worktree result confirms that
this toolchain is available on the Windows development host. From **Developer
PowerShell for VS 2022 (x64 tools)** at the repository root, this fail-fast sequence
builds both configurations, checks binary metadata and runs only offline tests; it does not install
or load the driver. Inspect the reported SYS machine (must be AMD64), entry
point, imports and any exports; DriverEntry need not be an exported symbol.

```powershell
$ErrorActionPreference = 'Stop'
.\scripts\build-driver.ps1 -Configuration Debug -BuildLecdiag -BuildLecwatch
.\scripts\build-driver.ps1 -Configuration Release
foreach ($configuration in @('Debug', 'Release')) {
    $sys = Join-Path (Get-Location) "x64\$configuration\LecS65AcqDrv.sys"
    if (!(Test-Path -LiteralPath $sys)) { throw "Missing driver: $sys" }
    dumpbin.exe /headers /imports /exports $sys
    if ($LASTEXITCODE -ne 0) { throw "SYS metadata inspection failed: $sys" }
}
.\scripts\test-driver.ps1 -Mode Dry -SkipBuild
.\tests\dry\test-dma-completion.ps1 -StressIterations 10000
```

The merged PR #10 source is Debug/Release x64 build-qualified and
software-regression-qualified. It is not Driver Verifier, HLK, kernel-runtime
or hardware qualified and is not a deployment-qualified release. The next
controlled stage is defined in the
[kernel-runtime qualification plan](kernel-runtime-qualification.md).

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
remove-lock ownership for its caller. The Phase 2C status-attribution extension
passes 10/10, including synchronous and pending lower-stack
`STATUS_NOT_SUPPORTED` injection. It also verifies the exact structured
`lower-dispatch`, `lower-complete` and `lower-wait` diagnostics without invoking
a private key, device interface or hardware operation. The corresponding
source/ABI contract run passes 53/53.

PnP status instrumentation is disabled by default. Build an offline diagnostic
binary only with the explicit MSBuild property below; omitting the property
retains the uninstrumented production path:

```powershell
msbuild.exe .\driver\LecS65AcqDrv.vcxproj /m /t:Rebuild `
  /p:Configuration=Release /p:Platform=x64 `
  /p:WindowsTargetPlatformVersion=10.0.28000.0 `
  /p:VCToolsVersion=14.41.34120 `
  /p:LecPnpStatusDiagnostics=true
```

This command only builds an unsigned offline binary. It does not authorize
signing, package creation, installation or a PnP transition.

For a complete package, use the package workflow's explicit
`-PnpStatusDiagnostics` switch instead of relying on a file name or an
incremental build. The workflow clean-rebuilds both configurations, verifies
the marker state, and records the mode in manifest schema 2. The 2026-10-18
unsigned qualification passed 397/397 functional checks and all 66,000 stress
iterations; workflow compatibility passed 25/25 and fail-closed boundaries
passed 16/16 on both supported PowerShell hosts. Exact identities are recorded
in the [capture/package report](phase-2c-kernel-trace-capture.md).

`test-dma-completion.c` compiles the production `DmaCompletion.c` tracker.
It deterministically verifies an IRQ immediately before timeout, a completion
event without IRQ evidence, pre-GO rejection, the fail-closed modeled interval
between GO and active publication, fast IRQ acceptance after synchronized
publication, early/repeated/stale/wrong-generation completion, preservation of
matching evidence against a stale consumer, exactly one concurrent consumer,
completion racing STOP/REMOVE uncertainty, quarantine racing attempted release
and no-launch versus launched cleanup. `CompletionObserved` never becomes
`IdleProved` implicitly and cannot authorize WDM mapping release. Source
contracts also require the generation-checked DPC/transfer-selection binding,
the synchronized IIMCL/GO/active-publication ordering and the
post-serialization MAM fault recheck before setup MMIO.

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

## Kernel Runtime Qualification Batch 1 focused offline checks

After the one authorized instrumented installation, no driver source was
changed. The focused hardware-independent suites were rerun against diagnostic
HEAD `cc44c2984c57bb9acf47bf18e33b1bc8afc69cfe`:

| Suite | Result |
|---|---:|
| Source and ABI contracts | 54/54 PASS |
| PnP IRP/remove-lock lifetime | 10/10 PASS |
| PnP interface publication | 38/38 PASS |
| DMA completion contracts | 39/39 PASS |
| Deterministic completion interleavings | 2/2 PASS |
| Default DMA stress | 5,000/5,000 PASS |
| Native INTEN functional checks | 49/49 PASS |
| Default INTEN RMW stress | 1,000/1,000 PASS |
| DMA ownership contracts | 17/17 PASS |

These tests are software-only. They support the source review of admission,
remove-lock, interface, IRQ/DPC/timer and quarantine lifetimes; they do not
prove physical DMA retirement or hardware bus idle. No reproducible defect was
found, so no source patch, driver rebuild or expanded stress rerun was
justified.

## Kernel Runtime Qualification Batch 2 focused checks

The live R0 sequence exposed an offline native-runner parameter-binding defect
before its fifth command started: a `string[]` parameter admitted an empty
collection but rejected the required empty string element. The runner now uses
both `AllowEmptyCollection` and `AllowEmptyString`; the existing C# quoting
layer already encodes an empty argument as `""`.

`test-package-workflow-compatibility.ps1` now launches a three-argument fixture
and requires the middle empty argument to retain its exact position. Results:

| Suite | Result |
|---|---:|
| Package workflow compatibility, Windows PowerShell 5.1.19041.6456 | 26/26 PASS |
| Package workflow compatibility, PowerShell 7.6.5 | 26/26 PASS |
| Source and safety contracts | 57/57 PASS |
| PnP IRP/remove-lock lifetime | 10/10 PASS |
| PnP interface publication | 38/38 PASS |
| DMA ownership | 17/17 PASS |

New source contracts specifically cover balanced CREATE/CLOSE remove-lock and
IOCTL-admission ownership, legacy event-object replacement/teardown, and empty
native arguments. The change affects only the offline process runner and tests;
driver implementation code did not change. DMA/INTEN stress and driver rebuilds
were therefore not repeated.

## Kernel Runtime Qualification Batch 2B focused checks

The corrected runner reached `lecdiag`, which then rejected the authorized
zero-length raw payload before issuing an IOCTL. `parse_hex_bytes` now treats an
explicit empty string as a valid zero-byte input, initializes its two outputs
and avoids a zero-byte allocation. The rebuilt x64 utility has SHA-256
`ADD4A869C05DD85A55DD9308927D2AE946907F26C0D0054E46A54738441FB7BA`.

`test-runtime-lifetime-audit.ps1` is part of Dry mode. Its native model covers
event-reference survival after user handle close, serialized DPC/event
replacement orderings, close and teardown races; first, middle, last and
multiple interface-enable failures plus idempotent partial teardown; and the
proposed per-process DMA open-reference rule, including final-close retirement
and `UnknownActive` retention. Source contracts bind existing event/interface
ordering to production. The DMA portion is a design characterization, not a
claim that the current driver implements the proposed open-reference state.

`test-lecdiag-command-chain.ps1` compiles the exact production `lecdiag.c`
behind a fake Win32 device boundary and invokes it with the production native
process runner. It verifies all nine R0 and both R1 argument/IOCTL shapes plus
five negative cases under Windows PowerShell 5.1. Optional `-EvidenceRoot`
preserves the mock executable, separate runner logs and a result record in a
new, non-overwriting directory.

| Suite | Result |
|---|---:|
| Source and safety contracts | 59/59 PASS |
| lecdiag R0/R1 command-chain mock | 16/16 PASS |
| Runtime event/interface/DMA lifetime audit | 19/19 PASS |
| Process/file-object DMA lifetime model | 22/22 PASS |
| Stable runtime identity baseline mocks (Windows PowerShell 5.1) | 6/6 PASS |
| Package workflow compatibility, Windows PowerShell 5.1.19041.6456 | 26/26 PASS |
| PnP IRP/remove-lock lifetime | 10/10 PASS |
| PnP interface publication | 38/38 PASS |
| DMA ownership | 17/17 PASS |

Only the user-mode utility, tests and documentation changed. No driver build,
DMA/INTEN stress rerun, signing, installation or live retry was performed.

The 2026-10-20 signing-recovery qualification then ran the complete Dry suite
without rebuilding the unchanged driver. It passed 469/469 functional/model
checks, 5,000/5,000 default DMA stress iterations and 1,000/1,000 default
INTEN stress iterations. The total includes 62/62 source/safety contracts,
22/22 process/file-object DMA lifetime cases and 6/6 Windows PowerShell 5.1
stable-identity mocks. Expanded stress was not repeated because neither driver
code nor the stressed DMA/INTEN implementation changed.

The post-recovery real passive baseline exposed two metadata-only robustness
issues: some present PnP objects do not return a `Data` property for the
hardware-ID query, and German PnPUtil reports `Schnittstellestatus: Aktiviert`.
The baseline now ignores property results without `Data`, requires exactly one
interface path per class and accepts the explicit English or German enabled
status. Focused source contracts remain 62/62 and identity mocks 6/6; the real
stable-identity, service, hash and five-interface gate passes. No device handle
or IOCTL is used.

## Batch 8 package and recovery regressions

`tests/dry/test-crash-dump-readiness.ps1` exercises the exact versioned
crash-dump implementation against isolated JSON state. It covers audit/plan,
guarded apply, new-backup enforcement, kernel-dump and no-auto-reboot values,
verification, exact restore and preservation of unrelated settings. Result:
13/13 PASS. No registry or page-file setting is touched by the test.

The frozen Batch 7 unsigned package workflow built Debug and Release x64,
validated INF/catalog/PE/PDB/diagnostic-marker identity and completed the full
Dry plus expanded path. Batch 8 independently revalidated those frozen hashes,
tree identities and marker/PDB evidence without rebuilding. Recorded result is
397/397 functional checks, 5,000/5,000 default DMA, 1,000/1,000 default INTEN,
50,000/50,000 expanded DMA and
10,000/10,000 expanded INTEN iterations. The total stress count is
66,000/66,000. Both package signatures are deliberately absent.

The workflow now evaluates host-stream evidence in the transcript that
actually produces it. This corrects the fail-closed package attempts that
properly stopped when the test runner's expected child stderr escaped into the
outer stream, when a stale 53/53 source-contract count remained, and when
host-stream evidence was searched only in the redirected native-process log.
The successful fourth fresh output directory proves the corrected complete
path; no failed directory was reused.

Batch 8 adds two production-script test suites to Dry mode:

- `test-system-image-readiness.ps1`: 21/21 PASS for read-only inventory,
  exact disk/volume binding, system-disk rejection, NTFS and capacity gates,
  existing-backup rejection, execution acknowledgement, non-overwriting
  evidence, one-shot WBAdmin
  success/failure, post-backup catalog enumeration and the explicit statement
  that catalog success is not restore validation; and
- `test-recovery-layout.ps1`: 6/6 PASS for distinct Windows, EFI and recovery
  roots, required SYSTEM/kernel/BCD/UEFI/boot-WIM markers, input-hash
  preservation, missing-marker rejection and exclusive result output.

The existing crash-dump tests remain 13/13 PASS. A complete current Dry run
with `-SkipBuild` passes all suites plus 5,000/5,000 DMA and 1,000/1,000 INTEN
stress iterations. No live backup, media write, BCD/CrashControl change, reboot
or hardware access occurs in any of these tests.

## Permanent runtime qualification harness

Run the focused harness test after changes to runtime gating, device-state
collection, allowed command shapes or the lecdiag mock boundary:

```powershell
.\tests\dry\test-runtime-ioctl-qualification.ps1
```

The test compiles the production `lecdiag.c` behind the fake Win32 boundary
and launches the exact production harness in Windows PowerShell 5.1. Current
result: 21/21 PASS. Coverage includes UTF-8 with and without BOM, German
metadata, stable identity across BDF 4:1.0 and 5:1.0, missing/changed PnP data,
Device Guard inactive/active/unknown/contradictory states, hash failures,
problem code 52, interface multiplicity, acquisition clients, the seven-command
allowlist, exact input buffers, timeout, missing output, non-overwrite and
stop-on-failure behavior. The full PASS path observes exactly eight mock
`DeviceIoControl` calls: one for each of five R0 commands and the PCI R1 read,
plus two reviewed reads for `start-register`.

The additional Batch 3 path permits only R0-8, R0-9 and the two conditional R1
commands. Its PASS case observes exactly five mock calls and proves that no
completed R0-1 through R0-7 request can be dispatched. Its failure case proves
that an R0-8 error prevents R0-9 and both R1 commands.

The R0 trace-size fixture returns `0x110`, matching the current driver source
and canonical ABI analysis. The historical `0x1A08` expectation was stale and
must not be reintroduced. The permanent harness still requires a separate
owner authorization for `RunAuthorized`; successful mocks or `Preflight` do
not grant hardware access.

The legacy query-size audit adds public canonical constants and compile-time
driver/tool layout assertions. `test-source-contracts.ps1` binds the public
`0x110`, `0x10A`, 43 and `0x2CAE` values to driver and mock behavior. Current
focused results are 63/63 source/ABI contracts and 16/16 lecdiag command-chain
cases. The stale `0x1A08` value is retained only as historical failure evidence.

## Device Guard preflight predicate

`tests/dry/test-deviceguard-preflight.ps1` imports the exact production
predicate from `scripts/deviceguard-preflight.ps1`. It runs without CIM, BCD,
device or hardware access and covers zero service sentinels, empty arrays,
null and absent values, known active HVCI, unknown numeric values,
contradictory reports, configured-but-not-blocking state, enforced kernel CI
and an active CI policy file. PowerShell 7.6.5 and Windows PowerShell
5.1.19041.6456 both pass 10/10. The live `{0}` Device Guard vectors evaluate
as `Inactive`; raw CIM values remain serialized with the decision.

This focused change does not touch driver, DMA or INTEN implementation. Driver
builds and unrelated stress suites are therefore not repeated.


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

## Batch 4 independent PR #27 reproduction

The exact PR #27 head `1120687c181e85ade70f91d656d127ef0533d7a6`
was checked out in an isolated detached worktree. SDK/WDK 10.0.28000.0 and
MSVC 14.41.34120 produced Debug and Release x64 drivers with zero warnings and
zero errors. The resulting SYS hashes are:

| Configuration | SHA-256 |
|---|---|
| Debug x64 | `D12C4822719193F0F4EDAAFBEEAB3760F351A6A95C3B901BE9B7E115044EB496` |
| Release x64 | `C2017E20F5E52DE0711B550E02567E181CCF78FFD227B812610B2DEAD5D4E325` |

Both images are AMD64 native drivers with `DriverEntry` and the expected
`ntoskrnl.exe` and `HAL.dll` imports. The complete Dry runner passed, including
63/63 source/ABI contracts, 23/23 process/file lifetime cases, 19/19 combined
lifetime cases, 10/10 PnP IRP cases, 38/38 publication cases, 39/39 DMA
completion cases, 2/2 deterministic interleavings and 49/49 INTEN functional
cases. Default stress passed 5,000/5,000 DMA plus 1,000/1,000 INTEN;
the independent expanded run passed 50,000/50,000 DMA plus 10,000/10,000
INTEN. No test opened the device, signed or installed a driver, or performed
MMIO/DMA/acquisition.

The Batch 4 launcher-only change adds one source contract, bringing its branch
to 64/64. Elevated Windows PowerShell 5.1.19041.6456 launcher mocks pass 20/20
and cover the one-shot restart gate without invoking real PnPUtil. The final
mock transcript SHA-256 is
`226FF30F0218DE1F53467AD27B686DF75DB8CFF40CD888F7A44CB15382734592`;
its result JSON SHA-256 is
`B5DD9B0CCFAA0C95F46FF82A27F7F5550ABEA206069399DC5285B66A4BD79914`.
Live DebugView buffering exposed a gap not represented by the immediate-write
mock: a 15-second readiness window can expire while valid kernel records remain
buffered. Real active modes now reject windows below 60 seconds and reserve at
least 120 seconds after the deadline. This correction is offline-qualified
only; a new authorization is required before any live restart.
## Multi-handle DMA ownership qualification

The production driver now associates each successful CREATE with its file
object and creating process. DMA token operations resolve ownership from that
association, and only the last CLOSE for the recorded process retires safe
inactive registrations. Focused source contracts bind CREATE rollback, CLOSE
detachment, last-handle release, IOCTL owner lookup and REMOVE-time context
rundown to the production source. The deterministic process/file model covers
both REGISTER/final-CLOSE orders, duplicate CLOSE, independent processes,
surprise removal and irreversible `UnknownActive` retention.

Offline results for this change:

- Debug x64 and Release x64: 0 warnings, 0 errors;
- source/ABI contracts: 63/63 PASS;
- process/file-object lifetime: 23/23 PASS;
- combined runtime lifetime audit: 19/19 PASS;
- complete Dry suite: PASS;
- default stress: DMA 5,000/5,000 and INTEN 1,000/1,000 PASS;
- expanded stress: DMA 50,000/50,000 and INTEN 10,000/10,000 PASS.

No live driver, PnP, device, MMIO or DMA operation is part of this evidence.

## Batch 10 fail-closed DMA and continuation qualification

The Batch 10 driver change blocks the legacy PFN MAM and MTT launch functions
at compile time by default and adds page/DWORD alignment checks to the legacy
descriptor builder. Source contracts require both launch gates and the new
validation. The installed driver is not involved in these tests.

SDK/WDK 10.0.28000.0 and MSVC 14.41.34120 built Debug x64 and Release x64 with
zero warnings and zero errors. The complete Dry runner passed:

- source/ABI contracts: 65/65;
- logical layout: 13/13;
- mapping ownership: 17/17;
- asynchronous SG bridge: 27/27;
- synchronous WDM-v3/PnP staging: 149/149;
- PnP publication: 38/38;
- PnP IRP lifetime: 10/10;
- lecdiag command chain: 16/16;
- combined runtime lifetime: 19/19;
- process/file DMA lifetime: 23/23;
- continuation mocks: 14/14;
- completion: 39/39 plus 2/2 deterministic interleavings;
- default stress: DMA 5,000/5,000 and INTEN 1,000/1,000.

The independent expanded run passed DMA 50,000/50,000 and INTEN
10,000/10,000. These tests prove host-side gates, ownership and deterministic
failure behavior only. They do not prove a physical abort, bus idle, final PCI
write ordering, power-transition safety or WDM-v3 acquisition readiness.

## Batch 6 capture and integration qualification

The hardened Windows PowerShell 5.1 launcher path passes 31/31 elevated mocks.
Coverage includes timestamped and millisecond records, `PNP_STATUS:` records,
split and delayed writes, a 5,000-line burst, incorrect embedded source text,
no output, early and post-readiness exit, timeout, collector stderr, successful
restart, PnP failure/cleanup and one-shot no-retry behavior. The transcript
SHA-256 is
`C71B94483966C214C80B236BC54A258896579AC9BC2E77F39ED163D6636E2103`;
the result JSON SHA-256 is
`EBDE8743716EBE8A2087255F3BB5F88916A9737CCC0AC3A41A4A2CBFF8ACC0EE`.
Source contracts pass 64/64.

## Batch 13 no-launch contracts

`tests/dry/test-pre-go-side-effects.ps1` consumes the machine-readable
allowlist and checks the exact production functions, compile gate, synchronous
DMA API choice, unconditional launch rejection and cleanup boundary. It fails
if a register/port primitive or hardware-launch helper becomes reachable.

`tests/dry/test-no-launch-preparation.ps1` runs the exact one-shot launcher
through the mocked `lecdiag` Win32 boundary and verifies `CFDC2124`, expected
launch-disabled `CFDC2138`, `CFDC2128`, non-overwriting evidence and hash
rejection. The command-chain matrix is now 17/17, the side-effect contract is
10/10 and the runner contract is 3/3. The package workflow's current aggregate
is 776 hardware-independent checks plus 66,000 stress iterations.

Batch 14 retains that observed aggregate but no longer treats `776` as a
separate signing constant. The package manifest records 23 named suite
outcomes and derives its functional total from them. The launcher verifies the
exact suite identities, per-suite success and recomputed totals. Windows
PowerShell 5.1 package compatibility remains 26/26; the full elevated no-key
launcher path passes 36/36.

After integrating the exact PR #27 driver tree, focused tests pass:

- process/file DMA lifetime: 23/23;
- combined runtime lifetime: 19/19;
- PnP IRP lifetime: 10/10;
- PnP publication: 38/38; and
- source/ABI contracts: 64/64.

The driver source is byte-identical to the previously built and stressed PR #27
revision. The 50,000 DMA and 10,000 INTEN expanded stress evidence therefore
remains applicable; no unrelated stress rerun was performed.

## Batch 11 WDM-v3 transaction qualification

The new portable transaction suite runs the production state machine and
descriptor encoder with only its post-GO evidence transition compiled as a
host-test hook. It passes 134/134 checks covering owner mismatch, adapter and
buffer admission, mapping failure and late callback, invalid/unrepresentable
addresses, descriptor capacity, publication barrier, hard launch rejection,
cancel-before-GO, IRQ during Arming, stale/duplicate completion, timeout,
POWER/PnP teardown, independent idle proof, quarantine and repeated transfer
generations.

The synchronous fake-DDI/PnP suite now passes 153/153 and additionally proves
QUERY admission closure, Dx closure, failed D0 restore and successful D0
reopening. The explicit WDM-v3 transaction-DDI suite passes 74/74, including
partial/malformed mapping rejection, descriptor encoding, exact no-launch
flush/free ownership and flush-failure quarantine. Source/safety contracts
pass 69/69. The exact-session continuation
workflow passes 17/17 mocks, including prompt/CLI drift, child failure and
timeout with disabled-task evidence retention.

Debug and Release x64 builds use SDK/WDK 10.0.28000.0 and MSVC 14.41.34120 and
complete with zero warnings and zero errors. The complete Dry runner passes.
Default stress passes 5,000/5,000 DMA and 1,000/1,000 INTEN; the independent
expanded run passes 50,000/50,000 DMA and 10,000/10,000 INTEN. These results
prove only software state, ordering and fail-closed behavior. They do not
authorize hardware launch, mapping flush after GO or acquisition.

## Batch 12 acquisition-integration qualification

The production `CFDC2138` preparation call is now compiled into the driver and
exercised by the same fake WDM-v3 DDIs used by the synchronous owner. Focused
coverage validates successful map/build/publish/hard-launch-denial/release,
pre-map cancellation, stable owner identities, query and power admission,
STOP/REMOVE/surprise rundown and irreversible quarantine. Synchronous SG/PnP
coverage is 157/157; publication is 38/38; source contracts are 69/69; exact-
session continuation is 17/17.

SDK/WDK 10.0.28000.0 and MSVC 14.41.34120 build Debug and Release x64 with
zero warnings and zero errors. The complete Dry runner passes. The expanded
run passes DMA 50,000/50,000 and INTEN 10,000/10,000; an additional DMA race
run passes 250,000/250,000 observations. All are hardware-independent. They do
not qualify GO, acquisition MMIO, physical DMA retirement or installed-driver
behavior.
