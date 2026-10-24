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
