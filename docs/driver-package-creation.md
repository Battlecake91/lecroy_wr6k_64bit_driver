# Offline driver-package creation contract

This document is the canonical contract for Phase 2C Stage 2. It specifies a
package-only build, staging, test-signing and verification workflow. It does
not authorize the workflow and it must not be used to install, stage or load a
driver.

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

1. exact reviewed Git commit;
2. expected clean branch/worktree identity;
3. SDK/WDK version `10.0.28000.0` and MSVC v143 toolset version
   `14.41.34120`;
4. runtime configuration, `Debug` or `Release`;
5. owner-approved `DriverVer` date and four-part version;
6. exact SHA-1 certificate thumbprint in `LocalMachine\My`;
7. a new, non-existing output root; and
8. the owner authorization that names the certificate and permits private-key
   use and creation of signed files.

The certificate must be currently valid, contain code-signing EKU
`1.3.6.1.5.5.7.3.3`, have an associated private key, and build successfully
under the intended local test trust policy. Subject-name selection is not
permitted because it can silently select the wrong renewed or duplicate
certificate. A local self-signed test certificate is not a Microsoft
kernel-policy or production signature.

`DriverVer` must pass INF syntax validation, must not be in the future in UTC,
and must be recorded before the run. The workflow must not synthesize it from
the invocation time. The selected version also needs an owner-reviewed ranking
policy relative to packages already present on the future disposable test
host; Stage 2 itself must not query or modify that host.

## Fail-closed procedure

Every external command must have its exit code checked immediately. Stop at
the first mismatch, retain the incomplete evidence directory, and do not sign
or copy a later component after a failed prerequisite.

1. **Preflight source identity.** Require `git rev-parse HEAD` to equal the
   approved commit, `git status --porcelain` to be empty and the intended
   branch relationship to hold. Record the commit and tree IDs for `driver/`
   and `include/`. Reject a detached, dirty or different source state.
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
   matching PDB and verify that its identity matches the SYS. A build from an
   earlier commit is not substituted merely because its driver tree matches.
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
