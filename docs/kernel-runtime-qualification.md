# Phase 2C kernel-runtime qualification plan

This is the execution procedure for development merge commit
`cb5010a4712f25de7c56d23d0d7502683542058c`. Phase 2C Stage 1 completed the
read-only source, package, toolchain, certificate-metadata and accessible-host
audit. Its exit gate is not met because no disposable/restorable test host,
recovery owner or rollback proof was provided. No later stage has been
executed. Passing offline builds and Dry tests does not authorize driver
installation, loading, device access, Driver Verifier, PnP transitions or
hardware tests.

The six stages below are strictly ordered and independently authorized. An
authorization for one stage does not authorize the next. Preserve the evidence
and obtain the stated approval at every exit gate before continuing.

## Absolute safety boundary

Use only a disposable or reliably reimageable Windows x64 test system. Any
stage that starts the driver on PCI hardware requires disposable or otherwise
recoverable WR6k-compatible hardware. Never use the licensed production
oscilloscope for driver replacement, Verifier or PnP testing.

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
| 1. Read-only validation | Build host and intended disposable test host; no WR6k required | Read package/system metadata and export non-sensitive evidence | None |
| 2. Package creation/signing | Offline build/signing host; no WR6k attached or required | Use the named test certificate/private key and create signed artifacts | Files and certificate-use audit only; no Driver Store or PnP change |
| 3. Recovery/rollback proof | Disposable test host with restorable image; WR6k not required | Snapshot/reimage operations, recovery boot, reboot and crash-control changes | Test-host configuration and reboots only |
| 4. Initial load | Disposable test host plus recoverable WR6k-compatible PCI hardware for a real START | Driver Store installation and one initial PnP START with automatic startup MMIO | Install and initial START only |
| 5. Five-interface verification | Same successfully started disposable host/hardware | Enumeration-only interface inspection | None beyond the already authorized loaded state |
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
3. On the intended disposable test host, record Windows build, architecture,
   firmware mode, Secure Boot, test-signing, hypervisor, VBS/HVCI, IOMMU,
   Driver Verifier state and available dump storage.
4. If a WR6k device is present, enumerate only PnP identity, current package,
   service state and assigned resources. Do not open an interface or issue an
   IOCTL. Preserve the existing package identity and hashes privately.
5. Confirm that the host is disposable/restorable and is not the licensed
   production oscilloscope.

### Exit gate

PASS requires an exact source identity, complete read-only inventory, no
unexpected active Verifier state and a named recovery owner. Any ambiguity is a
stop. Stage 1 approval does not permit signing or package creation.

The audit result and exact host/tool/package evidence are recorded in the
[Phase 2C Stage 1 handoff](handoffs/phase-2c-stage-1-read-only-audit.md).
Source and toolchain identity, INF intent and certificate metadata are
verified. The accessible host is the licensed production oscilloscope, not an
eligible test host; recovery identity and restore proof are absent. Stage 1 is
therefore stopped at this exit gate.

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

Do not use `scripts/build-sign-load-driver.ps1` for Stage 2. That script
continues from package creation into `pnputil` installation, device restart
and `lecdiag` device access. A package-only reviewed wrapper or individually
reviewed commands are required before Stage 2 may execute.

### Exit gate

PASS requires successful signature/catalog verification and a complete frozen
manifest. No package may exist in the test host's Driver Store. Stage 2
approval does not permit installation, loading or PnP activity.

## Stage 3 — recovery and rollback verification

### Preconditions

- Stage 2 artifact hashes are accepted.
- The owner has authorized snapshot/reimage operations, recovery boot, reboot
  and crash-control changes on the disposable host.
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
- The owner has authorized one installation and initial START on disposable,
  recoverable hardware, including the driver's automatic startup MMIO.
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

## Stage 6 — targeted Driver Verifier and controlled PnP

Stage 6 needs a new explicit authorization for Verifier, reboot, approved
device handles/software-only controls and each PnP transition group. Do not
infer that authorization from Stages 1–5.

### 6A. Targeted Verifier

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

### 6B. Software/read-only lifetime checks

Only after separate approval, run bounded concurrent open/query/close tests and
the documented software-only 250-ms delay control while admission closes. Do
not run the Hardware suite by default: even its zero-mask boundary may process
an already pending source. No acquisition, DMA, nonzero interrupt mask or
hardware register control is allowed.

A future pending-IRP path must prove exactly-once cancel completion and
remove-lock release. Until an approved lower-stack fault-injection harness
exists, pending lower PnP cancellation remains NOT RUN rather than PASS.

### 6C. Orderly PnP matrix

Use the exact instance ID, never a class or hardware-ID wildcard:

1. 100 disable/enable cycles, each bounded to 30 seconds.
2. 25 `pnputil /restart-device "<exact-device-instance-id>"` cycles.
3. 25 orderly remove/rescan cycles, re-resolving the instance ID after every
   scan.

At every STOP/REMOVE boundary require closure of new admission, five-interface
unpublication, remove-lock rundown, interrupt disconnect, DPC/timer drain and
no post-unavailable hardware access. Every START must restore exactly the five
expected interfaces and no alias.

True surprise removal is a separate optional authorization. It requires a
hot-removal-capable disposable chassis and recoverable WR6k hardware with all
acquisition and DMA disabled. `pnputil /remove-device` is orderly removal and
must never be reported as surprise-removal coverage. If suitable hardware is
unavailable, record surprise removal as NOT RUN.

### 6D. DMA/quarantine boundary

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
