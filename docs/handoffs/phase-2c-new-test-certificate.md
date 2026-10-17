# Phase 2C new test certificate

## Authorization and scope

The owner explicitly authorized one new test-certificate/key-pair creation,
public `.cer` export, imports of the public certificate into
`LocalMachine\Root` and `LocalMachine\TrustedPublisher`, and read-only
certificate/container/chain validation. Signing, driver staging, installation,
PnP changes, reboot and hardware access remained unauthorized and did not
occur.

The first non-elevated creation attempt returned `NTE_PERM` before creating a
certificate or output file. A uniqueness and file-existence check confirmed no
partial state. The same approved operation then ran once with an elevated
token and completed successfully.

## Certificate result

| Property | Verified value |
|---|---|
| Subject / issuer | `CN=LecS65 x64 Test Phase2C 2026-10` / self-signed |
| Personal store | `LocalMachine\My` |
| SHA-1 thumbprint | `4296FA2E355613DF0D5CE6F24D0898E00B706472` |
| Serial number | `153EE6DD7DF34E93482EA473ED793166` |
| Provider | `Microsoft Software Key Storage Provider` |
| Provider model | CNG KSP, machine key, `KeySpec=0` / `XCN_AT_NONE` |
| Container | `te-7e799dd3-f896-4db8-b6ff-f680e0b5a7cf` |
| Public-key algorithm | RSA, 2048 bits |
| Certificate signature | SHA-256 with RSA |
| Key export policy | Non-exportable, as created |
| Key usage | Digital Signature only |
| EKU | Code Signing `1.3.6.1.5.5.7.3.3` only |
| Validity, local | `2026-10-17T00:00:00+02:00` to `2027-04-17T23:59:59+02:00` |
| Validity, UTC | `2026-10-16T22:00:00Z` to `2027-04-17T21:59:59Z` |

Exactly one certificate exists in `LocalMachine\My`. Exactly one public copy
exists in each of `LocalMachine\Root` and
`LocalMachine\TrustedPublisher`. The local offline chain builds successfully
with no chain status errors and contains the expected self-signed certificate.

The public certificate was exported outside the repository to:

```text
C:\Users\LeCroyUser\.codex\driver-backups\LecS65-x64-Test-Phase2C-2026-10.cer
```

Its SHA-256 is
`5B18C8C3C5D0BB8AD1EB03E685E43873630B14445AA93D6CF1860BF2A5D204FE`.
No private-key material was exported or displayed.

## Key-container accessibility

An exact read-only lookup of container
`te-7e799dd3-f896-4db8-b6ff-f680e0b5a7cf` through the Microsoft Software KSP
succeeds with an elevated token: it reports neither `NTE_BAD_KEYSET` nor
access denied. This proves that the newly created private-key container exists
and is accessible to a later elevated signing process.

The same lookup from the filtered non-elevated token returns
`NTE_BAD_KEYSET`. This is a token/access-context difference, not the stale-key
condition diagnosed for the obsolete certificate, because the elevated lookup
opens the new container successfully. No key ACL was modified. The hardened
package workflow must therefore be launched elevated for a future signing run,
unless a separate owner-approved ACL policy is implemented and qualified.

No private-key cryptographic operation or SignTool invocation was used during
validation.

## Package identity retained for the next gate

- package-workflow source commit:
  `e20f92c7593e4cb9f7249bff204e4a30f55d94d2`;
- reviewed driver/include source commit:
  `cb5010a4712f25de7c56d23d0d7502683542058c`;
- driver tree: `fc0e1cbca171e3aba45bd60d034b86bd63466eae`;
- include tree: `dc000daeafc7e54c2c1ffeb46444cb2e7e3d363b`;
- runtime configuration: Release x64; and
- proposed, not finalized `DriverVer=10/17/2026,0.2026.1017.1`.

The driver and include trees match the reviewed revision exactly, so this
certificate-preparation step introduces no public IOCTL ABI change.

## Rollback preservation

The private exported `oem100.inf` recovery set remains present and hash
identical:

| Artifact | SHA-256 |
|---|---|
| INF | `C271BBA926667BEB378FDE39AA226CEAA4680AF5A21519A5F26E3A814E7C0E16` |
| CAT | `DD2941EA62DB3CAD0F0FB447DE5C8B390A142C46895EB9A7FA0035462BB8CF18` |
| SYS | `4EB0BA4165F38D7FB9C539A2A0ACA060647D47BBBF3293DA99A7CFB5F583B687` |

The installed `oem100.inf`, driver service and PCI device were not modified.

## Next explicit gate

One elevated signing run using thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472` remains unauthorized. Before it can
run, the owner must explicitly approve the proposed `DriverVer`, exact source
identity, output directory and private-key signing operation. Installation and
the first PnP START remain later, independent gates.
