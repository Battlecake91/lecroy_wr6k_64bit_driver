# WR6k DMA and acquisition architecture

This document is the canonical end-to-end architecture and proof-boundary
reference. It describes the development tree after Batch 10. It does not claim
that acquisition DMA is release-ready.

## Evidence classes

### Verified replacement-source facts

- `CFDC2124` registers a user buffer, locks its pages, builds the legacy
  descriptor chain and returns an opaque token. `CFDC2128` unregisters it.
- `CFDC2138` selects a registered token, prepares one-channel MAM state, writes
  `SGTA`/`IIMTC`, enables completion interrupt bit 0, writes `IIMCL=1`, writes
  `MAMRGO`, waits up to five seconds and performs completion cleanup.
- `CFDC2110` family 1 opcodes `0x50` and `0x51` use the same DMA front end and
  launch through `MTTRGO`.
- The legacy descriptor builder derives 32-bit addresses from MDL CPU PFNs.
  It does not use a Windows DMA adapter mapping. Batch 10 additionally rejects
  a non-page-aligned descriptor table and non-DWORD-aligned source addresses.
- Software generations, ISR exclusion around GO, DPC attribution and
  `UnknownActive` quarantine harden CPU-side ownership. They do not tag a
  hardware interrupt or prove physical retirement.
- The WDM-v3 adapter, common-buffer, scatter/gather and ownership modules are
  compiled no-launch staging. They have no production acquisition call site.
- The current development build defines
  `LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA=0`. Both legacy MAM and MTT launch
  functions therefore return `STATUS_NOT_SUPPORTED` before DMA-related MMIO.
  The installed Batch 9 driver predates this gate and remains unchanged.

### Verified original-x86 disassembly findings

- Registration at `0x1731C` and builder `0x18194` produce 8-byte
  `{count_dwords,address32}` entries. Slots 0 through 510 describe data; slot
  511 can link to the next descriptor page; `0/0` terminates the chain.
- `0x171DE` programs `SGTA` and `IIMTC`, resets the event, enables `INTEN` bit
  0, writes `IIMCL=1`, launches MAM or MTT, waits five seconds and masks the
  completion interrupt.
- ISR `0x108D6` observes `INTST`, writes `IIMCL=0`, acknowledges the source and
  queues DPC `0x11390`. A separate software path can set pending bit 0, so the
  event alone is not physical-completion evidence.
- Timeout, STOP, REMOVE and power paths contain no verified hardware-idle poll,
  PCI transaction drain or safe DMA abort. Original cleanup may later free
  MDLs after timeout; legacy behavior is evidence, not a safety contract.

### Verified FPGA and board facts

- PCI-card Spartan-IIE `U3` is the physical PCI/INTA endpoint. It is separate
  from acquisition-board ADC+MAM and FPGA blocks.
- Available schematics establish connectivity, not the running RTL semantics
  of GO, `IIMST`, `IIMCL`, `MTTCTL`, descriptor fetch, reset or IRQ ordering.
- The descriptor format exposes no host-visible ownership, consumer index or
  done field. Host memory cannot prove that U3 has stopped referencing it.

### Verified hardware observations

- Historical traces demonstrate successful MAM/MTT data and IRQ paths, and PCI
  Command `0x0006` showed Memory Space and Bus Master Enable set.
- Batch 9 established the installed non-DMA runtime baseline: `oem102.inf`,
  version `0.2026.1022.1`, problem code zero, service running and five
  interfaces. Batch 10 performed no device access.
- Successful transfers and completion interrupts demonstrate functionality;
  they do not establish final-write ordering or a PCI drain.

### Hypotheses, not safety predicates

`IIMCL=0` may stop or clear part of the engine; `MTTCTL=0` may affect the MTT
producer; `IIMST` bit 0 may describe busy/done; START may reset FPGA state; and
clearing PCI Bus Master Enable may prevent new requests. None has a verified
contract covering already-issued requests, posted writes, internal FIFOs and
the last host-memory access.

## End-to-end ownership path

1. A process opens one of the five compatible interfaces. CREATE associates
   the file object with its process owner.
2. Registration probes and locks the user pages, constructs source MDLs and a
   legacy descriptor table, then publishes the token under `TransferMutex`.
3. An acquisition request resolves the token to the file/process owner and
   serializes the selected transfer.
4. In the historical active path, setup MMIO precedes synchronized completion
   publication and GO. The ISR records only the current software generation;
   the DPC signals the matching event.
5. Timeout, inconsistent completion or cleanup failure enters irreversible
   `UnknownActive`; pages and descriptors are retained through FDO teardown.
6. Historical normal IRQ completion allowed later unregister/CLOSE cleanup.
   That is an unverified compatibility assumption, so the Batch 10 development
   build blocks GO instead.
7. A future WDM-v3 transaction must obtain adapter-logical SG addresses,
   serialize descriptor publication, launch once, establish physical idle and
   only then release its mapping, common buffer and locked pages exactly once.

## Acquisition implementation gap

The no-launch WDM-v3 modules provide adapter ownership, logical-address
validation and fake-DDI tests, but not a complete acquisition transaction.
Missing pieces are: token-to-mapping policy, persistent-versus-map-at-launch
ownership, common descriptor table publication to `SGTA`, launch/completion
integration, physical idle evidence, PnP and power rundown, mapping release,
and multi-token resource-pressure policy. POWER IRPs currently pass through;
a D0/Dx admission and rundown model is required before acquisition.

## Release gates

Real DMA remains blocked until all of the following exist:

1. Windows DMA logical addresses are used end to end; no CPU PFN is published.
2. MAM and MTT have a documented stop/abort and physical-idle predicate.
3. IRQ observation is separated from producer stop, transaction drain and
   mapping retirement.
4. STOP, REMOVE, surprise removal, CLEANUP/CLOSE and D0/Dx transitions preserve
   ownership or quarantine it without a premature release.
5. Fake-DDI fault injection, expanded race tests, Driver Verifier DMA checks
   and a separately authorized isolated-hardware experiment pass.

The precise lifetime state machine is maintained in
[DMA lifetime and timeout](dma-lifetime-and-timeout.md); original x86 evidence
is maintained in [the legacy abort audit](legacy-dma-abort-bus-idle-audit.md).
