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
This section is a **design plan only**. No DMA adapter was added and no
IOMMU compatibility is claimed.

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
mapping nor physical bus-idle**. The new version is not yet validated
on Windows. Next implementation milestone: acquire and retain the
appropriate `DMA_ADAPTER`, obtain mappings for user buffer MDLs
and common-buffer descriptor pages, and replace the old PFN table
builder only when resource ownership and abort/idle guarantees
are satisfactorily modeled.

## WDM adapter/common-buffer staging (not yet activated)

A separate `driver/DmaAdapterStage.c/.h` module now compiles as part
of the draft driver. It provides:
- `LecDmaCreateAdapterContext`: `IoGetDmaAdapter` using a
  `DEVICE_DESCRIPTION_VERSION2` PCI bus-master, 32-bit DMA-address
  constraint, scatter/gather support and maximum transfer size. The
  returned map-register count is retained, **not assumed sufficient**.
- `LecDmaAllocateCommonTable`: requests a device-visible
  `AllocateCommonBuffer`, validates page alignment and entire 32-bit
  logical-address range, and zero-initializes the descriptor table.
- `LecDmaReleaseAdapterContext`: frees common buffer and adapter
  only with an explicit, externally established `ProvenIdle=TRUE`
  and no previous quarantine. Otherwise the independent context and
  device mapping remain quarantined; this is a leak, **not recovery**.

No PnP or acquisition call site uses these functions yet. In particular,
no transfer MDL scatter/gather mapping or `GetScatterGatherList` callback
lifetime has been introduced. The existing PFN-based active path is still
unmodified. Review callback IRQL, limited map registers, owner lifetimes,
STOP/REMOVE and reset/idle requirements before wiring this into DMA.
The new source is pending Windows WDK compilation and Dry verification.

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
WDM integration. Tests and WDK build of this new revision are pending.

## Remaining P0 work, no shortcuts

- Obtain real board documentation or a verified, recoverable bench measurement of MAM and MTT abort/idle, physical completion ordering and safe reset/readback. Do not invent MMIO commands from guesses or treat the old x86 driver's behavior as a proof of hardware safety.
- Validate unexpected/premature IRQ, completion vs timeout, DPC pointer lifetime, STOP/REMOVE races and multiple handles under fault-injected software tests before any controlled hardware test.
- Replace direct PFN-derived bus addresses (`MmGetMdlPfnArray` and PFN shift) with an appropriate Windows DMA adapter/mapping model for supported DMA-remapping/IOMMU configurations. Retain the hardware 32-bit address and descriptor format limits; DMA mappings must not be released before hardware inactivity is proven.
- Replace the emergency pinned-memory quarantine with verified abort/drain/recovery ownership, including what happens across driver remove/reinstallation. It is not a viable final production resource-management model.

No artificial nonzero IRQ injection, licensed Dallas writes, hazardous DMA timeouts or unverified register writes should be performed on the only working WR6k.
