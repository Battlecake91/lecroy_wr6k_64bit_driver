# Batch 30 MAM status propagation

## Scope and result

Batch 30 resolved the Batch 28 discrepancy offline. It modified the pre-GO
status cleanup in `driver/DmaPnpStage.c` and added a regression against the
compiled production DMA/PnP stage. It did not start XStream, issue device
IOCTLs, install or sign a driver, reload the driver, or perform DMA/PnP
operations.

Root cause: the launch-limit callback correctly returned
`STATUS_DEVICE_BUSY`, but the `CancelAndRelease` path replaced that operation
status with the successful return from `LecSgSyncReleaseNoLaunch`. The
`CFDC2138` caller therefore saw success even though the request was rejected
before arm/interrupt-enable/GO. The dispatch then completed the IRP with that
success status. Because `prepareResult.GoPossible` was false,
`IoStatus.Information` remained zero and no output bytes were copied. This
reproduces the observed success/zero-information records exactly.

The fix retains the operation status when cleanup succeeds. A real cleanup
failure still takes precedence and enters the existing quarantine path. A
successful segment-copy operation that reports an invalid empty or oversized
segment count now returns `STATUS_INVALID_DEVICE_STATE` instead of accidentally
being turned into success by cleanup.

## Installed binary identity

The frozen candidate's package manifest identifies:

- INF `oem108.inf`, DriverVer `0.2026.1030.2`.
- SYS SHA-256 `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`.
- PDB SHA-256 `78D8D941C159C639787D5513B4AB727560EB173FB285CB6C75F119020552710B`.
- Manifest SHA-256 `F615EA1F0F31C8227D7592DECB2D106F4EBF874BA7104294238AA0827DD5D116`.
- Build source/workflow commit `414f18e6c4f8072b05e7d60d8ef2a5c1218ada42`.
- Driver tree `a323455df40cb506e5776bedd92c947d08042430` and include tree
  `eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`.
- Release x64, WDM-v3 MAM enabled, `MamLaunchLimit=1`, and
  `FirstMamExperimentOnly=false`.

The frozen SYS and PDB hashes were independently rechecked. `symchk` matched
the SYS CodeView RSDS GUID `{4478B1A5-48B7-4A95-8A6E-419FE19E24FF}`, age 1, to
the manifest's PDB with line information. The signed SYS contains the
`MAM_DMA_LIMIT` and `CFDC2138 WDM-v3 transaction` diagnostic strings and the
launch-limit build marker. The relevant source files at the manifest revision
are byte-identical to the reviewed source before the Batch 30 fix. No inference
from a newer unbuilt checkout was used to attribute the old behavior.

The installed binary therefore has the status-overwrite defect. The correction
in this branch is not signed or installed; `oem108.inf` remains unchanged.

## Status path

1. `Ioctl.c::LecValidateMamLaunchLimit` returns
   `STATUS_DEVICE_BUSY` when `LecMamLaunchLimitTryConsume` reports exhausted.
2. `DmaPnpStage.c::LecDmaPnpExecuteLegacyTransfer` calls the validation
   callback after WDM-v3 mapping, SG copy and descriptor preparation but before
   `LecDmaTransactionBeginArm` and `LecDmaTransactionCommitHardwareLaunch`.
3. The validation failure enters `CancelAndRelease`. The old code assigned
   `status = LecSgSyncReleaseNoLaunch(...)`; successful unmapping changed the
   request status to `STATUS_SUCCESS`. The mapping was released and there was
   no GO.
4. `Ioctl.c::LecIoctlAcquireBufferedOneChannel` initialized `*Information=0`
   and only writes the four-byte requested length when `GoPossible` is true.
   Pre-GO denial leaves information zero and the output buffer untouched.
5. The top-level IOCTL dispatch assigns its local `status` to
   `Irp->IoStatus.Status`, assigns `information` to
   `Irp->IoStatus.Information`, and completes the IRP. Thus the overwritten
   success reached the caller; this was not an unrelated Win32 status mapping.

The Batch 28 hook records the return from `NtDeviceIoControlFile`, pending
state, and `IO_STATUS_BLOCK.Information`; it does not separately record
`IO_STATUS_BLOCK.Status` or a Win32 `DeviceIoControl` BOOL/GetLastError result.
All 34 captured calls have `pending=false`, so they were completed synchronously
at the captured native call boundary. Their native return status is therefore
the observed caller-visible NTSTATUS. The distinct IOSB status field cannot be
independently recovered from the old trace. The kernel trace independently
records the same final success after status had been overwritten.

## Correlation of all 34 requests

All records are from PID 7580, TID 10416, one device handle (H1), with 15-byte
input and four-byte output capacity. Kernel log order and request order match:
the first entry was admitted and completed; the following 33 have an explicit
`MAM_DMA_LIMIT: admission denied limit=1` marker and a matching transaction
record with zero completion, released mapping, no quarantine, and final
success status caused by the status overwrite. `launchDenied=0` describes a
later launch-commit denial field; it does not negate these earlier validator
denials.

`Info/Out` is the `IO_STATUS_BLOCK.Information` value / output bytes captured
by the hook. The four-byte successful result is summarized, not reproduced as
raw payload. No opaque request/response bytes are included.

| Seq | UTC | Channel | Config | Bytes | Kernel admission/result | NTSTATUS | Pending | Info/Out |
| ---: | --- | ---: | --- | ---: | --- | --- | --- | --- |
| 2915 | 21:40:46.896729 | 1 | `0x00000000` | 3,072 | admitted; completion observed | `0x00000000` | false | 4/4; value 3,072 |
| 3845 | 21:40:52.197469 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 3848 | 21:40:52.205465 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 3886 | 21:40:52.237444 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4150 | 21:40:54.077312 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4151 | 21:40:54.083310 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4187 | 21:40:54.116289 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4451 | 21:40:55.210615 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4452 | 21:40:55.218611 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4488 | 21:40:55.245594 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4752 | 21:40:56.282958 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4753 | 21:40:56.288952 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 4789 | 21:40:56.322932 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5053 | 21:40:57.557387 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5054 | 21:40:57.565383 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5090 | 21:40:57.593365 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5354 | 21:40:58.570764 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5355 | 21:40:58.576760 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5391 | 21:40:58.606742 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5655 | 21:40:59.471210 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5656 | 21:40:59.477206 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5692 | 21:40:59.500192 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5956 | 21:41:00.340674 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5957 | 21:41:00.346672 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 5993 | 21:41:00.376653 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6257 | 21:41:01.226130 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6258 | 21:41:01.232128 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6294 | 21:41:01.252118 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6558 | 21:41:02.125577 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6559 | 21:41:02.134573 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6595 | 21:41:02.161556 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6859 | 21:41:03.295857 | 2 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6860 | 21:41:03.302853 | 50 | `0x00000C00` | 21,504 | denied pre-arm | `0x00000000` | false | 0/0 |
| 6896 | 21:41:03.336858 | 1 | `0x00000000` | 3,072 | denied pre-arm | `0x00000000` | false | 0/0 |

All requests returned in under 300 microseconds in the native trace, and all
were synchronous (`pending=false`). The six `CFDC2124` registrations were also
completed synchronously on the same PID/TID/handle, with 12-byte input,
four-byte output capacity, native success, `Information=4`, and four output
bytes captured. Registered lengths were 3,076; 1,028; 21,508 twice; and 61,444
twice. The trace does not retain `IO_STATUS_BLOCK.Status` separately.

## Original x86 comparison and RPC relationship

The artificial `MamLaunchLimit` gate is specific to the x64 qualification
build; there is no equivalent launch-limit-exhaustion case in the original
x86 path. The documented original successful `CFDC2138` path returns the
requested length after completion. It is therefore not valid to claim that
legacy behavior proves how an artificially denied request should be mapped.
For the x64 gate, `STATUS_DEVICE_BUSY` accurately communicates that the
request was not launched and avoids a false-success completion. The public
IOCTL number and packed request/output ABI are unchanged.

The 33 denials occurred after the COM dispatch marker and before the eventual
RPC-unavailable observer result. They could have affected application startup
because the old driver falsely reported success with no result, but ordering
alone does not prove a causal relationship. The RPC server lifetime issue
remains unresolved. No live opcode-`0x03` request or waveform was established.

## Offline qualification and current host

The new regression first failed against the old implementation (164/165); the
added `launch-limit denial survives successful pre-GO mapping cleanup` test
then passed against the corrected production `DmaPnpStage.c` compiled into the
host harness. After the fix, focused results were:

| Suite | Result |
| --- | ---: |
| WDM-v3 SG/PnP production stage, including status and resource retirement | 165/165 PASS |
| DMA transaction and production launch gate | 176/176 and 16/16 PASS |
| First-MAM/regular launch-limit gate | 21/21 PASS |
| DMA descriptor layout | 13/13 PASS |
| WDM-v3 transfer | 74/74 PASS |
| MDL-only registration | 15/15 PASS |
| Pre-GO side-effect contract | 11/11 PASS |
| Source/ABI contracts, including `IoStatus.Status`/`Information` propagation | 75/75 PASS |

Debug and Release x64 builds succeeded with MSVC `14.41.34120` and WDK
`10.0.28000.0`, both with default hardware launch disabled and with the
development WDM-v3/MAM limit 1 gate enabled. All generated SYS files were
unsigned and kept under the private temporary build root; none was packaged,
signed, staged, or installed.

| Build | SHA-256 |
| --- | --- |
| Debug, default-off | `866B3DEF4A32B79525EA435D07F8F02E57AE8E822C7C8DE6FB5E52255D1A4C61` |
| Release, default-off | `27E168377B39F5E512EF55D36737AC16329CA4905041D311D3A5DD2D870F38FD` |
| Debug, WDM-v3 MAM/limit 1 | `431FA629285E4CB2BAAAEBE8661F9923613E7DDE07F5561578AEA0B7FC4EBDF6` |
| Release, WDM-v3 MAM/limit 1 | `FCEEF8828A4CB998F83AF3CB863230169F8AD920D87482971AC8D901E97755AF` |

A passive snapshot on `DESKTOP-0Q0RKAQ` found the device `OK`, problem code 0,
bound to `oem108.inf` version `0.2026.1030.2`; the installed SYS hash remained
`2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`, and
service `LecS65AcqDrv` was running. No XStream or acquisition process was
present. The five-interface count was last verified in Batch 28 and was not
re-enumerated during Batch 30. No live test was performed.

## Next gate

The corrected source is not yet in the installed package. Prepare a newly
versioned unsigned candidate from the reviewed Batch 30 source, validate its
manifest and hashes, and request separate authorization for private-key use
before signing. Driver installation and a controlled XStream attempt each
remain separate gates. The first runtime check on the corrected candidate
should verify that one available admission is accepted and that the next
request is returned as `STATUS_DEVICE_BUSY` with `Information=0`, no output,
no second GO, and normal mapping retirement. Any subsequent XStream attempt
must receive fresh authorization and renewed physical x86-disk isolation
confirmation; `MamLaunchLimit=1` means it may cause one GO.
