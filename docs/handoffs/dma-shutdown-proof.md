# DMA engine shutdown proof needed (analysis workstream)

This is a focused, read-only research request for a **separate analysis chat**. Do not edit the production driver, enable new IOCTLs, write PCI registers, or test fault injection on the licensed working WR6k.

## Question

Before the native x64 driver frees locked user pages and physical descriptor tables after an acquisition timeout or STOP/REMOVE, how can we **prove the original PCI DMA engine is no longer bus-mastering**? Identify the original x86 host's exact safe stop/abort/idle protocol and whether it differs for `CFDC2138` (MAMRGO acquisition) versus family-1 `CFDC2110` MTT via `MTTRGO`.

## Known native paths to compare

- `driver/Ioctl.c:LecIoctlAcquireBufferedOneChannel` programs `SGTA`, `IIMTC`, `IIMCL`, `MAMRGO`, then waits up to 5 s for completion and on timeout disables the completion interrupt, clears `CurrentTransfer`, reads IIMST and may write IIMCL=0.
- `driver/Ioctl.c:LecExecuteLegacyMttTransferLocked` programs `SGTA`, `IIMTC`, `IIMCL`, `MTTRGO` and also waits at most 5 s, then clears the interrupt bit and transfer pointer.
- `driver/Acquisition.c:LecUnregisterTransfer`, `LecReleaseTransfersForProcess`, `LecReleaseAllTransfers`, `LecFreeTransfer` can release MDLs and descriptor storage after those waits. `driver/Device.c:LecS65Pnp` now drains synchronous IOCTLs and deferred work before teardown, but does not prove idle DMA hardware on prior timeout.
- Current working branch: `fix/p0-irq-start-dma-gating`; keep `main` and this branch untouched while analyzing.

## Read-only original evidence

Use the existing Ghidra exports/assembly and canonical reverse-engineering docs, focusing on original functions `0x171DE`, `0x1731C`, `0x17478`, `0x12D6A`, `0x13C84`, `0x138D4` (hardware quiesce), `0x137C4` (cleanup), and relevant MTT/MAM/IRQ helpers. Do not assume merely masking INTEN or observing a five-second timeout stops PCI bus mastering.

## Required deliverable

1. Exact original machine-instruction evidence: original function/address, register BAR+offset, bit masks and read/write/ack sequence, including when the DMA engine is proven idle.
2. Distinguish successful completion, timeout, explicit cancellation, PnP STOP, surprise removal and power transitions; state what is source-proven and what remains unknown.
3. State whether `IIMST` or other status bits are a reliable DMA-idle indication; account for posted writes, retry/timeouts and races.
4. Provide a compact proposed **safe implementation contract** for the native x64 driver, specifying what may be done without hardware and what absolutely requires disposable/recoverable test hardware.
5. Explain DMA-remapping/IOMMU implications of current PFN-derived bus addresses, without assuming 32-bit PFNs are universally valid DMA device addresses.
6. If no reliable abort/idle sequence is demonstrable, say so explicitly and propose a conservative policy without guessing register values.

This file is a temporary handoff. Once the result is integrated and its conclusion documented canonically, remove the handoff.
