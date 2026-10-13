# Current project status

## Build and software regression

- Owner-reported Windows validation on 2026-10-13 at reviewed PR #10 HEAD
  `07683fa6ea293836ffb9cb656f6728084ac924e0` used SDK/WDK 10.0.28000.0
  and MSVC tools 14.41.34120. The owner ran
  `scripts/build-driver.ps1 -Configuration Debug -BuildLecdiag -BuildLecwatch`
  and `scripts/test-driver.ps1 -Mode Dry`: the Debug x64 kernel driver built
  with 0 warnings and 0 errors, `LecS65AcqDrv.sys` was generated, and x64
  `lecdiag.exe` and `lecwatch.exe` passed their build checks. The same owner
  console log reports 388/388 Dry functional checks, 5,000/5,000 DMA race
  iterations and 1,000/1,000 INTEN RMW iterations passing. This evidence was
  supplied by the owner; it was not an independent Codex build execution.
  Release x64, driver loading, Driver Verifier, HLK and hardware qualification
  were not performed.
- The hardware-independent Dry regression covers source/ABI contracts, DMA
  descriptor layout, mapping ownership, the quarantined asynchronous SG bridge,
  the inactive synchronous v3 no-launch stage, the live PnP publication
  lifetime anchor, actual IRP/remove-lock forwarding helpers and live DMA
  completion-evidence transitions. See
  [regression testing](regression-testing.md) for the current counts.
- A separate Codex-run offline check on 2026-10-13 used
  `scripts/test-driver.ps1 -Mode Dry -SkipBuild` at the same reviewed source
  baseline. Result: source/ABI 46/46, layout 13/13, ownership 17/17,
  asynchronous SG 27/27, synchronous v3/PnP lifetime 149/149 and live PnP
  publication 38/38 plus IRP/remove-lock 8/8 and DMA completion 39/39;
  plus deterministic interleavings 2/2 and native INTEN checks 49/49;
  overall 388/388 PASS, with 5,000/5,000 DMA stress and 1,000/1,000 INTEN
  RMW stress iterations passing. Native C tests use `/W4 /WX`; this rerun
  skipped the kernel build and does not execute kernel acquisition/rundown.

## DMA/PnP staging

- The active acquisition path still uses PFN-derived DMA addresses; its
  software completion attribution and rundown are now generation-checked.
  Adapter-mapped staging is not reachable from acquisition, IOCTL
  or live PnP paths. Live PnP reaches only the software lifetime-publication
  layer; it cannot start the adapter stage or create mappings.
- The synchronous v3 stage uses `GetScatterGatherListEx` with
  `DMA_SYNCHRONOUS_CALLBACK` and no execution callback. It has no launch API.
- No-launch mappings now have centralized adapter/PDO ownership, unique transfer
  contexts, explicit `FreeAdapterObject(DeallocateObject)` cleanup, STOP/drain
  rundown, token serialization, MDL/map-register validation and WR6k descriptor
  capacity derived from the actual common-buffer length. The owner has an
  explicit construct/init/stop/destroy lifecycle, so failed repeated init does
  not overwrite live mappings or reinitialize an active lock.
- An inactive `LECS65_DMA_PNP_STAGE` now owns one adapter context, one sync
  owner, parent-call rundown, START generations, STOP/SURPRISE/REMOVE states,
  ordered IRQ/DPC/timer software-quiescence flags and an authoritative atomic
  unknown-active latch. It is allocated only through a one-time factory;
  destruction requires empty stopped/removed state plus external unpublication
  and caller rundown. Blocked GetEx completion is retained after quarantine;
  notification after an irreversible no-launch release commit is reported as
  late and prevents all remaining cleanup.
  It prevents multiple contexts within one parent. AddDevice now publishes one
  separately allocated wrapper and parent per FDO/PDO, with its own PDO
  reference and two-level remove-lock/internal-user rundown. Clean REMOVE
  destroys it after unpublication; unknown-active legacy transfer ownership is
  rehomed into the wrapper so the parent, PDO, pinned MDLs and descriptors can
  remain retained beyond FDO deletion. This is restart-only containment, not
  production recovery.
- Duplicate publication and legacy-transfer retention are now rejected by
  explicit ownership state. Unknown-active handling closes admission before
  parent notification, and transfer ownership commits to the independent
  retained list before that notification can fail. Production IRP forwarding
  is software-tested for synchronous/pending/error/power completion and exact
  remove-lock release.
- Live legacy DMA completion now records explicit 64-bit generations and
  `NeverLaunched`, transient `Arming`, `DeviceActive`, `CompletionObserved`,
  `IdleProved`, `UnknownActive` and `Quarantined` evidence states. IIMCL/GO and
  post-GO active publication run under ISR exclusion. A pre-GO ISR poisons the
  transfer rather than satisfying it; a fast post-GO IRQ is processed after
  publication. The ISR cannot infer idle, an old recorded generation cannot
  signal a later transfer, a stale consumer cannot clear current evidence, and
  a DPC holds the completion lock while touching the selected event. A queued
  MAM request rechecks terminal DMA failure after serialization and before
  MMIO.
- Successful legacy acquisition behavior remains unchanged: a matching
  physical IRQ produces `CompletionObserved`, not `IdleProved`. Later release
  of its PFN-based resources therefore still depends on an unverified hardware
  IRQ-to-final-memory-access ordering premise. WDM mapping release remains
  forbidden on that evidence alone.
- Launched cleanup now fails closed on interrupt-mask failure before
  deselection; duplicate IRQs cannot republish consumed completion evidence.
  Busy transfer selection cannot reset the current tracker. See the
  [normal-success audit](dma-lifetime-and-timeout.md#normal-success-cleanup-audit-and-implementation-boundary)
  for compatibility limits and future live-WDM prerequisites.
- The final PR #10 integration review found one additional gap: surprise
  removal's unavailable latch did not gate ISR or HWInt DPC MMIO. It is now a
  resident interlocked LONG predicate checked before both hardware branches.
  Native tests cover latch/writer ordering and irreversible admission closure;
  source guards cover the ISR/DPC call sites. The predicate cannot cancel work
  already past its check, and does not replace disconnect/flush/drain or prove
  absent-device safety for all other raw MMIO.
  Deterministic native tests demonstrate that an already
  in-flight marker publisher/consumer can return TRUE after uncertainty wins;
  that result is not release authority. Serialized owner and event lifetime
  locking remain prerequisites. Integrated kernel scheduling remains
  unqualified. Device-wide INTEN control now serializes full writes, RMW,
  physical reads and admission closure, including already admitted writers.
  Generic controls must preserve acquisition bit 0; only acquisition may
  enable/clear it. Quarantine permits only the existing owner clear-bit-0
  and PnP zero-mask exceptions. Surprise-unavailable blocks even those and
  physical reads. Native helper tests use MMIO/lock doubles; hardware mask
  acceptance and initial/reset shadow agreement remain unverified. See the
  [INTEN access inventory](dma-lifetime-and-timeout.md#device-wide-inten-ownership).
- Integration target `82b4c4e` is an ancestor of the reviewed implementation
  (0 target-only / 4 PR commits at `07683fa`);
  no textual conflict or missing project source/header item was found. The
  source-backed lifetime matrix, legacy DEVICE_BUSY compatibility classification
  and remaining decision gates are in [DMA lifetime](dma-lifetime-and-timeout.md).
  PR #10 is ready for controlled development-branch integration but remains
  unmerged and is not deployment-ready. Debug x64 is owner-build-qualified at
  the reviewed HEAD; Release x64 and kernel/runtime qualification remain open.
  Research PR #9 at `6bff97f` remains separate; no research merge or live WDM
  activation occurred.
- ISR serialization does not prove that a physically pending untagged source
  originated after GO; hardware IRQ provenance and final PCI transaction
  ordering remain unverified.
- Unknown-active hardware DMA remains quarantined. No software test establishes
  physical WR6k bus-idle, safe removal of an active mapping or complete real PnP
  teardown.

## Hardware status

- No hardware, driver installation/reload or XStream testing was performed for
  the current lifetime-hardening changes.
- The established real-PCI safe ABI baseline remains 9/9 PASS and the last
  established hardware regression milestone remains 11/11 PASS.
