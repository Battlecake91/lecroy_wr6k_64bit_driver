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

Focused offline contracts pass: source/ABI 54/54, PnP/remove-lock 10/10,
publication 38/38, DMA completion 39/39, interleavings 2/2, DMA stress
5,000/5,000, INTEN 49/49 plus 1,000/1,000 stress, and ownership 17/17. No
reproducible source defect was identified and the signed package was not
rebuilt.

## Next boundary

No further PnP transition, reboot, Verifier activation, device handle, IOCTL,
XStream, acquisition or DMA operation is authorized. The smallest next gate is
one explicitly authorized, bounded R0 metadata-only IOCTL batch as specified in
[`../ioctl-runtime-safety.md`](../ioctl-runtime-safety.md). A single captured
PnP restart and targeted one-boot Driver Verifier are later independent gates.
Rollback or package replacement also requires separate authorization.
