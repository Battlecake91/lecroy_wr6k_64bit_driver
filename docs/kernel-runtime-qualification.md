# Kernel-runtime qualification plan

This plan is for the merged PR #10 lifetime and interrupt hardening. It is a
future execution procedure, not evidence that the driver has been installed,
loaded, run under Driver Verifier or exercised on hardware.

Use only a disposable Windows x64 test system that can be reimaged and, for
device PnP tests, disposable or otherwise recoverable WR6k-compatible test
hardware. Never use the licensed production oscilloscope. A virtual machine
without PCI passthrough can qualify install/uninstall failure paths but cannot
qualify START, interrupts, DMA or physical removal.

## Safety boundary

The following operations are prohibited throughout this plan:

- acquisition, live legacy PFN DMA, or activation of the staged WDM v3 DMA path;
- arbitrary MMIO or register writes;
- `SetOneRegister` (`0x0022303C`), including same-value writes, unless a later
  owner-approved test plan explicitly authorizes one validated register;
- Dallas EEPROM reads that expose licensed contents, Dallas writes/deletes,
  serial FPGA/GPIO writer `0xCFDC2130`, FPGA programming or device reset;
- nonzero synthetic interrupt masks, forced hardware interrupts or guessed
  IRQ/register values;
- release of quarantined resources after `UnknownActive`; quarantine is
  irreversible until reboot and does not prove physical DMA retirement.

`CompletionObserved` is not `IdleProved`. Successful legacy PFN cleanup remains
an unverified compatibility exception. Active PFN DMA is not generally
IOMMU-safe, physical DMA retirement is not proven, and live WDM v3 DMA remains
disabled.

## 1. Freeze the test input and recovery path

1. Start from a fresh system image or snapshot. Record Windows build, firmware,
   chipset, IOMMU/Memory Integrity state and PCI device instance ID. Confirm
   local console or out-of-band access and a bootable recovery environment.
2. Use a package built from the exact reviewed commit with SDK/WDK
   10.0.28000.0. Record SHA-256 hashes of the INF, CAT and SYS, and retain the
   matching PDB privately. Do not commit dumps, PDBs, certificates, EEPROM
   contents or machine-specific traces to this public repository.
3. Confirm the package is test-signed for this isolated host. Export the
   current driver package and system configuration before replacement.
4. Save the current crash-control settings, then configure a kernel dump and
   disable automatic reboot:

```powershell
reg.exe export HKLM\SYSTEM\CurrentControlSet\Control\CrashControl `
    C:\Qualification\CrashControl-before.reg /y
$crash = 'HKLM:\SYSTEM\CurrentControlSet\Control\CrashControl'
New-ItemProperty $crash CrashDumpEnabled -PropertyType DWord -Value 2 -Force
New-ItemProperty $crash DumpFile -PropertyType ExpandString `
    -Value '%SystemRoot%\MEMORY.DMP' -Force
New-ItemProperty $crash Overwrite -PropertyType DWord -Value 0 -Force
New-ItemProperty $crash AutoReboot -PropertyType DWord -Value 0 -Force
New-ItemProperty $crash LogEvent -PropertyType DWord -Value 1 -Force
```

Use a system-managed boot-volume page file and verify adequate free space.
Microsoft documents `CrashDumpEnabled=2` as a kernel dump and the default file
as `%SystemRoot%\MEMORY.DMP`; see
[kernel memory dumps](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/kernel-memory-dump)
and [system failure/recovery settings](https://learn.microsoft.com/en-us/troubleshoot/windows-client/performance/configure-system-failure-and-recovery-options).

5. Before enabling Driver Verifier, prove the recovery route once: boot the
   recovery environment, locate the Windows volume, and return without making
   changes. Keep the original image/snapshot and package available off-host.

## 2. Driver Verifier setup and recovery

Run an elevated command prompt. Target only this driver; do not select all
drivers. The standard profile includes Special Pool, IRQL checking, pool
tracking, I/O verification, deadlock detection, DMA verification, security,
miscellaneous and DDI compliance checks.

```text
verifier /reset
verifier /standard /driver LecS65AcqDrv.sys
verifier /bootmode oneboot
verifier /querysettings
shutdown /r /t 0
```

After boot, save `verifier /query` and `verifier /querysettings` output and
confirm that `LecS65AcqDrv.sys` is the only project driver under verification.
Use `oneboot` so an additional reboot clears the active session unless it is
deliberately configured again. See Microsoft's
[Verifier command reference](https://learn.microsoft.com/en-us/windows-server/administration/windows-commands/verifier)
and [driver-selection guidance](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/selecting-drivers-to-be-verified).

Normal shutdown procedure:

```text
verifier /reset
shutdown /r /t 0
```

If the system cannot boot, enter Windows Recovery Environment or Safe Mode,
run `verifier /reset` from an elevated command prompt against the installed
Windows instance, and reboot. If recovery is uncertain, restore the known-good
image instead of repeatedly booting. Preserve any dump before rollback.

## 3. Baseline load and read-only checks

Install the exact test-signed package only after the hash check. Use the
package directory containing the matching INF, CAT and SYS:

```text
pnputil /add-driver .\LecS65AcqDrv.inf /install
pnputil /enum-devices /instanceid "<device-instance-id>" /drivers
```

Confirm one successful START, the expected service/image, exactly one present
instance in each of the five expected device-interface classes, no duplicate or
unexpected interface class, no verifier report and no unexpected System event.
With XStream closed, run
the established `scripts/test-driver.ps1 -Mode Hardware` batch once. It is
limited to the documented eleven low-impact ABI checks. Do not add
`-IncludeErrorStatus`; that optional operation consumes the error-status latch.
The `CFDC2400` requests in this batch contain only a zero mask but may process
an already pending source, so stop immediately if the device is not idle.

Record each command, exit code, driver trace, event-log export and verifier
snapshot. A failure stops the phase; it is not a reason to widen the allowed
hardware operations.

## 4. PnP START, STOP and REMOVE matrix

Use the exact instance ID, never a broad class or hardware-ID filter. PnPUtil
syntax is documented by Microsoft in the
[PnPUtil command reference](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/pnputil-command-syntax).

1. Run 100 disable/enable cycles. Each disable must complete within 30 seconds,
   report the device stopped, and each enable must produce exactly one healthy
   START and interface publication.
2. Run 25 `pnputil /restart-device "<device-instance-id>"` cycles with the same
   30-second bound.
3. Run 25 orderly remove/rescan cycles:

```text
pnputil /remove-device "<device-instance-id>"
pnputil /scan-devices
```

   Re-resolve and record the instance ID after each scan before the next cycle.
4. Exercise true `SURPRISE_REMOVAL` only on a hot-removal-capable disposable
   chassis with acquisition and all DMA disabled. Perform 10 physical removal /
   rescan cycles. `pnputil /remove-device` is an orderly remove and must not be
   reported as surprise-removal coverage. If the platform cannot generate a
   genuine surprise removal safely, record this row as **NOT RUN**, not PASS.

At every STOP/REMOVE boundary, verify that user handles close, all five
interfaces are unpublished, new IOCTLs are rejected, the interrupt is
disconnected before BAR teardown, queued DPC/timer work drains, and no driver
access occurs after the device becomes unavailable. Any timeout, verifier
finding, bugcheck, stale
interface, post-remove MMIO or unexplained retained ordinary resource is FAIL.

## 5. Interrupt, DPC and IRP lifetime

No interrupt is injected. Coverage is limited to naturally delivered idle
interrupts and the documented zero-mask boundary in the safe ABI batch.

1. Under Verifier, run 100 concurrent open/read-only-query/close loops while a
   second controller performs the disable/enable matrix. Accept either a
   successful query before admission closes or the documented unavailable/
   delete-pending status after closure; reject hangs, stale success after
   removal and inconsistent byte counts.
2. For remove-lock rundown, issue 100 bounded 250-ms software-only delay IOCTLs
   (`0x00222C00`, little-endian input `FA000000`, zero output) and begin an
   orderly disable 25 ms after each request starts. The admitted request must
   finish, STOP must wait for it, and the 30-second PnP bound must hold. This
   native implementation deliberately omits the legacy auxiliary hardware
   toggle, so this case must cause no MMIO.
3. Run a qualification harness that opens the device with
   `FILE_FLAG_OVERLAPPED`, issues only read-only or software-only controls, and
   calls `CancelIoEx` while PnP admission is closing. The current dispatch path
   completes DeviceControl IRPs synchronously and owns no cancel queue; a
   request that has already completed may therefore return `ERROR_NOT_FOUND`
   from `CancelIoEx`. Record that outcome rather than claiming cancellation.
   Any future path that returns `STATUS_PENDING` must prove exactly-once cancel
   completion and remove-lock release before it can pass.
4. Repeat the pending-lower-stack START/REMOVE cases with an approved test
   filter or fault-injection harness. Until that harness exists, runtime
   cancellation of a pending lower PnP IRP is an open qualification gate; the
   existing fake-lower-stack tests are not kernel proof.

Pass requires zero verifier violations, deadlocks, leaked remove locks,
double completion, completion after object deletion, post-remove ISR/DPC MMIO,
or outstanding driver-owned timer/DPC work.

## 6. DMA and quarantine gates

Do not launch DMA during this qualification stage. Confirm from traces and
debug counters that the staged WDM v3 adapter path is never entered and no
adapter mapping or common buffer is created by ordinary START/STOP/REMOVE.

`UnknownActive` may be tested only with a separately reviewed test-only hook
that changes software ownership state before any hardware launch or mapping.
Once latched, verify irreversible admission closure and retained lifetime
ownership through STOP/REMOVE; then reboot or restore the image. Never attempt
cleanup, restart or release in the same boot. Without that safe hook, record
kernel-runtime quarantine injection as **NOT RUN**. Do not create a real
unknown-active transfer merely for coverage.

## 7. Evidence, pass/fail and rollback

For every phase retain privately:

- exact Git commit, package hashes, build log and PDB identity;
- Windows/firmware/IOMMU configuration and exact device instance ID;
- command transcript with timestamps and iteration counts;
- Driver Verifier settings/statistics, System event log and driver trace;
- for a bugcheck, the untouched dump plus `!analyze -v`, `lmvm LecS65AcqDrv`
  and `!verifier` output from WinDbg.

The phase passes only if every required iteration completes, all expected
statuses and ordering checks match, no verifier or event-log defect appears,
and no dump is generated. A verifier violation, bugcheck, hang, resource leak,
post-remove hardware access, unexpected DMA activity, quarantine cleanup or
status mismatch is an immediate FAIL.

On failure, stop the matrix, preserve volatile evidence, run `verifier /reset`
when the host is stable, power down, and restore the known-good image/snapshot.
If `UnknownActive` was reached, do not unload, restart the device or free the
transfer in the same boot. Never continue qualification on the production
oscilloscope. A dump can contain sensitive machine data and must not be
committed to this public repository.

Passing this plan would qualify the described kernel-runtime cases only. It
would not prove physical DMA bus-idle, IOMMU-safe PFN DMA, general hardware
acquisition, HLK success, WHQL/WHCP certification or production deployment.
