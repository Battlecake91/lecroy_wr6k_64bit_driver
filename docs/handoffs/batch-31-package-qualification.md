# Batch 31 package qualification

## Result

The Batch 30 status-propagation correction is frozen in an offline-validated
unsigned Release x64 source package and a separately promoted, locally
test-signed package. The original unsigned artifact set remains unchanged.
The signed candidate has not been staged, installed, or used against hardware;
its manifest records `InstallAuthorized=false`. No Driver Store, PnP,
device-open, or signing-policy change occurred.

## Source and package identity

| Item | Identity |
| --- | --- |
| Branch | `codex/phase2c-runtime-batch31` |
| Package workflow/source commit | `50530b8d88cfee36ec23b8648611a4bafd6dfc4e` |
| Driver tree | `5225aaed874c2f5b461280b63f234d83563cd2e3` |
| Include tree | `eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add` |
| Configuration | Release x64; SDK/WDK `10.0.28000.0`; MSVC `14.41.34120` |
| DriverVer | `11/26/2026,0.2026.1126.1` |
| Package state | Unsigned; INF and CAT both `NotSigned` |
| Manifest SHA-256 | `E33B0E50C525B78F0E7FFF50A74FA00318E19C5D398988B7C126F4D78B50F297` |

The manifest is at
`C:\Users\LeCroyUser\.codex\driver-backups\Batch31-UnsignedCandidate-20261126-1126-3\evidence\manifest.json`.
The frozen output root is
`C:\Users\LeCroyUser\.codex\driver-backups\Batch31-UnsignedCandidate-20261126-1126-3`.
It contains only INF/CAT/SYS in `package`; the matching PDB and manifest are in
`evidence`. Do not rebuild or mutate this artifact set.

| Artifact | SHA-256 |
| --- | --- |
| INF | `AF7A16A700B5D90B9EA5DFDB0B243464804DF63F785423CE2D9F39E2969449EF` |
| CAT | `F1A12260E6032EBCA5C563B4ECDAFF2A25FD8A368FEF857853F403AAB8613FDA` |
| Release SYS | `AA41BCA0C3DC10AF6B6501AF4DB0EEA0CDE43C507068980FAB6DBE4F1A250D11` |
| Release PDB | `020EC0333DB143AE76EA71B28CBF66DBFA1F86816724D7EA8A4A81C97F4F2484` |
| Debug SYS | `2E5BCAC9DA7E1EDE84D9825B96C062FE53B8B0FD3ABAF843126BB24C7363F96A` |
| Debug PDB | `6B7CC3A71A5C088AA6562D34274C231AD9408F68BE983F6741FEE77D05FCF362` |
| `lecdiag.exe` | `374FC640A1DD8CFDC73FFE854DA988AA5ABFAB854D1F096EF5AD2F6747CA2CF0` |
| `lecwatch.exe` | `AEE7AE68F4AA0B793C263E27EC1058809E1DFEA5A46B27321E74DFF460FCE027` |

## One-time signed promotion

The owner authorized one signing promotion of the frozen input above. The
versioned `scripts/sign-frozen-driver-package.ps1` ran once from workflow HEAD
`2e0e9a781e6df68fa83795889cf6bbcf85ef5472`, under elevated 64-bit Windows
PowerShell 5.1. It did not build or modify source. It copied INF/SYS/PDB to a
new output root, signed the copied SYS, generated a matching catalog, signed
that catalog, verified the package, and emitted a signed manifest. The process
returned exit code 0; no retry was made.

Signed output root:
`C:\Users\LeCroyUser\.codex\driver-backups\Batch31-SignedCandidate-20261126-01`.
The attempt marker, elevated transcript, result JSON, exception path, and
per-tool logs are preserved outside the repository under
`C:\Users\LeCroyUser\.codex\driver-backups\Batch31-SigningAttempt-20261126-01`.

| Signed artifact | SHA-256 |
| --- | --- |
| INF | `AF7A16A700B5D90B9EA5DFDB0B243464804DF63F785423CE2D9F39E2969449EF` |
| CAT | `BEE0B662B55AEB0931806F9E0A9056330FCE64E02B182FC6014423601112D789` |
| Signed SYS | `6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75` |
| Matching PDB | `020EC0333DB143AE76EA71B28CBF66DBFA1F86816724D7EA8A4A81C97F4F2484` |
| Signed manifest | `3A7EB449FD7394EAF7E28C04226131EB7FF65EE561A1EDC2DD171078FECC8647` |

The resulting manifest records `PackageState=SignedCandidate`,
`RebuildPerformed=false`, the original unsigned manifest hash, and unchanged
driver/include trees. Both SYS and CAT Authenticode statuses are `Valid` and
the signer thumbprint is `4296FA2E355613DF0D5CE6F24D0898E00B706472`
(`CN=LecS65 x64 Test Phase2C 2026-10`). The local certificate chain built
successfully with no chain-status errors; its code-signing EKU is
`1.3.6.1.5.5.7.3.3`. InfVerif passed, SignTool catalog membership passed, and
SymChk matched the SYS to the PDB RSDS GUID
`3FAECB31-B528-4C6F-A6B3-F71218A917F4`, age 1, with line information. The
explicit `/kp` check failed as expected for this local test certificate; this
is not Microsoft kernel-policy or WHQL/WHCP signing. `manifest.sha256` matches
the signed manifest hash above.

The unsigned source package was rehashed after promotion and all five hashes
remain identical to the table above. No build, install, staging, PnP, reboot,
XStream, DMA, device-handle, or IOCTL operation occurred.

The selected candidate configuration is Release x64, PnP status diagnostics
enabled, WDM-v3 MAM enabled, `MamLaunchLimit=1`,
`FirstMamExperimentOnly=false`, unsafe legacy PFN DMA disabled, and MTT launch
disabled. The first-experiment-only build marker is correctly absent because
that option is false. The Release SYS contains `PNP_STATUS:`,
`LECS65_WDM_V3_MAM_DMA_ENABLED`, and `LECS65_MAM_LAUNCH_LIMIT=1`. PE inspection
reports AMD64 (`0x8664`), Native subsystem, `DriverEntry`, imports only
`hal.dll` and `ntoskrnl.exe`, and RSDS GUID
`3FAECB31-B528-4C6F-A6B3-F71218A917F4`, age 1. The package workflow matched
the PDB identity and source records. `InfVerif` passed and `Inf2Cat` reported no
errors or warnings. DriverVer is later/newer than installed
`0.2026.1030.2`; Windows selection still depends on full package ranking and
matching rules, not just this version string.

## Build and regression evidence

The successful package workflow was executed under Windows PowerShell 5.1.
Debug and Release x64 builds completed; the package build was a clean Release
rebuild. The complete Dry suite passed with 852/852 functional checks.
Default DMA/INTEN stress passed 5,000/5,000 and 1,000/1,000; expanded stress
passed 50,000/50,000 and 10,000/10,000. All 24 manifest-listed suites passed,
including production SG/PnP stage 165/165, DMA transaction 176/176, transfer
74/74, descriptors 13/13, launch/pre-GO checks, PnP/lifetime checks, and
source/ABI contracts 76/76.

The first package workflow run used PowerShell 7.6.9 and failed in a nested
PowerShell 5.1 test because `Get-FileHash` was unavailable in the child test
environment. The second run correctly used Windows PowerShell 5.1 but exposed
expected native stderr becoming terminating under the caller's
`$ErrorActionPreference='Stop'` during timeout-negative tests. The harness was
corrected in commit `50530b8d88cfee36ec23b8648611a4bafd6dfc4e`: the test sets
`$ErrorActionPreference='Continue'` for expected native stderr while asserting
native exit/result status; a source contract pins this behavior. Source/ABI
contracts pass 76/76, and the focused XStream timeout suite passes 28/28
directly and under strict parent output capture. The first two incomplete
private output roots remain preserved; those failed package attempts did not
sign. The subsequent single signing promotion is recorded below.

## Passive host snapshot

On 2026-11-26, read-only checks observed Windows 10 Pro x64, build 19045; the
stable PCI instance
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0` was `OK` with
problem code zero and bound to `oem108.inf`, version `0.2026.1030.2`. Service
`LecS65AcqDrv` was running. The on-disk loaded SYS hash was
`2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`. No
XStream/acquisition-named process was present. No device handle was opened.

The five interface instances were not re-enumerated during Batch 31. The
test-signing state could not be freshly read: `bcdedit /enum {current}` exited
1 in this shell. Capture readiness and the rollback set were not revalidated
in this package pass. These are explicit preflight checks for any later
installation; no claim is made that they passed here.

## Status correction and next gates

The fixed source preserves the operation status (`STATUS_DEVICE_BUSY` on
exhausted launch limit) when no-launch cleanup succeeds. Cleanup failure
remains authoritative and follows existing fail-closed handling. Invalid SG
segment counts fail explicitly. The denial remains pre-arm and does not permit
DMA arm, INTEN enable, or MAMRGO; no completion count is fabricated. The
installed `oem108.inf` still contains the old status-overwrite behavior.

Signing is complete. The next step requires separate explicit installation
approval and a fresh passive preflight of
the package hashes, test-signing, device/service/interface state, capture
readiness, acquisition-client absence, and verified rollback retention. After
installation, qualify R0 9/9 and R1 2/2 once. Any XStream startup requires
another fresh authorization and confirmation that the productive x86 disk is
physically isolated; preserve `MamLaunchLimit=1`, and treat the possible one
MAM GO as an explicitly authorized DMA experiment. Do not repeat Batch 28's
consumed authorization.
