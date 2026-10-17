# Phase 2C installation and recovery preparation

This document is the controlled bridge between offline package creation and a
future first driver START. It records preparation only. None of the commands
under an authorization gate were executed while preparing this document.

## Current preparation evidence

- The owner designated this WR6k Windows installation as an expendable
  development/test environment and accepted crash, reinstallation, local-data
  loss and hardware-damage risk. Private-key use, installation, Driver
  Verifier, PnP transitions, device handles and acquisition remain separately
  authorized operations.
- The active service remains `LecS65AcqDrv`, `Running`, `Manual`, from
  `C:\Windows\system32\drivers\LecS65AcqDrv.sys`. No service or device state
  was changed.
- Driver Verifier has flags `0x00000000` and no selected drivers.
- Crash control is `CrashDumpEnabled=7` (Automatic Memory Dump),
  `AutoReboot=1`, `Overwrite=1`, `LogEvent=1`, with
  `C:\Windows\MEMORY.DMP`. The system has 8,458,924,032 bytes of physical
  memory, a 1,856 MiB allocated boot-volume page file and 22,305,583,104 bytes
  free on `C:`. This inventory does not prove that a kernel dump will survive
  the planned failure mode.
- `reagentc /info` and `bcdedit /enum all` could not be read without elevation.
  WinRE availability, boot-entry identity, recovery-console access and an
  actual reinstall/restore rehearsal therefore remain blockers.

## Preserved previous package

The currently installed `oem100.inf` package was exported, without deletion or
device action, to the private non-repository directory:

```text
C:\Users\LeCroyUser\.codex\driver-backups\lecroy-oem100-20261017
```

The export completed successfully and reproduced the Phase 1 hashes:

| File | Bytes | SHA-256 |
|---|---:|---|
| `lecs65acqdrv.inf` | 1,247 | `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16` |
| `LecS65AcqDrv.cat` | 2,461 | `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` |
| `LecS65AcqDrv.sys` | 67,448 | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` |

These files are private recovery material and must not be committed. Copy them
to recovery media before authorizing installation. Recompute all three hashes
from that media before relying on it.

## Rollback command sheet

Run only after separate authorization and only from an elevated console. First
verify the recovery copy against the table above. The normal rollback command
is:

```powershell
pnputil.exe /add-driver `
  'C:\Users\LeCroyUser\.codex\driver-backups\lecroy-oem100-20261017\lecs65acqdrv.inf' `
  /install
```

That command may replace the selected package and cause a PnP transition. It
was not executed. Do not delete either `oem100.inf` or the candidate package as
part of first rollback; deletion is unnecessary and reduces recovery options.
After rollback, verify the device instance, selected INF, service image and SYS
hash before launching XStream.

If Windows cannot boot normally, use the previously verified recovery path to
reach Safe Mode or WinRE. From an elevated Safe Mode console, clear Verifier:

```text
verifier.exe /reset
shutdown.exe /r /t 0
```

If normal package rollback is unavailable, reinstall Windows and import the
hash-verified previous INF from recovery media. Exact WinRE volume letters and
BCD identifiers must be captured during the recovery rehearsal; they must not
be guessed during a failure.

## Crash-dump and recovery gate

Before installation, separately authorize and perform all of the following:

1. Copy the previous package and this command sheet to boot-independent media.
2. Run elevated `reagentc.exe /info` and `bcdedit.exe /enum all`; record the
   enabled WinRE location and the exact current/default boot identifiers.
3. Boot the recovery environment once and prove keyboard, display, Windows
   volume and recovery-media access without changing the driver.
4. Confirm a boot-volume page-file policy sufficient for a kernel dump. Do not
   infer sufficiency solely from the current 1,856 MiB allocation.
5. Set `AutoReboot=0`, retain dump logging, and choose an explicit overwrite or
   preservation policy. Save the prior CrashControl values before changing
   them.
6. Reboot only under its separate authorization, then re-read CrashControl,
   page-file and WinRE state. A registry write without a confirming reboot is
   not a passed gate.

No crash-control, page-file, BCD, WinRE or reboot change was made during the
offline preparation.

## Candidate verification gate

The validated output at workflow commit `b0d3a2c` is unsigned and is not an
installation candidate. Before any future install, require a newly created,
owner-approved signed output and compare its manifest plus INF, CAT, SYS and
PDB hashes. Require all of the following:

- exact approved `driver/` and `include/` tree IDs;
- approved Debug/Release selection and `DriverVer` ranking;
- approved exact certificate thumbprint and private-key use;
- local signature and catalog-membership verification;
- Microsoft kernel-policy verification classified accurately for the chosen
  signing method;
- the distributable directory contains exactly INF, CAT and SYS;
- Driver Verifier remains off before the first installation; and
- the old package and recovery procedure remain available.

Any mismatch creates a new candidate and returns to offline validation.

## Bounded first-START procedure

This procedure requires a new explicit authorization for one installation and
one PnP START. It does not authorize XStream, a device handle, an IOCTL, an
additional restart or any acquisition.

1. Close XStream and every process that could open the driver. Confirm Verifier
   is off and capture the pre-install System and Code Integrity event-log
   bookmarks.
2. Recompute the approved candidate INF/CAT/SYS hashes in place. Stop on the
   first mismatch.
3. From one elevated console, install only the frozen INF:

   ```text
   pnputil.exe /add-driver <approved-package>\LecS65AcqDrv.inf /install
   ```

4. Permit only the installation-triggered START. Do not issue disable, enable,
   restart, remove, rescan or reboot commands.
5. Without opening an interface, require device problem code zero, service
   `Running`, the approved SYS hash, the expected selected INF and exactly one
   interface instance in each of the five documented legacy classes.
6. Compare new System, Code Integrity and PnP events with the pre-install
   bookmarks. Stop on a signature error, resource mismatch, unexpected extra
   START, bugcheck, hang, interrupt storm, acquisition indication or dump.
7. Preserve evidence and request the next authorization. Do not retry a failed
   START. Use the rollback sheet or reimage instead.

## Later targeted Verifier preparation

Verifier remains a later gate. After a successful first START and a separate
authorization, the proposed bounded configuration is restricted to the exact
image name and one boot:

```text
verifier.exe /standard /driver LecS65AcqDrv.sys
verifier.exe /bootmode oneboot
verifier.exe /querysettings
shutdown.exe /r /t 0
```

The reboot is itself gated. Recovery operators must have the `verifier /reset`
sheet available before enabling this configuration. Do not select `/all`, do
not add random or systematic low-resource simulation to the initial pass, and
do not combine the first Verifier boot with acquisition, DMA or an unreviewed
PnP matrix.

## Remaining blockers before installation

1. Approve the exact runtime configuration and final `DriverVer`.
2. Approve the exact certificate thumbprint and one private-key signing run.
3. Produce and verify a signed immutable candidate; the current set is
   intentionally unsigned.
4. Complete elevated WinRE/BCD inventory, recovery-media copy and an actual
   recovery-console/reinstall rehearsal.
5. Approve and verify crash-dump/page-file changes and the required reboot.
6. Grant a separate authorization naming the frozen hashes for one install and
   one initial START.

Acquisition and DMA remain a separate workstream. In particular, ordinary
kernel and PnP success must not activate the inactive WDM v3 DMA path, and
physical DMA retirement remains unproved.
