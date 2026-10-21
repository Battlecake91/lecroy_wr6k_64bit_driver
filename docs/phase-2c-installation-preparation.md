# Phase 2C installation and recovery preparation

This document is the controlled bridge between offline package creation and
runtime qualification. The first authorized installation and START completed
on 2026-10-18; later gated commands remain unexecuted unless stated otherwise.

## Current post-recovery state

The 2026-10-20 audit found the active EFI/BCD loader without `testsigning` and
attributed Code 52 to that missing boot element. On 2026-10-21 the owner
authorized exactly one `testsigning on` change and one restart. Both BCD
commands returned zero, only that loader element changed, and post-boot
readback shows `testsigning Yes`. The HFM NVMe boot/system path is unchanged.
The exact `oem101.inf` driver is now loaded, problem-free and running with all
five interfaces enabled. Windows RE remains disabled and only one disk is
visible; no recovery rehearsal was added. See
[Code 52 signing recovery](code52-signing-recovery.md). Any later Test Mode
disable, reboot, IOCTL or PnP test remains separately gated.

## First installation outcome

One elevated 64-bit Windows PowerShell 5.1 invocation ran PnPUtil from
16:38:08.632 +02:00 through 16:38:10.618 +02:00 and returned zero. Windows
published the candidate as `oem17.inf`. SetupAPI records one `Query-remove`,
one `Start`, `Restart verified` and final `SUCCESS`. At 16:38:59 the PCI
instance was `OK` with problem code zero, version `0.2026.1017.1`, a running
service and the approved loaded-SYS hash. The five expected interface classes
each had exactly one path for the target function.

Kernel-PnP event 411 nevertheless recorded problem `0x15` and status
`0xC00000BB` during the update. These values map to a removal-state problem and
`STATUS_NOT_SUPPORTED`, not a signature failure, and the event was followed
inside the same operation by SetupAPI's verified restart and healthy live
state. Its origin remains indeterminate: no further PnP transition is
authorized and the result must not be described as an event-free START.

### Event 411 passive analysis

The retained clocks and record identifiers establish this timeline for device
instance
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`:

| Time (2026-10-18, UTC+02:00) | Direct evidence |
|---|---|
| 16:38:08.6324511 | The one elevated PnPUtil process started. |
| 16:38:09.9077719 | Kernel-PnP event 400, record 465, recorded successful configuration selection of `oem17.inf`. |
| 16:38:09.908 | SetupAPI completed device configuration and retained device status `0x0180200a`. |
| 16:38:09.913 | SetupAPI entered its `Restarting Devices` block. Its following un-timestamped lines are `Query-remove`, `Start`, then `Restart verified`, in that textual order. |
| 16:38:09.9371050 | Kernel-PnP event 411, record 466, recorded problem `0x15` and status `0xC00000BB`. |
| 16:38:10.601 | SetupAPI left `Restarting Devices`; the next entries report success from Configure Device and Core Device Install. |
| 16:38:10.616 | SetupAPI closed the device-update section with `SUCCESS`. |
| 16:38:10.6182255 | PnPUtil exited with code zero. |

The Windows 10.0.28000.0 headers identify `0x15` as
`CM_PROB_WILL_BE_REMOVED` and `0xC00000BB` as `STATUS_NOT_SUPPORTED`. There was
no event 410 in the installation interval. There was no driver-related Service
Control Manager event, and no new Code Integrity event. SetupAPI proves the
logged operation order, but its individual `Query-remove`, `Start` and
`Restart verified` lines have no timestamps. Consequently, the retained data
does not prove where event 411 falls between those individual calls, nor does
it reveal which stack component supplied `STATUS_NOT_SUPPORTED`. No assumption
about undocumented event-generation or SetupAPI buffering is used to close
that gap.

Source review at the installed driver revision establishes:

- `IRP_MN_QUERY_REMOVE_DEVICE` and `IRP_MN_CANCEL_REMOVE_DEVICE` take the
  default PnP path. `LecForwardLockedIrp` forwards them unchanged and releases
  the remove lock from completion; it does not synthesize a status.
- `IRP_MN_STOP_DEVICE` closes IOCTL admission, disables interfaces, drains
  admitted synchronous IOCTLs, quiesces interrupt/DPC/timer work, releases
  transfer/event state and unmaps BARs before forwarding. Its returned status
  is the lower-stack status.
- `IRP_MN_START_DEVICE` first returns the exact result of
  `LecForwardAndWait` when the lower stack fails. After lower-stack success,
  locally authored failures are `STATUS_DEVICE_NOT_READY`,
  `STATUS_DEVICE_CONFIGURATION_ERROR`, `STATUS_INSUFFICIENT_RESOURCES` or
  `STATUS_UNSUCCESSFUL`; none is `STATUS_NOT_SUPPORTED`.
- START passes through the status returned by `IoConnectInterrupt`. Thus the
  replacement has no explicit `STATUS_NOT_SUPPORTED` branch, but it could
  still return that value indirectly if the lower stack or that kernel API
  supplied it. The retained logs contain no driver debug output that
  distinguishes these cases.
- Interface-enable failures are logged per interface but do not fail START.
  STOP, surprise removal and removal withdraw enabled interfaces; interrupt
  teardown disconnects the ISR and clears its software shadows.
- A remove-lock acquisition failure is completed immediately with that status.
  No device handle or IOCTL existed during installation, and the evidence gives
  no IOCTL-admission path a role in event 411.

The original focused hardware-independent PnP forwarding test passed 8/8 on
2026-10-18. The diagnostic-preparation revision expands it to 10/10 by injecting
both synchronous and pending `STATUS_NOT_SUPPORTED` results. The added cases
confirm exact status preservation and the diagnostic attribution described
below. Neither result attributes the historical live event retrospectively.

A read-only runtime snapshot at 16:53:12 +02:00 confirmed `oem17.inf`, version
`0.2026.1017.1`, device `OK`, problem code zero, service `Running`, valid
loaded-SYS signature and SHA-256
`F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`.
Exactly one Driver Store directory matched all frozen INF/CAT/SYS hashes, and
each of the five interface classes still exposed exactly one target path.
There was no later target Kernel-PnP event after record 466 and no later target
System or Code Integrity warning/error. This snapshot used metadata only and
opened no interface.

The private evidence references are
`phase2c-first-install-20261018-attempt1/install-result.json`,
`setupapi.dev.before.log`, `setupapi.dev.after.log`,
`event-bookmarks-before.json`, the four new-event extracts,
`postinstall-summary.json`, `interfaces-summary.json` and
`driver-store-file-hashes.json`. Raw host logs remain outside the public
repository.

**Classification: indeterminate, requiring additional targeted evidence.**
Confidence is high that event 411 was a transient part of the package-update
restart rather than a persistent devnode failure: SetupAPI completed the same
restart successfully and all later snapshots are healthy. Confidence is also
high that the replacement does not explicitly manufacture
`STATUS_NOT_SUPPORTED`. Attribution to Windows/PnP, the lower PCI stack or an
indirect kernel call remains unproved, so the event is not classified as
harmless or as a confirmed driver defect.

The installed Release driver has useful `[LecS65x64]` messages for the incoming
PnP minor function, START resource/probe processing, IRQ connection and each
interface enable. Those messages cannot identify the lower dispatch/completion
status, the exact local START status or the final returned status. They also
cannot be recovered retrospectively because no kernel-debug listener captured
the installation. Therefore the installed image cannot meet the attribution
gate without replacement.

### Opt-in PnP status diagnostics

The diagnostic-preparation revision adds a compile-time-only
`LecPnpStatusDiagnostics` MSBuild property. Its default is `false`; an
instrumented build requires the explicit property
`/p:LecPnpStatusDiagnostics=true`. The default binaries contain no
`PNP_STATUS:` strings. The instrumented binary emits narrowly scoped records
through the existing `LecTrace`/`vDbgPrintEx` mechanism. Every record includes
the IRP address, PnP minor function and an attribution stage:

- `enter` and `remove-lock` identify dispatch entry or pre-dispatch rejection;
- `lower-dispatch` and `lower-complete` distinguish the immediate lower-driver
  return from the eventual `Irp->IoStatus.Status` for forwarded
  QUERY_REMOVE, CANCEL_REMOVE and STOP requests;
- `lower-wait` and `lower-final` record the lower START/REMOVE dispatch value
  and the value retained after a pending completion;
- `local-start` distinguishes the DMA-quarantine gate from resource parsing,
  BAR mapping and startup-probe handling;
- `interrupt-connect` records the exact `IoConnectInterrupt` result;
- `interface-enable` records each of the five `IoSetDeviceInterfaceState`
  results; and
- `final` records the status completed by locally owned START/REMOVE IRPs.

The trace points do not change any status, wait, allocation, lock, IOCTL,
MMIO, IRQ, DMA or interface ordering. No trace is added to an ISR or DPC. The
default build retains the original direct forwarding expression, and both
default and instrumented Debug/Release x64 configurations build independently.
Installing an instrumented image is a new driver replacement, not a logging
toggle for the current driver. It requires a separately authorized package
build/signing operation and a separately authorized installation.

The capture tool and unsigned package are prepared under the separate
[kernel-trace capture contract](phase-2c-kernel-trace-capture.md). A later
separately authorized DebugView run captured 618 independently attributable
StorAHCI kernel records with Win32 capture disabled and exited zero. Its raw-log
hash and cleanup evidence are frozen in that contract. The temporary kernel
component left no detected process, service, registry or temporary-file state.
Capture readiness is therefore established; signing and instrumented driver
replacement remain separate authorization gates.

#### Offline evidence

The four isolated builds used SDK/WDK 10.0.28000.0 and MSVC 14.41.34120. All
reported AMD64 machine `0x8664`, Native subsystem, `DriverEntry` and the
expected `ntoskrnl.exe` import. The SHA-256 values are evidence for these
unsigned, non-deployment build outputs only:

| Mode | Configuration | SYS SHA-256 | PDB SHA-256 |
|---|---|---|---|
| default | Debug x64 | `ED32C79AEFA9A7A9CBA16EE3AEA9E7840E1251AE8E3A4E028B63AF60C9FDE74D` | `D0919AC0DC4FDEBDBA05B1C35E2FFCF6E85BE3C3064DF84AF6EB8BD80392EC90` |
| default | Release x64 | `04EF959DEF6D96B3D1744D9F0C710ED939F0E78AA6E3CE61561DBC7E764EB4AD` | `DF36FDAF8F8F84CD51769C661CB6E55238B1EA068D8239250DA452A0B60F7646` |
| instrumented | Debug x64 | `9CD420775E47BB9E38E86A11A90F840C0661811686042D9CCE031FAEE387AAA9` | `EB464E33FF8B05064EAA62A9163FA21FE58F2A2D346A6E694C61729D7FDBA547` |
| instrumented | Release x64 | `88B1C38387CE0344335DC4364819406457014ACD3EE285DE09D39525F53E7C6A` | `3A0F069CCF20B94FD4EE4BBF0C06434B6FCA46B87F7123AA5EFD28C21261EB8A` |

The focused forwarding/remove-lock suite passes 10/10 and all applicable
source, public-ABI, interface, package-safety and hardware-sequencing contracts
pass 53/53. The source diff does not change a public header, IOCTL definition,
resource parser, interrupt implementation, DMA path or MMIO operation. The
66,000-iteration DMA/INTEN stress suite was not repeated because none of the
code it exercises changed.

### Proposed single-cycle attribution test

This is a plan, not an authorization or a record of execution.

1. Produce one signed Release x64 package from the reviewed diagnostic commit
   with `LecPnpStatusDiagnostics=true`. Freeze its source revision, package
   identity, certificate, SYS/PDB/INF/CAT hashes and complete manifest. This
   requires explicit private-key/signing authorization.
2. Before replacement, confirm the current `oem17.inf` device is `OK`, problem
   code zero, service `Running`, loaded SYS hash
   `F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`,
   Driver Verifier off, five interfaces present, no acquisition running and
   XStream closed. Preserve the current package and independently verified
   rollback copy.
3. Install the frozen instrumented package under a separate one-install/START
   authorization. Stop if package identity, signature, binding, START, device
   status, interface count or event inspection differs from the approved
   expectation. Do not combine installation with the diagnostic PnP cycle.
4. Before the later cycle, verify the reviewed DebugView CLI hash and start it
   elevated with kernel and verbose-kernel capture into a new log. Its kernel
   component load requires separate authorization. Confirm an independently
   attributable harmless kernel message before consuming the one-cycle
   authorization. An empty log or user-mode-only marker is not proof.
5. Record UTC/local start markers, SetupAPI log size, event-record bookmarks,
   devnode/service metadata and the five interface paths. From the same
   elevated 64-bit Windows PowerShell session, execute exactly once:

   ```powershell
   $instance = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0'
   & "$env:SystemRoot\System32\pnputil.exe" /restart-device $instance
   if ($LASTEXITCODE -ne 0) { throw "PnP restart failed; do not retry" }
   ```

6. Stop capture without another PnP action. Save the raw debug log, SetupAPI
   delta and complete XML for new Kernel-PnP, Code Integrity, System and SCM
   events. Correlate one IRP address at a time. A repeated `0xC00000BB` is
   lower-stack-originated if it first appears in `lower-complete`/`lower-wait`;
   local-resource-originated if `lower-final` succeeds and `local-start` first
   reports it; or interrupt-originated if both preceding stages succeed and
   `interrupt-connect` first reports it. Interface publication is independently
   visible in five indexed `interface-enable` records.

Stop immediately, without retry or automatic rollback, on a bugcheck/hang,
unexpected extra PnP cycle, nonzero PnPUtil result, signature/Code Integrity
failure, absent or malformed diagnostic stages, non-successful START stage,
fewer or more than five successful interface records, persistent devnode
problem, interrupt storm, or any acquisition/DMA activity. Recovery is the already documented
WinRE/known-package rollback or reinstallation procedure and requires its own
authorization if it causes another PnP transition or reboot.

Separate owner approvals remain required for private-key use/signing, the
instrumented driver replacement and first START, the one explicit PnP restart,
and any rollback/reboot. Driver Verifier, device handles, IOCTLs, acquisition,
DMA, arbitrary MMIO, firmware and EEPROM operations are outside this test.

## Current preparation evidence

- The owner designated this WR6k Windows installation as an expendable
  development/test environment and accepted crash, reinstallation, local-data
  loss and hardware-damage risk. Further private-key use, rollback, Driver
  Verifier, PnP transitions, device handles and acquisition remain separately
  authorized operations.
- The active service is `LecS65AcqDrv`, `Running`, `Manual`, from
  `C:\Windows\system32\drivers\LecS65AcqDrv.sys`; that file is the approved
  candidate SYS. The target devnode selects `oem17.inf` version
  `0.2026.1017.1` and reports `OK`, problem code zero.
- Numerous older development packages, including the previous `oem100.inf`,
  remain in the Driver Store. None was deleted. The candidate's independent
  Driver Store identity is the one directory whose INF/CAT/SYS all match the
  frozen hashes, not merely the package currently selected by the devnode.
- Driver Verifier has flags `0x00000000` and no selected drivers.
- Crash control is `CrashDumpEnabled=7` (Automatic Memory Dump),
  `AutoReboot=1`, `Overwrite=1`, `LogEvent=1`, with
  `C:\Windows\MEMORY.DMP`. The system has 8,458,924,032 bytes of physical
  memory, a 1,856 MiB allocated boot-volume page file and 22,305,583,104 bytes
  free on `C:`. This inventory does not prove that a kernel dump will survive
  the planned failure mode.
- Elevated read-only `reagentc /info` and `bcdedit /enum all` now confirm WinRE
  `Enabled` at
  `\\?\GLOBALROOT\device\harddisk1\partition4\Recovery\WindowsRE`, a concrete
  recovery sequence, `recoveryenabled Yes` and `testsigning Yes`. The WinRE
  partition and system volume are on healthy physical disk 1; a second healthy
  physical disk is present. An actual recovery boot/reinstall rehearsal and a
  hash-verified rollback copy outside `C:` remain unproved.

## Preserved previous package

The previously installed `oem100.inf` package was exported before installation,
without deletion or device action, to the private non-repository directory:

```text
C:\Users\LeCroyUser\.codex\driver-backups\lecroy-oem100-20261017
```

The export completed successfully and reproduced the Phase 1 hashes:

| File | Bytes | SHA-256 |
|---|---:|---|
| `lecs65acqdrv.inf` | 1,247 | `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16` |
| `LecS65AcqDrv.cat` | 2,461 | `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` |
| `LecS65AcqDrv.sys` | 67,448 | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` |

These files are private recovery material and must not be committed. Copy them
to recovery media before authorizing installation. Recompute all three hashes
from that media before relying on it.

## Rollback command sheet

Run only after separate authorization and only from an elevated console. First
verify the recovery copy against the table above. Re-adding the old INF alone
is not a reliable downgrade: [PnPUtil does not force a lower-ranked package
onto a device](https://learn.microsoft.com/windows-hardware/drivers/devtest/pnputil-command-syntax).
The primary normal-boot rollback is therefore to identify and remove only the
newly published candidate package while preserving `oem100.inf`:

```powershell
$newInf = 'oem17.inf'
if ((Get-FileHash -LiteralPath "$env:SystemRoot\INF\$newInf" `
      -Algorithm SHA256).Hash -ne `
    'C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783') {
  throw "Refusing rollback: $newInf is not the frozen candidate INF"
}
$repositoryRoot = "$env:SystemRoot\System32\DriverStore\FileRepository"
$matches = @(Get-ChildItem -LiteralPath $repositoryRoot -Directory `
  -Filter 'lecs65acqdrv.inf_amd64_*' | Where-Object {
    $inf = Get-ChildItem -LiteralPath $_.FullName -File -Filter '*.inf' |
      Select-Object -First 1
    $cat = Get-ChildItem -LiteralPath $_.FullName -File -Filter '*.cat' |
      Select-Object -First 1
    $sys = Get-ChildItem -LiteralPath $_.FullName -File -Filter '*.sys' |
      Select-Object -First 1
    $inf -and $cat -and $sys -and
      (Get-FileHash -LiteralPath $inf.FullName -Algorithm SHA256).Hash -eq
        'C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783' -and
      (Get-FileHash -LiteralPath $cat.FullName -Algorithm SHA256).Hash -eq
        'C904CCDA2A7591435F8CFB79C6728D4A44229676CBA31E21EB1353264173C091' -and
      (Get-FileHash -LiteralPath $sys.FullName -Algorithm SHA256).Hash -eq
        'F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D'
  })
if ($matches.Count -ne 1) {
  throw "Refusing rollback: expected one hash-identical candidate package"
}
pnputil.exe /delete-driver $newInf /uninstall /force
```

This command deletes the new candidate package, not `oem100.inf`, and may cause
one or more PnP transitions while Windows reselects the best remaining package.
It was not executed and must never run automatically after a failed START. If
`oem100.inf` is unexpectedly absent after candidate removal, re-add the
hash-verified recovery INF with `pnputil /add-driver <path> /install` under a
new explicit authorization. After rollback, require `oem100.inf`, version
`0.2026.930.54`, active SYS hash
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`,
device problem code zero and a running service before any XStream launch.

If Windows cannot boot normally, use the previously verified recovery path to
reach Safe Mode or WinRE. From an elevated Safe Mode console, clear Verifier:

```text
verifier.exe /reset
shutdown.exe /r /t 0
```

If normal package rollback is unavailable, reinstall Windows and import the
hash-verified previous INF from recovery media. Exact WinRE volume letters and
BCD identifiers must be captured during the recovery rehearsal; they must not
be guessed during a failure.

## Crash-dump and recovery gate

Before higher-risk runtime qualification, separately authorize and perform the
still-relevant recovery improvements below. The owner explicitly accepted
their absence for the completed first installation:

1. Copy the previous package and this command sheet to boot-independent media.
2. Run elevated `reagentc.exe /info` and `bcdedit.exe /enum all`; record the
   enabled WinRE location and the exact current/default boot identifiers.
3. Boot the recovery environment once and prove keyboard, display, Windows
   volume and recovery-media access without changing the driver.
4. Confirm a boot-volume page-file policy sufficient for a kernel dump. Do not
   infer sufficiency solely from the current 1,856 MiB allocation.
5. Set `AutoReboot=0`, retain dump logging, and choose an explicit overwrite or
   preservation policy. Save the prior CrashControl values before changing
   them.
6. Reboot only under its separate authorization, then re-read CrashControl,
   page-file and WinRE state. A registry write without a confirming reboot is
   not a passed gate.

No crash-control, page-file, BCD, WinRE or reboot change was made during the
offline preparation.

## Candidate verification gate

Workflow commit `5cc0d7921cbf05d8730b15b2736f431113010b42` produced one
frozen, locally test-signed Release x64 candidate from the exact reviewed
`cb5010a4712f25de7c56d23d0d7502683542058c` driver/include trees. Its approved
identity is `DriverVer=10/17/2026,0.2026.1017.1` and certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`. Before any future install,
recompute and compare these frozen hashes:

| Artifact | SHA-256 |
|---|---|
| INF | `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783` |
| CAT | `C904CCDA2A7591435F8CFB79C6728D4A44229676CBA31E21EB1353264173C091` |
| Signed SYS | `F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D` |
| Matching PDB | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` |
| Manifest | `CB58D910A02EA02E982D986C6761EB844DE7D0A63CF36FFE2D0CE5268BF17CA9` |

The signing workflow and independent post-run checks verified SYS and CAT
Authenticode, the local test chain, catalog membership and the signed SYS/PDB
identity. The signatures are not timestamped and rely on the explicitly trusted
local self-signed test certificate, not Microsoft production signing.
Installation was not authorized and did not occur. Continue to require all of
the following:

- exact approved `driver/` and `include/` tree IDs;
- approved Debug/Release selection and `DriverVer` ranking;
- approved exact certificate thumbprint and private-key use;
- local signature and catalog-membership verification;
- Microsoft kernel-policy verification classified accurately for the chosen
  signing method;
- the distributable directory contains exactly INF, CAT and SYS;
- Driver Verifier remains off before the first installation; and
- the old package and recovery procedure remain available.

Any mismatch creates a new candidate and returns to offline validation.

The approved candidate identity is `DriverVer=10/17/2026,0.2026.1017.1`.
It is later than the installed `09/29/2026,0.2026.930.54`, uses four components
within the Windows INF range, and does not change the public IOCTL ABI or legacy
driver-build constant.

Read-only diagnosis established that the old certificate thumbprint
`DBB8F215BE00ACA1D6F7D258773DBE8748387D90` references a missing CNG machine
key. It cannot be used for signing on this host and must not appear in a future
signing command. The separately authorized replacement certificate is
`CN=LecS65 x64 Test Phase2C 2026-10`, thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`. Its CNG container and local trust
chain are verified. The fourth one-run authorization used this identity and is
consumed. Do not rebuild or re-sign the candidate; any future signature run
would create a distinct candidate and require new explicit authorization.

The expected runtime service is `LecS65AcqDrv`, demand-start (`StartType=3`),
with image `%SystemRoot%\System32\drivers\LecS65AcqDrv.sys`. Passive
enumeration after the separately authorized first START must find exactly one
present interface for the target PCI function in each class:

```text
{958695A4-693A-435E-8297-66F805D8E46A}
{7AC34BE9-F766-4F15-9E88-854BA5E2146E}
{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}
{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}
{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}
```

## Bounded first-START procedure

This procedure requires a new explicit authorization for one installation and
one PnP START. It does not authorize XStream, a device handle, an IOCTL, an
additional restart or any acquisition.

1. Close XStream and every process that could open the driver. Confirm Verifier
   is off and capture the pre-install System and Code Integrity event-log
   bookmarks.
2. Recompute the approved candidate INF/CAT/SYS hashes in place. Stop on the
   first mismatch.
3. From one elevated console, install only the frozen INF:

   ```text
   pnputil.exe /add-driver <approved-signed-output>\package\LecS65AcqDrv.inf /install
   ```

4. Permit only the installation-triggered START. Do not issue disable, enable,
   restart, remove, rescan or reboot commands.
5. Without opening an interface, require device problem code zero, service
   `Running`, the approved SYS hash, the expected selected INF and exactly one
   interface instance in each of the five documented legacy classes.
6. Compare new System, Code Integrity and PnP events with the pre-install
   bookmarks. Stop on a signature error, resource mismatch, unexpected extra
   START, bugcheck, hang, interrupt storm, acquisition indication or dump.
7. Preserve evidence and request the next authorization. Do not retry a failed
   START. Use the rollback sheet or reimage instead.

### Exact first-install command

The frozen private backup, not a rebuilt tree, is the installation source. In
an already elevated 64-bit Windows PowerShell console, use exactly this working
directory and one PnPUtil invocation:

```powershell
$evidenceRoot = `
  'C:\Users\LeCroyUser\.codex\driver-backups\phase2c-first-install-20261018-attempt1'
if (-not (Test-Path -LiteralPath $evidenceRoot -PathType Container)) {
  throw "Run the pre-install evidence capture first: $evidenceRoot"
}
Set-Location -LiteralPath `
  'C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-SignedCandidate-20261018-fourth-1017-1\package'
$installOutput = & "$env:SystemRoot\System32\pnputil.exe" /add-driver `
  '.\LecS65AcqDrv.inf' /install 2>&1
$installExit = $LASTEXITCODE
$installOutput | Out-String | Set-Content -LiteralPath `
  (Join-Path $evidenceRoot 'pnputil-install.txt') -Encoding UTF8
$installExit | Set-Content -LiteralPath `
  (Join-Path $evidenceRoot 'pnputil-install-exit.txt') -Encoding ASCII
$installOutput
if ($installExit -ne 0) {
  throw "PnPUtil stopped with exit code $installExit; do not retry"
}
```

Do not add `/reboot`, do not run `/scan-devices`, and do not retry. Expected
effects are a new Driver Store `oem#.inf`, replacement of the selected package
for the existing PCI function, withdrawal of the old interfaces, a PnP
STOP/rebind and one candidate START, replacement/loading of the service image,
and republication of exactly one path in each of the five interface classes. A
return value requiring reboot is a stop condition, not permission to reboot.

### START hardware effects and residual risk

The candidate START is not hardware-passive. Source review confirms this
bounded sequence before interface publication:

1. map three translated PCI memory resources as logical BAR0, BAR1 and BAR2;
2. write `1` to BAR0 `START` offset `0x00C`, delay 100 microseconds and read
   back bit 0;
3. on success, pulse BAR2 `BUZZER` offset `0x000`, write `7` then `3` to BAR1
   `ITMODE` offset `0x000`, with the documented delays and a second pulse;
4. connect the translated interrupt and enable five registered interfaces.

If START bit 0 is clear, the driver emits one longer buzzer pulse, fails START
and unmaps the BARs. IRQ connection itself performs no INTEN write. The
zero-initialized interrupt shadow causes the ISR to reject all sources until a
later IOCTL explicitly establishes ownership; no such IOCTL is authorized in
this stage. `LecDmaPnpStageCreate` allocates only software state. The routine
that obtains a DMA adapter and common table, `LecDmaPnpStartNoLaunch`, has no
production call site, so first START does not activate WDM v3 DMA. These source
properties reduce scope but cannot exclude a bugcheck, hang, device-side
reaction to the fixed legacy writes, interrupt anomaly or hardware damage.

### Evidence capture and stop conditions

Immediately before an authorized install, recapture the last record ID from
`System`, `Microsoft-Windows-CodeIntegrity/Operational`,
`Microsoft-Windows-UserPnp/DeviceInstall` and
`Microsoft-Windows-Kernel-PnP/Configuration`, and copy
`%SystemRoot%\INF\setupapi.dev.log` to a new private evidence path. After the
single PnPUtil call, use only read-only `Get-PnpDevice`,
`Get-PnpDeviceProperty`, `Get-CimInstance Win32_SystemDriver`, `Get-FileHash`,
`pnputil /enum-devices ... /drivers`, `pnputil /enum-interfaces /class <GUID>`
and `Get-WinEvent` queries. Do not open an interface.

Use this new private evidence directory and pre-install capture. Refuse an
existing directory rather than overwriting an earlier attempt:

```powershell
$evidenceRoot = `
  'C:\Users\LeCroyUser\.codex\driver-backups\phase2c-first-install-20261018-attempt1'
if (Test-Path -LiteralPath $evidenceRoot) {
  throw "Evidence path already exists: $evidenceRoot"
}
New-Item -ItemType Directory -Path $evidenceRoot | Out-Null

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not [Environment]::Is64BitProcess -or
    -not $principal.IsInRole(
      [Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw 'Installation requires an already elevated 64-bit PowerShell process'
}

$candidateRoot = `
  'C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-SignedCandidate-20261018-fourth-1017-1'
$expected = [ordered]@{
  'package\LecS65AcqDrv.inf' =
    'C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783'
  'package\lecs65acqdrv.cat' =
    'C904CCDA2A7591435F8CFB79C6728D4A44229676CBA31E21EB1353264173C091'
  'package\LecS65AcqDrv.sys' =
    'F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D'
  'evidence\LecS65AcqDrv.pdb' =
    '25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774'
  'evidence\manifest.json' =
    'CB58D910A02EA02E982D986C6761EB844DE7D0A63CF36FFE2D0CE5268BF17CA9'
}
$candidateHashes = foreach ($relativePath in $expected.Keys) {
  $path = Join-Path $candidateRoot $relativePath
  $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
  if ($actual -ne $expected[$relativePath]) {
    throw "Frozen candidate mismatch: $relativePath"
  }
  [pscustomobject]@{ RelativePath = $relativePath; SHA256 = $actual }
}
$candidateHashes | Format-Table -AutoSize | Out-File `
  (Join-Path $evidenceRoot 'candidate-hashes.txt') -Encoding utf8

$sys = Join-Path $candidateRoot 'package\LecS65AcqDrv.sys'
$cat = Join-Path $candidateRoot 'package\lecs65acqdrv.cat'
$thumbprint = '4296FA2E355613DF0D5CE6F24D0898E00B706472'
$sysSignature = Get-AuthenticodeSignature -LiteralPath $sys
$catSignature = Get-AuthenticodeSignature -LiteralPath $cat
if ($sysSignature.Status -ne 'Valid' -or $catSignature.Status -ne 'Valid' -or
    $sysSignature.SignerCertificate.Thumbprint -ne $thumbprint -or
    $catSignature.SignerCertificate.Thumbprint -ne $thumbprint) {
  throw 'Frozen candidate signature identity failed'
}
$signTool = `
  'C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64\signtool.exe'
& $signTool verify /pa /q /c $cat $sys
if ($LASTEXITCODE -ne 0) { throw 'Frozen candidate CAT membership failed' }

$clients = @(Get-CimInstance Win32_Process | Where-Object {
  $_.Name -match '^(XStream|lecdiag|lecwatch|WaveStudio|ScopeExplorer)(\.exe)?$' -or
  ($_.ExecutablePath -and $_.ExecutablePath -like 'C:\Program Files\LeCroy\*')
})
if ($clients.Count -ne 0) { throw 'Known acquisition client is still running' }
verifier.exe /querysettings *> (Join-Path $evidenceRoot 'verifier-before.txt')
if ($LASTEXITCODE -ne 0 -or
    (Get-Content -LiteralPath (Join-Path $evidenceRoot 'verifier-before.txt') `
      -Raw) -notmatch '0x00000000') {
  throw 'Driver Verifier precondition failed'
}

$logs = @(
  'System',
  'Microsoft-Windows-CodeIntegrity/Operational',
  'Microsoft-Windows-UserPnp/DeviceInstall',
  'Microsoft-Windows-Kernel-PnP/Configuration'
)
$bookmarks = foreach ($log in $logs) {
  $last = Get-WinEvent -LogName $log -MaxEvents 1
  [pscustomobject]@{ Log = $log; RecordId = [long]$last.RecordId }
}
$bookmarks | ConvertTo-Json | Set-Content -LiteralPath `
  (Join-Path $evidenceRoot 'event-bookmarks.json') -Encoding UTF8
Copy-Item -LiteralPath "$env:SystemRoot\INF\setupapi.dev.log" `
  -Destination (Join-Path $evidenceRoot 'setupapi.dev.before.log')
```

Run the exact install command in the preceding section once. Preserve its full
console output and numeric exit code. If it returns zero, capture only passive
post-START evidence:

```powershell
$instanceId = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0'
Get-PnpDevice -InstanceId $instanceId | Format-List * | Out-File `
  (Join-Path $evidenceRoot 'device.txt') -Encoding utf8
@('DEVPKEY_Device_ProblemCode','DEVPKEY_Device_DriverInfPath',
  'DEVPKEY_Device_DriverVersion','DEVPKEY_Device_DriverDate',
  'DEVPKEY_Device_Service') | ForEach-Object {
    Get-PnpDeviceProperty -InstanceId $instanceId -KeyName $_
  } | Format-List * | Out-File `
    (Join-Path $evidenceRoot 'device-properties.txt') -Encoding utf8
Get-CimInstance Win32_SystemDriver -Filter "Name='LecS65AcqDrv'" |
  Format-List * | Out-File (Join-Path $evidenceRoot 'service.txt') -Encoding utf8
Get-FileHash -Algorithm SHA256 -LiteralPath `
  "$env:SystemRoot\System32\drivers\LecS65AcqDrv.sys" | Format-List * |
  Out-File (Join-Path $evidenceRoot 'loaded-sys-hash.txt') -Encoding utf8
pnputil.exe /enum-devices /instanceid $instanceId /drivers *> `
  (Join-Path $evidenceRoot 'device-drivers.txt')

$guids = @(
  '{958695A4-693A-435E-8297-66F805D8E46A}',
  '{7AC34BE9-F766-4F15-9E88-854BA5E2146E}',
  '{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}',
  '{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}',
  '{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}'
)
foreach ($guid in $guids) {
  pnputil.exe /enum-interfaces /class $guid *> `
    (Join-Path $evidenceRoot ("interface-{0}.txt" -f $guid.Trim('{}')))
}

$bookmarks = Get-Content -LiteralPath `
  (Join-Path $evidenceRoot 'event-bookmarks.json') -Raw | ConvertFrom-Json
foreach ($bookmark in $bookmarks) {
  $safeName = $bookmark.Log -replace '[^A-Za-z0-9.-]', '_'
  Get-WinEvent -LogName $bookmark.Log |
    Where-Object { $_.RecordId -gt [long]$bookmark.RecordId } |
    Sort-Object RecordId |
    Format-List TimeCreated,RecordId,Id,ProviderName,LevelDisplayName,Message |
    Out-File (Join-Path $evidenceRoot ("events-{0}.txt" -f $safeName)) `
      -Encoding utf8
}
Copy-Item -LiteralPath "$env:SystemRoot\INF\setupapi.dev.log" `
  -Destination (Join-Path $evidenceRoot 'setupapi.dev.after.log')
```

Stop without retry or automatic rollback on any candidate hash/signature
mismatch, PnPUtil nonzero or reboot-required result, unexpected selected INF,
loaded SYS hash mismatch, problem code other than zero, service not running,
missing/duplicate interface, signature or Code Integrity error, unexpected
extra PnP transition, START-probe failure, bugcheck, hang, interrupt storm,
acquisition indication or new kernel error. Preserve the attempted package's
published OEM name and all logs before proposing rollback.

## Later targeted Verifier preparation

Verifier remains a later gate. After a successful first START and a separate
authorization, the proposed bounded configuration is restricted to the exact
image name and one boot:

```text
verifier.exe /standard /driver LecS65AcqDrv.sys
verifier.exe /bootmode oneboot
verifier.exe /querysettings
shutdown.exe /r /t 0
```

The reboot is itself gated. Recovery operators must have the `verifier /reset`
sheet available before enabling this configuration. Do not select `/all`, do
not add random or systematic low-resource simulation to the initial pass, and
do not combine the first Verifier boot with acquisition, DMA or an unreviewed
PnP matrix.

## Remaining gates after first installation

1. Event 411 remains indeterminate. Before authorizing any additional PnP
   transition, approve an exact debug-capture plan that can distinguish lower
   START status, local START status and IRQ-connect status.
2. Keep rollback separately authorized. Identify `oem17.inf` through its
   published INF hash and independently matched Driver Store INF/CAT/SYS set;
   do not infer its identity solely from the currently bound device.
3. Recovery boot, external rollback media and the automatic-dump configuration
   remain unproved. No dump, page-file, BCD or reboot change was made.
4. Driver Verifier, software-only/read-only IOCTL work, controlled STOP/START,
   REMOVE, device handles, XStream, acquisition and DMA remain separate gates.

Acquisition and DMA remain a separate workstream. In particular, ordinary
kernel and PnP success must not activate the inactive WDM v3 DMA path, and
physical DMA retirement remains unproved.
