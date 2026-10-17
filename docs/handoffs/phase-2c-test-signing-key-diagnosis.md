# Phase 2C test-signing key diagnosis

## Scope and safety

This read-only investigation examined certificate metadata, provider presence,
key-container existence and public certificate matches for thumbprint
`DBB8F215BE00ACA1D6F7D258773DBE8748387D90`. It did not open a private key for a
cryptographic operation, invoke SignTool, repair an association, change an ACL,
create/import/export a certificate or modify any certificate store. Driver
builds and regression tests were not repeated.

## Established state

| Property | Result |
|---|---|
| Signing-certificate store | `LocalMachine\My` |
| Subject / issuer | `CN=LecS65 x64 Test` / self-signed |
| Public key | RSA, 2048 bits |
| Certificate signature | SHA-256 with RSA |
| Key provider model | CNG / KSP, not legacy CAPI |
| Provider | `Microsoft Software Key Storage Provider` |
| Provider type | `0` (CNG KSP) |
| Key location | Machine key set |
| Key specification | `0` / `XCN_AT_NONE` |
| Referenced container | `te-aa54bed6-ed44-4a0d-a952-18db0e1422e1` |
| Exact container open | `NTE_BAD_KEYSET` (`0x80090016`) |
| Access-denied evidence | None; the result is not `NTE_PERM` or access denied |

`certutil -store -v My <thumbprint>` exposes a persisted
`CERT_KEY_PROV_INFO_PROP_ID` with that provider and container, which explains
why the .NET certificate projection reports `HasPrivateKey=true`. The same
command reports that the stored key set is missing. An exact read-only
`certutil -csp "Microsoft Software Key Storage Provider" -key <container>`
lookup returns `NTE_BAD_KEYSET`. Provider enumeration succeeds and lists other
machine keys, proving that the KSP itself is installed and operational.

The same public certificate is present without a private key in the machine
Root, CA and Trusted Publishers stores and in the corresponding current-user
trust stores. There is no matching certificate/private-key pair in
`CurrentUser\My`, no other certificate with the same subject or public key, and
no user or machine container with the referenced name.

The decisive read-only commands were:

```powershell
certutil.exe -store -v My DBB8F215BE00ACA1D6F7D258773DBE8748387D90
certutil.exe -user -store -v My DBB8F215BE00ACA1D6F7D258773DBE8748387D90
certutil.exe -csp 'Microsoft Software Key Storage Provider' -key
certutil.exe -user -csp 'Microsoft Software Key Storage Provider' -key
certutil.exe -csp 'Microsoft Software Key Storage Provider' `
  -key te-aa54bed6-ed44-4a0d-a952-18db0e1422e1
certutil.exe -user -csp 'Microsoft Software Key Storage Provider' `
  -key te-aa54bed6-ed44-4a0d-a952-18db0e1422e1
certutil.exe -csplist
```

PowerShell certificate-provider enumeration was used only to compare subject,
thumbprint, public-key bytes and `HasPrivateKey` metadata across
`Cert:\LocalMachine` and `Cert:\CurrentUser`.

## Diagnosis and confidence

The `LocalMachine\My` certificate contains a stale reference to a missing CNG
machine-key container. This is conclusively a missing-key condition on this
host, rather than the wrong store, a missing provider or a key ACL denial.
Confidence in the present-state diagnosis is **high**. The historical event
that removed or failed to preserve the container is unknown; current evidence
cannot distinguish deletion, incomplete migration/restore or an import that
retained stale provider metadata.

The old certificate is not locally recoverable. It becomes recoverable only if
an external PFX/backup or another computer still contains the exact original
private key. `certutil -repairstore`, ACL changes and a newly generated key
cannot reconstruct that key and must not be used to bind a replacement key to
the old certificate.

## Proposed replacement behind owner authorization

For this disposable development host, create a new non-exportable CNG
code-signing key and a distinctly named self-signed certificate. The following
commands are a proposal only and were **not executed**:

```powershell
$newCertificate = New-SelfSignedCertificate `
  -Type CodeSigningCert `
  -Subject 'CN=LecS65 x64 Test 2026-10 Replacement' `
  -FriendlyName 'LecS65 x64 Test 2026-10 Replacement' `
  -CertStoreLocation 'Cert:\LocalMachine\My' `
  -Provider 'Microsoft Software Key Storage Provider' `
  -KeyAlgorithm RSA `
  -KeyLength 3072 `
  -HashAlgorithm SHA256 `
  -KeyExportPolicy NonExportable `
  -KeyUsage DigitalSignature `
  -NotAfter (Get-Date).AddYears(1)

Export-Certificate `
  -Cert $newCertificate `
  -FilePath .\LecS65-x64-Test-2026-10-Replacement.cer `
  -Type CERT

Import-Certificate `
  -FilePath .\LecS65-x64-Test-2026-10-Replacement.cer `
  -CertStoreLocation 'Cert:\LocalMachine\Root'

Import-Certificate `
  -FilePath .\LecS65-x64-Test-2026-10-Replacement.cer `
  -CertStoreLocation 'Cert:\LocalMachine\TrustedPublisher'
```

Certificate/key creation and both trust-store imports require explicit owner
authorization. Public-certificate export may be performed in the same approved
certificate-management stage; no private-key export is proposed. After
creation, first record the new thumbprint and container metadata and prove an
exact container lookup succeeds. The first SignTool/package signing operation
is a later, separately authorized gate.

## Candidate version review

The proposed `DriverVer=10/17/2026,0.2026.1017.1` follows Windows INF syntax:
month/day/four-digit-year plus four numeric version components, each below
65535. Its date was the package-build date and was not in the future. It is
compatible with the Windows 10 x64 package format already accepted by InfVerif
and Inf2Cat.

The installed package is `DriverVer=09/29/2026,0.2026.930.54` with observed
rank `00FF0000`. After valid signing, and assuming the candidate retains the
same hardware match and therefore equal rank, Windows compares date before
version; `10/17/2026` is newer and the candidate would outrank the installed
package. The unsigned validation set itself is not installable and has no
usable signing rank. This review does not approve or finalize the proposed
identity.

Microsoft's [DriverVer directive](https://learn.microsoft.com/windows-hardware/drivers/install/inf-driverver-directive)
defines the date and four-component syntax. The documented
[driver-package selection process](https://learn.microsoft.com/windows-hardware/drivers/install/overview-of-the-driver-selection-process)
selects the lowest rank first, then the newest date and finally the highest
version for equal-rank candidates. Microsoft's
[test-certificate guidance](https://learn.microsoft.com/windows-hardware/drivers/install/test-signing)
and [test-certificate installation guidance](https://learn.microsoft.com/windows-hardware/drivers/install/installing-test-certificates)
describe the separate signing and local-machine trust-store roles.

## Remaining gates before one real signing run

1. Owner approval of the proposed replacement certificate parameters.
2. Separate authorization to create the key/certificate and install its public
   certificate in the required local-machine trust stores.
3. Read-only verification of the new thumbprint, EKU, validity, provider,
   container existence and trust chain.
4. Owner approval or replacement of
   `DriverVer=10/17/2026,0.2026.1017.1`.
5. Approval of the exact new thumbprint, immutable source revision, output
   directory and one real private-key signing run.
6. Post-signing hash, catalog membership and signature-policy verification.

Signing, installation, PnP START and all runtime tests remain unperformed and
separately gated.
