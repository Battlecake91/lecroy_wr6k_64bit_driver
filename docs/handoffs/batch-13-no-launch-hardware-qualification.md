# Batch 13 no-launch hardware qualification handoff

## Scope and immutable source

The candidate retains the exact Batch 12 driver tree from commit
`fba434344d82817f9f517f4ddcc97a0b60310571`. The unique proposed package
identity is Release x64 `DriverVer=10/24/2026,0.2026.1024.1` with
`PnpStatusDiagnostics=true`. The existing local test certificate is permitted
only for this development candidate.

## Pre-GO result

The production `CFDC2124`/one-channel `CFDC2138` path performs no WR6k MMIO or
port access. Its only non-software boundary is synchronous WDM-v3
scatter/gather mapping; descriptors are written to common-buffer memory and
the production launch commit unconditionally returns launch-disabled before
the mapping is released. The unsafe PFN branch remains compile-disabled. The
machine-readable allowlist and source contract are in
`docs/dma-pre-go-side-effect-allowlist.json` and
`tests/dry/test-pre-go-side-effects.ps1`.

`lecdiag no-launch-prepare` and
`scripts/invoke-no-launch-preparation.ps1` provide the one-shot 4-KiB public-
ABI experiment. Its only accepted acquisition result is Win32 error 50
(`ERROR_NOT_SUPPORTED`) followed by successful unregister. The deterministic
mock covers the exact three calls and non-overwriting evidence.

## Remaining active sequence

After a clean commit: run the complete package workflow once with the approved
certificate, install once under verified kernel capture, run the existing
R0/R1 matrix once, restart PnP once under capture, then run the no-launch
preparation once only if every prior state remains healthy. Record final
artifact, runtime and evidence hashes here. No step authorizes GO, acquisition
or real DMA.

## Physical idle

No board/RTL contract proves DMA retirement and no suitable conventional-PCI
analyzer is currently identified. The exact analyzer signals and falsification
criteria are in `docs/legacy-dma-abort-bus-idle-audit.md`. Preserve the proof
obligation and irreversible `UnknownActive` policy.
