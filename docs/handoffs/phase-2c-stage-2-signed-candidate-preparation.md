# Phase 2C Stage 2 signed-candidate preparation handoff

## Stop point

The Release x64 candidate workflow is hardened and fully validated without
signing. The task stops before private-key use. No candidate was signed, added
to the Driver Store, installed, loaded or opened. No service, PnP, Verifier,
boot, crash-control, MMIO, interrupt, DMA, acquisition, EEPROM or FPGA state
was changed.

## Ancestry and source identity

- Development reference:
  `cb5010a4712f25de7c56d23d0d7502683542058c`
- Stage 1 commit:
  `8c5ee831a1c02170d68deea30c58a6df02bcc6df`
- Package-workflow PR #16 head:
  `1b5b6f57d9dd4eb7c74b5bf2b771c51f537008c6`
- Hardened workflow commit tested here:
  `67a3075b4061cca5c853551427484db336e3e370`
- Driver tree: `fc0e1cbca171e3aba45bd60d034b86bd63466eae`
- Include tree: `dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b`

Git proves `cb5010a` is an ancestor of Stage 1, Stage 1 is an ancestor of PR
#16, and the hardened commit is layered on PR #16. Its driver/include trees are
identical to the development reference; no research or documentation revision
can silently substitute different driver sources.

## Confirmed workflow defects fixed

1. Output creation used a check-then-create sequence. The workflow now creates
   the exact output root exclusively before creating package/evidence children.
2. Nested PowerShell build/test invocations did not explicitly reject a nonzero
   native exit code. They now check both invocation success and exit code.
3. PDB validation proved readability and name only. The workflow now pins
   `SymChk` and proves the actual CodeView/PDB identity for both SYS files.
4. EKU extraction relied on a provider-specific property that was empty on this
   host. It now parses the standard `2.5.29.37` extension directly.
5. The manifest was emitted without a final completeness contract. Required
   source, build, regression, artifact-hash and offline-safety fields are now
   checked before serialization.

The workflow still contains no installation, Driver Store, restart, device
handle or hardware path. Certificate selection remains exact-thumbprint only.

## Unsigned validation result

The proposed, not owner-approved identity is:

```text
Release | x64
DriverVer=10/17/2026,0.2026.1017.1
```

It is newer than installed `09/29/2026,0.2026.930.54`, is not future-dated,
uses valid four-part INF components and changes neither public IOCTL ABI nor
legacy driver build `1002`.

The full workflow passed with SDK/WDK 10.0.28000.0 and MSVC 14.41.34120:

| Check | Result |
|---|---|
| Debug / Release builds | PASS, 0 warnings, 0 errors |
| Functional Dry | 394/394 PASS |
| Default plus expanded stress | 66,000/66,000 PASS |
| PE / imports / entry point | PASS |
| PDB readability and exact identity | PASS for both configurations |
| InfVerif | PASS before and after catalog creation |
| Inf2Cat | PASS, zero errors and warnings |
| Signature state | SYS and CAT `NotSigned` |
| Package state | `UnsignedValidationOnly`, `InstallAuthorized=false` |

Hashes:

| Artifact | SHA-256 |
|---|---|
| Debug SYS | `7B2CAB2C74D67510ABE1A9079420851B8334504371ECA9EA939DF6BBD6360B3A` |
| Debug PDB | `4A80D5CF2D931EAA7822E63D49A42861BB8FC5003818A6E963C9E0475B3B15E6` |
| Release SYS | `7BD0FDD9A02B24CEF4531D514846A0DAAE59C9BCDBC915EB307F756AB1ECFF14` |
| Release PDB | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` |
| Staged INF | `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783` |
| Unsigned CAT | `480007AF821A7E127F1714C7C9C7F9CF05DA2748C79BB0F76F438C696866DE00` |
| Manifest | `3C748D04269F831D28CA836121F1B6C42B5999A3E675797B17CBFF7211A70821` |

The last successful unsigned output is retained under the ignored local
`x64/Phase2C-UnsignedCandidatePrep-67a3075` directory.

## Negative tests

`tests/dry/test-package-workflow-negative.ps1` passed 16/16. Every requested
negative condition was rejected:

- wrong commit and dirty worktree;
- missing pinned tool and missing SYS/PDB;
- wrong thumbprint and mocked missing private-key association;
- invalid syntax, out-of-range component and future `DriverVer`;
- existing output directory;
- an actual Debug-SYS/Release-PDB mismatch through `SymChk`;
- mocked InfVerif, Inf2Cat and signing-command failures;
- mocked invalid catalog membership; and
- incomplete manifest.

No real signing operation was used by these tests.

## Certificate metadata

Thumbprint `DBB8F215BE00ACA1D6F7D258773DBE8748387D90` matches exactly one
`LocalMachine\My` certificate. It is self-signed as `CN=LecS65 x64 Test`, has
digital-signature usage and code-signing EKU, is valid from
`2026-09-25T17:54:22Z` through `2027-09-25T18:14:22Z`, and builds its local
offline chain successfully. `HasPrivateKey=true` reports an association.

However, read-only `certutil -store` reports that the stored key set is missing.
The certificate is therefore not yet classified as signing-ready. No private
key was opened, used or exported, and no certificate or trust state changed.

## Installed baseline and recovery

The device remains started with `oem100.inf`, service `LecS65AcqDrv` in
`Running`/`Manual` state and installed SYS hash
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`.
The installed INF reports `DriverVer=09/29/2026,0.2026.930.54` and rank
`00FF0000`. The exported private recovery package remains complete and matches
the established INF/CAT/SYS hashes. It was not deleted, installed or restored.

## Commands behind later gates

The exact proposed signing command is documented in
[installation preparation](../phase-2c-installation-preparation.md). It must
not run until the owner approves the Release identity and exact thumbprint and
the missing-key-set blocker is resolved. The later installation command is
limited to the frozen signed INF with `pnputil /add-driver ... /install`; it is
a separate authorization because it can install and START the device.

The first live validation remains limited to one installation-triggered START,
passive status/service/hash/event checks and exactly one present instance in
each of the five legacy interface classes. XStream, IOCTLs and acquisition are
excluded.

## Remaining risks and gates

1. Owner approval or replacement of the proposed `DriverVer`.
2. Resolution of the contradictory private-key metadata without silently
   changing key, trust or ACL state.
3. Separate approval for one exact-thumbprint signing run into a new directory.
4. Hash/signature/catalog acceptance of the resulting signed set.
5. Recovery-console and dump/page-file preparation under their own gates.
6. A later explicit authorization naming the frozen hashes for one install and
   one START.
7. Physical DMA retirement remains unproved; acquisition and DMA remain a
   different workstream and the inactive WDM v3 path must remain unreachable.
