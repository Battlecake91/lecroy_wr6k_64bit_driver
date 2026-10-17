# Phase 2B post-merge validation

## Scope and safety result

Phase 2B validated development merge commit
`aba7ba35ab75cb3b1e20935806eb86376a790726` from
`fix/p0-irq-start-dma-gating` in a separate clean worktree on the WR6k Windows
development host. The owner's existing dirty checkout was not modified.

This phase did not create or sign a driver package, stage or install a driver,
change signing policy, load/unload/restart a driver or device, reboot, enable
Driver Verifier, open a device handle, issue an IOCTL, or perform MMIO, DMA,
acquisition, PnP, EEPROM, FPGA or interrupt activity. Ignored build outputs were
not committed.

## Exact build evidence

Both configurations used Visual Studio 2022 MSVC 14.41.34120, platform toolset
v143 and Windows SDK/WDK 10.0.28000.0.

| Output | Result | Size | SHA-256 |
|---|---|---:|---|
| Debug x64 `LecS65AcqDrv.sys` | PASS, 0 warnings, 0 errors | 104,960 | `803E3A0FA0040F644410030126F01780C503C10619B260B891143BFE46D57602` |
| Release x64 `LecS65AcqDrv.sys` | PASS, 0 warnings, 0 errors | 76,288 | `B858D8E1F8228617138626AF6FBF3099D41E4D0C8F80ED56048886C58E16C0FE` |
| x64 `lecdiag.exe` | PASS | 190,976 | `DB941639BFE68D0E24C304684E4FE6C343F76FCA517CE240ED4B67F231012A11` |
| x64 `lecwatch.exe` | PASS | 186,880 | `2EAFDE2DD4FEAB598FF11CAF14DAF6E7E170D99A419AACA24746C2C35C9E94CE` |

Both SYS files are AMD64 PE machine `0x8664`, Native subsystem, entry-point RVA
`0x1000` resolving to `DriverEntry`, and import only `ntoskrnl.exe` and
`HAL.dll`. `lecdiag` is AMD64 Windows CUI with entry-point RVA `0x3FC0` and
imports `SETUPAPI.dll` and `KERNEL32.dll`. `lecwatch` is AMD64 Windows GUI with
entry-point RVA `0x71F8` and imports `USER32.dll`, `GDI32.dll`, `SETUPAPI.dll`,
`COMCTL32.dll`, `COMDLG32.dll` and `KERNEL32.dll`.

The Phase 2B SYS hashes differ from Phase 2A despite an unchanged driver tree
and identical toolchain. `dumpbin` shows the expected cause: each link embeds a
new PE timestamp, CodeView GUID and absolute worktree-specific PDB path. Current
builds are therefore not bit-reproducible across clean worktrees. This is not a
runtime-source regression, but Phase 2C must promote one immutable hash-frozen
build rather than rebuild and assume binary identity.

## Regression evidence

| Suite | Actual result |
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
| **Default plus expanded stress** | **66,000/66,000 PASS** |

No reproducible source or regression defect was found, so no driver code or ABI
was changed.

## Integration audit

The merge diff from the previously qualified integration commit `9c97e49c` to
`aba7ba35` contains documentation and the offline interface source contract
only. There is no change under `driver/` or `include/`.

| Invariant | Result | Evidence |
|---|---|---|
| Five legacy interface GUIDs | PASS | `LECS65_INTERFACE_COUNT` is 5; the exact five constants are present; the new merged contract passes. |
| One registration per class per PCI function | PASS offline | The only repository `IoRegisterDeviceInterface` call is the five-entry loop in `AddDevice`, which runs once for each FDO/PDO. Runtime cardinality remains a Phase 2C gate. |
| Public IOCTL ABI unchanged | PASS | `driver/` and `include/` are identical to `9c97e49c`; all numeric/source ABI contracts pass. |
| No live WDM v3 activation | PASS offline | Acquisition, live PnP and IOCTL sources have no synchronous-stage/GetEx entry; only the software lifetime publication is reachable. The no-live-entry contract passes. |
| `UnknownActive` is irreversible | PASS offline | Production completion/owner state tests and default/expanded races reject reset, reuse, release and late revival. |
| INTEN ownership intact | PASS offline | 49/49 functional plus 1,000 and 10,000 RMW stress pass; source contracts retain shared synchronization and narrow teardown exceptions. |
| Surprise-removal protection intact | PASS offline | Source contract confirms the unavailable latch gates ISR and HWInt DPC hardware branches; PnP tests retain ordered admission closure and rundown. |
| Phase 1 and Phase 2A evidence retained | PASS | Both topic handoffs are present in the merge ancestry and remain linked to canonical status/interface/runtime planning. |

These are source/native-test results, not kernel scheduling, runtime interface,
PnP or physical DMA proof.

## Phase 2C preparation

`docs/kernel-runtime-qualification.md` now defines six independently authorized
stages with explicit hardware, state-change and exit gates:

1. Read-only package and test-host validation.
2. Offline package creation and test signing on a host that needs no WR6k.
3. Recovery boot and rollback proof on a disposable/restorable system.
4. One initial install and START without XStream, device handles or acquisition.
5. Enumeration-only verification of exactly five interface instances.
6. Targeted one-boot Verifier, approved software/read-only lifetime checks and
   controlled PnP matrices.

No stage was executed. Approval for one stage explicitly does not authorize the
next.

## Remaining blockers before Phase 2C execution

1. Name the disposable/restorable test host and recoverable WR6k-compatible
   hardware. The licensed production oscilloscope is excluded.
2. Assign an operator and obtain separate authorization for each of the six
   stages, including certificate/private-key use, recovery reboots, initial
   START MMIO, enumeration, Verifier, device handles and PnP transitions.
3. Provide and test a package-only build/sign workflow. The existing
   `build-sign-load-driver.ps1` is not acceptable for Stage 2 because it
   immediately installs, restarts PnP and invokes `lecdiag`.
4. Choose and record an approved `DriverVer`, signing certificate and exact
   runtime configuration; freeze one signed INF/CAT/SYS/PDB manifest by hash.
5. Prove recovery boot and actual image restore before installing the package.
6. Provide an audited interface-enumeration-only helper that never calls
   `CreateFile` or `DeviceIoControl`.
7. Keep acquisition and all DMA disabled. Physical DMA bus-idle, final PCI
   transaction ordering, IOMMU-safe PFN handling and production abort/recovery
   remain unresolved.
8. Driver Verifier, PnP, surprise removal, HLK and WHQL/WHCP remain unexecuted
   qualification gates.

Phase 2B establishes merged-head offline reproducibility of the procedure,
buildability and software contracts only. It does not qualify the driver for
installation, hardware runtime or production deployment.
