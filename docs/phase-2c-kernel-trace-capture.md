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

## Instrumented installation attempt stopped before capture

The owner authorized one instrumented installation and its installation-driven
initial START on 2026-10-18. Passive preflight at
`2026-10-18T20:24:31Z` revalidated all five frozen artifact hashes, SYS/CAT
Authenticode, catalog membership, the diagnostic manifest, the complete
hash-identical `oem17.inf` backup, the active production-driver identity, the
five published interface classes, absence of acquisition clients, DebugView
5.02 identity, and event-log bookmarks. No discrepancy was found.

A local fail-closed orchestration wrapper was parsed successfully by 64-bit
Windows PowerShell 5.1.19041.6456 before it was started once through UAC. The
elevated host existed from `2026-10-18T20:26:28.233Z` through
`2026-10-18T20:26:29.595Z` and returned exit code 1. It stopped before creating
the DebugView stdout/stderr files, raw kernel log, capture-ready record,
one-attempt marker, PnPUtil output, PnPUtil exit record or final result record.
Consequently DebugView and its temporary driver were not started, PnPUtil was
not invoked, the candidate was not staged or installed, and no PnP START was
caused.

The elevated console exception text was not retained by the outer UAC launcher.
A later elevated reproduction, with the DebugView child replaced by a harmless
mock, preserved the missing exception and established the exact stop point. The
process inventory rejected `LecTouchScreenCtrl.exe` because its executable path
is under `C:\Program Files (x86)\LeCroy\XStream`. The wrapper expression
`ExecutablePath -match 'LeCroy.*XStream'` classified every helper in that
directory as an acquisition client. The touchscreen controller is not one of
the blocked acquisition executables. The exception was therefore thrown before
the DebugView `Start-Process` statement. No retry was made.

The post-stop snapshot at `2026-10-18T20:27:43Z` still found `oem17.inf`,
version `0.2026.1017.1`, device status `OK`, problem code zero, a running
`LecS65AcqDrv` service, loaded SYS SHA-256
`F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`
and exactly one target interface in each legacy class. `setupapi.dev.log`
remained byte-identical at 2,529,101 bytes with SHA-256
`A7DE28AA932026694B9F386EE74A40EE310C00496FA95E720BB77F82CFF2C938`
and last-write time `2026-10-18T14:38:10.6162264Z`. No new target Kernel-PnP,
Code Integrity, UserPnp or System event was found, and no DebugView process
remained.

This result supplies no new `PNP_STATUS:` data and no Event 411 attribution.
The historical Event 411 classification therefore remains indeterminate. Any
new capture or installation attempt requires fresh explicit authorization and
must preserve its elevated exception text before active work begins.

## Versioned diagnostic launcher

Implementation commit `4ef961154d9a90183728ac522380c1c789605a0b`
adds `scripts/invoke-diagnostic-capture-install.ps1` (SHA-256
`49F7A75F2683C876BBB834A823417770213AB093343FC88CBD4D19B04128E349`).
It must run in an already elevated 64-bit Windows PowerShell 5.1 host and never
elevates or retries itself. Exact executable-name matching replaces the unsafe
directory regex; `LecTouchScreenCtrl.exe` is no longer treated as acquisition.

The modes have disjoint gates:

- `Preflight` rejects every active authorization switch. It validates the clean
  launcher revision, frozen manifest and artifact hashes, diagnostic package
  identity, SYS/CAT Authenticode, pinned DebugView identity and current driver
  state. It cannot start DebugView or PnPUtil.
- `CaptureQualification` requires an authorization identifier and
  `-AuthorizeKernelCapture`, forbids installation authorization, creates an
  exclusive capture marker, then starts only the pinned capture executable.
- `InstallWithCapture` requires the same capture gate plus
  `-AuthorizeInstallation`, a pinned PnPUtil hash and an exclusive installation
  marker. PnPUtil is reachable only after the new raw log exists and capture is
  still running. It is invoked at most once.

Every evidence root is non-overwriting. `launcher.stdout.txt`,
`launcher.stderr.txt`, `launcher.exception.txt` and `result.json` are created
before dependency loading or passive validation. Active modes additionally
reserve `capture-attempt.marker`, `capture-child.log` with independent stream
files, `capture-ready.json` and the raw `kernel-capture.log`;
`InstallWithCapture` also reserves `install-attempt.marker` and
`install-child.log` with independent stream files. The native process runner
uses a bounded Windows job object. Timeout or launcher failure closes only that
child tree, and no automatic retry or rollback exists.

The elevated Windows PowerShell 5.1 mock suite passed 18/18 checks. It covered
successful preflight, capture-only and capture/install paths; non-elevated
rejection; early terminating exceptions; DebugView start failure; missing
readiness; unexpected capture exit; timeout; nonzero PnPUtil exit; log failure;
package, tool and revision mismatches; existing markers; invocation counts; and
orphan cleanup. Source contracts passed 54/54. The successful elevated test
transcript SHA-256 is
`7144C916D87EBE035C0CA9FDB87992DECCE67AB47A7D8D340498D12253AB9EAC`.

Real `Preflight` against the frozen candidate completed at
`2026-10-18T20:45:03.5662606Z` with exit code zero. Its result records
`CaptureInvocations=0` and `InstallInvocations=0`; result SHA-256 is
`D959DEA7485B87ABA1B6BB5BCF9C331C2BE8CD69FF5346988B1F4F94736A575A`.
No DebugView process, kernel capture component, PnPUtil invocation, Driver Store
operation or PnP transition occurred.

The first real `CaptureQualification` run was separately authorized and was
executed exactly once from an elevated 64-bit Windows PowerShell 5.1 host on
2026-10-19. The command below is retained as the exact executed command; its
authorization identifier and evidence root are consumed and must never be
reused:

```powershell
& 'C:\Users\LeCroyUser\.codex\worktrees\phase2c-instrumented-package\lecroy_wr6k_64bit_driver\scripts\invoke-diagnostic-capture-install.ps1' `
  -Mode CaptureQualification `
  -RepositoryRoot 'C:\Users\LeCroyUser\.codex\worktrees\phase2c-instrumented-package\lecroy_wr6k_64bit_driver' `
  -ExpectedLauncherCommit '4ef961154d9a90183728ac522380c1c789605a0b' `
  -PackageRoot 'C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-InstrumentedSignedCandidate-20261018-1018-1-first' `
  -ExpectedManifestHash '216F15716C9496B476C877129853A07BAB59097444AA2D160E2812F25AA2DC69' `
  -ExpectedPackageWorkflowCommit 'b13f6637e5dc8499244474ad96a9c533be914904' `
  -ExpectedDriverSourceCommit '8dfdd69664ff5598d4708d2df8e083002bd5813b' `
  -ExpectedDriverTree '34ffdb81839799e323b1b4b5df2035151780dcc0' `
  -ExpectedIncludeTree 'dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b' `
  -ExpectedDriverVer '10/18/2026,0.2026.1018.1' `
  -ExpectedCertificateThumbprint '4296FA2E355613DF0D5CE6F24D0898E00B706472' `
  -ExpectedInfHash 'ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C' `
  -ExpectedCatHash '2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF' `
  -ExpectedSysHash '8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A' `
  -ExpectedPdbHash '980161BC328FFB5214B2FA76C03B59ADA22473446DA0AE3C77F970FE0B5202A3' `
  -DebugViewPath 'C:\Users\LeCroyUser\.codex\tools\sysinternals\DebugView-5.02\extracted\dbgviewcli64.exe' `
  -ExpectedDebugViewHash '7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B' `
  -EvidenceRoot 'C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-DiagnosticCaptureQualification-20261018-attempt1' `
  -AuthorizationId 'phase2c-capture-qualification-20261018-attempt1' `
  -AuthorizeKernelCapture
```

The launcher began at `2026-10-19T16:27:35.0281130Z`, declared capture ready at
`2026-10-19T16:27:37.3008418Z`, reached the configured 300-second duration
limit, and completed at `2026-10-19T16:32:37.4560270Z`. Launcher and capture
child exit codes were both zero. `result.json` records `Status=PASS`,
`CaptureInvocations=1` and `InstallInvocations=0`. Its SHA-256 is
`477B8B8B74CF502F84C0E4A1CDDA7FEFA462838C4E01A633128D0EE5BF6D835E`.
The launcher stderr and exception logs are empty. The capture child's stderr
contains only DebugView's expected duration-limit and capture-stopped notices.

| Evidence file | Bytes | SHA-256 |
|---|---:|---|
| `capture-attempt.marker` | 269 | `FCAD7756C7C7B4C54C79C650270054493503677A53403F6FE59C4BC5ECF0A50D` |
| `capture-child.log` | 96,887 | `F684A109FD101D0756299D7FC489D96BA62ED6A78B50C6C66533970F926E6D52` |
| `capture-child.log.stdout.txt` | 96,750 | `87F6D8448B40A8DDE0927BE3F1AC6B26A5156F3CD828A53E505F5F9ECBC940CC` |
| `capture-child.log.stderr.txt` | 62 | `61BAE908A8009EDCCC3AB4788E753D101569E4BFB72CD960E12F5C494FAC9A7C` |
| `capture-ready.json` | 307 | `C39CF2E15CBAB715EBD10D84D8A2033BB1D6283558EC8AB85D0667A7BA396A2D` |
| `kernel-capture.log` | 95,733 | `75C7DC09F0700908CD1C183EC7D76513224A88905FE4A838B7CDF7C36C241182` |
| `launcher.stdout.txt` | 168 | `FA72B6869D2E0C7BB18FA2AE1D7BF4C71FD168312C04B285D2828B05A2E63C5E` |
| `launcher.stderr.txt` | 0 | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |
| `launcher.exception.txt` | 0 | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |
| `result.json` | 292 | `477B8B8B74CF502F84C0E4A1CDDA7FEFA462838C4E01A633128D0EE5BF6D835E` |

The 95,733-byte raw kernel log has SHA-256
`75C7DC09F0700908CD1C183EC7D76513224A88905FE4A838B7CDF7C36C241182`.
It contains 908 records: 861 independently attributable
`STORMINI: StorAHCI` messages and 47 `KTM` messages. Win32 capture was disabled,
so these records prove that the selected DebugView path received kernel debug
output; an empty or user-mode-only log was not accepted. The first record is at
16:27:37.682 and the last at 16:32:36.595. No PnP operation was performed and,
as expected, this capture-only run contains no instrumented-driver
`PNP_STATUS:` record.

After completion no DebugView process, `DBGV` system-driver instance or
`HKLM\SYSTEM\CurrentControlSet\Services\DBGV` key remained. The LeCroy device
remained `OK` with problem code zero, bound to `oem17.inf` version
`0.2026.1017.1`; `LecS65AcqDrv` remained running and the loaded SYS SHA-256
remained
`F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`.

Kernel-capture readiness is therefore verified. `InstallWithCapture` remains a
separate active gate. Before it can run, the owner must approve one exact
installation attempt, a new non-existing evidence root and authorization ID
must be selected, the installed `pnputil.exe` SHA-256 must be pinned, and all
package, rollback, process and current-device preflight checks must pass. The
mode must start and verify capture before invoking PnPUtil, must not retry, and
must stop on the first capture, signature, package, installation or PnP
failure.

## Kernel Runtime Qualification Batch 1

The owner subsequently authorized one `InstallWithCapture` execution. A clean
worktree was temporarily checked out at the exact launcher implementation
commit `4ef961154d9a90183728ac522380c1c789605a0b`; the launcher SHA-256 remained
`49F7A75F2683C876BBB834A823417770213AB093343FC88CBD4D19B04128E349`.
The system PnPUtil SHA-256 was pinned as
`CFE15658F1F4AB0E19465A15738B7FDB4BE947CDA4F2AA8B1C1B995CF50CBDDB`.
All frozen package, signature, catalog, PDB, rollback, device, interface and
process preconditions passed before active work.

The elevated Windows PowerShell 5.1 launcher ran exactly once from
`2026-10-19T16:53:29.2283759Z` through
`2026-10-19T16:58:32.8669865Z`. `result.json` reports `PASS`, one capture
invocation and one installation invocation. Launcher and PnPUtil exit codes
were zero. PnPUtil published `oem101.inf` and installed it for the exact WR6k
PCI function. No retry or rollback occurred.

| Evidence | SHA-256 |
|---|---|
| Raw `kernel-capture.log` | `D14DAC42D6CEBED65313DE8A265E10B80D52CB57A33C0F79A888F625257948A4` |
| Launcher `result.json` | `5C9B186BF86DC3B2CB4FF0B4C3EB9AEE16D98D1A080C9A141EDF618831D39437` |
| PnPUtil install log | `3F8617C5FD5522BAAE5FC6AB141A44E75DFE33486A1C235A963A8B203FFD738D` |
| Capture-ready record | `6A5DC21958D5E4D1E91391C79A42F2D61B88C6DF393907B4665F6CC9EB31EA00` |
| Final private evidence manifest | `C50F5578F5FF822BB0A1A7D15084A956A18ABDBD21407BC861DAF3339BE6BA8F` |
| SetupAPI appended interval | `C225D62259A3E579282C7B21079161C9BC3807F75A71E5D851CFA77F4A44A3B4` |

The raw log is 73,498 bytes with 691 records: 84 LecS65 records, including 32
`PNP_STATUS:` records, plus 607 independently attributable StorAHCI records.
The launcher stderr and exception logs are empty. After capture, no DebugView
process, `DBGV` driver instance, service key or temporary driver file remained.

### PnP and Event 411 correlation

The old driver received PnP minors `0x07`, `0x01` (QUERY_REMOVE) and `0x02`
(REMOVE) at `16:53:33.872Z`. It disabled all five interfaces, disconnected the
interrupt, completed teardown with zero retained transfers, unmapped resources
and unloaded by `16:53:33.873Z`.

Kernel-PnP Event 400 was recorded at
`2026-10-19T16:53:33.8422476Z`. Event 411 was recorded at
`2026-10-19T16:53:33.8741035Z` with problem `0x15` and status
`0xC00000BB`. The replacement driver's `DriverEntry`/AddDevice did not begin
until `16:53:33.901Z`. Event 411 therefore falls after old-driver removal and
before any new-driver PnP dispatch.

The new driver later recorded lower-stack `STATUS_NOT_SUPPORTED` for
QUERY_LEGACY_BUS_INFORMATION, two QUERY_INTERFACE requests and
QUERY_DEVICE_RELATIONS. Those statuses are **VERIFIED lower-stack-originated**,
but their timestamps are later than Event 411 and they cannot be its source.
The new START is fully attributed: lower completion/final, local START,
`IoConnectInterrupt`, five indexed interface-enable calls and final returned
status are all `STATUS_SUCCESS`. There is no driver-originated START failure or
interrupt-connection failure.

Event 411 is classified as a **VERIFIED intermediate installation/PnP state**.
This classification is based on the trace ordering, not merely on the final
healthy device. The internal Windows/PnP component that emitted the event's
`0xC00000BB` remains **INDETERMINATE**. The earlier, uninstrumented Event 411 is
strongly consistent with the same installation sequence, but its exact source
was not directly traced and remains separately unproved.

SetupAPI records best-driver selection, package configuration, one
query-remove, one device restart, `Start`, `Restart verified` and final
`SUCCESS`. The package configuration noted that devices using the driver
required restart; this was the single installation-induced device restart, not
an operating-system reboot request. No target Code Integrity, Service Control
Manager or System error occurred. Code Integrity records for Bonjour
`mdnsNSP.dll` in the interval are unrelated.

### Final runtime state and next gate

The target is bound to `oem101.inf`, version `0.2026.1018.1`, with device
status `OK`, problem code zero and a running `LecS65AcqDrv` service. The loaded
SYS and Driver Store INF/CAT/SYS match the frozen candidate; each of the five
legacy interface classes has exactly one enabled path for the PCI function.
No acquisition application is running.

No additional PnP transition, device handle or IOCTL was used. The smallest
next authorization is the bounded R0 metadata-only batch in
[Runtime IOCTL safety classification](ioctl-runtime-safety.md). A controlled
PnP restart and Driver Verifier remain later, separately authorized gates.
