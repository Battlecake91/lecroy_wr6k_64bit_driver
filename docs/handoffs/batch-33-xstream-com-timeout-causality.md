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

## Ordinary XStream GUI startup

### Residual process and COM registration

Passive process enumeration on November 28 at `00:00:08Z` found zero
`lecroyxstreamdso.exe` or `XStream.exe` instances. PID 9160 from the previous
attempt was no longer an XStream process. Its exit mechanism and actions
after capture ended are unknown, so the FDO's remaining MAM admission is
unknown. Its executable arguments, effective security context, windows and
stacks can no longer be inspected. The earlier evidence establishes only
its image name, creation time, session, and DcomLaunch service-host parent.

The installed ProgID `LeCroy.XStreamDSO` resolves to CLSID
`{03960F1A-A888-4335-AB08-6F8BE55F0D9B}`. The merged and machine Registry32
views identify the quoted `C:\Program Files (x86)\LeCroy\XStream\lecroyxstreamdso.exe`
without extra arguments, with AppID `{1719D3E3-9F5B-4B27-AEB3-07443CCEA0A6}`
and no observed `ServerExecutable`, `RunAs` or `LocalService` override in the
keys inspected. The 64-bit view resolves the same ProgID and executable AppID
alias but did not expose a `LocalServer32` value for the class. No per-user
override was found in either view. The x86 executable imports
`CoRegisterClassObject`, `CoResumeClassObjects` and `CoRevokeClassObject`.
The imports establish capability. Actual class registration timing, flags and
reuse of the manually started server remain unproven. No live XStream process
was available to establish an XStream Running Object Table (ROT) entry.
The executable has no direct `RegisterActiveObject` or
`GetRunningObjectTable` import in the inspected table, which cannot exclude
registration by another module or a dynamically resolved call.

[COM appends `-Embedding` to LocalServer32](https://learn.microsoft.com/en-us/windows/win32/com/localserver32).
The previous native launcher explicitly passed `/Embedding`. Whether this
XStream version treats the spellings identically remains unverified.
The new GUI path passes no application arguments. The readied marker is
written after trace-hook initialization and before `ResumeThread`. It proves
neither class registration nor [publication of a running object](https://learn.microsoft.com/en-us/windows/win32/api/objbase/nf-objbase-getrunningobjecttable).
Registering a [COM class factory](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coregisterclassobject)
and registering an already running object are separate operations. The prior
instrumented process resumed 2.808 seconds before activation. A second
process appeared approximately 29 ms after activation began. A longer wait
or different switch spelling is a hypothesis. The two-process run cannot
attribute the successful COM replies to the traced hardware process or
establish a driver-side interrupt or pending-bit signaling failure.

### Boundaries and operation

Use `XStreamGuiWithCapture` from an elevated 64-bit Windows PowerShell 5.1
window on the interactive user desktop. Obtain fresh owner approval for a
single GUI startup, temporary kernel capture, private tracing, optional CDB
if the GUI stalls, and the cleanup described below. Reconfirm physical
disconnection of the productive x86 disk. Re-enumerate XStream processes.
If one exists, verify PID, image path, creation time, session and ownership
and stop before dispatch. Its closure requires separate explicit approval.
The current passive check requires no residual-process action.

The launcher verifies the unchanged installed `oem109.inf` candidate, the
signed artifacts and diagnostic tools, BCD, Verifier and kernel capture
readiness. It dispatches the x86 tracer exactly once with no `--embedding`.
The tracer starts XStream without arguments in the executable's directory.
The observer is absent from this mode. The supervisor records the hooked
PID, native parent, session, creation time, executable path, command line
and sampled window changes. A competing XStream process fails attribution.
The expected hardware exposure is ordinary application startup and
automatic IOCTLs, with at most one admission under the existing kernel
MAM limit. The existing latch, MTT gate and DMA quarantine stay in effect.

The GUI target has a 240-second native deadline. Observe startup, errors,
calibration and any visible C1 waveform without changing controls. Record
windows or screenshots in the private evidence directory. Exit the verified
GUI normally before the deadline when safe, to allow a trace footer. If it
stalls, an approved operator may take one bounded non-suspending stack
snapshot before the deadline. Use the confirmed `gui_target_validated` PID
with the matching path and creation time still present. Apply the pinned CDB
and 10-second supervised command from the preceding evidence section. The
condition is observable GUI progress having stalled. A COM lookup is not
part of this test. On unresolved state the native deadline terminates only
its owned target. An unexpected process is never selected for automated
cleanup. No automatic retry, device reset, reload or further acquisition
follows the attempt.

Compare the native IOCTL trace and kernel data for event registration,
`0x85` enable, `85FB/40/01`, pending status, `0x88`, `CFDC2184`, and
`CFDC2124`/`CFDC2138`. Compare against the known-good GUI traces
`005808` and `011938`. Distinguish MAM admission or limit rejection from
GO, IRQ/DPC completion and mapping retirement. Without a corresponding
kernel marker or usable wait evidence, report the exact evidence gap. A
complete trace or nonempty window alone does not establish waveform
acquisition. Check device health and residual process state passively after
the 420-second kernel capture finishes. Preserve and hash the raw originals
privately.

Offline validation on Windows PowerShell 5.1 passed 36/36 native trace cases
and 4/4 focused outer launcher cases. The synthetic target verified zero
application arguments, trace and process identity, a complete capture
without an observer, and rejection of a preexisting process. The outer mock
verified one dispatch, retained kernel collector evidence on GUI failure,
and both authorization gates. No vendor process or driver I/O was started
by these tests.

### Executed GUI invocation

The November 28 attempt used the command below with `$ApprovedHead` set
to `ea836cc8e8eb2c3a2a1650e1be5534e859fee06c`. Authorization
`B33-XSTREAM-GUI-01` and the evidence directory are consumed. The command
is retained as an execution record.

```powershell
$repo = (Get-Location).Path
$backup = Join-Path $env:USERPROFILE '.codex\driver-backups'
$scripts = Join-Path $repo 'scripts'
$traceTools = Join-Path $repo 'tools\xstream-ioctl-trace\build\x86'
$entry = Join-Path $scripts 'invoke-diagnostic-capture-install.ps1'
if ($ApprovedHead -notmatch '^[0-9a-fA-F]{40}$' -or
    (Get-FileHash -LiteralPath $entry).Hash -ne '4B416D46BD7EDC92789D6654D5167B84B2141823C0D5915D806E3BBBB9EDF915') {
    throw 'The approved commit or launcher hash is missing or incorrect.'
}
$argsForGui = @{
    Mode = 'XStreamGuiWithCapture'
    RepositoryRoot = $repo
    ExpectedLauncherCommit = $ApprovedHead
    EvidenceRoot = (Join-Path $backup 'Batch33-XStreamGui-01')
    AuthorizationId = 'B33-XSTREAM-GUI-01'
    AuthorizeKernelCapture = $true
    AuthorizeXStream = $true
    ConfirmPhysicalDiskDisconnected = $true
    PackageRoot = (Join-Path $backup 'Batch31-SignedCandidate-20261126-01')
    ExpectedManifestHash = '3A7EB449FD7394EAF7E28C04226131EB7FF65EE561A1EDC2DD171078FECC8647'
    ExpectedPackageWorkflowCommit = '2e0e9a781e6df68fa83795889cf6bbcf85ef5472'
    ExpectedDriverSourceCommit = '50530b8d88cfee36ec23b8648611a4bafd6dfc4e'
    ExpectedDriverTree = '5225aaed874c2f5b461280b63f234d83563cd2e3'
    ExpectedIncludeTree = 'eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add'
    ExpectedDriverVer = '11/26/2026,0.2026.1126.1'
    ExpectedCertificateThumbprint = '4296FA2E355613DF0D5CE6F24D0898E00B706472'
    ExpectedInfHash = 'AF7A16A700B5D90B9EA5DFDB0B243464804DF63F785423CE2D9F39E2969449EF'
    ExpectedCatHash = 'BEE0B662B55AEB0931806F9E0A9056330FCE64E02B182FC6014423601112D789'
    ExpectedSysHash = '6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75'
    ExpectedPdbHash = '020EC0333DB143AE76EA71B28CBF66DBFA1F86816724D7EA8A4A81C97F4F2484'
    ExpectedCurrentInf = 'oem109.inf'
    ExpectedCurrentVersion = '0.2026.1126.1'
    ExpectedCurrentSysHash = '6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75'
    DebugViewPath = (Join-Path $env:USERPROFILE '.codex\tools\sysinternals\DebugView-5.02\extracted\dbgviewcli64.exe')
    ExpectedDebugViewHash = '7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B'
    XStreamExecutablePath = 'C:\Program Files (x86)\LeCroy\XStream\lecroyxstreamdso.exe'
    ExpectedXStreamExecutableHash = '27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F'
    XStreamRunnerPath = (Join-Path $scripts 'invoke-xstream-lossless-capture.ps1')
    ExpectedXStreamRunnerHash = 'C0C757642332694DF7D7CF53F0CFE84044A0D84A233520ED2B8EF5802D21AD50'
    XStreamTraceLauncherPath = (Join-Path $traceTools 'xstream_trace_launcher.exe')
    ExpectedXStreamTraceLauncherHash = '42E87BCFE64ADBA12CF5888C1AD0CE29825E9BF19F8A93988F5677C3726218B8'
    XStreamTraceHookPath = (Join-Path $traceTools 'xstream_io_hook.dll')
    ExpectedXStreamTraceHookHash = 'C7BFA34E380DA0D6090D28F1F6F397C6A7E288B00FE0B0BBE8F602D03012E43E'
    XStreamTraceValidatorPath = (Join-Path $scripts 'validate-xstream-ioctl-capture.ps1')
    ExpectedXStreamTraceValidatorHash = '745CB6BAB8A05768CDFA2E9518D579B1F00FB9B01695E5452B2669C780CB68C9'
    XStreamTimeoutSeconds = 240
    CaptureDurationSeconds = 420
    CaptureReadinessTimeoutSeconds = 60
    CaptureShutdownGraceSeconds = 15
}
& $entry @argsForGui
```

### GUI result and first failing operation

On November 28, the elevated preflight and kernel capture readiness passed.
Exactly one `XStreamGuiWithCapture` invocation started the pinned XStream
executable without arguments in session 1. The monitor confirmed PID 13628,
parent PID 2484 and the expected image. No second XStream process or
automation observer was observed.

| UTC | Recorded event |
|---|---|
| `11:12:04.148` | Windows recorded wake from system sleep. The sleep event predates this attempt. |
| `11:13:59.222` | Native launcher resumed the traced GUI process. |
| `11:13:59.968` | The process exposed its `LeCroy DSO` window. |
| `11:14:00.671` | The first and only WR6k IOCTL completed with `STATUS_IO_TIMEOUT` and zero bytes. |
| `11:15:44.257` | One bounded non-suspending CDB snapshot began and exited zero about 0.6 seconds later. |
| `11:17:59.374` | The native launcher observed target exit 1460 after its 240-second deadline. |
| `11:20:57.319` | Kernel capture worker completed with exit zero. The outer launcher reported GUI failure with exit 1. |

The visible main client area remained black. The modal title was
`No Hardware Detected (2)`. No calibration UI or waveform was observed.
The debugger showed the primary thread in `win32u!NtUserWaitMessage` through
`USER32!SoftModalMessageBox`, `MessageBoxTimeoutW` and `MessageBoxW`. This
locates the visible wait in the error dialog after the IOCTL had returned.
Vendor frame unwinding was unreliable and does not identify the source
method. The first CDB preparation stopped before dispatch because a
PowerShell JSON date was compared as text. A normalized creation-time check
then verified the same PID, path, parent and session and dispatched CDB
once. The modal was left untouched and the native deadline ended the target.

The historical `005808` and `011938` GUI traces and the earlier embedded
attempt on this same installed driver started with:

| Operation | Historical and earlier embedded result | This GUI run |
|---|---|---|
| `0x00223080` GET_DALLAS_ID | Success, eight bytes | `0xC00000B5`, zero bytes from the kernel. |
| `0x00222400` | Success | Absent. |
| `0x00223084` Dallas memory read | Success | Absent. |
| First `CFDC2110`, event setup, and later acquisition | Reached transport and event setup. The historical GUI traces reached `85FB/40/01` with pending `0x0080` and MAM. | Absent from both user and kernel traces. |

The completed collector recorded one WR6k entry and one completion. In
that request the installed `LecOneWireIssue` exhausted its busy polling
after the reset command ten times, with final `BAR2+0x040` read
`0xFFFFFFFF` each time. It returned a timeout before proceeding to the
ROM command, presence sample, or CRC. The native trace's information field
for the failed call is implausible for an eight-byte output. The kernel
completion is the source for zero bytes. The private native trace is
classified as valid partial launcher termination with 340 records plus its
header and no footer.

No event registration, `85FB/40/01` query, MAM request, limit rejection,
GO, IRQ/DPC completion, or mapping retirement was observed. This run stopped
earlier than both the known-good GUI profiles and the earlier embedded
attempt. Movement of the MAM latch is not evidenced.

### Narrow correction and remaining cause

The all-ones BAR response shows that the polled register did not supply
a usable response. It does not prove a missing Dallas chip or localize the
fault to the PCI BAR, address decoding, FPGA, or resume path. The ROM read
and 1-Wire source was identical in `6cc5a238`, `34907090` and the installed
source revision `50530b8d`. This same installed image returned eight bytes
from GET_DALLAS_ID in the preceding embedded attempt. Windows logged a
sleep/wake interval between attempts, with wake about 110 seconds before
this startup. The PCI device still reported problem code zero. The power
record makes resume a concrete lead, without proving when or why BAR2
became unresponsive.

The source correction preserves the command sequence and polling bound.
If the busy poll is exhausted and the last value is all ones, it returns
`STATUS_DEVICE_NOT_CONNECTED` and ends the ROM retry loop. A transient
all-ones sample followed by valid completion behaves as before. A genuine
busy timeout with another terminal value still follows the previous
bounded retry path. This avoids the observed repeated resets and separates
the unavailable-controller result from an ordinary timeout. It neither
fabricates an ID nor attempts hardware recovery. The source contract suite
passed 77/77, and a host-only harness compiled the exact changed production
functions with five mock cases. No driver was built, packaged or installed.

The separately authorized follow-up reached the PCI configuration check
and found disabled memory decode. See the
[post-resume snapshot](#post-resume-pci-decode-snapshot).

Passive completion at `11:21:39.742Z` showed `oem109.inf`
`0.2026.1126.1` with the approved SYS, device OK/problem code zero and the
service running. XStream, CDB and DebugView were absent, with no capture
driver service left. The interval had no matching Application, Kernel-PnP,
WHEA or DCOM event. The Code Integrity 3033/3089 records concerned
`mdnsNSP.dll` and `svchost.exe`.

| Private evidence | SHA-256 |
|---|---|
| Kernel capture | `8811717AACB118A66B88AED174BCFE9829BFDE00D89C53B4FF2A83B6C4990E26` |
| IOCTL trace | `86402E679792F908C04E73F601DAC53917CDBB4B695B0E3E0A107DA0989CB2EC` |
| Native target termination | `E13886B91580733CCD67FA58C7088C0D2D8171005D590D5562B1428BB03E9111` |
| CDB output | `2336B4937103824BE976AB3E0B1B683B7E022EC026A6E0B408522023855ED32A` |
| Operator observation | `F61F532835647158149D1470EEC31413E02704134342F41CD259D849A82CDE51` |
| Collector completion | `820D84AB9A0727F90BDCA306E7AD1694FB71E6E7FB7CC34CA1B9D06A3C34A558` |
| Passive post-run state | `6851D38BD54302228899FCDF5552B1A2042B943DCF690E7F43E1A8DD3A70D1D9` |

## Post-resume PCI decode snapshot

The owner confirmed physical isolation of the productive x86 disk and
authorized a bounded read-only comparison on the installed driver.
This continuation started at commit `746a19d143020a049339bf0bb6e94bd64e901051`.
The stable PCI instance was
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`.
Windows 10 x64 build 19045 reported boot at
`2026-11-27T19:00:54.7311200Z`. Power-Troubleshooter record 6655 gave
sleep at `2026-11-28T01:42:10.9274089Z` and wake at
`2026-11-28T11:12:04.1477941Z` (target and effective system state both
4, S3). The GUI resumed about 115 seconds after this wake. There was no
intervening reboot or later wake in this snapshot.

The passive snapshots at `12:06:40.3934779Z` and `12:06:41.4302768Z`
agreed on device OK, problem zero, observed BDF `5:1.0`, and the most
recent reported device state `1` (D0) from `CM_POWER_DATA`. The service
`LecS65AcqDrv` was running and the file at its loaded-image path matched
SHA-256 `6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75`.
The binding remained `oem109.inf` `0.2026.1126.1`. No matching
acquisition or diagnostic client was present before or after the check.

One invocation of the qualified `lecdiag.exe pci` started at
`12:06:40.5379545Z` and exited 0 at `12:06:40.6299685Z`. Tool SHA-256
was `374FC640A1DD8CFDC73FFE854DA988AA5ABFAB854D1F096EF5AD2F6747CA2CF0`.
The installed driver path uses the documented legacy
`HalGetBusDataByOffset` read via an existing private read-only IOCTL.
Microsoft documents this as a compatibility fallback for
[PCI configuration-space access](https://learn.microsoft.com/en-us/windows-hardware/drivers/pci/accessing-pci-device-configuration-space).
The handler returned `STATUS_SUCCESS`, with 64 configuration bytes
read and an 84-byte IOCTL response. The standard header returned
`1570:0005`, revision 0 and status `0x0208`. The Batch 31 comparator
used the same installed candidate and the same diagnostic executable.

| Observation | Batch 31 healthy PCI config | Post-resume PCI config | Current assigned resource |
| --- | --- | --- | --- |
| Command | `0x0006` | `0x0000` | n/a |
| Memory Space Enable / Bus Master Enable | Enabled / enabled | Disabled / disabled | n/a |
| BAR0 | `0xF7CBFE00` | `0x00000000` | `0xF7CBFE00–0xF7CBFFFF` (512 B) |
| BAR1 | `0xF7CC0000` | `0x00000000` | `0xF7CC0000–0xF7CFFFFF` (256 KiB) |
| BAR2 | `0xF7CBFC00` | `0x00000000` | `0xF7CBFC00–0xF7CBFDFF` (512 B) |
| Interrupt line / pin | 19 / 1 | 255 / 1 | IRQ 19 |

The healthy comparator is retained in the
[Batch 31 handoff](batch-31-package-qualification.md). The wrapper stopped
with exit 1 on the configuration mismatch. Memory decode was disabled
and the live BAR values did not match the OS resource list, so it
dispatched zero memory-register reads. There is no new per-register
NTSTATUS, returned byte count or value.

| Approved register | Historical healthy value | Current result |
| --- | --- | --- |
| BAR0+0x000 FVER | `0x00000002` | Not dispatched |
| BAR1+0x00C ACQFVER | `0x00000003` | Not dispatched |
| BAR2+0x040 ONEWIRE | `0x00000000` (idle sample) | Not dispatched |

The installed source's `0xCFDC21C0` five-byte request resolves each
selected BAR/offset and issues a single `READ_REGISTER_ULONG`. FVER
and ACQFVER are version registers, and the recovered x86 `0x16CA4`
uses a standalone read for ONEWIRE status. The table's historical values
come from the [startup probe reference](../x64-bringup.md#startup-probe-runtime-confirmation).
No MMIO write, Dallas command, DMA, XStream, driver restart, new boot or
latch change occurred in this follow-up.

This is a function-wide memory-decode failure at snapshot time. The
configuration transaction responding and the Windows-reported D0/OK
state can coexist with BARs that cannot decode the assigned addresses.
The earlier BAR2 all-ones failure is consistent with this condition.
The PCI header was not recorded during that call, so its exact state at
GUI startup, the sequence of IRPs at wake, and the cause of the lost
configuration remain unproven. A BAR2-only or Dallas-local fault cannot
be inferred from these data. See
[power/PnP lifetime](../power-pnp-dma-lifetime.md#pci-configuration-and-board-context-on-resume)
for the x86 quiesce/restore and native x64 power-policy differences.

**Next experiment.** With separate authorization, cold boot the same
Windows test installation once and collect the PCI header and resource
snapshot before any XStream or acquisition client starts. If memory
decoding is enabled and the BARs match the assigned resources, read the
three selected DWORDs once. A later, separately authorized sleep/resume
comparison against that healthy baseline can determine whether the
configuration loss recurs and when it becomes visible. Each phase should
stop at configuration mismatch. A cold boot recreates the FDO and may
refresh its one-admission count. It supplies no authorization for a GO.

Private evidence is in `Batch33-PostResume-BarRead-20261128-01` under
the local driver-backups root. The SHA-256 of `sha256-manifest.txt` is
`BFA86CE46E25CA8B0992E53289549813E6C522CE50A217AA2F5271C8C23182D7`.
The PCI stdout SHA-256 is
`0D102925712FF6BA9C278647FA00DC16F46596DAF716964A97F5E1091C7E6E3D`.
The manifest covers the passive snapshots and the single PCI query's
attempt, result, stdout and stderr.

## Prepared cold-boot and conditional GUI experiment

Preparation started from `2492fe2120ed24cb9158ba79f98a7a47b7106ffe`
on PR #58. Retain `oem109.inf`, version `0.2026.1126.1`, with SYS SHA-256
`6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75`.
The source-only Dallas correction remains undeployed. Offline file checks
matched the signed manifest and four artifacts, the capture launcher and
all six tool/script pins from the [executed GUI invocation](#executed-gui-invocation),
`lecdiag`, and optional x86 CDB. The two evidence roots below were absent.
Passive startup and running-process checks found no matching XStream
entry or process. Windows PowerShell 5.1.19041.6456 x64 parsed both new
command blocks. Offline checks passed all three register request
encodings and four 1-Wire status cases (idle with either DATA value,
busy, and all ones). None of these checks executed a capture or device
IOCTL.

### 1. Full shutdown and manual power-on

Obtain explicit approval `B33-COLD-BOOT-01` for one full shutdown and cold
boot of this installation and its one-shot read-only diagnostics. Before
shutdown, save open work, confirm the expected driver and absence of acquisition
clients, and record approval, current boot timestamp and latest
Kernel-Boot event 27 in the fresh private evidence root
`Batch33-ColdBoot-01` under the local driver-backups directory. After
recording the shutdown attempt, invoke once from the local x64 session:

```powershell
& (Join-Path $env:SystemRoot 'System32\shutdown.exe') /s /t 0
```

Wait for full power-off, then power on the scope locally and return to the
Windows x64 session. Microsoft documents `/s /t 0` as full shutdown and
`/hybrid` as the optional Fast Startup path. Leave Windows power settings
and startup tasks unchanged. The cold boot recreates the FDO and can
refresh the cumulative `MamLaunchLimit=1`. Record that boundary
explicitly. The counter value itself is not measured by the read-only
diagnostics. See [Microsoft full shutdown instructions](https://learn.microsoft.com/en-us/troubleshoot/windows-client/setup-upgrade-and-drivers/fast-startup-causes-system-hibernation-shutdown-fail).

### 2. PCI and three bounded reads

In elevated x64 Windows PowerShell 5.1, record the new CIM boot timestamp,
new Kernel-Boot event 27 with `BootType=0`, and latest Power-Troubleshooter
wake record. Stop if the boot is not new or sleep/resume intervened.
Verify the same stable PCI instance `1570:0005` from the
[post-resume snapshot](#post-resume-pci-decode-snapshot), expected bound
INF/version/SYS, running service, problem zero, reported D0
(`DEVPKEY_Device_PowerData`), and no acquisition client. Capture current
resources using `Win32_PNPAllocatedResource`. Record BDF, which can
change with enumeration.

Use the existing `scripts/native-process.ps1` runner (SHA-256
`F2D6C82AFB059457BAB3AE587DC41F811E8286D1201F6BE407F75C30C1DF0F5B`).
Run each row of the table as a separate bounded child, with the exact
array shown passed as `-Arguments`. From the verified clean repository
root, set `$name` and `$diagArgs` to the corresponding row and use:

```powershell
$repo = (Get-Location).Path
$evidence = Join-Path $env:USERPROFILE '.codex\driver-backups\Batch33-ColdBoot-01'
$diag = Join-Path $repo 'tools\lecdiag\build\lecdiag.exe'
$native = Join-Path $repo 'scripts\native-process.ps1'
if ((Get-FileHash -LiteralPath $diag).Hash -ne '374FC640A1DD8CFDC73FFE854DA988AA5ABFAB854D1F096EF5AD2F6747CA2CF0' -or
    (Get-FileHash -LiteralPath $native).Hash -ne 'F2D6C82AFB059457BAB3AE587DC41F811E8286D1201F6BE407F75C30C1DF0F5B') {
    throw 'Read-only diagnostic or process runner hash mismatch.'
}
. $native
$result = Invoke-LecS65NativeProcess -Name $name -FilePath $diag -Arguments $diagArgs -WorkingDirectory $repo -TimeoutSeconds 5 -StreamDrainTimeoutSeconds 2 -ProcessInfoPath (Join-Path $evidence "$name.process.txt") -LogPath (Join-Path $evidence "$name.log")
if ($result.StreamDrainTimedOut) { throw 'Diagnostic output is incomplete.' }
```

Persist the attempt marker before dispatch. Require the diagnostic to
show the expected interface for the stable instance. Record each result
and hash the passive snapshots and per-command logs.

| Name | Exact argument array | Requirement |
| --- | --- | --- |
| `pci` | `@('pci')` | 64 configuration bytes, 84-byte IOCTL reply, correct PCI identity |
| `bar0-fver` | `@('raw-ioctl','0xCFDC21C0','0000000000','4')` | `0x00000002` |
| `bar1-acqfver` | `@('raw-ioctl','0xCFDC21C0','010C000000','4')` | `0x00000003` |
| `bar2-onewire` | `@('raw-ioctl','0xCFDC21C0','0240000000','4')` | A value other than all ones, with BUSY clear |

After `pci`, preserve the raw Command, BAR0–BAR2, IRQ line/pin and INTx
disable fields. Require `(Command -band 0x0006) -eq 0x0006` and
three distinct, nonzero 32-bit memory BAR bases. Mask the low four BAR
attribute bits before comparing the bases against the actual three
Windows memory resource starts. Retain each range end and the Windows
IRQ allocation. Do not use old addresses or IRQ line 19 as hard-coded
gates. Stop before MMIO on invalid PCI state or uncertain mapping.

For each approved DWORD, require process exit 0 and an actual four-byte
reply. Decode the hex as a little-endian DWORD. The reviewed installed
handler returns `STATUS_SUCCESS` on this branch. `lecdiag` reports a
Win32 error on failure, so leave exact NTSTATUS unknown if the command
fails. For ONEWIRE, BUSY is bit 0 and sampled DATA is bit 1. An idle
read with either DATA value is acceptable. The historical idle snapshot
was `0x00000000`. An all-ones value or BUSY set stops progression without
a repeat read. No Dallas ROM-ID request is part of this read-only stage.

### 3. Authorized conditional GUI startup

Only after both prior stages pass, proceed directly if the owner has
also explicitly authorized the GUI stage and confirmed physical
isolation of the productive x86 disk. In the elevated x64 Windows
PowerShell 5.1 console, use the full
[GUI invocation](#executed-gui-invocation) with these substitutions
before invoking `& $entry @argsForGui` once:

| Parameter | Cold-boot experiment value |
| --- | --- |
| `$ApprovedHead` | Exact clean continuation commit reviewed by the owner for this run |
| `EvidenceRoot` | `Join-Path $backup 'Batch33-XStreamGui-ColdBoot-01'` |
| `AuthorizationId` | `B33-XSTREAM-GUI-COLD-01` |

Keep the same package and executable pins, capture readiness check,
240-second GUI limit and 420-second capture bound. This authorization
must cover temporary kernel capture, native private IOCTL tracing, one
normal GUI start and the possibility of one automatic MAM GO on the
recreated FDO. One previously qualified non-suspending CDB snapshot
on a confirmed stall is optional when included in that approval. Follow
the [existing identity and time-bound checks](#boundaries-and-operation).
The GUI invocation uses no observer or explicit COM activation. Leave
application settings unchanged and stop at the first error, timeout
or uncertain outcome. Preserve the private evidence and compare the
Dallas ROM-ID outcome, transport and calibration milestones,
`85FB/40/01`, MAM admission/denials, IRQ/DPC/completion and any visible
waveform. If the launch limit becomes the first functional blocker,
prioritize the separately qualified bounded repeated-MAM candidate.

The separate [power-management workstream](../power-pnp-dma-lifetime.md#separate-power-management-workstream)
remains open.

### Cold-boot and GUI result

The owner confirmed physical isolation of the productive x86 disk and
approved the combined experiment. The shutdown and GUI authorization IDs
were consumed once. The executed revision was
`8a44e34e5ff70a29f4da0babcb434902a7e1918d`. The installed
`oem109.inf`, version `0.2026.1126.1`, retained SYS SHA-256
`6596BF9F486DABE2A3D1CF36AE8906DB24663381D724B85CB42C34777F3D3D75`.

| UTC, 2026-11-28 | Evidence |
| --- | --- |
| `13:14:38.653` | Durable one-shot shutdown marker. `shutdown.exe /s /t 0` returned 0. |
| `13:15:05.500` | New Windows boot. Kernel-Boot record 6685 reported `BootType=0`. The latest wake event still belonged to the previous boot. |
| `13:18:39–13:18:40` | Elevated x64 Windows PowerShell 5.1.19041.6456 completed PCI and all three read-only queries. |
| `13:19:39.613` | Capture readiness recorded a confirmed kernel message before XStream dispatch. |
| `13:19:41.766` | Native launcher resumed verified GUI PID 3736, without application arguments or a COM observer. |
| `13:19:43.554` | `GET_DALLAS_ID` returned `STATUS_SUCCESS` and eight bytes. |
| `13:19:47.135` | The verified GUI title became `Teledyne LeCroy WR6200A`. |
| `13:20:37` | A private screenshot showed the oscilloscope UI, trigger `Stop` and an empty waveform grid. |
| `13:21:42.271` | The owner-approved close request was sent to the verified GUI window and accepted. |
| `13:21:48.627` | The native launcher observed target exit code 0, before its deadline and without forced termination. |

The first post-boot shell defaulted to the other checkout. The read-only
wrapper rejected its HEAD before recording a PCI attempt or invoking
any diagnostic. Its exception was retained. With the absolute approved
worktree selected, the first and only hardware query sequence passed.
There was one PCI-header query, one read for each selected DWORD, and no
repeat shutdown.

| Hardware evidence | Post-resume | After cold boot |
| --- | --- | --- |
| PCI function | `1570:0005` | `1570:0005`, same instance, observed BDF `5:1.0` |
| Command / Status | `0x0000 / 0x0208` | `0x0006 / 0x0208` |
| Memory decode / bus mastering | Disabled / disabled | Enabled / enabled |
| BAR0 | `0x00000000` | `0xF7CBFE00`, Windows range through `0xF7CBFFFF` |
| BAR1 | `0x00000000` | `0xF7CC0000`, Windows range through `0xF7CFFFFF` |
| BAR2 | `0x00000000` | `0xF7CBFC00`, Windows range through `0xF7CBFDFF` |
| Interrupt line / pin | `255 / 1` | `19 / 1`, Windows IRQ 19, INTx enabled |
| BAR0+0x000 FVER | No read dispatched | `0x00000002` |
| BAR1+0x00C ACQFVER | No read dispatched | `0x00000003` |
| BAR2+0x040 ONEWIRE | No read dispatched | `0x00000000`, BUSY clear |

The PCI query returned `STATUS_SUCCESS`, 64 configuration bytes and an
84-byte reply. Each register returned `STATUS_SUCCESS` and four bytes.
Problem code was zero and reported D0 and the running service agreed
with the passive checks. The cold boot recreated the FDO and could
refresh its MAM admission count. The test did not read or infer the
remaining admission count.

The native trace records the Dallas EEPROM read returning 512 bytes,
three successful event registrations (`CFDC2180`, `CFDC218C`,
`0x00223100`), and the early `CFDC2184` reply. Its 512 surviving WR6k
native completions all reported `STATUS_SUCCESS`. Of those, 482 were
`CFDC2110` with no recorded request or response truncation. The
family-0 `0x85` requests reached mask `0x02BF` at sequence 2326,
as in both known-good GUI references. No `85FB/40/01` query,
post-startup `0x0080` acknowledgement, second `CFDC2184`, family-2
opcode `0x03` or family-1 `0x51` was seen. Calibration completion was
unverified. The application showed trigger `Stop`, so the missing query
does not establish an interrupt, DPC or pending-bit defect.

The kernel capture contains no `CFDC2124`, `CFDC2138`, MAM admission,
limit denial, GO or matching IRQ/DPC completion or mapping retirement.
The surviving native trace likewise has zero MAM requests. There was
no observed waveform and no observed GO. The one-launch limit was
therefore not shown to block this run. No explicit acquisition or
trigger change, second launch, direct PCI configuration write, driver
replacement or additional reboot occurred. CDB was unnecessary for the
responsive GUI.

The native sidecar records successful parent flush and no termination
request, but no footer. The classifier therefore reports
`UNKNOWN_TERMINATION` and retains 3,817 records plus the header.
This limits trace completeness. It does not change the independently
observed accepted close request and exit status. The driver kernel
capture corroborates Dallas success and the absence of observed MAM.
The source writes the footer from the hook's `DLL_PROCESS_DETACH`
handler. The reason it is absent here is unverified.

The 420-second collector completed at `13:26:40.815Z` with exit 0,
closed its process tree and drained its streams. Both kernel outputs
retained 496 WR6k dispatch entries for PID 3736 and 494 success
completion lines, fewer than the 512 native completions. The count
difference prevents a claim of lossless kernel capture. The outer GUI
launcher exited 1 at `13:26:41.114Z` because the GUI runner rejected
the missing footer. It invoked capture and XStream once each.

Passive postflight at `13:27:12.192Z` found device OK, problem code
zero, the same SYS/package and a running service. No XStream,
DebugView or CDB process or DebugView driver service remained. The
GUI interval had no matching device/driver Kernel-PnP, Code Integrity,
WHEA, service, or application crash/hang event.

| Private evidence | SHA-256 |
| --- | --- |
| Cold-boot manifest | `EFAAA28D7C37DECAE5F791C925C7BF7E5C5A8E095C9E4120CA916EC618CBFE14` |
| GUI and dispatch manifest, 41 verified files | `F671C36336D7A31B3CEE7277BA33070DBDF85F6D9DB0B4C50F82FB6780BC626E` |
| Kernel raw log | `8DA53FF8B5C17C4D5B8469CDEBCF6E50BC2ED6AAA47EB50C5BE561BA3CF58AE9` |
| Kernel live stdout | `34858A92E90315BF32D6EADD22156C8BC1C713F2B0695BDCE0617EE8670BEF2B` |
| Native IOCTL trace | `C664F15B41DBBC8D6862C15945BFEC020B65C919053223C699A619B8C8034424` |
| Native completion sidecar | `30EF833FD83B17C664984BD75CD41D88B4E448A0D808C5C8F0E7755E5921A63B` |
| GUI screenshot | `BC2E5A80E6D916B61F874232826E69125B3ABA08F956E7ED68ADAB09ECB710DF` |
| Passive postflight | `D40F4A68266DAC68A325CC3885EDFC12D64A2D8024371621F666F3129D917208` |

The cold-boot baseline and Dallas recovery are established for this
run. Repeatable acquisition is still open. Before a separately
approved attempt that transitions the GUI out of Stop, review the
native footer path offline. The current absence of acquisition
traffic supplies no reason to raise the launch limit. The original
post-resume PCI failure remains a separate power-management issue.

## Repository state

- Starting commit: `f1bbd89dfa57fdac8b74641f859e443456c18ca3`.
- Development branch: `codex/phase2c-runtime-batch33`.
- Batch 32 parent pull request: #57, base `codex/phase2c-runtime-batch31`.
- Batch 33 pull request: #58, base `codex/phase2c-runtime-batch32`.
- Batch 33 contains no raw trace, opaque request/response data, vendor binary,
  or private system evidence.
