# Batch 29 XStream RPC lifetime and capture integrity

## Batch 30 follow-up

Batch 30 later resolved the status discrepancy described in this historical
analysis: successful `LecSgSyncReleaseNoLaunch` cleanup overwrote the earlier
launch-limit `STATUS_DEVICE_BUSY` result. The source correction is offline-only
and the installed `oem108.inf` binary remains affected. See the
[Batch 30 status-propagation handoff](batch-30-mam-status-propagation.md).

## Scope and safety

This batch analyzed retained Batch 28 evidence and modified only offline
diagnostic tooling, tests, and documentation. It did not start XStream, access
the device, issue IOCTLs, change the installed driver, or perform a PnP or
hardware operation. The Batch 28 one-shot authorization remains consumed.

## Batch 28 execution and COM result

The approved Batch 28 execution used installed `oem108.inf`, version
`0.2026.1030.2`, SYS SHA-256
`2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`. There
was one prelaunched XStream process (PID 7580), one COM activation attempt,
zero explicit `Acquire()` calls, and no retry. PowerShell's apartment was STA.
`New-Object -ComObject 'LeCroy.XStreamDSO'` returned, but resolving
`Acquisition` encountered RPC server unavailable (`0x800706BA`). This is
evidence that a later automation/RPC call failed; the root proxy's existence
does not show that the out-of-process server remained available. STA use rules
out neither server termination nor a separate RPC lifetime fault. No matching
Application event, XStream WER report, or dump was found. The exact cause and
time of the first RPC failure remain unknown because the old observer retried
lookups until its timeout and only recorded the eventual exception.

The private trace has 7,160 JSONL records, including 2,130 IOCTL API records,
1,098 `CFDC2110`, six `CFDC2124`, and 34 `CFDC2138` records. It ends at
`2026-11-06T21:41:03.3368581Z` without a footer. There is no family-2 opcode
`0x03` request in the surviving prefix; whether one occurred after the final
record is unknown. Therefore the 130-byte request/14-byte response and the
original `STATUS_INVALID_DEVICE_REQUEST` behavior remain unqualified. Raw
request and response bytes remain private.

The trace launcher reported target exit code 0 and the outer result says the
target process was no longer present. However, the trace has no footer and the
old launcher did not persist parent-owned finalization data. Thus there is no
proof of complete capture, but also no evidence that the target crashed. The
missing footer cannot be attributed conclusively to a process crash, hook
detachment, or write failure.

### Reconstructed timeline

All displayed trace times are UTC. Events marked *inferred* follow source
ordering or later process state; they are not directly timestamped events.

| Time | Event | Evidence / limit |
| --- | --- | --- |
| 21:39:10.098 | DebugView process starts | `capture-process.txt` |
| 21:39:10.344 | Independent kernel message first seen | DebugView readiness record |
| 21:39:11.006 | Kernel capture reports ready | Kernel/verbose enabled, initialized, flushed, confirmed message |
| After readiness; exact time absent | XStream PID 7580 created suspended, hook injected/initialized, ready marker written, then process resumed | Native launcher source establishes order; marker has PID/embedding only, no timestamp |
| 21:39:15.970 | First captured `CFDC2110` API record | Trace sequence 181, TID 8884; PID 7580 from trace header |
| 21:39:17.679 | XStream dispatch marker written immediately before COM activation | Marker time; actual `New-Object` call and root-object return were not individually timestamped |
| After dispatch; exact time absent | Root COM object returned; first `Acquisition` property lookup began | Prior observer did not persist per-call timestamps |
| 21:40:46.897 | First `CFDC2124` and `CFDC2138` calls | Trace sequence 2914–2916; one CFDC2138 completion record follows |
| 21:40:52.196–21:41:03.337 | Remaining 33 `CFDC2138` calls and four `CFDC2124` registrations | Trace record timestamps; last retained record at 21:41:03.3368581Z |
| 21:43:37.798 | Observer records failure after its lookup timeout | Result timestamp, not the time of the first RPC error because calls were retried |
| 21:43:38.103 | Lossless orchestrator records observer exit 1 and target no longer remaining | Result JSON. Failure path requests `CloseMainWindow`; close-request and target-exit times were not individually recorded |
| 21:43:38.253 | Outer launcher records failure and no retry | Outer result; trace launcher separately reports target exit code 0 |
| About 21:44:11 | 300-second capture stops | Capture duration/log end; capture outlived XStream |
| About 21:49:38 | Final passive device/process inspection | Existing post-run summary; no device operation |

Process creation/resume, hook initialization and readiness are ordered by the
native launcher's source, but their individual timestamps are missing. The
trace has no process-exit event. The parent `CloseMainWindow` request is
source-supported; its exact time is unavailable. The target's reported clean
exit and disappearance do not prove why the earlier COM call failed.

### `CFDC2124` / `CFDC2138` correlation

The trace header ties records to PID 7580. All 40 calls were on TID 10416 and
one device handle (anonymized here as H1); all were synchronous native IOCTLs
with NTSTATUS `0x00000000`. The six `CFDC2124` registrations each had a 12-byte
input, four-byte output capacity, four output bytes captured and
`IO_STATUS_BLOCK.Information=4`:

| Trace sequence | UTC | Registered byte length |
| --- | --- | ---: |
| 2914 | 21:40:46.896730 | 3,076 |
| 2916 | 21:40:46.896730 | 1,028 |
| 3843–3844 | 21:40:52.196471–21:40:52.197469 | 21,508 each |
| 3846–3847 | 21:40:52.205465 | 61,444 each |

Each `CFDC2138` call had a 15-byte input and four-byte output capacity. The
first (sequence 2915 at 21:40:46.896730) requested channel 1, configuration 0,
and 3,072 bytes. It captured a four-byte return, `Information=4`, and success;
the return decodes to a 3,072-byte completion count. The remaining 33 calls
captured zero output bytes and `Information=0`, despite success status. Their
request shapes group as:

| Count | Channel | Configuration | Requested bytes | Time span |
| ---: | ---: | ---: | ---: | --- |
| 11 | 2 | `0x00000C00` | 21,504 | 21:40:52.197–21:41:03.296 |
| 11 | `0x32` | `0x00000C00` | 21,504 | 21:40:52.205–21:41:03.303 |
| 11 | 1 | `0x00000000` | 3,072 | 21:40:52.237–21:41:03.337 |

The first CFDC2138 call followed the COM dispatch marker by about 89 seconds.
The remaining calls occurred before the last trace record; the observer's
eventual failure result followed about 156 seconds later. The first actual COM
property access and first RPC failure were not timestamped, so this is ordering
against the pre-invocation marker and eventual timeout only. A CFDC2138 request
shows submission to the driver path; only the first matching kernel completion
marker proves normal completion. Matching kernel `MAM_DMA_LIMIT` markers
establish that the other 33 calls were rejected before GO, as detailed below.

## Kernel transaction evidence correction

The retained kernel log contains one `CFDC2138` transaction with 3,072
requested bytes, two mapped DMA-logical segments, three descriptors,
`completed=1`, `released=1`, `quarantined=0`, and status success. The installed
package manifest identifies its source as `414f18e6c4f8072b05e7d60d8ef2a5c1218ada42`;
the relevant `DmaPnpStage.c` and `Ioctl.c` match the inspected source at that
revision. In that source:

1. `LecExecuteWdmMamTransfer` calls `LecLaunchSelectedDma`.
2. `LecCommitDmaLaunchSynchronized` writes the MAMRGO register and sets
   `GoWritten`; `GoCommitted` follows that result.
3. The transfer waits on its completion event; `CompletionObserved` is set
   only after a successful wait and non-uncertain cleanup.
4. `LecDmaPnpExecuteLegacyTransfer` reports successful completion only when
   GO was committed, completion was observed, and normal mapping retirement
   succeeded.
5. The ISR/DPC path signals the selected transfer's event for INTST bit 0.

This is source-corroborated evidence for one MAMRGO register write, matching
interrupt/DPC completion signaling, and normal mapping retirement for this
single transfer. For a 3,072-byte request, the written launch-count value is
three; the code performs one write to the MAMRGO register. The kernel log does
not separately record the raw register write or IRQ/DPC timestamps, so this is
not independent bus-analyzer evidence. It is not proof of waveform content.

The kernel capture has exactly 34 launch-limit records: one
`admission=1 limit=1` and 33 `admission denied limit=1`. The latter correspond
in sequence to the 33 post-completion `CFDC2138` requests and prove those
requests were denied before the source's arm/interrupt-enable/GO stages. Their
matching transaction markers show released mappings and `completed=0`. Their
`launchDenied=0` field does not contradict the denial: it is set only for the
later `LecDmaTxLaunchDisabled` commit result, while these requests fail the
earlier validation callback.

The source returns `STATUS_DEVICE_BUSY` on launch-limit exhaustion, but all 33
user-mode records and their kernel transaction markers report
`0x00000000`. At the time of this analysis, the discrepancy was unresolved;
Batch 30 later proved that successful no-launch cleanup overwrote the earlier
launch-limit status. Launch state is determinate from the
explicit denial messages: none of these 33 reached GO. No second MAMRGO write,
IRQ or DPC completion is supported by this capture.

## Offline corrections

- `scripts/xstream-com-lifecycle.ps1` centralizes RPC-server-unavailable
  detection by HRESULT, including inner exceptions. The waveform observer now
  immediately propagates this failure instead of repeating COM calls through
  the timeout; ordinary object lookup errors retain the existing fallback.
- The observer now requires STA for authorized COM execution and records
  observer start, apartment state, COM root activation start/end, first
  `Acquisition` lookup, and RPC failure timestamps. These timestamps improve
  the next capture but do not retroactively establish the prior activation's
  exact timing.
- The native trace launcher writes a unique sidecar only after target exit.
  It records target PID/exit code, whether termination was launcher-induced,
  footer state, and parent flush result. It uses create-new/atomic publication
  and returns failure when finalization cannot prove a normal footer and flush.
- The validator distinguishes complete normal capture, bounded
  launcher-terminated partial capture, corrupt/truncated data, and unknown
  termination. A process exit code by itself is not crash evidence. Normal
  opcode-`0x03` validation requires matching parent metadata, footer, PID, and
  successful parent flush.
- The production tracer still builds without test hooks. A separate
  `build\x86-test` target enables synthetic success-response and flush-failure
  hooks only under `-TestMode`.
- The orchestrator invokes the explicit Windows PowerShell 5.1 executable and
  sets the child `PSModulePath`, avoiding reliance on an embedded host's
  `$PSHOME` for `powershell.exe` or standard modules.

## Offline validation

All focused tests ran on Windows PowerShell 5.1, x64, without hardware access:

| Suite | Result |
| --- | ---: |
| Lossless capture and native launcher, including complete request/response, footer, timeout, termination, flush-failure, malformed trace, and production-orchestrator cases | 28/28 PASS |
| COM lifecycle / RPC error classification | 8/8 PASS |
| Single-waveform observer mocks and fail-closed preflight cases | 13/13 PASS |

The synthetic successful IOCTL path checks the 130-byte request and all 14
returned bytes without using the LeCroy device. The exact in-process hook and
native parent launcher were exercised. C4152 warnings for existing hook
function-pointer casts remain; both test binaries build and complete the
tests. The mocks do not reproduce an actual COM local-server crash, RPC
marshaling, Windows service recovery, or real XStream shutdown behavior.

The production x86 tracer was built separately without `-TestMode`. Its
private local build artifacts were:

| Artifact | SHA-256 |
| --- | --- |
| `xstream_trace_launcher.exe` | `AD14613C962FB9E4D86096143CE8FF6CC2D0AFBD3EFBB0C807A5AAAA0F058BD9` |
| `xstream_io_hook.dll` | `4C4ED97F56598900BEBACF9686EF3220ED730BB6B9E593FB2191AE4FCA50A1F3` |

The offline test build is separate and must never be substituted for these
production binaries. `C4152` function-pointer-cast warnings remain in the
existing hook code; they did not prevent either build or the synthetic tests.

## Evidence and current state

Raw private evidence remains outside the repository under
`C:\Users\LeCroyUser\.codex\driver-backups\Batch28-XStreamWithCapture-20261106-attempt2`.
Evidence hashes:

| Evidence | SHA-256 |
| --- | --- |
| Private user-mode JSONL trace | `CAC96BA72349C13C7D928A7042F608523E72BD74CB5B4157912BB8B27EE45848` |
| Kernel capture | `47A0F528C1EC5B24C00E414DAEFAD863D4B80416F215BCCD32BF81FEE10AC868` |
| Final evidence inventory | `1C360F4DE289AF40C67F18FC71F9E001686B127261075844B168C572E2BA4EFD` |
| Original attempt summary | `5D4FA76CCA94266BC05F9631317BD2C01E75E16E1E7CD8CC30D467937ABE2629` |

The Batch 28 post-check recorded five published interface classes. A new
read-only metadata snapshot on `DESKTOP-0Q0RKAQ` at
`2026-11-25T00:46:40Z` observed the stable PCI instance present as `OK`, bound
to `oem108.inf` version `0.2026.1030.2`; service `LecS65AcqDrv` was running,
and the on-disk SYS SHA-256 remained the approved
`2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`. No
XStream/LeCroy/acquisition process was found. This snapshot did not enumerate
interfaces; the five-interface result is from the retained Batch 28 check.
No driver, PnP or hardware operation occurred.

The current OS boot time is `2026-11-25T00:47:29.4988160+01:00`, later than
the Batch 28 attempt. The installed driver initializes its FDO-local MAM
admission count when the FDO is created, so the previous attempt's consumed
count cannot be assumed to persist across this boot. With the same installed
binary and `MamLaunchLimit=1`, a future XStream startup may admit one MAM GO.
The Batch 28 authorization remains consumed; any later attempt requires fresh
authorization explicitly covering at most one GO and renewed confirmation of
physical x86-disk isolation. No test was run to inspect or alter the current
admission counter.

A derived JSON report with metadata only (no opaque IOCTL bytes) is retained
outside the repository at
`C:\Users\LeCroyUser\AppData\Local\Temp\lecroy-batch29-analysis-68095c1d852d4b56b0a128bda32ffcba\derived-analysis-verified.json`,
SHA-256 `8A6454760314D653B3FF02E4E74F41A2877258F51CD3BC7B3130F18F03BF9428`.

## Next bounded step

Do not repeat XStream under the consumed Batch 28 authorization. Before any
later attempt, resolve the 33-record kernel status inconsistency from a
verified runtime/binary identity and improve the diagnostic correlation of
launch, IRQ, DPC and final status. The corrected observer should perform one
COM activation, make one fail-fast `Acquisition` lookup path with timestamps,
and let the capture parent durably classify the trace. Require fresh owner
authorization and renewed physical x86-disk isolation confirmation. Preserve
`MamLaunchLimit=1`; do not reload or reset it. Because Windows booted after
Batch 28, the FDO-local admission count may be fresh and can admit one GO.
Treat the next run as a possible physical DMA launch, not as a COM-only probe.
No waveform, opcode-`0x03` compatibility, repeated acquisition,
timeout/cancellation, or full RPC-server lifetime qualification is
established by this batch.

### Pinned identity for a future, separately authorized run

The following hashes were read-only verified on this host after the offline
changes. A future invocation must run from the exact committed Batch 29 HEAD;
its `ExpectedLauncherCommit` must equal that HEAD. It must use a new evidence
root and authorization ID, and the caller must again confirm physical disk
isolation. No command below has been executed as a live XStream attempt.

| Component | SHA-256 |
| --- | --- |
| Outer `invoke-diagnostic-capture-install.ps1` | `8A7D86E5E6AFD1D4322CB204DDEBC42896919E537AAB762B7994CEE9F27891D2` |
| `invoke-xstream-lossless-capture.ps1` | `FEB38B4BE53BE4FD4CA8705A69ED2975BBCBA2BDDF25A7834AB95DCB609F03D4` |
| `invoke-xstream-single-waveform.ps1` | `56D41E3F16109746FCFEB848AB31760B1287F9E8855FB069D09E2C9BCF61678D` |
| `xstream-com-lifecycle.ps1` | `F3C646DC8E1E1FD06C859BC58EBE58BF2815FAF490A6309C2D95FDEEBE684F0A` |
| `validate-xstream-ioctl-capture.ps1` | `CDED466A4DD077DE93FB74E42B091B0156D0A6410CE333BC8C434704ED3F6670` |
| Production `xstream_trace_launcher.exe` | `AD14613C962FB9E4D86096143CE8FF6CC2D0AFBD3EFBB0C807A5AAAA0F058BD9` |
| Production `xstream_io_hook.dll` | `4C4ED97F56598900BEBACF9686EF3220ED730BB6B9E593FB2191AE4FCA50A1F3` |
| Installed `lecroyxstreamdso.exe` | `27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F` |
| DebugView 5.02 CLI | `7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B` |

The driver package identity remains `oem108.inf`, version
`0.2026.1030.2`, SYS `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`,
manifest `F615EA1F0F31C8227D7592DECB2D106F4EBF874BA7104294238AA0827DD5D116`,
INF `BC7734C13F276A23ACD18B3CA004267B508F5DE9AD734F426E947AA051976CF4`,
CAT `28BFF32CC2579CCC920991E4C36CA2007D04F9F9F1D255E1EEB4350924DA5617`,
and PDB `78D8D941C159C639787D5513B4AB727560EB173FB285CB6C75F119020552710B`.
The source/workflow commit is `414f18e6c4f8072b05e7d60d8ef2a5c1218ada42`,
driver tree `a323455df40cb506e5776bedd92c947d08042430`, and include tree
`eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`. The package uses the approved
local test certificate `4296FA2E355613DF0D5CE6F24D0898E00B706472`, with
WDM-v3 MAM enabled, first-experiment mode disabled, and
`MamLaunchLimit=1`.

The future operation is the outer launcher's `XStreamWithCapture` mode. Its
required command identity includes `-AuthorizeKernelCapture`, `-AuthorizeXStream`,
`-ConfirmPhysicalDiskDisconnected`, the pinned package and component hashes
above, a new `-AuthorizationId`, a new `-EvidenceRoot`, and
`-ExpectedLauncherCommit` equal to the exact Batch 29 HEAD. The live invocation
is intentionally not issued or marked authorized here; the owner must provide
fresh approval after reviewing the one-GO possibility.
