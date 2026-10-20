# Phase 2C instrumented-installation handoff

Kernel Runtime Qualification Batch 1 completed the separately authorized
instrumented installation on 2026-10-19. This handoff records only the current
transfer state; detailed evidence and analysis are canonical in
[`../phase-2c-kernel-trace-capture.md`](../phase-2c-kernel-trace-capture.md) and
[`../kernel-runtime-qualification.md`](../kernel-runtime-qualification.md).

## Current runtime state

- bound INF: `oem101.inf`;
- driver version: `0.2026.1018.1`;
- installed SYS SHA-256:
  `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`;
- device status: `Error`, problem code 52 after an intervening host reboot;
- service: `LecS65AcqDrv`, stopped;
- interfaces: all five expected instances exist but are disabled;
- current PCI location: 5:1.0, not the previously approved 4:1.0 baseline;
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
event/interface lifetime audit 19/19, PnP/remove-lock 10/10, publication 38/38
and ownership 17/17. Driver sources and the installed package were not changed.

Source review also established two deferred lifetime gaps: CLOSE releases all
DMA registrations for the process on every handle close rather than a proven
last-process-handle boundary, and partial interface-enable failure does not
fail START. Neither affected the R0 run, and neither has been deployed or
changed in this workstream.

## Batch 2C stop and next boundary

The complete offline command chain passed 16/16 with a mocked device boundary.
The subsequent passive preflight found Code Integrity event 3004 and
Kernel-PnP event 219 immediately after the intervening boot. It stopped before
opening a device handle, so R0-5 through R0-9 and R1 are NOT RUN. No retry,
repair or PnP transition occurred. The exact boot-policy cause is not proven.
Private Batch 2C evidence is retained at
`C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-KernelRuntimeBatch2C-R0R1-20261020-attempt1`;
raw host logs remain outside this public repository. The result-record SHA-256
is `14E0B58920F01B92EBFF7597BBF5BE423DB47BD4F953233C5772D98DAE974248`
and the final evidence-manifest SHA-256 is
`AD1732D26E8D8C114A165FD7E661C8B0D3C5A4EDC8A76451940A220DCF4F722A`.

No PnP transition, reboot, boot-policy change, driver load, Verifier activation,
device handle, IOCTL, XStream, acquisition or DMA operation is authorized.
Restoring a healthy approved baseline is now the smallest separate gate. A
later runtime authorization must again bind only R0 commands 5–9 to the rebuilt
utility hash above and may conditionally name the two R1 reads. The four
earlier passing commands must not be repeated merely to rebuild one transcript.
