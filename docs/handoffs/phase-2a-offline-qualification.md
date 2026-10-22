# Phase 2A offline qualification and interface reconciliation

## Scope and safety result

Phase 2A reproduced the offline qualification of integration commit
`9c97e49c7a6f3f42485e9e6f0ccca8c3b3536dea` in a clean isolated worktree on
the scope's Windows development environment. The owner's checkout and its
uncommitted changes were not used or modified.

This phase did not sign, package, stage, install, load, stop, restart or replace
a driver. It did not open a device handle or perform any IOCTL, MMIO, DMA,
acquisition, interrupt, PnP, EEPROM, FPGA, firmware or reboot operation. Build
outputs remain ignored local artifacts and are not committed.

## Build results

Both configurations used Visual Studio 2022 MSVC 14.41.34120 with Windows
SDK/WDK 10.0.28000.0 and platform toolset v143.

| Command/output | Result | Size | SHA-256 |
|---|---|---:|---|
| `scripts/build-driver.ps1 -Configuration Debug -BuildLecdiag -BuildLecwatch` | PASS, 0 warnings, 0 errors | | |
| Debug `LecS65AcqDrv.sys` | PASS | 104,960 | `00EA34C32254920068C3B14284A23A8CB7ECCDA4052563B58E5810450AAA5D42` |
| `lecdiag.exe` | PASS | 190,976 | `5B8387CC385D392442A04BA1822258C43D0DFF81A67C6889BC77DD6CEB30B85B` |
| `lecwatch.exe` | PASS | 186,880 | `F1E136B1DE14A53F280365B320AD3BB293DFF9B6569CAEC502CE01477C817AFC` |
| `scripts/build-driver.ps1 -Configuration Release` | PASS, 0 warnings, 0 errors | | |
| Release `LecS65AcqDrv.sys` | PASS | 76,288 | `ACACAFD96DA0100CFC91AE03C20651ACC4748864E9AB12A030F7AA46D3FE3619` |

`dumpbin` reports PE machine `0x8664` for both SYS files and both tools. Both
SYS files use the Native subsystem, have entry-point RVA `0x1000` resolving to
`DriverEntry`, and import only `ntoskrnl.exe` and `HAL.dll`. `lecdiag.exe` uses
the Windows CUI subsystem, entry-point RVA `0x3FC0`, and imports `SETUPAPI.dll`
and `KERNEL32.dll`. `lecwatch.exe` uses the Windows GUI subsystem, entry-point
RVA `0x71F8`, and imports `USER32.dll`, `GDI32.dll`, `SETUPAPI.dll`,
`COMCTL32.dll`, `COMDLG32.dll` and `KERNEL32.dll`.

## Hardware-independent regression

The unmodified target source first reproduced the documented baseline exactly:
389/389 functional checks, 5,000/5,000 default DMA race iterations,
1,000/1,000 default INTEN RMW iterations, 50,000/50,000 expanded DMA race
iterations and 10,000/10,000 expanded INTEN RMW iterations.

Phase 2A then added one source contract for the exact five-GUID interface set,
the count, the driver registration loop and the absence of INF `AddInterface`
registration. The final default Dry result is:

| Suite | Result |
|---|---:|
| Source/ABI contracts | 48/48 PASS |
| Descriptor layout | 13/13 PASS |
| Mapping ownership | 17/17 PASS |
| Asynchronous SG bridge | 27/27 PASS |
| Synchronous v3/PnP lifetime | 149/149 PASS |
| PnP publication | 38/38 PASS |
| PnP IRP/remove-lock | 8/8 PASS |
| DMA completion | 39/39 PASS |
| Deterministic interleavings | 2/2 PASS |
| INTEN functional | 49/49 PASS |
| **Functional total** | **390/390 PASS** |
| Default DMA stress | 5,000/5,000 PASS |
| Default INTEN stress | 1,000/1,000 PASS |
| Expanded DMA stress | 50,000/50,000 PASS |
| Expanded INTEN stress | 10,000/10,000 PASS |

The source-contract addition does not modify driver code. The expanded run was
performed against the same target driver source before that test-only addition.

## Five-interface reconciliation

All five active interface classes observed in Phase 1 are intentional legacy
compatibility surfaces:

| GUID | Role established by original-driver evidence |
|---|---|
| `{958695A4-693A-435E-8297-66F805D8E46A}` | Discovery and main acquisition/control endpoint after the original DOS-name probe fails |
| `{7AC34BE9-F766-4F15-9E88-854BA5E2146E}` | Delay/flag helper |
| `{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}` | Dallas/board identification |
| `{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}` | Trace control |
| `{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}` | Three-event registration |

Static original-x86 kernel analysis exposed the latter four consecutive
DriverWorks registrations. Passive original-XStream `NtCreateFile` tracing
exposed the fifth and proved that omitting it was a compatibility defect. The
replacement declares all five in `Driver.c`, registers each once from
`AddDevice`, enables successfully registered links only after START setup and
IRQ connection, and disables them during STOP, SURPRISE_REMOVAL and REMOVE
before resource teardown. REMOVE releases the registered link strings after
remove-lock rundown.

The INF binds the PCI hardware ID, class and service but contains no
`AddInterface` directive. It does not account for the five classes. The old
qualification expectation of one interface total was therefore a documentation
error, probably inherited from treating the acquisition device as one logical
endpoint rather than counting compatibility interface classes. The corrected
runtime expectation is one present instance in each of the five classes for
the single PCI function. Registration behavior was not changed.

## Historical PnP event classification

Phase 1 found 86 retained Kernel-PnP event 411 records, two event 219 load
warnings and four event 225 removal-veto warnings. No new event was generated.

| Evidence | Classification | Source comparison |
|---|---|---|
| Two event 411 records with problem `0x34` and status `0xC0000428`; two event 219 load warnings | Signature failure | `CM_PROB_UNSIGNED_DRIVER` / `STATUS_INVALID_IMAGE_HASH` is consistent with rejection before the local test-signing trust path was accepted. This is package/signing state, not an interface or PnP-dispatch defect. |
| 84 event 411 records with problem `0x15` and status `0xC00000BB` during repeated package churn | Expected removal-state evidence, with exact origin unproved | `CM_PROB_WILL_BE_REMOVED` is consistent with a START attempted while the devnode was already marked for removal. Neither the late-September source nor the target's own START path explicitly returns `STATUS_NOT_SUPPORTED`; it can also be propagated from the lower stack. The retained event alone cannot attribute the status to this driver's code. |
| Four event 225 warnings naming XStream | Expected removal veto | These record a user process holding the device during removal/eject attempts. They do not prove a kernel defect. |
| Latest `oem100.inf` configuration paired with a removal-state 411, no later retained 410, but current devnode `OK` and service running | Insufficient evidence | The retained logs do not reconstruct the final successful transition. Current health and historical event retention must not be merged into a claim of either failure or clean PnP behavior. |

The older source family did have independent PnP/DMA lifetime risks: it could
continue START without an interrupt, lacked complete remove-lock participation,
did not drain admitted IOCTLs and deferred work in the target ordering, and did
not retain indeterminate DMA ownership through FDO removal. The target adds
those hardening layers. That source comparison establishes risk reduction but
does not prove that any retained `0x15/0xC00000BB` event was caused by one of
those defects.

## Installed-package versus target comparison

The installed package was observed in Phase 1 only; Phase 2A did not read from
or modify it.

| Property | Installed older Debug package | New offline target builds |
|---|---|---|
| Package identity | `oem100.inf`, version `0.2026.930.54`, dated 2026-09-29 | No package generated or installed |
| SYS | 67,448 bytes, `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` | Debug and Release hashes listed above |
| INF/CAT | INF `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16`; CAT `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` | Not generated |
| Signing | Locally test-signed; not Microsoft kernel-policy signed | Unsigned build outputs; deliberately not signed |
| Exact source commit | Not embedded and therefore UNKNOWN | Exact commit `9c97e49c7a6f3f42485e9e6f0ccca8c3b3536dea` |
| Interface publication | Five classes observed live | Same exact five classes declared and registered by source |
| PnP/DMA lifetime | Predates the October hardening series | Remove locks, IOCTL drain, IRQ/DPC/timer quiescence, fail-closed IRQ START, generation-checked DMA completion and unknown-active retention are present |

The installed package's timestamp and known late-September functionality place
it in the source era after the five-interface compatibility work and before the
October PnP/DMA hardening, but they do not establish a unique Git commit. No
binary-to-commit claim is made.

Expected successful user-mode endpoint discovery and the five-class set do not
change at the target. External behavior can change deliberately at unsafe
boundaries: IRQ-less START is rejected, new IOCTL admission closes before
teardown, teardown waits for admitted work, and indeterminate DMA causes
fail-closed quarantine/restart behavior. Offline comparison cannot prove
runtime ABI equivalence, timing, status propagation or physical teardown.

## Legacy compatibility risks and installation blockers

The following gates remain before any installation or activation:

1. Use a disposable or reliably restorable Windows x64 host and recoverable
   WR6k-compatible hardware, not the licensed production oscilloscope.
2. Build the exact reviewed commit into a package, record INF/CAT/SYS hashes,
   test-sign it for that isolated host and prove the recovery path first.
3. Resolve the target INF versioning/package-generation procedure; the checked
   source INF still contains its static development `DriverVer`.
4. Confirm all five present interface instances and clean unpublication during
   controlled START/STOP/REMOVE. The offline contract cannot prove runtime
   instance cardinality or interface-enable success.
5. Run targeted one-boot Driver Verifier and the bounded PnP/remove-lock matrix
   from `kernel-runtime-qualification.md`; stop on the first unexpected event,
   verifier finding, hang or dump.
6. Keep acquisition and all DMA disabled until IRQ provenance, final PCI
   transaction ordering, IOMMU-safe mapping and a production-grade abort/idle
   protocol are established. `CompletionObserved` is not `IdleProved`.
7. Treat helper-interface routing, WOW64 structures and the deliberate absence
   of Dallas WRITE and serial FPGA/GPIO writer `0xCFDC2130` as compatibility
   boundaries, not reasons to force hazardous coverage.
8. Normal-boot production signing, HLK readiness and eventual WHQL/WHCP remain
   separate future gates.

Phase 2A establishes offline build reproducibility, software-regression
stability and the correct five-interface expectation only. It does not qualify
the target driver for installation or hardware use.
