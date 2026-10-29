# Phase 2C runtime Batch 20 handoff

## Outcome

Batch 20 completed the source correction, full offline qualification and one
authorized signing operation. The signed exact-shape candidate was not
installed. The first `InstallWithCapture` launcher attempt failed closed after
capture readiness and before PnPUtil dispatch because the caller passed the
literal string `$env:SystemRoot\System32\pnputil.exe` as `PnPUtilPath`.

The result is therefore **hardware experiment skipped due to failed
prerequisites**. There was no installation, R0/R1 execution, buffer
registration, WDM mapping, MMIO, GO, IRQ, DPC, payload or DMA cleanup event.

## Source and gate

- Branch: `codex/phase2c-runtime-batch20`
- Starting commit: `424eb3b4197ed50c59a4b535fcdb1c881598605a`
- Gate commit: `389add8559c82a656e5e7a8b45bae12847f72bb5`
- Driver tree: `117d1126e9d29e23bf88e58f79ca51f98d1af31c`
- Include tree: `eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`

The production helper validates the actual Windows DMA mapping and encoded
descriptor table before any acquisition MMIO. It requires channel `0x30`,
config `0x00000C00`, 16 bytes, exactly one complete SG element, a dword-aligned
32-bit logical data range, a page-aligned/ranged common descriptor table, two
descriptor entries, exact payload/address agreement and a zero terminator. An
atomic per-FDO latch admits only one valid attempt. Invalid preparation does
not consume the latch; concurrent valid requests admit exactly one. Default
builds continue to prohibit GO.

## Offline qualification

- Debug x64 and Release x64: zero warnings, zero errors.
- Functional checks: 846/846.
- First-MAM production-gate unit checks: 17/17.
- DMA stress: 5,000/5,000 default and 50,000/50,000 expanded.
- INTEN stress: 1,000/1,000 default and 10,000/10,000 expanded.
- Windows PowerShell 5.1 package compatibility: 28/28.
- Package negative tests: 16/16.
- INF validation, Inf2Cat, x64 PE metadata, imports, `DriverEntry` and SYS/PDB
  identity passed.

## Signed candidate

The one authorized signing dispatch completed successfully with local test
certificate `4296FA2E355613DF0D5CE6F24D0898E00B706472`.

| Artifact | SHA-256 |
| --- | --- |
| INF | `653622D71A7E4F62954A441947C691D090AC6ABF4B3FDC6BF342BEBF24F88B06` |
| CAT | `E28B7EC2870454CFF6B88747D4496C67FCF3F1F8D0045FDB57E22E6D7752DD83` |
| signed SYS | `647A7B38597C19D96DF509557E05895C5074727C329EFBD182AD01AC144E3F71` |
| PDB | `AF8FF333B31BCA2FEBD40ACE87BE741296D46F028861D48EF40A95CB093ADCB3` |
| manifest | `000055E084E4554669ED8D74D2EB747FCA5393DF19415AB14EA0B57E486BE653` |

DriverVer is `10/29/2026,0.2026.1029.1`. Authenticode, local chain, CAT
membership and exact SYS/PDB identity passed. The signed SYS contains
`PNP_STATUS:`, `LECS65_WDM_V3_MAM_DMA_ENABLED` and
`LECS65_FIRST_MAM_EXPERIMENT_ONLY`. The package is immutable at:

`C:\Users\LeCroyUser\.codex\driver-backups\Batch20-SignedCandidate-20261029-1029-1`

## Installation attempt evidence

The passive preflight passed with testsigning enabled, Secure Boot disabled,
VBS/HVCI inactive, Driver Verifier inactive, no acquisition clients and the
healthy `oem105.inf` baseline. Capture readiness was established from an
independently attributable `STORMINI` kernel message. The launcher then rejected
the non-absolute PnPUtil path before creating `install-attempt.marker`.

- `CaptureInvocations=1`
- `InstallInvocations=0`
- `PnpRestartInvocations=0`
- `NoLaunchInvocations=0`
- result SHA-256:
  `2F6B4A7312309A3AE76758453C5EECDB6274BFEA69C9FA1726DDE368A0C3C53E`
- capture-ready SHA-256:
  `AB73EE56513B211EF1207770FB70943627F991A64B8918D4352C40A7129229B5`
- exception SHA-256:
  `67C7CE863C8D33DA621AFF46A1FE28A78856EF0A6BFF2D4FCCEB40C427BC6486`

DebugView was explicitly stopped after failure. No DebugView process or DBGV
service remains.

## Current runtime and remaining gates

The current package remains `oem105.inf`, version `0.2026.1028.1`, with loaded
SYS SHA-256
`09BCC6BCD7A3BE9DC4EFD4B12C66E125B3F9C4455D97AA35C10CDB356EF69AC6`.
Before and after the failed launcher, the stable instance was `OK`, problem code
zero, service running and all five interfaces published. Observed BDF is
`5:1.0`; it is evidence, not an identity key.

A future continuation requires explicit authorization for one corrected
captured installation using the already signed package and a fully resolved
absolute PnPUtil path. After installation it must run exactly one 9/9 R0 and
2/2 R1 matrix. GO also remains blocked until the owner explicitly confirms that
the productive Windows x86 disk is physically disconnected. Software inventory
showing only the x64 NVMe disk is insufficient.

Do not rebuild or re-sign the candidate, retry installation without approval,
launch XStream, or issue the one-shot MAM command from this handoff state.
