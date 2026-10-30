# Batch 26 CFDC2110 family-2 opcode 0x03

## Recovered contract

The six Batch 25 calls share the same outer shape: 130 input bytes and 14 output
bytes. The first record has output 6, payload 112, type 3, signature A5FB and
payload prefix `40 02 03`. The 109-byte body is opaque to the original driver.
The companion record begins at offset 120 and has output 8, payload 2, type 3,
signature 85FB and payload `40 00`.

Original `FUN_000166A8` dispatches opcode `0x03` to the common message sender.
The historical `0x19` argument only selects response-pending state. Opcode
`0x03` separately changes the next response wait from five to ten seconds. The
initial send is 114 bytes from `record + 6`; no DMA or acquisition path is
involved. The fetch restores the default timeout before waiting. Timeout is
reported in the response as DWORD `0x102`, WORD length 2, WORD protocol `0x20`.

## Native implementation

`CfDc2110Protocol.c` is a hardware-independent exact-shape validator used by
the actual whitelist and dispatcher. `LecIoctlCfDc2110` reuses the established
BAR1 transport and `TransferMutex`, binds the ten-second wait only to the
companion fetch, keeps later chunks at five seconds and frames transport
failures without returning a fabricated successful payload. The default public
ABI and all DMA/MAM/MTT gates are unchanged.

The original has no request-specific cancel check after transport dispatch.
The x64 path remains synchronous and creates no queued IRP, borrowed user
pointer or asynchronous ownership. Broader cancellable transport redesign was
not introduced in this focused compatibility patch.

## Offline evidence

- focused opcode contract: 20/20
- source and ABI contracts: 74/74
- Debug x64 rebuild: 0 warnings, 0 errors
- Release x64 rebuild: 0 warnings, 0 errors
- complete Dry suite under Windows PowerShell 5.1: PASS
- default DMA stress: 5,000/5,000
- default INTEN stress: 1,000/1,000
- toolchain: SDK/WDK 10.0.28000.0, MSVC 14.41.34120

No device handle, XStream process, installation, PnP transition, DMA operation
or hardware request was used.

## Remaining runtime gate

The full private 130-byte request was not retained in the public repository;
only its prefix and structure are available. Do not fabricate the opaque body.
After a signed candidate is separately installed, the smallest live proof is
one captured exact opcode request with a ten-second maximum response wait and
post-request health inspection. A later XStream activation remains a separate
authorization boundary and must retain the one-launch kernel limit.

## Signed candidate

One authorized signing run completed without retry. The immutable candidate is
stored outside the repository at
`C:\Users\LeCroyUser\.codex\driver-backups\Batch26-SignedCandidate-20261030-1030-2`.

- DriverVer: `10/30/2026,0.2026.1030.2`
- source and workflow commit: `414f18e6c4f8072b05e7d60d8ef2a5c1218ada42`
- certificate: `4296FA2E355613DF0D5CE6F24D0898E00B706472`
- INF SHA-256: `BC7734C13F276A23ACD18B3CA004267B508F5DE9AD734F426E947AA051976CF4`
- CAT SHA-256: `28BFF32CC2579CCC920991E4C36CA2007D04F9F9F1D255E1EEB4350924DA5617`
- signed SYS SHA-256: `2F0F53A707C6B658FF28FAFC4A8904E7EEB5F7B2E9D2BC414A2382A859C68653`
- PDB SHA-256: `78D8D941C159C639787D5513B4AB727560EB173FB285CB6C75F119020552710B`
- manifest SHA-256: `F615EA1F0F31C8227D7592DECB2D106F4EBF874BA7104294238AA0827DD5D116`
- package regression: 849/849 functional, 66,000/66,000 stress
- configuration: Release x64, PnP diagnostics, WDM-v3 MAM,
  `MamLaunchLimit=1`, unsafe PFN DMA disabled
- validation: SYS/CAT Authenticode valid, local chain valid, CAT membership
  valid, INF consistent, SYS/PDB identity valid

The workflow records `DriverStoreChanged=false`, `PnpChanged=false` and
`DeviceOpened=false`. Passive enumeration after signing still reports
`oem107.inf`, version `0.2026.1030.1`, as installed and started. No candidate
file was installed or staged.
