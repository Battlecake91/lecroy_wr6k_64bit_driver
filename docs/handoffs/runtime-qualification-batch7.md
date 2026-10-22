# Runtime Qualification Batch 7 handoff

## Scope and branch

Batch 7 starts from integration PR #31 commit
`1c61a5e624c7d8522af0614acbc3f8b3ffb9df2f`. Work is on
`codex/phase2c-runtime-batch7`. It performed passive host/storage inspection,
offline dump-readiness implementation and testing, an unsigned package build,
source/binary risk review and documentation. It did not change BCD, WinRE,
CrashControl, page-file, Verifier, Driver Store, PnP or hardware state.

## Passive host baseline

Private evidence root:
`C:\Users\LeCroyUser\.codex\driver-backups\Phase7-PassiveAudit-20261022`.
The runtime baseline JSON SHA-256 is
`643D7387305C0A658FC5907C4883CC5A97F1CF88FB3B65046C77836F38D9E33D`.

Verified state is Windows 10 Pro x64 build 19045, test signing active for the
current boot, Secure Boot disabled, inactive Device Guard services,
`oem101.inf` version `0.2026.1018.1`, loaded SYS SHA-256
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`,
device OK/problem code zero, running service and five active interfaces. The
observed location is PCI bus 5, device 1, function 0; BDF is observational, not
the durable identity. No target error was found since current boot.

## Recovery blocker

Only one internal GPT NVMe exists: `HFM256GDHTNG-8510B`, serial
`ACE4_2E00_9590_A2AA.`, unique ID `eui.ACE42E009590A2AA`. It contains only the
600-MiB EFI partition and C:. No removable/optical media, mapped destination,
recovery partition, off-C: rollback copy, image or VSS snapshot was found.
Therefore no copy or image was attempted. Follow
`docs/recovery-and-backup.md`; Driver Verifier remains blocked.

The crash audit JSON SHA-256 is
`CE8DF3A90B08C63D541A75838BE5AD9759E726CAF8E77ED2C7D220BB23C732FE`.
It records `CrashDumpEnabled=7`, `AutoReboot=1`, overwrite enabled, no dedicated
dump, a 1,280-MiB system-managed boot page file, 8,458,805,248 bytes RAM and
about 17.3 GiB free on C:. Proposed settings are kernel dump 2 and no automatic
reboot while preserving system-managed paging. No setting was applied.
`tests/dry/test-crash-dump-readiness.ps1` passes 13/13 mocked cases.

## Unsigned candidate

Frozen root:
`C:\Users\LeCroyUser\.codex\driver-backups\Phase7-UnsignedCandidate-20261022-1022-1-attempt4`.
The first three unique attempts stopped fail-closed before candidate completion
because of expected child-stderr propagation, a stale source-contract count and
host-stream evidence searched in the wrong log. None was reused.

The successful workflow identity is:

- workflow/source commit:
  `b1a5404dbee0d3352b9eb1b9cb6ada57fb26d6fc`;
- driver tree: `5dcd2b6e31193a20d733bfce008aeafb3fb38a71`;
- include tree: `eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`;
- Release x64, `DriverVer=10/22/2026,0.2026.1022.1`;
- `PnpStatusDiagnostics=true`, WDK 10.0.28000.0, MSVC 14.41.34120; and
- package state `UnsignedValidationOnly`.

Artifacts:

| Artifact | SHA-256 |
|---|---|
| INF | `C7F7D496A302FB53ACEC940A77F8A17F781F822EB5A34CEA11A4708B6385C3B6` |
| CAT | `813F87489BC35D53B5008A4D3BAD19D637FCAC6CFDC74EAF13EAC6FE1605CA3A` |
| Release SYS | `D236EC3809E02BD0DA0BC3A679A48E346298A1076393BA5A8812794956A70646` |
| matching PDB | `5F36EF5F238F65F17EBD22B4456523817B8A7549984304FFD317A39428A174D7` |
| manifest | `5FA35C702314A689867DAB3E27AEA83AB2EB738642C2495540BFF0C7621DAA60` |

Debug and Release build with zero warnings/errors. Regression is 397/397
functional and 66,000/66,000 stress iterations. INF/catalog, AMD64/native PE,
`DriverEntry`, imports, diagnostic marker and SYS/PDB identity pass. SYS and CAT
are intentionally `NotSigned`; no staging or installation occurred.

## Risk review

The exact driver/include trees match reviewed PR #27. Relative to the installed
diagnostic source basis, intentional changes are the process/FileObject DMA
ownership implementation: CREATE association/rollback, last-close retirement,
concurrent registration/close serialization and remove rundown. Public IOCTL
ABI, START/STOP hardware ordering, five-interface publication, interrupt
ordering and irreversible UnknownActive quarantine do not change. Dormant WDM
v3 acquisition remains unreachable. Remaining binary differences include
normal rebuild metadata, RSDS/PDB path, package version and missing signature.

## Integration order and next gates

The integrated ancestry is PR #26 -> #28 -> #29 -> #30 -> #31 -> Batch 7.
PR #31 already contains the reviewed PR #27 driver/include trees; do not merge
PR #27 again.

Smallest next authorizations are separate and ordered:

1. create official x64 Windows 10 UEFI media on a positively identified USB;
2. boot-test it once;
3. copy and verify rollback/evidence off C:;
4. create and independently restore-test an EFI+Windows system image;
5. apply and reboot-verify the dump policy, then rehearse Verifier recovery;
6. only then authorize one real signing run for the frozen source identity;
7. separately authorize installation/runtime qualification; and
8. separately authorize a one-boot non-DMA Verifier pass.

Acquisition and physical DMA remain outside all of these gates.
