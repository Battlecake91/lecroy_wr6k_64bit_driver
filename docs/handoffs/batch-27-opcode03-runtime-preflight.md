# Batch 27 opcode 0x03 runtime preflight

## Correct execution environment

The incorrect `codex/legacy-dma-cleanup-hardening` checkout was not modified.
The existing clean worktree at
`C:\Users\LeCroyUser\.codex\worktrees\phase2c-runtime-batch12\lecroy_wr6k_64bit_driver`
contained the required Batch 26 source at
`f5782a61d42d317b82f6cf834921237a409aa6fc`. The Batch 26 handoff is present
at that commit. Batch 27 preparation continues on
`codex/phase2c-runtime-batch27` from that exact parent.

The active host is the local console session of `DESKTOP-0Q0RKAQ`, running
Windows 10 Pro x64 build 19045 as
`DESKTOP-0Q0RKAQ\LeCroyUser`. The reported Gigabyte system model and
`HypervisorPresent=false` show no remote-session or virtualization indicator.
The console-session observation is direct; absence of every possible
virtualization layer is not independently proven.

This is consistent with the Batch 25 hardware environment: the stable PCI
instance
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`, installed
package, loaded binary hash, operating-system build and five interface classes
all match the retained Batch 25 evidence.

## Passive installed state

- PCI device: present and started, `VEN_1570:DEV_0005`, problem code zero
- bound package: `oem107.inf`, version `0.2026.1030.1`
- loaded SYS SHA-256:
  `184471C04BCE4688D3EE6EAC01049606F7878D9A0568419F0A08516282F35CAE`
- service: `LecS65AcqDrv`, running
- enabled interface classes:
  - `{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}`
  - `{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}`
  - `{7AC34BE9-F766-4F15-9E88-854BA5E2146E}`
  - `{958695A4-693A-435E-8297-66F805D8E46A}`
  - `{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}`
- Driver Verifier flags: zero; no verified drivers
- XStream/LeCroy acquisition processes: none

The current non-elevated token could not read the BCD store, so the
`testsigning` flag was not independently refreshed during this passive pass.
The expected locally test-signed driver remains loaded and healthy.

The pinned DebugView 5.02 x64 collector is present, Authenticode-valid and has
SHA-256
`7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B`.
No DebugView process or temporary `Dbgv` service is active. Batch 25 proved the
live-stdout readiness mechanism on this host; a future active attempt must
establish a fresh readiness record before any installation or request dispatch.

## Frozen candidate

The Batch 26 candidate remains unchanged at
`C:\Users\LeCroyUser\.codex\driver-backups\Batch26-SignedCandidate-20261030-1030-2`.

- DriverVer: `10/30/2026,0.2026.1030.2`
- INF SHA-256: `BC7734C13F276A23ACD18B3CA004267B508F5DE9AD734F426E947AA051976CF4`
- CAT SHA-256: `28BFF32CC2579CCC920991E4C36CA2007D04F9F9F1D255E1EEB4350924DA5617`
- signed SYS SHA-256: `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`
- PDB SHA-256: `78D8D941C159C639787D5513B4AB727560EB173FB285CB6C75F119020552710B`
- manifest SHA-256: `F615EA1F0F31C8227D7592DECB2D106F4EBF874BA7104294238AA0827DD5D116`
- SYS and CAT Authenticode: valid
- signer thumbprint: `4296FA2E355613DF0D5CE6F24D0898E00B706472`
- source/workflow commit: `414f18e6c4f8072b05e7d60d8ef2a5c1218ada42`
- configuration: Release x64, PnP diagnostics, WDM-v3 MAM,
  `MamLaunchLimit=1`, unsafe legacy PFN DMA disabled

The package manifest still records `InstallAuthorized=false`. No package file
was rebuilt, re-signed, staged or installed during this preflight.

## Exact-request evidence gap

The private Batch 25 kernel capture contains exactly six `CFDC2110` calls with
130 input bytes and 14 output bytes. Their logged 64-byte previews are
identical; the preview SHA-256 is
`9A18FDEC752F591CB528C62453A9D9B475ED6BC5B51D7621B8934FD3731CA0E2`.
Each trace record then explicitly reports `66 more bytes`. The raw log and its
live-stdout copy are the only private files containing that preview. No retained
evidence contains the missing 66 bytes, so no full 130-byte request or request
SHA-256 can be recovered from the existing capture.

This is an evidence-capture defect, not permission to reconstruct the opaque
body. A standalone opcode qualification cannot be prepared faithfully from the
retained evidence. The public repository contains no request bytes.

## Lossless external capture qualification

The existing x86 user-mode IOCTL observer can capture the missing evidence
without changing the signed driver. It now starts the pinned XStream local
server suspended, injects the hook before the first application IOCTL, creates
an exclusive PID-bound readiness marker, and then resumes `/Embedding`. The
hook's format-4 JSONL records preserve the complete request and synchronous
response, UTC/QPC ordering, thread and handle correlation, IOCTL identity,
family/opcode metadata, status/last-error data, returned byte count and a
normal-completion footer. Nested `DeviceIoControl`/`NtDeviceIoControlFile`
observation is de-duplicated without suppressing direct NT calls.

`scripts/invoke-xstream-lossless-capture.ps1` pins every executable/script by
SHA-256, starts exactly one injected target, waits for readiness, and passes
that exact PID to the bounded COM observer. The observer rejects every other
pre-existing or additionally spawned XStream process. The post-capture
validator requires complete 130-byte family-2/opcode-`0x03` input and 14-byte
output framing, computes request/response SHA-256 values and writes metadata
only. Opaque bytes remain exclusively in the private evidence root.

Windows PowerShell 5.1 focused results are:

- lossless tracer/orchestration: 18/18 PASS;
- bounded single-waveform runner: 9/9 PASS;
- diagnostic capture/install launcher: 44/44 PASS.

The final local capture identities are launcher
`77B2910B54AC7B2E9CA5A53CB66F5B862AB3B60024D97B9AA78FC875923AE2A6`,
hook `F602235A7F676113B3A8233EDCA4796AB39E089F552C60EFE19F3821D03188BC`,
validator `330FFFEDDAF1CF1D8A9D55332FCB52C96ED304014481F9937A9A72BE07A0AF78`,
orchestrator `9C0C32A8258FE2B9B9AB26C3CDE5A2CB27CCBE7F08C83B2AD2715260D118EA28`
and bounded observer
`A5611EAD6F9FACA3EB8460E6ADACA56D2D06DDEFDF4124AE2A46AD50DCB7F1FB`.
The generated x86 binaries remain local and ignored by Git; rebuilding them
changes their identity and therefore requires updating the pinned live command.

The synthetic target covers NUL-containing binary input, six identical target
requests, two threads, two handles, error responses, successful output,
pre-dispatch path collisions, unavailable trace output, `/Embedding`, truncated
input rejection, repository-containment rejection and complete orchestration.
Tracing did not change the target's observed IOCTL inputs, outputs or errors.
The x86 synthetic target builds with `/W4 /WX`. The hook build retains nine
MSVC C4152 warnings at the established IAT function/data-pointer conversion
sites; no new warning class or build error was introduced.

The frozen Batch 26 package hashes were reverified and remain exactly those
listed above. It was not rebuilt, re-signed, staged or installed.
The final passive snapshot still reports `oem107.inf`, version
`0.2026.1030.1`, device `OK`, problem code zero, service `Running`, loaded SYS
`184471C04BCE4688D3EE6EAC01049606F7878D9A0568419F0A08516282F35CAE`
and no XStream process.

## Authorization boundary and next actions

No installation, PnP transition, device handle, IOCTL, XStream launch or DMA
operation occurred in this preparation.

The following remain separate owner-authorized actions:

1. one elevated installation of the frozen INF with
   `pnputil.exe /add-driver <absolute Batch 26 INF path> /install`, under a
   fresh qualified kernel capture;
2. the established R0/R1 matrix on the installed candidate;
3. one bounded XStream startup through the qualified lossless observer, after a
   fresh physical-disk-isolation confirmation, with no explicit `Acquire()`, no
   CVar change, no retry and the existing kernel `MamLaunchLimit=1`;
4. private validation of every captured opcode-`0x03` request and response,
   followed by COM, DMA and waveform outcome classification.

The repository, hardware baseline, frozen candidate and lossless capture path
are ready for a separately authorized installation. Installation itself is not
authorized by this work. After installation and R0/R1, the single XStream
startup remains a second, independent authorization boundary. Standalone replay
is no longer a prerequisite because the missing historical bytes cannot be
recovered and the controlled application run can capture them faithfully.

The separately authorized installation command is:

```powershell
$pnp = Join-Path $env:SystemRoot 'System32\pnputil.exe'
& $pnp /add-driver `
  'C:\Users\LeCroyUser\.codex\driver-backups\Batch26-SignedCandidate-20261030-1030-2\package\LecS65AcqDrv.inf' `
  /install
```

It must run only through the existing captured, exact-once installation mode
after rechecking all five package hashes, elevation, test-signing state,
rollback retention, device identity and XStream absence. The subsequent active
XStream command is intentionally not frozen until installation has established
the actual published INF and loaded SYS identity. At that later gate,
`XStreamRunnerPath` for `XStreamWithCapture` must be
`scripts\invoke-xstream-lossless-capture.ps1`; the diagnostic launcher now
requires and pins the trace launcher, hook, validator and inner waveform runner
before starting kernel capture. A real call lacking any of those identities
fails before XStream dispatch. The next approval request must present the fully
materialized command, fresh evidence root, actual installed INF/SYS values and
fresh physical-disk-isolation confirmation.

## Authorized installation and interrupted R0/R1 run (2026-11-04)

The owner authorized one installation of the frozen candidate and one complete
R0/R1 run, while explicitly excluding XStream and further MAM GO. The package
was rechecked against the approved identity before dispatch:

| Artifact | SHA-256 |
| --- | --- |
| INF | `BC7734C13F276A23ACD18B3CA004267B508F5DE9AD734F426E947AA051976CF4` |
| CAT | `28BFF32CC2579CCC920991E4C36CA2007D04F9F9F1D255E1EEB4350924DA5617` |
| signed SYS | `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653` |
| PDB | `78D8D941C159C639787D5513B4AB727560EB173FB285CB6C75F119020552710B` |
| manifest | `F615EA1F0F31C8227D7592DECB2D106F4EBF874BA7104294238AA0827DD5D116` |

The elevated Windows PowerShell 5.1 preflight passed: test signing was enabled,
Secure Boot and Driver Verifier were inactive, the stable PCI instance was
present, the old device was healthy, and XStream/acquisition clients were not
running. The pinned certificate and local signatures were valid. No candidate
file was rebuilt or changed.

One captured installation ran from
`C:\Users\LeCroyUser\.codex\driver-backups\Batch27-InstallWithCapture-20261104-attempt1`.
PnPUtil exited 0, published `oem108.inf`, and reported installation on the
approved PCI instance. Passive verification found version `0.2026.1030.2`,
loaded SYS hash equal to the approved hash, device `OK`/problem code 0, and
`LecS65AcqDrv` running. Metadata enumeration found one enabled instance for
each of the five expected interface classes. The 300-second DebugView capture
ended at its configured duration; DebugView and the `Dbgv` service were absent
afterward.

The installation child succeeded, but the outer launcher did not complete its
result write: `result.json` is zero bytes and launcher stdout ends after
`INSTALL CHILD PASS`. With no child process remaining, the hung PowerShell
parent was stopped. Therefore the installation outcome is established by the
captured PnPUtil child output and passive device metadata, while the wrapper's
final exit/result remains incomplete. The raw capture and logs remain outside
the public repository at the path above. Kernel-capture SHA-256 is
`23C7F975D4B44A8BC3FCC90498E72BAE2E37D46B5917ACC02A36F9088CEF4743`;
install-child log SHA-256 is
`0C70977A230A72E5EAD262CBD0553FE9FF1A0C5EC10B7C6CFDCBB47C99A28179`.

The single R0/R1 runner invocation used evidence root
`C:\Users\LeCroyUser\.codex\driver-backups\Batch27-R0R1-20261104-attempt1`.
R0-1 through R0-7 completed exactly once with exit code 0:

| Command | Result | SHA-256 of result JSON |
| --- | --- | --- |
| `lecdiag build` | PASS, build 1002 | `19B8A9D3BF38FF6DC2A9A53E5B70D87F714D1B61B7C25D6E458F103047C44157` |
| `lecdiag stats` | PASS, zero unknown IOCTLs | `A67062E4247D8115B85299786E7B3B1D6B006EDD362454A344710841E0A38CDE` |
| `lecdiag bars` | PASS, three documented BARs | `A8CD433927034BD29E5F5CB8992EC913BA62AF4139EDF111C6B8423AB488EBAC` |
| `lecdiag trace` | PASS, one prior trace record | `2DD73C9E7D3319BA3FE626CE9507444D2B8B4413CBBB50BE80CCAEDDD0F5A51A` |
| R0-5 zero-length no-op | PASS | `9BC866F63EBD36E64F1892F9DAA365AA042A943EBCB5A42913E2C41B47A1DD03` |
| R0-6 zero-length no-op | PASS | `70D9F4D291461565046E67BA73073371BCD0AAE9A5F2120E9945810908A56953` |
| R0-7 trace-size query | PASS, returned `0x110` | `E5F053E15164A978BAA1278B0319F75D083B26CCF2421FE7DF4B85492C3A67A2` |

R0-8 (`IOCTL 0x00223040`, four-byte output) has an exclusive durable attempt
marker but no result JSON and empty stdout/stderr files. Its actual dispatch
and completion cannot be established. Do not repeat it. R0-9 and both R1
commands were not run because the serial matrix did not produce a verified
R0-8 completion. The resulting matrix is incomplete, not a pass. A subsequent
active continuation needs a fresh review of the interrupted runner state and
explicit bounded authorization; XStream remains separately gated. No MAM GO,
XStream start, driver replacement, reboot, rollback or additional PnP operation
occurred.

Latest passive snapshot: stable instance
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`, `oem108.inf`
version `0.2026.1030.2`, expected loaded SYS hash, device `OK`, problem code 0,
service running and all five expected interfaces enabled. XStream, lecdiag,
DebugView and `Dbgv` were not running. Do not infer physical x86-disk status
from this snapshot; the prior separate owner confirmation remains necessary
for any future XStream attempt. A read-only System log query from the install
window onward found no matching LeCroy/PnP, Code Integrity, WHEA or service
events by device/service/package identifiers; this does not claim that the
entire System log contained no unrelated events.
