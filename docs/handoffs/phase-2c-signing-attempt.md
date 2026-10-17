# Phase 2C signed Release candidate attempts

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

## First-attempt remediation

The first one-run signing authorization was consumed and was not reused. The
PowerShell 5.1-compatible path implementation and its dual-host regression
coverage were subsequently committed as
`a911adf6d7158fb3eefa754bce0d785806275236`.

## Second authorized attempt: launcher failure before workflow entry

The owner authorized one new elevated signing attempt using workflow commit
`a911adf6d7158fb3eefa754bce0d785806275236`, reviewed driver/include commit
`cb5010a4712f25de7c56d23d0d7502683542058c`, Release x64,
`DriverVer=10/17/2026,0.2026.1017.1`, and certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`.

Immediately before the attempt, the worktree was clean at the exact approved
workflow commit. The driver tree was
`fc0e1cbca171e3aba45bd60d034b86bd63466eae` and the include tree was
`dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b`, exactly matching the reviewed
source commit. The new candidate output, run log and preservation paths did
not exist.

The elevated Windows PowerShell 5.1.19041.6456 process successfully verified:

- one matching certificate in `LocalMachine\My`;
- the exact subject `CN=LecS65 x64 Test Phase2C 2026-10` and thumbprint;
- the exact code-signing EKU and current validity;
- an offline chain build with no chain status errors; and
- elevated access to CNG container
  `te-7e799dd3-f896-4db8-b6ff-f680e0b5a7cf` through Microsoft Software Key
  Storage Provider.

The elevated one-shot launcher was intended to invoke exactly this command
after its preflight:

```powershell
& .\scripts\build-driver-package.ps1 `
  -ExpectedDriverTreeCommit cb5010a4712f25de7c56d23d0d7502683542058c `
  -RuntimeConfiguration Release `
  -DriverVer '10/17/2026,0.2026.1017.1' `
  -OutputRoot .\x64\Phase2C-SignedCandidate-20261018-1017-1 `
  -Sign `
  -CertificateThumbprint 4296FA2E355613DF0D5CE6F24D0898E00B706472
```

The attempt then failed in the external one-shot launcher before entering
`scripts/build-driver-package.ps1`. Windows PowerShell 5.1 rejected the
launcher's log-capture command because `Tee-Object -LiteralPath -Append` does
not form a valid parameter set in that host. Parameter binding happens before
the upstream command is invoked, so the package workflow did not start. This
failure is outside the corrected repository workflow and does not invalidate
its 5.1-compatible relative-path implementation.

No output root was created. There is no package INF, CAT, SYS or PDB, no
manifest, no workflow transcript and no SYS- or CAT-signing log. SignTool was
not invoked and no cryptographic signature was produced. Therefore there are
no signed-candidate artifact or manifest hashes to report.

Private evidence retained outside the repository:

| Evidence | SHA-256 |
|---|---|
| One-attempt marker | `6E0871C91C684ED44A7C95DE22B8821B8BBB3856AC8DA8328AAC9BB655280CCB` |
| Elevated run log | `13C2FC8233878ACC765FBC979927575CF8D81E22ACAB75897F16D5952A05C972` |
| Result JSON | `697D7F56E9BE85BB06DEEBAC521F5984B157F0008829FFC0008F4445D9224699` |

Post-attempt passive verification found the selected package still
`oem100.inf`, version `0.2026.930.54`, with the service still `Running` and
`Manual`. The installed and exported rollback package hashes remain:

| Artifact | SHA-256 |
|---|---|
| INF | `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16` |
| CAT | `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` |
| SYS | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` |

Certificate-store counts also remain one matching entry in each approved
store: `LocalMachine\My`, `LocalMachine\Root` and
`LocalMachine\TrustedPublisher`. No store, ACL or system configuration was
changed.

The second one-run authorization is consumed regardless of the pre-workflow
failure and must not be reused. Before any future attempt, the external
launcher must be reviewed and tested under Windows PowerShell 5.1 using a
compatible log-capture mechanism such as `Tee-Object -FilePath -Append`, with
only mocked or unsigned operations. A future real signing run requires another
new output path and a new explicit authorization.

Installation remains a separate later gate.

Before the first attempt, the available alternatives were:

1. add and qualify an explicit PowerShell 7 host requirement for elevated
   execution; or
2. replace `System.IO.Path.GetRelativePath` with reviewed Windows PowerShell
   5.1-compatible path handling and rerun the offline negative contracts.

The second attempt used the qualified second alternative; its failure was in
the external launcher rather than the repository workflow.

## Launcher qualification for a possible third attempt

The external one-shot launcher has been replaced by the tracked
`scripts/invoke-elevated-signing-workflow.ps1`. It requires an already elevated
64-bit Windows PowerShell 5.1 token and exact absolute paths and identities. It
does not elevate or retry itself. Child stdout and stderr use separate
`Start-Process` redirections, avoiding both the unsupported `Tee-Object`
parameter combination and Windows PowerShell 5.1 native-stderr ErrorRecord
conversion. Existing output, log, sidecar or marker paths are rejected, and an
exclusive marker makes each invocation one-shot.

The final elevated mock qualification passed 24/24 on Windows PowerShell
5.1.19041.6456 / CLR 4.0.30319.42000. All requested negative cases stopped
safely, exact mock child exit codes propagated, failure logs were preserved,
and no case invoked the child more than once. The unchanged package-workflow
compatibility and fail-closed suites passed 19/19 and 16/16 respectively on
both Windows PowerShell 5.1 and PowerShell 7.6.5. A non-elevated launcher call
was rejected before it created any evidence or output file.

No certificate store, private key, real signing tool, driver build,
installation, PnP or hardware path was used. The launcher is qualified for
review, but a real third attempt remains unauthorized and must use a new output
root, run log and marker named in a new explicit owner authorization.
