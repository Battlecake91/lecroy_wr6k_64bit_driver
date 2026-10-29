# Batch 19 first MAM DMA qualification handoff

## Outcome

The first real GO was **not executed**. One candidate was signed and installed,
and its complete safe R0/R1 matrix passed. The final pre-GO review then found
that the installed binary did not enforce the exact authorized experiment at
the kernel boundary. No launch-attempt marker was created and no acquisition
MMIO, DMA GO, IRQ completion or mapping retirement occurred.

Keep XStream and acquisition clients closed while the installed development
package remains active. Signing, installation and GO authorizations from this
batch are consumed. Any replacement, rollback or first transfer requires new
explicit authorization.

## Storage isolation evidence

Windows disk, partition and volume inventory exposed exactly one physical disk:
the 256,060,514,304-byte NVMe system disk `HFM256GDHTNG-8510B`, serial
`ACE4_2E00_9590_A2AA.`, containing the GPT boot/system `C:` volume. No second
disk or volume was visible. Existing project evidence records the owner's
productive x86 installation as a physically separate disk. Software can prove
that no second disk is exposed to this Windows instance, but cannot identify an
absent disk by model or serial. No storage configuration was changed.

## Historical parity review

The reviewed original path supports the selected bounded request:

- `CFDC2124` registers a process-owned writable buffer and returns a token.
- One-channel `CFDC2138` uses channel `0x30`, config `0x00000C00` and 16 bytes.
- Descriptors are `{DWORD count, DWORD address}` with a zero/zero terminator.
- The order is MAM configuration, SGTA, IIMTC, completion reset/selection,
  INTEN bit 0, IIMCL and one MAMRGO, then an at-most-five-second wait.
- A physical IRQ, DPC event and INTEN cleanup are the normal software success
  path; the public result is a four-byte completed count.

The WDM-v3 replacement uses Windows DMA-logical addresses rather than CPU PFNs.
Historical success is practical compatibility evidence, not a formal proof of
final physical PCI write retirement.

## Signed and installed candidate

The source/workflow commit used for the single signed attempt was
`78a3b26ffd4c5fb128cf4ad72ebeb5a8752835c6`. Driver tree was
`df836cbe7ca3c653d9252923f9cfaa78c0dc0d86`; include tree was
`eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`. Configuration was Release x64,
`PnpStatusDiagnostics=true`, `WdmV3MamDma=true`, version
`10/28/2026,0.2026.1028.1`, signed with test certificate thumbprint
`4296FA2E355613DF0D5CE6F24D0898E00B706472`.

| Artifact | SHA-256 |
|---|---|
| INF | `60C744B8C8BFC54C6585EA0571B1683C5FF9DA1580F20196EF376FA3A34DA657` |
| CAT | `3E7F9596E06638178998EC50D40BC3BB3374EB5EAB205A89F3280DC064D17026` |
| Signed SYS | `09BCC6BCD7A3BE9DC4EFD4B12C66E125B3F9C4455D97AA35C10CDB356EF69AC6` |
| PDB | `B95CE60B010541C63EEF253F96BB9403C6B1FE25F16CDAC8E514FE4773AA1E74` |
| Manifest | `91CE1ABA6036DAF4068BB395533CA60D0676DDA11CD54A95EFD8380953BFF991` |
| lecdiag | `E4E042A81666A3548310090A0663FE1D51C1F6F03DE0ADC5241FAB182E3E108F` |

Authenticode, the local test chain, CAT membership, INF consistency and SYS/PDB
association passed. The package workflow passed 845 functional checks and
66,000 stress iterations.

The repository-controlled captured install completed once without retry and
published `oem105.inf`. The raw kernel capture SHA-256 is
`B2724EB638E5F4B182DD73F7211880C9D39C307B0CA6B1668FA285E95522091F`;
the launcher result SHA-256 is
`243922DED8DEC200FF264CFED035ABB6F4A15189B028D2AB998E6C7D0D6C4682`.

## Runtime matrix and final state

The one-shot runner passed 9/9 R0 and 2/2 R1. Trace size was `0x110`, register
list size was `0x2CAE`, PCI identity was `1570:0005`, observed BDF was `5:1.0`,
PCI command was `0x0006`, IRQ was 19 and both START reads returned
`0x00000002`. The evidence manifest SHA-256 is
`CA6327CBF0FA3E546D5F09444B294A9A7A9177C782B7730FFCDA7BB7E19F73F4`;
the outcome SHA-256 is
`99DDFEDB8731FF46E41D6BDE8C0A397F3305F585B9D8D642A6487F7F71AA09B4`.

Final passive state is stable instance
`PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`,
`oem105.inf`, version `0.2026.1028.1`, exact loaded SYS hash above, device `OK`,
problem code zero, service Running and one published instance of each of the
five legacy interface classes. No test command produced a target Kernel-PnP or
Code Integrity event. DebugView was no longer running after capture. The
original rollback export hashes remained valid.

## Why GO stopped

The installed package's `LecWdmV3MamDma` switch enabled the general validated
one-channel transaction. It did not independently require channel `0x30`,
config `0xC00`, 16 bytes, exactly one SG data segment plus terminator, or one
attempt per loaded FDO. The diagnostic command's shape and user-mode one-shot
ledger were insufficient substitutes for a kernel-enforced boundary. This
failed final pre-GO items 7, 8, 9 and 13. The command
`lecdiag mam-dma-once I-AUTHORIZE-ONE-MAM-GO` was not dispatched.

Consequently there is no DMA payload, GO/IRQ/DPC timestamp, returned completion
count, mapping retirement or quarantine outcome. The mapping remained
`NeverLaunched` because no new mapping was requested after the R0/R1 matrix.

## Offline correction

The successor source adds default-off `FirstMamExperimentOnly` propagation
through the driver build, package workflow and elevated launcher. Immediately
before acquisition MMIO it requires channel `0x30`, config `0xC00`, 16 bytes
and two descriptor entries, then atomically consumes a per-FDO latch. It also
records the first DMA-logical segment address and length for bounded tracing.

Debug and Release x64 builds completed with zero warnings and errors. The full
Windows PowerShell 5.1 Dry suite passed, including 5,000 default DMA and 1,000
default INTEN stress iterations. Expanded stress passed 50,000 DMA and 10,000
INTEN iterations. Package workflow compatibility passed 28/28. This source has
not been signed, installed or tested on hardware.

## Next bounded milestone

1. Review the final exact-shape patch and its package-manifest contract.
2. Obtain new authorization for one signing run.
3. Obtain separate authorization to replace or roll back `oem105.inf`.
4. Re-run passive candidate, device, storage and capture preflight.
5. Only after a new explicit GO authorization, execute one 16-byte experiment.
6. If and only if it completes normally, qualify repeat registration/transfer,
   cancellation, timeout, CLOSE, PnP and power boundaries before XStream.

No XStream acquisition belongs to the immediate next step.
