# Phase 2C kernel-runtime qualification plan

This is the execution procedure for the Phase 2C WR6k test host. Kernel Runtime
Qualification Batch 1 installed the frozen instrumented candidate once and
completed its installation-induced START. The current package is `oem101.inf`,
version `0.2026.1018.1`, with loaded SYS SHA-256
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`.
The device is problem-free, the service is running, and the five compatibility
interfaces are present. This established load, START and diagnostic status
attribution only. Driver Verifier, any later PnP transition, device handles,
IOCTLs, acquisition and DMA remain separate authorization gates.

The six stages below are strictly ordered and independently authorized. An
authorization for one stage does not authorize the next. Preserve the evidence
and obtain the stated approval at every exit gate before continuing.

## Absolute safety boundary

The owner-designated WR6k host is the Phase 2C test system. Recovery media,
console access and the exported previous package must still be verified before
installation; accepting loss does not make an untested rollback path valid.

The following remain prohibited throughout Phase 2C:

- XStream or other acquisition, legacy PFN DMA, and activation of the staged
  WDM v3 DMA path;
- arbitrary MMIO/register writes and `SetOneRegister` (`0x0022303C`);
- Dallas EEPROM reads that expose licensed contents, Dallas writes/deletes,
  serial FPGA/GPIO writer `0xCFDC2130`, FPGA programming or device reset;
- nonzero synthetic interrupt masks, forced interrupts and guessed register
  values;
- releasing quarantined resources after `UnknownActive`; quarantine is
  irreversible until reboot or image restore and is not hardware-idle proof.

Driver START itself maps resources and performs the established startup probe.
Therefore a package installation that binds to real hardware is not a passive
metadata operation even when no user-mode device handle is opened.

`CompletionObserved` is not `IdleProved`. Active PFN DMA is not generally
IOMMU-safe, physical DMA retirement remains unproved, and live WDM v3 DMA must
remain unreachable.

## Authorization and hardware matrix

| Stage | Hardware | Separate owner authorization | Permitted state change |
|---|---|---|---|
| 1. Read-only validation | Build host and owner-designated WR6k test host | Read package/system metadata and export non-sensitive evidence | None |
| 2. Package creation/signing | Offline build/signing host; no WR6k attached or required | Use the named test certificate/private key and create signed artifacts | Files and certificate-use audit only; no Driver Store or PnP change |
| 3. Recovery/rollback proof | Owner-designated WR6k test host | Recovery boot, reinstall/reimage, reboot and crash-control changes | Test-host configuration and reboots only |
| 4. Initial load | Same host and its WR6k PCI device for one real START | Driver Store installation and one initial PnP START with automatic startup MMIO | Install and initial START only |
| 5. Five-interface verification | Same successfully started host/hardware | Enumeration-only interface inspection | None beyond the already authorized loaded state |
| 6. Verifier/PnP | Same host/hardware; hot-removal chassis only for optional surprise removal | Verifier, reboots, device handles for approved software/read-only controls, and each PnP matrix | Explicitly bounded Verifier and PnP transitions |

## Stage 1 — read-only package and system validation

### Preconditions

- The candidate commit, branch and proposed package source are identified.
- No candidate driver is installed, staged or loaded.
- The owner has authorized read-only collection on the intended test host.

### Procedure

1. Verify a clean worktree at the exact reviewed commit. Record `git status`,
   commit SHA, SDK/WDK/MSVC versions and the complete Dry result.
2. Inventory the candidate INF and any existing SYS/CAT without executing or
   trusting them. Record size, SHA-256, PE machine, subsystem, entry point and
   imports.
3. On the intended test host, record Windows build, architecture,
   firmware mode, Secure Boot, test-signing, hypervisor, VBS/HVCI, IOMMU,
   Driver Verifier state and available dump storage.
4. If a WR6k device is present, enumerate only PnP identity, current package,
   service state and assigned resources. Do not open an interface or issue an
   IOCTL. Preserve the existing package identity and hashes privately.
5. Record the owner's test-host designation, accepted loss model and the
   independently gated operations that remain unauthorized.

### Exit gate

PASS requires an exact source identity, complete read-only inventory, no
unexpected active Verifier state and a named recovery owner. Any ambiguity is a
stop. Stage 1 approval does not permit signing or package creation.

The original audit result and exact host/tool/package evidence are recorded in the
[Phase 2C Stage 1 handoff](handoffs/phase-2c-stage-1-read-only-audit.md).
The later owner designation makes this host eligible for controlled testing,
but recovery boot and restore/reinstallation proof remain incomplete.

## Stage 2 — reproducible package creation and test signing

Follow the canonical
[offline driver-package creation contract](driver-package-creation.md). It is
the controlling fail-closed specification; the summary below does not relax
it.

### Preconditions

- Stage 1 evidence is accepted.
- The owner has separately authorized use of a specific test certificate and
  its private key.
- The build/signing host has no need for WR6k hardware.

### Procedure

1. Build Debug and Release x64 from the exact reviewed commit with SDK/WDK
   10.0.28000.0. Run the complete Dry suite and expanded stress profile.
2. Select exactly one configuration for the runtime candidate. Copy its SYS and
   the reviewed INF into a new empty staging directory. Use an owner-approved,
   recorded `DriverVer`; do not derive an undocumented candidate identity from
   an unrecorded wall-clock invocation.
3. Sign the staged SYS with the named test certificate, generate the catalog
   with `Inf2Cat /os:10_X64`, then sign the CAT. Verify SYS and CAT with the
   intended test-signing policy and record certificate thumbprint/expiry.
4. Record SHA-256 for INF, CAT, signed SYS and the privately retained matching
   PDB. Create a manifest containing the source commit, tool versions,
   configuration, `DriverVer`, hashes and signing identity.
5. Copy the complete immutable candidate set to write-protected or otherwise
   controlled storage. Every later stage must use these exact hashes.

The current linker embeds a build timestamp, a new CodeView GUID and an
absolute PDB path. Identical source built in another worktree therefore does
not currently produce the same SYS hash. Reproducibility for Phase 2C means
that one reviewed build is hash-frozen and promoted unchanged; do not rebuild
and assume equivalence. Bit-reproducible linking is a separate future change.

Use `scripts/build-driver-package.ps1` for Stage 2. Its hardened default
unsigned mode was validated at workflow commit `67a3075`. Do not use
`scripts/build-sign-load-driver.ps1`; that script
continues from package creation into `pnputil` installation, device restart
and `lecdiag` device access.

### Exit gate

PASS requires successful signature/catalog verification and a complete frozen
manifest. No package may exist in the test host's Driver Store. Stage 2
approval does not permit installation, loading or PnP activity.

## Stage 3 — recovery and rollback verification

### Preconditions

- Stage 2 artifact hashes are accepted.
- The owner has separately authorized recovery boot, reboot and crash-control
  changes on the designated test host.
- The original working package and a bootable recovery environment are
  available off-host.

### Procedure

1. Create and identify a fresh VM snapshot or bare-metal image. Record the
   restore procedure, storage location and responsible operator.
2. Export the existing driver package and current boot, code-integrity,
   Verifier and crash-control state.
3. Configure a kernel dump, system-managed boot-volume page file, dump
   overwrite protection and disabled automatic reboot. Keep the exported prior
   crash-control settings.
4. Boot the recovery environment once, locate the Windows volume, prove access
   to `verifier /reset` and the offline Driver Store, and return without
   changing the installed driver.
5. Restore the snapshot/image once and revalidate its identity. A snapshot that
   has never been restored is not a proven rollback path.

### Exit gate

PASS requires a demonstrated recovery boot and successful restore. Failure or
uncertain console access stops Phase 2C. Stage 3 approval does not permit
candidate installation.

## Stage 4 — initial driver loading without acquisition

### Preconditions

- Stages 1–3 passed.
- Exact INF/CAT/SYS hashes match the Stage 2 manifest.
- The owner has separately authorized one installation and initial START on
  the designated test host, including the driver's automatic startup MMIO.
- XStream and every process capable of opening the driver are stopped.

### Procedure

1. Confirm Driver Verifier is off. Capture a fresh pre-install event-log and
   service/device snapshot.
2. Install only the frozen package:

```text
pnputil /add-driver .\LecS65AcqDrv.inf /install
pnputil /enum-devices /instanceid "<exact-device-instance-id>" /drivers
```

3. Permit one initial START. Do not use `lecdiag`, `lecwatch`, XStream or any
   other device-opening program.
4. Verify the exact service/image/package identity, healthy devnode, one START,
   no unexpected System/Code Integrity event, no dump and no automatic restart.
5. Stop immediately on a hang, bugcheck, unexpected interrupt activity,
   acquisition/DMA indication, signature failure or resource error. Preserve
   evidence and restore the image rather than retrying.

### Exit gate

PASS establishes load and START only. It does not establish interface handles,
IOCTL ABI, DMA safety or PnP teardown. Stage 4 approval does not permit opening
the device or changing its PnP state.

## Stage 5 — five-interface runtime verification

### Preconditions

- Stage 4 passed and the host remains otherwise untouched.
- The owner has authorized enumeration-only interface inspection.

### Procedure

1. Use an audited SetupAPI/Configuration Manager enumeration-only helper. It
   must not call `CreateFile` or `DeviceIoControl`.
2. For the exact WR6k PCI instance, require exactly one present symbolic-link
   instance in each class:

```text
{958695A4-693A-435E-8297-66F805D8E46A}
{7AC34BE9-F766-4F15-9E88-854BA5E2146E}
{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}
{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}
{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}
```

3. Reject a missing class, duplicate instance, unexpected sixth class, wrong
   device-instance association or published `\\.\ALADDINAcqDriver0` alias.
4. Save only non-sensitive interface metadata. Do not open any returned path.

### Exit gate

PASS requires five classes and five total present instances for the one PCI
function. Stage 5 approval does not permit IOCTLs, Verifier, restart, disable,
remove or rescan.

## Kernel Runtime Qualification Batch 1 result

The authorized `InstallWithCapture` operation ran exactly once on 2026-10-19
from `2026-10-19T16:53:29.2283759Z` through
`2026-10-19T16:58:32.8669865Z`. The elevated Windows PowerShell 5.1 launcher,
capture child and PnPUtil all exited zero. PnPUtil published `oem101.inf` and
bound it to the exact PCI function. There was one capture invocation, one
installation invocation, no retry and no rollback.

The 73,498-byte raw kernel log has SHA-256
`D14DAC42D6CEBED65313DE8A265E10B80D52CB57A33C0F79A888F625257948A4`.
It contains 691 records, including 32 `PNP_STATUS:` records from the
instrumented driver. The new START recorded lower-stack completion and final
status `STATUS_SUCCESS`, local START `STATUS_SUCCESS`, interrupt connection
`STATUS_SUCCESS`, five successful indexed interface-enable operations and
final START `STATUS_SUCCESS`.

Kernel-PnP Event 400 was recorded at
`2026-10-19T16:53:33.8422476Z`. Event 411 followed at
`2026-10-19T16:53:33.8741035Z` with problem `0x15` and status
`0xC00000BB`. The old driver had already received query-remove/remove, disabled
all interfaces, disconnected its interrupt, completed teardown and unloaded by
`16:53:33.873Z`. The replacement driver's `DriverEntry`/AddDevice and first PnP
dispatch did not begin until `16:53:33.901Z`. Event 411 is therefore a
**VERIFIED intermediate installation/PnP state**, not a status returned by the
new replacement driver's START, interface publication or interrupt connection.
The exact Windows component that supplied the event's `0xC00000BB` remains
indeterminate. Later `STATUS_NOT_SUPPORTED` records are verified lower-stack
responses to query requests, but they occurred after Event 411 and cannot be
its source.

SetupAPI completed the single device restart and overall installation with
`SUCCESS`. No target Code Integrity, Service Control Manager or System error
was found. The final device is `OK`, problem code zero, the service is running,
the Driver Store INF/CAT/SYS match the frozen package, and each of the five
interface classes exposes exactly one target path. DebugView and its temporary
`DBGV` component were absent afterward. The unrelated Bonjour Code Integrity
records in the interval are not LeCroy-driver evidence.

The complete IOCTL side-effect review and the separately gated first runtime
commands are in [Runtime IOCTL safety classification](ioctl-runtime-safety.md).

## Kernel Runtime Qualification Batch 2 result

Batch 2 preflight at `2026-10-19T18:04:14Z` reconfirmed `oem101.inf`, the
approved loaded SYS, problem-free device, running service, five enabled
interfaces, closed acquisition clients and intact rollback set. The selected
x64 `lecdiag.exe` SHA-256 is
`824F089EBED1D4C5EC0337CBE18F05489D2B062476AD5C1723333869B299A8DD`.

R0 commands `build`, `stats`, `bars` and `trace` passed once. They established
build `1002`, debug structure version `1`, zero unknown IOCTLs, BAR lengths
`0x200 / 0x40000 / 0x200`, trace version `3` and the expected response sizes.
Before the fifth command could start, the PowerShell native runner rejected the
required empty raw-input argument at parameter binding. No fifth IOCTL was
issued, commands 6–9 were not attempted and R1 was skipped. No retry occurred.

Post-stop checks retained the complete baseline and found no event in the exact
R0 interval. The runner now explicitly permits an empty string array element;
focused Windows PowerShell 5.1 and PowerShell 7 mocks each pass 26/26. This
offline fix does not authorize another live attempt.

### Lifetime-analysis result

- CREATE, CLOSE, DEVICE_CONTROL, PnP and power dispatches retain balanced
  remove-lock ownership. DEVICE_CONTROL is synchronous and releases IOCTL
  admission before its remove lock and completion.
- STOP, SURPRISE_REMOVE and REMOVE close admission, withdraw interfaces, drain
  admitted IOCTLs, quiesce IRQ/DPC/timer activity, release event and transfer
  ownership, and only then unmap BARs. REMOVE waits for lower-stack completion
  and remove-lock rundown before detaching/deleting the FDO.
- Event replacement references the new object before publication, swaps under
  the DPC-shared spin lock and dereferences the old object afterward. PnP
  teardown detaches all five event objects only after deferred work is quiet.
- `UnknownActive` remains irreversible. Publication/rundown tests retain
  mapping and MDL ownership through REMOVE rather than freeing it without
  hardware-idle proof.
- DEVICE_CONTROL has no pending/cancelable IOCTL. Pending lower-PnP forwarding
  is unit-tested; targeted cancellation and cancel-routine races remain NOT RUN.
- Event registrations are FDO-global, not per file object. Object lifetime is
  safe, but replacement ownership between concurrent clients remains a legacy
  semantic risk without native concurrency fault injection.
- CLOSE releases every DMA registration owned by the calling process whenever
  any handle from that process closes. It therefore does not implement the
  comment's “last process close” semantics for multiple concurrent handles.
  Correcting this confirmed lifetime limitation requires reviewed per-file or
  per-process ownership design and is not a safe minimal patch while DMA is
  unqualified.
- Interface-enable failure is logged but does not fail START or roll back
  already enabled classes. The live START published all five; partial-
  publication failure handling remains a source gap before repeated PnP tests.

New contracts cover balanced CREATE/CLOSE ownership, event-object lifetime and
empty native arguments. Results are source/safety 57/57, PnP IRP lifetime
10/10, publication 38/38 and DMA ownership 17/17. No driver implementation
changed, so driver builds and DMA/INTEN stress reruns were not justified.

### Batch 2B stopped result and lifetime follow-up

Batch 2B attempted only the next authorized R0 command. The process runner
preserved the empty positional argument, but the reviewed `lecdiag` binary
opened the interface and rejected an empty hex payload in user mode. Exit code
2 occurred before `DeviceIoControl`; commands 6–9 and R1 were not run. The
post-stop device state matched the baseline and the exact 29.3-second interval
contained no System, Kernel-PnP or Code Integrity event.

The defect was in the diagnostic utility rather than the driver or process
runner. The offline fix accepts a zero-length payload deterministically and
initializes both output fields before returning. The rebuilt x64 utility hash is
`ADD4A869C05DD85A55DD9308927D2AE946907F26C0D0054E46A54738441FB7BA`.
It has not been used against the device. Source contracts pass 59/59; a new
hardware authorization must bind to this hash.

The independent lifetime audit reached these classifications:

- Process identity is part of every transfer token lookup, but CLOSE has no
  file-object or per-process open-count state. The first close from a process
  can therefore free that process's idle registrations while another handle in
  the same process remains open. This contradicts the source comment's
  last-process-close claim. A per-file ownership change would alter the legacy
  sharing ABI; a per-process count needs new allocation/rundown state. Neither
  is proven safe enough for a minimal production patch while DMA is inactive.
- Interface enable remains continue-on-error. A one-failure fault model proves
  that it can leave four published classes and still permit START to continue.
  Expanded first, middle, last and multiple-failure models plus STOP/REMOVE
  teardown all pass. The original image proves four registrations and passive
  XStream evidence proves the fifth endpoint is required, but neither proves
  the original driver's response to an enable failure. Failing START or rolling
  back successful classes could therefore change compatibility. Production
  behavior remains unchanged pending direct lower-stack fault injection and an
  explicit compatibility decision.
- Event slots are intentionally FDO-global. Registration references the new
  object before publication, swaps under the DPC-shared lock and dereferences
  the old object after the swap. Handle close does not stale the referenced
  object. A 19/19 host model covers multiple handles, both serialized
  DPC/replacement orderings, DPC during close, STOP/REMOVE during replacement,
  partial interface activation and the DMA ownership design cases. Concurrent
  clients can replace each other's notification target; device-global slots
  match the recovered device-object storage and are therefore intentional
  legacy semantics, not a demonstrated object-lifetime defect.
- DEVICE_CONTROL completes synchronously and contains no pending/cancel path.
  Adding one solely for Verifier would create new ABI/lifetime behavior rather
  than fix a demonstrated defect. Pending lower-PnP forwarding remains covered
  separately.

No driver source changed, so driver rebuilds and DMA/INTEN stress reruns were
not warranted. The private final evidence-manifest SHA-256 is
`3472CE4AC5854DA4861A9E5139DCC61F802C1D4CEBCEA10BF69004C613EC5DC8`.

### Batch 2C offline proof and live stop

The production PowerShell 5.1 runner and exact `lecdiag.c` parser completed a
fake-device end-to-end matrix before any live operation. All nine R0 command
forms, both R1 forms and five malformed inputs passed 16/16. Exact mocked
request codes and lengths included the explicit zero-length inputs, four zero
delay bytes, the 84-byte PCI output and both START-register reads.

The live preflight then failed closed. A successful host boot outside this
test was followed by Code Integrity event 3004 rejecting
`LecS65AcqDrv.sys` and Kernel-PnP event 219 recording the failed driver load.
The package identity and SYS file hash remained correct, but the device was at
problem code 52, `LecS65AcqDrv` was stopped, all five interfaces were disabled
and PCI enumeration reported 5:1.0 instead of the authorized 4:1.0 baseline.
No device handle was opened and none of R0-5 through R0-9 or R1 executed.

The follow-up elevated read-only signing audit established the boot-policy
cause. The active `{current}` EFI/BCD loader contains no `testsigning` element,
whereas retained pre-install evidence recorded `testsigning Yes` on the prior
boot path. Code Integrity and Kernel-PnP recorded `0xC0000428`; package hashes,
local Authenticode, local chain and catalog membership remain valid. Secure
Boot, VBS, HVCI and WDAC are inactive. See
[Code 52 signing recovery](code52-signing-recovery.md).

The separately authorized recovery on 2026-10-21 changed only the active
loader's `testsigning` element and issued one restart. Immediate and post-boot
readback show `testsigning Yes`; the current HFM/HarddiskVolume1 boot identity
is unchanged. The stable PCI instance returned to problem code zero, the exact
approved image is loaded, the service is running and all five interfaces are
enabled. No LeCroy-related Code Integrity error, Kernel-PnP 219 or SCM error
occurred after boot. This restores the passive prerequisite only; R0-5 through
R0-9 and R1 remain NOT RUN and require new explicit authorization. Private
evidence remains outside the public repository; see the recovery document for
manifest hashes.

### Batch 2D stopped preflight and offline follow-up

Batch 2D retained the restored boot baseline and unchanged diagnostic binary
and runner identities. A fresh mock traversal of all R0/R1 command shapes and
malformed-input boundaries passed 16/16. The one authorized elevated live
invocation nevertheless stopped before the runtime baseline and before any
device command. `Win32_DeviceGuard` returned VBS status `0`, both policy
enforcement states `0` and `SecurityServicesRunning={0}`. The temporary
one-shot wrapper incorrectly interpreted the zero sentinel as an active
service because it rejected every nonempty array.

No attempt marker, native process log or device snapshot was created; only the
failure, outcome and manifest records exist. Thus no device path was opened,
R0-5 through R0-9 were not invoked, and the conditional R1 gate never became
effective. There was no retry or system repair. The private manifest SHA-256 is
`A41492D7CD65C20DE4CE0C72C3D40D643BC22EE8D30E9CD2BF8C7DAB9D1DF9FB`.
A subsequent passive snapshot passed: the exact instance remained problem-free
at 5:1.0, `oem101.inf` and the loaded SYS hash were unchanged, the service was
running and all five interfaces remained published. No target event appeared
in the exact post-attempt window. Snapshot SHA-256 is
`08C05F481BBED834B8CEF857661267EA2F96FB3C343F39A1E748D155243422DA`;
supplemental evidence-manifest SHA-256 is
`D459BA05A31B90EFF665D320EFA7A4C336593CC09A994F9AF15607DD2CB7BC90`.

The independent offline lifetime review remains green: process/file DMA 22/22,
event/interface/DMA lifetime 19/19, PnP IRP 10/10, publication 38/38, mapping
ownership 17/17, completion 39/39 plus 2/2 deterministic interleavings, DMA
stress 5,000/5,000 and INTEN 49/49 plus 1,000/1,000. These prove software model
contracts, not physical DMA retirement. No production source change was
justified. A later hardware attempt requires a new explicit authorization and
a reviewed Device Guard predicate that rejects nonzero service identifiers,
not the `{0}` sentinel.

### R0/R1 completion attempt: guard fixed, live gate stopped

`scripts/deviceguard-preflight.ps1` is now the reusable production predicate
for passive security-policy gating. Its 10/10 matrix passes on PowerShell 7 and
Windows PowerShell 5.1. It handles scalar and vector unrolling, accepts zero and
empty service sets, fails closed on null/absent/unknown data, distinguishes
configured-but-not-blocking state and rejects active or contradictory VBS,
HVCI and CI-policy evidence. The serialized result contains both evaluated and
raw values. The actual host evaluates `Inactive` and allowed.

The subsequently authorized one-shot live chain did not reach `lecdiag`.
Windows PowerShell 5.1 treated the temporary UTF-8-without-BOM wrapper as an
ANSI script and decoded the previous-location literal `Gerät` as `GerÃ¤t`.
The stable instance ID, hardware ID, `oem101.inf`, version, problem code zero,
running service, exact SYS hash and five interfaces all matched. The baseline
still failed closed on the location mismatch because the separate acknowledgement
flag was intentionally absent. Zero device-command attempt markers prove that
R0-5 through R0-9 and the conditional R1 pair remain NOT RUN; no retry occurred.

The initial private manifest SHA-256 is
`8995F1FFE864ADCE58244AD5A8710CD71852B4FA1EE6E79AA3E6116671E8AA99`.
The post-stop passive baseline passed and has SHA-256
`55967F8633D61E66D8B89C91A883A6B831C0ED1D8C25C214FE03A5DF2B4D6780`;
supplemental manifest SHA-256 is
`AB9D0181B675FADEDB1B546498CAD4904AC1A0A3AE17044B98FD70E94000A48C`,
and no target event occurred in the exact interval. Any later active run requires
new authorization and an ASCII-safe or retained-JSON location input. BDF
remains evidence only and never a selection key.

### Batch 2F permanent harness and partial runtime result

The repository-controlled
`scripts/invoke-runtime-ioctl-qualification.ps1` replaces the temporary
wrappers. It requires 64-bit Windows PowerShell 5.1, uses ASCII-safe source,
structured PnP/CIM identity data, the production Device Guard predicate and
the reviewed job-object process runner. Its modes separate complete mock
execution, passive preflight and owner-authorized runtime execution. The
19/19 offline matrix proves the seven-command allowlist, exact one-shot calls,
five-second timeout, exclusive evidence and fail-closed state transitions.

The first real passive run found a harness-only `OrderedDictionary` versus
`PSCustomObject` mismatch while all rollback bytes matched. The normalized
collector passed a fresh passive run with no command markers. The subsequent
single authorized runtime attempt passed R0-5 and R0-6. R0-7 returned four
bytes containing `0x00000110`, exit 0 and no stderr. The harness stopped before
its post-command check because the plan-derived output contract incorrectly
expected `0x1A08`. R0-8, R0-9 and both R1 commands were not attempted.

The current driver source and canonical ABI reconstruction independently
establish `0x110` as the `CKeTraceControl` descriptor size returned by
`0x00223004`; `0x1A08` was stale planning data. The permanent harness and mock
now use `0x110`, and the complete offline matrix passes again. No runtime retry
was performed. A fresh passive post-stop run confirms the same stable instance,
problem code zero, running service, loaded SYS hash, five published interfaces
and an empty target-event set. The next gate is a new explicit authorization
for exactly R0-8, R0-9 and, only after their success, the two conditional R1
reads. The already completed R0 commands must not be repeated merely to obtain
a contiguous transcript.

Private evidence identities:

| Evidence | SHA-256 |
|---|---|
| Passing passive-preflight manifest | `EACB718CC5E255DCD3DF11FE2F5F2AC33A2AACE5EA3A52445FCA07C5FACD091C` |
| Authorized-run manifest | `9BDD0BBD23962B1726350C5DAFF48465CA44410214B5D4B7D0A105A638EE9C5B` |
| R0-5 combined log / structured result | `649EE70902B0929AEDF6367A4BE8FB2AAF316A234A25466D80E3BD9694BA9520` / `DAC361A15F0783C793699743E1D5799CB8FAFF050A1FFDF134FD1295DB384A79` |
| R0-6 combined log / structured result | `2A606239022BAC70177637F88CABCDBE0C25E1A6E3491D3572E640F88C2DAA11` / `E6F2BABABD63AF4DD6CF8878A508294FDFD064403F8CA13119E8E967C39E83CA` |
| R0-7 combined log / structured result | `EB554BF844E7863BEAAA1345FEEA06F4D1EED53F3191929FE99C0E04C13E3433` / `1BBDCC976FBC0305A83DD1F067617F6CE4C1FB3E67CB3983AC3EEBBFF513ED44` |
| Post-stop passive-preflight JSON / target events / manifest | `E3AF928DD2F53AB812A18F16F283E09611BF61CB406D49FB304849DC2D1F64D8` / `7EB70257593DA06F682A3DDDA54A9D260D4FC514F645237F5CA74B08F8DA61A6` / `B791CCA4E4776AC62499D58CE871870AFBCC4CE68D3351F39BDD7DDC5CD5CEDE` |

## Stage 6 — targeted Driver Verifier and controlled PnP

Stage 6 needs a new explicit authorization for Verifier, reboot, approved
device handles/software-only controls and each PnP transition group. Do not
infer that authorization from Stages 1–5.

### 6A. Bounded metadata and ABI requests

Before any runtime authorization, run `scripts/test-runtime-baseline.ps1`.
Selection must match the exact full hardware ID and instance ID. BDF/location
is recorded only as mutable topology evidence; a changed value fails pending
explicit acknowledgement but never selects a different device.

The smallest runtime gate after a healthy passive baseline is a new
authorization for the five R0 commands that did
not run in Batch 2: the two no-ops, two four-byte size queries and zero-delay
request documented in [Runtime IOCTL safety classification](ioctl-runtime-safety.md).
Run each once with the corrected, qualified runner and a five-second timeout,
no PnP transition and no Verifier. Preserve the four already passed results;
do not repeat them merely to obtain a single transcript. Capture pre/post
device, service, interface and kernel-event snapshots.

After R0 passes, authorize R1 separately: the bounded PCI-configuration query
and reviewed START-register read. Stop on any timeout, unexpected length/value,
device problem, service/interface change or new target kernel error.

### 6B. Software lifetime checks

After a separate approval, exercise bounded open/query/close lifetime using R0
only. Run serially first, then a small fixed concurrency count. Cancellation
must be limited to the supervised user-mode process tree; no PnP state change
may occur while requests are open. Event-handle replacement, trace/stat clears
and all R2/R3 requests remain excluded.

A future pending-IRP path must prove exactly-once cancel completion and
remove-lock release. Until an approved lower-stack fault-injection harness
exists, pending lower PnP cancellation remains NOT RUN rather than PASS.

### 6C. One controlled STOP/START

Only after the handle tests close cleanly, authorize one exact-instance
`pnputil /restart-device` operation under a fresh verified kernel capture. No
device handle may remain open. Require STOP admission closure, five-interface
withdrawal, remove-lock/IOCTL drain, IRQ disconnect, DPC/timer quiescence and
no unavailable-state MMIO. START must again report lower/local/interrupt/final
success and republish exactly five interfaces. Stop without retry or automatic
rollback on any mismatch.

Disable/enable, remove/rescan and true surprise removal are later independent
matrices. Do not begin with repeated cycles. `pnputil /remove-device` is an
orderly removal and must never be reported as surprise-removal coverage.

### 6D. Targeted Verifier

The first one-boot pass should exclude DMA Verification and every resource-
fault injection option. Target only the exact image with flags `0x0002093B`:
Special Pool, Force IRQL Checking, Pool Tracking, I/O Verification, Deadlock
Detection, Security Checks, Miscellaneous Checks and DDI Compliance Checking.

```text
verifier /reset
verifier /flags 0x0002093B /driver LecS65AcqDrv.sys
verifier /bootmode oneboot
verifier /querysettings
shutdown /r /t 0
```

These commands are prepared, not executed. Configuration, one-boot mode and
reboot each require explicit authorization. `/all`, random/systematic low-
resource simulation, Force Pending I/O Requests, IRP Logging, DMA Verification
and Code Integrity Checks are excluded from the first pass. DMA Verification
does not itself launch DMA, but would mix DMA-API findings into the initial
non-DMA lifetime baseline.

Selected risks are deliberate. Special Pool can expose overruns under memory
pressure; Force IRQL can bugcheck pageable or wrong-IRQL access; I/O and DDI
verification can bugcheck malformed forwarding/completion; Deadlock Detection
tracks lock order; Pool Tracking reports leaks; Security and Miscellaneous
Checks enforce handle, buffer and other kernel contracts. Any finding, hang or
unexpected selected driver is FAIL.

Read-only readiness remains incomplete: flags are zero and no driver is
selected; `CrashDumpEnabled=7`, dump path is `C:\Windows\MEMORY.DMP`,
`AutoReboot=1`, physical memory is 8,458,924,032 bytes, the boot-volume page
file is 1,664 MiB, `C:` has about 23.9 GB free and no current `MEMORY.DMP`
exists. This does not prove that a useful kernel dump will survive a verifier
bugcheck. Before activation, separately authorize a recovery rehearsal, verify
page-file/dump sufficiency and decide whether to set `AutoReboot=0`.

Recovery commands remain:

```text
verifier.exe /reset
verifier.exe /querysettings
shutdown.exe /r /t 0
```

After boot, require `LecS65AcqDrv.sys` to be the only selected project driver.
Save `verifier /query`, `/querysettings`, System events and any untouched dump.
Run only a separately authorized R0 metadata batch; do not combine the first
Verifier boot with PnP, R1 MMIO reads, acquisition or DMA.

### 6E. Limited cancellation and teardown

After the first clean Verifier boot, repeat only the bounded R0 open/query/close
matrix. A later separately authorized single restart may test teardown with no
open requests. Cancellation with requests deliberately held across STOP is not
authorized until a reviewed software-only pending-request harness exists.

True surprise removal requires a hot-removal-capable chassis and its own risk
review. If unavailable, record it as NOT RUN.

### 6F. DMA/quarantine boundary

Do not launch DMA. Confirm from approved traces/counters that ordinary
START/STOP/REMOVE never enters the WDM v3 adapter path or creates an adapter
mapping/common buffer.

`UnknownActive` runtime injection is NOT RUN unless a separately reviewed
software-only hook can latch ownership before any mapping or hardware launch.
If such a hook is later authorized, verify irreversible admission closure and
retention through STOP/REMOVE, then restore or reboot. Never create a real
indeterminate DMA transfer for coverage and never free quarantined ownership in
the same boot.

## Prepared single PnP restart experiment

This plan is not authorized for execution. It targets only `oem101.inf`,
version `0.2026.1018.1`, loaded SYS SHA-256
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`
and instance
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39fd0a79&0&08F0`.

1. Create a new exclusive evidence root and record device, service, five-
   interface, SetupAPI and event bookmarks. Require no open driver handle,
   acquisition client, Verifier setting or existing DebugView process.
2. Revalidate DebugView CLI 5.02 x64 SHA-256
   `7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B`
   and PnPUtil SHA-256
   `CFE15658F1F4AB0E19465A15738B7FDB4BE947CDA4F2AA8B1C1B995CF50CBDDB`.
3. Start one bounded kernel-only capture:

   ```text
   dbgviewcli64.exe --kernel --verbose-kernel --no-win32 --clock-ms --format text --duration 300 --max-lines 100000 --log <new-raw-log> --no-banner
   ```

   Require the collector to remain live, the raw log to exist and an exclusive
   `capture-ready.json` record before PnPUtil is reachable. Require one
   independently attributable kernel record within the readiness window;
   otherwise stop without restart.
4. Start a read-only interface metadata watcher, then invoke once:

   ```text
   pnputil.exe /restart-device "PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39fd0a79&0&08F0"
   ```

5. Expect STOP entry/lower forwarding, five successful interface-disable
   records, IRQ disconnect and no post-quiescence hardware access. Then require
   START entry, lower dispatch/completion/final success, local resource success,
   interrupt-connect success, five indexed interface-enable successes and final
   success. Metadata must observe withdrawal and restoration to exactly five
   enabled instances.
6. Correlate UTC trace times with SetupAPI, Kernel-PnP, Code Integrity, SCM and
   System events. Record PnPUtil exit, problem code, service, loaded SYS,
   interface counts and SHA-256 for every evidence file.

Admission closure, drain and remove-lock balance are source-backed but lack a
dedicated runtime marker in the installed image. A clean restart supports but
does not directly prove those internal boundaries. Do not hold an IOCTL open
across this first cycle.

Stop without retry or automatic rollback on missing capture readiness,
collector exit, PnPUtil nonzero, reboot request, unexpected minor/status,
missing disable/enable record, IRQ error, persistent device problem,
service/image change, interface mismatch, kernel error, bugcheck, hang,
interrupt storm, acquisition or DMA indication. Recovery, rollback and reboot
each require their own authorization.

## Evidence, failure and rollback

For every executed stage retain privately:

- authorization, operator, timestamps and exact commands;
- source commit, package manifest, hashes, PDB identity and certificate;
- host/firmware/IOMMU configuration and exact device instance ID;
- Verifier settings/statistics, System events and approved driver trace;
- any untouched dump plus `!analyze -v`, `lmvm LecS65AcqDrv` and `!verifier`.

A hash mismatch, unexpected package, signature failure, verifier finding,
bugcheck, hang, stale interface, resource leak, post-remove hardware access,
unexpected DMA activity, quarantine cleanup or status mismatch is immediate
FAIL. Stop the matrix, preserve volatile evidence and use the proven recovery
path. Do not widen operations to diagnose a failure on the same boot.

Passing all applicable stages would qualify only the bounded kernel-runtime
cases above. It would not prove physical DMA bus-idle, IOMMU-safe PFN DMA,
general acquisition, HLK success, WHQL/WHCP certification or production
deployment.

## Batch 3 runtime completion and next-stage readiness

The exact `Batch3Remaining` harness path completed R0-8, R0-9 and the two
conditional R1 reads once each. The final R0/R1 status is 9/9 plus 2/2 PASS;
the device remained problem-free and no relevant event was recorded. The
private run manifest is
`D6F6EAC9CDABAEEA32DB684276D65B02036EBD697EA5AE2C47F05B9EB2F71C63`.

The prepared one-cycle PnP procedure above remains technically ready but is
not authorized. DebugView CLI 5.02 x64 is present with the pinned SHA-256,
valid Microsoft Authenticode signature, and no current DebugView process or
`DBGV` service instance. The cycle must use the exact installed instance and
package, establish capture readiness before the single `pnputil /restart-device`
call, correlate all `PNP_STATUS:` stages, and never retry automatically.

The proposed initial Verifier mask `0x0002093B` selects Special Pool, Force
IRQL Checking, Pool Tracking, I/O Verification, Deadlock Detection, Security
Checks, Miscellaneous Checks and DDI Compliance. It deliberately excludes
random/systematic low-resource injection, forced pending I/O and DMA
Verification from the first boot. That is appropriate for the first lifetime
pass because the installed driver still uses the legacy PFN-derived DMA path,
acquisition is excluded, and DMA Verification would not prove physical
retirement of that bypass path.

Current passive readiness is incomplete: Verifier is off with no selected
driver, `CrashDumpEnabled=7`, `C:\Windows\MEMORY.DMP`, `AutoReboot=1` and
overwrite enabled. The boot-volume page file is automatic but currently only
1,280 MiB for approximately 8 GiB RAM; 21.1 GB is free on C:. This does not
prove that a useful kernel dump will be captured. No settings were changed.
Before authorizing Verifier, the owner must separately approve or explicitly
accept a reviewed dump/page-file policy, recovery-path validation, the one-boot
Verifier configuration and the reboot. Do not combine the first Verifier boot
with PnP restart, device IOCTLs, acquisition or DMA.

## Batch 4 captured-restart attempt

The installed baseline passed before active work: exact PCI instance
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`, observed
BDF 5:1.0, `oem101.inf` version `0.2026.1018.1`, problem code zero, running
`LecS65AcqDrv`, five enabled legacy interfaces and loaded SYS SHA-256
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`.
Test-signing was enabled, Device Guard reported no active service, acquisition
clients were absent, the signed candidate and original `oem100.inf` rollback
set matched their frozen hashes, and Verifier was off.

The one authorized `RestartWithCapture` launcher execution began at
`2026-10-21T22:02:56.4158729Z`. DebugView started once, but its raw log still
reported zero bytes when the 15-second readiness deadline expired. The
launcher recorded `FAIL`, exit code 1, one capture invocation, zero install
invocations and zero PnP restart invocations. No restart marker or PnPUtil log
exists. The authorization was consumed and no retry occurred.

After the fail-closed decision, DebugView flushed a 458,752-byte raw file. It
contains 5,862 records, 5,861 of which are independently attributable
`STORMINI: StorNVMe` kernel records. The first record is timestamped
22:02:58.745, but the file length was not externally visible during the gate;
the last record is 22:05:11.540 and the final line is truncated. There is no
`PNP_STATUS:` record because PnPUtil was never reachable. This proves that the
failure was buffered log visibility, not missing kernel capture, but it does
not authorize retroactively treating capture as ready.

Post-run state is unchanged. DebugView and `DBGV` are absent; device, package,
service and loaded SYS identity match preflight. SetupAPI retains its prior
length, last-write time and SHA-256
`2FE8D1DFE99D97651F8DD3BB5FD8355BA7510A2064344B3CD2E26A53A618C714`.
No target Kernel-PnP, Code Integrity or SCM event occurred after the bookmark,
including Event 411. Because no PnP transition occurred, the attempt proves
nothing new about STOP/START, IRQ disconnect/reconnect, interface withdrawal
or physical DMA idle. Historical Event 411 attribution remains unchanged.

Private evidence hashes:

| Evidence | SHA-256 |
|---|---|
| Passive preflight JSON | `58A74F4B22DD04690904E43ACA2D88A7FB5836A43C0CF92A806BE954AAE84733` |
| Capture attempt marker | `73214D7AEF7A170BC2D6CAE55E9636E6852C63CF50485B4E457535441959F828` |
| Raw kernel capture | `73D3238F8C23CE3B782880C05CB0F013D4FC01D4A34C2DBE727B79BC653A47B9` |
| Launcher result | `DB4EA044BA09AAFF01B25055D33AB81CDCFDF5ECFC96F52FFD7FCC67D4951DE7` |
| Launcher exception | `57CDF5BCD4749B657E8E3C4D609232247A4609492C398D6AA39228733A52CA43` |

Real capture modes now require a readiness window of at least 60 seconds and a
capture duration that leaves at least 120 seconds after that deadline. This is
a fail-closed correction for DebugView's observed buffering; it has only mock
and source-contract validation. A future restart requires a fresh evidence
root, a newly reviewed exact command, successful live readiness and a new
explicit owner authorization.

## Batch 5 live-readiness result

The launcher now uses the existing job-object process runner with optional
exclusive live stdout/stderr files. Its gate records collector health, capture
initialization, raw-log flush and confirmed kernel-message reception as
separate properties. The gate requires a recognizable kernel record; process
existence or a created zero-byte file is insufficient. Elevated Windows
PowerShell 5.1 mocks pass 21/21, including delayed raw-file flush, early exit,
missing output, timeout, nonzero PnPUtil and no-retry/no-orphan checks. Source
and ABI contracts pass 64/64.

The single Batch 5 authorization was consumed at launcher commit
`869b3cdae0ee4ebb68f536f85247ae77c6d99f55`. Passive preflight passed, then
DebugView loaded once and emitted valid records. The first live predicate
matched only a marker at the beginning of a line, while DebugView prefixes each
real record with sequence, time, CPU and ` - `. It therefore failed closed
before creating the restart marker. Result counts are capture 1, install 0 and
PnP restart 0. The 7,655,850-byte raw log contains 100,002 timestamped
`STORMINI: StorNVMe` records from 18:46:53.439 through 18:47:06.829 and no
`PNP_STATUS:` record. DebugView stopped at its 100,000-line bound and left no
process. Raw log SHA-256 is
`63017DA62845963535E20D67138A9DBA1D05EAB4AFFA831DB25BF49E1455BF85`;
result SHA-256 is
`E44442DC406F418A3903DAD6D1C216C2E17404B8A1E83728B1E7908C53457951`;
the evidence manifest SHA-256 is
`1092480531CD50FE96757A84E7C1E43C813AB4A1718D36E543BE9FE579362E8D`.

The installed device remains `OK` with problem code zero, `oem101.inf`
0.2026.1018.1, running service, five enabled interfaces and loaded SYS SHA-256
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`.
SetupAPI remains unchanged at SHA-256
`2FE8D1DFE99D97651F8DD3BB5FD8355BA7510A2064344B3CD2E26A53A618C714`.
No target Kernel-PnP event or Event 411 occurred because PnPUtil was never
invoked. Commit `ca358d0a5ca1616377dd1308b9fb477c3e735dbc` accepts both bare mock
markers and DebugView's timestamped ` - STORMINI:`/` - PNP_STATUS:` form; it
is offline-qualified only.

### Recovery and one-boot Verifier gate

The read-only recovery audit found one healthy 256-GB GPT NVMe disk with only
a 600-MiB EFI partition and the 110.6-GiB C: partition. `reagentc /info`
reports WinRE disabled with no location. No removable or optical recovery
volume was present. BCD `{current}` boots C: with `testsigning Yes`; stale
recovery entries refer to `unknown` devices and are not a demonstrated path.
CrashControl uses automatic dumps (`CrashDumpEnabled=7`), `AutoReboot=1`,
`C:\Windows\MEMORY.DMP` and overwrite. The system-managed page file is
1,280 MiB for 8,458,805,248 bytes RAM, with about 17.6 GB free on C:.

The minimum robust preparation is: create and boot-test external Windows 10
recovery/install media; preserve the verified `oem100.inf` rollback set off
C:; separately authorize a page-file/dump policy large enough for the selected
dump; disable automatic reboot for observation if desired; and rehearse Safe
Mode plus elevated `verifier /reset` before enabling Verifier. Enabling WinRE
would require adding or safely provisioning recovery storage and must not be
assumed from the stale BCD objects.

After those gates, the future first pass targets only the exact installed
`LecS65AcqDrv.sys` and uses:

```text
verifier /reset
verifier /flags 0x0002093B /driver LecS65AcqDrv.sys
verifier /bootmode oneboot
verifier /querysettings
```

The mask covers Special Pool, Force IRQL Checking, Pool Tracking, I/O
Verification, Deadlock Detection, Security Checks, Miscellaneous Checks and
DDI Compliance. It excludes DMA verification and fault injection. `oneboot`
enables the settings only for the next Windows start and disables them for
subsequent boots. The first boot workload is limited to passive device,
service, interface and event inspection; PnP, acquisition, DMA and hardware
IOCTLs remain separate gates. A failure requires Safe Mode or external media,
`verifier /reset`, evidence preservation and a controlled reboot.

### DMA integration order

PR #27 head `1120687c181e85ade70f91d656d127ef0533d7a6` remains ABI-compatible
and offline-qualified. It associates CREATE/CLOSE with a nonpaged file context,
preserves process-shared registrations until the last file closes, serializes
file/transfer ownership under `TransferMutex`, drains contexts on REMOVE and
never retires `CurrentTransfer` or `DmaUnsafeToFree` storage. It changes no
PnP START, IRQ or public IOCTL contract and does not prove physical bus idle.

The safe stack order is PR #26, PR #27, then PR #28 plus Batch 5 tooling and
documentation. PR #27 and PR #28 cannot be merged directly without resolving
their two canonical-document conflicts. A later signed DMA-fixed candidate
needs a distinct DriverVer and full package identity. After separately approved
installation, repeat package/START health, five-interface enumeration, the 9/9
R0 and 2/2 bounded R1 matrix, captured single PnP lifetime testing and the
recoverable non-DMA Verifier pass. Acquisition and active DMA remain a later,
independent workstream.

### Verifier readiness result

The proposed `0x0002093B` mask remains appropriate for the first non-DMA WDM
lifetime pass. It includes Special Pool, Force IRQL Checking, Pool Tracking,
I/O Verification, Deadlock Detection, Security Checks, Miscellaneous Checks
and DDI Compliance, while excluding DMA Verification and resource-fault
injection.

The elevated passive audit recorded Verifier flags zero and no selected
driver, current-boot `testsigning Yes`, `CrashDumpEnabled=7`, automatic reboot,
`C:\Windows\MEMORY.DMP`, an automatically managed 1,280-MiB boot page file,
approximately 8,067 MiB visible RAM and 19.64 GiB free on C:. `reagentc /info`
reported `WinRE Status: Disabled`. Therefore neither useful dump capture nor a
recovery route has been demonstrated, and the test-signing state of a recovery
boot path cannot be verified. The private readiness JSON SHA-256 is
`5483CB3DA8E97C4604D91C73306F09F078FDDC504F4515782B8827DF75CD20FD`.

Verifier activation remains blocked until a separately authorized change and
rehearsal establish: adequate page-file or dedicated-dump backing, a usable
WinRE/Safe Mode or external recovery path, successful `verifier /reset`
recovery, and preservation of the signed test-driver boot policy. Only then may
the already documented one-boot commands be authorized. The first pass must
target only `LecS65AcqDrv.sys` and must not include PnP restart, acquisition,
DMA or hardware IOCTLs.

## Batch 6 captured PnP result

One separately authorized restart ran through the qualified launcher exactly
once. DebugView became capture-ready at `2026-10-22T19:43:15.0537626Z`; the
verified capture process was PID 4640 and had 295 seconds remaining. PnPUtil
returned zero. Invocation counts were capture 1, install 0 and restart 1.

The 887,838-byte raw log contains 42 `PNP_STATUS:` and 11,330 independently
attributable `STORMINI:` records. The old device stack received query-relations,
QUERY_REMOVE and REMOVE, disabled all five interfaces, disconnected the IRQ,
reported zero retained transfers, unmapped BARs and unloaded. Re-enumeration
then registered all five classes. START completed from lower-stack pending to
success; local resource setup, `IoConnectInterrupt`, all five interface enables
and the final IRP status were successful. No STOP_DEVICE IRP occurred.

Five `0xC00000BB` completions are directly attributed to the lower stack for
query-relations, reserved/legacy and QUERY_INTERFACE requests. None is a local
START, IRQ or publication failure. Event 411 did not recur and no target Code
Integrity, Kernel-PnP or Service Control Manager error appeared. The device,
service, package, loaded SYS and five interfaces remained healthy. This
corroborates the historical Event 411 classification as an installation
intermediate, while the exact internal Windows producer remains unproved.
The capture does not prove physical DMA bus-idle.

Private evidence hashes are: raw capture
`A6E5D87F996F32F6FC126E1ABEFF9FFAED6D83CA015D0ED57A78034237598F33`,
trace summary
`4CA9C8BF5D0FCDFA358772E36C58AA37C0A63D462BA34E27E00484F007FE7E33`,
result JSON
`1CDC1D6FF78B38EECBF156CA8405E8E85DC7AF67C03B65B993454632617B5CBF`
and evidence manifest
`283E1AD73C32DA8341CCBCEC127777410BF4507AE48515A4B9A58A36654CB077`.

## Recovery and dump preparation

The owner explicitly accepts loss and reinstallation of the expendable x64
environment, so external media and a system image were not prerequisites for
Batch 9. The separate production x86 disk and all hardware/firmware remain out
of scope. The only visible disk was the x64 GPT NVMe; it contains EFI and C:.

CrashControl is now kernel dump with automatic reboot disabled. The preserved
system-managed page file allocated 8,192 MiB after reboot. The one-boot
Verifier experiment below completed without a bugcheck and Verifier is now
inactive. Any future Verifier run requires a new authorization and fresh
identity, dump-backing and free-space checks. See
[recovery and backup readiness](recovery-and-backup.md) for optional external
recovery and destructive offline-repair procedures.

## Batch 9 Verifier, deployment and bounded hardware result

The owner accepted loss of the expendable x64 installation and removed the
external-image prerequisite for this phase while explicitly protecting the
separate production x86 disk and scope hardware. Only the x64 GPT NVMe was
visible. CrashControl was set to kernel dump with automatic reboot disabled;
the system-managed page file was 8,192 MiB after reboot.

One one-boot Driver Verifier run selected only `LecS65AcqDrv.sys` with exact
mask `0x0002093B`. The decoded checks were Special Pool, IRQL checking, pool
tracking, I/O verification, deadlock detection, security, miscellaneous and
DDI compliance. DMA verification and fault injection were absent. Runtime
counters reported 4,609,185 attempted and successful allocations, zero
failures and no bugcheck. Persistent and runtime Verifier state are inactive.

The frozen DMA-fixed package was signed without rebuilding and installed once.
The current package is `oem102.inf`, version `0.2026.1022.1`, loaded SYS
SHA-256
`D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`.
The problem code is zero, the service is running and all five interfaces are
active.

The permanent runtime harness was updated to pin that package and a newly
built x64 `lecdiag.exe` SHA-256
`0567A21992E95FFBB8E08B24ED10DA1CB4339682CB6DD31CD42F740F99E955D2`.
It executes the exact nine R0 commands before the conditional two R1 reads.
Offline qualification passes 21/21. The authorized live result is 9/9 R0 and
2/2 R1 PASS: trace size `0x110`, register-list size `0x2CAE`, PCI
`1570:0005`, observed BDF 5:1.0 and matching START reads `0x00000002`.

The installation capture preserved valid installation evidence but its
original launcher result falsely failed while Windows PowerShell serialized
about 100 MiB of already-persisted stdout. Capture children now suppress
console replay and return only a compact process result. The exclusive raw and
child logs remain authoritative; elevated Windows PowerShell 5.1 mocks pass
31/31.

One subsequent `RestartWithCapture` run invoked capture once, installation
zero times and PnP restart once. It exited successfully and retained 42
`PNP_STATUS:` records. Teardown, local resource START, IRQ connection, five
interface enables and final START all returned success. `0xC00000BB` appears
only on forwarded lower-stack query paths. No target error event occurred;
the post-restart baseline passed and DebugView left no process or kernel
component. Raw capture SHA-256 is
`9E7DB4826E6638A38347099BC5B2BB133DE1C7D40C9B8E9DB8B00EBA29C29AAF`.

No acquisition or real DMA ran. Fresh focused unchanged-source tests pass
23/23 process/file lifetime, 19/19 combined lifetime, 17/17 ownership, 39/39
completion, 2/2 interleavings, 5,000/5,000 default DMA stress, 49/49 plus
1,000/1,000 INTEN, 38/38 PnP publication and 64/64 source contracts. These
prove software ownership and irreversible quarantine contracts, not physical
DMA bus-idle, abort or final PCI-access ordering.

## Batch 10 DMA experiment gates

The Batch 10 development tree disables the two legacy PFN-based GO paths by
default. The installed `oem102.inf` baseline remains unchanged. No acquisition
or DMA experiment is authorized by the following plan.

| Order | Objective and proposed operation | Preconditions and evidence | Stop/recovery boundary |
| --- | --- | --- | --- |
| 1 | Obtain and statically review the exact U3 RTL/bitstream build, PCI core documentation and register contract. No device command. | Authorized artifacts; establish GO, busy/idle, descriptor-fetch, stop/reset and INTA ordering. | Stop if the running bitstream cannot be matched. No system mutation. |
| 2 | Attach a conventional-PCI analyzer and validate passive decode with no DMA or new MMIO. | Production x86 disk physically disconnected or electrically isolated before later active work; analyzer clock/trigger verified. | Any signal-integrity or decode uncertainty stops the plan. |
| 3 | Future read-only status characterization only if the exact status register is documented. Command must be a reviewed dedicated diagnostic, not arbitrary `lecdiag read`. | Signed instrumented build, pinned register identity, before/after health snapshot and capture. | Any undocumented value or side effect stops; cold power recovery available. |
| 4 | Smallest MAM transfer: future harness registers one 4-KiB guarded buffer through `0xCFDC2124`, starts exactly one reviewed 15-byte `0xCFDC2138` request, and intentionally retains all DMA ownership until cold power-down. The exact pointer-bearing payload must be generated by that reviewed harness; `raw-ioctl` is not suitable. | Separate authorization; production disk physically disconnected; WDM-v3 logical mapping; external analyzer; dump/recovery readiness; no XStream; immutable command/artifact hashes. | First unexpected PCI request, guard change, timeout, error IRQ or health change. Do not free or reuse mapping; power off and inspect offline. |
| 5 | Correlate `INTST.bit0`/INTA with descriptor fetches and the last host write over repeated *separately authorized* small transfers. | Stage 4 clean; exact generation/phase trace; analyzer shows all transactions. | A post-IRQ request disproves IRQ-as-drain immediately. Successful samples still do not create a universal contract. |
| 6 | Test a manufacturer/RTL-documented MAM and MTT stop/abort separately, then verify no new descriptor fetch, no host write, stable idle status and upstream drain. | Exact abort contract available; sacrificial x64 host; production disk isolated; mapping retained. | Unknown status or traffic requires cold power-off and permanent quarantine of the attempt. |
| 7 | Evaluate PCI Bus Master Disable only as the highest-risk sacrificial test. | Board/bridge documentation, external analyzer and independent drain predicate. | BME clear alone never authorizes release; any continued/posted transaction stops the experiment. |

The proposed active command is deliberately not executable today: the offline
WDM-v3 transaction components exist, but their reviewed live acquisition
integration and a physical-idle predicate do not. Creating a raw pointer-
bearing IOCTL example would bypass the ownership and safety gates.
Each active row requires a new explicit authorization and immutable artifact
identity. See [DMA architecture](dma-architecture.md).

## Batch 13 qualification gates

The Batch 13 order is fixed: immutable-tree offline validation and signing;
one captured installation; one complete R0/R1 matrix; one captured PnP
restart; then, only while the exact installed identity remains healthy, the
bounded no-launch preparation runner. Each active stage has one attempt and no
automatic retry. The no-launch runner's accepted result is
`ERROR_NOT_SUPPORTED`, not acquisition success. Any timeout, device error,
interface loss, new relevant kernel error or `UnknownActive` evidence stops
the remaining hardware stages.

Physical idle is explicitly outside this sequence. A successful restart or
successful mapping preparation cannot prove descriptor-fetch termination,
last posted-write retirement or safe post-GO mapping release.

## Batch 11 next DMA authorization boundary

The software transaction, closed launch gate and explicit no-launch WDM-v3
channel/map/descriptor/flush/free owner are offline-qualified, but this does
not satisfy the active rows above. Before the first DMA experiment, a reviewed
candidate must connect these owners to exactly one legacy acquisition request
without changing the ABI, join the borrowed request/MDL lifetime to live
remove-lock and D0/PnP rundown, and retain every post-possible-GO resource.
Separately, board/RTL or external analyzer evidence must establish a MAM/MTT
stop and physical PCI retirement predicate.

Only after those prerequisites are reviewed may the owner authorize signing,
installation and one smallest bounded transfer as separate gates. The current
Batch 11 binary is unsigned, uninstalled and has no production transition to
DeviceActive; no command in this document authorizes running it on hardware.
