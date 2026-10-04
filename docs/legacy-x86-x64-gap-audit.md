# Legacy x86 to native x64 gap audit

Baseline: `main` at `4d42cc82e18ac6ecde65d57cb67149cc03908f1d` (2026-10-04).
Input: static original-Ghidra-to-native-source comparison handed off by the analysis workstream. This is **not** a new build, runtime test, or hardware verification. Selected native findings were cross-checked against the baseline source. Findings and proposed remedies below are not claims that a fix has landed.

## Scope and evidence levels

- **Source-confirmed:** a behavior or missing synchronization is visible in the inspected native implementation; does not prove a crash has occurred.
- **Analysis-reported:** static comparison with original Ghidra exports, requiring targeted reinspection for implementation specifics.
- **Runtime-verified:** only previously recorded hardware outcomes, not new validation of this audit.
- **Intentionally gated:** a missing or disabled behavior retained for device safety pending isolated validation.

The original executable reconstruction is substantially complete, but native x64 behavioral equivalence is not. Preserve the working PCI BAR mapping, startup probe, ISR/DPC acquisition core, MDL/token hardening, 32-bit DMA range checks, safe status read-and-clear, and restrictive unknown-IOCTL handling. Do not reproduce legacy unsafe pointer handling or invalid completion behavior.

## P0: driver lifetime and hardware quiescence

1. **Remove/outstanding I/O (source-confirmed).** `driver/Device.c:LecS65Pnp` STOP/SURPRISE_REMOVE/REMOVE releases events, transfers and BAR mappings without first draining all device users. REMOVE eventually calls `IoDeleteDevice`. Add per-device request/remove lifetime tracking and explicit Running/Stopping/Removing/Removed admission states. All device-dependent entry points and asynchronous work must participate; stop new admission before shutdown and wait for admitted work to finish before resource destruction. Review PnP forwarding/completion and cancellation while doing so.
2. **IRQ, DPC, DMA (source-confirmed risk, shutdown details not yet proven).** `LecDisconnectInterrupt` disconnects the ISR and resets software shadows without first applying a known final hardware interrupt mask. `LecInterruptDpc` accesses transfer pointers and device state, while `CurrentTransfer` has no demonstrated independent lifetime pin. Add hardware quiesce while BARs remain mapped, correct ISR/DPC drain/cancel ordering, DMA cancellation and actual bus-master inactivity guarantees before descriptors/MDLs can be freed. Do not assume a timed-out DMA request means hardware has stopped. The shutdown register sequence must be derived and validated, not guessed.
3. **Embedded timer (source-confirmed).** `LecLegacyTimerArmOrExtend` initializes and arms an embedded `KTIMER`, but PnP shutdown has no matching timer cancellation/state reset. Cancel and coordinate concurrent rearming before device destruction. Verify timer use paths, not merely presence of `KeCancelTimer`.
4. **START without connected IRQ (source-confirmed).** `LecS65Pnp` sets `Started=TRUE` and enables device interfaces even if `LecConnectInterrupt` fails. Active acquisition/DMA must be prohibited without completion IRQ; recommended default is failure and rollback of the partial start. An optional passive-only diagnostic state would require explicit enforced operation gating.

**Required software tests:** parallel requests plus Stop/Remove, transfer-DPC overlap, timer rearm at Stop, start-IRQ injection/rollback, remove-lock race and Driver Verifier (I/O verification and deadlocks). Avoid deliberate destructive DMA/IRQ races on the only productive licensed scope.

## P1: lifecycle correctness and ABI behavior

- **PnP/Power (source-confirmed partial dispatch):** original PnP `0x1A6EA`, power `0x1B096`, quiesce `0x138D4`, restore `0x1381E`. Native `LecS65Pnp` handles only START, STOP, SURPRISE_REMOVAL, REMOVE; `LecS65Power` forwards without coordinating D0 exit/return. Implement query/cancel/forward/completion semantics and a tested device quiesce/restore; invalidate or restore `LegacyMamShadowInitialized`, `LegacyMamSeqShadowInitialized` and applicable command/IRQ states.
- **CREATE/CLOSE/CLEANUP (analysis-reported, dispatch absence source-confirmed):** original `0x10E3A`, `0x10EAF`, `0x10F30`, `0x13768`. Native lacks dedicated `IRP_MJ_CLEANUP`; `CLOSE` frees process transfers unconditionally. Model file-object ownership, process references, wait/cancel IRPs and event reference releases together. Moving one call mechanically from CLOSE to CLEANUP is insufficient.
- **Register coherence (source-confirmed direct-write gap):** original `CFDC21C4` at `0x1272A` maintains INTEN software state while native `LecIoctlRegisterWrite` directly calls `WRITE_REGISTER_ULONG`. Route validated INTEN/ERRM/SPICTL writes through coherent shared state. `0x00223040` type-2 descriptor readback needs descriptor caches, not arbitrary side-effectful MMIO reads (original `0x123B4`, `0x124C2`).
- **Event replay (analysis-reported):** original `CFDC2180` at `0x128F8` signals a just-registered event when enabled pending command bits already exist. Native capture clears the event without replay. Check `(LegacyCommandPendingMask & LegacyCommandEnableMask) != 0` under `LegacyEventLock` and preserve correct reference lifetime.
- **CFDC2110 (intentionally gated):** existing structural guard is safer than unrestricted original-command execution. Build a full per-family/per-opcode matrix and implement validated commands one by one. Examples missing: family 1 opcode 0x92 and family 2 opcodes 0x00, 0x03, 0x04, 0x09, 0x0A, 0x40.
- **Acquisition (partly gated):** `CFDC2138` handles an observed one-channel variant, not the complete variable-size multi-channel/MAM sequence. `CFDD219F` METHOD_NEITHER acquisition is deliberately disabled pending robust user-pointer/WOW64/cancel and DMA lifetime handling.
- **Required write features, not optional omissions (intentionally gated):** `0x00223088` Dallas WRITE requires original 1–512-byte write, 32-byte scratchpad chunks, copy/check/retry and full readback verification; test only with disposable DS2433 hardware. `CFDC2130` serial FPGA/GPIO programmer uses BAR1 GPIODAT mask `0xE000` (bits 15:13), preserving other bits and synchronizing shared register ownership; validate on recoverable hardware only.

## P2 and generic IRPs

- `0x00222C00` / `0x00222C04`: delayed operation and buzzer flag/hardware semantics differ.
- `0x00223000` / `0x00223004`: trace configuration is not dynamically maintained as in original `0x12ADA` / `0x12A5E`.
- Original READ/WRITE `0x118E4` queues packets, and 19 unspecified IRP majors route to `0x10C18` (not implemented), whereas native defaults to pass-through. Evaluate WDM-compatible behavior and cancel semantics, not blind imitation.
- Review AddDevice interface registration rollback.

## Original 27-case IOCTL coverage

The original dispatcher `0x11018` has **27 distinct cases**. The native dispatcher contains **25 of those original cases**: **24 native functional/partial cases plus `CFDD219F`, which is explicitly gated**. Both `0x00223088` and `CFDC2130` have no native case. Native-only/trace-discovered `0x00222400` and debug IOCTLs are not members of the original 27. Presence of a case does **not** establish full semantic coverage; notably CFDC2110 and CFDC2138 remain partial.

## Documentation corrections and unresolved evidence

- Original register names: BAR0+0x004 = ERRS; BAR0+0x008 = ERRM; BAR1+0x004 = CLRERR; BAR1+0x008 = CLRIRQ.
- Serial-programming GPIODAT bit mask = `0xE000`, **not** `0x0E00`.
- Reconcile final byte-coverage census in `legacy-driver-function-map.md` and `legacy-executable-code-coverage.md` against the definitive audit instead of mixing interim counts.
- Unresolved: true DMA inactivity on timeout; PFN addressing under IOMMU/DMA remapping; exact safe interrupt masking; multi-handle cleanup; power/state restore; multi-channel MAM sequencing; METHOD_NEITHER buffers and cancellation; disposable-hardware Dallas and FPGA validation; type-2 cache membership.

## Verification and implementation order

Previously recorded 9/9 safe PCI ABI and 11/11 hardware regression passes, working XStream waveforms and AP015 checks remain valid **only for their tested revisions and paths**. The latest SetOneRegister revision and extended dry tests were not proven installed/tested in this audit, nor were the lifecycle races tested.

Proceed in separable, reviewed changes:
1. I/O admission/removal and DMA lifetime, then hardware quiesce/IRQ/DPC/timer drain and IRQ-failure rollback (**P0**).
2. PnP/Power restore and process/file cleanup, register-shadow consistency and event replay (**P1**).
3. CFDC2110 matrix, additional acquisition and safe hardware write features (**P1**).
4. Trace/buzzer/generic IRP parity (**P2**).
5. Per-package software, controlled hardware, XStream and eventually HLK regression.

All changes affecting the only working scope require safe software-only validation first. The original driver's semantics are the compatibility target; its vulnerabilities are not.
