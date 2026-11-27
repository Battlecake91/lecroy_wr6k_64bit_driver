# Batch 33 — XStream COM blocking and timeout causality

## Scope and result

Batch 33 is an offline-only analysis of the retained Batch 32 attempt and
diagnostic tooling. No XStream process, device interface, IOCTL, capture
driver, PnP operation, driver, or hardware was accessed. The Batch 32 private
evidence remains outside this public repository.

Classification of `0x800706BA`: **indeterminate; launcher-induced RPC loss is
strongly plausible but not proven**. The native trace launcher did terminate
its target at its 260-second process timeout and reported target exit code
1460. However, its old result omitted the target resume, termination-request,
termination-call return, and observed-exit timestamps. The calculated target
deadline window includes the RPC exception timestamp, so the evidence does
not establish which occurred first.

## Source and timeout ownership

`Get-XStreamObject` synchronously calls `Parent.Item(Name)`. For ordinary
dispatch errors it tries the legacy `Objects.Item` and `Object.Item` paths;
RPC-server-unavailable HRESULTs are rethrown immediately. The waveform
observer establishes its 240-second readiness deadline after the root COM
object is returned, but checks that deadline only between synchronous COM
calls. A blocked `Item('Acquisition')` cannot return control to that loop, so
the deadline cannot interrupt it.

On the Batch 32 source path, the native trace launcher waited for 260 seconds
after `ResumeThread` (`XStreamTimeoutSeconds + 20`). At `WAIT_TIMEOUT`, it
called `TerminateProcess(target, ERROR_TIMEOUT)`, which accounts for exit code
1460 when termination completed. The PowerShell observer process was given a
270-second native-process wait (`TimeoutSeconds + 30`) and the outer diagnostic
wrapper also used a 270-second XStream-runner wait. Neither outer timeout is
shown as the source of the Batch 32 observer exit: the observer returned exit
1 with the RPC exception. The separate capture worker had a longer bounded
capture lifetime.

The trace hook header was written at `00:36:08.1847243Z`, before the native
launcher resumed the target. The waveform observer started at
`00:36:08.7843545Z`, after target readiness/resume. The exact resume instant
was not stored. Therefore the target's nominal 260-second deadline lies
between approximately `00:40:28.184Z` and `00:40:28.784Z`, a range containing
the recorded RPC time. The endpoints are bounds from hook-header and observer
start ordering, not measured resume/deadline timestamps.

## Directly recorded timeline

All timestamps below are UTC. “Observed” values came from retained JSON or
logs; timeout windows are calculated bounds.

| Time | Event | Evidence and limitation |
|---|---|---|
| `00:36:04.1885488` | DebugView capture process start | `capture-process.txt`; not the XStream target start |
| `00:36:07.2023315` | Kernel capture ready | Readiness JSON confirms initialized capture and an attributable StorNVMe message |
| `00:36:08.1847243` | Trace hook header | Private JSONL header; target had not yet resumed |
| between hook header and observer start | Target resume | Required by launcher source ordering; no exact timestamp in Batch 32 |
| `00:36:08.7843545` | Observer start | Observer result JSON |
| `00:36:10.1635044` | Root COM activation begins | Observer result JSON |
| `00:36:28.5251868` | Root COM activation returns | Observer result JSON |
| `00:36:28.5301839` | First `Acquisition` lookup begins | Observer result JSON; the synchronous call then blocks |
| `00:40:28.3430275` | `0x800706BA` recorded | Observer result JSON; this is exception observation time, not necessarily first internal failure time |
| `00:40:28.5251868` | Internal 240-second lookup deadline | Root-return time plus 240 seconds; it was 182 ms after the RPC exception and could not interrupt the blocked call |
| approx. `00:40:28.184`–`.784` | Native 260-second target deadline | Calculated from the pre-resume hook header and post-resume observer start; crosses the RPC time |
| `00:40:30.4197461` | Partial trace classified | Classification result; target exit had been reported as 1460 and termination as `launcher_timeout_terminated` |
| `00:40:30.8015096` | Lossless capture result completed | Result JSON; target was no longer present |
| `00:40:30.9264345` | Outer launcher result completed | Result JSON; one XStream invocation, zero install/restart operations |

The `0x800706BA` observation may have followed target termination, or a
separate RPC failure may have occurred shortly before it. The overlapping
deadline bounds prohibit a definitive causal ordering. The successful root
proxy return does not prove the out-of-process server remained alive for the
subsequent property access.

## Acquisition lookup and application initialization

Source proves that the observer makes one synchronous automation property
request through COM. It does not expose XStream's internal implementation of
the `Acquisition` property, its locks, event/callback dependencies, or server
thread state. The property can plausibly wait for object-model initialization,
but this remains a hypothesis. A driver deadlock is not established by the
absence of `CFDC2138`.

The registered COM server in retained Batch 32 preflight was the 32-bit
`lecroyxstreamdso.exe`, version `0,23,95,31`. No application dump, matching
WER report, or relevant Application/System error event was found in the
available local records for the attempt window. This absence does not prove
the server was healthy or show its internal wait state. STA use was recorded;
that does not rule out server termination or a server-side RPC failure.

## Private IOCTL trace analysis

The retained JSONL has 3,749 total records: header plus 3,748 records, of
which 532 are completed native `NtDeviceIoControlFile` records. Of those,
482 are `CFDC2110`; all 482 have returned NTSTATUS `0x00000000`, are not
pending, and have no captured input/output truncation. This establishes API
completion and byte capture boundaries only; the response bytes were not
validated as accepted XStream protocol results.

The final completed LeCroy IOCTL is sequence 3748 at
`00:38:18.0196946Z`, TID 11072, family 1/opcode `0x81`, with native status
success and 16 bytes reported. Preceding completions at sequences 3746–3747
were family 1/opcode `0x81` and family 0/opcode `0x84`, respectively. Their
opaque payload bytes are intentionally not reproduced here.

Across the trace there are 63 family-1/opcode-`0x81` and 52
family-0/opcode-`0x84` native calls. Four `CFDC2110` completions occurred
between the first `Acquisition` lookup and the recorded RPC exception,
including two family-1/opcode-`0x81` completions and one family-0/opcode-`0x84`
completion. The repetition is consistent with polling, but source semantics
were not established for this evidence. It does show that some transport
calls completed while the COM lookup was blocked.

The hook writes records after the underlying API returns; it does not emit
request-entry records. Therefore there is no unmatched entry record to
identify, but a call still in flight at process termination cannot be ruled
out. No family-2 opcode `0x03` or direct `CFDC2138` was captured. The kernel
log has no `CFDC2138` or `MAMRGO` marker. No MAM GO, IRQ/DPC completion, or
waveform was observed. Because the trace is partial, the FDO-local
`MamLaunchLimit=1` latch remains unknown and must not be treated as unused.

Batch 28's earlier RPC investigation also lacked exact target termination and
COM-call boundaries. A later Batch 28 continuation had different transport
activity and an observer-guard failure; it is not evidence of identical timing
or cause. See the [Batch 28 handoff](batch-28-first-xstream-startup.md) and
[Batch 29 analysis](batch-29-xstream-rpc-capture.md).

## Offline diagnostic changes

The Batch 33 branch adds:

- `scripts/xstream-process-diagnostics.ps1`: exclusive, flushed JSONL
  lifecycle journals, process/thread snapshots, and conservative causal
  classification.
- `scripts/invoke-xstream-single-waveform.ps1`: durable records immediately
  before and after root COM activation and the `Acquisition` lookup, including
  HRESULT and observer state. No request/response payload is written to this
  lifecycle journal.
- `scripts/invoke-xstream-lossless-capture.ps1`: separate 245-second observer
  process bound and 300-second target bound for the default 240-second
  observer configuration; a process/thread snapshot is captured after the
  observer bound and before eventual target cleanup. The observer result and
  target lifetime are recorded separately. The long target bound is not an
  artificial delay; it reserves a bounded interval to snapshot the server
  while a synchronous client call is stuck.
- `scripts/native-process.ps1`: durable wait start/deadline, timeout cleanup
  request, job-handle close result, and child exit-observation timestamps.
- `tools/xstream-ioctl-trace/xstream_trace_launcher.c`: UTC creation/resume,
  wait/deadline, timeout, `TerminateProcess` result, and exit-observation
  metadata in the capture result. Termination waits are bounded to five
  seconds; a missing exit observation remains explicit.

The default production trace hook and driver are unchanged. No synthetic test
hook is compiled into the production binary. Offline testing exposed and
corrected a test-fixture omission for the new diagnostic helper, and a
PowerShell 5.1 `ConvertFrom-StringData` backslash-escape failure was replaced
with literal key/value parsing.

## Offline validation

Validated with Windows PowerShell `5.1.19041.6456`:

| Suite | Result |
|---|---:|
| Diagnostic capture/install launcher | 51/51 PASS |
| XStream COM lifecycle and synthetic process deadlines | 24/24 PASS |
| XStream IOCTL trace/launcher/OfflineMock chain | 32/32 PASS |
| Single-waveform observer | 13/13 PASS |

The IOCTL trace suite compiles the x86 production launcher and hook and runs
only a synthetic target. The COM lifecycle tests use synthetic child
processes; they do not instantiate the vendor COM server. No driver build,
signing, installation, kernel capture driver, device handle, IOCTL, or
hardware operation occurred.

## Freshly authorized follow-up: stopped before capture

On 2026-11-27 the owner freshly confirmed physical disconnection of the
productive x86 disk and authorized one additional `XStreamWithCapture` run on
the unchanged `oem109.inf` candidate. The elevated 64-bit PowerShell 5.1
bootstrap started once but failed closed before invoking the repository's
diagnostic launcher. Its Driver Verifier command returned flags
`0x00000000` and output `Überprüfte Treiber: Keine`; however, a temporary
bootstrap file written as UTF-8 without a BOM contained a literal German
`Überprüfte Treiber` regex. Windows PowerShell 5.1 decoded the no-BOM script
using the legacy ANSI code page, so the regex returned false and the bootstrap
mistook the inactive state for an unproven state. An ASCII-only PS 5.1 check
that constructed the German characters from Unicode code points matched the
same read-only Verifier output, confirming the false-negative parser cause.

Control flow shows that the bootstrap passed its BCD `testsigning Yes`
assertion before stopping at the Verifier guard; the raw BCD text was not
persisted, so this is a control-flow inference rather than retained command
output. The private failure JSON is
`batch33-bootstrap-failure.json` in the private attempt directory,
SHA-256 `C936EB1BCD9DDF20F81AB590F3F31BE7E6BF696CEDBAD3EC564A00DF55DC36FC`.
It contains the complete verifier output and preflight error. No production
launcher `result.json`, capture-attempt marker, XStream-attempt marker, raw
capture, DebugView process, `Dbgv` service, XStream process, COM activation,
device handle, IOCTL, or MAM GO was created or dispatched. The evidence
directory contains only this failure JSON.

Post-stop passive checks at `2026-11-27T22:38:45.2349851Z` confirmed
`oem109.inf`, version `0.2026.1126.1`, device `OK` / problem code 0, service
running, loaded SYS SHA-256
`6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75`, and no
XStream/DebugView process or `Dbgv` service. The authorized run itself did not
reach capture readiness or XStream dispatch. The stop condition was honored;
do not repair and rerun under this authorization. Any future active run
requires fresh owner approval, with the FDO-local MAM latch still unknown.

## Authorized startup on November 28, 2026

Authorization `B33-XSTREAM-ACQUISITION-20261128-01` was used once with
`f43988551967d6cca376ff9773c398f738f993ae` and the installed `oem109.inf`
candidate. The owner reconfirmed that the productive x86 disk was physically
disconnected. The captured host readback passed BCD test signing and both
Verifier queries. A StorNVMe kernel message established capture readiness.
The outer launcher recorded one XStream dispatch, one capture, and zero
install, restart, or no-launch invocations. It returned exit 1. There was one
root COM activation and zero explicit `Acquire()` calls.

| UTC, November 27 | Recorded event |
|---|---|
| `23:22:33.054` | Kernel capture ready. |
| `23:22:34.190` | The native launcher resumed instrumented PID 1824. |
| `23:22:36.998` | The observer began the root COM activation. |
| `23:22:37.027` | A second `lecroyxstreamdso.exe` process, PID 9160, was created in the same session. Its recorded parent, PID 884, hosted `DcomLaunch` and other services. |
| `23:22:55.051` | COM root activation returned. `Acquisition` returned an object about 297 ms after its first lookup began. |
| `23:24:44.336` | Final captured LeCroy native completion, family 1 opcode `0x81`, sequence 3748. |
| `23:25:54.318` | The optional debugger gate observed a returned lookup and skipped CDB. |
| `23:26:39.124` | The last of 718 recorded `Acquisition` calls returned an object. Median call duration was 11 ms, maximum 297 ms. |
| `23:26:39.400` | The independent observer deadline expired. The supervisor closed the observer job and reported timeout 124. |
| `23:26:39.472` | Instrumented PID 1824 was still alive. Its 33 threads reported 30 `UserRequest` waits and three `EventPairLow` waits. This was thread-state metadata without stacks or waited-object identity. |
| `23:27:34.191` | The native launcher reached its 300-second target deadline and its termination request succeeded. |
| `23:27:34.287` | PID 1824 exit observed with code 1460. The private IOCTL trace had no footer. |
| `23:27:36.283` | The outer launcher persisted failure. |
| `23:31:26.610` | Passive inspection confirmed that DebugView and its kernel service were absent and that PID 9160 remained present. The device was OK, problem code zero, with the expected running service and SYS hash. |

The observed process count exceeded the intended one-process condition.
The observer invoked root activation once. The start timing and parent of
PID 9160 strongly support a separate COM server launch. Neither the COM
interface pointer's server PID nor the second process's command line was
captured. Automation responses therefore cannot be tied to the instrumented
process. The observer's prior guard checked for another server only after
its normal waveform path. It failed to detect the split in this run.
PID 9160 was left alone after the run. Passive inspection found no relevant
PnP, WHEA, DCOM, Code Integrity, crash or hang event in the checked interval.

Batch 32's `Acquisition` wait did not reproduce. This run recorded no
`0x800706BA` before the observer stopped, about 55 seconds before target
termination. There was no live observer to report a later RPC result. The
current timing therefore does not resolve the older RPC failure. The observer
produced no waveform result. Its internal waveform polling deadline started
after activation and fell about 16 seconds after its independent process
deadline. The last recorded successful `Acquisition` call cannot establish
which later property or waveform condition prevented delivery.

### Event and DMA evidence

Both the saved kernel log and live collector output contain 511 WR6k IOCTL
entries and 511 corresponding completions by code. Every entry belongs to
instrumented PID 1824. There are six creates and six closes for that PID and
no captured WR6k IOCTL from PID 9160. The private trace classifies as valid
partial launcher termination and has 3,748 records plus the header, with
532 completed native calls. All 511 LeCroy completions succeeded at the
NTSTATUS boundary without pending returns. The 483 `CFDC2110` records had no
capture truncation. Matching kernel counts argue against an open WR6k IOCTL
inside the logged dispatch interval. They do not rule out a wait in user
mode, below the visible capture boundary, or on an event.

The event registrations `CFDC2180`, `CFDC218C` and `0x00223100` succeeded
before the early acknowledgement and enable calls. The recorded `0x85`
enable mask reached `0x02BF`, as in both known-good traces. Those traces then
reach `85FB/40/01` reporting sticky pending `0x0080`, the corresponding
`0x88` acknowledgement and the second `CFDC2184`; none of those post-startup
operations arrived in the retained current run. The driver source latches
enabled interrupt bits before signaling its registered event in the DPC.
The installed build does not journal the relevant interrupt source, DPC
pending mask, or `KeSetEvent` decision. Registration success and the missing
status query cannot prove a lost signal, a lost replay, or that the expected
interrupt was raised. The same registration code did not replay pending state
in the earlier working source. Treat the process split as a confirmed
diagnostic defect before changing driver event semantics.

Neither source recorded `CFDC2124`, `CFDC2138`, a MAM admission or GO,
or DMA retirement. No family 1 opcode `0x51` or family 2 opcode `0x03`
was captured. The evidence establishes no observed MAM GO and gives no basis
for assuming that the FDO-local latch is unused.

The following hashes identify private originals. The capture worker did not
publish an exit sidecar after the outer run failed, although DebugView was
subsequently observed to exit and both kernel outputs were preserved.

| Evidence | SHA-256 |
|---|---|
| Kernel raw log | `8605C1371A7271C6CA192CE1413909E5F9D3D11097DDC30ED9ED1E2E702ECD14` |
| Kernel live stdout | `D22EAFD49402B0C58F9A389FEF116E58F6EFEBB3BDBB2337854945B5FF5EAE2E` |
| Private IOCTL trace | `FBA3259B36F57F18982AAAEEAC55F7E5C2B2F7BFC41C5009C3E0F2F0D6AD17AC` |
| Observer lifecycle | `30459D1449D9CB04AA6CBD4AAE9B617A9B2DD4E5A5B999D3454D6B04A253584D` |
| Native target termination record | `89C69EB6671FBFEA3C04A1EAA1AA3CB408B791F7DB955E4C4891A463E826121F` |
| Final passive state | `99D951E7E9A8113DB117B8BEB5E7DFF299684E958E2BA0C81DF67523867BFBA2` |

### Prepared observer correction

The observer now verifies after COM activation that the only matching
XStream process still has the captured PID, executable path and creation
time. It journals the visible process identities and stops before the first
`Acquisition` lookup if the check fails. This detects the observed split
once activation returns. It cannot prevent COM from
creating a second process during activation or independently resolve the
server PID behind a proxy. Focused Windows PowerShell 5.1 tests passed
30/30 COM lifecycle cases and 13/13 single-waveform mocks. Before another
hardware run, establish offline how to demonstrate one instrumented COM
server, then refresh the approved pins. The installed driver and latch
were untouched by the correction.

## Pre-run preparation and executed command

`XStreamWithCapture` now performs its own read-only BCD and Verifier queries
in the versioned launcher. It writes the commands' combined output, exit
codes and the timestamp to private
`xstream-host-preflight.json` before evaluating the gate. The Verifier regex
uses ASCII source with Unicode escapes. Nonzero flags, selected or running
drivers, contradictory fields, unrecognized output and command failures block
capture. Under Windows PowerShell 5.1 the focused check passed 13 fixtures,
the retained Verifier output and passive live Verifier readback. The last two
cases supply a simulated positive BCD value. The actual BCD read requires the
elevated launcher and must pass at dispatch. The existing launcher mock suite
passed 51/51 on Windows PowerShell 5.1 using its test-only elevation override.
Package hashes, running service,
problem code zero and the SYS at the service path were checked passively.

The native artifacts were rebuilt from the Batch 33 source with the production
build command. Both PE images are x86. The launcher contains the resume,
termination API and exit-observation fields, which the earlier Batch 32
launcher on disk lacked. A synthetic `/Embedding` run using the pinned
production binaries exited zero and preserved seven complete requests,
the normal footer and the new resume and exit timestamps. The committed
source and on-disk artifact hashes are
separate pins. The command below records both identities; the only inputs
supplied at the approval boundary are the exact reviewed Git HEAD and the
fresh owner authorization, including physical x86-disk isolation.

The [trace comparison](../runtime-trace.md#comparison-with-the-stalled-batch-32-com-startup)
narrows the useful question to the transition from the initial transport and
event registrations to the first `85FB/40/01` pending-status query and MAM.
The first MTT request followed MAM in both known-good traces. The historical
tests used the normal GUI and kernel-ring recording. The logs lack historical
process arguments and COM apartment metadata. The upcoming test uses the
32-bit COM local server prelaunched suspended with `/Embedding` and a 64-bit
STA automation client, then performs one root activation and looks up
`Acquisition` synchronously. The client's STA setting does not establish the
server's apartment or show whether the GUI and embedded paths initialize the
same workers. Keep the launch mode fixed for this attempt.

### Invocation used at the original approved revision

This historical command was used from an elevated 64-bit Windows PowerShell
5.1 process with `$ApprovedHead` set to
`f43988551967d6cca376ff9773c398f738f993ae`. Its authorization ID and evidence
directory are consumed. The pins apply to that revision. The 420-second
kernel capture duration covered readiness, the 300-second target bound, and
evidence finalization. Readiness required an attributable kernel line.

```powershell
$repo = (Get-Location).Path
$backup = Join-Path $env:USERPROFILE '.codex\driver-backups'
$pkg = Join-Path $backup 'Batch31-SignedCandidate-20261126-01'
$evidence = Join-Path $backup 'Batch33-XStreamWithCapture-20261128-01'
if (Test-Path -LiteralPath $evidence) { throw 'Attempt evidence already exists.' }
if ($ApprovedHead -notmatch '^[0-9a-fA-F]{40}$') { throw 'Set the approved review commit.' }
$launcher = Join-Path $repo 'scripts\invoke-diagnostic-capture-install.ps1'
if ((Get-FileHash -LiteralPath $launcher).Hash -ne 'A4A27E8C6B9DFE2893475DBFA5190EBB6957468B2865EF86037E40384EC3EEDA') {
    throw 'Outer launcher hash mismatch.'
}
$debugView = Join-Path $env:USERPROFILE '.codex\tools\sysinternals\DebugView-5.02\extracted\dbgviewcli64.exe'
$sysHash = '6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75'
$ps51 = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
& $ps51 -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $launcher `
  -Mode XStreamWithCapture -RepositoryRoot $repo -ExpectedLauncherCommit $ApprovedHead `
  -PackageRoot $pkg -ExpectedManifestHash '3A7EB449FD7394EAF7E28C04226131EB7FF65EE561A1EDC2DD171078FECC8647' `
  -ExpectedPackageWorkflowCommit '2e0e9a781e6df68fa83795889cf6bbcf85ef5472' `
  -ExpectedDriverSourceCommit '50530b8d88cfee36ec23b8648611a4bafd6dfc4e' `
  -ExpectedDriverTree '5225aaed874c2f5b461280b63f234d83563cd2e3' `
  -ExpectedIncludeTree 'eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add' `
  -ExpectedDriverVer '11/26/2026,0.2026.1126.1' `
  -ExpectedCertificateThumbprint '4296FA2E355613DF0D5CE6F24D0898E00B706472' `
  -ExpectedInfHash 'AF7A16A700B5D90B9EA5DFDB0B243464804DF63F785423CE2D9F39E2969449EF' `
  -ExpectedCatHash 'BEE0B662B55AEB0931806F9E0A9056330FCE64E02B182FC6014423601112D789' `
  -ExpectedSysHash $sysHash -ExpectedPdbHash '020EC0333DB143AE76EA71B28CBF66DBFA1F86816724D7EA8A4A81C97F4F2484' `
  -DebugViewPath $debugView -ExpectedDebugViewHash '7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B' `
  -EvidenceRoot $evidence -AuthorizationId 'B33-XSTREAM-ACQUISITION-20261128-01' `
  -AuthorizeKernelCapture -AuthorizeXStream -ConfirmPhysicalDiskDisconnected `
  -ExpectedCurrentInf 'oem109.inf' -ExpectedCurrentVersion '0.2026.1126.1' -ExpectedCurrentSysHash $sysHash `
  -XStreamExecutablePath 'C:\Program Files (x86)\LeCroy\XStream\lecroyxstreamdso.exe' `
  -ExpectedXStreamExecutableHash '27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F' `
  -XStreamRunnerPath (Join-Path $repo 'scripts\invoke-xstream-lossless-capture.ps1') `
  -ExpectedXStreamRunnerHash '994229B4ECE93C845004A33DCA8FE417A98DD86733706E133B64B984CF4EE555' `
  -XStreamWaveformRunnerPath (Join-Path $repo 'scripts\invoke-xstream-single-waveform.ps1') `
  -ExpectedXStreamWaveformRunnerHash 'AFD7807C14E3E671C19BD8E54D877CF6D5628BA443B82A1BC854AF4007650941' `
  -XStreamTraceLauncherPath (Join-Path $repo 'tools\xstream-ioctl-trace\build\x86\xstream_trace_launcher.exe') `
  -ExpectedXStreamTraceLauncherHash '42E87BCFE64ADBA12CF5888C1AD0CE29825E9BF19F8A93988F5677C3726218B8' `
  -XStreamTraceHookPath (Join-Path $repo 'tools\xstream-ioctl-trace\build\x86\xstream_io_hook.dll') `
  -ExpectedXStreamTraceHookHash 'C7BFA34E380DA0D6090D28F1F6F397C6A7E288B00FE0B0BBE8F602D03012E43E' `
  -XStreamTraceValidatorPath (Join-Path $repo 'scripts\validate-xstream-ioctl-capture.ps1') `
  -ExpectedXStreamTraceValidatorHash '745CB6BAB8A05768CDFA2E9518D579B1F00FB9B01695E5452B2669C780CB68C9' `
  -XStreamTimeoutSeconds 240 -CaptureDurationSeconds 420 `
  -CaptureReadinessTimeoutSeconds 60 -CaptureShutdownGraceSeconds 15
```

### Evidence during the same attempt

- If `acquisition_lookup_started` remains the last observer journal event
  about 200 seconds after observer dispatch, take one bounded CDB snapshot
  in parallel, subject to the same approval. Verify the PID from the new
  `xstream-trace-ready.txt` against the live executable path and process
  start time within the attempt before querying it. Use the x86 Windows SDK
  CDB `10.0.28000.2526`, SHA-256
  `80E8C4F8437D273DFF3A72AAE6DE01EEF91E16A2BE3D4B64251395A5C651391F`.
  Call the existing `Invoke-LecS65NativeProcess` with CDB arguments
  `@('-pvr','-p',$targetId.ToString(),'-sins','-netsyms:no','-c','~* k 12;q')`,
  `-TimeoutSeconds 10 -StreamDrainTimeoutSeconds 2`, and a fresh private log.
  Record UTC boundaries and return status. `-pvr` is a noninvasive session
  without target suspension per the [CDB options reference](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/cdb-command-line-options).
  The exact command shape returned wait frames and left a synthetic x86
  process running. Vendor symbols and reliable unwind are not guaranteed.
  Skip the snapshot on PID mismatch, early completion or insufficient time.
- Retain the built-in snapshot at the 245-second observer process bound and
  native target termination metadata at the 300-second target bound. Compare
  the journal HRESULT timestamp separately with termination request, API
  return and observed exit. If the observer is stopped first, treat the RPC
  observation as censored by that independent cutoff. A live server with the
  call still pending does not establish a particular wait cause.
- Correlate any RPC, event wait, lock wait or `NtDeviceIoControlFile` frame
  with the same thread's latest completed call and the kernel trace. A stack
  may localize a wait. The completed-call hook alone cannot identify an
  unmatched call or prove which IOCTL is in flight. Keep raw stacks, kernel
  logs and transport bytes private.
- Check `85FB/40/01`, acknowledgement and any `CFDC2124` or `CFDC2138`
  outcome, including launch-limit denial, separately from a matched
  GO/IRQ/DPC/retirement. If initialization completes, record whether a C1
  waveform was delivered. Functional acceptance also requires comparison
  against a known physical input and successive valid acquisitions.
- Stop after the single attempt or the first prerequisite failure. No
  explicit `Acquire()`, CVar change, retry, PnP transition, driver replacement
  or latch reset is part of the proposed action.

## Repository state

- Starting commit: `f1bbd89dfa57fdac8b74641f859e443456c18ca3`.
- Development branch: `codex/phase2c-runtime-batch33`.
- Batch 32 parent pull request: #57, base `codex/phase2c-runtime-batch31`.
- Batch 33 pull request: #58, base `codex/phase2c-runtime-batch32`.
- Batch 33 contains no raw trace, opaque request/response data, vendor binary,
  or private system evidence.
