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
