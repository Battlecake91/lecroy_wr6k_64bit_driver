# Batch 27 opcode 0x03 runtime preflight

## Batch 27 R0/R1 continuation result

The prior run passed R0-1 through R0-7. Its R0-8 attempt remains indeterminate:
the marker existed, but process-info, result and child logs were absent. The
later owner-authorized continuation did not replay R0-1 through R0-7. It
executed only R0-8, R0-9, R1 PCI and R1 START-register, in order, each exactly
once. All four completed with exit code 0, no timeout, and closed child process
trees. The runner outcome is PASS; the combined matrix is R0 9/9 and R1 2/2
across separate runs, not one uninterrupted run.

The installed package and post-run state match the approved identity:

- INF `oem108.inf`, version `0.2026.1030.2`
- stable instance `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`
- loaded SYS SHA-256 `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`
- device `OK`, problem code 0; `LecS65AcqDrv` running
- five expected interface classes, one enabled interface instance per class
- no XStream or LeCroy acquisition client present
- preflight and post-R1 state checks passed

Results and UTC timestamps:

| Test | Exact request / observation | Result JSON SHA-256 | Command log SHA-256 |
| --- | --- | --- | --- |
| R0-8 | `raw-ioctl 0x00223040 "" 4`; output `AE2C0000` = `0x2CAE` | `1E0A7049BFD0F49C612EF0AC283280B7F7D6D4AD99AB6EA8E328341AA38FA218` | `608F381DC6B57A6C7CA9D0E72B5AD251B5F7C3760BF31EFB59DFBD4DF32476CC` |
| R0-9 | `raw-ioctl 0x00222C00 00000000 0`; returned 0 bytes | `15C7A671C9D888F345E60F59F1BA5234BDBB09DB0D669CF3F44545D2A626827E` | `659D14F24FA0A14D2FAE37585F4CB246C51EF274EE12F56C03900C1AA4324B40` |
| R1-1 | PCI vendor/device `1570:0005`; observed BDF `5:1.0` | `629D45DD6AA7E322B39F64FA50CFA5766D219CD5F68B8AB0D94F582D8536863C` | `0C2626620E651CE38BC83E4597315781D0242CDCBBB40D72FFCB2C69CBF4B439` |
| R1-2 | Legacy and BAR0 START reads both `0x00000002` | `CF2B3FC2BAABF8AB7D0C52AEF6B41BC9AFF3165B376AE7ADBBC4021E9DFF06B5` | `41435F5AE955B8DA5E765F543CE89ED38604C91D287428077A4AF117B9F8804B` |

The continuation ran from `2026-11-05T21:45:55Z` through
`2026-11-05T21:49:08Z` using Windows PowerShell 5.1.19041.6456 and the pinned
`lecdiag.exe` SHA-256
`53F45F18D6887698AB2221EF69F4F0729A2BE5764FFFAECA3CC3ADB376795767`. Its
49-entry manifest SHA-256 is
`93EFE470BC39434FFD7806300EA2223A21EB39843BA187DAAADB574AB3E58E4A`; all
listed artifact hashes were rechecked with zero mismatches. The final outer
launcher exit code was 0. Dispatch marker SHA-256:
`2A82FEA40664A62E042745C92024D876654014F5E6E440514368B43FB28B7504`;
outer result SHA-256:
`9F3C5183C6B7BBA09477027A7676CADEEA6DCC03E221FA583F9FEE250C1FE097`.

No driver installation, signing, PnP transition, reboot, XStream launch,
acquisition, DMA GO, or other hardware command occurred in the continuation.
The four authorized read/test operations are consumed. Further XStream,
acquisition, or DMA testing requires separate authorization.

The offline recovery preceding this run bounded native process exit and
stdout/stderr drain waits, replaced PowerShell job-stream completion with an
exclusive atomically published worker-result sidecar, and verified prior
evidence before resumption. Source/ABI contracts passed 74/74; Batch3Remaining
offline runtime mocks 24/24; package compatibility 32/32 on Windows PowerShell
5.1 and 32/32 on PowerShell 7.6; diagnostic capture/install mocks 51/51 on
Windows PowerShell 5.1. The mock launcher suite used only its test-only
elevation override and did not run an active mode.

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

## Passive installed state before the authorized Batch 26 installation

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

## Lossless XStream capture preparation (completed; runtime remains gated)

The historical 130-byte opcode-`0x03` XStream request was not fully retained;
its missing bytes must not be reconstructed. The lossless external observer
and bounded capture path are prepared to record the complete request/response
during a future controlled XStream run. This Batch 27 continuation did not run
XStream. Any future XStream start requires its own fresh explicit authorization,
physical-disk-isolation confirmation, the installed candidate identity, and
the pinned `scripts\invoke-xstream-lossless-capture.ps1` path. It must preserve
`MamLaunchLimit=1`, make no explicit `Acquire()` or CVar changes, and have no
retry. A successful R0/R1 result alone does not authorize that test.

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

At the end of this initial run, R0-8 (`IOCTL 0x00223040`) had an attempt marker
but no result JSON or child output; dispatch and completion were indeterminate.
R0-9 and both R1 commands were therefore not run in that attempt. The later
separately authorized continuation and its passing results are recorded at the
top of this handoff. No MAM GO or XStream start occurred in either run.

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
