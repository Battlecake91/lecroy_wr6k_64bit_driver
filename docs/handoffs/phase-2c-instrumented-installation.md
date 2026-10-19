# Phase 2C instrumented-installation handoff

This handoff records the failed pre-capture execution on 2026-10-18 and the
separately authorized successful capture qualification on 2026-10-19. Neither
execution installed or replaced the LeCroy driver.

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

The outer UAC launcher did not retain the elevated console exception. A later
elevated reproduction using a harmless capture mock recovered the exact error:
the process inventory rejected `LecTouchScreenCtrl.exe`. Its path contains
`LeCroy\XStream`, and the one-off wrapper used the over-broad expression
`ExecutablePath -match 'LeCroy.*XStream'`. The failure therefore occurred in
the process inventory before the DebugView start statement. No retry was made.

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

Event 411 remains unattributed because no diagnostic START has occurred.
Versioned launcher commit `4ef961154d9a90183728ac522380c1c789605a0b`
completed the separately authorized `CaptureQualification` run once from
`2026-10-19T16:27:35.0281130Z` through
`2026-10-19T16:32:37.4560270Z`. Launcher and capture-child exit codes were zero,
and the result records one capture invocation and zero installation
invocations. The raw log contains 908 kernel records, including 861
independently attributable `STORMINI: StorAHCI` messages, and has SHA-256
`75C7DC09F0700908CD1C183EC7D76513224A88905FE4A838B7CDF7C36C241182`.
This establishes the kernel-message capture path without relying on an empty
log or user-mode messages.

No DebugView process, `DBGV` system-driver instance or service registry key
remained afterward. The device stayed `OK`, problem-free and bound to
`oem17.inf` version `0.2026.1017.1`; the running production SYS retained SHA-256
`F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`.

The next active gate is one separately authorized `InstallWithCapture` run. It
requires a new evidence root and authorization identifier, a pinned local
PnPUtil hash and renewed passive validation of package, rollback, process and
device state. No installation, PnP transition, rollback, restart, reboot,
device handle, IOCTL, MMIO, DMA, acquisition, EEPROM or FPGA operation is
authorized by this handoff.
