# Phase 2C signed Release candidate attempt

## Authorized identity

The owner authorized exactly one elevated signing workflow run with:

| Property | Authorized value |
|---|---|
| Workflow HEAD | `471be4be2ed47ee2f110b3d0ac02b99a0c061f7c` |
| Reviewed driver/include commit | `cb5010a4712f25de7c56d23d0d7502683542058c` |
| Runtime package | Release x64 |
| `DriverVer` | `10/17/2026,0.2026.1017.1` |
| Certificate thumbprint | `4296FA2E355613DF0D5CE6F24D0898E00B706472` |
| Intended output | `x64/Phase2C-SignedCandidate-20261017-1017-1` |

Before the run, HEAD and the worktree were clean, the reviewed commit was an
ancestor, and the current driver and include tree IDs matched the reviewed
trees exactly. The output and external preservation paths did not exist. A
static audit confirmed that the workflow contains only local build, regression,
staging, signing and verification operations and no installation, PnP, device
or network path.

An elevated read-only preflight found exactly one matching certificate and
successfully opened CNG container
`te-7e799dd3-f896-4db8-b6ff-f680e0b5a7cf`, with no missing-key or access-denied
result.

## Single-run result: failed before signing

The workflow was invoked exactly once with the authorized arguments. Debug and
Release builds, the complete Dry suite and expanded stress scripts completed.
The workflow advanced through its required regression-evidence assertions,
which proves that all expected functional, 5,000/1,000 default and
50,000/10,000 expanded DMA/INTEN markers were present.

It then stopped at the first relative-path manifest operation with:

```text
Method invocation failed because [System.IO.Path] does not contain a method
named GetRelativePath.
```

The elevated launcher used Windows PowerShell `5.1.19041.6456` on .NET
Framework `4.8.9195.0`. `System.IO.Path.GetRelativePath` is available to the
previously validated PowerShell 7 runtime but not this .NET Framework host. The
failure occurred while recording the built utility path, before the package
SYS was copied and before either SignTool invocation.

The external console log is retained privately at:

```text
C:\Users\LeCroyUser\.codex\driver-backups\phase2c-signing-run-20261017.log
```

Its SHA-256 is
`285F4FFBE672BB2398B7E0D63F91F672FFBA2B2179200AE8DC2EC81F39A2881B`.

## Negative artifact proof

Post-failure inspection established all of the following:

- the intended candidate output directory does not exist;
- no package CAT exists;
- no manifest or manifest hash exists;
- no SYS- or CAT-signing log exists;
- no signed package SYS exists; and
- no backup candidate directory exists.

Consequently there are no signed INF/CAT/SYS/PDB hashes to report. The attempt
did not invoke SignTool and did not consume the private key for a cryptographic
operation.

The installed driver remains `Running`/`Manual` with SYS SHA-256
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`.
The exported rollback INF/CAT/SYS hashes remain unchanged. No installation,
Driver Store staging, PnP operation, reboot, device handle or hardware access
occurred.

## Required next decision

The one-run signing authorization is consumed and must not be reused. Before a
new attempt, either:

1. add and qualify an explicit PowerShell 7 host requirement for elevated
   execution; or
2. replace `System.IO.Path.GetRelativePath` with reviewed Windows PowerShell
   5.1-compatible path handling and rerun the offline negative contracts.

Either change requires review, a clean new output path and a new explicit owner
authorization for one signing run. Installation remains a separate later gate.
