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

## Package attempt and active-work stop

The single authorized package-signing attempt ran from clean workflow commit
`20e6bbac2b4224da6ccb9310a5d2ee221bcfefd1`. Debug and Release builds, the
complete Dry suite and the expanded 50,000 DMA plus 10,000 INTEN stress run
passed. The workflow then stopped before artifact staging, Inf2Cat or any
private-key operation because its frozen evidence predicate still required
the obsolete `CRASH DUMP READINESS: 13/13` line while the current focused
suite correctly reports `14/14`. The attempt marker, transcript and separate
logs are retained under the non-reusable Batch 13 output root. No package was
created or signed, and the installed `oem102.inf` driver remained loaded with
SYS SHA-256
`D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`.

Retained SHA-256 evidence:

- attempt marker:
  `DE88CC519AF095C05869BDBAD88EC78664E54B8EF437D57C46BEEDD85B1F19D4`;
- elevated launcher log:
  `9243486336F5D93720E9045BF1C3AE0988CFB6E4EBF7CB06EA00806AA1128772`;
- workflow transcript:
  `5192060C0EA2E8008DBFFAFDB354D097137F0C652F558F103E468C6EF043DA19`;
- Debug build log:
  `68904B9F45F3AB5788CCDBA65B372C6222FE5715DA0971437F0B3CBB6483F0E0`;
- Release build log:
  `C0EA0E397AA32078D3B02DFAD4045944742A84A82A468BFDCBC75CE5A49DE935`;
- Dry regression log:
  `0EC6BE718A5183A2067683741F12ADCDB321E3A145F205752123AA83EA726465`;
- expanded stress log:
  `0C2C7A263161FA99E90DCE3A19C017DA082787D3D171DE3D0B1C4DC230544D6F`.

The output package directory contains zero files. The freshly built Release
SYS is unsigned, which independently confirms that the failure preceded the
first SignTool invocation.

The package predicate now requires `14/14`, and a compatibility contract
rejects a recurrence of the stale `13/13` expectation. This correction is
offline only. Windows PowerShell 5.1 focused validation passes package-workflow
compatibility 26/26, crash-dump readiness 14/14 and source contracts 69/69.
The failed attempt consumed the signing authorization, so no installation,
R0/R1 run, PnP restart or live no-launch IOCTL was permitted.

## Remaining active sequence

After a new explicit signing authorization: run the corrected complete package
workflow once with the approved certificate. A successful new signed candidate
would then require fresh authorization boundaries for installation and the
dependent runtime sequence. No current step authorizes GO, acquisition or real
DMA.

## Physical idle

No board/RTL contract proves DMA retirement and no suitable conventional-PCI
analyzer is currently identified. The exact analyzer signals and falsification
criteria are in `docs/legacy-dma-abort-bus-idle-audit.md`. Preserve the proof
obligation and irreversible `UnknownActive` policy.
