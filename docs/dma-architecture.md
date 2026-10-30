# WR6k DMA and acquisition architecture

## Regular MAM development admission

A development build may set `LecMamLaunchLimit` to a positive number. The
counter is atomically consumed after mapping/descriptor validation but before
arm, interrupt enable or acquisition MMIO. Exhaustion returns
`STATUS_DEVICE_BUSY`; a later pre-GO failure conservatively still consumes the
admission. This private build gate is separate from and mutually exclusive
with `FirstMamExperimentOnly`. The default WDM-v3 launch gate remains closed.

Batch 25 does not broaden that kernel gate. Its XStream observer performs one
COM activation and no explicit `Acquire()` call. If XStream automatically
submits a later `CFDC2138`, `LecMamLaunchLimitTryConsume` returns exhausted
before arm, interrupt enable or acquisition MMIO. The first completed request
keeps its own generation and retirement state; rejection of a later request
does not reset, deselect or free the first transaction.

The first live Batch 25 activation stopped in control initialization. No
`CFDC2124` registration or `CFDC2138` preparation reached the driver, so no DMA
adapter mapping, SG list, descriptor table or launch admission existed. The
observed failure therefore provides no new DMA success or abnormal-lifetime
evidence; it leaves the installed FDO's one-admission counter unused.

This document is the canonical end-to-end architecture and proof-boundary
reference. It describes the development tree after Batch 12. It does not claim
that acquisition DMA is release-ready.

## Evidence classes

### Verified replacement-source facts

- `CFDC2124` registers a user buffer, locks its pages and returns an opaque
  token. Production defers descriptor construction to WDM-v3 `CFDC2138`
  preparation; `CFDC2128` unregisters the buffer.
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
  connected to the bounded one-channel `CFDC2138` production preparation path.
  That call site has no hardware-launch or MMIO operation.
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
2. Registration probes and locks the user pages, constructs source MDLs and
   publishes the token under `TransferMutex`. The legacy PFN descriptor table
   exists only behind the explicitly unsafe compile gate.
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
7. Batch 12 obtains adapter-logical SG addresses and serializes descriptor
   publication for one exact request shape. The launch gate remains closed;
   future work still needs an independently proved physical-idle predicate
   before any possible GO can be admitted or a post-GO mapping released.

## Batch 11 software transaction

`DmaTransaction` now supplies the hardware-independent owner that connects
request validation, process/FileObject identity, adapter and buffer admission,
mapping-callback ownership, logical-address descriptor construction,
publication, generation-safe completion, cancellation, timeout, retirement and
quarantine. Its diagnostics count mapping acquisition/release, descriptor
build/publication, launch admission/rejection, completion, timeout, power/PnP
teardown and quarantine. The public IOCTL ABI is unchanged.

The transaction calls the established WR6k encoder only with DMA logical
addresses and propagates invalid, 32-bit-address and table-capacity failures.
Publication requires an explicit caller-supplied barrier. The production
`CommitHardwareLaunch` operation always returns `LecDmaTxLaunchDisabled` and
rolls Arming back to Published. Only host tests compile a model-only possible-
GO hook, so the driver binary has no transaction transition to DeviceActive.

The staged PnP parent now tracks an independent D0/Dx generation. QUERY and
power-down close new-mapping admission; only a successful Started+D0 restore
may reopen it. Existing no-launch mappings remain releasable during rundown.
Batch 12 wires these rules to live source paths: START owns adapter creation,
QUERY_STOP/QUERY_REMOVE close preparation, cancellation reopens only the exact
Started+D0 state, STOP/REMOVE/surprise removal drain never-launched mappings,
and device SET_POWER completion records D0/Dx without blocking or changing
hardware sequencing. The currently loaded driver is unaffected because this
development build was neither signed nor installed.

`DmaV3Transfer` now implements the explicit no-launch WDM-v3 sequence through
`GetDmaTransferInfo`, `InitializeDmaTransferContext`,
`AllocateAdapterChannelEx`, complete `MapTransferEx`, SG validation, common-
buffer descriptor encoding, `FlushAdapterBuffersEx` and
`FreeAdapterObject`. It rejects partial mappings, malformed coverage and
addresses outside the WR6k 32-bit descriptor format. A failed flush retains
the mapping and permanently quarantines the adapter context. The module has no
GO or MMIO API and no live acquisition caller. The synchronous
`GetScatterGatherListEx` owner remains separate no-launch staging with MDL-
chain coverage; neither path is a live acquisition transaction.

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

Batch 11 completes every transition that can be justified without those
hardware facts. It does not claim that IRQ observation, DPC drain, INTEN
masking, D0 exit or original-x86 cleanup proves physical retirement.

## Batch 12 selected acquisition path

The initial production integration is limited to the observed one-channel MAM
shape: a process registers a destination buffer with `CFDC2124`, then submits
the 15-byte METHOD_BUFFERED `CFDC2138` request `{token, channelCount=1,
pairMarker=1, channel<=0x3f, config, requestedBytes}`. The byte count must equal
the registered data length, stay below 16 MiB and retain the legacy 0x400-byte
alignment rule. The persistent MDL chain was probed and locked for device-to-
host access at registration, so preparation never dereferences user virtual
memory after registration.

DeviceControl already owns the IRP RemoveLock and active-IOCTL admission.
`TransferMutex` holds the token, MDL and process registration stable against
unregister, CLOSE and process cleanup. A publication reference holds the per-
PDO parent. `LecDmaPnpPrepareLegacyNoLaunch` binds the owner PID and FileObject,
maps synchronously, copies and validates the complete SG list, encodes only DMA
logical addresses, publishes behind a memory barrier, calls the permanently
closed launch gate, cancels and releases exactly once. No legacy PFN address is
used by this path. The legacy PFN table is not built while
`LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA` is zero.

The precise lifetime state machine is maintained in
[DMA lifetime and timeout](dma-lifetime-and-timeout.md); original x86 evidence
is maintained in [the legacy abort audit](legacy-dma-abort-bus-idle-audit.md).

## Batch 13 pre-GO side-effect boundary

The complete production path from `CFDC2124` through the one-channel
`CFDC2138` no-launch result is recorded in the machine-readable
[`dma-pre-go-side-effect-allowlist.json`](dma-pre-go-side-effect-allowlist.json).
The reachable driver operations are memory allocation/locking, owner and PnP
rundown, synchronous WDM-v3
scatter/gather mapping, DMA-logical descriptor construction in the START-time
common buffer, a memory barrier, unconditional software launch rejection and
never-launched mapping release. The driver issues no register or port read or
write on this path. Windows may establish platform/IOMMU mapping state through
`GetScatterGatherListEx`; that is not a WR6k command and is released through
`FreeAdapterObject` before the request returns.

The excluded `LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA` branch remains visibly
hardware-active and contains GPIODAT, MAM, SGTA, IIMTC, INTEN, IIMCL and GO
operations. It is zero by default and is forbidden by the Batch 13 source
contract. The no-launch evidence therefore cannot qualify acquisition or
physical DMA idle.

## Batch 15 installed no-launch finding

The installed Batch 14 binary passed 9/9 R0, 2/2 R1 and one captured PnP
restart. Its single authorized no-launch attempt then returned Win32
`ERROR_NOT_SUPPORTED` from `CFDC2124`, before `CFDC2138` or unregister was
issued. The source has exactly two matching registration-time statuses: the
legacy descriptor table or a source PFN above 4 GiB. The retained evidence
does not distinguish which allocation triggered the status, and neither is a
WDM-v3 mapping failure because that path was never reached.

The development fix keeps MDL creation and token publication in production
registration but compiles `LecBuildDescriptorTable` only when
`LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA` is enabled. Debug and Release x64 builds
and the focused contracts pass. The fix is not installed, so no live evidence
yet proves WDM-v3 mapping, SG construction, common-buffer descriptor
publication or rollback on this hardware.

## Batch 17 real no-launch mapping evidence

The installed MDL-only candidate completed one authorized production
registration/preparation sequence. `CFDC2124` retained a 4-KiB locked MDL
registration. `CFDC2138` then produced one Windows DMA-logical SG segment and
two WR6k-format descriptors in the START-time common buffer. This proves that
the real adapter accepted the bounded mapping and that address-range, coverage,
alignment and descriptor-capacity validation passed. Exact logical addresses
are private runtime evidence and are not needed for the architectural result.

The launch gate remained closed. The trace records `launchDenied=1`,
`released=1` and `STATUS_NOT_SUPPORTED`; therefore no descriptor pointer, MAM,
MTT, GO or acquisition register was written. This is the first real-hardware
proof of the selected WDM-v3 mapping and descriptor-preparation path, but it is
deliberately not evidence of hardware consumption or physical bus idle.

The remaining architecture boundary begins only after possible GO. A release
design still needs authoritative evidence for descriptor-fetch retirement,
MAM/MTT producer quiescence, final PCI write completion, bus-master retirement
and posted-write drain before pages, mappings or descriptor storage can be
reused. No existing IRQ, timeout, STOP, reset or software completion proves
that boundary.

## Batch 18 development-gated MAM transaction

The new path replaces only the unsafe addressing and lifetime parts of the
historically working implementation. `CFDC2124` retains the process-owned MDL
chain. `CFDC2138` obtains a synchronous WDM-v3 scatter/gather list, copies its
DMA logical segments, encodes the original descriptor format in the common
buffer and publishes that table before arming hardware.

`LecWdmV3MamDma` defaults to `false`. When explicitly enabled, the callback
preserves the original order: clear `GPIODAT` bit 16; program five `MAMDAT`
values, `MAMPGO` and `MAMSEQ`; write the common-buffer logical address to
`SGTA` and byte-count/4 to `IIMTC`; reset/select/arm; enable INTEN bit 0;
synchronize `IIMCL=1` and one `MAMRGO`; wait at most five seconds; mask INTEN;
finalize completion; deselect; then perform the IIMST/IIMCL/ERRS postlude.

Normal return requires a matching physical IRQ/DPC observation and successful
cleanup. The caller then retires the WDM mapping and permits another generation
with the same registration. Every uncertain post-GO state is terminal
quarantine. See [x86/x64 acquisition parity](x86-x64-acquisition-parity.md).

## Batch 19 first-experiment boundary

The first signed GO-capable package proved installation and the safe R0/R1
baseline, but final review stopped before GO: its compile-time MAM gate did not
itself restrict the request to the authorized 16-byte experiment and did not
consume a per-loaded-FDO attempt latch. Relying only on the diagnostic command
line would not make the kernel boundary fail closed.

The offline successor adds `LECS65_FIRST_MAM_EXPERIMENT_ONLY`, controlled by
the default-off `LecFirstMamExperimentOnly` build property. After the Windows
DMA mapping and descriptor construction are known, but before any acquisition
MMIO, it requires channel `0x30`, config `0x00000C00`, 16 transfer bytes and
exactly two descriptor entries (one SG data entry plus the terminator). It then
atomically changes `FirstMamExperimentConsumed` from zero to one. A shape
mismatch returns `STATUS_NOT_SUPPORTED`; a consumed latch returns
`STATUS_DEVICE_BUSY`. Failure after latch consumption remains fail closed and
cannot authorize another GO in the same loaded FDO.

The package workflow records and verifies this mode independently of the broad
development MAM gate. Batch 20 additionally validates the actual SG element,
encoded payload/address pair, common-buffer alignment/range and `{0,0}`
terminator before consuming the latch. The corrected version `0.2026.1029.1`
binary is locally test-signed and hash-frozen, but it is not installed. It must
not be confused with installed `oem105.inf`, which lacks this final restriction.

The first Batch 20 installation launcher established kernel-capture readiness
but failed before PnPUtil dispatch because its caller supplied a literal
environment-variable expression rather than an absolute path. This is not DMA
evidence: there was no installation, `CFDC2124`, `CFDC2138`, MMIO or GO.

Batch 21 corrected only the launcher boundary and installed that immutable
candidate as `oem106.inf`. The marker-bearing SYS hash is
`647A7B38597C19D96DF509557E05895C5074727C329EFBD182AD01AC144E3F71`.
The complete safe R0/R1 matrix passed after installation. At the end of Batch
21 no registration, descriptor publication, MAM register programming or GO had
occurred; Batch 23 later satisfied both independent gates and ran once.

## Batch 23 first physical transfer

One authorized 16-byte/channel-`0x30`/config-`0xC00` transfer exercised the
installed WDM-v3 path. Runtime evidence contains one SG data segment, two
gate-validated descriptors including `{0,0}`, one atomic gate consumption and
one successful transaction summary: launch admitted, completion observed,
mapping released, no quarantine and `STATUS_SUCCESS`. The client received a
four-byte count of 16 and successfully unregistered the persistent token.

On this implementation, `CompletionObserved` cannot be produced by successful
dispatch alone. The ISR must observe enabled physical INTST bit 0 for the active
generation; the DPC must consume that generation and signal the selected
transfer; cleanup must then clear INTEN bit 0 without uncertainty. Only after
that chain does the transaction retire and `LecSgSyncReleaseCompleted` release
the Windows mapping. The capture did not emit one line per MMIO write, so the
original MAM/SGTA/IIMTC/INTEN/IIMCL/MAMRGO order is source-proven and exercised
as one path, not independently probed at every register.

The diagnostic initialized but did not print or retain the 16-byte destination
after completion. This result proves the bounded DMA/IRQ/DPC/lifetime path for
the test shape, not waveform content, repeated operation or XStream parity.
