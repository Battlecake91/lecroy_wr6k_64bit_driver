# WR6k DMA completion, timeout and memory lifetime

Status: **source reconstruction and defensive source patch only**, 2026-10-04.
Baseline analyzed: `fix/p0-irq-start-dma-gating` at `79ef70626e2649d5e01ecbd6f32afecb4293716d`.
Analysis: original x86 disassembly compared against the native x64 acquisition and cleanup paths. **Neither an original-hardware idle guarantee nor a successful x64 hardware test is established here.**

## Established original x86 control flow

Both original acquisition variants converge through `0x17478 → 0x171DE`:

- MAM/`CFDC2138`: `0x13C84 → 0x12D6A → 0x17478 → 0x171DE`.
- MTT/family-1 `CFDC2110`: `0x160DC → 0x17478 → 0x171DE`.

Original `0x171DE` writes BAR0 `SGTA +0x040` at `0x171EA–171ED`, `IIMTC +0x044` at `0x17204–1720A`, resets completion event at `0x1721B–17222`, enables INTEN bit 0 at `0x17228–1722C`, sets `IIMCL +0x048 = 1` at `0x1722F–17235`, launches MAMRGO (BAR1 `+0x064`) or MTTRGO (BAR1 `+0x084`) at `0x1723A–1724F`, waits up to five seconds at `0x17254–17279` and disables INTEN bit 0 at `0x17281–17285`. Timeout `0x102` becomes `STATUS_IO_TIMEOUT (0xC00000B5)` at `0x1728C–17294`.

Cleanup `0x137C4` handles/acknowledges interrupts (`0x1260E`, `0x126EE`), writes BAR1 `MTTCTL +0x080 = 0`, sets BAR0 `ERRM +0x008 = 0xFFFFFFFF`, clears the software interrupt mask and synchronously sets BAR0 `INTEN +0x084 = 0`. Quiesce `0x138D4` saves/masks interrupts but does not visibly abort DMA. The MAM postlude `0x12D6A` reads `IIMST +0x04C`, writes `IIMCL=0` if bit 0 is set, and reads ERRS; it does **not** poll until a proven idle state.

**No original abort/idle protocol was proved.** In particular, `MTTCTL=0`, `IIMCL=0`, `IIMST` bit 0, an IRQ, or masking INTEN must not be treated as proof of completed bus transactions. A logged timeout is not a hardware reset. Physical DMA completion's memory-ordering guarantee also remains unknown.

Original `CFDC2400 → 0x12EDE → 0x12EC2 → 0x11390` can inject software pending IRQ bit 0 into the DPC and signal an acquisition completion event without physical INTST bit 0. Therefore an event signal alone is not evidence of hardware completion.

## Defensive x64 behavior staged on the draft branch

This is **partial damage containment**, not DMA engine stop/recovery:

1. The existing DMA start paths (`LecExecuteLegacyMttTransferLocked` and `LecIoctlAcquireBufferedOneChannel`) reject a latched `DmaUnknownActive` device.
2. Launch clears `DmaCompletionIrqSeen`. Only the physical ISR sets that marker when real INTST bit 0 is processed for an attached transfer.
3. On a launched request with timeout/failed wait or a supposedly successful event with no hardware-IRQ marker, mark its transfer `DmaUnsafeToFree`, latch `DmaUnknownActive` and disable new IOCTL admission. A software-event-only success becomes `STATUS_IO_DEVICE_ERROR`; ordinary timeout remains `STATUS_IO_TIMEOUT`.
4. `LecFreeTransfer` does not release poisoned descriptor pool storage, descriptor MDLs, or user MDLs. `LecUnregisterTransfer` and process cleanup cannot free them; STOP/REMOVE detach them without freeing. **This deliberately leaks pinned pages and kernel allocations until system restart** when the device's inactivity cannot be proved, to avoid returning possibly DMA-owned memory to the OS. A driver unload or new attachment is **not** proven safe recovery.
5. `CFDC2400` continues to accept the known zero-mask diagnostic but rejects software completion bit 0. Source and debug ABI contracts must not interpret this as full legacy opcode parity.
6. STOP/REMOVE still unmap BARs and detach the FDO after quiesce. Retaining DMA memory is necessary but **insufficient** to prove a fully safe PnP lifecycle (PCI transaction completion and hardware reset remain unverified).

The defensive patch has not been executed under Driver Verifier, in a DMA fault injection simulation, or on real PCI hardware. The previously owner-verified 17/17 Dry pass **predates** this new patch; do not assign it to the modified code.

## Windows DMA-adapter migration design (unimplemented)

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
- `LecDmaAllocateCommonTable`: requests a device-visible
  `AllocateCommonBuffer`, validates page alignment and entire 32-bit
  logical-address range, and zero-initializes the descriptor table.
- `LecDmaReleaseAdapterContext`: frees common buffer and adapter
  only with an explicit, externally established `ProvenIdle=TRUE`
  and no previous quarantine. Otherwise the independent context and
  device mapping remain quarantined; this is a leak, **not recovery**.

No PnP or acquisition call site uses these functions yet. The separate
inactive SG stage can request a mapping for one MDL, but adapter/MDL/PNP
lifetime is not yet unified and no live mapping exists. The existing
PFN-based active path remains unmodified. Review callback IRQL, limited map registers, owner lifetimes,
STOP/REMOVE and reset/idle requirements before wiring this into DMA.
The adapter stage compiled successfully in the owner's Windows WDK Dry
run (2026-10-04). It has not been exercised by runtime DMA requests.

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

The **new, inactive** `DmaSyncStage.c/.h` implementation:
- Requires DMA adapter **v3** and its transfer-context initialization,
  synchronous GetEx and FreeAdapterObject operations; no automatic v2
  fallback. `DmaAdapterStage` now requests `DEVICE_DESCRIPTION_VERSION3`.
- Has **no device launch function**, no MMIO and no integration with
  the active `Acquisition.c`, PnP or IOCTL paths.
- Uses a unique, aligned `DMA_TRANSFER_CONTEXT_SIZE_V1` context per
  request and a locked MDL chain (including more than one 32-MiB MDL).
- Validates requested byte extent and copied SG elements for full
  coverage, nonzero dword-aligned lengths, 32-bit logical addresses,
  and bounded caller-provided element capacity. Actual descriptor
  encoding and map-register/SG capacity negotiation remain open.
- Adds a nonpaged parent `LECS65_SG_SYNC_OWNER` with a spinlocked
  `Stopping` gate and `Outstanding` count. A submission increments
  before its WDM call, and cleanup decrements only after its no-launch
  allocation is released. There is **at most one outstanding mapping
  per adapter owner** until v3 channel-sharing semantics are proven.
  Successful mappings are referenced by monotonic, nonreused tokens:
  copies and token removal use the same lock, and physical adapter
  resources are freed outside it. No raw stage pointer escapes. STOP closes admission and returns BUSY while
  mappings remain; it never waits and therefore does not by itself
  deadlock PnP/remove locks. Caller MUST preserve the parent, DMA
  adapter, PDO, and pinned MDLs until the gate reports quiescence.
- In the specific no-launch case calls `FreeAdapterObject`, drops
  the held device-object reference, clears the sole caller's stage
  pointer and deallocates stage memory. It does not implement any
  release for hardware-started or unknown-active DMA.

The new `tests/dry/test-sg-sync.c` fake-v3 DDI test checks immediate
success/failure, transfer-context failure, no deferred callback,
no-launch cleanup, sequential double release, 48-MiB two-MDL mapping,
truncation, 4-GiB rejection and STOP racing with mapping allocation.
**Current changes require owner-run Windows WDK and Dry verification.**
These synthetic tests do not verify real OS resource allocation.

### Unresolved activation blockers

This owner is a staging-only PnP gate, not a live STOP/REMOVE policy:
the real PnP code does not reference it. No existing DMA transfer
may be converted to this path yet. In particular, a physical
hardware-start/IRQ/timeout path still lacks a proven bus-master idle
transition and must **never** call the no-launch release method.
The old async SG stage remains fail-closed and does not launch or
free mappings. Real STOP/REMOVE must eventually reconcile pinned MDL
chain ownership, adapters, descriptors, DMA completion, loss of device,
and remove locks, without waiting on permanently quarantined mappings.

Sources:
[GetScatterGatherListEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pget_scatter_gather_list_ex),
[InitializeDmaTransferContext](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pinitialize_dma_transfer_context),
[FreeAdapterObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pfree_adapter_object).

## Remaining P0 work, no shortcuts

- Obtain real board documentation or a verified, recoverable bench measurement of MAM and MTT abort/idle, physical completion ordering and safe reset/readback. Do not invent MMIO commands from guesses or treat the old x86 driver's behavior as a proof of hardware safety.
- Validate unexpected/premature IRQ, completion vs timeout, DPC pointer lifetime, STOP/REMOVE races and multiple handles under fault-injected software tests before any controlled hardware test.
- Replace direct PFN-derived bus addresses (`MmGetMdlPfnArray` and PFN shift) with an appropriate Windows DMA adapter/mapping model for supported DMA-remapping/IOMMU configurations. Retain the hardware 32-bit address and descriptor format limits; DMA mappings must not be released before hardware inactivity is proven.
- Replace the emergency pinned-memory quarantine with verified abort/drain/recovery ownership, including what happens across driver remove/reinstallation. It is not a viable final production resource-management model.

No artificial nonzero IRQ injection, licensed Dallas writes, hazardous DMA timeouts or unverified register writes should be performed on the only working WR6k.
