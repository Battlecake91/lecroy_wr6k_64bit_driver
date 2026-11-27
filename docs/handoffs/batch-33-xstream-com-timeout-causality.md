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

## Next bounded experiment (not executed)

Use the currently installed `oem109.inf` candidate only after fresh owner
authorization and fresh confirmation that the productive x86 disk is
physically disconnected. First recheck device health, the loaded SYS identity,
active acquisition processes, and capture-tool identity passively. Preserve
the MAM one-shot latch as unknown; do not reload the driver or attempt to reset
it. Any approval must explicitly account for a possible automatic MAM GO from
XStream initialization, with the existing kernel limit of one.

For the single approved XStream startup, run the updated pinned
`XStreamWithCapture` chain once. Capture the COM observer's pre-call journal,
its success/exception record, native observer-process timeout metadata, target
process/thread snapshot at the separate observer bound, and native target
resume/deadline/termination/exit timeline. The 245-second observer bound must
expire before the 300-second target bound, leaving time to observe the target
before the native launcher terminates it. Keep full private IOCTL and kernel
capture; publish only hashes and redacted metadata. Do not invoke `Acquire()`
explicitly, retry, reload the driver, reset the MAM limit, or start another
acquisition. At completion, do not infer DMA from IOCTL success; require the
appropriate kernel GO/IRQ/DPC/completion evidence. No such experiment was
executed in Batch 33; it requires a new explicit authorization.

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

## Repository state

- Starting commit: `f1bbd89dfa57fdac8b74641f859e443456c18ca3`.
- Development branch: `codex/phase2c-runtime-batch33`.
- Batch 32 parent pull request: #57, base `codex/phase2c-runtime-batch31`.
- Batch 33 pull request: #58, base `codex/phase2c-runtime-batch32`.
- Batch 33 contains no raw trace, opaque request/response data, vendor binary,
  or private system evidence.
