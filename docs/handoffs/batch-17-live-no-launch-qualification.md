# Batch 17 live no-launch qualification

## Scope and immutable identities

Batch 17 qualified the installed MDL-only registration and WDM-v3 preparation
path without GO or acquisition. The repository base was
`25febb7f00651786b6e81d259670f3b6ed5809a9`. The installed package remained
`oem104.inf`, version `0.2026.1026.1`, with loaded SYS SHA-256
`32878635FCA12750A1B4489A542E0F71A22D002A445E824A07B5AD160F4F3E4E`.
The device stayed `OK`, problem code zero, with a running service and five
interfaces. No installation, PnP transition or restart occurred.

## Single authorized live attempt

The fresh evidence root is
`C:\Users\LeCroyUser\.codex\driver-backups\Batch17-NoLaunchWithCapture-20261028-1`.
Capture became ready at `2026-10-28T17:47:39.5153589Z` from an attributable
StorNVMe kernel record while the raw file was still unflushed. The launcher
invoked the no-launch child exactly once and did not retry.

The device sequence was exactly one 4-KiB `CFDC2124` registration followed by
one one-channel `CFDC2138` preparation. Kernel evidence records one SG segment,
two descriptors, `launchDenied=1`, `released=1`, final
`STATUS_NOT_SUPPORTED` and zero IRP information. There was no GO, DMA-control
MMIO, acquisition or physical DMA. Explicit `CFDC2128` was not issued because
the live tool stopped fail-closed; CLOSE followed immediately.

The tool's outer FAIL is a false-negative. A failed DeviceIoControl preserved
the byte count from the successful registration call, and the old tool did not
reset that observation before preparation. The kernel result is unambiguous.
The tool and mock are corrected offline; no second hardware run occurred.

## Evidence hashes

| Evidence | SHA-256 |
|---|---|
| Raw kernel capture | `9D15FAF2FA9928C45279EB3613BCB581C705D9A5E20FC414A49E4900E756ABE2` |
| Live capture stdout | `29C40726DB29149E3B8F23CA50BC16921DB3F1028967215470E68FDB4927AD4E` |
| Capture-ready record | `EAEC40F876585AA97D78990EEE86B7380A766ED288A4909859F7D0BC47222526` |
| Outer result | `80FEDAD1FB733401B62E493FF5014E6804D1FAFE8ECE8FE4D9F46F429F7EA429` |
| Inner result | `DE34A47BA787AE4206A002F08A98BCE4C257271A36750899E22077BF2DE1E93A` |
| No-launch child result | `FB7AA45C41F0B5ACB2C2FC0DCFF6E5DF7F9FAADD53C3224BC1A320EFB7FC6DD5` |
| No-launch child log | `543E8505AF16C838B006B5528BF0B5D487D955A7E19C9D17AA3CC8977C506E97` |

Raw logs remain outside the public repository. They may contain private runtime
addresses and are referenced only by identity and hash.

## Offline corrections and verification

The versioned capture launcher now has a separate `NoLaunchWithCapture` mode,
authorization gate, pinned identities, attempt marker and invocation count.
Readiness uses the qualified live stdout collector. The tool zeroes returned
byte/output observations before `CFDC2138`; its mock retains stale failure data
to prevent regression.

Focused results after the correction are:

- elevated capture launcher: 34/34;
- no-launch preparation: 4/4;
- command chain: 17/17; and
- source/ABI contracts: 70/70.

The corrected x64 tool SHA-256 is
`CE0681F148B9CAD53134D7EED2CB814A7FE3CE762B4EB007D30D41639201BB6B`.
Driver and include trees did not change.

## Remaining boundary

Batch 17 proves real MDL ownership, Windows DMA-logical mapping, descriptor
construction, hard launch denial and never-launched release. It does not prove
descriptor consumption, MAM/MTT stop, last PCI write retirement, posted-write
drain, bus-master idle, safe post-GO unmapping, real DMA or acquisition.

A repeated no-launch run is not the next milestone. Before any GO-capable
candidate or hardware attempt, the project needs an authoritative physical
idle/abort contract or an approved external PCI-observation method. Any source
change, signing, installation and smallest bounded transfer remain separate
owner authorization gates.
