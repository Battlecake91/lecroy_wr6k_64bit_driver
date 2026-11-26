# Batch 31 package qualification

## Result

The Batch 30 status-propagation correction is frozen in one unsigned,
offline-validated Release x64 package. It has not been signed, staged,
installed, or used against hardware. The package workflow reported
`UnsignedValidationOnly`; its manifest explicitly records
`InstallAuthorized=false` and no Driver Store, PnP, device-open, or signing
policy changes.

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
private output roots remain preserved; no signing took place in any attempt.

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

Next, request separate explicit approval to use the private key to sign this
exact frozen candidate. Signing approval does not authorize installation. A
later installation needs its own approval and a fresh passive preflight of
the package hashes, test-signing, device/service/interface state, capture
readiness, acquisition-client absence, and verified rollback retention. After
installation, qualify R0 9/9 and R1 2/2 once. Any XStream startup requires
another fresh authorization and confirmation that the productive x86 disk is
physically isolated; preserve `MamLaunchLimit=1`, and treat the possible one
MAM GO as an explicitly authorized DMA experiment. Do not repeat Batch 28's
consumed authorization.
