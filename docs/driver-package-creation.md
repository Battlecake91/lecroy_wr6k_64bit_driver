# Offline driver-package creation contract

This document is the canonical contract for Phase 2C package preparation. It
specifies a package-only build, staging, optional test-signing and verification
workflow. It does not authorize private-key use and it must not be used to
install, stage or load a driver.

The implemented entry point is `scripts/build-driver-package.ps1`. Its default
mode creates an unsigned validation set and never signs or installs it. Signing
is an explicit `-Sign` mode that additionally requires an exact certificate
thumbprint; that mode remains behind a separate owner authorization.

An instrumented PnP-status package additionally requires the explicit
`-PnpStatusDiagnostics` switch. The workflow performs clean rebuilds, passes
the property consistently to Debug and Release, verifies the `PNP_STATUS:`
marker in every selected binary and records the mode in manifest schema 2.
Omitting the switch preserves and verifies the uninstrumented default. See the
[kernel-trace capture and instrumented-package contract](phase-2c-kernel-trace-capture.md).

## Safety boundary

Stage 2 runs on an offline build/signing host and requires no WR6k hardware.
Its only permitted state changes are repository build outputs, a newly created
staging/evidence directory and use of an already installed, explicitly
authorized certificate private key.

The workflow must not invoke or contain:

- `pnputil`, Driver Store APIs, `Get-PnpDevice`, device-manager operations or
  service-control operations;
- `CreateFile` or `DeviceIoControl` against a device path, `lecdiag`,
  `lecwatch`, XStream or another program that can open the driver;
- device restart, disable, enable, remove, rescan, reboot, Driver Verifier,
  MMIO, DMA, acquisition, EEPROM, FPGA or interrupt operations;
- certificate import, creation, deletion, trust-store change, private-key ACL
  change or boot/code-integrity policy change; or
- `scripts/build-sign-load-driver.ps1`. That script continues into package
  installation, PnP restart and device access.

Run with least privilege. Do not add an administrator requirement merely to
make private-key access succeed. A key-access failure is a hard stop and must
not be repaired by changing certificate or system policy within Stage 2.

## Required immutable inputs

The operator must supply every value explicitly; there are no wall-clock or
"latest installed tool" defaults:

1. exact reviewed development commit whose `driver/` and `include/` trees are
   approved, plus the exact workflow commit layered on that revision;
2. expected clean isolated worktree identity;
3. SDK/WDK version `10.0.28000.0` and MSVC v143 toolset version
   `14.41.34120`;
4. runtime configuration, `Debug` or `Release`;
5. owner-approved `DriverVer` date and four-part version;
6. for an authorized signing run only, the exact SHA-1 certificate thumbprint
   in `LocalMachine\My`;
7. a new, non-existing output root; and
8. for an authorized signing run only, owner authorization that names the
   certificate and permits private-key use and creation of signed files.

The certificate must be currently valid, contain code-signing EKU
`1.3.6.1.5.5.7.3.3`, have an associated private key, and build successfully
under the intended local test trust policy. Subject-name selection is not
permitted because it can silently select the wrong renewed or duplicate
certificate. A local self-signed test certificate is not a Microsoft
kernel-policy or production signature.

`DriverVer` must pass INF syntax validation, must not be in the future in UTC,
and must be recorded before the run. The workflow must not synthesize it from
the invocation time. The selected version also needs an owner-reviewed ranking
policy relative to packages already present on the designated test host; the
package workflow itself must not query or modify that host.

## Fail-closed procedure

Every external command must have its exit code checked immediately. Stop at
the first mismatch, retain the incomplete evidence directory, and do not sign
or copy a later component after a failed prerequisite.

1. **Preflight source identity.** Require a clean worktree. Record the workflow
   commit and require the current `driver/` and `include/` tree IDs to equal
   those at the approved development commit. This permits the reviewed wrapper
   and documentation to be layered on the approved driver revision without
   permitting driver-source drift.
2. **Pin tools.** Resolve only the recorded absolute paths for MSBuild, CL,
   LINK, DUMPBIN, InfVerif, Inf2Cat and SignTool. Record file/product versions
   and SHA-256 hashes. Reject missing tools or a version/path/hash mismatch.
3. **Build and regress.** Build Debug x64 and Release x64 from that exact
   worktree with SDK/WDK 10.0.28000.0 and MSVC 14.41.34120. Build `lecdiag`
   and `lecwatch`, run the complete Dry suite, and run the default and expanded
   DMA/INTEN stress profiles. Require the then-current documented minimums.
   None of the user-mode tools may be executed against a device.
4. **Select one candidate.** Select only the pre-authorized Debug or Release
   SYS. Record its pre-signing SHA-256, size, AMD64 machine, Native subsystem,
   `DriverEntry`, imports, PE timestamp and RSDS GUID/age/PDB path. Hash the
   matching PDB and verify that it is readable and that the SYS embeds its RSDS
   identity. A different driver/include tree is never substituted.
5. **Create empty output.** Atomically create a new output root and fail if it
   already exists. Create separate `package` and `evidence` children. Never
   delete, clean or overwrite an existing directory. Copy only the selected
   SYS and reviewed source INF into `package`; retain the PDB in private
   `evidence`, not in the distributable package.
6. **Set the reviewed identity.** Replace exactly one `DriverVer=` line in the
   staged INF with the pre-authorized value. Require one `CatalogFile=` entry
   naming `LecS65AcqDrv.cat`, the exact WR6k hardware ID, the expected x64
   decoration, service binary and demand-start service settings. Hash the
   staged INF and run InfVerif 10.0.28000.0. Any warning or error is a stop.
7. **Validate before signing.** Recheck that `package` contains exactly
   `LecS65AcqDrv.inf` and `LecS65AcqDrv.sys`, that no CAT exists yet, and that
   the staged SYS still has the recorded pre-signing hash. Re-resolve the
   certificate by exact thumbprint and repeat the validity, EKU, private-key
   association and local-chain checks without displaying or exporting key
   material.
8. **Sign the SYS.** Use the pinned SignTool with SHA-256, `LocalMachine\My`
   and the exact thumbprint. No network timestamp is used in the strictly
   offline workflow. Record the command shape, exit code, resulting signer,
   signing time if present and post-signing SYS hash. Never treat the
   pre-signing hash as the package identity.
9. **Generate the catalog.** Run the pinned Inf2Cat only against the isolated
   `package` directory with `/os:10_X64`. Require zero errors and warnings and
   exactly one newly created `LecS65AcqDrv.cat`. Re-run InfVerif against the
   final staged INF.
10. **Sign the CAT.** Sign the exact CAT with SHA-256 and the same certificate
    thumbprint. No other files or stores may be changed.
11. **Verify coherence.** Require Authenticode verification of the staged SYS
    and CAT under the intended local test trust policy, catalog membership of
    the staged SYS, identical SYS signer identity, an INF reference to the
    exact SYS/CAT names, and unchanged SYS bytes between catalog generation
    and final verification. Record that Microsoft kernel-policy verification
    is expected to fail for the local self-signed test certificate; it must not
    be reported as production signing.
12. **Freeze evidence.** Generate a machine-readable manifest containing the
    source commit/tree IDs, configuration, `DriverVer`, complete tool
    identities, certificate thumbprint/validity/EKU, pre- and post-signing SYS
    hashes, INF/CAT/SYS/PDB hashes and sizes, PE/RSDS metadata, build/test
    totals and every verification result. Hash the manifest itself. Mark the
    set immutable in controlled storage and compare all four artifact hashes
    before every later stage.

The distributable package is exactly INF, CAT and signed SYS. The matching PDB
and manifest are controlled evidence and must remain available to the recovery
operator without being published if they contain private paths or symbols.

## Validated unsigned invocation

The hardened offline implementation was validated with the approved
development trees from `cb5010a4712f25de7c56d23d0d7502683542058c`, workflow
commit `67a3075b4061cca5c853551427484db336e3e370`, Release as the staged
runtime configuration, and the proposed but unapproved identity
`10/17/2026,0.2026.1017.1`:

```powershell
.\scripts\build-driver-package.ps1 `
  -ExpectedDriverTreeCommit cb5010a4712f25de7c56d23d0d7502683542058c `
  -RuntimeConfiguration Release `
  -DriverVer '10/17/2026,0.2026.1017.1' `
  -OutputRoot .\x64\Phase2C-UnsignedCandidatePrep-67a3075
```

The run built Debug and Release x64 with zero warnings and errors, built both
utilities, passed 394/394 functional contracts and 66,000/66,000 default plus
expanded stress iterations, validated both PE/PDB pairs, passed InfVerif, and
generated the catalog with Inf2Cat with zero errors and warnings. `SymChk`
proved the exact SYS/PDB identity for both configurations. The manifest SHA-256
is `3C748D04269F831D28CA836121F1B6C42B5999A3E675797B17CBFF7211A70821`.
The resulting SYS and CAT are both `NotSigned`; the output state is
`UnsignedValidationOnly` and is not an installation candidate.

The companion negative suite passed 16/16 rejection cases, including wrong
source identity, dirty worktree, missing tool/build output, invalid `DriverVer`,
certificate metadata failures, existing output, mismatched SYS/PDB, mocked
InfVerif/Inf2Cat/signing/catalog-membership failures and incomplete manifest.

## Supported PowerShell hosts and path containment

The package workflow supports Windows PowerShell 5.1 on .NET Framework 4.8 and
PowerShell 7 on Windows. It must be invoked explicitly with the intended host;
the workflow does not relaunch itself under another PowerShell runtime.

The first authorized signing attempt established that
`System.IO.Path.GetRelativePath` is unavailable in Windows PowerShell 5.1. The
attempt stopped after build and regression evidence collection but before
package staging or signing. No candidate was created, and that historical
failed-attempt evidence remains in
`docs/handoffs/phase-2c-signing-attempt.md`.

All manifest-relative paths now use one Windows PowerShell 5.1-compatible,
fail-closed implementation. It canonicalizes the artifact root and file path,
requires both to exist with the expected directory/file types, rejects a
different drive root, rejects sibling-prefix and parent traversal escapes, and
emits deterministic backslash-separated relative paths. It never falls back
to an absolute or guessed path.

`tests/dry/test-package-workflow-compatibility.ps1` exercises the former
failure point and the complete mocked artifact-record/manifest sequence. It
uses temporary files with spaces and special characters and covers missing
files, sibling and outside-root paths, different drives, complete and
incomplete manifests, unsigned-state enforcement, and mocked matching and
mismatched signer identities. No certificate store or private key is used.
The focused suite passed 19/19 on Windows PowerShell 5.1.19041.6456 / CLR
4.0.30319.42000 and 19/19 on PowerShell 7.6.5 / CLR 10.0.11. The existing
16-case fail-closed suite also passed on both hosts. Static parser checks passed
for the package workflow and its direct build/regression dependencies on both
hosts.

These compatibility results do not authorize signing. The earlier one-run
authorization was consumed; a new owner authorization naming the exact source,
package identity, certificate and output root is required before any real
private-key operation.

A subsequent second one-run authorization was also consumed without producing
a signature. Its elevated external launcher successfully validated the exact
certificate, local chain and CNG key access, but Windows PowerShell 5.1 rejected
the launcher's `Tee-Object -LiteralPath -Append` log-capture parameter set
before invoking this package workflow. No output root or package artifact was
created. This was a launcher defect, not a recurrence of the workflow's
relative-path defect. Any future launcher must be mock-tested on the selected
PowerShell host, and real signing again requires a new explicit authorization.

## Elevated signing launcher

The tracked production launcher is
`scripts/invoke-elevated-signing-workflow.ps1`. The previous launchers were
external Codex-generated scripts and were not repository-controlled. The
second attempt failed because its Windows PowerShell 5.1 log pipeline used the
unsupported parameter combination `Tee-Object -LiteralPath -Append` before the
package workflow could start.

Review also established a second Windows PowerShell 5.1 boundary: directly
invoking a native child with merged redirection can convert child stderr into
PowerShell ErrorRecords. With `$ErrorActionPreference = 'Stop'`, that can skip
explicit child-exit-code handling. Native tools and the launcher child now use
the shared `scripts/native-process.ps1` supervisor. It starts executables
directly without a command shell, applies Windows command-line quoting to each
argument, drains stdout and stderr independently, evaluates the actual exit
code and writes separate plus combined logs. Diagnostic stderr is not itself a
failure; a nonzero exit remains fatal. Every invocation is bounded. A Windows
job object with kill-on-close contains the assigned child tree, so timeout and
post-exit descendant cleanup cannot target unrelated processes.

The launcher requires absolute repository, workflow, output, log and attempt
marker paths. It verifies an exact clean Git HEAD, requires the workflow to be
inside the repository, requires all attempt evidence paths to remain outside
the repository, rejects an existing output or evidence path, creates an
exclusive attempt marker before starting the child, invokes the child exactly
once, and never retries. `Sign` and `UnsignedQualification` are explicit,
mutually exclusive modes; the unsigned mode rejects a thumbprint and never
passes `-Sign` to the package workflow. A log-creation failure leaves the
attempt marker in place. After a zero child exit, the launcher independently
parses the mode-specific manifest, checks the approved source, configuration
and version (plus thumbprint only in signing mode), recomputes all four
artifact hashes, verifies package contents and the manifest hash, and requires
all offline safety fields to remain false.

The proposed production form, to be run only from an already elevated 64-bit
Windows PowerShell 5.1 console after a new explicit authorization, is:

```powershell
& 'C:\absolute\repository\scripts\invoke-elevated-signing-workflow.ps1' `
  -RepositoryRoot 'C:\absolute\repository' `
  -WorkflowPath 'C:\absolute\repository\scripts\build-driver-package.ps1' `
  -ExpectedWorkflowCommit '<approved-clean-workflow-commit>' `
  -ExpectedDriverTreeCommit cb5010a4712f25de7c56d23d0d7502683542058c `
  -RuntimeConfiguration Release `
  -DriverVer '10/17/2026,0.2026.1017.1' `
  -Mode Sign `
  -CertificateThumbprint 4296FA2E355613DF0D5CE6F24D0898E00B706472 `
  -OutputRoot 'C:\new\nonexistent\candidate-output' `
  -RunLog 'C:\new\nonexistent-run.log' `
  -AttemptMarker 'C:\new\nonexistent-attempt.marker'
```

The final elevated mock qualification used Windows PowerShell 5.1.19041.6456
on CLR 4.0.30319.42000 and passed 24/24 checks. It exercised the exact launcher
with a temporary Git repository and a mock workflow that has the production
workflow's parameter surface. Coverage includes argument and working-directory
forwarding, mocked SignTool/InfVerif/Inf2Cat/Authenticode/catalog membership,
complete manifest and artifact-hash validation, separate stdout/stderr capture,
native exit codes, failure logs and no-retry behavior. Negative cases cover a
non-elevated token, PowerShell 7, missing arguments/workflow/certificate
metadata, dirty or incorrect Git state, existing output/log/marker, signing and
verification failures, malformed manifest, unexpected child termination and
log creation failure.

Private qualification evidence retained outside the repository:

| Evidence | SHA-256 |
|---|---|
| Elevated transcript | `D1003A044D760DD28C9A43DC674AC7D1BFD42A424D95A0EC2FCDCF03C1BF5F72` |
| Result JSON | `08F299DE973976DD30EB3A16CEC34640C2BA5374B4CAA997BBC0E993BE1F18CB` |

## Third authorized signing attempt

The owner authorized one production invocation at launcher commit
`aa009667fd426a415f13fa6d38a645953912eebe`, using reviewed driver/include
revision `cb5010a4712f25de7c56d23d0d7502683542058c`, Release x64,
`DriverVer=10/17/2026,0.2026.1017.1` and certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`. Preflight verified the exact
clean HEAD, identical reviewed driver/include trees and nonexistent output,
log, sidecar and attempt-marker paths.

The one invocation built Debug and Release x64 plus the utilities and passed
394/394 functional checks and 66,000/66,000 required stress iterations. It
then stopped at the first Debug SYS/PDB identity check, before creating any
package artifact. `symchk.exe` writes its search diagnostic to stderr even on
this normal validation path. In Windows PowerShell 5.1, the workflow's
`@(& $FilePath @Arguments 2>&1)` expression converted that native stderr line
to a terminating `NativeCommandError` under
`$ErrorActionPreference = 'Stop'`. Consequently, the helper never reached its
explicit native exit-code handling or wrote the intended identity log.

The workflow transcript closed at that error. No package INF, CAT, SYS or PDB
was staged; no certificate lookup, CNG private-key access, SignTool, InfVerif,
Inf2Cat, Authenticode verification, catalog-membership verification or final
manifest generation occurred. The output `package` directory is empty and
there is no `manifest.json` or `manifest.sha256`. Therefore there are no final
candidate hashes to report.

The workflow child ended, but the launcher remained blocked without combining
its sidecars or recording a child exit code. After more than ten minutes with
no child process and no file progress, the remaining launcher process was
terminated to honor the fail-on-first-error rule. No retry occurred. The empty
combined log, stdout/stderr sidecars and exclusive attempt marker were
preserved. A hash-identical copy of the seven partial workflow evidence files
is retained privately under
`Phase2C-SigningAttempt-20261018-third-failed-evidence`.

| Private evidence | SHA-256 |
|---|---|
| Attempt marker | `3255C64BB65EE9D0A0EAF746F94FBFCA000D8A24E97C63263A0FC718D3FE07E8` |
| Child stdout sidecar | `B38880F4321FB7634960CAB58EBCE2807D1997374CE01C18277C7EB0508CB84C` |
| Child stderr sidecar | `409A868EF55FE550C8009E3CCBFF62865C2F931B273DE7A0BECA5979ECF56BA1` |
| Partial workflow transcript | `768C3C8DE365EE6344B6B1946BB3207D97AA1F010C5482515326AA42D726CF8B` |

Passive post-attempt verification found `oem100.inf` version
`0.2026.930.54`, problem code 0 and the `LecS65AcqDrv` service still running.
The active and Driver Store SYS remains
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`;
the installed INF and CAT remain
`C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16`
and `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18`.
No installation, Driver Store staging, PnP transition or hardware operation
occurred.

The third authorization is consumed. Before another real attempt, native-tool
capture must be made fail-closed without treating diagnostic stderr as a
PowerShell terminating error, and the launcher must be proven to return and
preserve a final combined log when the real child fails this way. Those exact
paths require Windows PowerShell 5.1 mock and negative coverage. Private-key
use and installation remain separately gated.

## Native-process fix and elevated unsigned qualification

Commit `c8cc9dd9bfda296783a3f84e475c423b8f1695cc` resolves both defects without
changing `driver/` or `include/`. The package workflow routes Git and every
relevant native package tool through `Invoke-LecS65NativeProcess`; SymChk's
normal stderr no longer becomes a PowerShell exception. The elevated launcher
uses the same supervisor instead of `Start-Process -Wait`. A bounded wait,
concurrent stream draining and a kill-on-close job object make success,
nonzero exit, timeout and descendant cleanup deterministic. Existing log paths
remain exclusive and no retry path exists.

Focused compatibility tests pass 22/22 on Windows PowerShell 5.1.19041.6456 /
CLR 4.0.30319.42000 and 22/22 on PowerShell 7.6.5 / CLR 10.0.11. Existing
package failure contracts pass 16/16 on each host. The actual elevated
Windows PowerShell 5.1 launcher suite passes 34/34, including diagnostic stderr
with exit zero, child startup supervision, native nonzero exit, SymChk, missing
PDB, InfVerif, Inf2Cat, malformed and mismatched manifests, log creation,
timeout, exit-before-output and a surviving descendant. Every child case was
invoked at most once. The descendant case confirmed that the assigned job tree
was removed while unrelated processes were untouched. The mock `Sign` path
exercised production argument construction but used no certificate or key.

Private focused-test evidence:

| Evidence | SHA-256 |
|---|---|
| Elevated 34/34 transcript | `C3D068BAEE6410E2034FD2D5A3D4085DC5A27E6DB46D583FDD3B4FE1B2D88E6B` |
| Elevated result JSON | `2A9079F5AD6288F736259BC1DB390B5F54BAC99CB33F78344E6D06747DDDB49C` |

The same clean commit then completed one actual elevated
`UnsignedQualification` run under 64-bit Windows PowerShell 5.1. It built
Debug and Release x64 plus both utilities, passed 394/394 functional checks
and 66,000/66,000 stress iterations, ran real SymChk successfully for both
SYS/PDB pairs, validated the INF before and after catalog generation, and ran
real Inf2Cat with no errors or warnings. The launcher returned exit code zero
and independently accepted the manifest and all artifact hashes. No SignTool
operation or certificate-store lookup occurred; the manifest certificate is
null and SYS/CAT both report `NotSigned`.

| Unsigned artifact | Bytes | SHA-256 |
|---|---:|---|
| INF | 1,247 | `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783` |
| CAT | 1,194 | `F3C65BB6E058D0764A479A908790FC781475D928EED866D6B0FD30422CE39099` |
| Release SYS | 76,288 | `7BD0FDD9A02B24CEF4531D514846A0DAAE59C9BCDBC915EB307F756AB1ECFF14` |
| Matching Release PDB | 569,344 | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` |
| Manifest | 12,968 | `9EF8F13525EFC7C692C4F6295906477DEAF2A6FEF6CB4163B1FF2F2BAA4C3B35` |

The private combined elevated log hash is
`CD936757A5710E8B05E27BC7DC87DC7827310F3502322D25CC8DA8A7FF5790AC`;
the attempt marker hash is
`B028B51873AFB17E07A6F00A94F3AC25CEAFCE0B51D3E9F86F579014AD56C39F`.
A 38-file copy under `Phase2C-UnsignedQualification-20261018-c8cc9dd` was
verified hash-identical to the output tree.

The launcher intentionally supports only same-host 64-bit Windows PowerShell
5.1 execution. A PowerShell 7 parent supervising a Windows PowerShell child can
propagate an incompatible `PSModulePath`; that cross-host arrangement is not a
production path and is not qualified. The launcher never switches hosts.

This unsigned qualification does not test certificate lookup, CNG key access,
real SignTool execution, signed catalog membership or chain validation. A
future real signing attempt still requires a new output/log/marker set and new
explicit owner authorization. Installation remains a separate gate.

## Current certificate-metadata gate

Read-only metadata and key-container inspection of thumbprint
`DBB8F215BE00ACA1D6F7D258773DBE8748387D90` found the signing certificate in
`LocalMachine\My`:

- subject and issuer `CN=LecS65 x64 Test` (self-signed);
- valid from `2026-09-25T17:54:22Z` through `2027-09-25T18:14:22Z`;
- critical digital-signature key usage and code-signing EKU
  `1.3.6.1.5.5.7.3.3`;
- successful offline local-chain construction with revocation checking disabled;
- RSA 2048 public key and SHA-256/RSA certificate signature;
- CNG provider `Microsoft Software Key Storage Provider`, machine-key flag,
  `KeySpec=0` (`XCN_AT_NONE`) and referenced container
  `te-aa54bed6-ed44-4a0d-a952-18db0e1422e1`;
- `HasPrivateKey=true`, which reflects the stored `CERT_KEY_PROV_INFO` reference;
  but
- an exact read-only open of that container returns `NTE_BAD_KEYSET`, not access
  denied, and provider enumeration does not contain the referenced container.

The Microsoft Software KSP is installed and can enumerate other machine keys.
No certificate with this public key and a usable private-key association exists
in another machine or current-user store; the copies in Root, CA and Trusted
Publishers contain only the public certificate. The `LocalMachine\My` entry
therefore has a stale key-provider reference and is **not signing-ready**. This
is a missing-key case, not a wrong-store, missing-provider or ACL diagnosis.
The original key can be recovered only from an external backup or another host
that still has that exact key pair. Re-association cannot recreate it.

The preferred remediation for this disposable test environment is a new,
distinctly named self-signed code-signing certificate and CNG key pair. Its
creation, public-certificate trust installation and first signing use remain
separate owner-authorized operations. Do not bind a new key to the old
certificate or reuse its subject as the replacement identity. No key was
created, opened, exported or used during this audit, and no store or ACL was
changed. Detailed evidence and gated commands are recorded in the
[missing-key diagnosis](handoffs/phase-2c-test-signing-key-diagnosis.md).

The owner subsequently authorized certificate creation and public trust-store
installation only. One replacement certificate now exists:

- subject `CN=LecS65 x64 Test Phase2C 2026-10`;
- SHA-1 thumbprint `4296FA2E355613DF0D5CE6F24D0898E00B706472`;
- `LocalMachine\My`, Microsoft Software Key Storage Provider, RSA 2048,
  SHA-256, non-exportable machine key;
- exact code-signing EKU `1.3.6.1.5.5.7.3.3` and digital-signature key usage;
- validity `2026-10-17T00:00:00+02:00` through
  `2027-04-17T23:59:59+02:00`; and
- one public copy in each of `LocalMachine\Root` and
  `LocalMachine\TrustedPublisher`, with a successful offline chain build.

The CNG container `te-7e799dd3-f896-4db8-b6ff-f680e0b5a7cf` opens successfully
with an elevated token. The filtered non-elevated token receives
`NTE_BAD_KEYSET`; therefore the separately authorized signing workflow must run
elevated unless a future, separately approved key-ACL policy changes. No ACL was
changed and no signing operation was performed. See the
[replacement-certificate handoff](handoffs/phase-2c-new-test-certificate.md).

The first owner-authorized elevated package attempt exposed a host-runtime
precondition that the workflow did not enforce. It was launched by Windows
PowerShell 5.1 / .NET Framework 4.8, where
`[System.IO.Path]::GetRelativePath` is unavailable. Debug and Release builds and
the required Dry/stress evidence completed, but the workflow then failed at its
first relative-path manifest operation, before staging or signing the SYS.
There is no candidate, CAT, manifest or signed artifact from that attempt. Do
not retry until the PowerShell runtime requirement is made explicit or the
path handling is made compatible, reviewed and separately authorized. See the
[signing-attempt handoff](handoffs/phase-2c-signing-attempt.md).

## Qualified signed Release candidate

The fourth separately authorized real signing attempt completed successfully
on 2026-10-18. The clean workflow commit was
`5cc0d7921cbf05d8730b15b2736f431113010b42`; its `driver/` and `include/`
trees were verified equal to reviewed source revision
`cb5010a4712f25de7c56d23d0d7502683542058c`. The versioned launcher ran once
from elevated 64-bit Windows PowerShell 5.1.19041.6456 with Release x64,
`DriverVer=10/17/2026,0.2026.1017.1`, SDK/WDK 10.0.28000.0 and MSVC
14.41.34120. It returned zero and did not retry.

The elevated preflight found exactly the approved certificate in
`LocalMachine\My`, exact code-signing EKU `1.3.6.1.5.5.7.3.3`, one public copy
in each approved trust store, a clean local chain and an accessible
non-exportable RSA-2048 key in the Microsoft Software Key Storage Provider.
The workflow then passed 394/394 functional tests and all 66,000/66,000 stress
iterations, built both configurations and utilities, and signed only the
Release package SYS and CAT with thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`.

| Frozen artifact | Bytes | SHA-256 |
|---|---:|---|
| `LecS65AcqDrv.inf` | 1,247 | `C2355D1E6E78C7ECEE6D3A2BE23F64441C08667FDBF94569FBD49D6083670783` |
| `lecs65acqdrv.cat` | 2,509 | `C904CCDA2A7591435F8CFB79C6728D4A44229676CBA31E21EB1353264173C091` |
| Signed `LecS65AcqDrv.sys` | 77,736 | `F760E77BF4DF973379207F2130362E6E16A05FF17C3F4B81441CC0802B02F25D` |
| Matching `LecS65AcqDrv.pdb` | 569,344 | `25B08D57D53C8584CF1076639599A03D112126A66131F7637218210E69B31774` |
| `manifest.json` | 13,975 | `CB58D910A02EA02E982D986C6761EB844DE7D0A63CF36FFE2D0CE5268BF17CA9` |

Independent post-run checks recomputed every manifest artifact hash, verified
valid Authenticode signatures on both SYS and CAT, rebuilt the local
certificate chain, and repeated SignTool SYS, CAT and catalog-membership
verification with zero errors. SymChk matched the frozen signed SYS to the
frozen PDB with RSDS GUID `22FF502A-4F06-4439-AE25-F9D18FEAF62E`, age 1.
The signatures are not timestamped and validate through the explicitly trusted
local self-signed test certificate; this is not Microsoft production signing.
The complete 53-file candidate tree was copied to a new private backup and all
source/backup hashes matched. The manifest records `InstallAuthorized=false`.

Passive post-run inspection still found device problem code 0,
`oem100.inf` version `0.2026.930.54`, the `LecS65AcqDrv` service running and
active SYS SHA-256
`4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687`.
No Driver Store, PnP, service, boot-policy or certificate-store change was
performed. Installation remains a separate explicit authorization gate.

## Qualified signed instrumented Release candidate

One later, separately authorized run used workflow commit
`b13f6637e5dc8499244474ad96a9c533be914904`, the reviewed diagnostic
driver/include basis `8dfdd69664ff5598d4708d2df8e083002bd5813b`, Release x64,
`DriverVer=10/18/2026,0.2026.1018.1` and explicit
`-PnpStatusDiagnostics`. Elevated CNG access, EKU and local chain validation
passed before the versioned launcher invoked the workflow exactly once.

The clean run passed 397/397 functional checks and 66,000/66,000 stress
iterations. Manifest schema 2 records diagnostics enabled for both builds and
the signed Release SYS contains the required `PNP_STATUS:` markers. Frozen
hashes are INF
`ADF7E71FBA10D0A7A99216578261CC3AA87EAF3459CFB08A456D808A9350169C`,
CAT `2354CDE2474F01CB32C10937DBB44775980A9126FF02CE58E5BB8E3EA7836FBF`,
signed SYS
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`,
matching PDB
`980161BC328FFB5214B2FA76C03B59ADA22473446DA0AE3C77F970FE0B5202A3`
and manifest
`216F15716C9496B476C877129853A07BAB59097444AA2D160E2812F25AA2DC69`.

Independent Authenticode, local-chain, catalog-membership, INF and SYS/PDB
checks passed. The active `oem17.inf` package and loaded SYS remained unchanged.
See the canonical
[kernel-trace and instrumented-package report](phase-2c-kernel-trace-capture.md)
for the complete identity and runtime gates. Installation is not authorized.

## Required Stage 2 exit evidence

PASS requires all of the following:

- exact-commit clean-source proof and pinned tool identities;
- successful Debug and Release builds plus the complete Dry/stress results;
- one explicitly selected runtime configuration and approved `DriverVer`;
- a coherent INF/CAT/signed-SYS set whose hashes and catalog membership verify;
- matching private PDB identity and a complete manifest;
- documented local-test certificate identity and successful local trust
  verification; and
- proof that no Driver Store, PnP, device, service, boot-policy or certificate
  store operation occurred.

Any ambiguity is FAIL. Stage 2 PASS authorizes neither copying the package to a
test host nor installing, loading, starting or opening the driver. Those remain
separate Phase 2C gates.

## Proposed combined diagnostic candidate

An unsigned validation package now exists for the integrated multi-handle DMA
ownership and Batch 6 diagnostic source. Its distinct identity is Release x64,
`DriverVer=10/22/2026,0.2026.1022.1`, `PnpStatusDiagnostics=true`, SDK/WDK
10.0.28000.0 and MSVC 14.41.34120. Workflow commit
`b1a5404dbee0d3352b9eb1b9cb6ada57fb26d6fc` records driver tree
`5dcd2b6e31193a20d733bfce008aeafb3fb38a71` and include tree
`eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`, exactly matching reviewed PR
#27.

Debug and Release built with zero warnings and errors. The complete workflow
passed 397/397 functional checks, 5,000 DMA plus 1,000 INTEN default stress and
50,000 DMA plus 10,000 INTEN expanded stress. The Release image is AMD64/native
with `DriverEntry`, imports only `hal.dll` and `ntoskrnl.exe`, contains the
diagnostic marker and matches its PDB identity. Frozen hashes are:

| Artifact | SHA-256 |
|---|---|
| INF | `C7F7D496A302FB53ACEC940A77F8A17F781F822EB5A34CEA11A4708B6385C3B6` |
| unsigned CAT | `813F87489BC35D53B5008A4D3BAD19D637FCAC6CFDC74EAF13EAC6FE1605CA3A` |
| unsigned Release SYS | `D236EC3809E02BD0DA0BC3A679A48E346298A1076393BA5A8812794956A70646` |
| matching PDB | `5F36EF5F238F65F17EBD22B4456523817B8A7549984304FFD317A39428A174D7` |
| manifest | `5FA35C702314A689867DAB3E27AEA83AB2EB738642C2495540BFF0C7621DAA60` |

Both SYS and CAT are deliberately `NotSigned`. The package was not staged or
installed, and the installed `oem101.inf` state remained unchanged.

Batch 8 reused this immutable set rather than rebuilding unchanged driver
sources. Independent checks reconfirmed the exact driver/include trees, all five
artifact hashes, `UnsignedValidationOnly`, the INF DriverVer/catalog/service
identity, 11 embedded `PNP_STATUS:` markers, Authenticode `NotSigned` for SYS
and CAT, and the RSDS/PDB match
`93FA4343-6EA4-4077-B409-D43ED110CD3F`, age 1. Current source/ABI and
CREATE/CLOSE/REMOVE/UnknownActive contracts pass; the installed signed SYS hash
and running service remain unchanged. No evidence justified a rebuild.

The unsigned artifacts remain the immutable source set; they must not be
mistaken for the signed promoted package described below.

## Frozen promotion and installed Batch 9 candidate

Batch 9 added `scripts/sign-frozen-driver-package.ps1` so an already qualified
unsigned package can be promoted without silently rebuilding it. The workflow
requires all frozen source hashes and tree identities, copies only the exact
INF/SYS/PDB set, generates a new catalog, signs only the copied SYS and catalog,
and verifies InfVerif, Authenticode, local chain, catalog membership and
SYS/PDB identity. It never stages or installs a package. Focused tests pass
11/11.

The one authorized promotion used the package above and certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`. Final hashes are:

| Artifact | SHA-256 |
|---|---|
| INF | `C7F7D496A302FB53ACEC940A77F8A17F781F822EB5A34CEA11A4708B6385C3B6` |
| signed CAT | `19E9358FB1346C8C40FE96D239E2000D0270559C54C04E6D3855CA710AB60B4B` |
| signed SYS | `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A` |
| matching PDB | `5F36EF5F238F65F17EBD22B4456523817B8A7549984304FFD317A39428A174D7` |
| signed manifest | `C8283DF89C4EC3E9517A90B72F35A5BF48E102B0B3BF66D3C12455BE86A1201F` |

The manifest records `RebuildPerformed=false`, exact source manifest
`5FA35C702314A689867DAB3E27AEA83AB2EB738642C2495540BFF0C7621DAA60`
and the unchanged driver/include trees. The package is installed as
`oem102.inf`, version `0.2026.1022.1`, and its loaded SYS matches the signed
hash. This is a locally test-signed development package, not WHQL/WHCP output.

## Batch 13 candidate identity

The next isolated no-launch candidate uses
`DriverVer=10/24/2026,0.2026.1024.1`, Release x64,
`PnpStatusDiagnostics=true`, SDK/WDK 10.0.28000.0 and MSVC 14.41.34120. Its
reviewed driver tree must remain exactly the tree from Batch 12 commit
`fba434344d82817f9f517f4ddcc97a0b60310571`; repository tooling and
documentation may advance around that immutable tree. Signing is limited to
the existing local test certificate and does not imply Microsoft or production
signing. Artifact hashes are recorded only after the one authorized workflow
finishes.
