# Read-only handoff: WDM scatter/gather callback and removal ownership

Current draft: `fix/p0-irq-start-dma-gating`, PR #3. This is an **analysis-only**
workstream. No code, register writes, device reloads, live x64 tests or
hardware fault injection. Keep the implementation chat as sole writer.

## Question

Before the inactive WDM SG bridge is wired into any active acquisition,
prove or disprove its callback, object-reference, MDL, adapter, SG-list and
PnP remove lifetime semantics under the exact Windows 10 WDK model.
Identify concrete race/correctness bugs, not speculative improvements.
The working licensed WR6k must not be used to test this.

## Inspect only

- `driver/DmaScatterGatherStage.c/.h`:
  `LecSgStageMap`, `LecSgStageReady`, `LecSgStagePeek`,
  `LecSgStageRelease`, `LecSgStageMarkLaunched`,
  `LecSgStageMarkIdleProved`.
- `driver/DmaMappingOwner.c/.h` and standalone
  `tests/dry/test-dma-ownership.c`.
- `driver/DmaAdapterStage.c/.h`:
  `IoGetDmaAdapter`, common buffer, `PutDmaAdapter`.
- `driver/Acquisition.c:LecBuildSourceMdlChain`,
  `LecFreeTransfer`, and release/registration paths.
- `driver/Device.c` PnP STOP/SURPRISE/REMOVE and `driver/Driver.c`
  remove locks, `LecEnterIoctl` and drain.
- Canonical `docs/dma-lifetime-and-timeout.md`.

## Verify with authoritative Microsoft WDK documentation

1. Exact IRQL/preconditions of `GetScatterGatherList`,
   `DRIVER_LIST_CONTROL` callback and `PutScatterGatherList`;
   synchronous vs delayed callbacks, whether a failed submission can
   still invoke callback; whether `KeRaiseIrql` usage is legitimate
   for our passive-only staging API.
2. Whether the callback may run *before* `GetScatterGatherList` returns,
   while START/REMOVE or a second thread is changing ownership; precise
   synchronization needed for `Stage->Submitted`, SG pointer and
   `PutStarted`, and premature freeing/releasing after callback.
3. Is `LecSgStagePeek`'s borrowed pointer sound if release occurs
   concurrently? What must be locked or reference-counted over the
   interval in which returned elements are encoded?
4. Adapter-reference, `DEVICE_OBJECT`, MDL/user-page, callback context,
   IOMMU map and `IRP_MN_REMOVE_DEVICE` lifetimes. Does object
   referencing alone permit safe teardown/detach? Account for the
   current deliberate quarantine on ambiguous DMA inactivity.
5. Mapped segment count, chained MDLs, map-register and descriptor
   page capacity limits; whether the DMA mapping callback may fail,
   duplicate, reorder or deliver fewer bytes than requested.
6. Determine the smallest **software-only** executable regression
   harness that would simulate delayed/reentrant callbacks, failed
   allocation, STOP/REMOVE, duplicate requests, and quarantined mappings
   without real hardware or guessed register operations.

## Response format

- **Confirmed findings**: severity, file/function, exact path/race,
  corresponding Windows API guarantee/source.
- **Not proved / assumptions**: differentiate undefined hardware idle
  and WDK callback semantics.
- **Implementation patch plan**: minimal safe ordering and state/lock
  model with exact required unit tests, and safety limits.
- State explicitly if inactive staged code must be kept unreachable
  because it cannot yet meet release requirements.

Do **not** assume a physical DMA IRQ or timeout indicates bus idle.
Do **not** recommend `PutScatterGatherList`, unpinning pages,
discarding descriptors or `PutDmaAdapter` after an unproven stop.
