# Runtime Qualification Batch 9 handoff

## Result

Batch 9 completed every authorized stage. Windows 10 x64 remained on the sole
visible GPT NVMe; the owner-reported separate production x86 disk was not
visible and was not touched. CrashControl is kernel dump with automatic reboot
disabled and an 8,192-MiB system-managed page file. The one-boot non-DMA
Verifier run targeted only `LecS65AcqDrv.sys`, produced no bugcheck or target
violation and is now inactive.

The frozen Batch 7 source was promoted without rebuilding, signed once and
installed once. Current runtime identity is:

| Item | Value |
|---|---|
| Published INF | `oem102.inf` |
| Driver version | `0.2026.1022.1` |
| INF SHA-256 | `C7F7D496A302FB53ACEC940A77F8A17F781F822EB5A34CEA11A4708B6385C3B6` |
| CAT SHA-256 | `19E9358FB1346C8C40FE96D239E2000D0270559C54C04E6D3855CA710AB60B4B` |
| Signed SYS SHA-256 | `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A` |
| PDB SHA-256 | `5F36EF5F238F65F17EBD22B4456523817B8A7549984304FFD317A39428A174D7` |
| Manifest SHA-256 | `C8283DF89C4EC3E9517A90B72F35A5BF48E102B0B3BF66D3C12455BE86A1201F` |

The bounded runtime matrix is 9/9 R0 and 2/2 R1 PASS. The x64 diagnostic
binary hash is
`0567A21992E95FFBB8E08B24ED10DA1CB4339682CB6DD31CD42F740F99E955D2`.
PCI identity is `1570:0005`, observed BDF 5:1.0, trace size `0x110`, register
list size `0x2CAE`, and both START reads were `0x00000002`. The runtime evidence
manifest SHA-256 is
`D70810B939FCD8FF7872715047E48B712B37D5EF96C05376F0DE799E78F6025B`.

## PnP capture

One `RestartWithCapture` run completed with one capture, zero installations and
one PnP restart. The raw capture has SHA-256
`9E7DB4826E6638A38347099BC5B2BB133DE1C7D40C9B8E9DB8B00EBA29C29AAF`;
the result JSON has SHA-256
`55EDC039CC9143CC74722DA7E322636404E4649687F4D5CC2EF78F656E0A76A5`.
Forty-two status markers prove successful teardown and START, IRQ connection
and all five interface enables. Lower-stack query paths returned the known
`0xC00000BB`; local START and final status were zero. The post-restart passive
manifest hash is
`AD86C285DDD7EB0ED598BF55053A5128AC2C7B001FD35CB07D8637FF75A9A83F`.
No target error event, DebugView process or loaded `Dbgv` component remained.

The preceding installation capture itself was successful but its old launcher
reported a false timeout while serializing about 100 MiB of output. The runner
now suppresses console replay for capture children and returns a compact job
result while preserving authoritative files. Elevated Windows PowerShell 5.1
launcher tests pass 31/31; the permanent runtime harness passes 21/21.

## DMA qualification boundary

The signed package retains the exact reviewed driver tree
`5dcd2b6e31193a20d733bfce008aeafb3fb38a71` and include tree
`eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`. Fresh focused tests pass:
process/file lifetime 23/23, combined lifetime 19/19, ownership 17/17,
completion 39/39, interleavings 2/2, default DMA stress 5,000/5,000, INTEN
49/49 plus 1,000/1,000, publication 38/38 and source contracts 64/64.

No real DMA or acquisition ran. The remaining release blockers are physical
DMA abort/bus-idle proof, final PCI-access ordering, real acquisition under the
new ownership model, power-transition qualification and eventual HLK/WHCP
work. WDM-v3 DMA remains unreachable and must not be enabled from these
results.
