# FPGA DMA Implementation-Owner Handoff

Hardware research baseline: `analysis/pci-dma-evidence-20261006` at
`7108ee7638da021094b7d22959d55a7a4ec0cb7b`. This is a production decision guide,
not code to merge. The software facts below refer to that research checkout;
later implementation branches/PR #10 may contain additional hardening and must
be checked at their own HEAD. This task does not review or revert those changes.

Read the [final evidence index](fpga-reverse-engineering-final-evidence.md),
the single authoritative
[`shutdown_evaluation` matrix](../pci-mmio-register-evidence.json), and the
[provider requests](fpga-dma-missing-evidence.md). All seven release/stability
obligations remain Unknown. Installed hardware/image identity remains Unknown.

## Safe Uses of the Research

- Preserve validated BAR/offset distinctions: START BAR0+0x00C is not ACQFVER
  BAR1+0x00C; local IIMCL/INTST/INTEN storage is not a downstream producer ACK.
- Preserve32-bit DMA address/descriptor limits, alignment, overflow rejection,
  count/address layout, chain slots and zero/zero software terminators. Hardware
  terminal logic uses Word-A zero; do not invent a new terminator protocol.
- Use recovered masks, readback aliases and source topology to strengthen
  software ownership tests and source attribution documentation. INTEN bit0
  readback aliases IRQ, not the mask-storage FF: it is not mask verification.
- Use actual equations to reject unsupported idle/empty meanings. Modulo
  indices, periodic publication, issue/rewind, C and IRQ are not drain flags.
- Keep local proof tests and private image/pin validation available for review.
  Never copy a bounded SAT trace or invented PCI_CE value into production policy.

No conclusion authorizes a new register sequence, hardware reset, timeout
recovery, PFN address relaxation or adapter-DMA activation.

## Software Guarantees Versus Hardware Premises

| Source boundary | Established software behavior at research baseline | Not established |
|---|---|---|
| `driver/Acquisition.c:LecSelectDmaTransfer,LecLaunchSelectedDma` | Generation selection/completion lock; synchronized launch publication excludes ISR in the GO/publication interval | Old physical source or old DMA already retired |
| `LecInterruptService,LecInterruptDpc,LecDeselectDmaTransfer` | Software generation checks and locked event-pointer handoff | Identity/epoch of the physical source, final PCI write or host visibility |
| `driver/DmaCompletion.c:LecDmaCompletionMayReleaseMapping` | CompletionObserved is not an allowed mapping-release phase; NeverLaunched and separately asserted IdleProved are distinct | `LecDmaCompletionProveIdle` is not itself a hardware observer/certificate |
| `LecFreeTransfer,LecUnregisterTransfer` | Poisoned/selected transfers cannot follow normal free; user/descriptor MDLs and process-list ownership are explicit | Ordinary legacy success frees unpoisoned resources without independent idle proof |
| `LecQuiesceDeferredWork`, `driver/Device.c` PnP paths | Admission/rundown and ISR/DPC/timer/list teardown establish CPU ownership boundaries | Producer stop, PCI retirement or a bridge-drain guarantee |
| `driver/DmaSyncStage.c:LecSgSyncReleaseNoLaunch` | Inactive staging releases only under a no-launch contract and complete owned-use rundown | A future hardware-active mapping may use that release path |

See [normal-success audit](../dma-lifetime-and-timeout.md#normal-success-lifetime-audit-and-implementation-owner-handoff)
and [defensive software review](../dma-lifetime-and-timeout.md#defensive-review-and-implementation-owner-decisions).
Those historical source reviews must not be mistaken for validation of newer
completion-finalization/INTEN-ownership changes on another branch. Source-pattern
guards and fake-DDI tests are not whole-program concurrency or physical proofs.

## Why Completion Is Not Idle

Configured C is set by the internal L&&Word-A-zero term. IRQ is sticky,
set-dominated by C, and untagged. INTEN masks its contribution to INTA# without
clearing C/IRQ. IIMCL0 has old-state pipeline latency; IIMCL1 while cl/C1 does
not clear C automatically. Neither source ACK nor pad Hi-Z establishes empty
RX/staging storage, final accepted PCI writes or bridge completion.

The ISR assigns the **currently active software generation** to an observed
physical source. A delayed old untagged source can therefore receive a new
software generation. Matching the DPC/event generation protects software
handoff; it cannot identify the hardware epoch that caused the source.
Do not add a guessed hardware generation tag or use event delivery as retirement.

## Compatibility Boundary to Preserve

Existing MTT/MAM ABI, descriptor layout, synchronization, timeout/error statuses,
validated launch/cleanup register sequences and tested successful legacy behavior
remain unchanged by this research. Do not apply a speculative FPGA shutdown fix.

**Known exception:** the active legacy PFN path still permits normal-success
unregister/free and later acquisition on an unverified IRQ-to-final-access premise.
The research does not certify that policy, and does not silently replace it with
permanent retention of every successful acquisition. The implementation owner
must explicitly decide compatibility versus fail-closed retention, document the
impact, and qualify any change independently. Mask-cleanup failure behavior must
be reviewed on the actual implementation HEAD, not copied from an older audit.

UnknownActive is different: uncertain launched resources remain poisoned,
admission stays closed, and retained MDLs/descriptors/ownership survive FDO
removal. A later success, new generation, reset status, unload or reattachment
cannot make them releasable. Restart-only retention is containment, not a proven
reset/recovery protocol or a production-ready abort solution.

## PFN Addresses and Inactive WDM v3

`LecBuildDescriptorTable` uses CPU PFNs for both descriptor and data addresses.
Below4GiB, alignment and pinned pages do not establish device-logical addresses
under DMA remapping/IOMMU or a particular bridge. The active PFN path owns no WDM
map-register/SG mapping to flush/free. Adding adapter cleanup calls to it is not
a migration or a retirement proof.

Future migration must move **both** descriptor and payload addresses to valid
adapter-provided logical addresses, retain existing hardware width/layout limits,
and tie their lifetimes to actual acquisition ownership. The inactive WDM v3
stage intentionally has no live acquisition path; its safe no-launch release is
not reusable after hardware activation. CPU barriers, KeSynchronizeExecution,
PutScatterGatherList or FlushAdapterBuffersEx cannot manufacture board stop.
Use only the completion/coherency/release APIs matching resources actually
acquired, at their documented IRQL and after the applicable completion contract.

## Exact Release and Activation Gates

For **NeverLaunched**, prove absence of any device exposure/launch capability,
irrevocably close launch admission for that generation, and retire every software
reader/callback before releasing its correctly owned resources. Never relabel an
older unknown launched transfer NeverLaunched.

For **launched mappings**, all of the following are required together:

1. A versioned, installed-revision stop/completion certificate satisfying every
   row of `shutdown_evaluation`, including producer/link queues, descriptor and
   initiator retirement, buffer conservation and stable restart exclusion.
2. Hardware epoch/source provenance independent of a merely new software number;
   old sources and buffered/retried accesses cannot target a later generation.
3. Actual bridge/platform write visibility and Windows adapter coherency/ordering
   contracts, followed by matching adapter completion/release operations.
4. Atomic software ownership/admission policy covering acquisition, explicit
   unregister, process exit, STOP/SURPRISE/REMOVE, power, timeout, late IRQ/DPC,
   callback cancellation, common buffers, map registers and adapter teardown.
5. Deterministic integrated race/failure tests plus separate Windows/WDK and
   recoverable-target/platform qualification authorized by the owner. Offline
   FPGA tests and a user-mode native test are not kernel/hardware qualification.

Only then may the owner design live WDM activation. No gate is satisfied merely
by calling ProveIdle, masking interrupts, resetting status or passing these tests.

## Prioritized Implementation Checklist

1. Pin the implementation HEAD; reconcile software finalization/INTEN ownership
   and existing PR #10 fixes against this research-baseline audit without merging PR #9.
2. Preserve admission, completion-lock/event lifetime and irreversible quarantine;
   test mask failure, late-source attribution, timeout and STOP/REMOVE races.
3. Record the normal-success PFN compatibility decision explicitly; do not claim
   IOMMU safety or hardware-idle assurance for the existing successful path.
4. Obtain the [minimal provider evidence](fpga-dma-missing-evidence.md); update the
   authoritative matrix only when sufficient end-to-end invariants are reviewed.
5. Keep WDM v3 no-launch until all release/activation gates above are met.

This handoff changes no production code or hardware behavior. PR #9 remains
draft/open/unmerged. No live experiment is authorized by this document.
