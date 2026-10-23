# Batch 11 WDM-v3 transaction handoff

## Integrated result

The branch adds a compiled, hardware-independent DMA transaction owner and
D0/Dx/query admission to the inactive PnP parent. The state model covers all
software-verifiable transitions through descriptor publication, completion,
cancellation, retirement and irreversible quarantine. Production hardware
launch is still unavailable; only host tests can inject a possible-GO state.

The branch also contains an explicit no-launch WDM-v3 owner using
`GetDmaTransferInfo`, `InitializeDmaTransferContext`,
`AllocateAdapterChannelEx`, complete `MapTransferEx`, common-buffer descriptor
encoding, `FlushAdapterBuffersEx` and exact-once `FreeAdapterObject`. It rejects
partial mappings and quarantines any uncertain cleanup. It must not be wired
to GO until live request/rundown ownership and independent physical-idle proof
exist.

## Offline evidence

- Debug x64: 0 warnings, 0 errors; SYS SHA-256
  `9D1868FE48C44185096E8B0CF38D9C77BF017EDB4047B240D561E6AC2B08BDC5`.
- Release x64: 0 warnings, 0 errors; SYS SHA-256
  `F3B07D710E41C38EF474996978A9F037CC84C312C79383185B4959731E816274`.
- Transaction: 134/134; explicit WDM-v3 mapping: 74/74; synchronous
  SG/PnP/POWER: 153/153; source contracts: 69/69; continuation: 17/17;
  complete Dry suite: PASS.
- Default stress: DMA 5,000/5,000, INTEN 1,000/1,000. Expanded: DMA
  50,000/50,000, INTEN 10,000/10,000.
- Passive installed baseline: `oem102.inf`, `0.2026.1022.1`, device OK,
  service running, loaded SYS SHA-256
  `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`.

## Remaining proof obligations

1. Integrate the offline-qualified transaction, explicit WDM-v3 mapping owner
   and one legacy acquisition request without changing the public ABI; join
   the borrowed MDL/request to live remove-lock and publication rundown.
2. Wire live POWER and QUERY/CANCEL PnP policy without waits under spin locks
   or premature mapping/MDL release.
3. Establish hardware MAM and MTT stop, descriptor-fetch cessation, final PCI
   write ordering and a physical retirement predicate from RTL/vendor evidence
   or a recoverable analyzer experiment.
4. Only then prepare a separately versioned, signed and authorized candidate.

No driver was signed, installed or loaded, and no hardware operation occurred.
