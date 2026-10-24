# Current project status

## Batch 12 legacy acquisition integration

- The installed runtime remains unchanged: `oem102.inf`, version
  `0.2026.1022.1`, device and service `OK`, and loaded SYS SHA-256
  `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`.
  Batch 12 performed no device open, IOCTL, MMIO, acquisition, DMA, PnP,
  installation, signing, Verifier, power transition or reboot operation.
- The exact one-channel METHOD_BUFFERED `CFDC2124`/`CFDC2138` MAM path is the
  first production integration point. It preserves the 15-byte request ABI,
  binds the registered locked MDL chain to its PID and FileObject, maps it
  through synchronous WDM-v3 `GetScatterGatherListEx`, validates DMA-logical
  addresses, builds the common-buffer descriptor chain and then reaches the
  unconditional `LecDmaTxLaunchDisabled` gate. It performs no acquisition or
  DMA-control MMIO and releases only the never-launched mapping.
- START now creates the per-PDO adapter/common-buffer owner before publishing
  IOCTL admission. QUERY_STOP/QUERY_REMOVE, STOP, REMOVE, surprise removal and
  asynchronous device D0/Dx completions close or restore DMA admission through
  the same parent. RemoveLock, active-IOCTL rundown, `TransferMutex` and the
  publication reference keep the IRP, MDL, FileObject and adapter ownership
  valid through preparation.
- Original-x86 and FPGA/PCI evidence still provides no physical DMA-idle
  predicate. IRQ/DPC, `IIMCL=0`, `MTTCTL=0`, INTEN masking, timeout, reset,
  STOP, REMOVE and power-down do not prove final descriptor fetch or PCI write
  retirement. Post-GO uncertainty remains irreversible `UnknownActive`.
- Debug and Release x64 build with SDK/WDK 10.0.28000.0 and MSVC 14.41.34120,
  zero warnings and zero errors. Debug SYS SHA-256 is
  `53BA585AFB165C6818ABBDDB9BA82C4DB6E09827A60403AB8CD4AD4B9FE13EF9`;
  Release is
  `4961A8F664ED9AC0FF172E2B35952A8841C8DC166AEC2BE45E644E6EEF30A0D1`.
- The complete Dry suite passes. Focused synchronous SG/PnP integration is
  157/157, source contracts 69/69, publication 38/38 and continuation 17/17.
  Expanded stress passed DMA 50,000/50,000 and INTEN 10,000/10,000; an
  additional DMA run passed 250,000/250,000 observations.

## Batch 11 WDM-v3 software transaction

- The installed Batch 9 baseline remains unchanged: `oem102.inf`, version
  `0.2026.1022.1`, service running, problem code zero and loaded SYS SHA-256
  `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`.
  Batch 11 performed no device open, IOCTL, MMIO, acquisition, DMA, PnP,
  installation, signing, Verifier or reboot operation.
- A compiled hardware-independent WDM-v3 transaction now models validation,
  file/process ownership, adapter/buffer admission, mapping, logical-address
  descriptor construction, publication, generation-safe completion,
  cancellation, timeout, retirement, POWER/PnP closure and diagnostics.
- A separate explicit no-launch WDM-v3 owner now executes the complete
  `GetDmaTransferInfo` / channel allocation / `MapTransferEx` / descriptor
  encoding / flush / free sequence against fake DDIs. Partial mappings and
  malformed or out-of-range SG lists fail before GO; cleanup uncertainty
  retains ownership and quarantines the adapter context.
- The production hardware-launch commit always fails with
  `LecDmaTxLaunchDisabled`; the possible-GO transition exists only under a host
  test define that is absent from the driver project. Legacy PFN launch gates
  remain closed.
- The staged PnP parent tracks D0/Dx separately from PnP state. QUERY and
  power-down close new mapping admission; failed resume stays closed, and only
  successful Started+D0 completion reopens no-launch preparation.
- Debug and Release x64 build with SDK/WDK 10.0.28000.0 and MSVC 14.41.34120,
  zero warnings and zero errors. Debug SYS SHA-256 is
  `9D1868FE48C44185096E8B0CF38D9C77BF017EDB4047B240D561E6AC2B08BDC5`;
  Release is
  `F3B07D710E41C38EF474996978A9F037CC84C312C79383185B4959731E816274`.
- Current focused counts are transaction 134/134, explicit WDM-v3 mapping
  74/74, synchronous SG/PnP/POWER 153/153, source contracts 69/69 and
  continuation 17/17. The complete Dry
  suite passes; default stress is DMA 5,000/5,000 plus INTEN 1,000/1,000 and
  expanded stress is DMA 50,000/50,000 plus INTEN 10,000/10,000.
- Physical bus idle remains unproved. No IRQ, DPC, mask, STOP, REMOVE or power
  event authorizes release after a possible GO. Such ownership remains
  `UnknownActive` and quarantined.
- The installed Codex CLI now supports exact noninteractive session resume.
  The one-shot mechanism pins UUID, CLI identity, prompt hash, worktree and
  authority, disables its task before launch and preserves failure evidence.
  It is mock-qualified only; no task was registered or session resumed.

## Batch 10 offline DMA safety

- The installed Batch 9 baseline remains unchanged. Batch 10 did not open a
  device, issue an IOCTL, load or install a driver, change PnP state, sign,
  reboot, acquire data or launch DMA.
- Independent x86, x64 and FPGA reviews found no physical bus-idle proof. IRQ
  completion, `IIMCL=0`, `MTTCTL=0`, INTEN masking and software rundown do not
  prove descriptor retirement, drained PCI transactions or safe mapping
  release. POWER remains a pass-through path and is an acquisition blocker.
- The development tree now blocks both legacy PFN-based MAM and MTT launches
  by default before DMA MMIO. It also rejects misaligned legacy descriptor
  storage/device addresses. The installed driver is not replaced by this code.
- [DMA architecture](dma-architecture.md) now separates verified source,
  disassembly, FPGA and hardware facts from hypotheses and defines the WDM-v3
  integration gates.
- The Batch 10 URI-only continuation limitation was superseded by the supported
  exact-session CLI path described in [Codex continuation](codex-continuation.md).

## Batch 9 runtime qualification

- The current Windows 10 Pro x64 build 19045 test environment boots from the
  only visible physical disk: GPT NVMe `HFM256GDHTNG-8510B`, serial
  `ACE4_2E00_9590_A2AA.`, unique ID `eui.ACE42E009590A2AA`. The owner states
  that production x86 resides on a physically separate disk with an independent
  bootloader; that disk was not visible from x64 and was not modified. The x64
  installation is explicitly expendable for this development phase.
- CrashControl is now `CrashDumpEnabled=2`, `AutoReboot=0`; the system-managed
  page file was preserved and allocated 8,192 MiB after the authorized reboot.
  Current-boot test signing remained enabled, Secure Boot remained disabled,
  and about 9.63 GiB remained free. No crash dump was produced.
- Exactly one non-DMA Driver Verifier boot targeted only
  `LecS65AcqDrv.sys` with mask `0x0002093B`: Special Pool, IRQL checking, pool
  tracking, I/O verification, deadlock detection, security checks,
  miscellaneous checks and DDI compliance. DMA verification and fault
  injection were absent. Runtime counters reached 4,609,185 successful of
  4,609,185 attempted allocations with zero failures; no bugcheck or target
  Verifier violation occurred. One-boot state cleared and both persistent and
  runtime Verifier state are now inactive.
- The frozen unsigned Batch 7 package was promoted without rebuilding and
  signed once with local test certificate
  `4296FA2E355613DF0D5CE6F24D0898E00B706472`. Signed candidate hashes are INF
  `C7F7D496A302FB53ACEC940A77F8A17F781F822EB5A34CEA11A4708B6385C3B6`,
  CAT `19E9358FB1346C8C40FE96D239E2000D0270559C54C04E6D3855CA710AB60B4B`,
  SYS `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`,
  PDB `5F36EF5F238F65F17EBD22B4456523817B8A7549984304FFD317A39428A174D7`
  and manifest
  `C8283DF89C4EC3E9517A90B72F35A5BF48E102B0B3BF66D3C12455BE86A1201F`.
  Authenticode, local chain, catalog membership and SYS/PDB identity pass;
  Microsoft kernel-policy verification fails as expected for a local test
  certificate.
- The single installation published `oem102.inf`, version
  `0.2026.1022.1`. The loaded SYS matches the signed hash, the device remains
  problem-free, the service runs and all five interfaces are active. The
  installation capture completed but the old launcher falsely timed out while
  serializing about 100 MiB of already-persisted capture output. It was fixed
  to keep large streams in exclusive evidence files and return only a compact
  job result; elevated Windows PowerShell 5.1 mocks pass 31/31.
- The rebuilt and pinned x64 diagnostic executable has SHA-256
  `0567A21992E95FFBB8E08B24ED10DA1CB4339682CB6DD31CD42F740F99E955D2`.
  The new driver passed the bounded hardware matrix: R0 9/9 and R1 2/2.
  Expected trace size is `0x110`, register-list size is `0x2CAE`, PCI is
  `1570:0005`, observed BDF is 5:1.0, and both bounded START reads returned
  `0x00000002`.
- One captured PnP restart then completed without retry: one DebugView capture,
  zero install invocations and one PnPUtil restart. Forty-two `PNP_STATUS:`
  records show successful teardown and START, IRQ connection and five interface
  enables. Lower-stack query paths returned the known `0xC00000BB`; local
  START and final status were successful. Post-restart baseline passes, no
  target error event occurred, and DebugView/`Dbgv.sys` left no running
  component.
- Focused unchanged-source DMA qualification passes: process/file lifetime
  23/23, combined lifetime 19/19, mapping ownership 17/17, completion 39/39,
  deterministic interleavings 2/2, default DMA stress 5,000/5,000, INTEN
  49/49 plus 1,000/1,000, PnP publication 38/38 and source contracts 64/64.
  No real DMA or acquisition was executed. Physical DMA retirement, safe abort
  and final PCI-access ordering remain unproved; WDM-v3 DMA remains dormant.

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
- The frozen instrumented Release x64 package has
  `DriverVer=10/18/2026,0.2026.1018.1`, `PnpStatusDiagnostics=true`, signed SYS
  SHA-256
  `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`
  and matching PDB SHA-256
  `980161BC328FFB5214B2FA76C03B59ADA22473446DA0AE3C77F970FE0B5202A3`.
  Its offline qualification passed 397/397 functional checks and
  66,000/66,000 stress iterations. The versioned Windows PowerShell 5.1
  launcher passed 18/18 elevated mock scenarios and source contracts 54/54.
- Kernel Runtime Qualification Batch 1 ran `InstallWithCapture` exactly once
  on 2026-10-19. Launcher, DebugView and PnPUtil exited zero; PnPUtil published
  `oem101.inf`. The active version is `0.2026.1018.1`, the device is `OK` with
  problem code zero, `LecS65AcqDrv` is running, the loaded SYS matches the
  frozen hash above, and each of the five legacy interface classes exposes
  exactly one target path. Driver Store INF/CAT/SYS hashes also match the
  frozen package. No DebugView process or temporary `DBGV` component remained.
- The 73,498-byte kernel log has SHA-256
  `D14DAC42D6CEBED65313DE8A265E10B80D52CB57A33C0F79A888F625257948A4`
  and contains 32 `PNP_STATUS:` records. New-driver lower START, local START,
  `IoConnectInterrupt`, all five interface-enable calls and final START all
  report success. Event 411 (`0x15` / `0xC00000BB`) occurred after the old
  driver completed REMOVE/unload and about 27 ms before the replacement
  driver's `DriverEntry`/AddDevice began. It is therefore a verified
  installation/PnP intermediate state rather than a status returned by the new
  driver's START, interrupt connection or interface publication. The exact
  Windows component that supplied the event status remains indeterminate.
  No target Code Integrity, SCM or System error occurred. See the
  [capture report](phase-2c-kernel-trace-capture.md) and
  [runtime qualification plan](kernel-runtime-qualification.md).
- Offline post-install contracts pass: source/ABI 54/54, PnP/remove-lock
  10/10, interface publication 38/38, DMA completion 39/39, interleavings 2/2,
  DMA stress 5,000/5,000, INTEN functional 49/49, INTEN stress 1,000/1,000 and
  DMA ownership 17/17. No reproducible source defect was found, so no driver
  source or signed package change was made. Device handles, IOCTLs, later PnP
  transitions, Verifier, reboot, XStream, acquisition and DMA remain separate
  gates. See the [IOCTL safety classification](ioctl-runtime-safety.md).
- Kernel Runtime Qualification Batch 2 established four passing R0 commands:
  build `1002`, debug stats version `1`, BAR lengths
  `0x200 / 0x40000 / 0x200`, and trace version `3`. Batch 2B then attempted the
  fifth command once. The corrected runner preserved the empty argument, but
  `lecdiag` rejected zero hex digits before `DeviceIoControl` and exited 2.
  Commands 6–9 and both R1 reads were not run. The exact interval contained no
  System, Kernel-PnP or Code Integrity event; the device, service, loaded SYS
  and five interfaces remained unchanged. `lecdiag` now accepts explicit
  zero-length raw input offline; rebuilt x64 SHA-256 is
  `ADD4A869C05DD85A55DD9308927D2AE946907F26C0D0054E46A54738441FB7BA`.
  Source/safety contracts pass 59/59 and the event/interface lifetime audit
  model passes 19/19. No driver source or installed image changed.
- Kernel Runtime Qualification Batch 2C proved the complete PowerShell 5.1 to
  native argument parser to IOCTL-buffer path with a mock Win32 boundary:
  all nine R0 command forms, both R1 command forms and five malformed-input
  cases passed 16/16 without opening the real device. The live gate then
  stopped before opening any device handle. A reboot outside this test had
  left `oem101.inf` selected but the device at problem code 52, the service
  stopped, all five interfaces disabled and the PCI location changed from the
  authorized 4:1.0 baseline to 5:1.0. Code Integrity event 3004 rejected the
  otherwise hash-identical signed SYS and Kernel-PnP event 219 recorded the
  failed load. R0-5 through R0-9 and both R1 reads were therefore **NOT RUN**.
  No retry, repair, PnP action or hardware request occurred. A subsequent
  elevated read-only audit established that the active EFI/BCD loader entry
  has no `testsigning` element. The unchanged locally test-signed image was
  rejected with `0xC0000428`; Secure Boot, VBS, HVCI and WDAC are not active
  blockers. The same stable device instance was enumerated at 5:1.0, so BDF is
  mutable topology evidence rather than device identity. See
  [Code 52 signing recovery](code52-signing-recovery.md).
- The passive Code 52 collector completed with evidence-manifest SHA-256
  `9544E8EDA2ED225404C112E6E4FBBF7DEAFF2A6960E1823422184E987A8464D8`.
  On 2026-10-21 the separately authorized single
  `bcdedit /set {current} testsigning on` change and one restart completed.
  Post-boot readback shows `testsigning Yes` on the same HFM EFI/BCD path. The
  stable device instance is `OK` at observed BDF 5:1.0, `oem101.inf` version
  `0.2026.1018.1` and its SYS hash are unchanged, `LecS65AcqDrv` is running,
  and all five interfaces are enabled. No new LeCroy Code Integrity, Kernel-PnP
  219 or SCM error occurred. R0-5 through R0-9 and R1 remain NOT RUN and need
  new explicit authorization.
- `scripts/test-runtime-baseline.ps1` gates future batches on the exact full
  hardware/instance identity and treats PCI location changes as separately
  acknowledged evidence. Its six Windows PowerShell 5.1 mock cases pass 6/6;
  the post-recovery real passive gate also passes. Interface enumeration now
  handles missing PnP properties and localized PnPUtil output fail-closed.
- The current offline Dry run passes 469/469 functional/model checks plus
  5,000/5,000 DMA and 1,000/1,000 INTEN stress iterations. This includes
  62/62 source/safety contracts and the new 22/22 process/file-object DMA
  lifetime model. No driver source changed, so no driver rebuild, signing or
  expanded stress rerun was warranted.
- Kernel Runtime Qualification Batch 2D revalidated the exact command chain
  16/16 with unchanged `lecdiag.exe` and process-runner identities. The single
  elevated live invocation then stopped in its passive preflight at
  `2026-10-21T16:45:04Z`: Windows reported VBS status `0` and security-service
  sentinel `{0}`, but the one-shot wrapper incorrectly treated every nonempty
  array as an active service. The device-command phase had not started and no
  attempt marker existed, so R0-5 through R0-9 and both R1 commands remain
  **NOT RUN**. No retry was made. Private evidence-manifest SHA-256 is
  `A41492D7CD65C20DE4CE0C72C3D40D643BC22EE8D30E9CD2BF8C7DAB9D1DF9FB`.
  A later passive snapshot passed with the unchanged device, service, SYS and
  five interfaces; no target event occurred in the test window. Its JSON
  SHA-256 is `08C05F481BBED834B8CEF857661267EA2F96FB3C343F39A1E748D155243422DA`
  and the supplemental evidence-manifest SHA-256 is
  `D459BA05A31B90EFF665D320EFA7A4C336593CC09A994F9AF15607DD2CB7BC90`.
- The Batch 2D offline follow-up again passes source/safety 62/62,
  process/file-object DMA lifetime 22/22, combined event/interface/DMA lifetime
  19/19, PnP IRP lifetime 10/10, PnP publication 38/38, DMA ownership 17/17,
  DMA completion 39/39, completion interleavings 2/2, DMA stress 5,000/5,000,
  INTEN functional 49/49 and INTEN stress 1,000/1,000. Source review still
  finds the per-close process cleanup gap, but the proposed per-process
  reference model is not yet safe enough to promote into the live driver.
  Interface activation and FDO-global event replacement remain intentional
  compatibility policies rather than verified defects; no driver source or
  public ABI changed.
- The next authorized R0/R1 completion attempt added a reusable, side-effect-free
  Device Guard predicate. Ten focused cases pass under both PowerShell 7 and
  Windows PowerShell 5.1, covering zero sentinels, empty/null/absent data,
  configured-only state, active HVCI/CI policy, unknown values and contradictory
  reports. The real host evaluates `Inactive` and allowed while preserving the
  raw CIM values. The single elevated live invocation still stopped before its
  first `lecdiag` call: Windows PowerShell 5.1 decoded the temporary UTF-8
  wrapper's non-ASCII previous-location literal as `GerÃ¤t`, so the otherwise
  passing stable-identity baseline required explicit location acknowledgement.
  There are zero device-command attempt markers; R0-5 through R0-9 and both R1
  reads remain **NOT RUN**, with no retry. A subsequent passive baseline passed
  with unchanged problem-zero device, running service, SYS hash, BDF 5:1.0 and
  five interfaces, and no target event appeared. Initial private manifest
  SHA-256 is `8995F1FFE864ADCE58244AD5A8710CD71852B4FA1EE6E79AA3E6116671E8AA99`;
  post-stop baseline SHA-256 is
  `55967F8633D61E66D8B89C91A883A6B831C0ED1D8C25C214FE03A5DF2B4D6780`;
  supplemental manifest SHA-256 is
  `AB9D0181B675FADEDB1B546498CAD4904AC1A0A3AE17044B98FD70E94000A48C`.
- Runtime Qualification Batch 2F replaced temporary wrappers with
  `scripts/invoke-runtime-ioctl-qualification.ps1`. Its exact Windows
  PowerShell 5.1 production flow passes 19/19 focused mock cases. The corrected
  passive preflight passed with zero device-command markers. In the one
  authorized runtime chain, R0-5 and R0-6 passed once. R0-7 also returned exit
  0 and the canonical four-byte value `0x00000110`, but the harness correctly
  stopped because it had copied the stale plan expectation `0x1A08`. R0-8,
  R0-9 and both R1 reads were not run. Source and canonical ABI evidence prove
  that `0x110` is the current trace-control descriptor size; the harness and
  mock are corrected offline. Post-stop passive state is unchanged: problem
  zero, `oem101.inf` version `0.2026.1018.1`, running service, expected SYS
  hash, five interfaces and no target event. A new explicit authorization is
  required for the remaining runtime commands.
- Runtime Qualification Batch 3 completed the low-risk runtime matrix without
  repeating R0-1 through R0-7. The permanent harness added an exact
  `Batch3Remaining` allowlist and passes 21/21 focused Windows PowerShell 5.1
  cases. R0-8 returned the canonical register-list size `0x2CAE`; R0-9
  completed the exact four-byte zero-delay request; the bounded PCI query
  returned `1570:0005` at observed BDF 5:1.0; and the dedicated and generic
  START reads both returned `0x00000002`. All four processes exited zero with
  no stderr, timeout, retry, health change or target event. The safe ABI
  runtime baseline is now 9/9 R0 plus 2/2 R1 PASS. The installed package,
  service, loaded SYS and five interfaces remain unchanged.
- The legacy ABI consistency audit now publishes canonical `0x110`, `0x10A`,
  43 and `0x2CAE` size constants, compiles driver/tool size assertions and
  binds the mock values through a source contract. Focused source contracts
  pass 63/63 and the lecdiag command chain passes 16/16.
- PR #26 is the runtime-evidence base; PR #27 and PR #28 are sibling stacks on
  that base. Merge PR #26 first, then integrate PR #27's driver change, then
  replay PR #28 and Batch 5 tooling/documentation. A direct local merge of the
  current PR #27 and PR #28 heads has real documentation conflicts in
  `docs/regression-testing.md` and `docs/status.md`; no shared branch was
  rebased, force-pushed or merged.
- Runtime Qualification Batch 5 added live stdout observability to the bounded
  process runner and separates collector health, capture initialization, raw
  file flush and confirmed kernel-message reception. Elevated Windows
  PowerShell 5.1 mocks pass 21/21 and source contracts pass 64/64. The single
  authorized real attempt captured 100,002 timestamped StorNVMe kernel records,
  but the first implementation accepted only unprefixed markers, so readiness
  failed closed. `PnpRestartInvocations=0`; no PnP transition or Event 411
  occurred. The timestamped-record predicate is corrected offline. Any real
  restart now requires a new explicit authorization.
- PR #26 has no repository-level merge conflict: its base commit is an ancestor
  of the head, the head is a strict descendant of the base, and a local
  `git merge-tree --write-tree` succeeds. The reported non-mergeable state is
  therefore not branch topology; GitHub's public page currently fails to load
  merge/check details, so a stale mergeability calculation or unmet hosted
  check/protection requirement remains the external explanation.

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
- Batch 13 offline preparation adds a machine-readable side-effect allowlist,
  exact-source contracts and a one-shot `CFDC2124`/`CFDC2138`/`CFDC2128`
  diagnostic. The reviewed Batch 12 driver/include trees remain unchanged;
  the default production branch contains no reachable MMIO or GO operation in
  the selected no-launch sequence. Runtime qualification is not claimed until
  the separately frozen package and authorized stages complete.

## Hardware status

- Runtime Qualification Batch 6 completed one authorized captured PnP restart
  with no retry. The launcher invoked DebugView once and PnPUtil once; it did
  not install a package. The restart completed with exit code zero. The trace
  records a complete REMOVE/unload/re-enumeration/START sequence, five
  successful interface disables and enables, IRQ disconnect/reconnect, and
  successful final REMOVE and START statuses. The installed `oem101.inf`
  package, version `0.2026.1018.1`, loaded SYS hash
  `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`,
  service, problem-free device state and five interfaces remained unchanged.
- Five observed `STATUS_NOT_SUPPORTED` completions are directly attributed to
  the lower stack for query-only PnP requests. No local START,
  interrupt-connection, interface-publication or final returned status failed.
  Event 411 did not recur. This corroborates the earlier Event 411 as an
  installation intermediate, but does not identify its internal Windows
  producer. No physical DMA-idle proof was obtained.
- The launcher path passes 31/31 elevated Windows PowerShell 5.1 mocks and
  64/64 source contracts. The exact reviewed DMA-fixed driver tree is now the
  installed Batch 9 candidate; its bounded non-DMA runtime result is summarized
  at the top of this document.
- The established real-PCI safe ABI baseline remains 9/9 PASS and the last
  established hardware regression milestone remains 11/11 PASS.
