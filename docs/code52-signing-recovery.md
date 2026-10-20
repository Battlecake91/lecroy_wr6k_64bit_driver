# Code 52 signing-policy diagnosis and recovery plan

## Scope and result

This is a passive diagnosis of the post-reboot Code 52 state observed on
2026-10-20. No BCD, certificate-store, driver, Driver Store, PnP, service or
hardware state was changed. The diagnosis used the repository-controlled
`scripts/collect-code52-signing-diagnostics.ps1` collector from an elevated
64-bit Windows PowerShell 5.1 process.

The root cause is established with high confidence: the Windows boot loader
entry actually used by the current installation does not contain
`testsigning`, while the installed driver is locally test-signed and does not
chain to a Microsoft kernel-policy root. Windows therefore rejected the
unchanged image with `STATUS_INVALID_IMAGE_HASH` (`0xC0000428`). This is a
boot-policy failure, not evidence of package corruption, a changed driver, or
a new PnP implementation defect.

## Exact evidence

The final read-only collection is retained outside the public repository at
`Phase2C-Code52-SigningRecovery-20261020-attempt3`. Its manifest SHA-256 is
`9544E8EDA2ED225404C112E6E4FBBF7DEAFF2A6960E1823422184E987A8464D8`.
The first attempt stopped safely because Windows 10 does not support the
requested PnPUtil `/files` option; the corrected collector used only supported
enumeration commands. The final collection completed with outcome `PASS`.

The device retained stable instance ID
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`, hardware
IDs, `oem101.inf` and version `0.2026.1018.1`. It reported problem code 52;
the service was stopped. The installed system image, Driver Store package and
frozen package all retained the approved hashes:

| Artifact | SHA-256 |
|---|---|
| INF | `ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C` |
| CAT | `2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF` |
| signed SYS | `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A` |

At `2026-10-20T17:55:51.3716774Z`, Code Integrity event 3004 recorded an
image-integrity/hash verification failure for `LecS65AcqDrv.sys` with
`SecureRequired=0x1`. At `2026-10-20T17:55:51.3729101Z`, Kernel-PnP event 219
recorded status decimal `3221226536`, exactly `0xC0000428`, for the same
instance and service. Event 3085 reported disabled WHQL enforcement for that
boot, but the subsequent target-specific 3004/219 pair directly proves that
the local test signature was still rejected; 3085 is not treated as proof
that Test Mode was enabled.

The elevated `bcdedit /enum {current}` output contains no `testsigning`
element. The boot manager and full enumeration also contain no such element.
Secure Boot is supported but disabled. VBS, HVCI, hypervisor and WDAC policy
enforcement are inactive, and no active Code Integrity policy file was found.
Those mechanisms are therefore not the current blocker.

The approved certificate remains in `LocalMachine\My`, `LocalMachine\Root`
and `LocalMachine\TrustedPublisher`. Its subject, thumbprint, Code Signing EKU
and validity match the frozen package. Local Authenticode and `/pa` validation
succeed for SYS and CAT, and catalog membership succeeds. SignTool `/kp`
fails as expected because the test certificate does not chain to a Microsoft
kernel-policy root. This separates intact local package trust from the stricter
kernel image-load policy used outside Test Mode.

The previously retained 2026-10-18 preflight records `{current}` with
`testsigning Yes`, boot manager device `HarddiskVolume4`, and a KINGSTON SATA
firmware entry. The current boot manager uses `HarddiskVolume1`; the visible
boot/system disk is an HFM256GDHTNG-8510B NVMe device, and the current loader
identifier differs. The same Windows installation is therefore now being
started through a different EFI/BCD source that lacks the required element.
Why the prior KINGSTON path is no longer visible or selected is not proven.

A signed AOMEI `ampa.sys` disk-management component was installed between the
two relevant boots. This is verified temporal evidence, not proof that AOMEI
changed the firmware selection or BCD. No contemporaneous Windows update was
found. Causality for the boot-path change remains indeterminate.

The last healthy snapshot at `2026-10-20T16:53:01.3960437Z` recorded the same
instance ID, problem code zero, running service, five enabled interfaces and
the approved loaded image. The snapshot SHA-256 is
`E71CE7BF7299D9BC4C3998628A46CE46053990E615765CEC6E3ACB6146FEE4AD`.
After reboot, PCI location changed from 4:1.0 to 5:1.0 while stable PnP identity
did not. BDF is consequently mutable topology evidence and must not be used as
the primary device identity.

## Minimal recovery design (not executed)

Recovery requires a new authorization covering one BCD write and one reboot.
Before that authorization is used, revalidate the exact current loader,
Secure Boot/VBS/HVCI/WDAC state, candidate hashes, stable instance ID and the
available recovery route. Windows RE is currently disabled and only one disk
is visible, so this limitation must remain an explicit prerequisite rather
than being assumed away.

The minimal proposed elevated change is:

```powershell
bcdedit.exe /set '{current}' testsigning on
bcdedit.exe /enum '{current}'
```

The second command must show `testsigning Yes`. One separately authorized
reboot is then required. After boot, perform only passive verification of Code
52, service state, loaded image hash and the five interface publications before
any IOCTL batch. Do not substitute `nointegritychecks`, do not select an old
firmware entry whose storage is absent, and do not repair certificate stores.

To restore normal kernel signing enforcement after a suitable Microsoft-signed
driver is available:

```powershell
bcdedit.exe /set '{current}' testsigning off
bcdedit.exe /enum '{current}'
```

That also requires a reboot. Turning Test Mode off while this locally
test-signed package is selected is expected to reproduce the load rejection.
Test Mode weakens normal kernel trust by permitting test-signed kernel images
and displays a desktop watermark; it is appropriate only for this expendable
development environment.

## Stable runtime gate

`scripts/test-runtime-baseline.ps1` provides the next passive gate. It requires
exact hardware ID, exact full instance ID, INF, version, problem-free state,
running service, loaded SYS hash and five published interface classes. PCI
location is logged but never used to select a device. A location change fails
closed until `-AcknowledgeLocationChange` is supplied after reviewing the
stable identity. Mock regressions cover stable, moved, duplicate, mismatched
and unhealthy cases. The gate opens no device interface and issues no IOCTL.

No recovery command in this document has been executed.
