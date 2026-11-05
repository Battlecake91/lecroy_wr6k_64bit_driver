# Batch 28 first XStream startup attempt

## Outcome

The owner reconfirmed physical x86-disk disconnection and authorized exactly
one `XStreamWithCapture` attempt. The attempt was dispatched once. The pinned
capture launcher started and resumed one XStream embedded local-server process;
the process emitted initialization IOCTLs before the waveform observer failed
its own-client guard. There was no retry, explicit `Acquire()`, CVar change,
MAM GO, driver reload, PnP transition or reboot. This authorization is
consumed; do not rerun under it.

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

The passive PnP interface check used metadata enumeration only and opened no
interface. The later approved XStream server process opened its normal handles;
the lossless private trace records those process events and IOCTLs. R0/R1 was
not repeated; its separate completed evidence remains in the Batch 27 handoff.

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

## Actual attempt and findings

The elevated 64-bit Windows PowerShell 5.1 launcher ran from
`2026-11-05T22:32:51.8496507Z` and passed passive identity validation. DebugView
reported a confirmed kernel message and `CAPTURE READY` at
`2026-11-05T22:32:55.3464617Z`. The trace launcher's readiness record names PID
528 and `Embedding=true`; its source writes readiness and then resumes the
created process. The server was therefore already running when the observer
performed its preflight.

The observer failed at `Assert-Preflight` with
`An acquisition client is already running.` Its result at
`2026-11-05T22:32:58.1417399Z` is `FAIL`, `XStreamLaunchCount=0`, and
`ExplicitAcquireCount=0`. The pinned lossless orchestrator then exited 1 at
`2026-11-05T22:33:28.6590608Z`; the outer launcher records one capture
invocation and one XStream runner invocation, with no retry. The observer never
reached `New-Object -ComObject 'LeCroy.XStreamDSO'`; COM activation and class
registration were not established; no DCOM event 10010 or other relevant
System/Application event was found in the attempt window.

This was not a no-IOCTL attempt. The private format-4 trace parses as 2,843
JSON records: one header, 1,401 `NtCreateFile` records, 913 Win32 IOCTL records
and 528 native IOCTL records (1,441 IOCTL API calls). It contains 478
`0xCFDC2110` calls; all captured `CFDC2110` native calls returned NTSTATUS
`STATUS_SUCCESS`. No family-2 opcode-`0x03` call with the required 130-byte
input and 14-byte output was present. No full request hash exists because that
request did not occur. The JSONL is an incomplete prefix: its `trace_footer`
record is absent, so total capture completeness and final target-process exit
status are unproven.

Eighteen native IOCTL records returned `STATUS_OBJECT_TYPE_MISMATCH`
(`0xC0000024`): sixteen on `0x80002000`, one on `0x80002004`, and one on
`0x80002008`. The captured handle data did not correlate these records to the
LeCroy device path; their source is therefore unassigned. No captured
`CFDC2124`, `CFDC2138` or `CFDC2128` was present. The kernel capture contains no
`PNP_STATUS:`, `CFDC2124`, `CFDC2138`, `MAMRGO` or DPC marker. There is no
evidence of MAM GO, DMA mapping, IRQ/DPC completion, completion status or
waveform payload.

The configured capture duration was 300 seconds, but failure cleanup ended
capture early; the final live-output write was at `2026-11-05T22:35:28Z`, about
154 seconds after DebugView started. The capture child exit code was not
persisted. After cleanup, DebugView, the `Dbgv` service and XStream processes
were absent. The post-check at `2026-11-05T22:46:21Z` found `oem108.inf`,
version `0.2026.1030.2`, the exact SYS hash, device `OK` / problem 0, the
service running and one published interface in each of the five classes. No
relevant PnP, Code Integrity, WHEA, service or DCOM events were found.

## Offline guard correction

The defect is in `scripts/invoke-xstream-single-waveform.ps1`: the observer
rejected every acquisition-client process before applying its existing
`ExpectedPrelaunchedProcessId` check. Preflight now allows only the one expected
`lecroyxstreamdso.exe` with the exact pinned executable path and PID, and still
rejects any additional client. Focused Windows PowerShell 5.1 tests pass
13/13, including wrong PID, wrong path and competing-client failures. The
installed driver and capture binaries were not changed by this correction.

## Evidence integrity and next gate

Raw evidence remains private at
`C:\Users\LeCroyUser\.codex\driver-backups\Batch28-XStreamWithCapture-20261105-attempt1`.
Its 26-file SHA-256 inventory is
`batch28-evidence-hashes.json`, SHA-256
`3077215D7AF51829EFFCAA069EFB741FB337AA43EEC4055F956D8FEF95CF650D`.
Key hashes:

- private user-mode trace:
  `FEED400895B4452FB4151B9E55AD6668C1C5E709D6C3196E07C2F604EB38827E`
- raw kernel capture:
  `1F92714B7E3E36CF4474A1417F036D48B3FCF09E3C806A032FFDCF2C98D501F3`
- outer result JSON:
  `F15B5CE9A4C04E4509427A731F7EAF3C4AF930422DEBCEB94DBEA29913F1972E`
- lossless orchestrator result:
  `39CCC2F32E827C478BB154A95D92EC18E3DA7C6E2382F7E230A31991F4891F6C`
- waveform-observer result:
  `8C7D1ABFAB30631DD37A16A089B1DCA1AB4F376FB328691C9403CFC1337EF5F3`

The XStream startup authorization is consumed regardless of the preflight
failure. Any further XStream attempt requires fresh explicit authorization and
fresh physical-isolation confirmation. Keep `MamLaunchLimit=1`; do not reset
the kernel gate. Review this corrected runner and incomplete first trace before
proposing another single captured startup.
