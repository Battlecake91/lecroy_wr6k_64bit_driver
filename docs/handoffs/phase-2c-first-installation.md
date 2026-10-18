# Phase 2C first-installation handoff

This handoff records the completed passive gate for the first controlled
installation. No package was staged or installed and no PnP transition, device
handle, IOCTL, reboot or hardware access was performed.

## Frozen candidate

The only approved input is the private backup
`Phase2C-SignedCandidate-20261018-fourth-1017-1`, workflow commit
`5cc0d7921cbf05d8730b15b2736f431113010b42`, reviewed driver/include revision
`cb5010a4712f25de7c56d23d0d7502683542058c`, Release x64,
`DriverVer=10/17/2026,0.2026.1017.1` and certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`.

| Artifact | SHA-256 |
|---|---|
| INF | `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783` |
| CAT | `C904CCDA2A7591435F8CFB79C6728D4A44229676CBA31E21EB1353264173C091` |
| Signed SYS | `F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D` |
| Matching PDB | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` |
| Manifest | `CB58D910A02EA02E982D986C6761EB844DE7D0A63CF36FFE2D0CE5268BF17CA9` |

All five hashes, the manifest identity, the exact three-file package contents,
SYS/CAT Authenticode, signer identity, local chain and CAT membership passed on
2026-10-18. The signatures are locally trusted test signatures, not Microsoft
production signatures.

## Pre-install live baseline

- PCI instance
  `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0` is `OK`,
  problem code 0, at bus 4/device 1/function 0.
- Selected package is `oem100.inf`, version `0.2026.930.54`; service
  `LecS65AcqDrv` is `Running`, demand-start.
- Installed INF hash is
  `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16`;
  active SYS hash is
  `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`.
- Driver Verifier flags are zero with no selected drivers.
- No running `XStream`, `lecdiag`, `lecwatch`, `WaveStudio`, `ScopeExplorer` or
  process under `C:\Program Files\LeCroy` was found. This process inventory is
  not physical proof of DMA bus-idle and no device handle was opened.
- PnPUtil found one interface path for the target PCI function in each of the
  five required interface classes.
- Preflight event record IDs were System 4820, Code Integrity 7934, UserPnp
  DeviceInstall 281 and Kernel-PnP Configuration 464. These are evidence only;
  recapture fresh bookmarks immediately before an authorized install.

The exported `oem100.inf` rollback directory is complete and hash-identical:

| Rollback artifact | SHA-256 |
|---|---|
| INF | `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16` |
| CAT | `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` |
| SYS | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` |

WinRE is enabled at
`\\?\GLOBALROOT\device\harddisk1\partition4\Recovery\WindowsRE`, the BCD
recovery sequence is present and enabled, test-signing is enabled, and both
physical disks report healthy. No recovery boot was rehearsed and the verified
rollback copy remains on `C:`; no removable or optical recovery volume was
present. CrashControl remains Automatic Memory Dump with `AutoReboot=1`.

## START audit

The initial START maps three BARs and executes fixed legacy MMIO before IRQ and
interface publication: BAR0 `START+0x00C <- 1`, START readback, BAR2 buzzer
pulses, and BAR1 `ITMODE+0x000 <- 7` then `<- 3`. A clear START bit fails START.
IRQ connection performs no INTEN write; the zero interrupt shadow rejects ISR
ownership until an IOCTL changes it. The production START path has no call to
`LecDmaPnpStartNoLaunch`, so it creates no DMA adapter/common table and launches
no WDM v3 DMA. No IOCTL is authorized for this stage.

## Authorization boundary

The exact install command, corrected rollback procedure, evidence commands and
stop conditions are canonical in
`docs/phase-2c-installation-preparation.md`. A separate owner authorization
must name this frozen candidate and approve exactly one PnPUtil install plus its
installation-triggered initial START. It must also acknowledge the fixed
START/ITMODE/BUZZER writes and decide whether the untested recovery boot,
same-system-volume rollback copy and current crash-dump policy are acceptable.
