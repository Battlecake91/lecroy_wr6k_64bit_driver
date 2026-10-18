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

The initial preparation did not load the kernel component. A later, separately
authorized validation completed successfully; its evidence is recorded below.

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

## Signed instrumented Release candidate

One separately authorized signing run completed on 2026-10-18. The versioned
launcher at workflow commit
`b13f6637e5dc8499244474ad96a9c533be914904` ran once under elevated 64-bit
Windows PowerShell 5.1.19041.6456 and exited zero without retry. It required the
reviewed driver/include basis
`8dfdd69664ff5598d4708d2df8e083002bd5813b`, exact tree hashes
`34ffdb81839799e323b1b4b5df2035151780dcc0` and
`dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b`, Release x64,
`DriverVer=10/18/2026,0.2026.1018.1`, and the explicit
`-PnpStatusDiagnostics` switch.

The elevated preflight opened the non-exportable RSA-2048 CNG private key with
the Microsoft Software Key Storage Provider, verified code-signing EKU
`1.3.6.1.5.5.7.3.3`, and built the local chain without error. The workflow
clean-rebuilt Debug and Release, passed 397/397 functional checks and all
66,000/66,000 stress iterations, and signed only the selected Release SYS and
its CAT with certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`.

| Frozen artifact | Bytes | SHA-256 |
|---|---:|---|
| `LecS65AcqDrv.inf` | 1,247 | `ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C` |
| `lecs65acqdrv.cat` | 2,509 | `2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF` |
| Signed `LecS65AcqDrv.sys` | 79,784 | `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A` |
| Matching `LecS65AcqDrv.pdb` | 569,344 | `980161BC328FFB5214B2FA76C03B59ADA22473446DA0AE3C77F970FE0B5202A3` |
| `manifest.json` | 14,274 | `216F15716C9496B476C877129853A07BAB59097444AA2D160E2812F25AA2DC69` |

Manifest schema 2 records `PackageState=SignedCandidate`,
`InstallAuthorized=false`, `PnpStatusDiagnostics=true`, the same true mode for
both clean builds and diagnostic marker `PNP_STATUS:`. Independent binary
inspection found 11 marker instances in the signed Release SYS. InfVerif and
Inf2Cat completed without errors or warnings. Get-AuthenticodeSignature and
SignTool independently accepted both SYS and CAT with the exact certificate;
SignTool also verified the SYS as a member of the CAT. SymChk matched the
signed SYS to the frozen PDB with RSDS GUID
`242FA5CC-1D26-400A-9F79-43F7859CA4C0`, age 1.

The signatures are deliberately not timestamped and validate through the
locally trusted self-signed test certificate. They are not Microsoft
production signatures. The immutable candidate remains in the private output
root `Phase2C-InstrumentedSignedCandidate-20261018-1018-1-first`; it contains
53 package/evidence files. Its external launcher log has SHA-256
`33332BA1BA731BDC4222395FF696D5CBB10B507FF7E385EA43A63E0481357124`.

Passive post-run inspection again found `oem17.inf`, version
`0.2026.1017.1`, device status `OK`, problem code zero, a running service and
loaded SYS SHA-256
`F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`.
The signed diagnostic candidate was not staged, installed or loaded.

## Validated kernel-capture run

The owner authorized exactly one temporary kernel-capture validation. From an
elevated 64-bit Windows PowerShell 5.1 process, the run used the reviewed
command shape below exactly once:

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
automatic retry. None of those alternatives was used.

The child process exited zero. The configured capture duration was 300 seconds;
the complete elevated child lifetime, including startup and shutdown, was
301.165 seconds from `2026-10-18T15:44:11.3475787Z` through
`2026-10-18T15:49:12.5123800Z`. The first and last captured records were at
15:44:13.291 and 15:49:11.554 respectively.

The raw 66,018-byte log contains 618 records and has SHA-256
`B8197022B9A23DA69B92881AB3F0A12A3AFB4BDF8B810CEF562F3FBA466E69EB`.
Every record is a `STORMINI: StorAHCI - LPM` kernel message for storage port 2.
That prefix and subsystem identify the independent source as the Windows
StorAHCI storage miniport. There are no prior user-mode validation markers and
no `LecS65x64` or `PNP_STATUS:` records, as expected because the currently
loaded LeCroy image is uninstrumented. The non-empty kernel-only stream proves
that the temporary DebugView kernel component was functional; a user-mode
capture cannot produce this result because the command disabled Win32 capture.

No persistent `Dbgv` service was visible before the run. Read-only polling did
not expose a durable SCM/WMI driver record while capture was active. After the
run there is no DebugView process, matching system-driver object or Services
registry entry, no related System or Code Integrity error, and no residual
`Dbgv*.sys` file under the user or Windows temporary directories. The component
therefore left no detected persistent service, binary or process state.

The driver source emits the required records with `DPFLTR_IHVDRIVER_ID` and
`DPFLTR_ERROR_LEVEL`. Level zero maps to mask bit `0x1`. The capture used
`--verbose-kernel`; the host has no `Debug Print Filter` registry override and
no current BCD `debug` or `bootdebug` setting. Together with the successfully
received kernel stream, this establishes the required capture/filter
configuration without changing a filter or boot option. The exact
`PNP_STATUS:` marker cannot be observed end to end until the separately signed
instrumented driver is installed; its absence in any future initial-START log
remains a mandatory stop condition.

The kernel-capture infrastructure is therefore **ready** for the later
instrumented initial-START experiment. This conclusion authorizes neither
private-key use nor driver replacement or PnP activity.

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
