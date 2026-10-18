# Phase 2C first-installation handoff

This handoff records the completed first controlled installation and its
passive post-install inspection. The one authorized PnPUtil invocation was
performed on 2026-10-18. No retry, rollback, reboot, Verifier action, device
handle, IOCTL, acquisition action or additional PnP command was performed.

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

## First installation result

The elevated 64-bit Windows PowerShell 5.1 process invoked PnPUtil exactly once
from 2026-10-18 16:38:08.632 +02:00 through 16:38:10.618 +02:00. PnPUtil
returned zero, imported the package as `oem17.inf` and reported installation on
the expected PCI instance. SetupAPI independently records the hash-matched
FileRepository directory
`lecs65acqdrv.inf_amd64_85c8e6ef975083de`, one `Query-remove`, one `Start`,
`Restart verified`, and final section status `SUCCESS`.

Passive post-install checks at 16:38:59 +02:00 established:

- selected INF `oem17.inf`, version `0.2026.1017.1`, device `OK`, problem code
  zero;
- service `LecS65AcqDrv` is `Running` and demand-start;
- `%SystemRoot%\INF\oem17.inf` and the matching Driver Store INF/CAT/SYS have
  the frozen candidate hashes;
- the loaded `%SystemRoot%\System32\drivers\LecS65AcqDrv.sys` has SHA-256
  `F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D`,
  valid Authenticode status and signer thumbprint
  `4296FA2E355613DF0D5CE6F24D0898E00B706472`;
- each of the five legacy interface classes exposes exactly one path for the
  expected PCI instance; and
- the preserved `oem100.inf` export remains hash-identical, known acquisition
  clients remain absent and Driver Verifier remains off.

The new event interval contains no Code Integrity event and no driver-related
Service Control Manager event. Kernel-PnP event 400 at 16:38:09.9077719
records configuration by `oem17.inf`; event 411 at 16:38:09.9371050 records
problem `0x15` (`CM_PROB_WILL_BE_REMOVED`) and status `0xC00000BB`
(`STATUS_NOT_SUPPORTED`). No event 410 occurred in the interval. SetupAPI
entered `Restarting Devices` at 16:38:09.913, lists `Query-remove`, `Start` and
`Restart verified` in that textual order, exits the block at 16:38:10.601 and
closes the section with `SUCCESS` at 16:38:10.616. The individual three lines
have no timestamps, so their exact ordering relative to event 411 is not
directly proved.

Source review finds no explicit `STATUS_NOT_SUPPORTED` return in QUERY_REMOVE,
CANCEL_REMOVE, STOP or START. QUERY_REMOVE/CANCEL_REMOVE and final STOP status
are forwarded from the lower stack. START can pass through a lower-stack
failure or a status from `IoConnectInterrupt`; its explicitly authored local
failures use other status values. Interface-enable failures cannot produce a
failed START. The retained evidence therefore cannot attribute
`STATUS_NOT_SUPPORTED` to the replacement, Windows/PnP or the lower PCI stack.
The classification is **indeterminate, requiring additional targeted
evidence**, not harmless and not a confirmed driver defect.

Private evidence is preserved under
`phase2c-first-install-20261018-attempt1`. It includes the exact PnPUtil output,
timestamps, before/after SetupAPI logs, event bookmarks and new-event extracts,
device/service properties, interface enumeration and independent Driver Store
file hashes.

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
- Immediate pre-install event record IDs were System 4828, Code Integrity
  8018, UserPnp DeviceInstall 281 and Kernel-PnP Configuration 464.

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

## Next authorization boundary

The first-install authorization is consumed. Any additional PnP transition
requires a separately approved debug-capture plan that can distinguish lower
START, local START and interrupt-connect results. Rollback, reboot, Driver
Verifier, device handles, IOCTLs,
acquisition and DMA each remain outside this result and require an explicit new
authorization. The rollback procedure must identify `oem17.inf` by its
published INF and hash-identical FileRepository contents, not merely by the
currently bound devnode.
