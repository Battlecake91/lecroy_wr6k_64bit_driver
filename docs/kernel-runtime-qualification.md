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

## Stage 6 — targeted Driver Verifier and controlled PnP

Stage 6 needs a new explicit authorization for Verifier, reboot, approved
device handles/software-only controls and each PnP transition group. Do not
infer that authorization from Stages 1–5.

### 6A. Bounded metadata and ABI requests

The smallest next gate is the exact R0 batch documented in
[Runtime IOCTL safety classification](ioctl-runtime-safety.md). It requires a
new owner authorization for the listed device handles and IOCTLs. Run one
command at a time with a five-second job-object timeout, no PnP transition and
no Verifier. Capture pre/post device, service, interface and kernel-event
snapshots. Do not include Dallas, event registration, register write,
interrupt, acquisition or DMA controls.

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

Target this driver only:

```text
verifier /reset
verifier /standard /driver LecS65AcqDrv.sys
verifier /bootmode oneboot
verifier /querysettings
shutdown /r /t 0
```

After boot, confirm that `LecS65AcqDrv.sys` is the only project driver under
verification. Save `verifier /query`, `verifier /querysettings`, System events
and dump state. A verifier finding or boot anomaly is immediate FAIL and invokes
the proven rollback route.

Verifier activation, its one-boot setting and the reboot each require explicit
authorization. Run the R0 lifetime batch first under Verifier; do not combine
the first Verifier boot with a PnP cycle. Recovery requires console/WinRE access,
`verifier /reset`, the preserved rollback package and separately authorized
reboot or package replacement.

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
