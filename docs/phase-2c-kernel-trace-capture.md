# Phase 2C kernel-trace capture and instrumented package

This document defines the evidence gate for attributing the Kernel-PnP event
411 status. It covers capture preparation and an unsigned diagnostic package.
It does not authorize a kernel capture driver, private-key use, driver
installation, a PnP transition, reboot or hardware access.

## Capture-method decision

The diagnostic driver writes `PNP_STATUS:` records through
`vDbgPrintExWithPrefix` with `DPFLTR_IHVDRIVER_ID` and
`DPFLTR_ERROR_LEVEL`. These records are debug-print output, not ETW events.
Consequently:

- ETW/WPP cannot collect the existing records without changing the driver to
  emit ETW/WPP events;
- WinDbg/KD/KDNET can collect them, but configuring a kernel-debug transport
  changes boot configuration and normally requires a reboot; and
- Microsoft Sysinternals DebugView is the preferred temporary collector
  because it needs no boot-configuration change or reboot.

DebugView kernel capture is not a user-mode-only operation. The command-line
tool extracts and loads its `Dbgv.sys` kernel component when `--kernel` is
selected. Loading that additional driver requires a separate owner
authorization. Neither `Dbgv.sys` nor another capture driver was loaded during
this preparation.

The source uses error level zero. Windows interprets a level from zero through
31 as the corresponding bit in the component mask, so this requires bit zero
(`0x1`) for the IHVDRIVER component. The host currently has no
`Debug Print Filter` registry key and no current `debug` or `bootdebug` BCDEdit
setting. That observation does not prove that a collector receives the output.
The approved validation command must select both `--kernel` and
`--verbose-kernel`; a non-empty, independently attributable kernel record must
then be observed before any diagnostic PnP authorization is consumed. An empty
log is a failed readiness check.

Official references:

- [Microsoft Sysinternals DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview)
- [Reading and filtering debugging messages](https://learn.microsoft.com/windows-hardware/drivers/debugger/reading-and-filtering-debugging-messages)

## Capture-tool identity and completed checks

The official Sysinternals archive was downloaded from
`https://download.sysinternals.com/files/DebugView.zip` and retained outside
the public repository.

| Item | Identity |
|---|---|
| Archive | 1,746,522 bytes; SHA-256 `A8454253756AF10667B82FAF2323DE536F0B7084D732ACBA63803DF01CE4C316` |
| Selected executable | `dbgviewcli64.exe`, version 5.02, 488,800 bytes |
| Executable SHA-256 | `7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B` |
| PE identity | AMD64 (`0x8664`), Windows CUI |
| Authenticode | Valid; signer `Microsoft Corporation` |

`dbgviewcli64.exe --help` exited zero without creating a `Dbgv` service. A
separate `--no-kernel --win32` run captured an independently emitted
`OutputDebugString` marker, exited zero and left no `Dbgv` service. The raw
70-byte validation log has SHA-256
`1B0E7FA2515ED6D65333FB0557C5AA1031C2334E5BB2F0A5B528AB31C20B0A26`.
This proves executable selection, user-mode capture and file logging only. It
is explicitly not evidence that kernel capture works.

The kernel path remains **not ready for an active PnP experiment** until a
separately authorized, elevated validation loads `Dbgv.sys`, confirms the
collector status and captures a harmless kernel message whose source can be
identified independently. No guaranteed message source was available without
loading a driver or causing another unauthorized system transition, so this
gate was not bypassed.

## Instrumented package contract

The package workflow now accepts the explicit switch
`-PnpStatusDiagnostics`. It passes
`/p:LecPnpStatusDiagnostics=true` to clean Debug and Release rebuilds, requires
the `PNP_STATUS:` marker in both resulting SYS files and the selected package
SYS, and records the mode and marker in manifest schema 2. Omitting the switch
passes `false`, requires the marker to be absent and preserves the production
default. A marker/mode mismatch, an incremental or mixed-mode manifest, or a
missing artifact is fatal.

The proposed diagnostic identity is
`DriverVer=10/18/2026,0.2026.1018.1`. It is distinct from the installed
`10/17/2026,0.2026.1017.1`. Windows compares rank before date and version; the
new identity is newer only when hardware match, signature and feature-score
rank are otherwise equal. The proposal has not been signed or installed.

An isolated Windows PowerShell 5.1 unsigned run used:

```powershell
.\scripts\build-driver-package.ps1 `
  -ExpectedDriverTreeCommit 8dfdd69664ff5598d4708d2df8e083002bd5813b `
  -RuntimeConfiguration Release `
  -DriverVer '10/18/2026,0.2026.1018.1' `
  -OutputRoot 'C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-InstrumentedUnsigned-20261018-1018-1-rebuild' `
  -PnpStatusDiagnostics
```

The exact driver and include tree hashes are respectively
`34ffdb81839799e323b1b4b5df2035151780dcc0` and
`dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b`. SDK/WDK 10.0.28000.0 and
MSVC 14.41.34120 produced clean AMD64 Debug and Release rebuilds. Both SYS
files have Native subsystem, `DriverEntry`, imports limited to `ntoskrnl.exe`
and `HAL.dll`, and the required diagnostic marker. The selected Release
SYS/PDB pair passed `symchk`; InfVerif and Inf2Cat completed with no errors or
warnings.

The package workflow implementation used commit
`f84ae49e95a1a9dd0ea7e8a8fba417684a846922`, layered on the reviewed
diagnostic driver/include trees from
`8dfdd69664ff5598d4708d2df8e083002bd5813b`.

Regression results were 397/397 functional checks, 5,000/5,000 default DMA,
1,000/1,000 default INTEN, 50,000/50,000 expanded DMA and 10,000/10,000
expanded INTEN iterations. Focused workflow compatibility passed 25/25 on both
Windows PowerShell 5.1 and PowerShell 7.6.5; fail-closed package tests passed
16/16 on both hosts. The private key was not accessed.

| Unsigned artifact | SHA-256 |
|---|---|
| INF | `ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C` |
| CAT | `CADFF300E7DC179CF54CF7055864469140590656F941167318E9D77583743F8D` |
| Release SYS | `E888C479069CB1393B9D8A5F5AC935C80065696DA1BD1D03560FCFBC09237155` |
| Matching PDB | `EE199AA3A30E635BEC2974DD1170FD16A59581FCBFAE389A2718A33985ED1337` |
| Manifest | `FF6E4AEF91141F92DE646AF18AEB5BE49DF8F003EF5253434F5AB17E515ACA7E` |

These hashes identify an unsigned validation set, not an installation
candidate. Signing changes the SYS and CAT hashes. A future signing approval
must name the final workflow commit, reviewed tree identity, proposed
`DriverVer`, certificate and a new output root. The signed manifest must freeze
the final INF/CAT/SYS/PDB hashes before installation is considered.

The installed driver was checked passively after the offline run. It remains
`oem17.inf`, version `0.2026.1017.1`, device status `OK`, problem code zero,
service `LecS65AcqDrv` running, and loaded SYS SHA-256
`F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`.

## Authorized-gate capture command

The next executable step is capture-path validation, not package signing or a
PnP action. It requires explicit authorization to load the Microsoft
Sysinternals `Dbgv.sys` component. From an already elevated 64-bit Windows
PowerShell 5.1 console, the reviewed command shape is:

```powershell
$tool = 'C:\Users\LeCroyUser\.codex\tools\sysinternals\DebugView-5.02\extracted\dbgviewcli64.exe'
$log = 'C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-PnpCapture\kernel-readiness-01.log'
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $tool).Hash -ne
    '7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B') {
    throw 'Unexpected DebugView CLI identity'
}
if (Test-Path -LiteralPath $log) { throw 'Capture log already exists' }
& $tool --kernel --verbose-kernel --no-win32 --clock-ms --format text `
  --duration 300 --max-lines 100000 --log $log --no-banner
if ($LASTEXITCODE -ne 0) { throw 'Kernel capture failed' }
```

Do not use `--boot-enable`, `--log-append`, a different executable or an
automatic retry. Record the command transcript, service/driver state before
and after, raw-log SHA-256, local and UTC bounds and the identity of the
independent kernel-message source. Stop if the component cannot load, status
cannot be verified, the log is empty, the source cannot be attributed, or the
tool identity changes.

## Future two-step active test

Both active steps remain separate authorization gates:

1. **Instrumented installation and first START.** Use exactly one frozen,
   signed Release package whose manifest proves diagnostics enabled and whose
   final hashes have been approved. Start already validated kernel capture
   before installation. Permit one installation/START only. Correlate
   `enter`, `lower-dispatch`, `lower-complete`/`lower-wait`, `lower-final`,
   `local-start`, `interrupt-connect`, five indexed `interface-enable` records
   and `final` with SetupAPI and event-log timestamps.
2. **Controlled restart.** After separate approval and only if step 1 is fully
   healthy, start a new verified capture and execute one
   `pnputil /restart-device` operation. No unattended retry is permitted.

For each step, hash the raw capture immediately after closing the collector and
preserve SetupAPI deltas plus complete XML for Kernel-PnP, Code Integrity,
System and Service Control Manager events. Stop without automatic rollback on
a capture failure, identity/signature mismatch, bugcheck/hang, unexpected PnP
cycle, nonzero command result, START failure, malformed/missing diagnostic
stage, interface-count mismatch, persistent devnode problem, interrupt storm
or any acquisition/DMA activity.

Recovery uses the preserved, hash-verified `oem17.inf` package or Windows
reinstallation. Any rollback that causes PnP activity and any reboot require
their own authorization. XStream, device handles, IOCTLs, Driver Verifier,
MMIO, DMA, acquisition, EEPROM and FPGA access remain outside this plan.
