# WR6k DMA completion, timeout and memory lifetime

## Current normal-completion boundary

The development-gated WDM-v3 MAM path uses a matching generation, accepted
physical bit-0 IRQ, DPC event delivery, successful wait, successful INTEN
cleanup and successful transaction retirement as the normal compatibility
boundary. Only that complete sequence releases the per-call WDM mapping. The
process-owned `CFDC2124` registration remains reusable.

This boundary is backed by thousands of historical acquisitions, but is not a
general proof of PCI write retirement. Timeout, missing completion, failed
cleanup, post-GO cancellation, CLOSE/PnP/power ambiguity or mapping mismatch
retains resources and permanently closes DMA admission. The original x86
timeout-and-free behavior is deliberately not reproduced.

For the first XStream waveform attempt, automatic application rearming does
not expand hardware ownership. The installed per-FDO limit admits at most one
validated transaction. A later request returns `STATUS_DEVICE_BUSY` before
acquisition MMIO and cannot retire or corrupt the first mapping. An admitted
pre-GO failure still consumes the limit. Any uncertain post-GO state continues
to retain mappings/pages and close admission through `UnknownActive`; neither
application exit nor a rejected second request proves hardware idle.

The actual Batch 25 application attempt failed earlier than this boundary.
There was no registration, mapping, descriptor, GO, IRQ or DPC record. No DMA
resource required retirement or quarantine, `UnknownActive` was not entered,
and the launch admission was not consumed. Four opened device handles closed
normally before DCOM reported the server-registration timeout.

Batch 21 installed the exact-shape, exact-once development build and completed
its 9/9 R0 plus 2/2 R1 compatibility matrix. At that point no GO had occurred,
so there was no new IRQ/DPC, retirement or `UnknownActive` evidence. Batch 23
subsequently exercised the complete matching normal path described below.

Batch 23 supplied the first live normal-completion evidence for this WDM-v3
path. One physical transfer returned through the matching IRQ/DPC event path;
INTEN cleanup remained certain, the transaction retired normally, the mapping
was released and the registration was subsequently unregistered. The summary
is `completed=1`, `released=1`, `quarantined=0`, `STATUS_SUCCESS`.

This does not weaken the abnormal boundary. No timeout, cancellation, CLOSE,
power or PnP race was exercised. Every future uncertain post-GO result must
still enter irreversible `UnknownActive` and retain all potentially referenced
resources. The one successful case is practical normal-path evidence, not a
general PCI retirement certificate.

Status: **fail-closed development path; not deployment-qualified**, 2026-10-23.
Implementation base: `fix/p0-irq-start-dma-gating` at merge commit `69ad853d2d48419f88be98935fdf879def417b43`.
Final PR #10 source-review parent: `fae8a50bb2b622628cd57e18723c3037698f06b5`; the merge tree is identical.
Research reference only: PR #9 at `6bff97ff473035cd7b5a0b6bd51cf48cfe82d364`; no research merge.
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

## Defensive x64 behavior on the merged development branch

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
3. On a launched request with timeout/failed wait, failed interrupt-mask cleanup, or a supposedly successful
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
result covers source contracts, native production completion/INTEN helpers and
inactive staging; it does not execute the integrated active kernel path.

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

The installed Batch 9 legacy PFN path remains behavior-compatible:
`CompletionObserved` satisfies its historical synchronous wait and ordinary
legacy MDLs/descriptors may later be freed. This is an explicit compatibility
policy based on an **unverified IRQ-to-final-memory-access ordering
assumption**, not a safety proof and not permission to release future WDM/IOMMU
mappings. Batch 10 applies strict enforcement in the development tree:
`LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA` defaults to zero and both MAM and MTT
return `STATUS_NOT_SUPPORTED` before DMA-related MMIO. This change is built and
offline-tested only; it has not replaced the installed driver.

### Physical retirement state machine

| State | Trigger/evidence | Required synchronization | Permitted cleanup | Forbidden cleanup | Confidence |
| --- | --- | --- | --- | --- | --- |
| `NoLaunchOwned` | Resources owned; GO proven not written | Owner/rundown locks | No-launch mapping and MDL release | Claiming a transfer ran | High, software |
| `Prepared` | SG and descriptors prepared, GO excluded | Owner plus ISR exclusion | Cancel to `NoLaunchOwned` | Release if GO publication is uncertain | High, software |
| `DeviceActive` | MAMRGO/MTTRGO written | Generation owner and ISR/DPC serialization | Retain and observe | Any DMA-resource release | High for GO, physical state unknown |
| `CompletionObserved` | Physical ISR accepted `INTST` bit 0 | ISR atomic state, DPC handoff | Complete software wait | Treat as stopped, drained or retirable | High as IRQ evidence only |
| `ProducerStopped` | Future documented abort/stop and status readback | MAM/MTT-specific exclusion | Proceed to drain proof | Assume old transactions ended | Currently unreachable |
| `TransactionsDrained` | Documented fence or external bus proof: no new requests, final writes complete, internal buffers empty | Device/platform ordering contract | Begin OS coherency/mapping retirement | Release from IRQ or delay alone | Currently unreachable |
| `Retirable` | Drain plus ISR/DPC/callback/remove-lock rundown and OS DMA completion | One transaction owner | Release mapping, table and pages exactly once | Partial owner release | Design target |
| `UnknownActive` | Timeout, uncertain GO, teardown after GO or missing drain proof | Terminal device latch | Close admission and retain all owners | Reset/reuse/free | High, fail-closed |
| `Quarantined` | Unknown owner survives FDO teardown | Publication anchor | Retention until proven cold recovery | Treat reload/restart as recovery | High, software |

`CompletionObserved != ProducerStopped != TransactionsDrained != Retirable`.
IRQ masking, DPC flush, a timeout, a delay and `IIMCL=0` are not PCI fences.
MAM and MTT must be proved independently unless a hardware contract explicitly
defines a shared stop/drain mechanism.

An already-admitted MAM IOCTL can wait behind another request on
`TransferMutex`. It now rechecks `DmaUnknownActive` after acquiring that mutex
and before any MAM setup MMIO, so a prior timeout cannot be followed by a
second launch from the queued request. STOP/SURPRISE/REMOVE still close the
outer admission gate and wait for already-admitted synchronous IOCTLs before
resource teardown.

## Normal-success cleanup audit and implementation boundary

The active MTT and MAM paths hold `TransferMutex` across selection, launch,
wait and deselection. Their production lifetime chain is:

| Boundary | Confirmed software contract | Remaining premise |
| --- | --- | --- |
| ISR | Reads enabled INTST, attributes bit 0 to the current software generation, acknowledges IIMCL and INTST and queues the DPC | The board IRQ is untagged; a delayed old physical source after a later GO cannot be distinguished. |
| DPC/event | Checks transfer and IRQ generations; consumes the marker and signals the embedded event under `DmaCompletionLock` | CPU object exclusion is not device retirement. |
| Wait/cleanup | Attempts the existing INTEN bit-0 mask before finalizing launched ownership; both wait and mask must succeed, with matching completion evidence | Mask helper success means register resolution/write was issued, not verified hardware masking or a PCI drain. |
| Deselect | Clears the selected pointer and generation under the same completion lock as event signaling | An in-flight ISR may still acknowledge/queue work; it does not hold a transfer pointer. |
| Unregister/process cleanup | Rejects selected or poisoned transfers; unlinks under `TransferMutex` before freeing | Unpoisoned normally completed legacy transfers still rely on the compatibility exception below. |
| Free | Releases descriptor MDL/storage, unlocks source MDLs and frees the transfer only if not poisoned; poisoned allocations move to independent retention ownership | This legacy free gate is not `IdleProved` and is not suitable for hardware-active WDM mappings. |
| STOP/REMOVE | Closes admission, drains IOCTLs, disconnects IRQ, flushes DPC work and stops the timer before transfer/BAR teardown; REMOVE also drains the remove lock | Software rundown cannot stop outstanding PCI writes. |
| Next acquisition | Rejects the device-wide unknown-active latch; fresh generation preparation occurs only after rejecting an existing selected transfer under the completion lock | Fresh software identity does not establish retirement of prior hardware work. |

Confirmed corrections:

- Previously a successful wait was finalized before fallible mask cleanup.
  A later mask failure changed only the returned status, leaving the transfer
  unpoisoned and later acquisition admission open. Both launch paths now call
  the shared `LecDmaCompletionFinishCleanup` decision after masking and before
  deselection. Failure after GO permanently poisons ownership, closes admission
  before fallible parent notification and retains descriptors and locked pages.
  A mask error replaces success; an earlier timeout/error remains the returned
  error. No additional MMIO operations or unverified readbacks are introduced.
- Duplicate physical IRQ observation previously republished the same generation
  even after a DPC consumed it. Only the first `DeviceActive -> Observed` winner
  now publishes the marker. Duplicate sources still receive the existing ISR
  acknowledgements, but cannot replenish software completion evidence.
- Busy selection previously prepared a fresh tracker before checking the
  selected pointer. Preparation now follows the busy check inside the lifetime
  lock. This preserves the current tracker on rejection; it is defensive
  ordering, not evidence of an observed race under the existing mutex contract.

Successful legacy PFN acquisition, status/output ABI, descriptor format and
32-bit limits remain unchanged. Cleanup before GO can still cancel arming and
release `NeverLaunched` resources. Timeout-before/after-IRQ and STOP/REMOVE
uncertainty cannot be rescued by a late IRQ or subsequent successful cleanup.
No double-free or event-use-after-free was found in the audited serialized
list/lock/rundown paths. Source-order tests guard those contracts; they are not
an integrated kernel concurrency proof. Native tracker tests execute production
C with synthetic thread scheduling, and fake-DDI suites check exact staging
release/retention counts. Actual results and build limits are recorded in
[regression testing](regression-testing.md).

The normal-success compatibility exception is deliberately unchanged:
`CompletionObserved != IdleProved`, yet the legacy PFN path can later free
unpoisoned resources on that evidence. Its IRQ-to-final-device-access and host
visibility ordering remains unverified. This patch does not solve that hardware
lifetime problem or authorize live WDM DMA acquisition.

### Concurrent execution review

The complete PR #10 diff at `07683fa` against the current development target
`82b4c4e` was reviewed independently of previous test passes. The target is an
ancestor (0 target-only / 4 implementation-only commits at review start), so
there is no textual integration conflict. One additional source-level gap was
demonstrated: surprise removal latched unavailable hardware for INTEN helpers,
but the shared ISR and queued HWInt DPC did not check that latch before MMIO.
The latch is now an interlocked LONG and a resident metadata-only predicate
gates both paths. Normal connected operation and register sequences are
unchanged. This guard is not an interrupt rundown or physical idle proof.

| Race | Result / required synchronization | Evidence boundary |
| --- | --- | --- |
| Two IRQ observations / duplicate after consumption | Only the first active-to-observed CAS publishes a marker. | Actual C native thread tests and repeated simultaneous observations. Real ISRs additionally share the interrupt synchronization domain. |
| IRQ publication versus failed cleanup | An ISR paused after the phase CAS can publish its marker after cleanup sets UnknownActive. That marker cannot restore the phase or permit subsequent consumption/release. | Deterministically paused native interlocked instruction; timeout and mask-failure stress. |
| DPC consumption versus failed cleanup | A consumer paused after checking Observed can still return TRUE after UnknownActive is set. This is a possible CPU wake, not release authority. The owner retains resources and later consumption/reuse fails. | Deterministically paused actual C consumer and consumption/mask-failure stress. |
| Selection while a transfer is selected | Busy check and preparation share DmaCompletionLock; rejection leaves the current tracker untouched. | Source/lock audit and integration guard; full selection function is not host-executed. |
| Timeout versus completion | Failed finalization wins regardless of prior/late observation; no late success can recover unknown ownership. | Native ordered cases and repeated concurrent calls. |
| STOP/REMOVE versus completion | Admission closes and synchronous owners drain before IRQ/DPC/list teardown. The current transfer owner, not a second PnP finalizer, resolves the wait. | Production call-path audit plus fake publication/IRP tests; no real kernel PnP execution. |
| Surprise notification versus ISR/HWInt DPC | A caller that observes the atomic unavailable latch skips its hardware branch; INTEN invalidation is serialized behind an existing INTEN writer. | Native production invalidation/predicate tests and ISR/DPC source guards. An access already past its predicate can still finish; disconnect/flush/drain remain necessary. |
| IRQ after deselection | ISR reads active generation zero and does not hold a transfer pointer; DPC finds no selected transfer. | Source audit. A delayed physical source after a later GO remains untagged and unresolved. |
| Quarantine versus unregister/process close | Both live cleanup classification and unregister/process list access are serialized by TransferMutex. Poison precedes deselection and mutex release; unregister rejects poisoned ownership. | Call-path/lock audit plus independent retention tests; integrated legacy unregister scheduling is not executed. |
| Reuse after success / failure then next acquisition | Success can prepare a fresh legacy generation; stale recorded generations fail. Mask failure leaves a terminal tracker and device latch before releasing the owner mutex. | Native success/reuse and failure tests; stale/new observation stress. |

The tracker contains naturally aligned 64-bit generation fields; interlocked
operations supply CPU atomicity/order, not a transactional update of all fields
and not completion of posted PCI writes. Initialize requires unpublished
storage. Prepare/finalization require the serialized transfer owner; the
completion lock also protects selected pointer/event handoff. An arbitrary
concurrent Prepare versus an old helper invocation is not a supported lock-free
API. GO/active publication excludes old in-flight ISR execution through
KeSynchronizeExecution; software identity still cannot tag a physical source.

Lock/IRQL audit: the owner holds TransferMutex before acquiring the completion
spinlock. DPC event access stays inside that spinlock; ISR never takes it or
dereferences a transfer. Prepare now runs at DISPATCH_LEVEL using only resident
interlocked helpers, without waits, allocation or MMIO. The GO callback runs in
the interrupt synchronization domain with only existing MMIO and atomic state
operations, and is not called while holding the completion spinlock. Teardown
releases that spinlock before acquiring TransferMutex, so the reviewed paths
introduce no reverse nested lock order. The non-atomic DmaUnsafeToFree flag is
protected by owner/list serialization and dispatch/PnP rundown, not by the
tracker atomics alone.

Remaining software qualification gaps: host tests execute the production
tracker and INTEN control helpers, not the complete ISR/DPC/acquisition/
unregister implementation. They do not emulate kernel IRQL, spinlock scheduling,
weak/unsupported caller ordering, generation wraparound or actual STOP/REMOVE.
Device-wide INTEN serialization is defined below; integrated kernel/Verifier
scheduling tests remain necessary. No quarantine or hardware-shutdown policy
was relaxed. Successful mask issue is not a hardware mask acknowledgement or
permission to release hardware-active memory.

### Source-backed lifecycle matrix

These rows describe the live legacy owner and the separate inactive WDM
mapping boundary, not a new hardware state machine. Sources are
`Acquisition.c:LecSelectDmaTransfer`, `LecArmSelectedDma`,
`LecDeselectDmaTransfer`, `LecMarkDmaUnknownActive`, `LecFreeTransfer`,
`LecUnregisterTransfer`, `LecQuiesceDeferredWork`; `DmaCompletion.c` state
helpers; `Ioctl.c` MTT/MAM cleanup; and `Device.c` PnP teardown.

| State/boundary | Owner, allowed operation and transition | Locks / ISR-DPC exposure | MDL/mapping and admission policy |
| --- | --- | --- | --- |
| NeverLaunched | Transfer/list owner may select, arm, unregister or free after no-launch rundown. Prepare requires unpublished or serialized storage. | TransferMutex for live list/owner; DmaCompletionLock for selection/event identity. No accepted active-generation completion. | Legacy MDLs/descriptors may be freed; staged WDM release requires its no-launch gate/rundown. Device admission must also be open before launch. |
| Arming | Selected owner may launch under ISR exclusion or cancel to NeverLaunched only before GO and without uncertainty. An IRQ here forces UnknownActive. | Owner mutex; short completion-lock publication, then KeSynchronizeExecution for GO/active publication. ISR uses metadata, never the transfer pointer. | Retain resources while arming; generic INTEN cannot change bit 0. No concurrent selected owner or unregister. |
| DeviceActive | Selected synchronous owner waits and finalizes; only first matching IRQ records CompletionObserved. Failure becomes UnknownActive. | Owner mutex spans wait/cleanup; ISR atomics, DPC event handoff under completion lock. | No software release. Other allowed INTEN writes preserve bit 0; serialized next acquisition cannot start. |
| CompletionObserved | DPC may signal the matching event; owner masks before FinishCleanup, then deselects. Failed wait/mask still becomes UnknownActive. | DPC and deselection use the same lock; mutex prevents unregister until classification completes. | Legacy PFN success can later free/reuse unpoisoned resources: explicit unverified compatibility exception. WDM mapping release remains forbidden without IdleProved. |
| UnknownActive | Owner poisons transfer before terminal device admission closure and fallible parent notification; late IRQ/cleanup cannot recover. | Owner mutex plus IC for latch/gate, completion atomics for evidence. Queued DPC cannot confer release authority. | Pinned MDLs/descriptors retained; acquisition rejected, ordinary INTEN writes rejected. Only existing clear-only/PnP-zero exceptions while hardware accessible. |
| Quarantined | LecFreeTransfer marks quarantine and rehomes poisoned allocation ownership into independent retention; no free/reset/reuse transition. | Transfer/list serialization and publication anchor rundown; disconnect/flush precede FDO teardown. | Retains transfer, descriptor MDL/storage and user MDLs beyond FDO deletion, including retention on notification failure. No live mapping release is introduced. |
| STOP/REMOVE | Close admission, drain owners, disconnect IRQ, remove/flush DPC and cancel timer before transfer/BAR teardown. Surprise first invalidates hardware. | PASSIVE PnP rundown; completion pointer clears under its lock, list cleanup under mutex; REMOVE also drains remove lock. | Unpoisoned legacy cleanup follows compatibility policy; poisoned resources survive. Software teardown alone never proves PCI idle. |
| Restart attempt | Clean STOP can reopen only after successful START/connection. Same-FDO unknown-active or surprise-unavailable latches cannot reopen or be reset by admission helpers. | IC -> IoAdmissionLock serializes restart against closure/writers. | No IRQ/mask success releases quarantine. New attachment, driver reload or device restart is not proven recovery of retained active DMA. |

The acquisition wait is bounded and nonalertable (five seconds). This review
does not add immediate IRP cancellation or a cancellation-driven hardware
abort: the synchronous IOCTL/remove-lock owner remains until wait and cleanup
return. Kernel cancellation, STOP/REMOVE and interrupt scheduling still need
controlled qualification; source contracts are not an execution of those paths.

## Device-wide INTEN ownership

Source audit and offline validation: 2026-10-13. INTEN is the aligned DWORD
at **BAR0+0x084**, not BAR1+0x084 (MTTRGO). There is no separate Interrupt.c;
ISR/DPC and PnP interrupt teardown are in Acquisition.c. Dynamic family-1
register writes use LecGetBar1Register and cannot alias BAR0 INTEN. Generic
register operations are DWORD aligned, so adjacent byte/word writes cannot
bypass this INTEN route. There is no new mask or shutdown command.

### Complete access inventory

The following are confirmed source callers, not a claim that all have been
observed in an external application trace. Physical INTEN reads/writes now
occur only in InterruptControl.c:LecInterruptOperationSynchronized.
`IC` below means InterruptControlLock plus KeSynchronizeExecution while the
interrupt is connected. Without an ISR, IC's spinlock remains sufficient for
software writers. Ordinary callers enter at <= DISPATCH_LEVEL; the short
synchronized callback runs at the connected interrupt's SynchronizeIrql.
IOCTL owners that take TransferMutex run at PASSIVE_LEVEL.

| Caller / route | BAR/offset; operation | Synchronization / IRQL | Ownership/admission; active transfer; after quarantine |
| --- | --- | --- | --- |
| LecExecuteLegacyMttTransferLocked | 0/084; shadow RMW, physical W: enable bit 0, cleanup clear bit 0 | TransferMutex then IC; PASSIVE -> DISPATCH -> DIRQL | Acquisition-only bit-0 role. Enable requires open gate; cleanup may only clear bit 0 after closure. Active: yes. Quarantine: clear-only exception. |
| LecIoctlAcquireBufferedOneChannel (MAM) | Same as MTT | Same as MTT | Same rules; final cleanup failure still poisons launched resources before deselection. |
| LecIoctlCfDc2110 control 00A0 | 0/084; shadow RMW, physical W: bits 04/10/20 | TransferMutex then IC | Open gate, preserves bit 0 and all other bits. Active: serialized by transfer owner. After quarantine: rejected, including already admitted calls. |
| LecIoctlCfDc2110 response fetch | 0/084; shadow RMW, physical W only when bit 08 changes | TransferMutex then IC | Open gate; existing first-enable-only behavior retained. Active: owner-serialized. Quarantine: rejected. |
| CFDC2190 in LecS65DeviceControl | 0/084; shadow RMW, physical W: bit 02 | IC; ordinary IOCTL context | Open gate; may race an active transfer but cannot change bit 0. Quarantine: INTEN change rejected. Its earlier ERRM write is a separate legacy operation, not a transactional INTEN/ERRM pair. |
| LecSetLegacyRegister -> LecCommitLegacyInterruptMask | 0/084; full physical W and shadow publication, known register-list index 13 | IC; IOCTL context | Full-value request must preserve current bit 0; otherwise DEVICE_BUSY. Active: yes, preserving bit 0. Quarantine: rejected. |
| LecIoctlRegisterWrite, legacy/extended | 0/084; full physical W and shadow publication | IC; IOCTL context, remove/admission rundown | Same ownership rule as SetOneRegister; other BAR/offset accesses unchanged. |
| LecIoctlRegisterRead, legacy/extended | 0/084; physical R, not a cached return | IC; IOCTL context, remove/admission rundown | Read does not modify shadow or establish idle. An already admitted read can finish after ordinary closure while BAR remains valid; unavailable hardware rejects it. |
| LecFillLegacyRegisterEntry | 0/084; shadow R only | Interlocked snapshot; IOCTL context | Legacy type-2 cached query remains cached, no physical read. Already admitted query may finish after closure. |
| LecInterruptService | Atomic unavailable check, then shadow R for early-zero rejection and INTST filtering; no physical INTEN access | Interrupt synchronization domain, DIRQL | Skips hardware if unavailable is observed. Otherwise runs until disconnect; no transfer pointer is used. Quarantine does not itself prove IRQ absence. |
| LecInterruptDpc | No INTEN/shadow access or write; atomic unavailable check gates HWInt MMIO | DPC/completion lock, DISPATCH | Software evidence processing remains possible; unavailable hardware skips the HWInt branch. Never re-enables INTEN or proves idle. |
| LecQuiesceDeferredWork -> LecMaskInterruptsForPnp | 0/084; physical zero W, shadow publication | IC after IOCTL drain; PASSIVE -> DIRQL | Existing mask-only teardown exception, never enable. STOP/normal REMOVE only with accessible BAR. SURPRISE skips it. No hardware-idle conclusion. |
| LecDisconnectInterrupt | Shadow invalidation to zero, no INTEN write | After drain and IoDisconnectInterrupt, PASSIVE | No ISR/writer remains. This is invalidation, not a claim that absent hardware accepted zero. |
| LecS65AddDevice / LecConnectInterrupt / START | Initial zeroed shadow; lock initialization; connect has no INTEN access | Unpublished FDO initialization, PASSIVE | START publishes gate only after successful connection. Physical initial/reset mask is not established by a zeroed software shadow. UnknownActive and surprise-unavailable cannot reopen. |
| LecSetIoctlAdmission -> LecSetInterruptAdmission | Gate state only, no INTEN access | IC spinlock then IoAdmissionLock; <= DISPATCH | Serializes STOP/REMOVE closure against in-flight INTEN writers. Outer IOCTL rundown still protects BAR/interrupt lifetime. |
| LecMarkDmaUnknownActive -> LecQuarantineInterruptAdmission | Terminal device latch/gate only, no INTEN access | TransferMutex then IC spinlock | Transfer poisoned first; latch and gate close atomically against INTEN writers, before outer admission closure and fallible notification. No reopening. |
| SURPRISE_REMOVAL -> LecInvalidateInterruptHardware | Interlocked unavailable latch/gate only, no INTEN access | IC spinlock before outer closure/drain | Later INTEN operations reject even cleanup, PnP masking and physical reads. ISR/DPC skip hardware when they observe the latch; already in-flight access cannot be retroactively undone. |

### Confirmed defects and corrected contract

Previously generic register writes bypassed the shadow entirely. The legacy
commit helper published shadow before MMIO without a device-wide lock; two
writers could leave the final shadow different from the last issued value.
Several callers read shadow outside the commit, so unrelated RMW changes could
be lost and a stale mask could re-enable acquisition bit 0 after cleanup.
Closing outer IOCTL admission did not stop an already admitted writer.

All normal writes, RMW decisions and physical reads now share IC. The ISR
cannot observe the write/shadow publication halfway through that critical
section. RMW starts from the latest shadow under the lock. Generic full-value
writes retain their explicit replacement semantics for non-DMA bits, including
unknown bits; they may not change bit 0 in either direction. Only the two
acquisition paths change bit 0. A stale generic value containing bit 0 therefore
fails after cleanup rather than silently undoing it. These are source-proven
call boundaries; arbitrary future internal callers must not claim that role.

Quarantine and STOP close the INTEN write gate under the same lock as writers.
A write already inside the critical section finishes before closure; a waiter
rechecks and rejects after closure. The only write exceptions are existing
acquisition clear-bit-0 cleanup and PnP's all-zero mask. Neither can enable a
bit. Surprise-unavailable overrides both exceptions and all physical reads.
This does not retrofit device-wide ownership for every other MMIO register:
for example an already admitted CFDC2190 may issue its preceding ERRM write
before its INTEN update is rejected. It cannot use that write to bypass INTEN
ownership. Broader raw-register/command safety remains a separate audit scope.

Lock order is TransferMutex (where already held) -> InterruptControlLock ->
interrupt synchronization lock, or InterruptControlLock -> IoAdmissionLock
for admission changes. No path holds IoAdmissionLock while acquiring IC.
The ISR takes neither ordinary spinlock, and no callback takes a blocking
mutex, allocates, logs or waits. DPC event lifetime remains under
DmaCompletionLock, which is not held when entering IC. PnP drains synchronous
owners before disconnect/unmap, so no admitted helper can outlive its BAR or
interrupt object. Helpers are nonpageable and must not be invoked recursively
from ISR/synchronization callbacks. This uses the documented
[KeSynchronizeExecution IRQL contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kesynchronizeexecution).

### Compatibility and unverified hardware boundaries

IOCTL numbers, input/output layouts, register index 13, and Information=0 on
successful writes are unchanged. Generic non-INTEN controls are not blanket
rejected. Generic INTEN attempts that change bit 0 now return
STATUS_DEVICE_BUSY; callers can preserve that bit and change other bits. The
original register UI can select INTEN, but no supplied runtime trace establishes
a requirement for unrelated software to own acquisition bit 0. This intentional
hardening exception still requires eventual owner/kernel compatibility testing.
The existing control-word masks, receive-enable idempotence, ERRM-before-INTEN
ordering, acquisition enable/cleanup writes and PnP zero-mask values remain.

ABI review against the current target confirms SetOneRegister `0x0022303C`
still accepts its packed 266-byte setter (index at +0x101, value at +0x106),
with index 13 resolving BAR0+0x084. Query `0x00223040` retains its 4-byte size,
266-byte entry and 11,438-byte list output forms; cached type-2 INTEN queries
do not read hardware. Generic read/write `0xCFDC21C0`/`0xCFDC21C4` keep the
4/5-byte read and 8/9-byte write request forms and BAR validation; successful
reads return Information=4 and writes Information=0. BAR1+0x084 remains
MTTRGO, not INTEN. Malformed lengths/alignment/ranges still fail before access.

| Changed caller outcome | Classification |
| --- | --- |
| Generic/setter full-value INTEN changes current bit 0 in either direction, even while idle | Intended safety hardening: STATUS_DEVICE_BUSY, no write/shadow change. |
| Stale full value would restore bit 0 after cleanup | Intended safety hardening: same DEVICE_BUSY rejection. |
| Existing external register UI depends on owning bit 0 | Unverified external caller dependency; no supplied trace establishes it. Not a demonstrated regression or reason to bypass ownership. |
| Normal MTT/MAM acquisition-owned enable/cleanup; non-DMA bit updates preserving bit 0 | Preserved documented software contracts; kernel/application compatibility remains unqualified. |

Already-admitted calls rejected after closure use INVALID_DEVICE_STATE;
dispatch rejects new calls with DEVICE_NOT_READY. No public layout or control
code changed, and no source-demonstrated unrelated ABI regression was found.

Validation failures return failure without changing shadow or issuing MMIO.
Mask failure continues to feed FinishCleanup and retain launched resources.
The actual [WRITE_REGISTER_ULONG DDI](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-write_register_ulong)
returns **void**, not an acknowledged-write status. STATUS_SUCCESS from this
helper means *issued*, not *hardware accepted* or *PCI write retired*.
No guessed readback/barrier/reset sequence or recoverable bus-error status is
invented. Faults on an unexpectedly absent device, reset-era physical/shadow
agreement, durable hardware masking, IRQ provenance and DMA final-host-access
ordering remain unverified. Tests inject invalid mapping/length, not a
fictional recoverable status from the void DDI. The normal legacy success
exception still depends on the unresolved hardware completion premise;
CompletionObserved != IdleProved and live WDM mapping release stays disabled.

Before future live WDM integration, independently establish stopped producers,
stopped descriptor consumption, no new busmaster transactions, terminal
outstanding transactions, drained internal buffers and completed/ordered host
writes, stable until explicit restart. Also require physical IRQ provenance,
adapter-defined completion/coherency handling, logical data/descriptor addresses
within 32 bits, and one release gate joining pinned MDLs, descriptors, mappings,
callbacks and remove-lock/publication rundown. Unknown-active ownership must
survive removal without unmapping; no-launch release APIs cannot be reused for
launched DMA. Recoverable kernel/Verifier and IOMMU qualification remains
required after those contracts exist.

### Post-merge qualification gates

- **PASS (ancestry and project):** PR #10 is merged at `69ad853`; all project
  source/header items exist and `InterruptControl.c` is included exactly once.
  Public ABI headers remain unchanged and the inactive WDM stage has no live
  acquisition, IOCTL or PnP entry point.
- **PASS (Debug and Release builds):** a new Codex execution in a clean Windows
  worktree used SDK/WDK 10.0.28000.0 and MSVC tools 14.41.34120. Debug x64 and,
  after correcting Release manifest generation, a clean Release x64 rebuild
  both passed with 0 warnings and 0 errors. The SYS files are AMD64 native
  images with `DriverEntry` and only `ntoskrnl.exe`/`HAL.dll` imports.
- **PASS (offline regression):** all Dry suites pass 394/394 functional checks.
  Default stress passes 5,000 DMA and 1,000 INTEN iterations; expanded stress
  passes 50,000 DMA and 10,000 INTEN iterations. The combined established
  comparison is 66,000/66,000. This remains software-only evidence.
- **DEVELOPMENT STATE:** merged and offline-qualified. This is not authorization
  to install, load or deploy the driver. Future execution must follow the
  [kernel-runtime qualification plan](kernel-runtime-qualification.md) under
  its independently authorized gates.
- **REMAINING QUALIFICATION:** validate kernel
  IRQL/interrupt-lock behavior, cancellation/rundown and legacy caller bit-0
  compatibility in a controlled environment before deployment. The predicate
  cannot cancel an ISR/DPC already past its check; broader already-admitted
  non-INTEN MMIO remains outside this focused ownership change.
- **HARDWARE BLOCKED:** physical IRQ provenance, producer/descriptor stop,
  initiator inactivity, outstanding transaction/buffer retirement and ordered
  host-memory visibility remain unproved. The completed research
  [owner handoff](https://github.com/Battlecake91/lecroy_wr6k_64bit_driver/blob/6bff97ff473035cd7b5a0b6bd51cf48cfe82d364/docs/handoffs/fpga-dma-implementation-owner-handoff.md)
  and [missing evidence](https://github.com/Battlecake91/lecroy_wr6k_64bit_driver/blob/6bff97ff473035cd7b5a0b6bd51cf48cfe82d364/docs/handoffs/fpga-dma-missing-evidence.md)
  are references, not merged implementation or release authority.

See [reproducible Windows validation](regression-testing.md) for the toolchain
inventory, actual test counts and commands. PR #10 is merged and Debug/Release
build-qualified, but is not Driver Verifier, HLK, hardware or deployment
qualified, nor a solution to physical DMA shutdown. PR #9 remains separate
and unmerged.

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

Batch 11 also adds the separate inactive `DmaV3Transfer` owner for the exact
transactional DDI chain intended by a future live path. Fake-DDI qualification
covers transfer-info discovery, transfer-context initialization, synchronous
channel allocation, complete `MapTransferEx`, exact SG byte coverage, 32-bit
device-address validation, common-buffer descriptor encoding and the no-launch
flush/free path. Partial mappings are rejected before any possible GO. A
failed cleanup flush is fail-closed: the mapping, channel, pages and adapter
context are retained under irreversible quarantine. This module contains no
hardware-start operation and is not called from production IOCTL, acquisition,
PnP or power dispatch.

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

### Process-close ownership boundary

Legacy registered transfers are keyed by opaque token plus process identity.
The current CLOSE path nevertheless calls `LecReleaseTransfersForProcess` on
every file-object close from that process. With two open handles in one process,
closing either handle can therefore unlink and free every idle registration
that the other handle can still address. Selected and `DmaUnsafeToFree`
transfers remain protected, so this is not a route around the active-DMA
quarantine; it is a lifetime and compatibility gap for idle registrations.

The source comment describes cleanup on the last process close, but no
per-process open count or per-file ownership exists. Changing token ownership
to file objects would prevent cross-handle use and may break the legacy ABI.
The narrow ABI-compatible behavior is a per-process open-reference table.
CREATE must record the originating process identity on the file object and
increment that process entry; CLOSE must decrement that recorded identity and
retire inactive registrations only at the last reference. CLEANUP alone must
not retire process-shared registrations. STOP, surprise removal and REMOVE
must still release only registrations proven inactive; `UnknownActive`
mappings remain quarantined. A 19/19 deterministic characterization model now
covers first/final CLOSE, CLEANUP, PnP teardown and quarantine alongside the
event/interface cases. A separate 22/22 process/file-object model expands the
proposed rule to transactional CREATE rollback, explicit file association,
two handles in one process, independent processes, both serialized
registration/CLOSE outcomes, duplicate CLOSE, surprise removal and
CREATE/removal ordering. It retains `UnknownActive` after final CLOSE and
teardown because no tested software event proves physical DMA retirement.

The production implementation now records a nonpaged file context on each
successful CREATE. The context binds the file object to the creating process
identity; all transfer registration, lookup, launch and unregister paths use
that recorded identity rather than the thread that happens to issue the IOCTL
or CLOSE. CREATE association, CLOSE detachment, last-handle detection and
transfer-list changes are serialized by `TransferMutex`. A failed CREATE does
not publish a context. The final CLOSE for one process unlinks only inactive,
non-current registrations for that process and frees them after dropping the
mutex. Earlier same-process closes preserve shared registrations. A CLOSE in a
different process context still uses the recorded owner.

REMOVE clears every remaining file association only after remove-lock rundown;
STOP and surprise removal retain file associations while applying the existing
transfer teardown. No cleanup path frees a current or `DmaUnsafeToFree`
transfer. `UnknownActive` therefore remains irreversible and independently
retained exactly as before. The obsolete every-CLOSE
`LecReleaseTransfersForProcess` helper was removed so the unsafe behavior
cannot be reintroduced accidentally.

Offline qualification builds Debug and Release x64 with zero warnings and zero
errors. Source/ABI contracts pass 63/63, the process/file model 23/23, the
combined lifetime audit 19/19, and the complete Dry suite passes. Default
stress passes 5,000/5,000 DMA races and 1,000/1,000 INTEN iterations; expanded
stress passes 50,000/50,000 DMA races and 10,000/10,000 INTEN iterations. This
is production-source qualification, not deployment qualification: the changed
driver has not been signed, installed, loaded or exercised on hardware.

### Independent review of PR #27

PR #27 head `1120687c181e85ade70f91d656d127ef0533d7a6` implements the
previously modeled process-shared ownership rule without changing token or
IOCTL wire formats. Each successful CREATE allocates a nonpaged file context,
records the originating process and publishes the context under
`TransferMutex`. Failed allocation or association returns an error without a
list entry. CLOSE resolves the recorded association rather than the current
thread/process, removes exactly that file context and retires safe inactive
registrations only when no other context for that process remains.

The source review found no production defect in the reviewed revision:

- CREATE, CLOSE and remove-time context rundown are covered by the remove lock;
- file-context and transfer-list mutations share `TransferMutex`, while frees
  and publication retention occur after releasing it;
- a CLOSE for one of several file objects cannot retire registrations while
  another context for that originating process remains;
- I/O-manager CLOSE ordering plus IOCTL admission/rundown prevents a file
  context from being freed underneath an outstanding request on that file;
- REMOVE waits for dispatch rundown, releases or independently retains every
  transfer, then clears and frees remaining file contexts before device
  deletion; and
- `CurrentTransfer` and `DmaUnsafeToFree` checks preserve live and
  `UnknownActive` ownership. Quarantined storage still moves to the independent
  publication owner and is never unlocked by final CLOSE or REMOVE.

No public IOCTL constant, packed record, buffer-length rule or token-sharing
rule changed. The residual assumptions are WDM/I/O-manager lifetime contracts,
not evidence of physical DMA idle. The source remains uninstalled and cannot
be called hardware-qualified.

Independent offline reproduction at the exact PR head produced zero-warning,
zero-error Debug and Release x64 builds. Debug SYS SHA-256 is
`D12C4822719193F0F4EDAAFBEEAB3760F351A6A95C3B901BE9B7E115044EB496`;
Release SYS SHA-256 is
`C2017E20F5E52DE0711B550E02567E181CCF78FFD227B812610B2DEAD5D4E325`.
Both are AMD64 native images with `DriverEntry` and only `ntoskrnl.exe`/`HAL.dll`
imports. Focused process/file tests pass 23/23, combined lifetime tests 19/19,
PnP IRP tests 10/10, publication tests 38/38, source/ABI contracts 63/63,
default DMA/INTEN stress 5,000/1,000 and expanded stress 50,000/10,000.

## Remaining P0 work, no shortcuts

- Obtain real board documentation or a verified, recoverable bench measurement of MAM and MTT abort/idle, physical completion ordering and safe reset/readback. Do not invent MMIO commands from guesses or treat the old x86 driver's behavior as a proof of hardware safety.
- Validate the remaining untagged physical-IRQ attribution premise, completion ordering and DPC/STOP/REMOVE behavior under Driver Verifier and a recoverable fault-injection target. The deterministic host tests cover software generations and pointer rundown, not the board's IRQ origin or final PCI transaction.
- Replace direct PFN-derived bus addresses (`MmGetMdlPfnArray` and PFN shift) with an appropriate Windows DMA adapter/mapping model for supported DMA-remapping/IOMMU configurations. Retain the hardware 32-bit address and descriptor format limits; DMA mappings must not be released before hardware inactivity is proven.
- Replace the emergency pinned-memory quarantine with verified abort/drain/recovery ownership, including what happens across driver remove/reinstallation. It is not a viable final production resource-management model.

No artificial nonzero IRQ injection, licensed Dallas writes, hazardous DMA timeouts or unverified register writes should be performed on the only working WR6k.

## Batch 7 unsigned integrated candidate

The integration branch in PR #31 contains the exact reviewed PR #27 driver and
include trees. The Batch 7 package workflow rebuilt those trees without signing
and froze Release SYS SHA-256
`D236EC3809E02BD0DA0BC3A679A48E346298A1076393BA5A8812794956A70646`.
The full workflow passes 397/397 functional checks and 66,000/66,000 combined
DMA/INTEN stress iterations.

Relative to the currently installed signed diagnostic package, the intentional
source changes are limited to process/FileObject DMA ownership and its lifetime
support: CREATE association and rollback, last-close retirement, concurrent
registration/close serialization and remove-time context rundown. Public IOCTL
values and packed wire layouts do not change. PnP START/STOP hardware ordering,
the five interface classes, interrupt connect/disconnect sequencing and the
irreversible `UnknownActive` quarantine remain unchanged. No IOCTL, acquisition
or PnP caller can enter the dormant WDM-v3 DMA mapping stage.

Binary byte differences also include ordinary rebuild metadata, absolute PDB
path/RSDS identity, the new INF version and absence of the installed binary's
Authenticode certificate table. They must not be mistaken for additional
runtime behavior. This candidate remains unsigned, unstaged, uninstalled and
hardware-unqualified. Required next evidence is recovery establishment,
separately authorized signing and installation, the bounded R0/R1 baseline,
captured PnP lifetime, non-DMA Verifier and only later acquisition/DMA work.

## Combined runtime branch integration

The PR #27 driver tree was merged into the Batch 6 runtime/tooling stack on an
isolated integration branch. A direct tree comparison confirms that the merged
`driver/` directory and process-lifetime model are identical to reviewed PR #27
head `1120687c181e85ade70f91d656d127ef0533d7a6`; only later launcher, contract
and canonical-document changes surround it.

Focused validation passes 23/23 process/file ownership cases, 19/19 combined
lifetime cases, 10/10 PnP IRP cases, 38/38 publication cases and 64/64 source
contracts. The already-qualified exact driver source also retains its prior
50,000 DMA and 10,000 INTEN expanded stress evidence, so those unchanged binary
paths were not rerun merely for documentation integration.

The captured restart of the currently installed pre-fix diagnostic package
showed clean REMOVE/START lifetime and zero retained transfers, but it does not
hardware-qualify this new ownership implementation and cannot prove physical
DMA idle. A future combined candidate requires a distinct package version,
signing authorization, installation authorization and repetition of the bounded
runtime matrix before any acquisition or DMA work.

## Batch 11 transaction and teardown contract

The compiled `DmaTransaction` model makes release ownership explicit across
the entire software-verifiable path. Each request has one monotonically
increasing generation and one process/FileObject identity. Mapping completion
may arrive after pre-GO cancellation; it is then owned only long enough for the
never-launched release path. A stale generation, duplicate completion or IRQ
during Arming cannot complete the current request.

Pre-GO cancellation, descriptor failure, QUERY, STOP or power-down may release
only after the mapping callback has retired and no publication user remains.
After any possible GO, timeout, power loss, surprise removal or uncertain
cleanup transitions to `UnknownActive`; mapping, MDLs, common table, adapter and
PDO ownership remain retained. `CompletionObserved` is deliberately distinct
from `IdleProved`. The latter still has no production caller because no
physical idle predicate exists.

## Batch 12 live preparation lifetime

The one-channel `CFDC2138` dispatch now enters the no-launch transaction while
the dispatch RemoveLock, active-IOCTL count and `TransferMutex` are all owned.
The associated FileObject and recorded creator PID are nonzero transaction
identities; the registered source MDL chain is already locked and therefore
does not depend on the creator's current process address space. The per-PDO
publication reference survives every blocking DMA DDI.

`GetScatterGatherListEx` uses the synchronous no-callback contract. There is
therefore no callback that can outlive the caller, IRP or FileObject in this
selected path. Cancellation is sampled before mapping and again before Arm.
Every pre-GO exit cancels the software transaction, frees the Windows mapping
once and leaves the persistent registration intact. Mapping-release failure
quarantines the parent instead of guessing ownership. CLOSE/unregister cannot
free the source MDLs while preparation owns `TransferMutex`.

START allocates the adapter and 0x33000-byte common table before interface and
IOCTL admission. QUERY_STOP/QUERY_REMOVE close global IOCTL admission and the
DMA parent before forwarding; rejected queries restore both. Device-power IRPs
use a nonpaged completion context and publication reference, never a blocking
wait. STOP, REMOVE and surprise removal record IRQ/DPC/timer software rundown
before releasing only no-launch mappings. None of those steps is a physical
idle claim after a possible GO.

The PnP staging parent now separates PnP generation from power generation and
keeps new mapping admission closed in QUERY, PowerDownPending, Dx and failed
resume. These are offline admission primitives, not live POWER-IRP wiring.
Live system-power policy, D0/Dx hardware save/restore, explicit v3 channel/map/
flush/free orchestration and physical abort/drain remain deployment blockers.

## Batch 13 bounded no-launch runner

`lecdiag no-launch-prepare` allocates one 4-KiB destination plus the legacy
four-byte registration trailer below the 32-bit user-VA ABI ceiling. It issues
exactly one `CFDC2124`, one 15-byte `CFDC2138` and one `CFDC2128`. Success means
that `CFDC2138` returns Win32 `ERROR_NOT_SUPPORTED` with zero output, followed
by successful unregister. The repository launcher gives the child five
seconds, uses a fresh evidence directory and refuses identity mismatch or
reuse. Its deterministic Win32 mock proves the exact three-call sequence.

This runner is not an acquisition runner. It cannot accept successful
`CFDC2138`, alternate buffers or retries, and it never authorizes GO.

The installed Batch 14 binary failed its single Batch 15 attempt at
registration with Win32 `ERROR_NOT_SUPPORTED`: production `CFDC2124` still
built the legacy PFN table and rejected either descriptor storage or a source
page above 4 GiB. The development fix confines that table to the unsafe
compile branch, leaving only the locked MDL chain for WDM-v3 preparation. The
fix is offline-built and tested but is not installed; another live attempt
requires a newly signed and separately authorized replacement package.

## Batch 16 MDL-only registration status

The installed `oem104.inf` production path no longer calls the legacy PFN
descriptor builder from `CFDC2124`. Registration locks and owns the MDL chain,
records its process-owned token and byte count, and leaves device DMA-logical
address creation exclusively to the later WDM-v3 mapping API. The safe
`CFDC2138` path validates `DataBytes`; it does not require legacy
`TotalDwords`, descriptor storage or CPU physical addresses.

Preparation and unregister both hold `TransferMutex`. A successful no-launch
preparation returns only after the never-launched mapping is released. A
release uncertainty irreversibly marks the transfer and DMA parent unsafe, so
unregister returns busy instead of unlocking pages. No asynchronous mapping
transaction survives a successful no-launch return. These are software and
source guarantees, not physical bus-idle proof.

The first Batch 16 live no-launch wrapper failed its independent capture-ready
gate before the repository runner was invoked. DebugView buffered the selected
raw-log path until exit; the final log contains independently attributable
kernel messages, but there is no device-attempt marker and no `CFDC2124` or
`CFDC2138` trace. Consequently MDL-only registration and WDM-v3 mapping remain
unqualified on real hardware. No retry was made.

## Batch 17 live never-launched lifetime evidence

One authorized live transaction exercised the intended pre-GO lifetime. A
4-KiB registration remained owned by the process/FileObject while preparation
held its serialized path. The real WDM-v3 mapping covered the request with one
SG segment; two descriptors were built, publication reached the closed launch
gate, and the trace records `released=1` before the request returned
`STATUS_NOT_SUPPORTED`. No asynchronous mapping owner survived the return and
no `UnknownActive` transition was observed.

The live diagnostic tool then stopped before explicit unregister because its
failed Win32 call retained a stale byte-count variable. CLOSE followed. Source
inspection establishes that final owner-handle cleanup removes an inactive
registration when it is not the current transfer and the transfer is not
unsafe to free. The mapping release is directly traced; final registration
cleanup is supported by the exact CLOSE control path rather than a dedicated
unregister trace. The tool observation has been corrected offline without
repeating the hardware operation.

This closes only the `NeverLaunched` lifetime row on the tested hardware.
After any possible GO, `UnknownActive` remains irreversible because neither the
driver nor the available hardware evidence can prove the last descriptor fetch
or PCI write has retired. Pages, mappings and common descriptor storage must
remain retained until a separately established physical-idle contract exists.

## Batch 19 GO gate result

The installed `oem105.inf` development package passed installation and the
complete safe R0/R1 matrix. No GO occurred. Immediately before the authorized
experiment, source review established that the installed kernel boundary did
not enforce the exact 16-byte request, one-SG-segment descriptor shape or one
attempt per loaded FDO. The live test therefore stopped before creating a
launch-attempt marker. There is no post-GO owner, mapping retirement or
quarantine transition to classify for this batch.

The offline correction consumes an atomic per-FDO latch only after mapping and
descriptor validation and before the first acquisition MMIO. It requires one
data descriptor plus the terminator. This closes accidental alternate requests
and repeats for a future first experiment; it does not alter the post-GO proof
boundary. A successful IRQ/DPC/INTEN cleanup remains practical normal-operation
evidence, not formal proof that the final physical PCI write retired. Any
timeout or ambiguous cleanup remains terminal `UnknownActive` quarantine.

Batch 20 expanded this validation to the actual single SG element, exact
descriptor payload/address, common-buffer alignment and 32-bit range, and the
terminator. The signed candidate was not installed: the captured launcher
stopped before PnPUtil dispatch. Consequently no mapping entered DeviceActive,
no latch was consumed by hardware execution, and there is no new retirement or
UnknownActive state to classify.
