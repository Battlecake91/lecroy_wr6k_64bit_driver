# Offline driver-package creation contract

This document is the canonical contract for Phase 2C package preparation. It
specifies a package-only build, staging, optional test-signing and verification
workflow. It does not authorize private-key use and it must not be used to
install, stage or load a driver.

The implemented entry point is `scripts/build-driver-package.ps1`. Its default
mode creates an unsigned validation set and never signs or installs it. Signing
is an explicit `-Sign` mode that additionally requires an exact certificate
thumbprint; that mode remains behind a separate owner authorization.

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
explicit child-exit-code handling. The tracked launcher therefore uses
`Start-Process` without `-Verb RunAs` after verifying that its current token is
already elevated. It starts the exact `$PSHOME\powershell.exe`, requires that
host to be 64-bit Windows PowerShell 5.1, redirects stdout and stderr to
separate new sidecars, combines both into the requested run log, and evaluates
the process exit code directly. It never changes hosts or elevates itself.

The launcher requires absolute repository, workflow, output, log and attempt
marker paths. It verifies an exact clean Git HEAD, requires the workflow to be
inside the repository, requires all attempt evidence paths to remain outside
the repository, rejects an existing output or evidence path, creates an
exclusive attempt marker before starting the child, invokes the child exactly
once, and never retries. A log-creation failure leaves the attempt marker in
place. After a zero child exit, the launcher independently parses the manifest,
checks the approved source/configuration/version/thumbprint, recomputes all
four artifact hashes, verifies package contents and the manifest hash, and
requires all offline safety fields to remain false.

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

The real certificate lookup, private-key operation, real SignTool, InfVerif,
Inf2Cat, Authenticode and catalog-membership path through this launcher remains
intentionally untested. Those operations require a third explicit one-run
authorization. Launcher qualification does not authorize signing or
installation.

This validation does not approve the proposed `DriverVer`, certificate,
private-key use or any installation. A signing run must use a new output root
and the separately approved exact inputs.

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
