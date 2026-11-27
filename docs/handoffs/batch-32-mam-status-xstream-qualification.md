# Batch 32 — Corrected MAM Status During XStream Startup

## Scope and result

Batch 32 completed offline preparation, elevated passive preflight and exactly
one freshly authorized `XStreamWithCapture` attempt. The attempt failed while
resolving the COM `Acquisition` object with RPC error `0x800706BA`. No explicit
`Acquire()` or waveform was observed. The private IOCTL trace is a valid
partial trace terminated at the configured timeout and has no footer. It
contains no family-2 opcode `0x03` request and no direct `CFDC2138` device
control. The kernel capture contains no `CFDC2138` or `MAMRGO` marker; no MAM GO
was observed. Treat the inaccessible FDO-local launch latch as uncertain and
do not retry. The capture and XStream processes ended; the installed driver
and device remain healthy.

The source branch was created from the required Batch 31 HEAD
`2282727655258d0661bd61324c1dafd17d0b7eb2` as
`codex/phase2c-runtime-batch32`.

## Passive installed baseline

Observed on `DESKTOP-0Q0RKAQ`, Windows 10 Pro x64, build 19045, under
`DESKTOP-0Q0RKAQ\LeCroyUser`:

| Check | Observation |
|---|---|
| PCI device | `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`, present and `OK` |
| Problem code | `0` |
| Bound package | `oem109.inf`, version `0.2026.1126.1` |
| Service | `LecS65AcqDrv`, running; image `C:\windows\system32\drivers\LecS65AcqDrv.sys` |
| Loaded image file hash | `6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75` |
| Device interfaces | Five enabled instances on the stable PCI instance: `{fc5df040-d6cd-4ba0-b5e0-2561972963a2}`, `{8d1103b8-5bf4-4b5c-b21e-eeaace97d418}`, `{7ac34be9-f766-4f15-9e88-854ba5e2146e}`, `{958695a4-693a-435e-8297-66f805d8e46a}`, `{9007c2bc-edfd-4f2f-a059-df1131cb1ae5}` |
| XStream/acquisition/diagnostic processes | None matched the inspected process-name set |
| DebugView process/service | No process or `Dbgv` system-driver service found |
| Registered XStream COM server | 32-bit COM server path resolves to `C:\Program Files (x86)\LeCroy\XStream\lecroyxstreamdso.exe`; version `0,23,95,31`, SHA-256 `27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F` |

The frozen Batch 31 directory was checked read-only. INF, CAT and SYS hashes
match `AF7A16A700B5D90B9EA5DFDB0B243464804DF63F785423CE2D9F39E2969449EF`,
`BEE0B662B55AEB0931806F9E0A9056330FCE64E02B182FC6014423601112D789`, and
`6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75`.
The retained evidence PDB and manifest match
`020EC0333DB143AE76EA71B28CBF66DBFA1F86816724D7EA8A4A81C97F4F2484` and
`3A7EB449FD7394EAF7E28C04226131EB7FF65EE561A1EDC2DD171078FECC8647`.
The PDB and manifest are in the package's `evidence` directory, not its
three-file installable `package` directory. Manifest metadata records
`MamLaunchLimit=1`, WDM-v3 MAM enabled, legacy PFN DMA disabled and MTT launch
disabled. This metadata does not establish that the current FDO launch latch
is unused.

An elevated 64-bit Windows PowerShell 5.1 passive check ran at
`2026-11-27T00:34:25.9458495Z`. BCD `{current}` reported `testsigning Yes`;
`verifier /querysettings` reported flags `0x00000000` and no selected drivers;
`verifier /query` reported no drivers currently verified. The PCI instance,
`oem109.inf`, version, running service and loaded SYS hash matched the pinned
baseline. No XStream/acquisition client was running before dispatch. The owner
freshly confirmed that the productive Windows x86 disk remained physically
disconnected; the passive disk inventory exposed only the test NVMe. The
preflight JSON SHA-256 is
`CA4C7BF228763D93D2DF00BE0F7ED9D18D89D4850E47292026E093005472BDDF`; the
transcript SHA-256 is
`07BC8B962EB42D0C72E8D5A85BA430912D3F7D10854C4ACFB2A0C762885A62EE`.

## Production observer rebuild and qualification

Focused offline tests passed under Windows PowerShell 5.1:

| Suite | Result |
|---|---:|
| Diagnostic capture/install launcher mock suite | 51/51 |
| XStream COM lifecycle | 8/8 |
| XStream IOCTL trace | 29/29 |
| Single-waveform observer | 13/13 |

The final combined Windows PowerShell 5.1 run passed all four suites with exit
code 0: launcher 51/51, IOCTL trace 29/29, COM lifecycle 8/8, and waveform
observer 13/13. Launcher result JSON records host
`Windows PowerShell 5.1.19041.6456`, 51 passed, 0 failed. Combined private
evidence is under
`%TEMP%\Batch32-PS51-Final-f361aefb64764f47be064b12262c97ca`.

The suite rerun used 64-bit Windows PowerShell 5.1. Aggregate results and the
launcher suite's private logs are retained outside the repository under
`%TEMP%\Batch32-PS51-Qualification-879e6adb1a4c49b580d227064b475361`.

The launcher fixture was corrected to copy its direct `xstream-com-lifecycle.ps1`
dependency into its isolated temporary repository. The previous test-only
failure was a missing fixture file, not a production observer failure. The
trace test generated synthetic traces and test-only x86 binaries; these are not
production assets and must not be used for a live capture.

The historic production launcher/hook files were not found in the scoped
Batch 27–29, `.codex\tools`, or repository build directories. They were
rebuilt from the tracked production sources using the repository build script
under Windows PowerShell 5.1, MSVC x86 toolchain 14.41.34120 (compiler file
version 19.41.34123.0). Source commit was
`efb1ad4fbf1100c8ff2648bedba6a4b0717f5998`. Source identities:

| File | SHA-256 |
|---|---|
| `tools/xstream-ioctl-trace/xstream_trace_launcher.c` | `48FC18EE251ADED2600412BAE0823C8B889752598653D4425035EEA766A55780` |
| `tools/xstream-ioctl-trace/xstream_io_hook.c` | `0B88DCA0C653E08B90407F94F8776A58D3200F863BA763291A02323A54351C16` |
| `scripts/build-xstream-ioctl-trace.ps1` | `17707EFA694000FC81C0C9028C27C091C75951885F54D837CFCBF0EF6355C418` |

The production artifacts are x86 PE (`Machine=0x014C`); `dumpbin` reports the
launcher entry point `0x004025D2` and hook entry point `0x10004D84`. Both import
only `KERNEL32.dll`. They are local ignored build outputs, not repository
artifacts:

| File | Bytes | SHA-256 |
|---|---:|---|
| `xstream_trace_launcher.exe` | 141,824 | `75AA30B4F6E09D7BC7C86DDB707B32C3C07FE38EAF8D4F3CF67C3726FAD1EE0B` |
| `xstream_io_hook.dll` | 103,424 | `B3BBE925B6CF93376200D2282F4C8AF8ABCD36D3C4272C663549F1044F3E1712` |

The old production hashes did not match these new binaries and are superseded
for this local execution. The built binaries were copied into a
separate clean detached worktree at the exact tested script HEAD
`efb1ad4fbf1100c8ff2648bedba6a4b0717f5998`:
`C:\Users\LeCroyUser\.codex\worktrees\batch32-live-candidate`. Its Git status
is clean because the ignored `build\x86` outputs are not tracked.

The intended production launcher/hook pair was run end-to-end against the
synthetic x86 target from `build\x86-test`; that target uses `NUL` and a
temporary file, not XStream or a device. It captured six Win32 and one direct
`NtDeviceIoControlFile` 130-byte `CFDC2110` request without truncation. The six
Win32 `NUL` calls failed with `ERROR_INVALID_FUNCTION`; the native call
returned `0xC0000010` with `IO_STATUS_BLOCK.Information=0`. These are expected
synthetic-target outcomes, not LeCroy transport results. The hook wrote a
normal footer, the parent recorded exit code 0 and successful flush, and the
production validator returned `PASS` / `COMPLETE_NORMAL_CAPTURE` for seven
records. Final synthetic trace SHA-256 is
`F0CE9C71CB429B9B58A55BD65E55C335F477C9F0EB9E285B04D44F65F1151BCC`; capture
result SHA-256 is
`F418A882FBEC0356EE57F05D88C3D1D8DA5B9D93E8A779DEE3192ABE3835F484`; validator
result SHA-256 is
`BBA095DCB108D3C33F23930ADADAFFFC6F21007B942AEBFF101E349C962569DF`. These
synthetic files remain private and outside the repository. The 29/29 focused
test-mode suite verifies exact request capture, native NTSTATUS and
`Information` metadata, response preservation via a test-only shim, footer
integrity, parent flush, and normal/partial/unknown/corrupt classifications.
The synthetic 14-byte success response applies only to the test-only response
shim, not the actual production hook run.

Current script SHA-256 identities:

| Script | SHA-256 |
|---|---|
| `scripts/invoke-diagnostic-capture-install.ps1` | `8A7D86E5E6AFD1D4322CB204DDEBC42896919E537AAB762B7994CEE9F27891D2` |
| `scripts/invoke-xstream-lossless-capture.ps1` | `6634F51CA16B9EA38FA61D09F43415B5CEEDFFBA066B157D7A028133C19BBA6D` |
| `scripts/invoke-xstream-single-waveform.ps1` | `8B4C87F0EC15FED1DC6ADC2EF27B60A32099133F23564F4A4F8E19E4DC6546F3` |
| `scripts/validate-xstream-ioctl-capture.ps1` | `745CB6BAB8A05768CDFA2E9518D579B1F00FB9B01695E5452B2669C780CB68C9` |
| `scripts/xstream-com-lifecycle.ps1` | `E948F2E4451D26FA2B392B996F8F76AFEC28E34277DEFCF90695F4B9EBC943A1` |

DebugView 5.02 is present with the documented SHA-256
`7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B` and a
valid Microsoft Authenticode signature. During the authorized run its kernel
component loaded temporarily. The capture readiness record confirmed kernel
and verbose-kernel modes, healthy collector process, initialized capture and a
stable independently attributable StorNVMe `POWER: ACTIVE` message. After the
300-second capture duration, the DebugView process had ended and no `Dbgv`
system-driver service remained.
The COM server executable remains pinned at SHA-256
`27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F`.

## Executed once: pinned XStream invocation

The following documented command was executed exactly once from the elevated
64-bit Windows PowerShell 5.1 parent, using the clean detached worktree above,
the frozen Batch 31 package and authorization ID
`B32-XSTREAM-20261127-01`. Its authorization is consumed. Do not replay it.
The evidence directory was new at dispatch. The initial attempt to extract the
block from the older pinned worktree stopped before process start; no capture
or XStream action occurred in that failed preparation. The single actual
execution used this exact block from the updated Batch 32 handoff.

```powershell
$repo = 'C:\Users\LeCroyUser\.codex\worktrees\batch32-live-candidate'
$package = 'C:\Users\LeCroyUser\.codex\driver-backups\Batch31-SignedCandidate-20261126-01'
$evidence = 'C:\Users\LeCroyUser\.codex\driver-backups\Batch32-XStreamWithCapture-20261127-01'
$debugView = 'C:\Users\LeCroyUser\.codex\tools\sysinternals\DebugView-5.02\extracted\dbgviewcli64.exe'
$script = Join-Path $repo 'scripts\invoke-diagnostic-capture-install.ps1'
$runner = Join-Path $repo 'scripts\invoke-xstream-lossless-capture.ps1'
$waveformRunner = Join-Path $repo 'scripts\invoke-xstream-single-waveform.ps1'
$traceLauncher = Join-Path $repo 'tools\xstream-ioctl-trace\build\x86\xstream_trace_launcher.exe'
$traceHook = Join-Path $repo 'tools\xstream-ioctl-trace\build\x86\xstream_io_hook.dll'
$validator = Join-Path $repo 'scripts\validate-xstream-ioctl-capture.ps1'
$xstream = 'C:\Program Files (x86)\LeCroy\XStream\lecroyxstreamdso.exe'
$ps51 = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
& $ps51 -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $script `
  -Mode XStreamWithCapture `
  -RepositoryRoot $repo -ExpectedLauncherCommit 'efb1ad4fbf1100c8ff2648bedba6a4b0717f5998' `
  -PackageRoot $package -ExpectedManifestHash '3A7EB449FD7394EAF7E28C04226131EB7FF65EE561A1EDC2DD171078FECC8647' `
  -ExpectedPackageWorkflowCommit '2e0e9a781e6df68fa83795889cf6bbcf85ef5472' `
  -ExpectedDriverSourceCommit '50530b8d88cfee36ec23b8648611a4bafd6dfc4e' `
  -ExpectedDriverTree '5225aaed874c2f5b461280b63f234d83563cd2e3' `
  -ExpectedIncludeTree 'eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add' `
  -ExpectedDriverVer '11/26/2026,0.2026.1126.1' `
  -ExpectedCertificateThumbprint '4296FA2E355613DF0D5CE6F24D0898E00B706472' `
  -ExpectedInfHash 'AF7A16A700B5D90B9EA5DFDB0B243464804DF63F785423CE2D9F39E2969449EF' `
  -ExpectedCatHash 'BEE0B662B55AEB0931806F9E0A9056330FCE64E02B182FC6014423601112D789' `
  -ExpectedSysHash '6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75' `
  -ExpectedPdbHash '020EC0333DB143AE76EA71B28CBF66DBFA1F86816724D7EA8A4A81C97F4F2484' `
  -DebugViewPath $debugView -ExpectedDebugViewHash '7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B' `
  -EvidenceRoot $evidence -AuthorizationId 'B32-XSTREAM-20261127-01' `
  -AuthorizeKernelCapture -AuthorizeXStream -ConfirmPhysicalDiskDisconnected `
  -ExpectedCurrentInf 'oem109.inf' -ExpectedCurrentVersion '0.2026.1126.1' `
  -ExpectedCurrentSysHash '6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75' `
  -XStreamRunnerPath $runner -ExpectedXStreamRunnerHash '6634F51CA16B9EA38FA61D09F43415B5CEEDFFBA066B157D7A028133C19BBA6D' `
  -XStreamExecutablePath $xstream -ExpectedXStreamExecutableHash '27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F' `
  -XStreamWaveformRunnerPath $waveformRunner -ExpectedXStreamWaveformRunnerHash '8B4C87F0EC15FED1DC6ADC2EF27B60A32099133F23564F4A4F8E19E4DC6546F3' `
  -XStreamTraceLauncherPath $traceLauncher -ExpectedXStreamTraceLauncherHash '75AA30B4F6E09D7BC7C86DDB707B32C3C07FE38EAF8D4F3CF67C3726FAD1EE0B' `
  -XStreamTraceHookPath $traceHook -ExpectedXStreamTraceHookHash 'B3BBE925B6CF93376200D2282F4C8AF8ABCD36D3C4272C663549F1044F3E1712' `
  -XStreamTraceValidatorPath $validator -ExpectedXStreamTraceValidatorHash '745CB6BAB8A05768CDFA2E9518D579B1F00FB9B01695E5452B2669C780CB68C9' `
  -XStreamTimeoutSeconds 240 -CaptureDurationSeconds 300 `
  -CaptureReadinessTimeoutSeconds 60 -CaptureShutdownGraceSeconds 15
```

## Authorized attempt timeline and results

All times below are UTC. The capture process started at `00:36:04.184Z`.
DebugView reported successful kernel-driver startup and emitted an attributable
StorNVMe kernel line. Capture readiness was persisted at `00:36:07.202Z`.
The XStream dispatch marker was written at `00:36:10.088Z`; exactly one
`lecroyxstreamdso.exe` process was created. COM root activation started at
`00:36:10.163Z` and completed at `00:36:28.525Z`. The first `Acquisition`
lookup began at `00:36:28.530Z`. It ended at `00:40:28.343Z` with
`0x800706BA`; the observer result records one XStream launch and zero explicit
`Acquire()` calls. The trace launcher timed out and terminated the target with
exit code `1460`. Capture stopped normally after its bounded 300-second
duration at `00:41:04Z`.

The outer launcher result is `FAIL`, exit code 1, with one capture invocation,
one XStream invocation, zero install/PnP/no-launch invocations and no retry.
The XStream capture classification is
`VALID_PARTIAL_LAUNCHER_TERMINATION`: 3,748 trace records, one header, no
footer, parent flush successful. The 482 captured `CFDC2110` calls all use the
native API, have NTSTATUS `0x00000000`, are non-pending, and have neither input
nor output truncation. The captured protocol identities contain no family-2
opcode `0x03`; the expected 130-byte request and 14-byte response were not
observed. No direct user-mode IOCTL `0xCFDC2138` was recorded. Therefore there
is no `CFDC2138` return status to report for this run.

The kernel log contains 42,154 lines and no `CFDC2138` transaction or `MAMRGO`
marker. The driver source emits its `CFDC2138` transaction trace at error
level; the verbose kernel capture did not receive it. **Observed MAM GO count:
zero.** This is an evidence-based no-GO result for the captured attempt, but
the internal one-shot latch cannot be queried; preserve the no-retry rule and
do not assume the latch is reusable. No DMA completion, IRQ/DPC transfer,
waveform, or acquisition was observed.

The raw private IOCTL trace is 2,214,085 bytes with SHA-256
`B30AD0D22D60F0FBA74B7B47D13574D260DCA42184B7C633FDC87AFAADBD2593`.
The kernel capture log is 3,398,232 bytes with SHA-256
`7D5DDE3DE7B232E4F34096F00DC3CB5E6C68E435F2430E36DBF025B617C644C8`; the
live stdout capture is 3,440,492 bytes with SHA-256
`F3455131761A1BA62CCA1793DC144B992B3DC7BFC15FA8F0B01283A3DE103C36`. The
capture readiness JSON hash is
`47A314F5E5EF5C14097359A3D0769728D77C4C500B7E3B47AA9DD2F064AD655B`.
The outer launcher result hash is
`E95E636FDE1BE2C7FD5B12601F2AA07278ACD4E289E5C5E61B47FEBD06FF4C27`;
the detailed XStream result hash is
`564F244EF3464C35B0C4D2E9F1B6C51DDED26B3602FE3C405DE14DCDCB8C0D5E`;
and the waveform observer result hash is
`4B95EDEB5BE63E95D435C2CFA424B0A0D365F02AFDBA025AB277F1C633CD6B1C`.
Raw traces and payloads remain outside the public repository. The sanitized,
JSON-validated analysis summary is stored beside the private evidence at
`batch32-analysis-summary-final.json` (SHA-256
`4D0D0C04DFA1C15247DDDEECBCCF98676E4A5FD9BE43827D65D6C9484C6C10CC`).

After capture, passive verification at `00:45:53Z` confirmed the exact same
`oem109.inf`, version, loaded SYS hash, device `OK`, problem code 0, running
service and five enabled interfaces. No XStream or DebugView process and no
`Dbgv` service key, system-driver entry, or `%SystemRoot%\System32\drivers\Dbgv.sys`
file remained. No relevant System Kernel-PnP, WHEA or service events were
found. Code Integrity 3033/3089 events at `00:35:37Z` and
`00:38:35Z` concern `svchost.exe` loading Bonjour `mdnsNSP.dll`; one occurrence
preceded dispatch and none named LeCroy, XStream or DebugView. The OS remained
responsive.

Batch 32 is stopped. Opcode `0x03`, `CFDC2138` completion/status,
`IO_STATUS_BLOCK.Information` for that request, Acquisition object availability
and waveform delivery remain unqualified. Any future test requires a new
diagnosis of the RPC server lifetime, a fresh owner authorization, and a plan
that treats the FDO-local MAM launch latch as unknown. Do not repeat this run,
call `Acquire()`, reset the launch limit, reload the driver, or start a second
COM activation under this authorization.
