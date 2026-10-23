# Runtime qualification Batch 10 handoff

## Scope and baseline

Batch 10 started from PR #34 commit
`c5c1f8c25f324f7abc81c965ff85ffa7fddc77e6` in isolated branch
`codex/phase2c-runtime-batch10`. The installed baseline was passively confirmed
unchanged: `oem102.inf`, version `0.2026.1022.1`, device problem code zero,
service running and loaded SYS SHA-256
`D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`.
No device handle, IOCTL, MMIO, PnP, installation, signing, reboot, acquisition
or DMA operation occurred.

## Conclusions

- Neither original x86 behavior, replacement source, FPGA schematics nor prior
  successful captures proves physical DMA bus idle.
- Completion IRQ, DPC/event completion, `IIMCL=0`, `MTTCTL=0`, INTEN masking,
  delays and PnP rundown are not transaction-drain predicates.
- The active legacy implementation publishes CPU PFNs rather than Windows DMA
  logical addresses. Normal IRQ success could later free its resources based
  on an unverified ordering assumption. POWER is pass-through.
- WDM-v3 code is no-launch staging, not an acquisition transaction.

## Implementation

- `LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA` defaults to zero. Both legacy MAM and
  MTT functions return `STATUS_NOT_SUPPORTED` before DMA-related MMIO.
- The legacy builder rejects a descriptor table with nonzero MDL byte offset
  and rejects non-DWORD-aligned source device addresses.
- Source contracts bind those gates. No public IOCTL number or structure
  changed.
- The continuation script records exact repository, host, authorization and
  evidence state and can register a one-time limited interactive-logon task.
  Batch 10 used mocks only and registered no real task.

## Offline evidence

Debug and Release x64 built with WDK/SDK 10.0.28000.0 and MSVC 14.41.34120,
zero warnings/errors. Complete Dry passed, including 65/65 source contracts and
14/14 continuation cases. Default stress passed DMA 5,000/5,000 and INTEN
1,000/1,000; expanded stress passed DMA 50,000/50,000 and INTEN 10,000/10,000.

Final local build hashes (unsigned, not a package candidate):

| Artifact | SHA-256 |
| --- | --- |
| Debug x64 SYS | `293FB734E3F7ED97C32E10B88F6A750AB0583621EDC0B2B8D62F7DA5DDD6BF89` |
| Release x64 SYS | `842FBFD582A1CF676FD80B2072478B7DE2C04F0C6405947A716C072D99E633EF` |
| Debug PDB | `01D43825279AED3EF9BB17BCFE1DCC7C8D018F858E0152E9A804FE8F6F4B7862` |
| Release PDB | `BFEF63AF9D732AFAD6229D807A26A6F7B851129091D23E29059BAD4BB14F1585` |
| x64 lecdiag | `B5CEDBA5D730995557A321E92161C8D8F4DCCC695F055D8F69169F5DA7ECB120` |
| x64 lecwatch | `0FC546F653F891C39B39048749FA23B197748D6250D4E6FCF93E7666354EBD7E` |

Existing generation/phase, ISR/DPC, timeout, interrupt-cleanup and quarantine
diagnostics remain available. Batch 10 adds deterministic launch rejection and
alignment diagnostics but no high-frequency general tracing, public IOCTL or
register access. Future mapping/retirement counters belong in the WDM-v3
transaction once its ownership contract exists.

## Remaining gates

1. Obtain a hardware/RTL stop, drain and final-write ordering contract.
2. Complete the WDM-v3 logical-address transaction and PnP/power integration.
3. Add fake-DDI mapping/release faults and D0/Dx/CLEANUP ownership tests.
4. Physically isolate the production x86 disk before any active DMA test.
5. Approve and execute only the smallest externally observed transfer after
   all prior gates pass.

The exact architecture, state machine and experiment order are in
`docs/dma-architecture.md`, `docs/dma-lifetime-and-timeout.md` and
`docs/kernel-runtime-qualification.md`.
