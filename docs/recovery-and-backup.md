# Recovery, backup and crash-dump readiness

This document records recovery readiness for the WR6k test host. The owner has
explicitly accepted complete loss and reinstallation of the Windows 10 x64
development environment; an external image or recovery rehearsal is therefore
not a prerequisite for current bounded kernel qualification. That acceptance
does not cover the production x86 installation, its physical disk, the scope,
calibration/license data, EEPROM or FPGA contents.

## Current passive inventory

The host runs Windows 10 Pro x64 build 19045 in UEFI mode. Batch 9 again found
no USB disk, optical volume or mapped network storage. The only visible storage
device is one healthy GPT NVMe disk, model `HFM256GDHTNG-8510B`, serial
`ACE4_2E00_9590_A2AA.` and unique ID `eui.ACE42E009590A2AA`. It contains a
600-MiB EFI System Partition and the approximately 110.6-GiB C: volume. No
Recovery partition, removable disk, optical medium, network mapping or other
writable off-C: destination was present. C: had 17,276,026,880 bytes free at
the Batch 8 audit. Record the physical identity again at
execution time; disk numbers and recovery-environment drive letters are not
stable identities.

The owner reports that production Windows x86 resides on a physically separate
disk with an independent bootloader. That disk was not visible during Batch 9;
no command addressed, mounted or modified it. This is owner-confirmed topology,
not independently observed x64 metadata.

Batch 9 applied and verified the bounded x64 crash-dump configuration:

- 8,458,805,248 bytes of physical RAM;
- approximately 16.1 GiB free on C:;
- a preserved system-managed boot page file with 8,192 MiB allocated after
  reboot;
- `CrashDumpEnabled=2`, `AutoReboot=0`, `DumpFile=%SystemRoot%\MEMORY.DMP`
  and overwrite enabled; and
- no dedicated dump file.

The private Batch 9 baseline evidence root is
`C:\Users\LeCroyUser\.codex\driver-backups\Phase9-Baseline-20261023-175650`.
Its exact applied-state CrashControl backup SHA-256 is
`B8F8737B4893A171AAA6DED01D21B22B8A4825946904FB5E4C7A74F43FEC8B53`.
Private evidence must not be committed.

## Immutable rollback inventory

The following source copies currently exist only on C:. Copy them to a newly
created directory on positively identified external storage; never overwrite a
previous recovery set.

| Item | SHA-256 |
|---|---|
| Previous `oem100.inf` INF | `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16` |
| Previous `oem100.inf` CAT | `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` |
| Previous `oem100.inf` SYS | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` |
| Current signed INF | `ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C` |
| Current signed CAT | `2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF` |
| Current signed SYS | `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A` |
| Current matching PDB | `980161BC328FFB5214B2FA76C03B59ADA22473446DA0AE3C77F970FE0B5202A3` |
| Current package manifest | `216F15716C9496B476C877129853A07BAB59097444AA2D160E2812F25AA2DC69` |
| Public test certificate | `5B18C8C3C5D0BB8AD1EB03E685E43873630B14445AA93D6CF1860BF2A5D204FE` |

After an external destination is attached, first record `Get-Disk`,
`Get-Volume`, model, serial, unique ID, partition style, capacity and free
space. Create a new destination directory. Copy the immutable package trees,
public certificate, passive-baseline JSON, SetupAPI/event evidence and a
machine-readable SHA-256 manifest. Re-hash the destination files independently
and compare every relative path, length and hash. A copy is not complete if a
source file is absent, an extra mutable file is substituted, or the destination
resides on the system disk.

## Recovery-media creation and boot test

Use Microsoft's official Windows 10 media-creation download on a separate
working PC. Select 64-bit Windows 10 installation media and an empty USB device
of at least 8 GB. Media creation erases the selected USB and therefore requires
separate authorization plus positive identification by model, serial and
capacity. Do not use the WR6k system disk as a target.

Before booting the scope, passively verify that the finished medium is GPT/UEFI
compatible and contains `\EFI\Boot\bootx64.efi`, a Windows `sources` directory
and readable setup/recovery files. Record the media volume identity and hashes
of its boot metadata. A separate authorization is required to select the USB
boot device and perform one boot test. The test passes only if the Windows
recovery command prompt opens and can read both the external recovery set and
the internal Windows and EFI volumes without writing them.

Microsoft source: [Create Windows 10 installation
media](https://www.microsoft.com/software-download/windows10).

## Complete system image

The Windows volume currently uses roughly 101.4 GiB. The external image target
must have at least the used Windows and EFI capacity plus filesystem and growth
margin; at least 128 GiB free is the narrow minimum and 256 GiB free is the
preferred target. It must not be the source disk.

After the target is identified and a live VSS backup is separately authorized,
the proposed elevated command is:

```text
wbadmin start backup -backupTarget:<EXTERNAL-VOLUME> -include:C: -allCritical -quiet
```

Do not run this command merely to test syntax. After completion, use
`wbadmin get versions -backupTarget:<EXTERNAL-VOLUME>` and
`wbadmin get items -version:<VERSION> -backupTarget:<EXTERNAL-VOLUME>`, preserve
the logs and hash the backup catalog/manifests. Recovery readiness additionally
requires a separately authorized restore rehearsal to different storage or an
equivalent disposable target. Tool success alone does not prove that the image
can be restored.

Microsoft source: [`wbadmin start backup`](https://learn.microsoft.com/windows-server/administration/windows-commands/wbadmin-start-backup).

### Versioned execution workflow

`scripts/manage-system-image-readiness.ps1` implements four explicit modes:

- `Audit` inventories disks and mounted volumes without selecting a target;
- `Plan` requires an existing local target root plus exact expected disk and
  volume unique IDs;
- `Execute` additionally requires elevation, `-ExecuteAcknowledged` and a new
  evidence directory; and
- `VerifyCatalog` performs only a version or item catalog query.

The target guard rejects the boot/system disk, mismatched identities, unhealthy,
offline or read-only media, non-NTFS volumes, less than 128 GiB free and any
existing `WindowsImageBackup` tree. Execute
invokes exactly one `wbadmin start backup` operation, never retries it and, only
after success, runs one read-only `wbadmin get versions` catalog query. Separate
stdout, stderr, process and combined logs are preserved by the versioned native
process runner. A catalog success still records `RestoreValidated=false`.

The exact future command is generated only after the target identities pass:

```text
wbadmin start backup -backupTarget:<APPROVED-VOLUME> -include:C: -allCritical -quiet
```

Mocked success, failure, catalog failure, identity mismatch, system-disk,
filesystem, capacity, existing-backup, acknowledgement and non-overwrite cases
pass 21/21. No
live WBAdmin backup was run.

## Kernel-dump recommendation

Use a kernel memory dump, not a complete dump. The `RAM + 257 MiB` initial
page-file rule applies to a complete memory dump and must not be copied into
this plan. Kernel-dump demand depends on kernel virtual-memory use and can vary
substantially. Keep the boot page file system-managed, select
`CrashDumpEnabled=2`, set `AutoReboot=0`, preserve overwrite behavior and keep
`%SystemRoot%\MEMORY.DMP`. A dedicated dump file offers no resilience here
because the host has only the same internal disk.

The versioned `scripts/manage-crash-dump-readiness.ps1` implements `Audit`,
`Plan`, `Apply`, `Verify` and exact `Restore` modes. `Apply` requires elevation,
an explicit acknowledgement and a new exclusive backup path. It changes only
`CrashDumpEnabled` and `AutoReboot`; it neither changes the page file nor adds a
dedicated dump. In this batch only the real read-only audit and mocked
apply/restore path were initially executed; Batch 9 subsequently completed the
authorized real apply and post-reboot verification.

Batch 9 completed the authorized apply, reboot and verification. The one-boot
non-DMA Verifier run completed without a bugcheck, and Verifier is inactive.
Future Verifier runs remain separate authorizations and must again verify
page-file allocation, free space, current boot identity and exact target.

Microsoft sources: [kernel memory
dumps](https://learn.microsoft.com/windows-hardware/drivers/debugger/kernel-memory-dump)
and [page-file sizing for crash
dumps](https://learn.microsoft.com/troubleshoot/windows-client/performance/how-to-determine-the-appropriate-page-file-size-for-64-bit-versions-of-windows).

## Offline recovery playbook

Every mutating step below is a separate recovery action. Preserve dumps and
metadata first, stop on identity ambiguity, and never substitute cached disk or
volume numbers.

1. Boot the verified external Windows medium and select **Repair your
   computer**, **Troubleshoot**, **Advanced options**, **Command Prompt**.
2. In `diskpart`, run only `list disk`, then select a disk only after matching
   model/capacity against the recorded NVMe identity. Use `detail disk`,
   `list partition` and `list volume`. Assign temporary letters only after
   positive identity. Locate Windows by
   `<W>:\Windows\System32\Config\SYSTEM`; locate EFI by FAT32 contents
   `<S>:\EFI\Microsoft\Boot\BCD`.
3. Identify the external destination independently. Before repair, copy
   `<W>:\Windows\MEMORY.DMP`, minidumps, SetupAPI logs and relevant event logs
   to a new external evidence directory and hash them.
4. Inspect the offline store with
   `bcdedit /store <S>:\EFI\Microsoft\Boot\BCD /enum all`. Record the exact
   Windows loader GUID. Do not use `{current}` in the recovery environment.
5. Preferred Verifier recovery is a one-time Safe Mode boot for that exact
   loader, followed by elevated `verifier /reset`, `verifier /querysettings`
   and a controlled reboot. Remove the exact Safe Mode BCD setting after the
   reset is verified. Each BCD write and reboot requires authorization.
6. If Windows cannot reach Safe Mode, back up the offline SYSTEM hive before
   any edit. Load it under a temporary name, read `Select`, select the actual
   `ControlSet00n`, and inspect
   `Control\Session Manager\Memory Management` values `VerifyDrivers` and
   `VerifyDriverLevel`. Deleting or changing them is a last-resort authorized
   action; unload the hive and retain the before/after transcript.
7. If test-signing state is wrong, change only the exact offline loader entry
   with `bcdedit /store ... /set <loader-guid> testsigning on`, and only after
   confirming Secure Boot remains disabled. Never rewrite the entire BCD store
   when one setting is sufficient.
8. For a driver boot loop, use
   `dism /Image:<W>:\ /Get-Drivers /Format:Table` and identify the candidate by
   published name, original INF, version and independently hashed files. An
   offline `Remove-Driver` is destructive and separately authorized. Restore
   the hash-verified old package only after the failing package is identified;
   use online PnPUtil after a successful boot when possible. Do not infer the
   target package solely from the last device binding.
9. Restore a saved BCD file only when the physical disk, EFI partition and BCD
   identity match the backup. Prefer a minimal setting repair to whole-store
   replacement.
10. If the NVMe is inaccessible, stop. The current system has no second copy.
    Replace the disk and restore the independently verified system image or
    reinstall Windows, then restore only hash-verified packages and evidence.

`scripts/validate-recovery-layout.ps1` is the read-only pre-drill validator for
already identified roots. It requires the offline SYSTEM hive and kernel, the
EFI Microsoft BCD store, the x64 `EFI\Boot\bootx64.efi` file, recovery
`sources\boot.wim` and, when supplied, the rollback manifest. It hashes those
markers, rejects missing or overlapping roots and writes only an optional new
result file. Mocked valid, missing-marker, overlap and non-overwrite cases pass
6/6. Passing this validator does not prove firmware bootability; the external
boot still requires separate authorization.

## Readiness decision

The scripts and commands are prepared, but operational recovery is **not yet
ready**. The gate requires all of: boot-tested external media, a verified off-C:
rollback/evidence copy, a verified restorable image, an authorized dump-policy
apply plus post-reboot verification, and a successful Safe Mode or offline
Verifier-reset rehearsal. None may be inferred from documentation alone.
