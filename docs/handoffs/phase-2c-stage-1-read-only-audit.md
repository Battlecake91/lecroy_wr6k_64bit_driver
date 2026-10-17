# Phase 2C Stage 1 read-only package and test-host audit

## Scope and safety result

This audit inspected merge commit
`cb5010a4712f25de7c56d23d0d7502683542058c` from
`fix/p0-irq-start-dma-gating` in a separate clean worktree. The commit is the
two-parent merge of `aba7ba35ab75cb3b1e20935806eb86376a790726` and
`8395e5398eb97fc204206c92b8b0b7d4bdbedaac`; the Phase 2B commit is an
ancestor. The `driver/` tree ID is
`fc0e1cbca171e3aba45bd60d034b86bd63466eae` and the `include/` tree ID is
`dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b`; both are unchanged from Phase 2B.

No driver or utility was built. No package was created, signed, staged,
installed or loaded. No certificate/key/store, signing policy, boot setting or
operating-system setting was changed. No driver/device restart, PnP transition,
reboot, Driver Verifier change, device-handle open, IOCTL, MMIO, DMA,
acquisition, EEPROM, FPGA, register or interrupt operation occurred.
`scripts/build-sign-load-driver.ps1` was read but not executed.

The accessible Windows host is the licensed production oscilloscope identified
by Phase 1, not a named disposable or proven-restorable test host. Stage 1's
exit gate is therefore **NOT MET**. The audit establishes build-host and package
readiness evidence only; it does not authorize Stage 2.

## Source and package metadata audit

The source INF is 1,240 bytes with SHA-256
`D03C43CAE73E82E2F99496D5A4FD7199996AE0E3020FC4077C2D0832D7F41E63`.
InfVerif 10.0.28000.2526 reports `INF is VALID` with exit code 0.

| Property | Verified source value |
|---|---|
| Class | `DataAcquisition`, `{BA5FE95F-EE73-4113-8121-F38CC4FF0095}` |
| Architecture | Manufacturer decoration `NTamd64.10.0`; x64-only project configurations |
| Hardware ID | `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00` |
| Provider | `LeCroy compatibility project` |
| Source `DriverVer` | `09/25/2026,0.1.0.0` (development placeholder; not an approved Stage 2 identity) |
| Catalog | `LecS65AcqDrv.cat` |
| Copied binary | `LecS65AcqDrv.sys` to driver directory 12 |
| Service | Kernel driver `LecS65AcqDrv`, demand start, normal error control, Base load-order group |
| Dependencies/coinstallers | No INF service dependency, coinstaller, property-page DLL or additional copied payload |
| Build configurations | Debug x64 (`DBG=1`, no optimization) and Release x64 (max-speed optimization); both use v143 and SDK/WDK 10.0.28000.0 |

The INF does not register device-interface classes. The driver source owns that
behavior. `AddDevice` contains the repository's only
`IoRegisterDeviceInterface` call and iterates the exact five legacy GUIDs once
per FDO/PDO. A successful START enables every successfully registered class;
STOP, SURPRISE_REMOVAL and REMOVE disable them. REMOVE frees the five link
strings. The legacy `\\.\ALADDINAcqDriver0` DOS alias is deliberately not
published because passive compatibility evidence requires XStream to fall
back to the main `{958695A4-...}` interface. The package therefore expects five
PnP interface classes, not one, and no INF change is required.

Installing the INF is not passive: it creates/updates a Driver Store package,
binds a demand-start kernel service and can cause PnP START. START maps BAR
resources, runs the legacy startup probe, connects the interrupt and publishes
interfaces. None of those effects were invoked.

## Toolchain and validation inventory

All required target tools are present. Paths are standard local installation
paths and were inspected by file metadata/hash only except for the read-only
InfVerif run.

| Component | Exact path / version | SHA-256 / readiness |
|---|---|---|
| Visual Studio | `C:\Program Files\Microsoft Visual Studio\2022\Community`, Community 2022 `17.14.37710.0` | **VERIFIED** |
| MSBuild | `C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe`, product `17.14.60+43b6357182b0684725151b8234813163afeea20e` | `9FB50C5BFD8EA1DE1C66151B25E60B7D4F4F56873618DA98ABB54090FB21AA75` |
| MSVC CL | `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\cl.exe`, `19.41.34123.0` | `5A85E24E8545C3CC58F8961020A546838ACF8F78ED7A3342472836130016F77B` |
| MSVC LINK | same v143 root, `Hostx64\x64\link.exe`, `14.41.34123.0` | `1EB109718DE427B37BD063E7E16D2AAED7F4040388D4DE64622E05C94403228C` |
| SDK/WDK | `C:\Program Files (x86)\Windows Kits\10`, Include and KM x64 library trees `10.0.28000.0`; product payload `10.0.28000.2526` | **VERIFIED** |
| SignTool | `C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x64\signtool.exe`, product `10.0.28000.2526` | `80972965E7FC311D293222B1A0E2C1BFB60F363239173964DBE2A71638314B9F`; not executed |
| Inf2Cat | `C:\Program Files (x86)\Windows Kits\10\bin\10.0.28000.0\x86\Inf2Cat.exe`, product `1.0.0112.33+12bb255d` | `82C302FC9069783674B51665CE80E769F8500DB7FC737A4B8A3773C521950B86`; not executed |
| InfVerif | `C:\Program Files (x86)\Windows Kits\10\Tools\10.0.28000.0\x64\infverif.exe`, `10.0.28000.2526` | `DE49FD7673251DC159501EC006FDA8B87350037B7EFB9B9C955BCC55E1BF012B`; source INF passed |
| PE inspection | v143 `Hostx64\x64\dumpbin.exe`, `14.41.34123.0` | `10B0C4F0FB39CD088916863092EB167010C940509C6C9A6748123CEA0943F92D` |
| Hash/signature inspection | PowerShell 7.6.5 `Get-FileHash` and `Get-AuthenticodeSignature` | **VERIFIED** |
| Source control | Git `2.55.0.windows.5` | **VERIFIED** |

MSVC 14.44 is also installed but must not be selected for this qualification.
`pnputil.exe` is present but was not invoked; it is explicitly outside Stage 2.

## Existing Phase 2B artifact evidence

The ignored Phase 2B outputs remain in their isolated worktree and reproduce
the recorded hashes:

| Artifact | Size | SHA-256 | Timestamp / identity |
|---|---:|---|---|
| Debug SYS | 104,960 | `803E3A0FA0040F644410030126F01780C503C10619B260B891143BFE46D57602` | 2026-10-17 10:17:13Z; RSDS `{6F4BEB45-2044-430E-AA19-7B64C987DF9F}`, age 1 |
| Debug PDB | 561,152 | `FC6145A80AACECA5E87061DF27467306ADEECFA52E9F5E3D11F6EDF553ECFBD7` | 2026-10-17 10:17:13Z |
| Release SYS | 76,288 | `B858D8E1F8228617138626AF6FBF3099D41E4D0C8F80ED56048886C58E16C0FE` | 2026-10-17 10:17:28Z; RSDS `{AB3C3CD6-B785-49F0-AF64-25418A98361D}`, age 1 |
| Release PDB | 569,344 | `73D048218D8DC64ED5656A2C709376A71BF1D7971D2E6A3DDB88546B61302F3B` | 2026-10-17 10:17:28Z |
| `lecdiag.exe` | 190,976 | `DB941639BFE68D0E24C304684E4FE6C343F76FCA517CE240ED4B67F231012A11` | 2026-10-17 10:17:17Z |
| `lecwatch.exe` | 186,880 | `2EAFDE2DD4FEAB598FF11CAF14DAF6E7E170D99A419AACA24746C2C35C9E94CE` | 2026-10-17 10:17:21Z |

Both SYS files are unsigned AMD64 Native images with `DriverEntry` at RVA
`0x1000`; their embedded absolute PDB paths point to the Phase 2B worktree.
There is no Phase 2B package directory, CAT or staged INF. These files were
built at `aba7ba35`, not at the exact Stage 1 target `cb5010a`. Although the
driver/include trees are identical, the current linker is not bit
reproducible and exact-commit provenance is required. The Phase 2B files are
therefore evidence, not a promotable Stage 2 package.

The working installed image remains the older 67,448-byte SYS with SHA-256
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`,
package `oem100.inf`, version `0.2026.930.54`. It is running and reports OK.
That hash differs from both Phase 2B SYS files, so installed-versus-candidate
identity is unambiguous. The installed binary is locally test-signed; the
Phase 2B binaries are unsigned.

## Existing script side-effect audit

`scripts/build-sign-load-driver.ps1` is unsuitable for Stage 2. Its modifying
or device-affecting operations are:

1. invokes `build-driver.ps1`, creating/replacing driver and `lecdiag` build
   outputs;
2. creates `x64/<Configuration>/package`;
3. deletes any existing staged INF, SYS and CAT;
4. copies the built SYS;
5. generates a wall-clock `DriverVer` and writes a staged INF;
6. signs the SYS with a subject-name-selected LocalMachine certificate;
7. runs Inf2Cat, creating a catalog;
8. signs the CAT;
9. enumerates the live WR6k devnode;
10. calls `pnputil /add-driver ... /install`;
11. restarts the device or falls back to disable/enable with delays; and
12. runs `lecdiag build` and `lecdiag pci`, which open the device and issue
    IOCTLs.

The script also requires elevation, can overwrite prior package evidence,
selects the newest matching subject rather than an exact thumbprint and derives
package identity from local/UTC time. These are fail-closed and provenance
defects for Stage 2 even before its prohibited installation tail.

## Certificate readiness

A local certificate was inspected without accessing or exporting key material:

| Property | Result |
|---|---|
| Subject/issuer | `CN=LecS65 x64 Test`, self-signed |
| Thumbprint | `DBB8F215BE00ACA1D6F7D258773DBE8748387D90` |
| Validity | 2026-09-25 17:54:22Z through 2027-09-25 18:14:22Z; currently valid |
| Purpose | Code-signing EKU `1.3.6.1.5.5.7.3.3` |
| Key | RSA 2048; private-key association reports present in `LocalMachine\My` |
| Local trust | Present in local Root and TrustedPublisher stores; offline no-revocation chain build succeeds |
| Microsoft kernel policy | Not suitable; Phase 1 `/kp` verification failed as expected for this local self-signed chain |

This is **VERIFIED metadata readiness for local test signing only**. Actual
private-key access/signing was not tested and remains **UNKNOWN**. Using the
certificate/private key is **REQUIRES AUTHORIZATION**. It cannot support a
production, WHQL/WHCP or normal Microsoft kernel-policy claim.

## Host and recovery readiness

Read-only live metadata confirms Windows 10 Pro 22H2 build 19045 x64, UEFI,
no hypervisor, VBS off and Driver Verifier flags zero with no selected driver.
The WR6k devnode is present/OK and the old driver service is running. Phase 1
already verified Secure Boot off, test-signing on and HVCI off; this
non-elevated Stage 1 process could not independently read Secure Boot or BCD.

Crash-control metadata currently selects automatic dumps, permits overwrite
and automatic reboot. Approximately 22.5 GB was free on the system volume at
inspection time. Page-file settings were not returned. `reagentc /info` and
BCD access were denied, and no restore point, image/snapshot identifier,
recovery medium, recovery owner or completed restore rehearsal was available.

| Gate | Classification |
|---|---|
| Exact source commit and clean isolated audit worktree | **VERIFIED** |
| Required offline toolchain | **VERIFIED** |
| Source INF validity and five-interface/package intent | **VERIFIED** |
| Existing coherent exact-commit INF/CAT/signed-SYS/PDB set | **UNKNOWN / absent** |
| Local test-certificate metadata | **VERIFIED** |
| Authorization to use private key or create signed files | **REQUIRES AUTHORIZATION** |
| Named disposable/restorable test host | **UNKNOWN / not provided** |
| Named recovery owner and off-host working-package backup | **UNKNOWN** |
| Recovery boot and actual restore rehearsal | **UNKNOWN / not performed** |
| Recoverable non-production WR6k-compatible PCI hardware | **UNKNOWN / not provided** |
| Production scope suitable for Phase 2C runtime work | **VERIFIED NO** |

## Stage 2 design and remaining authorizations

The exact fail-closed package-only design is now canonical in
[offline driver-package creation](../driver-package-creation.md). It pins the
source and tool identities, requires an explicit configuration, `DriverVer`
and certificate thumbprint, builds/tests the exact commit, stages into a new
non-existing directory, signs SYS then CAT, verifies catalog membership and
freezes INF/CAT/signed-SYS/PDB plus a manifest. It contains no installation,
Driver Store, PnP or device operation. It was not implemented or executed.

Before Stage 2, the owner must separately authorize:

1. the exact commit, runtime configuration and `DriverVer`;
2. use of certificate thumbprint
   `DBB8F215BE00ACA1D6F7D258773DBE8748387D90` and its private key;
3. creation of build, staged package and private evidence files; and
4. either a reviewed package-only wrapper implementing the canonical contract
   or the exact individually reviewed commands.

Before any later installation, the owner must additionally name a disposable
or proven-restorable Windows host, recovery operator, tested image/restore
route and recoverable non-production WR6k-compatible hardware. Recovery boot,
restore rehearsal, package transfer, installation, initial START, interface
enumeration, Driver Verifier and every PnP transition remain separate explicit
authorizations. The licensed production oscilloscope remains excluded.
