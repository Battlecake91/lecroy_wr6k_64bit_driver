# Batch 14 no-launch hardware qualification handoff

## Frozen source and authorization

The driver and include trees remain the exact reviewed Batch 12/13 identities:
driver `0e19b6eff8dfe368be7c2cb008829251fe82a53a` and include
`eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`. The intended Release x64 package
uses `DriverVer=10/24/2026,0.2026.1024.1` and
`PnpStatusDiagnostics=true`. No driver source changed in Batch 14 preparation.

## Signing preflight hardening

The package workflow no longer accepts a detached global functional-test
constant. It extracts 23 named suite outcomes from the real workflow
transcript, requires every suite to report complete success, records the
machine-readable outcome array in the manifest and derives the aggregate from
those records. The elevated launcher and frozen-package signer independently
require the exact suite-ID set and recompute both functional and stress totals.
This prevents a newly added focused check from invalidating signing through an
unrelated stale aggregate while retaining fail-closed suite identity checks.

Windows PowerShell 5.1 validation passes package compatibility 26/26,
crash-dump readiness 14/14 and source contracts 69/69. The complete elevated
mock launcher path passes 36/36 without private-key use. Its transcript
SHA-256 is
`7C21A70D43227FB706747BB398898100545F7FC626485A1BD4793E4F251B24E4`.

## Ordered active gates

One real signing attempt is authorized after the preflight commit is clean.
Only a fully verified signed candidate permits one captured installation.
Healthy installation permits one R0 9/9 plus R1 2/2 matrix; that success
permits one captured PnP restart; only complete success of every preceding
stage permits the bounded `CFDC2124`/`CFDC2138` no-launch preparation. No stage
authorizes GO, acquisition or real DMA.

Physical PCI bus-idle remains a separate unresolved proof obligation requiring
external conventional-PCI analyzer evidence.

## Signed and installed candidate

The one authorized signing attempt completed from clean workflow commit
`d90571323984249c93993235c4cad6cf71cd9de6`. Local Authenticode, chain,
catalog membership and SYS/PDB identity all pass. Frozen SHA-256 values are:

- INF `FED0F507EC7E91553EFA7A2E2AD43C6EB9232653EECE2853935A0D3A310E8B8A`;
- CAT `97F4C8B25926B3A5202558610B3098872DB7F37410B80583770875808A23BFD7`;
- signed SYS `73B235BD4D19354612752A87BA499C06D15336F63A6B73EDDC0C674C3086D908`;
- PDB `99BF95283A9213A2BBACF5DEF602A68D7C52E91BC141EFFAB2FC2BF8848387D5`;
- manifest `CDC234B493436ADDBD9E8A5E79EE5B25FF1AEEAFDFF6BFC2A2C8AE7EEED178A0`.

One captured installation completed without retry. Windows published and bound
`oem103.inf`, version `0.2026.1024.1`. The device is problem-free, the service
runs and the loaded SYS matches the frozen signed hash. The capture contains
49 `PNP_STATUS:` records including successful local START, interrupt connect,
DMA-parent start, all five interface enables and final START. Raw capture
SHA-256 is
`29FE149118C08AF5B7667C4B0BC2AE0F3CFF63C1AC6048A82D0EB62D80B43E0D`.
The prior `oem102.inf` package was not deleted.

The runtime qualification harness now accepts explicit expected INF, version,
SYS and diagnostic hashes while preserving the previous defaults. Its complete
mock matrix passes 21/21, including fail-closed and no-retry cases.
