# Batch 11 WDM-v3 transaction handoff

## Integrated result

The branch adds a compiled, hardware-independent DMA transaction owner and
D0/Dx/query admission to the inactive PnP parent. The state model covers all
software-verifiable transitions through descriptor publication, completion,
cancellation, retirement and irreversible quarantine. Production hardware
launch is still unavailable; only host tests can inject a possible-GO state.

The current explicit adapter mapping stage remains no-launch
`GetScatterGatherListEx` staging. A future live implementation should use the
explicit WDM-v3 channel/map/flush/free sequence and reject partial mappings.
It must not derive device addresses from PFNs or release any mapping after a
possible GO without independent physical-idle proof.

## Offline evidence

- Debug x64: 0 warnings, 0 errors; SYS SHA-256
  `87E5C186AF21F1CAAECB06BC0C94CA4D1CD141CA784FFC4B076B612CBEC7288E`.
- Release x64: 0 warnings, 0 errors; SYS SHA-256
  `C04D36DEFFA48086C542A1A2FD94E336DDD06175118A2FC56D290C0E5F48A357`.
- Transaction: 134/134; synchronous SG/PnP/POWER: 153/153; source contracts:
  68/68; continuation: 17/17; complete Dry suite: PASS.
- Default stress: DMA 5,000/5,000, INTEN 1,000/1,000. Expanded: DMA
  50,000/50,000, INTEN 10,000/10,000.
- Passive installed baseline: `oem102.inf`, `0.2026.1022.1`, device OK,
  service running, loaded SYS SHA-256
  `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`.

## Remaining proof obligations

1. Implement explicit WDM-v3 channel allocation, complete `MapTransferEx`,
   descriptor publication, post-idle `FlushAdapterBuffersEx` and exact-once
   `FreeAdapterObject` ownership under fake DDIs.
2. Wire live POWER and QUERY/CANCEL PnP policy without waits under spin locks
   or premature mapping/MDL release.
3. Establish hardware MAM and MTT stop, descriptor-fetch cessation, final PCI
   write ordering and a physical retirement predicate from RTL/vendor evidence
   or a recoverable analyzer experiment.
4. Only then prepare a separately versioned, signed and authorized candidate.

No driver was signed, installed or loaded, and no hardware operation occurred.
