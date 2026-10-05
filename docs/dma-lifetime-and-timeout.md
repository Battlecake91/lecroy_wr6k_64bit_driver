# WR6k DMA completion, timeout and memory lifetime

Status: **software-only staged lifetime hardening**, 2026-10-05.
Base reviewed: `fix/p0-irq-start-dma-gating` at `9f2ffa9`.
Analysis: original x86 disassembly compared against the native x64 acquisition and cleanup paths. **Neither an original-hardware idle guarantee nor a successful x64 hardware test is established here.**

## Established original x86 control flow

Both original acquisition variants converge through `0x17478 → 0x171DE`:

- MAM/`CFDC2138`: `0x13C84 → 0x12D6A → 0x17478 → 0x171DE`.
- MTT/family-1 `CFDC2110`: `0x160DC → 0x17478 → 0x171DE`.

Original `0x171DE` writes BAR0 `SGTA +0x040` at `0x171EA–171ED`, `IIMTC +0x044` at `0x17204–1720A`, resets completion event at `0x1721B–17222`, enables INTEN bit 0 at `0x17228–1722C`, sets `IIMCL +0x048 = 1` at `0x1722F–17235`, launches MAMRGO (BAR1 `+0x064`) or MTTRGO (BAR1 `+0x084`) at `0x1723A–1724F`, waits up to five seconds at `0x17254–17279` and disables INTEN bit 0 at `0x17281–17285`. Timeout `0x102` becomes `STATUS_IO_TIMEOUT (0xC00000B5)` at `0x1728C–17294`.

Cleanup `0x137C4` handles/acknowledges interrupts (`0x1260E`, `0x126EE`), writes BAR1 `MTTCTL +0x080 = 0`, sets BAR0 `ERRM +0x008 = 0xFFFFFFFF`, clears the software interrupt mask and synchronously sets BAR0 `INTEN +0x084 = 0`. Quiesce `0x138D4` saves/masks interrupts but does not visibly abort DMA. The MAM postlude `0x12D6A` reads `IIMST +0x04C`, writes `IIMCL=0` if bit 0 is set, and reads ERRS; it does **not** poll until a proven idle state.

**No original abort/idle protocol was proved.** In particular, `MTTCTL=0`, `IIMCL=0`, `IIMST` bit 0, an IRQ, or masking INTEN must not be treated as proof of completed bus transactions. A logged timeout is not a hardware reset. Physical DMA completion's memory-ordering guarantee also remains unknown.

Original `CFDC2400 → 0x12EDE → 0x12EC2 → 0x11390` can inject software pending IRQ bit 0 into the DPC and signal an acquisition completion event without physical INTST bit 0. Therefore an event signal alone is not evidence of hardware completion.

## Original x86 DMA abort and physical bus-idle: proof boundary

The focused [x86 DMA abort / PCI bus-idle audit](legacy-dma-abort-bus-idle-audit.md)
compares original Ghidra acquisition, ISR/DPC, MDL release, PnP
and power paths against x64 code at `87ee1638`. Its central
result is **no confirmed hardware abort/PCI-bus-idle
contract**. The original timeout path only disables
`INTEN` bit 0; `IIMCL=0`, `MTTCTL=0`,
`IIMST` bit 0 and completion IRQ do **not**
prove that posted or outstanding host-memory accesses
are drained. Original unregister can subsequently
free descriptor MDLs and source pages without
such proof.

On x64, timeout/failed-event quarantine retains
suspect resources, including an FDO-independent
ownership path, but even an accepted normal
completion IRQ is currently **not** a hardware
bus-idle certificate. Do not enable WDM v3
live mappings or call release DDIs based on
ISR/DPC rundown, a timeout or an assumed
end-of-transfer bit. The legacy active PFN
descriptor builder is not IOMMU-safe. PnP
software quiescence is likewise not DMA
hardware quiescence.

Full evidence, per-register access table,
confidence levels, x86/x64 differences,
and safe future verification requirements
are kept in the focused audit.

## Defensive x64 behavior staged on the draft branch

This is **partial damage containment**, not DMA engine stop/recovery:

1. The existing DMA start paths (`LecExecuteLegacyMttTransferLocked` and `LecIoctlAcquireBufferedOneChannel`) reject a latched `DmaUnknownActive` device.
2. Each serialized launch now has a nonzero 64-bit software generation and an
   explicit completion-evidence state. Before enabling completion IRQ bit 0,
   the selected generation enters a transient `Arming` state. The existing
   `IIMCL=1` and MAMRGO/MTTRGO writes, followed by publication of
   `DeviceActive`, run inside one `KeSynchronizeExecution` callback. The ISR
   therefore cannot run between GO and active publication. An ISR that runs
   earlier while `Arming` rejects the completion, changes ownership to
   `UnknownActive` and prevents GO. The physical ISR can otherwise change only
   the current `DeviceActive` generation to `CompletionObserved`; the DPC
   signals the event only when its recorded IRQ generation, the selected
   transfer and the current generation still match. A completion spin lock
   keeps the selected transfer resident through `KeSetEvent` while launch
   cleanup deselects it. Wrong-generation consumers use compare-and-swap and
   cannot erase a valid current IRQ marker.
3. On a launched request with timeout/failed wait or a supposedly successful
   event without matching physical-IRQ evidence, transition to
   `UnknownActive`, mark its transfer `DmaUnsafeToFree`, latch
   `DmaUnknownActive` and disable new IOCTL admission. An IRQ observed just
   before a software timeout still becomes `UnknownActive`; it is not allowed
   to rescue the timed-out request. A software-event-only success becomes
   `STATUS_IO_DEVICE_ERROR`; ordinary timeout remains `STATUS_IO_TIMEOUT`.
4. `LecFreeTransfer` does not release poisoned descriptor pool storage, descriptor MDLs, or user MDLs. `LecUnregisterTransfer` and process cleanup cannot free them; STOP/REMOVE detach them without freeing. **This deliberately leaks pinned pages and kernel allocations until system restart** when the device's inactivity cannot be proved, to avoid returning possibly DMA-owned memory to the OS. A driver unload or new attachment is **not** proven safe recovery.
5. `CFDC2400` continues to accept the known zero-mask diagnostic but rejects software completion bit 0. Source and debug ABI contracts must not interpret this as full legacy opcode parity.
6. STOP/REMOVE still unmap BARs and detach the FDO after quiesce. Retaining DMA memory is necessary but **insufficient** to prove a fully safe PnP lifecycle (PCI transaction completion and hardware reset remain unverified).

The defensive active-path patch has not been executed under Driver Verifier,
in a DMA fault-injection simulation or on real PCI hardware. The current Dry
result covers source contracts and inactive staging only; do not treat it as
runtime evidence for the active path.

### Live completion evidence states

The live legacy path uses the following explicit software states. They record
ownership evidence, not undocumented FPGA behavior:

| State | Entry | Permitted consequence |
| --- | --- | --- |
| `NeverLaunched` | Fresh generation prepared before GO, cancelled arming, or no-launch staging | A real WDM mapping may be released only if no device launch occurred. |
| `Arming` | Selected generation is published before completion IRQ bit 0 is enabled | Transient, non-releasable launch gate. A physical IRQ observed here is not attributed to the new transfer and forces `UnknownActive`; failure before GO may cancel back to `NeverLaunched` only if no IRQ changed the state. |
| `DeviceActive` | The synchronized launch callback has written `IIMCL=1` and MAMRGO/MTTRGO, then publishes the generation before releasing ISR exclusion | Transfer resources and mappings remain owned; no release is permitted. |
| `CompletionObserved` | Physical ISR accepts INTST bit 0 for the current active generation | Matching DPC may signal that transfer's event. This is **not** `IdleProved` and cannot release a WDM mapping. |
| `IdleProved` | Reserved for a future independent, documented board/platform idle predicate | Mapping release may be considered. No live call site performs this transition. |
| `UnknownActive` | Timeout, failed wait, successful event without matching IRQ, STOP/REMOVE uncertainty or generation inconsistency after launch | Terminal FDO fault; close admission and retain DMA-owned memory. |
| `Quarantined` | Unknown-active transfer reaches cleanup and its MDLs/descriptors are retained | Ownership survives FDO teardown through the publication anchor; no recovery or release. |

`CompletionObserved -> NeverLaunched` is permitted only when the existing
legacy compatibility path prepares a later serialized acquisition. Software
generation matching prevents a queued old DPC or recorded old IRQ from
signaling a different transfer, and repeated completion cannot create
`IdleProved`. Conditional consumption also leaves a matching marker intact
when a stale caller presents the wrong generation, while concurrent matching
consumers have only one winner.

The synchronized callback removes the source-established interleaving in
which software previously published `DeviceActive` before writing GO. It does
not identify the physical origin of an untagged board IRQ. In particular, a
source that was physically pending before GO but was not delivered to the ISR
until after the synchronized callback is indistinguishable from a genuine
fast completion, and an IRQ from an earlier transfer asserted after a later
generation reaches `DeviceActive` has no hardware generation tag. These remain
unproven live-path premises. The correction adds no register access and keeps
the existing IIMCL-then-GO order and values; it changes only the software
state and ISR exclusion around those writes.

The successful legacy PFN path intentionally remains behavior-compatible:
`CompletionObserved` satisfies its historical synchronous wait and ordinary
legacy MDLs/descriptors may later be freed. This is an explicit compatibility
policy based on an **unverified IRQ-to-final-memory-access ordering
assumption**, not a safety proof and not permission to release future WDM/IOMMU
mappings. Strict enforcement would require disabling successful acquisition
or quarantining every normally completed transfer indefinitely; that design
decision is reserved for the owner after hardware evidence is available.

An already-admitted MAM IOCTL can wait behind another request on
`TransferMutex`. It now rechecks `DmaUnknownActive` after acquiring that mutex
and before any MAM setup MMIO, so a prior timeout cannot be followed by a
second launch from the queued request. STOP/SURPRISE/REMOVE still close the
outer admission gate and wait for already-admitted synchronous IOCTLs before
resource teardown.

## Windows DMA-adapter migration design (not activated)

Current descriptor builder: `driver/Acquisition.c:LecBuildDescriptorTable`.
The board consumes an 8-byte descriptor (`CountDwords` and a **32-bit device
address**) and a page-chained descriptor table. Each 4 KiB descriptor page has
512 slots; slot 511 chains to the **next device-visible table page** with a
zero count. Both data slots and chain slots currently derive addresses
directly from `MmGetMdlPfnArray`, assuming CPU PFN addresses are usable on
the PCI bus. Checking `<= 0xFFFFFFFF` does not establish this.

Migration implementation sequence, contingent on driver/DMA model review:

1. At device setup, establish a `DEVICE_DESCRIPTION` that matches the
   WR6k's 32-bit bus-master addressing and maximum fragment/transfer limits.
   Use `IoGetDmaAdapter`; release its reference only after no DMA mappings,
   buffers or asynchronous callbacks remain.
2. Keep each requested transfer's MDL lifetime separate from its DMA mapping
   lifetime. Use DMA-operations scatter/gather mapping (for example,
   `GetScatterGatherList`/`PutScatterGatherList`) and populate the board
   descriptors from the returned **logical/device** addresses and lengths,
   not from PFNs. Handle callbacks, alignment, fragmentation, allocation
   failure, buffer offsets and the board's dword unit restrictions. Preserve
   the x86-compatible command and opaque-token ABI.
3. Allocate a suitable DMA-visible common buffer (or another fully verified
   mapping) for hardware-read descriptor pages. The current `0x33000` byte
   nonpaged-pool buffer and its PFN-linked pages are not guaranteed to be
   device contiguous or mapped for DMA. Prove each chain link and initial
   SGTA device address, maintain device-address bounds and prevent table
   overflow. Avoid assuming a common buffer is inherently cache coherent
   without the platform's DMA API contract.
4. Map once per valid DMA ownership period, and unmap only after physical
   DMA inactivity is established. An unknown-active transfer must **retain
   DMA mapping objects and descriptor pages** along with locked MDLs;
   `PutScatterGatherList` is not a substitute for abort. The existing
   emergency pinned-page quarantine is not sufficient once an IOMMU mapping
   is in use. Plan owner state that cannot disappear at FDO REMOVE.
5. Software-only verification should use an injectable/mockable mapping
   boundary: discontiguous and above-4-GiB CPU PFNs mapped to legal
   below-4-GiB device addresses, segment splits, table-page chaining,
   capacity errors, mapping callbacks racing with STOP, and unknown-active
   quarantine without unmapping. Runtime behavior under IOMMU and the
   physical DMA-idle guarantee still require recoverable hardware.

References: Microsoft Learn,
[Using Scatter/Gather DMA](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/using-scatter-gather-dma),
[GetScatterGatherList](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list),
[Map Registers](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/map-registers).
This plan is **not active**. An isolated DMA adapter/common-buffer and
SG bridge have been staged but not connected to acquisitions. No PCI
DMA/IOMMU compatibility is claimed.

## Hardware-independent encoder staging

A pure `LecDmaEncodeMappedSegments` function is now provided in
`driver/DmaLayout.c` and `driver/DmaLayout.h`. The driver project
compiles the new source, but does **not** call it from the live
acquisition paths: existing `LecBuildDescriptorTable` still derives
PFNs. This separation permits validating the descriptor-ABI encoder
before committing to a DMA adapter lifetime redesign.

Inputs are **already mapped device-logical** scatter/gather segments,
not CPU PFNs. A contiguous, 4-KiB-aligned common-buffer logical base
is required for the encoded descriptor table; 512 descriptor slots
occupy each 4-KiB page, slot 511 is used as a link to the next page
when needed (including before the end marker), and the terminator
has zero count and zero address. Entries are restricted to 4-byte
alignment, 32-bit device addresses and 4-KiB-bounded fragments.
The encoder rejects wrong totals, incomplete tables, invalid addresses
and boundary crossings rather than truncating pointers.

A standalone MSVC test binary (`tests/dry/test-dma-layout.c`) is now
part of the Dry regression runner. It exercises synthetic mapped
device-address segments only; **it proves neither actual adapter
mapping nor physical bus-idle**. The encoder passed the owner's 2026-10-04 Windows Dry run
(13/13 native layout tests); this is not an actual DMA mapping test. Next implementation milestone: acquire and retain the
appropriate `DMA_ADAPTER`, obtain mappings for user buffer MDLs
and common-buffer descriptor pages, and replace the old PFN table
builder only when resource ownership and abort/idle guarantees
are satisfactorily modeled.

## WDM adapter/common-buffer staging (not yet activated)

A separate `driver/DmaAdapterStage.c/.h` module now compiles as part
of the draft driver. It provides:
- `LecDmaCreateAdapterContext`: `IoGetDmaAdapter` using a
  `DEVICE_DESCRIPTION_VERSION3` PCI bus-master, 32-bit DMA-address
  constraint, scatter/gather support and maximum transfer size. The
  returned map-register count is retained, **not assumed sufficient**.
  The context validates `DMA_OPERATIONS.Size`, owns a PDO reference and
  permits only one synchronous owner claim.
- `LecDmaAllocateCommonTable`: requests a device-visible
  `AllocateCommonBuffer`, validates page alignment and entire 32-bit
  logical-address range, and zero-initializes the descriptor table. A
  pending allocation blocks owner claim and adapter teardown.
- `LecDmaReleaseAdapterContext`: frees common buffer and adapter
  only with an explicit, externally established `ProvenIdle=TRUE`
  after owner rundown and with no previous quarantine. Otherwise the
  independent context and device mapping remain retained or quarantined;
  this is a leak, **not recovery**.

No PnP or acquisition call site uses these functions yet. The separate
inactive SG stage can request a mapping for one MDL, but adapter/MDL/PNP
lifetime is not yet unified and no live mapping exists. The existing
PFN-based active path remains unmodified. Review callback IRQL, limited map registers, owner lifetimes,
STOP/REMOVE and reset/idle requirements before wiring this into DMA.
The adapter stage compiles in the current Windows WDK Dry run and is
exercised only through fake host DDIs. It has not been exercised by an
actual kernel DMA request.

Windows references: [IoGetDmaAdapter](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-iogetdmaadapter),
[AllocateCommonBuffer](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pallocate_common_buffer),
[GetScatterGatherList](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list).

## Staged asynchronous mapping ownership checks

`DmaMappingOwner.c/.h` now models `EMPTY`, `AWAITING_CALLBACK`,
`READY`, `DEVICE_ACTIVE`, `UNKNOWN_ACTIVE`, and `RETURNED`.
This is a **pure software** transition model, not a DMA mapping API.
It refuses returning a mapping before its callback arrives, while
the device could still access memory, and after an unknown-active
timeout or surprise removal. A true mapping request failure may reset
a pending owner **only if the DDI guarantees no late callback**.
A successful software or physical IRQ alone does **not** enter the
externally proven-idle transition. Terminal uncertainty cannot be
recovered by a late callback.

The separate Windows-native test runner
`tests/dry/test-dma-ownership.ps1` checks delayed callbacks, failed
requests, launches, proven-idle prerequisites and permanent quarantine.
Its methods are deliberately **not connected** to the active driver
or to the staged WDM `GetScatterGatherList` implementation. The
callback ownership and serialization protocol (including IRQL and
device/remove lifetime references) must be reviewed before actual
WDM integration. Owner's Windows WDK/Dry run verified this inactive model on
2026-10-04 (17/17 native ownership tests), not concurrent DMA/DDI behavior.

## Inactive WDM scatter/gather callback bridge (WDK compiled; runtime unverified)

The newly staged `DmaScatterGatherStage.c/.h` wraps the WDM
`GetScatterGatherList` / `PutScatterGatherList` functions without
changing `LecBuildDescriptorTable` or invoking hardware. It accepts
one already locked MDL, records the device-logical SG list in a
`DRIVER_LIST_CONTROL` callback, holds an object reference while a
callback can be outstanding, and uses a spin lock to synchronize
its mapping-ownership state. The callback itself starts **no DMA**.
The WDM SG DDIs are invoked at DISPATCH_LEVEL. The temporary stage
is *not* called by PnP, IOCTL or real acquisition code.

The state model requires callback completion and an explicit,
separately proven DMA-idle predicate before `PutScatterGatherList`.
A failed submission or unconfirmed idle conservatively retains the
stage and mappings. **A late callback or a signal from the DMA IRQ
is not proof of bus inactivity.**

Before enabling this code, the following blockers must be resolved:
- Model device-object, adapter, source-MDL and IRP/remove-lock ownership
  together (referencing an FDO alone does not retain every PnP resource).
- Serialize all peeks and releases across worker/DPC and PnP actions;
  borrowed SG pointers cannot survive asynchronous destruction.
- Confirm precise WDM callback and failure/cancellation contracts,
  including resources queued before STOP and callbacks after remove.
- Handle chained MDLs, map-register shortage, scatter/gather length
  limits, descriptor-buffer capacity and the unknown-active quarantine
  without releasing any DMA/IOMMU mapping prematurely.
- Observe that `PutScatterGatherList` flushes/unmaps resources, so
  it may only be used after real hardware DMA is demonstrably finished.

The source stage was compiled and linked by the Windows WDK on
2026-10-04; the full Dry runner reported 24/24 source contracts,
13/13 layout tests and 17/17 mapping-ownership tests.
**No actual WDM SG callback was exercised in the verified driver
run.** The focused read-only WDM SG review subsequently identified
concrete P0 issues: `LecSgStagePeek` returned an unprotected SG pointer;
`CallbackComplete` could precede callback retirement; FDO-object
referencing did not establish a joint adapter, locked-MDL, mapping,
callback and PnP remove lifetime. Failures also changed shared state
outside the spin lock. Multi-MDL (>32 MiB) transfers and map-register
limits remain unsupported.

The draft now removes the raw-pointer `Peek` interface in favor of
`LecSgStageCopySegments`, which copies bounded SG elements while
holding the lock and checks that the complete returned device-logical
SG length matches the requested MDL span, addresses fit the board's
32-bit DMA limit and lengths are DWORD aligned. Invalid mappings are
quarantined rather than being truncated. Both synchronous and delayed
callbacks, submitted
status and submission failure transition under the same spin lock.
`LecSgStageRelease` marks closing and permanently quarantines unknown
DMA. **It also refuses ALL DMA launches and ALL releases**, even `ProvenIdle=TRUE`:
callback publication is not proof of callback retirement and there is
no verified joint MDL/adapter/FDO rundown. Therefore it never calls
`PutScatterGatherList` or deallocates a stage or device-object
reference. This conservative leak is an intentional development
interlock, **not finished resource ownership**.

An additional host-only fake WDM shim builds and executes the actual
bridge code (`tests/dry/test-sg-stage.c`), covering inline and delayed
callbacks, bounded copying, duplicated callbacks, failed submission
followed by late notification, STOP/REMOVE with outstanding callbacks,
and concurrent callback/REMOVE. It does not invoke real kernel WDM operations, model the PCI bus or
prove a physical DMA stop. The owner confirmed the full Windows x64 Dry
run on 2026-10-04 at 22:40:42: driver/lecdiag builds succeeded
(0 errors, 0 warnings in an incremental build), source contracts
24/24, descriptor layout 13/13, ownership model 17/17 and fake WDM SG
callback bridge **27/27 PASS**. All 81 checks passed. This is
software-only regression evidence; it neither exercises actual WDM
resource allocation nor proves safe DMA bus idle or REMOVE rundown.

**Activation blockers remain**: a reference-counted owner for the
adapter, pinned MDL(s), descriptor common buffer, callback retirement,
all readers, device teardown and DMA mapping, an explicit certified
successful no-launch cleanup path, multi-MDL sizing and map-register
budget, and independent hardware bus-idle proof. Pending or unknown
mappings cannot be returned. Until resolved, the staged bridge MUST
remain unreachable from active acquisition and PnP paths.

## Experimental synchronous DMA v3 path: safe no-launch cleanup only

The Microsoft WDM v3 `GetScatterGatherListEx` contract explicitly allows
`DMA_SYNCHRONOUS_CALLBACK` with a NULL execution callback. Resource
shortage then returns `STATUS_INSUFFICIENT_RESOURCES` without queuing
an asynchronous mapping callback. On a successful allocation, the
caller owns the mapping until `FreeAdapterObject(DeallocateObject)`.
Unlike the earlier asynchronous `GetScatterGatherList` bridge,
this path can distinguish **allocation without any device DMA launch**
from indeterminate device activity.

The **inactive** `DmaSyncStage.c/.h` implementation now follows the
installed WDK 10.0.28000.0 declarations and the Microsoft v3 contract:
- Requires DMA adapter **v3** and its transfer-context initialization,
  synchronous GetEx and FreeAdapterObject operations; no automatic v2
  fallback. `DmaAdapterStage` requests `DEVICE_DESCRIPTION_VERSION3`,
  sets the v3-only `DmaAddressWidth` to the board's 32-bit limit,
  checks `DMA_OPERATIONS.Size` through `FreeAdapterObject`, requires all
  operations used by the stage and rejects a zero map-register budget.
- Has **no device launch function**, no MMIO and no integration with
  the active `Acquisition.c`, PnP or IOCTL paths.
- Uses a unique, aligned `DMA_TRANSFER_CONTEXT_SIZE_V1` context per
  request and a locked MDL chain (including more than one 32-MiB MDL).
- Validates requested byte extent and copied SG elements for full
  coverage, nonzero dword-aligned lengths, 32-bit logical addresses,
  the adapter's reported map-register budget and exact WR6k descriptor
  slot consumption, including 4-KiB splitting, page-chain slots and the
  final zero descriptor. Cyclic or unlocked MDL chains are rejected.
- Adds a nonpaged parent `LECS65_SG_SYNC_OWNER` with an explicit spinlocked
  `Constructed -> Initializing -> Active -> Stopping/Quarantined ->
  Destroying -> Destroyed` lifecycle and `Outstanding` plus
  `ReleasesInFlight` counts. One-time construction
  initializes the lock without inspecting prior storage. Init never clears or
  reinitializes a published owner; repeated or concurrent init returns BUSY
  without changing its mapping list, token sequence, adapter pointer or count.
  The caller-owned storage must remain resident until all API callers have
  completed, including after Destroy returns. A submission increments
  before its WDM call, and cleanup decrements only after its no-launch
  allocation is released. A central adapter-context claim prevents two
  independent owners from bypassing the **one outstanding mapping per
  adapter context** rule. This is deliberately not described as device-wide:
  two separately created contexts for the same PDO/adapter are not serialized.
  The inactive PnP parent described below now supplies a single-context
  creation boundary. Live PnP must still publish exactly one such parent per
  physical device before this stage can be activated.
  Successful mappings are referenced by monotonic, nonreused tokens:
  copies and token removal use the same lock, and physical adapter
  resources are freed outside it. No raw stage pointer escapes. STOP
  closes admission and returns BUSY while a request is in flight. A
  request that loses the STOP race releases its never-launched mapping
  and returns `STATUS_DELETE_PENDING`; published no-launch mappings can
  be drained explicitly before the owner relinquishes the adapter claim.
  Unknown-active notification instead sets a permanent atomic latch. A
  successful GetEx allocation completing after that latch is retained and
  published only to the internal quarantine list; it is never returned as a
  usable token or passed to `FreeAdapterObject`.
- In the specific no-launch case calls `FreeAdapterObject`, drops
  the per-mapping device-object reference, invalidates the mapping token
  and deallocates stage memory. A successful GetEx result with a malformed
  or NULL list is also explicitly freed instead of being orphaned. Adapter
  teardown later uses `PutDmaAdapter`; these are distinct ownership steps.
  The adapter context holds its own PDO reference and refuses teardown
  while an owner or common-buffer allocation is outstanding. A duplicate,
  stale or concurrent second release is rejected. The stage does not
  implement any release for hardware-started or unknown-active DMA.
- Owner claim requires a present, page-aligned descriptor common buffer of at
  least one page. Its capacity is derived only from `TableLength / 8`; caller
  input can no longer inflate the capacity independently of the allocation.
  Missing, undersized, non-page-sized, misaligned or 32-bit-range-inconsistent
  backing is rejected. The same 512-slot/page constants as the encoder govern
  slot 511 chaining and the final zero descriptor.

The expanded `tests/dry/test-sg-sync.c` compiles the actual
`DmaSyncStage.c`, `DmaAdapterStage.c` and `DmaPnpStage.c` sources against
fake v3 DDIs.
It covers allocation and initialization failure, resource shortage,
malformed successful GetEx results, repeated/concurrent owner init,
concurrent init/destroy API rejection, sequential and concurrent duplicate
release, copy/release and map/release races, STOP during GetEx, explicit
PnP-style no-launch drain, context-scoped adapter ownership, multiple contexts
for the same fake adapter, 48-MiB MDL chains, cyclic/unlocked MDLs,
map-register limits, missing/inconsistent tables and exact descriptor
page-chain capacity. Parent tests additionally block GetEx,
`FreeAdapterObject` and failed-START common-buffer allocation while a real
second thread requests quarantine; they cover Begin/Finish races, duplicate
notification, post-STOP notification and exact retained ownership. The current
full Dry result is recorded in
`docs/regression-testing.md` and `docs/status.md`. These synthetic tests
do not exercise actual OS DMA resource allocation, real PnP REMOVE or
hardware bus-master idle.

## Live-published inactive PnP/DMA parent ownership

`DmaPnpStage.c/.h` supplies the software parent for the staged adapter and
synchronous owner. `DmaPnpPublication.c/.h` now publishes exactly one
separately allocated lifetime anchor from AddDevice and mirrors live
STOP/SURPRISE_REMOVE/REMOVE into that parent. Publication does not call
`LecDmaPnpStartNoLaunch`, acquire a DMA adapter, map memory or change the active
PFN-derived acquisition path. Source contracts prohibit all staged adapter and
mapping entry points from the live driver sources.

The ownership contract is:

| Resource | Current owner | Valid/release boundary |
| --- | --- | --- |
| Dispatch/remove reference | `IO_REMOVE_LOCK` in the live device extension | Acquired before dispatch work and released at local or lower-stack completion; REMOVE waits for all references. |
| Live IOCTL admission | `AcceptIoctls`, `ActiveIoctls` and `IoIdleEvent` | STOP/SURPRISE/REMOVE close admission and drain synchronous requests before deferred work or transfer cleanup. |
| BAR mappings | Live device extension | Published only after START resource mapping; unmapped after IOCTL drain and IRQ/DPC/timer quiescence. Surprise removal performs no MMIO. |
| IRQ and queued DPC | Live device extension | Interrupt disconnect precedes DPC removal/flush; neither step proves physical DMA idle. |
| Embedded timer | Live device extension | Used only as a waited timer object; canceled after the DPC rundown and before transfer/BAR release. |
| Live publication and PDO reference | One separately allocated `LECS65_DMA_PNP_PUBLICATION` per FDO/PDO | Created transactionally during AddDevice. Clean REMOVE unpublishes it after remove-lock rundown, drains software users, destroys the empty parent, dereferences the PDO and frees the wrapper. |
| Active PFN transfer MDLs and descriptors | `LECS65_TRANSFER` | Released only when `DmaUnsafeToFree` is false. An unknown-active transfer is detached from the FDO list and linked to the independently resident publication before FDO deletion. The wrapper, parent, PDO reference, transfer, pinned MDLs and descriptors are then intentionally retained until restart; none is declared idle. |
| Staged adapter, PDO reference and descriptor common buffer | One `LECS65_DMA_PNP_STAGE` through its single `LECS65_DMA_ADAPTER_CONTEXT` | Created transactionally on inactive START; retained until the child owner is stopped, all parent calls retire, every no-launch mapping drains and software quiescence ordering is recorded. Live START does not invoke this staged START. |
| Staged SG allocation and transfer context | `LECS65_SG_SYNC_OWNER`/token | One allocation per adapter context; released by `FreeAdapterObject(DeallocateObject)` only because this interface has no launch operation. |
| Staged locked MDL chain | External future request owner; borrowed by the SG stage | Must remain pinned through mapping release and parent-call rundown. The parent does not unlock borrowed MDLs. |

The live-source audit confirms that valid STOP and surprise-removal paths close
admission and drain IOCTLs before disconnecting the interrupt, flushing queued
DPCs, canceling the timer, releasing ordinary transfers and unmapping BARs.
REMOVE additionally waits on the remove lock before deleting the FDO. No
concrete double release or DPC-after-transfer-free path was found in that
ordering. This is source evidence only: lower-stack completion timing, power
IRPs, repeated or malformed PnP sequences and Driver Verifier behavior have
not been exercised.

Parent storage comes only from `LecDmaPnpStageCreate`. Its private construction
step initializes fresh, uniquely owned nonpaged storage; there is no public
reconstruct-in-place API that can clear a live lock, token, counter or
quarantine latch. `LecDmaPnpStageDestroy` accepts only an empty `Stopped` or
`Removed` stage. The live wrapper supplies the required publication and
external rundown: callers first hold the FDO remove lock, wrapper acquisition
adds an internal user reference, and REMOVE prevents new acquisitions before
waiting for existing users. Clean parent destruction happens only after both
rundown domains have retired.

The parent implements `Stopped -> Starting -> Started` and explicit STOP,
surprise-removal and REMOVE states. START publishes admission only after the
adapter, descriptor table and child owner all succeed. Failed partial START
releases only never-launched allocations and returns to `Stopped`. Teardown
atomically closes parent admission, requests child STOP without holding the
parent spin lock, rejects new mappings, waits by retry rather than blocking
under a lock, and requires interrupt disconnect, DPC drain and timer stop in
that order. Those flags describe software rundown only and are deliberately
not a DMA-idle flag.

The parent keeps a call reference across map, copy and release. Consequently
STOP cannot destroy the child while `GetScatterGatherListEx`, a copy or
`FreeAdapterObject` is executing. A cleanup attempt that loses this race
returns `STATUS_DEVICE_BUSY`; after callers retire, the same state can be
retried. Adapter cleanup also records when the SG owner has already detached,
so a later adapter-release failure does not cause a second owner destruction.
Tokens remain monotonic across START generations, so stale tokens cannot name
a mapping from a later generation.

One parent holds at most one adapter context. This corrects the confirmed
staging architecture gap in which two contexts for the same fake PDO could
independently claim owners. Live AddDevice creates one parent and wrapper per
FDO/PDO and holds its own PDO object reference. Direct adapter-context creation
remains unreachable from live sources. The wrapper is independent of the FDO
so an unknown-active quarantine can survive FDO deletion.

The active timeout paths use one helper that first poisons the transfer, then
latches device-wide uncertainty and closes IOCTL admission before notifying
the parent. During teardown, a poisoned transfer is removed from the FDO list
and its list link is rehomed into the wrapper's retained-transfer list. The
transfer uses an atomic `FDO -> TRANSFERRING -> PUBLICATION` ownership state,
so duplicate cleanup cannot insert the same link twice. Publication commits
the list ownership before the fallible parent notification; a notification
failure therefore retains the MDLs and descriptors rather than orphaning or
freeing them. REMOVE
unpublishes the device-extension pointer after `IoReleaseRemoveLockAndWait`.
If the wrapper observes a retained transfer or parent quarantine, it returns
`STATUS_DEVICE_BUSY` to its cleanup caller and intentionally retains all of
its ownership rather than blocking device removal or freeing unknown-active
memory. This status is diagnostic; it does not fail or delay the already
forwarded PnP REMOVE IRP.

`LecDmaPnpQuarantineUnknownActive` first uses an interlocked operation to set
an authoritative permanent latch, before waiting for either parent or child
spin lock. Every admission and cleanup commit checks this latch. The parent
then closes admission and marks the sync owner and adapter context
quarantined outside the parent lock. A blocked successful GetEx completion is
retained with its stage, PDO reference and borrowed pinned MDL chain. A failed
START publishes partial adapter ownership before common-buffer allocation, so
a concurrent notification can retain it rather than losing it to rollback.

Mapping release has an explicit irreversible commit: the stage is removed and
`ReleasesInFlight` is incremented under the child lock before
`FreeAdapterObject` runs without a spin lock. A notification that wins before
that commit prevents the release. A notification observed after the commit
cannot undo the WDM call; it records `LateQuarantine` and retains every
remaining adapter/common-buffer/owner resource. The same phase marker prevents
the notification path from dereferencing an adapter context while its final
release is already committed. This late outcome is an unresolved integration
condition, not proof that releasing an active mapping is safe. It is tolerated
only in this inactive API because no operation can launch DMA. Future live
integration must exclude unknown-active notification before any release commit
or independently prove hardware idle.

Duplicate notifications are idempotent. Teardown attempts after the latch,
including STOP/REMOVE cleanup, remain blocked. This is retention, not recovery,
and may intentionally leave the parent, adapter, descriptor common buffer,
mapping, PDO references and external MDL ownership outstanding.

### Confirmed software behavior and unresolved activation blockers

The parent lifetime is wired into live STOP/SURPRISE_REMOVE/REMOVE, but its
adapter and child owners remain staging-only. Live START deliberately leaves
the parent in `Stopped`; no existing DMA transfer is converted to the staged
mapping path. In particular, a physical
hardware-start/IRQ/timeout path still lacks a proven bus-master idle
transition and must **never** call the no-launch release method.
The old async SG stage remains fail-closed and does not launch or
free mappings. Real STOP/REMOVE must eventually reconcile pinned MDL
chain ownership, adapters, descriptors, DMA completion, loss of device,
and remove locks, without waiting on permanently quarantined mappings.
The staged sync owner still borrows rather than unlocks its MDLs; only the
legacy unknown-active transfer path is currently joined to the independent
retention anchor. No adapter mapping may become live until every staged MDL
and request is also joined to remove-lock/publication rundown. The
software-only phase/latch protocol does not by itself close the physical
notification-versus-release window for active DMA.

The live tests confirm allocation rollback, one wrapper/parent/PDO reference,
unpublish-before-internal-rundown, clean STOP/surprise/remove transitions,
stale and duplicate release rejection, and FDO-independent retention of a
synthetic poisoned transfer. They also fault parent notification after the
ownership commit, race that notification with REMOVE, reject duplicate
publication/retention and verify that unpublish rejection leaves ownership
with the caller. The production IRP forwarding helpers are separately run
against synchronous, pending, error and power fake-lower-stack completions;
the tests verify exact remove-lock release and the held-IRP contract used by
START/REMOVE. They do not execute real kernel PnP concurrency, HAL/IOMMU
mappings, Driver Verifier, power transitions or physical DMA.

The production completion tracker is additionally compiled into a native
host test. Deterministic cases cover IRQ immediately before timeout, event
without physical IRQ, pre-GO completion, the fail-closed modeled interval
between GO and active publication, genuine fast completion after synchronized
publication, early and repeated completion, stale/wrong generation,
wrong-generation marker preservation, concurrent consumers, completion versus
STOP/REMOVE uncertainty, quarantine versus attempted release and the exact
`NeverLaunched`/`CompletionObserved`/`IdleProved` mapping-release boundary.
These tests establish software attribution and retention only; they cannot tag
a real board IRQ or prove PCI bus-idle.

The publication factory now treats its output slot as an in/out ownership
slot and rejects a non-NULL existing publication rather than overwriting and
leaking it. This enforces one publication in the live device extension. It is
not a global PDO registry; the remaining device-wide uniqueness premise is
the WDM AddDevice contract for one FDO attachment per device stack.

Sources:
[GetScatterGatherListEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list_ex),
[InitializeDmaTransferContext](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pinitialize_dma_transfer_context),
[FreeAdapterObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pfree_adapter_object).

Confirmed DDI boundary: `InitializeDmaTransferContext` initializes an
opaque caller buffer and requires a unique context for each adapter
allocation request. With `DMA_SYNCHRONOUS_CALLBACK` and NULL execution
callback, GetEx either returns the mapping synchronously or fails without
queuing a callback; successful resources require explicit
`FreeAdapterObject(DeallocateObject)`. `FreeAdapterObject` releases the
channel/map-register allocation, while `PutDmaAdapter` releases the
longer-lived adapter returned by `IoGetDmaAdapter`. Software tests still
cannot prove how a real DMA-remapping implementation behaves under device
removal, nor can they turn IRQ/timeout state into physical bus-idle proof.
The documentation and WDK headers do not establish that separately acquired
adapter contexts for one PDO share a serialization domain usable by this
driver; only the per-context guarantee is currently confirmed in source.
Microsoft describes `FreeAdapterObject(DeallocateObject)` as the explicit
release after successful synchronous GetEx allocation, but does not separately
spell out the abandon-before-launch case used by this quarantine stage. The
software implementation releases that never-launched allocation through the
same required primitive; actual HAL/IOMMU behavior, chained-MDL handling and
teardown ordering remain unverified until controlled kernel tests are possible.

## Remaining P0 work, no shortcuts

- Obtain real board documentation or a verified, recoverable bench measurement of MAM and MTT abort/idle, physical completion ordering and safe reset/readback. Do not invent MMIO commands from guesses or treat the old x86 driver's behavior as a proof of hardware safety.
- Validate the remaining untagged physical-IRQ attribution premise, completion ordering and DPC/STOP/REMOVE behavior under Driver Verifier and a recoverable fault-injection target. The deterministic host tests cover software generations and pointer rundown, not the board's IRQ origin or final PCI transaction.
- Replace direct PFN-derived bus addresses (`MmGetMdlPfnArray` and PFN shift) with an appropriate Windows DMA adapter/mapping model for supported DMA-remapping/IOMMU configurations. Retain the hardware 32-bit address and descriptor format limits; DMA mappings must not be released before hardware inactivity is proven.
- Replace the emergency pinned-memory quarantine with verified abort/drain/recovery ownership, including what happens across driver remove/reinstallation. It is not a viable final production resource-management model.

No artificial nonzero IRQ injection, licensed Dallas writes, hazardous DMA timeouts or unverified register writes should be performed on the only working WR6k.
