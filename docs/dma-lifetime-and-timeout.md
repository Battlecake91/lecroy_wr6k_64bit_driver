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

## Remaining P0 work, no shortcuts

- Obtain real board documentation or a verified, recoverable bench measurement of MAM and MTT abort/idle, physical completion ordering and safe reset/readback. Do not invent MMIO commands from guesses or treat the old x86 driver's behavior as a proof of hardware safety.
- Validate unexpected/premature IRQ, completion vs timeout, DPC pointer lifetime, STOP/REMOVE races and multiple handles under fault-injected software tests before any controlled hardware test.
- Replace direct PFN-derived bus addresses (`MmGetMdlPfnArray` and PFN shift) with an appropriate Windows DMA adapter/mapping model for supported DMA-remapping/IOMMU configurations. Retain the hardware 32-bit address and descriptor format limits; DMA mappings must not be released before hardware inactivity is proven.
- Replace the emergency pinned-memory quarantine with verified abort/drain/recovery ownership, including what happens across driver remove/reinstallation. It is not a viable final production resource-management model.

No artificial nonzero IRQ injection, licensed Dallas writes, hazardous DMA timeouts or unverified register writes should be performed on the only working WR6k.
