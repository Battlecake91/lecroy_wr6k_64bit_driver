# Phase 2C instrumented-installation handoff

This handoff records the one authorized execution on 2026-10-18. The execution
stopped before kernel capture and before driver installation. No retry was
performed.

## Approved candidate

The approved private package is Release x64,
`DriverVer=10/18/2026,0.2026.1018.1`, with
`PnpStatusDiagnostics=true` and local test-signing certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`.

| Artifact | SHA-256 |
|---|---|
| INF | `ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C` |
| CAT | `2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF` |
| Signed SYS | `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A` |
| Matching PDB | `980161BC328FFB5214B2FA76C03B59ADA22473446DA0AE3C77F970FE0B5202A3` |
| Manifest | `216F15716C9496B476C877129853A07BAB59097444AA2D160E2812F25AA2DC69` |

The passive preflight reconfirmed these hashes, signatures, catalog membership,
manifest identity, rollback package, active driver, interface publication,
closed acquisition applications, DebugView identity and event bookmarks.

## Execution result

An elevated 64-bit Windows PowerShell 5.1.19041.6456 host ran once from
`2026-10-18T20:26:28.233Z` to `2026-10-18T20:26:29.595Z` and exited with code
1. It created none of the gated active evidence files: no DebugView output,
kernel log, capture-ready record, attempt marker, PnPUtil output, PnPUtil exit
record or final result record. Therefore:

- DebugView and `Dbgv.sys` were not started;
- `pnputil /add-driver ... /install` was not invoked;
- the instrumented candidate was not staged, installed or loaded;
- no installation-driven PnP START occurred; and
- no `PNP_STATUS:` records were generated.

The outer UAC launcher did not retain the elevated console exception. The
available evidence bounds the failure to the wrapper before, or at, DebugView
process launch, but does not identify the exact expression. Read-only
revalidation found all pre-capture predicates still passing. No retry was made.

## Verified unchanged state

At `2026-10-18T20:27:43Z`, the PCI device remained `OK` with problem code zero,
bound to `oem17.inf` version `0.2026.1017.1`; the service remained running and
the loaded SYS hash remained
`F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`.
Each of the five legacy interface classes still exposed exactly one path for
the target function. The SetupAPI device-install log retained its preflight
length, last-write time and SHA-256, and no new relevant PnP, Code Integrity,
UserPnp or System event was found.

## Next authorization boundary

Event 411 remains unattributed because no diagnostic START occurred. Any new
kernel capture or installation requires fresh explicit authorization. Before
such authorization is exercised, the elevated launcher must preserve its own
terminating exception and process exit evidence without changing the reviewed
capture or installation command sequence. No PnP transition, rollback,
restart, reboot, device handle, IOCTL, MMIO, DMA, acquisition, EEPROM or FPGA
operation is authorized by this handoff.
