# Current project status

## Build and software regression

- Windows x64 Debug driver and `lecdiag` build successfully with the installed
  WDK 10.0.28000.0 toolchain.
- The hardware-independent Dry regression covers source/ABI contracts, DMA
  descriptor layout, mapping ownership, the quarantined asynchronous SG bridge,
  the inactive synchronous v3 no-launch stage, the live PnP publication
  lifetime anchor, actual IRP/remove-lock forwarding helpers and live DMA
  completion-evidence transitions. See
  [regression testing](regression-testing.md) for the current counts.
- Current result: source/ABI 43/43, layout 13/13, ownership 17/17,
  asynchronous SG 27/27, synchronous v3/PnP lifetime 149/149 and live PnP
  publication 38/38 plus IRP/remove-lock 8/8 and DMA completion 37/37;
  overall 332/332 PASS, zero failures/skips.
- Offline FPGA/MCU analysis regression: 18 C166 (14 synthetic, 4 private),
  45 routing, 52 MMIO, 65 acquisition (10 synthetic, 21 private configured-logic,
  24 U21 SMT, 9 timebase SMT, 1 cross-FPGA SMT), 125 PCI DMA (25 synthetic,
  54 private configured-logic, 46 private SMT), and 52 synthetic SMT tests:
  **357 distinct tests PASS** (159 synthetic, 198 private-firmware), zero
  final failures/skips. The full runner passed355 tests; the final receive
  rerun passed12 (ten overlap, two additional publication checks),
  plus decoder self-test and private frame calibration,
  50 native architecture checks and 19 PCI local-state equations PASS.
  The corrected U38 START=N2/SACQ=M1 mapping, physical configuration chain,
  sparse C161 loader/flash-offset correlation, dual SR/REV primitive semantics,
  local START/SACQ cones and buffer-admission limitations are documented in
  [FPGA analysis](pci-fpga-firmware-analysis.md#u38-identity-and-timebase-shutdown).
  Stream1-to-U38 assignment is strongly inferred, not a live-image assertion.
  [PCI DMA retirement analysis](pci-dma-retirement-analysis.md) adds exact
  SGTA/IIMTC/two-word capture maps, 15 local equations, full pointer/master-address
  equations, conditional read/write C/BE and all32 AD alternatives, and staging
  pointer increment/rewind/retention proofs. HEX/LH/LV owner qualification resolves
  all9,150 routes in the control-root report, including the named payload mux.
  The handshake extension adds19 local truth tables, exact issue/rollback and
  two-bit prefetch/four-bit availability equations, delayed-accept counter CEs,
  eight receive-pointer inequalities and explicit PCI output-enable/CE limits.
  Twelve focused bound2/4 scenarios have consistent bases, eleven SAT and one
  bounded UNSAT, zero unknown/timeout results. Q=0/no issue locally excludes
  rewind under mode1/A0; IRQ with Q nonzero is only a runtime-cut candidate.
  Token observers never constrain hardware. Owned acceptance, global word
  conservation, IRQ-after-every-word and bridge visibility remain Unknown.
  Ten additional bound1 runtime diagnostics yield nine SAT and
  one UNSAT with ten consistent bases and zero solver-unknown results; all
  are Inconclusive for physical shutdown, with 218 symbolic boundaries.
  A full-network bound2 IRQ query timed out; its actual-register cone is SAT
  only as a retained-state candidate with unknown external cuts. Final-phase
  ordering and host visibility remain separate unproved obligations.
  The new initialized transitive-state closure (1,513 FFs,42 memories) starts
  with configured INIT/reset and four actually accepted host writes. A single
  conditional execution reaches descriptor address/command at35, external
  read accepts at37/38, delayed internal A at38/39, and count1/address0x200000
  capture. No internal runtime FF cut or guessed PCI_CE is used. Two separate
  whole-prefix environments (bound40 stalled fetch and bound52 mapped memory
  with no remote edges) yield9 SAT/3 UNSAT, zero timeouts/unknown results; neither
  can raise IRQ in its bound. No payload-completion/IRQ safety proof follows.
  All six DMA-quiescence obligations and stable restart exclusion remain
  Unknown. No new driver build or hardware test was performed for this offline
  analysis. PR #9 remains draft, open and unmerged.
  The [receive-to-payload closure](pci-dma-retirement-analysis.md#receive-to-payload-evidence-closure)
  adds15 distinct regressions: all32 input muxes, six pointer-bank contracts,
  periodic modulo8/modulo4 publication (not ACK handshakes), threshold224,
  C+W read lookahead, pre-edge DOB staging and reset-retained RAM. Snapshot
  coherence requires clock/hold/setup evidence; modulo counters do not prove
  valid occupancy, external acceptance, physical retirement or safe release.

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
  The source-backed normal-success audit now covers MDL/page/descriptor free,
  software transfer ownership, DPC handoff and later-generation reuse. Active
  PFN cleanup allocates/releases no adapter map registers. IRQ-mask failure
  after an observed wait and untagged late-source attribution are explicit
  implementation-owner review items; production behavior is unchanged.
  The defensive [review matrix](dma-lifetime-and-timeout.md#defensive-review-and-implementation-owner-decisions)
  records source facts, unproved premises, proposed containment, tests and
  compatibility impact. New source guards reject production idle-hook callers
  or IRQ-to-stage release dependencies; native tests cover all seven release
  phases and irreversible quarantine. No new production lifetime defect was
  established. This does not resolve the legacy success exception.
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
