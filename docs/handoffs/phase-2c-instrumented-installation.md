# Phase 2C instrumented-installation handoff

Kernel Runtime Qualification Batch 1 completed the separately authorized
instrumented installation on 2026-10-19. This handoff records only the current
transfer state; detailed evidence and analysis are canonical in
[`../phase-2c-kernel-trace-capture.md`](../phase-2c-kernel-trace-capture.md) and
[`../kernel-runtime-qualification.md`](../kernel-runtime-qualification.md).

## Current runtime state

- bound INF: `oem101.inf`;
- driver version: `0.2026.1018.1`;
- loaded SYS SHA-256:
  `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`;
- device status: `OK`, problem code zero;
- service: `LecS65AcqDrv`, running;
- interfaces: exactly one enabled target path in each of the five legacy
  classes;
- acquisition clients: not running;
- DebugView/`DBGV`: absent after the completed capture.

The private evidence root is
`C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-KernelRuntimeBatch1-InstallWithCapture-20261019-attempt1`.
Do not commit its raw traces or host-specific material to this public
repository. Its final evidence-manifest SHA-256 is
`C50F5578F5FF822BB0A1A7D15084A956A18ABDBD21407BC861DAF3339BE6BA8F`.

## Established result

The launcher ran once, reported `PASS`, and recorded one capture invocation and
one PnPUtil invocation. The raw kernel log SHA-256 is
`D14DAC42D6CEBED65313DE8A265E10B80D52CB57A33C0F79A888F625257948A4`.
The new START's lower, local, interrupt-connect, all five interface-enable and
final statuses were successful.

Event 411 at `2026-10-19T16:53:33.8741035Z` occurred after the old driver
completed REMOVE/unload and before the new driver's first entry/dispatch. It is
a verified installation/PnP intermediate state, not a failure returned by the
new driver's START. The exact internal Windows/PnP source of status
`0xC00000BB` remains indeterminate.

Kernel Runtime Qualification Batch 2 passed its first four R0 commands once.
Batch 2B then attempted command 5 once with the corrected runner. `lecdiag`
opened the approved interface but rejected its explicit zero-length hex input
and exited 2 before `DeviceIoControl`. Commands 6–9 and R1 were skipped; no
retry occurred. The exact interval had no System, Kernel-PnP or Code Integrity
event, and the post-stop state remains identical to the current runtime state
above.

The utility parser now accepts zero-length raw input offline. Rebuilt x64
`lecdiag.exe` SHA-256 is
`ADD4A869C05DD85A55DD9308927D2AE946907F26C0D0054E46A54738441FB7BA`;
it has not issued a live request. Focused contracts pass: source/safety 59/59,
event/interface lifetime audit 6/6, PnP/remove-lock 10/10, publication 38/38
and ownership 17/17. Driver sources and the installed package were not changed.

Source review also established two deferred lifetime gaps: CLOSE releases all
DMA registrations for the process on every handle close rather than a proven
last-process-handle boundary, and partial interface-enable failure does not
fail START. Neither affected the R0 run, and neither has been deployed or
changed in this workstream.

## Next boundary

No further PnP transition, reboot, Verifier activation, device handle, IOCTL,
XStream, acquisition or DMA operation is authorized. The smallest next gate is
one explicit authorization for only R0 commands 5–9 using the rebuilt utility
hash above, followed conditionally by the two R1 reads. The four passed commands
must not be repeated merely to rebuild one transcript. A single captured PnP
restart and targeted one-boot Verifier configuration remain later independent
gates. Rollback or package replacement also requires separate authorization.
