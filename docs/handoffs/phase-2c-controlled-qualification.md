# Phase 2C controlled qualification handoff

## Scope and stop point

Offline package preparation and installation/recovery planning are complete.
The owner-designated WR6k host was not modified except for exporting the
already installed package to a private backup directory. No certificate private
key was used. No candidate was signed, staged in the Driver Store, installed,
loaded or opened. Driver Verifier, PnP state, crash control, boot state, MMIO,
interrupts, DMA, acquisition, EEPROM and FPGA state were not changed.

The current stop is the private-key/package-identity gate. Later installation,
Verifier/PnP and acquisition gates remain independent.

## Source and workflow identity

- Approved development revision:
  `cb5010a4712f25de7c56d23d0d7502683542058c`
- Approved driver tree: `fc0e1cbca171e3aba45bd60d034b86bd63466eae`
- Approved include tree: `dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b`
- Package-workflow commit used for final validation:
  `b0d3a2c933e7c0c79564dec5161e1054f04bbb6a`
- SDK/WDK: `10.0.28000.0`
- MSVC toolset directory: `14.41.34120`; CL/LINK/DUMPBIN file version
  `14.41.34123.0`

The workflow commit is layered on the approved development revision. It changes
the workflow and Dry contracts only; the recorded driver/include tree IDs are
exactly those of `cb5010a`.

## Final unsigned validation run

Command:

```powershell
.\scripts\build-driver-package.ps1 `
  -ExpectedDriverTreeCommit cb5010a4712f25de7c56d23d0d7502683542058c `
  -RuntimeConfiguration Release `
  -DriverVer '09/25/2026,0.1.0.0' `
  -OutputRoot .\x64\Phase2C-UnsignedValidation-b0d3a2c
```

`09/25/2026,0.1.0.0` is a provisional validation identity, not an approved
signed or installable identity.

Results:

| Check | Result |
|---|---|
| Debug x64 | PASS, 0 warnings, 0 errors |
| Release x64 | PASS, 0 warnings, 0 errors |
| `lecdiag` / `lecwatch` | PASS, AMD64 |
| Functional Dry contracts | 394/394 PASS |
| Default stress | 5,000/5,000 DMA; 1,000/1,000 INTEN |
| Expanded stress | 50,000/50,000 DMA; 10,000/10,000 INTEN |
| Total stress | 66,000/66,000 PASS |
| PE | Both AMD64 `0x8664`, Native, `DriverEntry`; imports only `ntoskrnl.exe` and `hal.dll` |
| PDB | Both readable; 159 indexed source files each |
| InfVerif | PASS before and after catalog generation |
| Inf2Cat | PASS, zero errors and warnings |
| SYS/CAT signature | `NotSigned` / `NotSigned` |
| Output state | `UnsignedValidationOnly`; `InstallAuthorized=false` |

Artifact hashes:

| Artifact | Bytes | SHA-256 |
|---|---:|---|
| Debug SYS | 104,960 | `AFFC2AFA2A442E8B9168C627BC0631EA253ED8429A86669DC68AC1561739A900` |
| Debug PDB | 561,152 | `0353B8DA964C2AFDA13608EE663490B0C7DF522ADEFC93EBB16FA3C40F8C638F` |
| Release SYS | 76,288 | `AF4453D2606457303C1C5689097298A495247C52E4C7A0F03DC300E6791F5BB0` |
| Release PDB | 569,344 | `CEEDF5F3E820167DBBB0E9D91E23E61C5F0C09D9A211D5FCFDEEDC150E80189E` |
| Staged INF | 1,241 | `B5F92B08F58604ACD1D8990E9C4362F6B61C76350334BAE4E37599973CD23FF5` |
| Unsigned CAT | 1,194 | `807E7869A9EC1EF143E5B89450D249307A5DF835800BD97D1C602329B1FE6104` |
| `lecdiag.exe` | 190,976 | `7E8E59082B8C8D36E6C8962E1781379C54CDADE85B3426F88060C24BE29A3381` |
| `lecwatch.exe` | 186,880 | `D4A7E92BD6F539C87ED94A83C340AD87D6107DFCD2E366445C66E8EA799BB8D5` |
| Manifest | — | `EE4416518469A8CE52D021BF242FACD22CDCD7E0949FD3A84D39934387F8D921` |

The preceding incomplete evidence directories are intentionally retained. They
record three fail-closed parser defects found during validation: PE import parsing
did not initially accept `.exe`, PDB source-count success was initially treated
as a failure, and the INF line parser did not initially tolerate CRLF. No run
reached signing or installation. The corrected final run passed end to end.

## Previous-package backup and host preparation

`pnputil /export-driver oem100.inf` completed successfully to the private
non-repository directory
`C:\Users\LeCroyUser\.codex\driver-backups\lecroy-oem100-20261017`. Its INF,
CAT and SYS hashes exactly reproduce Phase 1. The package was not deleted or
reinstalled.

Current read-only host findings:

- Driver Verifier flags are zero and no drivers are selected.
- Automatic Memory Dump is selected, automatic reboot and overwrite are on.
- `C:\Windows\MEMORY.DMP` is configured; `C:` has about 22.3 GB free.
- The allocated boot page file is 1,856 MiB for approximately 8.46 GB of RAM;
  dump sufficiency is not established.
- elevated WinRE and BCD inventory was denied in the current unelevated
  session, so recovery-console identity and rehearsal remain incomplete.

Exact rollback, crash-dump, first-START and future one-boot Verifier procedures
are in [Phase 2C installation preparation](../phase-2c-installation-preparation.md).

## Remaining gates

1. Owner approval of Release or Debug and the final `DriverVer`.
2. Owner approval naming the exact certificate thumbprint and permitting one
   private-key signing run into a new output directory.
3. Verification and acceptance of the resulting signed immutable hashes.
4. Elevated WinRE/BCD inventory, recovery-media copy and a proven recovery or
   reinstall path.
5. Separate authorization for crash-control/page-file changes and reboot.
6. Separate authorization naming the frozen package hashes for one installation
   and one initial START.
7. After first-START success, separate authorization for interface enumeration,
   then a later authorization for one-boot targeted Verifier and bounded PnP.

Acquisition and DMA are not part of the prepared runtime sequence. The inactive
WDM v3 path must remain unreachable, and no current evidence proves physical
DMA retirement.
