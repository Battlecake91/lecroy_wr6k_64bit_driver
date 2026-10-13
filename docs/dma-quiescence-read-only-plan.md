# Passive DMA quiescence observation plan

**Not executed; not authorization to access hardware.** This plan is limited
to existing recordings and, after a separate owner safety decision, strictly
passive observation of an independently scheduled normal acquisition. It
does not request a new timeout, abort, remove, reset or error experiment.
No custom MMIO writes, driver loads/reloads, IRQ injection, FPGA/Dallas
programming, busmaster-enable changes or DMA-buffer releases are permitted.

## Offline prerequisite

First use existing private x86 traces, if any, to find one normal transfer
with timestamps for launch, physical INTST, IIMCL write, IIMST read, DPC/event
and unregister. A log that records only IOCTL return values cannot establish
the physical ordering. The selected x86 exports already establish the command
sequences; repeating them is unnecessary. Match binary/board revision and
capture provenance. Do not publish raw traces or firmware-derived networks.

Before proposing status reads, obtain documentation of read side effects and
the exact hardware revision. INTST/ERRS/IIMST are **not presumed harmless to
read**. Do not add polling merely because the original driver reads once.
Observe already-occurring reads in a passive PCI capture instead. Reject a
capture method requiring new driver instrumentation or register access under
this plan; seek separate review if that later becomes necessary.

## Minimal capture

Prefer an already available PCI protocol-analyzer recording. If none exists,
only a separately approved non-driving analyzer may observe an existing normal
workload. Physical installation/reconfiguration is outside this plan. Record:

- Correct card identity, board/FPGA revision evidence, clock and analyzer
  sampling resolution, time synchronization and capture/drop limitations.
- Device-initiated descriptor reads and host-memory writes, distinguishing
  the WR6k initiator from CPU MMIO and other masters. REQ/FRAME alone cannot
  identify addresses, final data phases or retries/disconnects/aborts.
- CPU MMIO transactions and read results for the exact BAR/offsets in the
  [audit](legacy-dma-abort-bus-idle-audit.md), without creating new accesses.
- Physical interrupt timing if the capture supports it. Software DPC/event
  timing requires an existing correlated log, not synthetic interrupt input.
- Acquisition-link activity only if an existing passive capture is available.
  Missing link observation leaves producer/queue obligations open.

Keep private physical addresses, payload, proprietary data and complete
recordings outside the repository. Publish only a derived ordering result
with capture identity and uncertainty, not bulk vendor material.

## Expected observations and interpretation

| Observation | Interpretation |
|---|---|
| Device DMA after a documented candidate ack, attributable to the same transfer without restart | Counterexample to that ack's no-more-DMA contract, subject to correct master/generation attribution |
| INTST/event before final device host-memory access | Refutes that observed completion marker as a final-access certificate |
| Status clears while device DMA continues | Refutes status-clear-implies-quiescence for that sequence |
| No device traffic after a marker during a finite recording | Corroboration only; does not prove no future restart, FIFO empty or producer stopped |
| Gaps, other masters, unidentified firmware or unmatched timestamps | Inconclusive; do not interpolate missing evidence |
| PCI final data phase observed | Card-side termination evidence only, not automatic retirement of upstream posted writes or Windows adapter cleanup |

A passive normal capture cannot establish fault-path behavior or hidden FIFO
occupancy. Those require original RTL/register contracts or a separately
reviewed recoverable test design; they are not authorized here. The actual
host-bridge/DMA-adapter ordering contract must be established independently.
Do not infer unbounded correctness from repeated successful captures.

## Acceptance boundary

Only a complete, applicable six-obligation proof may justify a later software
release design. Observation can **disprove** a proposed ack with one correctly
attributed execution, but silent finite captures cannot certify it. Preserve
`UnknownActive`, pinned mappings and quarantine regardless of this plan's
capture outcome. Hardware execution and production-driver changes remain out
of scope for this analysis branch.
