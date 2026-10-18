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

## Third authorized attempt: native stderr failure before signing

The owner authorized exactly one invocation with launcher commit
`aa009667fd426a415f13fa6d38a645953912eebe`, driver/include revision
`cb5010a4712f25de7c56d23d0d7502683542058c`, Release x64,
`DriverVer=10/17/2026,0.2026.1017.1` and certificate
`4296FA2E355613DF0D5CE6F24D0898E00B706472`. The preflight found the exact
clean HEAD, matching reviewed trees and no existing output, log, sidecar or
marker path. The launcher was invoked once from an elevated 64-bit Windows
PowerShell 5.1 process. No retry was attempted.

Debug and Release x64 builds and both utilities completed. The complete Dry
run passed 52/52 package/source contracts plus 342/342 native functional
checks, for 394/394 total. Default stress passed 5,000/5,000 DMA and
1,000/1,000 INTEN iterations; expanded stress passed 50,000/50,000 DMA and
10,000/10,000 INTEN iterations, for 66,000/66,000 total.

The build outputs existed only in the repository build directories and were
never promoted into the package. Their hashes are evidence of this attempt,
not signed-candidate hashes:

| Build output | Bytes | SHA-256 | Signature state |
|---|---:|---|---|
| Debug SYS | 104,960 | `7B2CAB2C74D67510ABE1A9079420851B8334504371ECA9EA939DF6BBD6360B3A` | NotSigned |
| Debug PDB | 561,152 | `4A80D5CF2D931EAA7822E63D49A42861BB8FC5003818A6E963C9E0475B3B15E6` | N/A |
| Release SYS | 76,288 | `7BD0FDD9A02B24CEF4531D514846A0DAAE59C9BCDBC915EB307F756AB1ECFF14` | NotSigned |
| Release PDB | 569,344 | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` | N/A |

The workflow then terminated at `Debug SYS/PDB identity`. `symchk.exe` wrote
its normal search message to stderr, which the Windows PowerShell 5.1 merged
capture expression converted into a terminating `NativeCommandError` under
`$ErrorActionPreference = 'Stop'`. This occurred before package staging and
before any certificate or private-key operation. The launcher remained blocked
after its child had ended and did not create a combined exit record. After
more than ten minutes with no child process and no log progress, only the
remaining launcher process was terminated. The one-run authorization is
consumed.

No package INF, CAT, SYS or PDB exists. No CAT, signature, catalog membership
or manifest was created, so final candidate hashes are not available. Private
failure evidence is retained with these hashes:

| Evidence | SHA-256 |
|---|---|
| Attempt marker | `3255C64BB65EE9D0A0EAF746F94FBFCA000D8A24E97C63263A0FC718D3FE07E8` |
| Empty combined run log | `E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855` |
| Child stdout sidecar | `B38880F4321FB7634960CAB58EBCE2807D1997374CE01C18277C7EB0508CB84C` |
| Child stderr sidecar | `409A868EF55FE550C8009E3CCBFF62865C2F931B273DE7A0BECA5979ECF56BA1` |
| Partial workflow transcript | `768C3C8DE365EE6344B6B1946BB3207D97AA1F010C5482515326AA42D726CF8B` |

The seven partial workflow evidence files were copied to the private
`Phase2C-SigningAttempt-20261018-third-failed-evidence` directory and verified
hash-identical. Passive post-attempt inspection confirmed that the device is
still `OK` with problem code 0, uses `oem100.inf` version `0.2026.930.54`, and
the service remains running. The installed INF/CAT/SYS hashes remain
`C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16`,
`DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18`
and `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`.

Another real attempt requires a reviewed native-stderr capture fix, a complete
Windows PowerShell 5.1 mock failure run that proves launcher return and final
log preservation, a new output/log/marker set and a new explicit owner
authorization. Installation remains separately unauthorized.

## Native-process repair and unsigned end-to-end qualification

Commit `c8cc9dd9bfda296783a3f84e475c423b8f1695cc` implements the required
repair. `scripts/native-process.ps1` is now the single native-process path for
the package tools and elevated launcher. It applies argument-by-argument
Windows quoting, concurrently drains stdout and stderr, logs them separately,
uses the real exit code, enforces an explicit timeout and assigns the child
tree to a kill-on-close Windows job. The package workflow no longer exposes
SymChk stderr to the PowerShell native pipeline. The launcher no longer uses
`Start-Process -Wait` and has explicit `Sign` and `UnsignedQualification`
modes. The unsigned mode rejects a certificate thumbprint and never supplies
`-Sign` to the child.

Focused results:

- package compatibility: 22/22 on Windows PowerShell 5.1.19041.6456 and
  22/22 on PowerShell 7.6.5;
- package negative contracts: 16/16 on each host;
- elevated same-host Windows PowerShell 5.1 launcher: 34/34;
- source/package contracts remain 52/52; and
- no test case retried a child.

The elevated launcher suite covers signed and unsigned mock construction,
diagnostic stderr with exit zero, child startup supervision, native nonzero
exit, SymChk failure, missing PDB, InfVerif and Inf2Cat failures, malformed and
mismatched manifests, log creation failure, early child exit, child timeout
and a descendant remaining after root failure. Timeout and normal root exit
both close the assigned job; the descendant test proves its child is gone
without enumerating or terminating unrelated processes. The signing-mode mock
uses the production argument construction but no certificate or key.

The actual elevated unsigned invocation used the clean fix commit, reviewed
driver/include revision `cb5010a4712f25de7c56d23d0d7502683542058c`, Release
x64 and `DriverVer=10/17/2026,0.2026.1017.1`. It completed in 64-bit Windows
PowerShell 5.1 with exit code zero:

- Debug and Release x64 plus `lecdiag` and `lecwatch` built successfully;
- 394/394 functional checks and 66,000/66,000 stress iterations passed;
- real SymChk passed both generated SYS/PDB pairs;
- real InfVerif reported the INF valid before and after CAT creation;
- real Inf2Cat completed with no errors or warnings;
- the launcher independently verified the manifest and all four artifact
  hashes; and
- the output state is `UnsignedValidationOnly`, certificate is null, SYS/CAT
  are `NotSigned`, and no SignTool evidence file exists.

| Unsigned output | Bytes | SHA-256 |
|---|---:|---|
| INF | 1,247 | `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783` |
| CAT | 1,194 | `F3C65BB6E058D0764A479A908790FC781475D928EED866D6B0FD30422CE39099` |
| Release SYS | 76,288 | `7BD0FDD9A02B24CEF4531D514846A0DAAE59C9BCDBC915EB307F756AB1ECFF14` |
| Matching PDB | 569,344 | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` |
| Manifest | 12,968 | `9EF8F13525EFC7C692C4F6295906477DEAF2A6FEF6CB4163B1FF2F2BAA4C3B35` |

Private evidence:

| Evidence | SHA-256 |
|---|---|
| Elevated launcher test transcript (34/34) | `C3D068BAEE6410E2034FD2D5A3D4085DC5A27E6DB46D583FDD3B4FE1B2D88E6B` |
| Elevated launcher result JSON | `2A9079F5AD6288F736259BC1DB390B5F54BAC99CB33F78344E6D06747DDDB49C` |
| Actual unsigned combined run log | `CD936757A5710E8B05E27BC7DC87DC7827310F3502322D25CC8DA8A7FF5790AC` |
| Actual unsigned stdout sidecar | `6EE8D12A2C0E638A9D54E867065B3F2CFF544440097AD5E28782AC73E53A86FA` |
| Actual unsigned stderr sidecar | `D0DFE3E3EE09814195FE403F1651BDC78F619B916714192EF4CE9F622335771C` |
| Actual unsigned attempt marker | `B028B51873AFB17E07A6F00A94F3AC25CEAFCE0B51D3E9F86F579014AD56C39F` |

The 38-file unsigned output was copied to the private
`Phase2C-UnsignedQualification-20261018-c8cc9dd` backup and verified
hash-identical. Passive post-run inspection still reports `oem100.inf` version
`0.2026.930.54`, device problem code 0, running `LecS65AcqDrv` service and
active SYS hash
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`.

The qualified production path is intentionally same-host Windows PowerShell
5.1. It never switches to PowerShell 7. A PowerShell 7 parent directly
supervising Windows PowerShell 5.1 can propagate an incompatible
`PSModulePath`; that cross-host arrangement is outside the production command
and remains unsupported.

No real certificate lookup, private-key operation, SignTool execution, signed
catalog membership or signed-chain validation was performed. A fourth real
signing attempt requires a new clean commit review, new output/log/marker paths
and new explicit owner authorization. Driver installation remains a separate
authorization gate.

## Fourth attempt: signed candidate produced

The owner separately authorized one run at workflow commit
`5cc0d7921cbf05d8730b15b2736f431113010b42`, reviewed driver/include revision
`cb5010a4712f25de7c56d23d0d7502683542058c`, Release x64 and
`DriverVer=10/17/2026,0.2026.1017.1`. All output, combined-log, stdout, stderr
and attempt-marker paths were new. The elevated preflight passed on 64-bit
Windows PowerShell 5.1.19041.6456 and confirmed the exact subject, thumbprint,
code-signing EKU, clean local chain, Microsoft Software Key Storage Provider,
expected machine-key container, RSA-2048 size and successful non-exporting key
open. No store or ACL was changed.

The versioned launcher ran exactly once in `Sign` mode and returned zero. It
built Debug and Release x64 plus both utilities, passed 394/394 functional
checks and 66,000/66,000 stress iterations, and signed only the intended
Release SYS and CAT. InfVerif and Inf2Cat passed. SignTool verified SYS, CAT and
catalog membership with no warnings or errors. Authenticode reports `Valid`
for both files with signer thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`; the local chain builds without
status entries. The signatures are not timestamped and use the explicitly
trusted local self-signed test chain, not Microsoft production signing. SymChk
verifies the frozen signed SYS against the frozen PDB
with RSDS GUID `22FF502A-4F06-4439-AE25-F9D18FEAF62E`, age 1.

| Frozen artifact | Bytes | SHA-256 |
|---|---:|---|
| INF | 1,247 | `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783` |
| CAT | 2,509 | `C904CCDA2A7591435F8CFB79C6728D4A44229676CBA31E21EB1353264173C091` |
| Signed SYS | 77,736 | `F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D` |
| Matching PDB | 569,344 | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` |
| Manifest | 13,975 | `CB58D910A02EA02E982D986C6761EB844DE7D0A63CF36FFE2D0CE5268BF17CA9` |

The first auxiliary post-run SymChk command used malformed `/s;path` syntax
and returned usage exit 1 without changing any file. Repeating the read-only
identity check with documented `/s path` syntax returned zero and reported
`PDB Matched: TRUE`. This was a verification-command error, not a package or
workflow failure and did not invoke signing again.

Private execution evidence:

| Evidence | SHA-256 |
|---|---|
| Elevated certificate/key preflight | `3B19C31003B18D95C2B92119B6DB45A4A23C3DF25D6C82D62ACD6DC878C68EE7` |
| Combined run log | `52A02D3F8CD48F1BF1A7398A811E0A3AE35E1A9976C882F0EA38D4C0BFAD8D5B` |
| Child stdout sidecar | `8D2FDAB45DD80459DA321FFCD1E7548D74768D6493A8FCFCF211A1A9D2DB6A45` |
| Child stderr sidecar | `D0DFE3E3EE09814195FE403F1651BDC78F619B916714192EF4CE9F622335771C` |
| Attempt marker | `2966258023447E277454DC4D03318584714394E673E557CBF4BEA824FB3ED541` |

All 53 files in the candidate tree were copied to the new private
`Phase2C-SignedCandidate-20261018-fourth-1017-1` backup and verified
hash-identical. Passive post-run inspection found device status `OK`, problem
code 0, `oem100.inf` version `0.2026.930.54`, the `LecS65AcqDrv` service still
running and active SYS SHA-256
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`.
No package was staged or installed and no PnP or service transition occurred.
The fourth signing authorization is consumed. First installation remains a
new, separate authorization gate.
