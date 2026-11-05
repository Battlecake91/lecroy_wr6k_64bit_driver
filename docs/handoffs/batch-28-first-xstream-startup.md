# Batch 28 first XStream startup preflight

## Outcome

Batch 28 stopped after passive preflight. No XStream process was started, no
capture driver was loaded, no device handle was opened, and no hardware request
or MAM GO was dispatched. The current preflight does not authorize those
operations.

## Repository identity

- Starting revision: `d45517cdb07ef28a8d040a283cf80cfca871b564`
- Branch: `codex/phase2c-runtime-batch28`
- Installed package: `oem108.inf`, version `0.2026.1030.2`
- Stable PCI instance:
  `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`
- Loaded SYS SHA-256:
  `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`
- Package source/workflow identity recorded in the manifest:
  `414f18e6c4f8072b05e7d60d8ef2a5c1218ada42`

## Passive host and device snapshot

Observed at `2026-11-05T22:07:54Z` on `DESKTOP-0Q0RKAQ`, local session 1,
as `DESKTOP-0Q0RKAQ\LeCroyUser`. The host reports Windows 10 Pro x64,
build 19045. The current shell is 64-bit PowerShell 7.6.9 and is not elevated;
the production execution path requires an already elevated 64-bit Windows
PowerShell 5.1 process.

| Check | Observation |
| --- | --- |
| PCI device | Stable instance present; `LeCroy Acquisition Device (S65)`, status `OK`, problem `CM_PROB_NONE` |
| Bound package | `oem108.inf`, version `0.2026.1030.2` |
| Loaded SYS | SHA-256 equals the expected candidate hash above |
| Service | `LecS65AcqDrv`, running, status `OK` |
| Interfaces | One PnPUtil-reported interface path for each of the five expected GUIDs; all tied to the target device instance |
| Driver Verifier | No verified drivers; flags `0x00000000` |
| Acquisition processes | No XStream, LeCroy, WaveStudio, ScopeExplorer, `lecdiag` or `lecwatch` process found |
| Capture state | No DebugView process and no `Dbgv` system-driver service found |
| Batch 28 attempt evidence | No Batch 28 evidence directory was present under the private driver-backup root |

The PnP interface check used metadata enumeration only. It did not open any
interface or device handle. R0/R1 was not repeated; its separate completed
evidence remains in the Batch 27 handoff.

## Frozen candidate and capture identities

The local frozen package is under
`C:\Users\LeCroyUser\.codex\driver-backups\Batch26-SignedCandidate-20261030-1030-2`.
The package INF, CAT and SYS hashes match the established frozen hashes; both
SYS and CAT Authenticode checks report `Valid` with signer thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`. The package manifest hash is
`F615EA1F0F31C8227D7592DECB2D106F4EBF874BA7104294238AA0827DD5D116` and
records `WdmV3MamDma=true`, `FirstMamExperimentOnly=false` and
`MamLaunchLimit=1`. It records the Release PDB identity
`78D8D941C159C639787D5513B4AB727560EB173FB285CB6C75F119020552710B`.

| Component | SHA-256 | Result |
| --- | --- | --- |
| Frozen INF | `BC7734C13F276A23ACD18B3CA004267B508F5DE9AD734F426E947AA051976CF4` | Match |
| Frozen CAT | `28BFF32CC2579CCC920991E4C36CA2007D04F9F9F1D255E1EEB4350924DA5617` | Match |
| Frozen signed SYS | `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653` | Match |
| Release PDB, as recorded in manifest | `78D8D941C159C639787D5513B4AB727560EB173FB285CB6C75F119020552710B` | Manifest identity match |
| Candidate manifest | `F615EA1F0F31C8227D7592DECB2D106F4EBF874BA7104294238AA0827DD5D116` | Match |
| Microsoft DebugView CLI 5.02 x64 | `7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B` | Version 5.02; Authenticode `Valid` |
| Lossless trace launcher | `77B2910B54AC7B2E9CA5A53CB66F5B862AB3B60024D97B9AA78FC875923AE2A6` | Match |
| Lossless trace hook | `F602235A7F676113B3A8233EDCA4796AB39E089F552C60EFE19F3821D03188BC` | Match |
| Trace validator | `330FFFEDDAF1CF1D8A9D55332FCB52C96ED304014481F9937A9A72BE07A0AF78` | Match |
| Lossless orchestrator | `9C0C32A8258FE2B9B9AB26C3CDE5A2CB27CCBE7F08C83B2AD2715260D118EA28` | Match |
| Waveform observer | `A5611EAD6F9FACA3EB8460E6ADACA56D2D06DDEFDF4124AE2A46AD50DCB7F1FB` | Match |
| XStream DSO 8.5.1.1 local server | `27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F` | Match; Authenticode `Valid` |

The PDB hash above is the Release identity recorded in the frozen manifest; the
PDB is not a member of the three-file runtime package directory. The lossless
capture binaries are the existing locally generated artifacts and were checked
without rebuilding them.

## Authorization and next step

Two live prerequisites are missing from this Batch 28 request:

1. A fresh owner confirmation that the productive Windows x86 disk remains
   physically disconnected. Prior-batch confirmation is not treated as current.
2. Separate explicit authorization for exactly one XStream startup, its
   automatically initiated common-transport requests, at most one MAM GO under
   the installed `MamLaunchLimit=1`, and complete private IOCTL capture. The
   authorization must identify `oem108.inf`, version `0.2026.1030.2`, and the
   exact loaded SYS hash above.

Once both are provided, the next action is to repeat the passive identity check
in an elevated 64-bit Windows PowerShell 5.1 session, reserve a new private
evidence directory and exclusive attempt marker, start the pinned capture,
require a genuine readiness record, and invoke the existing
`XStreamWithCapture` path once. Do not call `Acquire()`, change CVars, retry,
reload the driver, or exceed the kernel launch limit. If a post-GO result is
uncertain, preserve the existing quarantine behavior and stop all further
hardware operations.
