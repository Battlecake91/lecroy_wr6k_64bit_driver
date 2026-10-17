# Phase 1 on-device kernel and PCI baseline

## Scope and evidence boundary

This report records a read-only baseline captured on 2026-10-17 from the
licensed WR6k production oscilloscope host. The capture ended at
2026-10-17 11:02:25 +02:00. No driver installation, removal, load, unload,
restart, PnP transition, device handle open, IOCTL, MMIO access, DMA,
acquisition, EEPROM access, FPGA access, interrupt injection, Driver Verifier
change, operating-system configuration change or reboot was performed.

The evidence sources were Git metadata, file metadata and hashes, Windows CIM
and PnP properties, service and registry metadata, `pnputil` enumeration,
Windows event logs, code-integrity state, Authenticode/SignTool verification
and existing repository documentation. Physical PCI resources were enumerated
but never mapped or accessed.

Classification used below:

- **VERIFIED**: directly observed from read-only metadata or an existing,
  specifically cited repository result.
- **MISMATCH**: directly observed evidence differs from the repository target
  or another directly observed source.
- **UNKNOWN**: the permitted evidence does not establish the fact.
- **REQUIRES AUTHORIZATION**: resolving the item requires an operation outside
  this phase's read-only boundary.

## 1. Repository and working tree

### Observed checkout

| Item | Classification | Observation |
| --- | --- | --- |
| Current branch | **MISMATCH** | `codex/legacy-dma-cleanup-hardening`, not `fix/p0-irq-start-dma-gating`. |
| Current commit | **MISMATCH** | `fae8a50bb2b622628cd57e18723c3037698f06b5`. |
| Expected integration commit | **VERIFIED** | Remote `fix/p0-irq-start-dma-gating` resolves to `9c97e49c7a6f3f42485e9e6f0ccca8c3b3536dea`. |
| Commit relationship | **VERIFIED** | The observed checkout commit is the merge base and is an ancestor of `9c97e49c`; the target is three commits ahead (`0` checkout-only / `3` target-only). |
| Working tree | **MISMATCH** | Pre-existing changes were present: one deleted tracked file and three untracked paths, including generated trace-tool output. None was changed or removed. |
| Target documentation | **MISMATCH** | `docs/kernel-runtime-qualification.md` is absent at the observed checkout but present at `9c97e49c`. |

The remote target is merge commit `9c97e49c` (`Merge PR #11: qualify
Debug/Release builds and kernel test plan`). It adds the kernel-runtime plan,
Release build correction and qualification documentation on top of the
observed checkout. The baseline report branch was therefore created from the
exact target commit rather than from the dirty checkout.

### Available driver artifacts

Only a local x64 Debug output tree was present. No x64 Release output directory
or Release SYS/INF/CAT artifact was found.

| Artifact | Size | UTC modification time | SHA-256 | Classification |
| --- | ---: | --- | --- | --- |
| Loose `x64/Debug/LecS65AcqDrv.sys` | 104,448 | 2026-10-13 19:40:28 | `4E29C8E472C8E9A931F34B70B483F938F4EADC7B0E3F3DA556E09C1C031F1F68` | **VERIFIED**, unsigned local build |
| Packaged Debug `LecS65AcqDrv.sys` | 67,448 | 2026-09-29 22:54:39 | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` | **VERIFIED**, test-signed |
| Packaged Debug `LecS65AcqDrv.inf` | 1,247 | 2026-09-29 22:54:39 | `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16` | **VERIFIED** |
| Packaged Debug `LecS65AcqDrv.cat` | 2,461 | 2026-09-29 22:54:40 | `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` | **VERIFIED**, locally valid test signature |
| x64 Release package | n/a | n/a | n/a | **UNKNOWN** locally; no artifact exists in this checkout |

The loose Debug SYS is newer and does not match the packaged or installed SYS.
The packaged INF, CAT and SYS are byte-for-byte identical to the active
DriverStore package. No build was run during this phase, so the target commit's
documented clean Debug/Release build result was not independently reproduced.

### Available diagnostics

The repository contains `lecdiag` x64 and x86 builds, `lecwatch` x64, the x86
XStream trace launcher/hook, dry test programs, and PowerShell entry points for
Dry, Hardware and XStream regression. Their presence is **VERIFIED**. None was
executed because `lecdiag`, Hardware mode, XStream mode and trace injection can
open the device, issue IOCTLs or otherwise cross this phase's safety boundary.

## 2. Windows and kernel baseline

| Item | Classification | Observation |
| --- | --- | --- |
| Operating system | **VERIFIED** | Windows 10 Pro 22H2, version 10.0.19045, build 19045 |
| Architecture | **VERIFIED** | 64-bit OS on an x64-based PC |
| Last boot | **VERIFIED** | 2026-10-17 10:35:44 +02:00 |
| Uptime at final capture | **VERIFIED** | 0.44 hours |
| Secure Boot | **VERIFIED** | Disabled (`UEFISecureBootEnabled=0`) |
| Code integrity | **VERIFIED** | Enabled; kernel-reported options `0x00200203` |
| Test signing | **VERIFIED** | Enabled, read directly through `SystemCodeIntegrityInformation` |
| Kernel debug mode | **VERIFIED** | Not set in the kernel code-integrity options |
| Hypervisor | **VERIFIED** | Not present |
| VBS | **VERIFIED** | Not running (`VirtualizationBasedSecurityStatus=0`) |
| HVCI / Memory Integrity | **VERIFIED** | Not enabled or auditing in kernel code-integrity state |
| Driver Verifier | **VERIFIED** | Flags `0x00000000`; no drivers selected |
| Existing bugchecks | **VERIFIED** within retained evidence | No System event 1001 in the preceding 365 days; no `MEMORY.DMP` or Minidump directory was present |
| Detailed BCD listing | **UNKNOWN** | `bcdedit /enum all` was denied to the non-elevated inspection context. Kernel state independently establishes test-signing, debug and HVCI status. An elevated BCD export is **REQUIRES AUTHORIZATION**. |

Available toolchains are Windows SDK/WDK 10.0.26100.0 and 10.0.28000.0,
repository-local SDK/WDK packages 10.0.28000.2526, Visual Studio 2022 Community
17.14.37710.0, and MSVC x64 toolsets 14.41.34120 and 14.44.35207. This includes
the SDK/WDK and MSVC 14.41 combination cited by the target's clean-build
qualification.

## 3. PCI acquisition device

| Item | Classification | Observation |
| --- | --- | --- |
| Device | **VERIFIED** | LeCroy Acquisition Device (S65), `VEN_1570`, `DEV_0005`, `SUBSYS_00000000`, `REV_00` |
| Full instance ID | **VERIFIED** | `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0` |
| Status | **VERIFIED** | Present, `OK`, `CM_PROB_NONE`, problem code 0 |
| BDF | **VERIFIED** | PCI bus 4, device 1, function 0 |
| Location paths | **VERIFIED** | `PCIROOT(0)#PCI(1E00)#PCI(0100)` and `ACPI(_SB_)#ACPI(PCI0)#ACPI(P0P1)#PCI(0100)` |
| PnP class | **VERIFIED** | `DataAcquisition`, class GUID `{BA5FE95F-EE73-4113-8121-F38CC4FF0095}` |
| PnP service | **VERIFIED** | `LecS65AcqDrv` |

Windows reports three assigned memory resources. The WMI association order and
lengths are consistent with the repository's established BAR0/BAR1/BAR2 order:

| Logical interpretation | Assigned range | Length | Classification |
| --- | --- | ---: | --- |
| BAR0 | `0xF7CBFE00` - `0xF7CBFFFF` | `0x200` (512) | **VERIFIED** as first memory resource; no mapping performed |
| BAR1 | `0xF7CC0000` - `0xF7CFFFFF` | `0x40000` (262,144) | **VERIFIED** as second memory resource; no mapping performed |
| BAR2 | `0xF7CBFC00` - `0xF7CBFDFF` | `0x200` (512) | **VERIFIED** as third memory resource; no mapping performed |

IRQ 19 is assigned and reports `OK`, hardware resource, level 2 / trigger type
2. The Windows allocation association enumerated no other IRQ 19 consumer.
The `Shareable` property was not populated, so absence of another enumerated
consumer is **VERIFIED**, while the resource's shareability capability is
**UNKNOWN**. No MSI/MSI-X assignment was exposed by the inspected metadata.

These findings match the previously documented real-hardware observation of
BDF 4:1.0, logical lengths `0x200 / 0x40000 / 0x200`, and IRQ 19. PCI command
register and interrupt-pin values were not reread because that would require a
diagnostic device operation; their earlier documented values remain historical
evidence only.

## 4. Installed driver identity

| Item | Classification | Observation |
| --- | --- | --- |
| Service | **VERIFIED** | `LecS65AcqDrv` / display name `LecS65Driver` |
| Service state | **VERIFIED** | Running, status OK, kernel driver, manual start, exit code 0 |
| Image path | **VERIFIED** | `%SystemRoot%\System32\drivers\LecS65AcqDrv.sys` |
| Active INF | **VERIFIED** | `oem100.inf`, original name `LecS65AcqDrv.inf` |
| Provider | **VERIFIED** | `LeCroy compatibility project` |
| Driver version/date | **VERIFIED** | `0.2026.930.54`, 2026-09-29 |
| SYS size | **VERIFIED** | 67,448 bytes |
| SYS SHA-256 | **VERIFIED** | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` |
| Package match | **VERIFIED** | Active DriverStore INF, CAT and SYS match the local Debug package exactly |
| New loose Debug build match | **MISMATCH** | The active SYS does not match the 2026-10-13 loose Debug SYS |
| Target integration build match | **UNKNOWN** | No target-commit SYS hash was available; the running September package predates the October integration commit and cannot be claimed as that build |

Signature observations require a precise distinction:

- Authenticode validates the SYS and CAT against the locally trusted
  self-signed `LecS65 x64 Test` certificate.
- `Win32_PnPSignedDriver` reports `IsSigned=False` and no signer.
- `signtool verify /kp` fails because the certificate does not chain to a
  Microsoft root.
- The system currently accepts the package with test-signing enabled.

The package is therefore **VERIFIED test-signed**, but **MISMATCH** against the
project's eventual normal-boot, production-signing target. It must not be
described as Microsoft-signed, WHQL/WHCP-certified or production-qualified.

Five active public device-interface classes were enumerated without opening
them:

- `{7AC34BE9-F766-4F15-9E88-854BA5E2146E}`
- `{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}`
- `{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}`
- `{958695A4-693A-435E-8297-66F805D8E46A}`
- `{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}`

This is a **MISMATCH** with the kernel-runtime plan's future expectation of
"one device interface" after installing the exact qualification package. The
five interfaces may be deliberate compatibility aliases; this phase did not
open them or infer semantics. Their intended count must be resolved from source
and a controlled qualification install, not by changing the working scope.

## 5. Existing kernel, PnP and event-log evidence

The retained 90-day logs contain 218 device-specific events:

| Event group | Count | Interval | Classification |
| --- | ---: | --- | --- |
| Kernel-PnP configuration 400 | 110 | 2026-09-25 through 2026-09-30 | **VERIFIED**, repeated package configuration |
| Kernel-PnP successful START 410 | 11 | 2026-09-25 through 2026-09-26 | **VERIFIED**, latest success references `oem9.inf` |
| Kernel-PnP START error 411 | 86 | 2026-09-25 through 2026-09-30 | **VERIFIED**, historical errors |
| Kernel-PnP load warning 219 | 2 | 2026-09-25 | **VERIFIED**, driver-load failures |
| Kernel-PnP removal-veto warning 225 | 4 | 2026-09-25 through 2026-09-27 | **VERIFIED**, XStream terminated removal/eject attempts |
| Service installation 7045 | 4 | 2026-09-25 through 2026-09-27 | **VERIFIED** |

Two error 411 records use problem `0x34` (`CM_PROB_UNSIGNED_DRIVER`) and status
`0xC0000428` (`STATUS_INVALID_IMAGE_HASH`). Eighty-four use problem `0x15`
(`CM_PROB_WILL_BE_REMOVED`) and status `0xC00000BB`
(`STATUS_NOT_SUPPORTED`). The latest configuration record for `oem100.inf`, at
2026-09-30 00:54:42, is paired with the latter START error. No device-specific
Code Integrity Operational event was found in the same period.

This history is **MISMATCH** with the clean future qualification expectation of
one successful START and no unexpected event. It does not contradict the
current live observation that the device is present/OK and the service is
running, but there is no later event 410 for `oem100.inf` in the retained
Configuration log. The most recent successful event 410 references `oem9.inf`.
The current state and historical log therefore must be recorded separately;
neither proves correct PnP teardown or DMA safety.

No device-specific resource-assignment error, interrupt error, bugcheck,
Driver Verifier finding or crash dump was found in the inspected retained
evidence. This is not proof that no event existed before log retention.

## 6. Hardware and firmware provenance

| Finding | Classification | Evidence |
| --- | --- | --- |
| PCI board identity | **VERIFIED** | PCI ID/revision and repository topology identify the host interface as the WR6k-compatible LeCroy PCI card; the documented schematic attributes the PCI endpoint to a Spartan-IIE FPGA. |
| Acquisition board revision | **UNKNOWN** | No permitted Windows or local software record identified the fitted acquisition-board revision. The schematic model/drawing family is not proof of the installed assembly revision. |
| Running PCI FPGA revision | **UNKNOWN** for this capture | Reading `BAR0 FVER` would require a device IOCTL/MMIO path and was prohibited. Existing repository evidence records historical `FVER=2`, not a fresh observation. |
| Running acquisition FPGA revision | **UNKNOWN** for this capture | Existing trace documentation records historical `ACQFVER=3`; no permitted local record proves that the currently running image is unchanged. |
| MCU firmware/version | **UNKNOWN** | No local record identifying the installed MCU firmware/version was found. |
| Installed XStream | **VERIFIED** | LeCroy XStream DSO 8.5.1.1 is installed. |
| Firmware image inventory | **VERIFIED**, not linked to running WR6k image | The XStream installation contains 52 `.bit`/`.mcs`/`.rbf` files (2/36/14). None has `WR6k` or `WaveRunner` in its filename, and no inspected record associates one with the running PCI or acquisition FPGA. |

The three top-level image files are `LabMaster10_FPGA.mcs`, `MSO_Mag.bit` and
`MSO.bit`; their names point to other product/features and they must not be
presented as the active WR6k image. General serial-trigger images are also
present. Hashes were computed locally during inspection, but are intentionally
not promoted into the public baseline because no evidence connects them to the
running WR6k hardware. No firmware contents were copied or committed.

The actual installed PCI FPGA bitstream, acquisition FPGA bitstream, boot
source/assembly option and MCU firmware remain **UNKNOWN**. Resolving them by
register reads, configuration access, device reset or programming is
**REQUIRES AUTHORIZATION** and is not suitable for this working production
scope without a separately reviewed plan.

## 7. Comparison with repository expectations

| Repository expectation | Classification | Baseline comparison |
| --- | --- | --- |
| Real PCI device `1570:0005` at BDF 4:1.0 | **VERIFIED** | Exact match |
| Logical BAR lengths `0x200 / 0x40000 / 0x200` | **VERIFIED** | Exact match in Windows resource metadata |
| IRQ 19 | **VERIFIED** | Exact match |
| Established 11/11 hardware regression | **VERIFIED** as historical evidence only | Not rerun; current metadata is consistent with its PCI/BAR/IRQ observations |
| Target commit `9c97e49c` and clean Debug/Release qualification | **VERIFIED** in remote Git/documentation | Observed checkout and running package are not the target; no local Release artifact exists |
| Kernel-runtime qualification not yet performed | **VERIFIED** | Verifier is off; no target install or runtime matrix was performed |
| One interface after qualification install | **MISMATCH** | Five active interface classes are currently exposed |
| No unexpected START event | **MISMATCH** historically | Retained logs contain 86 START errors; current state is nevertheless healthy |
| Physical DMA bus-idle proven | **UNKNOWN**, as expected | No DMA operation was performed; healthy PnP/load state is not DMA evidence |
| `CompletionObserved` equivalent to `IdleProved` | **MISMATCH** if assumed | Repository explicitly rejects this inference; this baseline provides no contrary evidence |
| Live WDM v3 DMA activation | **UNKNOWN** from system metadata; repository says disabled at target | No device operation or trace was allowed to test entry into the stage |
| Production signing/certification | **MISMATCH** | Current package is locally test-signed and accepted only with test-signing; no Microsoft production chain or certification |

The most important safety conclusion is that this production scope currently
works in a development boot/signing configuration with an older test package.
That operational state must not be used as evidence for target-commit PnP
hardening, physical IRQ provenance, final PCI transaction ordering, DMA
retirement, IOMMU-safe addressing, safe mapping release, HLK readiness or
production deployment.

## 8. Proposed Phase 2, lowest to highest risk

Phase 2 should be split by evidence layer and performed in this order. A failed
gate stops later gates rather than widening the test.

1. **Offline exact-target build, no hardware access.** In a clean isolated
   worktree at `9c97e49c`, build Debug x64 and Release x64 with SDK/WDK
   10.0.28000.0 and MSVC 14.41.34120. Record build logs, PE architecture,
   imports and INF/CAT/SYS hashes. Do not install the result.
2. **Offline Dry regression, no hardware access.** Run the target's full Dry
   suite and stress counts, compare against the documented 389/389 functional
   result and 66,000/66,000 established stress comparison, and investigate any
   mismatch before proceeding.
3. **Elevated read-only system evidence.** Export the active BCD entries and
   relevant event logs, and capture a timestamped `pnputil`/service/package
   snapshot. This closes the only privilege-limited Phase 1 metadata gap but
   still must not open the device.
4. **Prepare a disposable, recoverable qualification host.** Use a fresh image
   or snapshot, prove recovery-console access, configure private kernel dumps,
   retain the original package and record chipset/IOMMU state. Do not use the
   licensed production oscilloscope for the kernel-runtime plan.
5. **Install the exact target package on the disposable host.** Verify hashes,
   install once, confirm a clean START, expected service/image and intended
   interface count. Stop immediately on an unexpected PnP or signature event.
6. **Targeted Driver Verifier on the disposable host.** Use one-boot Verifier
   for `LecS65AcqDrv.sys` only, with a proven recovery path. Begin with
   load/open/close and software-only controls. Preserve any dump privately and
   stop on the first finding.
7. **Safe ABI hardware batch on recoverable hardware.** With XStream closed,
   run only the documented default 11-check Hardware suite. Do not add the
   consuming error-status option. Although low impact, the batch opens the
   device and its zero-mask boundary can process an already pending source.
8. **Software-only lifetime concurrency.** Under Verifier, exercise bounded
   software-only delay requests, concurrent open/read-only-query/close,
   overlapped cancellation observation and admission closure. Do not inject an
   interrupt and do not start DMA.
9. **Orderly PnP matrix on the disposable host.** Run the documented bounded
   disable/enable, restart and orderly remove/rescan cycles while checking
   interface publication, remove-lock rundown, IRQ/DPC/timer quiescence and
   absence of post-remove MMIO.
10. **True surprise removal only on disposable hot-removal hardware.** If the
    platform cannot generate it safely, record NOT RUN. Software removal must
    not be reported as surprise-removal coverage.
11. **Software-only quarantine injection.** Use a separately reviewed test hook
    that latches unknown-active ownership before any mapping or hardware launch.
    Verify irreversible retention, then reboot/restore. Never create a real
    indeterminate DMA transfer for coverage.

Live acquisition, live legacy PFN DMA, activation of WDM v3 DMA, DMA timeout
experiments, nonzero interrupt injection, arbitrary/same-value MMIO writes,
Dallas access, serial FPGA programming, reset and firmware programming are not
part of this proposed Phase 2 on the production scope.

## 9. Explicit approval gates

The following actions require explicit owner approval before execution:

- entering Phase 2 at all, including build outputs or test-generated files;
- any elevated BCD/event-log export or other administrative collection;
- exporting/backing up the active driver package or changing crash-control
  settings;
- any driver install, replacement, start, stop, unload or PnP transition;
- any reboot, recovery-environment validation or Driver Verifier change;
- opening a device interface or issuing any IOCTL, including the default
  Hardware regression and zero-mask diagnostic;
- any XStream/acquisition test, interrupt observation test, PnP stress or
  physical removal;
- any DMA mapping, launch, timeout/quarantine experiment or test hook;
- any MMIO/register access, PCI configuration access, EEPROM access, FPGA/MCU
  access, reset or programming.

Approval alone does not make hazardous tests suitable for the licensed
production scope. The target kernel-runtime plan requires disposable,
recoverable hardware for active driver, Verifier, PnP and removal work. Dallas
write/delete, serial FPGA programming, guessed register operations and live DMA
fault experiments remain deferred even with ordinary Phase 2 approval; each
would need its own evidence-backed safety plan and appropriate disposable
hardware.

## Final baseline conclusion

The current Windows host sees a healthy WR6k PCI device with the expected
identity, BDF, three resource lengths and IRQ. The running driver is an older,
locally test-signed Debug package that exactly matches the local packaged
INF/CAT/SYS, not the newer loose Debug SYS and not a demonstrated build from
the expected integration commit. Test-signing is enabled; Secure Boot, VBS and
HVCI are disabled; Driver Verifier is inactive.

Retained logs show substantial historical install/START churn, including
unsigned-image failures and many removal-state START failures, while the live
device is presently OK. Five active interface classes are exposed. No current
metadata identifies the fitted acquisition-board revision, active FPGA images
or MCU version. Nothing in this baseline proves DMA bus-idle, physical
completion ordering or safe teardown. The next safe step is exact-target
offline build and Dry qualification, followed by active kernel/PnP work only on
a disposable, recoverable test system after explicit approval.
