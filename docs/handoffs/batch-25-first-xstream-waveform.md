# Batch 25 first XStream attempt

## Prepared identity

- Installed package: `oem107.inf`, version `0.2026.1030.1`
- Loaded SYS SHA-256:
  `184471C04BCE4688D3EE6EAC01049606F7878D9A0568419F0A08516282F35CAE`
- Package-manifest SHA-256:
  `F3D16A27BDB1F137E47942C38F4A959A503AD1F12771F57542BE29081C31BC82`
- Batch 25 launcher implementation commit:
  `fe49b88352317f3b4cee103fde14eddb7523ec2c`
- Diagnostic launcher SHA-256:
  `E9B6C4FBCF5FDCD4302846210E46928CF36F313271C2491E3860D3C9E96667D1`
- Single-waveform runner SHA-256:
  `859920676C5A1A23BF5B6479E00FBE76CF8ED2062FF23939A93B18D94148BB58`
- XStream DSO 8.5.1.1 local-server SHA-256:
  `27664EBDB356BB11740312AA68FAF38FB2739D7B4E6421C0A4ADA9FF0C3D0A4F`
- DebugView CLI 5.02 SHA-256:
  `7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B`

The passive runtime baseline passed with problem code zero, a running service,
all five interfaces and no acquisition/capture process. Its private JSON has
SHA-256 `0BC9CB921DC0226791E86FCF5240415C6B281DB29F94674F1530048859853CB3`.

## Execution design

`XStreamWithCapture` starts the pinned DebugView collector, requires a genuine
kernel readiness record, writes an exclusive XStream attempt marker and then
invokes the pinned observer once. The observer creates `LeCroy.XStreamDSO`
once, never invokes `Acquire()`, changes no CVar, and polls only the existing C1
result. It records configuration, counts, finite-value statistics and a payload
hash without publishing raw values. On success it requests an orderly window
close; it never force-kills the application.

The installed kernel permits one MAM launch admission. Automatic later XStream
requests receive `STATUS_DEVICE_BUSY` before acquisition MMIO. No driver reload,
PnP transition, retry or second application launch is allowed.

Offline results: single-waveform 9/9, launcher 44/44, launch gate 21/21 and
source/ABI contracts 73/73. The 44-case launcher run used the explicit
test-only non-elevated override; the real mode still requires elevated 64-bit
Windows PowerShell 5.1.

## Live result

The owner confirmed the productive x86 disk was physically disconnected and
authorized exactly one `XStreamWithCapture` run. The launcher established a
real kernel-capture message, persisted the attempt marker and dispatched one
COM activation. The runner made no explicit `Acquire()` call and did not retry.

The XStream process reached driver control initialization but did not register
its COM class. Activation failed with `CO_E_SERVER_EXEC_FAILURE` and Windows
recorded DCOM event 10010. The trace contains 511 IOCTL requests over four
handles. Six identical family-2 opcode-`0x03` `CFDC2110` chains were rejected
with `STATUS_INVALID_DEVICE_REQUEST`; every other captured IOCTL completed at
NTSTATUS success. The missing opcode is already identified by static original
driver analysis as a common-transport send with an extended response timeout.
This is the leading blocker, although the trace does not prove sole causality.

No `CFDC2124`, `CFDC2138`, MAM mapping, descriptor, GO, IRQ, DPC, completion or
waveform occurred. There was no DMA resource to retire or quarantine, and the
kernel launch admission remains unused. Post-state is healthy with the exact
SYS hash, problem code zero, running service and five interfaces. DebugView,
XStream and the temporary `DBGV` service are gone. No Kernel-PnP, Code Integrity
or WHEA error was observed.

Private evidence hashes:

- kernel capture:
  `DA4B0DC8066BC645658557728F4FA8F76DEE1AA8827AEA3D2010B51A5CE003B9`
- launcher result:
  `58072D5A1D0CA435C1F3193B9B9DB37A7DD7A66929F96AD2220593D34DE89958`
- XStream result:
  `B3856361D890E8B743210F15B5158797332B0589CCC18C38077542D30CD72F1E`
- post-state JSON:
  `24CC773D634F062537904E9ACA715E4FD19E8F18B5651ACB6C2454A0420F67EE`

Do not retry this candidate. The next workstream is offline opcode-`0x03`
contract recovery, implementation and qualification, followed by a new signed
candidate and a fresh authorization boundary.
