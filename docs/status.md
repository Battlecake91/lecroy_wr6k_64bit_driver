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
- Phase 2C now has a hardened validated package-only workflow at `67a3075`,
  layered on the exact `cb5010a` driver/include trees. It pins SDK/WDK 10.0.28000.0 and
  MSVC 14.41.34120, builds both configurations and tools, enforces the complete
  Dry/stress evidence, validates PE/PDB/INF/CAT metadata, records hashes, and
  contains no installation or device-operation path. The validated Release
  set is intentionally unsigned, uses the proposed unapproved
  `DriverVer=10/17/2026,0.2026.1017.1`, and has manifest SHA-256
  `3C748D04269F831D28CA836121F1B6C42B5999A3E675797B17CBFF7211A70821`.
  It is not an installation candidate. The owner has designated the WR6k host
  as an expendable test environment, but private-key use, signing,
  crash-control/reboot changes, installation, PnP, Verifier and acquisition
  retain separate gates. The previous `oem100.inf` package has been exported
  privately with Phase 1-matching hashes; elevated WinRE/BCD inventory and a
  recovery rehearsal remain incomplete. Sixteen negative package tests pass,
  including an actual mismatched SYS/PDB rejection. Read-only CNG inspection
  proves that the old test certificate has a stale machine-key reference: its
  named Microsoft Software KSP container returns `NTE_BAD_KEYSET`, while the
  provider itself is present and no alternate certificate/private-key pair was
  found. The old certificate is not signing-ready; a newly named test
  certificate/key pair was subsequently created under explicit authorization.
  Replacement thumbprint `4296FA2E355613DF0D5CE6F24D0898E00B706472` has an
  RSA-2048 non-exportable CNG machine key, exact code-signing EKU, public trust
  copies in the local-machine Root and Trusted Publishers stores, and a
  successful offline chain. Its container opens with an elevated token but not
  the filtered token, so any later signing run must be separately authorized
  and elevated. The first authorized elevated attempt used Windows PowerShell
  5.1 and failed after builds/regressions but before staging or signing because
  that runtime lacks `System.IO.Path.GetRelativePath`. No candidate, CAT,
  manifest or signed driver artifact was produced, and the authorization must
  not be reused. The workflow now uses bounded path handling compatible with
  Windows PowerShell 5.1 and PowerShell 7. Focused path, mocked package/manifest
  and signature-state tests pass 19/19 on each host; all 16 existing
  fail-closed tests also pass on each host. No real signing was performed, and
  a later second authorization reached an elevated certificate, chain and CNG
  access preflight but failed in the external launcher before the repository
  workflow was invoked: Windows PowerShell 5.1 rejected
  `Tee-Object -LiteralPath -Append` as an invalid parameter set. No output
  root, package, manifest or signature was created. Both one-run
  authorizations are consumed; a future attempt needs a separately tested
  5.1-compatible launcher, a new output path and a new explicit authorization.
  The launcher is now tracked as
  `scripts/invoke-elevated-signing-workflow.ps1` and its exact elevated
  Windows PowerShell 5.1 path passes 24/24 mock and negative checks. It uses
  separate stdout/stderr redirection, propagates child exit codes, prevents
  duplicate attempts, and independently validates the signed manifest and
  hashes. Package compatibility remains 19/19 and package fail-closed coverage
  remains 16/16 on both supported analysis hosts. A third one-run
  authorization exercised the exact tracked launcher at commit `aa009667`.
  Both driver configurations, utilities and all required offline regressions
  completed successfully: 394/394 functional checks and 66,000/66,000 stress
  iterations. The workflow then failed at the first Debug SYS/PDB `symchk`
  check. `symchk` wrote its normal search diagnostic to stderr; Windows
  PowerShell 5.1 converted it to a terminating `NativeCommandError` because
  the workflow used merged native output under
  `$ErrorActionPreference = 'Stop'`. The child ended before package staging,
  certificate lookup, private-key use, SignTool, InfVerif, Inf2Cat or manifest
  generation. The launcher did not regain control after the child ended and
  was terminated after evidence showed no child process or log progress for
  more than ten minutes. No package INF, CAT, SYS, PDB, manifest or signature
  exists, and the third authorization is consumed. The failure evidence is
  retained privately. Commit `c8cc9dd9` now replaces native PowerShell
  pipeline capture and `Start-Process -Wait` with one bounded shared process
  supervisor. It independently captures stdout/stderr, uses real process exit
  codes and assigns each child tree to a kill-on-close Windows job. Focused
  compatibility tests pass 22/22 and package negative tests pass 16/16 on both
  Windows PowerShell 5.1 and PowerShell 7.6.5. The elevated same-host Windows
  PowerShell 5.1 launcher suite passes 34/34, including timeout and descendant
  cleanup without retry or certificate access. An actual elevated unsigned run
  at this clean commit completed real Debug/Release builds, 394/394 functional
  checks, 66,000/66,000 stress iterations, matching Debug/Release SymChk,
  InfVerif and Inf2Cat with no errors or warnings, deterministic launcher exit
  and independent manifest/hash verification. Its unsigned Release artifacts
  are INF
  `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783`,
  CAT `F3C65BB6E058D0764A479A908790FC781475D928EED866D6B0FD30422CE39099`,
  SYS `7BD0FDD9A02B24CEF4531D514846A0DAAE59C9BCDBC915EB307F756AB1ECFF14`,
  matching PDB
  `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774`
  and manifest
  `9EF8F13525EFC7C692C4F6295906477DEAF2A6FEF6CB4163B1FF2F2BAA4C3B35`.
  SYS and CAT remain `NotSigned`, the manifest certificate is null, no
  SignTool log exists and no certificate or private key was accessed. Real
  signing remains untested and requires a new explicit authorization.
  See the
  canonical
  [package contract](driver-package-creation.md) and
  [installation preparation](phase-2c-installation-preparation.md).

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
