# Batch 25 first XStream waveform preparation

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

## Remaining authorization boundary

No Batch 25 XStream process or DMA request has been started. Software sees only
the x64 NVMe `HFM256GDHTNG-8510B`, serial `ACE4_2E00_9590_A2AA.`, but that is
not proof of physical isolation. Before the live command is assembled against
a clean immutable launcher commit, the owner must state both:

1. the productive Windows x86 disk remains physically disconnected; and
2. exactly one potential MAM GO through the installed `oem107.inf` XStream path
   is authorized.

After that statement, use a fresh evidence root and the exact hashes above.
Never retry. Classify missing/uncertain completion as terminal for this loaded
FDO and perform only passive/offline analysis afterward.
