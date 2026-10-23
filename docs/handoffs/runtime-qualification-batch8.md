# Runtime Qualification Batch 8 handoff

## Scope

Batch 8 starts from PR #32 commit
`5fc3bd5a74caebe3369fdfad50e41886a476d9c8` on isolated branch
`codex/phase2c-runtime-batch8`. It prepares external recovery, system-image and
offline drill tooling and revalidates the frozen unsigned DMA-fixed candidate.
It performs no formatting, backup, recovery boot, BCD/CrashControl/page-file
change, reboot, Verifier activation, signing, installation, PnP or hardware
operation.

## Physical inventory and blocker

The 2026-10-23 read-only inventory found only disk 0:

- model `HFM256GDHTNG-8510B`;
- serial `ACE4_2E00_9590_A2AA.`;
- unique ID `eui.ACE42E009590A2AA`;
- NVMe, GPT, healthy, online, 256,060,514,304 bytes; and
- 600-MiB EFI partition plus the 118,728,910,848-byte NTFS C: volume.

C: had 17,276,026,880 bytes free. No USB disk, optical volume, second internal
disk or mapped network filesystem was present. Consequently no recovery medium
could be created and no off-C: rollback copy or WBAdmin image could be made.
Required hardware is a separately approved empty >=8-GB USB device for official
Windows 10 x64 media and an existing NTFS external volume with at least 128 GiB
free, preferably 256 GiB, for rollback and image storage.

Private passive evidence root:
`C:\Users\LeCroyUser\.codex\driver-backups\Phase8-PassiveAudit-20261023-final2`.

| Evidence | SHA-256 |
|---|---|
| System-image/storage audit | `2CB491C12E620EE24FCED75DFD475F6C97A7A5822A3C9D311B197571B7E79A64` |
| Crash-dump audit | `DDD09A676DCAE37947DD08BE8EA3264DDF7E271A1B2695FAC7C7B26A9A210620` |

## New recovery tooling

`scripts/manage-system-image-readiness.ps1` provides `Audit`, `Plan`, `Execute`
and `VerifyCatalog`. Production execution requires elevation, exact expected
disk and volume unique IDs, an existing healthy writable NTFS non-system target
with >=128 GiB free, `-ExecuteAcknowledged` and a new evidence directory. It
invokes one `wbadmin start backup` with C: plus `-allCritical`, never retries,
and only after success enumerates the target catalog once. It records
`RestoreValidated=false`; a catalog is not restore proof. The target guard also
rejects an existing `WindowsImageBackup` tree. Mocks pass 21/21.

`scripts/validate-recovery-layout.ps1` read-only validates distinct Windows,
EFI and recovery-media roots by their SYSTEM/kernel, Microsoft BCD, x64 UEFI
boot and boot-WIM markers. Optional rollback validation requires
`recovery-manifest.json`. It never mounts, edits BCD, loads a hive, touches the
Driver Store or reboots. Mocks pass 6/6.

The existing crash-dump workflow remains unchanged and passes 13/13. The live
configuration remains automatic dump 7, automatic reboot 1, system-managed
page file and no dedicated dump.

## Frozen unsigned candidate

No rebuild was justified. Batch 8 revalidated the frozen package at
`C:\Users\LeCroyUser\.codex\driver-backups\Phase7-UnsignedCandidate-20261022-1022-1-attempt4`:

- driver tree `5dcd2b6e31193a20d733bfce008aeafb3fb38a71`;
- include tree `eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`;
- Release x64, `DriverVer=10/22/2026,0.2026.1022.1`;
- `PnpStatusDiagnostics=true`, package state `UnsignedValidationOnly`;
- INF `C7F7D496A302FB53ACEC940A77F8A17F781F822EB5A34CEA11A4708B6385C3B6`;
- CAT `813F87489BC35D53B5008A4D3BAD19D637FCAC6CFDC74EAF13EAC6FE1605CA3A`;
- SYS `D236EC3809E02BD0DA0BC3A679A48E346298A1076393BA5A8812794956A70646`;
- PDB `5F36EF5F238F65F17EBD22B4456523817B8A7549984304FFD317A39428A174D7`;
- manifest `5FA35C702314A689867DAB3E27AEA83AB2EB738642C2495540BFF0C7621DAA60`;
- 11 `PNP_STATUS:` markers; and
- matching RSDS GUID `93FA4343-6EA4-4077-B409-D43ED110CD3F`, age 1.

SYS and CAT remain unsigned. Recorded package evidence remains 397/397
functional and 66,000/66,000 stress iterations. The current complete Dry run
also passes, including 64/64 source contracts, the new 21/21 and 6/6 recovery
suites, 13/13 dump tests, 5,000 DMA and 1,000 INTEN default stress. Public ABI,
CREATE/CLOSE/REMOVE ownership, irreversible UnknownActive and dormant WDM-v3
DMA contracts remain intact. Installed SYS
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`
and the running service are unchanged.

## PR order and next authorization gates

GitHub reports PRs #26, #27, #28, #29, #30, #31 and #32 mergeable. The intended
stack is #26 -> #28 -> #29 -> #30 -> #31 -> #32 -> Batch 8. PR #31 already
contains the exact reviewed PR #27 driver/include trees; do not merge #27 again.

Separate remaining authorizations are:

1. identify and erase one exact USB target to create official Microsoft Windows
   10 x64 UEFI media;
2. boot-test that exact medium;
3. identify an existing writable external NTFS volume and create one new
   rollback directory;
4. execute one WBAdmin image and later restore-test it on a safe target;
5. apply the dump policy, reboot and verify actual page-file/dump backing;
6. rehearse Safe Mode/offline Verifier reset and test-signing recovery;
7. authorize one private-key signing run; and only later
8. authorize installation, bounded runtime tests and one-boot non-DMA Verifier.

Each gate is independent. Acquisition and physical DMA remain excluded.
