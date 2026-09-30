# Windows driver signing and project funding

Last reviewed: 2026-09-30.

This project currently uses test signing for development. Public distribution without
Windows test mode is a separate release step and should not be confused with ordinary
Authenticode signing of user-mode applications.

## Current Microsoft signing requirements

Microsoft's current Windows driver documentation requires a Windows Hardware Dev Center
account with an **Extended Validation (EV) code-signing certificate** associated with the
account before driver submissions can be made.

For Windows 10 Desktop and later, Microsoft documents two relevant paths:

- **Attestation signing**: intended for testing scenarios, does not require HLK testing,
  and produces a Microsoft-trusted driver signature. Microsoft currently states that
  attestation-signed drivers are not for retail Windows Update publication.
- **Windows Hardware Compatibility Program (WHCP)**: the production/certification path,
  using HLK results and the Hardware Dev Center submission process.

The old kernel cross-signing route must not be treated as a current distribution option.

Official references:

- Microsoft Hardware Dev Center:
  https://learn.microsoft.com/windows-hardware/drivers/dashboard/
- Microsoft code-signing requirements:
  https://learn.microsoft.com/windows-hardware/drivers/dashboard/code-signing-reqs
- Driver signing options:
  https://learn.microsoft.com/windows-hardware/drivers/dashboard/driver-signing-offerings
- Microsoft Windows driver policy:
  https://support.microsoft.com/windows/hardware/drivers/the-windows-driver-policy

## Certificate cost snapshot

The EV certificate is the main unavoidable third-party cost for establishing the
Hardware Dev Center identity. Prices and availability change, so these are only
2026-09-30 snapshots and must be rechecked before purchase.

Examples from Microsoft-listed certificate authorities:

- Certum advertised EV Code Signing from about **EUR 359** for a physical set and
  about **EUR 379** for its cloud variant.
- DigiCert advertised EV Code Signing subscriptions around **EUR 840/year** for
  several EV storage variants.
- SSL.com advertised EV Code Signing from **USD 349/year**, with secure key-storage
  options potentially adding cost.

Public code-signing certificate validity is now limited to roughly 459 days by current
industry rules, even where vendors sell multi-year service plans with certificate
reissuance.

Normal low-cost "open source code signing" products are not automatically a substitute:
the Hardware Dev Center enrollment requirement specifically calls for an EV certificate.

Vendor references:

- Certum EV Code Signing:
  https://shop.certum.eu/certum-ev-code-sigining.html
- DigiCert EV Code Signing:
  https://www.digicert.com/signing/compare-code-signing-certificates
- SSL.com EV Code Signing:
  https://www.ssl.com/products/software-integrity/code-signing/ev/

## Funding approach

For a small open-source hardware preservation project, a **single transparent funding
goal** is preferable to an open-ended donation request.

A practical campaign should state:

1. what has already been demonstrated on real hardware;
2. that the source code and reverse-engineering documentation are public;
3. the exact signing milestone being funded;
4. the current certificate quote and a small allowance for payment/transaction costs;
5. what happens to excess funds;
6. that contributions to an individual project are not automatically tax-deductible
   charitable donations.

A campaign target of roughly **EUR 450-500** is a reasonable provisional range if the
project selects one of the lower-cost EV options, but the target should be replaced by
the actual checkout quote before publishing the campaign.

Suitable campaign surfaces include a Ko-fi Goal or GoFundMe campaign linked prominently
from the GitHub repository. GitHub Sponsors is also available in Germany, but is better
suited to ongoing maintainer support than to a one-off fixed signing invoice.

The highest-value audience is the test-equipment / oscilloscope repair and preservation
community. A campaign link can be shared in technically relevant communities such as the
EEVblog Test Equipment forum, where long-running LeCroy upgrade and repair discussions
already exist.

No funding link is currently endorsed by this repository. Add one only after the owner
has chosen the platform, target amount, and wording.

## WHCP cost model

There is no separate paid Microsoft HLK product required for self-testing. Microsoft
provides HLK/VHLK downloads, and VHLK includes a 180-day Windows Server evaluation for
the controller environment. Current Microsoft registration/submission documentation
does not list a separate per-submission WHCP certification fee; the mandatory external
cash cost remains the EV code-signing certificate required for Hardware Developer
Program enrollment.

For this project, self-testing is technically plausible because Microsoft's PCI HLK
prerequisites require one test computer containing the PCI device, the driver under
test, the HLK client, and access to an HLK Controller/Studio system. The existing
WaveRunner PC can potentially serve as the PCI test client, while the controller can be
hosted separately or via VHLK.

Therefore the practical funding ranges are:

- **Self-run WHCP/HLK:** approximately the EV-certificate cost plus incidental payment
  costs and any missing test hardware. With the current lower-cost EV examples, a
  provisional project target around EUR 450-500 remains plausible.
- **Commercial test lab:** potentially hundreds to several thousand euros depending on
  driver/device category, operating-system matrix, failed-test investigation and
  retesting. Lab pricing is vendor-specific and should be quoted rather than treated as
  a Microsoft fee.

The largest uncertainty is not the Microsoft fee but whether the legacy PCI acquisition
device and replacement driver can pass all HLK tests selected for its device category
without changes. The HLK run should therefore be attempted in-house before paying an
external certification laboratory.
