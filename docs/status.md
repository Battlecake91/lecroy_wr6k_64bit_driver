# Current project status

## Build and software regression

- Development merge commit `cb5010a4712f25de7c56d23d0d7502683542058c`
  retains the Phase 2B-qualified driver/include trees plus the Phase 1, Phase
  2A and Phase 2B evidence.
- A clean isolated Windows worktree at Phase 2B commit
  `aba7ba35ab75cb3b1e20935806eb86376a790726` was validated on 2026-10-17
  with SDK/WDK 10.0.28000.0 and MSVC tools 14.41.34120. Its driver/include
  trees are identical to `cb5010a`. Debug x64 and Release x64 each built with
  0 warnings and 0 errors. The Debug SYS is
  104,960 bytes with SHA-256
  `803E3A0FA0040F644410030126F01780C503C10619B260B891143BFE46D57602`;
  the Release SYS is 76,288 bytes with SHA-256
  `B858D8E1F8228617138626AF6FBF3099D41E4D0C8F80ED56048886C58E16C0FE`.
  x64 `lecdiag.exe` and `lecwatch.exe` also built successfully.
- Both Debug and Release SYS files report AMD64 machine `0x8664`, native
  subsystem, entry point `DriverEntry`, and imports limited to `ntoskrnl.exe`
  and `HAL.dll`. `DriverEntry` is the PE entry point and is not required to be
  an export.
- The merge from the previously qualified `9c97e49c` tree changes only
  documentation and the offline five-interface source contract. `driver/` and
  `include/` are unchanged, so the public IOCTL ABI and runtime implementation
  did not change. The exact five-interface set, absence of live WDM v3 entry,
  irreversible `UnknownActive` handling, device-wide INTEN ownership and
  surprise-removal hardware gates all pass their merged-head contracts.
- The hardware-independent Dry regression covers source/ABI contracts, DMA
  descriptor layout, mapping ownership, the quarantined asynchronous SG bridge,
  the inactive synchronous v3 no-launch stage, the live PnP publication
  lifetime anchor, actual IRP/remove-lock forwarding helpers and live DMA
  completion-evidence transitions. See
  [regression testing](regression-testing.md) for the current counts.
- The package-workflow offline check on 2026-10-17 used
  `scripts/test-driver.ps1 -Mode Dry -SkipBuild`. Result: source/ABI and
  package-workflow contracts 52/52,
  layout 13/13, ownership 17/17,
  asynchronous SG 27/27, synchronous v3/PnP lifetime 149/149 and live PnP
  publication 38/38 plus IRP/remove-lock 8/8 and DMA completion 39/39;
  plus deterministic interleavings 2/2 and native INTEN checks 49/49;
  overall 394/394 PASS, with 5,000/5,000 DMA stress and 1,000/1,000 INTEN
  RMW stress iterations passing. Source contracts prevent recurrence of the
  Release manifest mismatch and require the exact five-interface compatibility
  set. An expanded run passed 50,000/50,000 DMA
  race iterations and 10,000/10,000 INTEN RMW iterations. The default and
  expanded runs therefore reproduce the established 66,000/66,000 stress
  comparison. Native C tests use `/W4 /WX`; these are software-only tests and
  do not execute kernel acquisition/rundown or prove physical DMA completion.
- Cross-worktree comparison confirms that current linker outputs are not
  bit-reproducible: PE timestamps, CodeView GUIDs and embedded absolute PDB
  paths differ even when the driver source and toolchain are unchanged. A
  runtime package must therefore be built once, hash-frozen and promoted as an
  immutable set; a later rebuild is a different artifact unless deterministic
  linking is separately implemented and qualified.
- Phase 2C now has one frozen, locally test-signed Release x64 candidate from
  clean workflow commit `5cc0d792` and the exact reviewed `cb5010a` driver and
  include trees. The elevated 64-bit Windows PowerShell 5.1 launcher completed
  once without retry, using SDK/WDK 10.0.28000.0, MSVC 14.41.34120,
  `DriverVer=10/17/2026,0.2026.1017.1` and approved certificate thumbprint
  `4296FA2E355613DF0D5CE6F24D0898E00B706472`. Both configurations and tools
  built, 394/394 functional checks and 66,000/66,000 stress iterations passed,
  and InfVerif, Inf2Cat, SYS/CAT Authenticode, local chain, catalog membership
  and signed SYS/PDB identity all verify. Frozen hashes are INF
  `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783`,
  CAT `C904CCDA2A7591435F8CFB79C6728D4A44229676CBA31E21EB1353264173C091`,
  signed SYS
  `F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`,
  matching PDB
  `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774`
  and manifest
  `CB58D910A02EA02E982D986C6761EB844DE7D0A63CF36FFE2D0CE5268BF17CA9`.
  A hash-identical private backup contains all 53 candidate/evidence files.
  The single authorized first installation completed on 2026-10-18 with
  PnPUtil exit zero and published the candidate as `oem17.inf`. Passive checks
  find device `OK`, problem code zero, version `0.2026.1017.1`, a running
  service and the approved loaded-SYS hash. Independent Driver Store hashes
  match the frozen INF/CAT/SYS, and all five legacy interface classes each
  expose exactly one path for the target PCI function.
  See the
  canonical
  [package contract](driver-package-creation.md) and
  [installation preparation](phase-2c-installation-preparation.md).
- The first installation used one authorized invocation with no retry. SetupAPI
  records one `Query-remove`, one `Start`, `Restart verified` and final
  `SUCCESS`. In the same restart interval, Kernel-PnP event 411 records
  removal-state problem `0x15` / status `0xC00000BB`; no event 410, Code
  Integrity error or
  driver-related Service Control Manager event occurred in the interval.
  Source review finds no explicit `STATUS_NOT_SUPPORTED` return in the relevant
  replacement PnP paths, but START can propagate that status from the lower
  stack or `IoConnectInterrupt`. Without the contemporaneous kernel debug
  stream, attribution remains indeterminate. A default-off, compile-time PnP
  status diagnostic now records lower dispatch/completion, local START,
  interrupt connection, five interface publications and final status without
  changing their behavior. Default and instrumented Debug/Release x64 builds
  pass; focused status/remove-lock tests pass 10/10 and applicable source/ABI
  contracts pass 53/53. The installed driver was not changed. A later passive
  snapshot still finds `oem17.inf`, version `0.2026.1017.1`, device `OK`,
  problem code zero, the approved loaded SYS, a running service and one target
  interface in each of the five classes, with no later target PnP or kernel
  error. The next PnP
  transition requires a separately signed and installed diagnostic package,
  validated kernel-debug capture, and a separately authorized single PnP cycle.
  Recovery boot, rollback, reboot, Verifier, device handles, IOCTLs, XStream,
  acquisition and DMA remain separate gates.
- The instrumented package path now clean-rebuilds Debug and Release with an
  explicit diagnostics switch, verifies the `PNP_STATUS:` marker and records
  the mode in manifest schema 2. An unsigned Windows PowerShell 5.1
  qualification proposed `DriverVer=10/18/2026,0.2026.1018.1`, passed 397/397
  functional checks and 66,000/66,000 stress iterations, and produced a
  matching Release SYS/PDB pair. It was not signed or installed. Official
  DebugView CLI 5.02 x64 was hash/signature/architecture checked. One separately
  authorized 300-second kernel-only validation then captured 618 independently
  attributable StorAHCI messages and exited zero. The raw 66,018-byte log hash
  is `B8197022B9A23DA69B92881AB3F0A12A3AFB4BDF8B810CEF562F3FBA466E69EB`;
  no persistent DebugView process, service, registry entry or temporary driver
  file remained. Kernel capture is ready, while private-key signing,
  instrumented installation/START and a later PnP restart remain independent
  gates. See the [capture and package report](phase-2c-kernel-trace-capture.md).
- One separately authorized elevated signing run produced the frozen
  instrumented Release x64 candidate at workflow commit `b13f6637`. The
  manifest records `DriverVer=10/18/2026,0.2026.1018.1`, diagnostics enabled,
  397/397 functional checks, 66,000/66,000 stress iterations, valid local-test
  SYS/CAT signatures and `InstallAuthorized=false`. Frozen hashes are INF
  `ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C`,
  CAT `2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF`,
  signed SYS
  `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`,
  matching PDB
  `980161BC328FFB5214B2FA76C03B59ADA22473446DA0AE3C77F970FE0B5202A3`
  and manifest
  `216F15716C9496B476C877129853A07BAB59097444AA2D160E2812F25AA2DC69`.
  Independent marker, Authenticode, catalog-membership and SYS/PDB checks pass.
  One later authorized instrumented installation execution stopped with exit
  code 1 before DebugView created any capture evidence and before its
  fail-closed attempt marker or PnPUtil records existed. PnPUtil was not
  invoked, no package was staged, no START occurred and no retry was made. The
  outer UAC launcher did not retain the elevated exception, so the exact
  pre-capture failure remains unknown. Passive post-stop checks retained
  `oem17.inf`, device `OK`, problem code zero, the production SYS hash, the
  running service, five published interfaces and a byte-identical SetupAPI log;
  no new relevant event was found. Event 411 therefore remains indeterminate.
  Any further capture or instrumented installation requires fresh explicit
  authorization. See the
  [instrumented-installation handoff](handoffs/phase-2c-instrumented-installation.md).

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
- PR #10 is merged into the development branch at `69ad853`. All added source
  files are present exactly once in the kernel project, Debug and Release use
  the same x64 WDK toolset and manifest policy, and no live PnP, acquisition or
  IOCTL caller can enter the staged WDM v3 mapping path. The source-backed
  lifetime matrix, legacy DEVICE_BUSY compatibility classification and
  remaining decision gates are in [DMA lifetime](dma-lifetime-and-timeout.md).
  Research PR #9 at `6bff97f` remains separate; no research merge or live WDM
  activation occurred. The merged source is offline-qualified, not deployment
  qualified. Follow the [kernel-runtime qualification plan](kernel-runtime-qualification.md)
  only under its independently authorized gates.
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
