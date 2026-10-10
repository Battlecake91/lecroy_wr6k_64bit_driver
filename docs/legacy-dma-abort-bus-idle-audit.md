# Legacy WR6k x86 DMA abort and PCI bus-idle evidence

## Original x86 DMA abort, bus-idle, and teardown evidence audit

Read-only correlation review against `f5d542f2130a1a457255658269c63dd29edab265`
(2026-10-10), including the existing bounded FPGA model. Primary evidence:
selected Ghidra C exports and corresponding assembly windows
`000171de`, `00017478`, `00012d6a`, `000160dc`,
`00013c84`, `000108d6`, `00011390`, `000137c4`,
`000138d4`, `0001381e`, `00012fde`, `0001a6ea`,
`0001b096`, `00010d46`, `00010d3c`, `000172a2`,
`00017fd6`, `00017f60`, `00018194` and `00017f8c`.
The cited symbols/offsets are specific to the original analyzed PE32,
not device datasheet definitions.

**Answer: NO verified bus-master idle/abort mechanism is established.**
Original-driver register writes and the routine names are not evidence
that all already-issued PCI transactions have drained. The cited
Ghidra paths contain no wait for a documented hardware quiescence
predicate, no measured read-back protocol that proves it and no board
specification establishing an IRQ-to-PCI-drain ordering guarantee.
This is a conclusion about **available evidence**, not proof that
hardware has no undocumented abort/reset command.

### Actual original transfer/descriptor sequence

1. Registration `0x1731C` allocates MDL(s) using `0x1807A`,
   reserves a `0x33000`-byte descriptor buffer and its MDL
   using `0x17F8C`, and builds 8-byte
   `{DWORD CountDwords, DWORD Address}` descriptors using
   `0x18194`. That builder shifts MDL PFNs by 12 and uses
   page links (slot 511) to the next descriptor page. It does
   **not** obtain IOMMU-remapped logical addresses using a
   Windows DMA adapter. Source-MDL lifetime is separate
   from the board's physical transfer lifetime.
2. MAM path: `0x13C84 -> 0x12D6A -> 0x17478 -> 0x171DE`.
   Family-1 MTT: `0x160DC -> 0x17478 -> 0x171DE`.
   MAM setup at `0x12D6A` first clears BAR1 `GPIODAT`
   bit 16 through `0x120DC`, prepares MAM settings,
   and calculates the launch count. These are not abort actions.
3. `0x171DE`: write BAR0 `SGTA` = descriptor-page
   address, write `IIMTC` = total transfer DWORDs,
   reset the selected transfer's completion event, enable
   `INTEN` bit 0 through synchronized virtual slot
   `+0x14 -> 0x13914 -> 0x11E46`, write BAR0
   `IIMCL=1`, then write **one of** BAR1
   `MAMRGO`/`MTTRGO` with the launch count.
   It waits up to five seconds on the software event,
   disables only `INTEN` bit 0 through virtual slot
   `+0x18 -> 0x13934`, converts
   `STATUS_TIMEOUT (0x102)` to
   `STATUS_IO_TIMEOUT (0xC00000B5)`, and returns.
   **There is no timeout-specific engine abort, reset,
   idle polling or DMA descriptor retention.**
4. Original ISR `0x108D6` reads BAR0 `INTST`
   (bit 0 for acquisition completion). For bit 0
   it writes `IIMCL=0`, acknowledges interrupt
   sources and queues the common DPC. DPC `0x11390`
   signals the selected transfer's event for pending
   software interrupt bit 0. A separate software path
   `CFDC2400 -> 0x12EDE -> 0x12EC2 -> 0x11390`
   can synthesize that pending bit without hardware
   `INTST` completion. Event signaling and ISR/DPC
   retirement therefore are **not proof of PCI bus-idle**.
5. The MAM postlude `0x12D6A` reads BAR0
   `IIMST` bit 0 and **conditionally** writes
   `IIMCL=0`, then reads `ERRS`; it has
   no repeat-until-idle loop. No definitive busy/idle
   polarity or no-outstanding-PCI-requests semantics
   for `IIMST` bit 0 can be inferred just from
   this branch.

### Exact observed register roles

| Register | Operation and observed value | Original code | Confidence / missing guarantee |
|---|---|---|---|
| BAR0 `0x040 SGTA` | Write 32-bit first descriptor-page address | `0x171DE`, `0x18194` | Direct, descriptor entry; no idle |
| BAR0 `0x044 IIMTC` | Write total DWORD count | `0x171DE` | Direct, length; no idle |
| BAR0 `0x048 IIMCL` | Write `1` pre-GO; write `0` in ISR or MAM postlude | `0x171DE`, `0x108D6`, `0x12D6A`, `0x1260E` | Direct writes; stop/abort/PCI-drain effects unknown |
| BAR0 `0x04C IIMST` | Read and test bit `0x1` before optional IIMCL clear | `0x12D6A` | Direct; bit polarity, outstanding transactions **unknown** |
| BAR0 `0x080 INTST` | Read IRQ source; acknowledge reported bits | `0x108D6`, `0x1260E` | IRQ state, **not bus-idle** |
| BAR0 `0x084 INTEN` | Set/clear bit `0x1`; write `0` when quiescing | `0x13914`, `0x13934`, `0x11E46`, `0x138D4` | Masks interrupt signaling; **does not stop DMA** |
| BAR0 `0x004 ERRS` | Read/acknowledge error bits | `0x108D6`, `0x1260E`, `0x12D6A` | Error status; **not bus-idle** |
| BAR0 `0x008 ERRM` | Write `0xFFFFFFFF` on cleanup/quiesce | `0x137C4`, `0x138D4` | Error mask; no DMA completion guarantee |
| BAR1 `0x064 MAMRGO` | Write MAM launch count | `0x171DE` | Direct MAM DMA start |
| BAR1 `0x084 MTTRGO` | Write MTT launch count | `0x171DE` | Direct MTT DMA start |
| BAR1 `0x080 MTTCTL` | Write `0` on teardown/init; family-2 opcode `0x02` writes body byte `0` or `1` | `0x137C4`, `0x1381E`, `0x14847`, `0x163B2` | Direct; potential MTT disable, **no drain proof**, not MAM abort |
| BAR0 `0x00C START` | Write `1`, delay, read bit 0 during initialization | `0x12FDE` | Initialization/reset-like action; neither DMA-specific abort nor idle certified |
| BAR1 `0x004 CLRERR`, `0x008 CLRIRQ` | Acknowledge translated error/IRQ masks | `0x108D6`, `0x1260E`, `0x126EE` | Acknowledgment only |
| BAR1 `0x0C4 GPIODAT` | Clear bit `0x00010000` during MAM preparation | `0x120DC` | Mode setup, not DMA stop |
| BAR1 `0x040 MAMDAT`, `0x044 MAMPGO`, `0x060 MAMSEQ` | Program MAM data, `MAMPGO` trigger `0x105`, sequence | `0x12D6A` and configuration helpers | Acquisition setup, not DMA drain |

**Ordering caveat:** `0x107FE` stores the shadow and performs
`WRITE_REGISTER_ULONG`, establishing the sequence of issued
MMIO writes in `0x171DE`. That sequence and the later
event wait do **not** establish an FPGA-to-PCI transaction
drain/readback fence. No board-level memory ordering guarantee
was found in the supplied public repo material or publicly
searchable manufacturer register documentation.

### STOP, REMOVE, SURPRISE_REMOVAL and Power

- `IRP_MN_STOP_DEVICE`: legacy framework PnP
  `0x1A6EA` case 4 calls virtual
  `+0x8C -> 0x10D3C -> 0x1082E` to remove
  mapped MMIO ranges through `0x106A0` and disconnect the interrupt
  through `0x18648`; framework policy may also cancel
  software IRPs. There is **no identified polling for
  hardware DMA idle** on this branch.
- `IRP_MN_REMOVE_DEVICE`: `0x1A6EA` case 2
  calls `+0x94 -> 0x10D46 -> 0x137C4`,
  then IRQ/resource cleanup `0x1082E`.
  `0x137C4` first performs source acknowledge/clear
  through `0x1260E`/`0x126EE`, writes
  `MTTCTL=0`, writes `ERRM=0xFFFFFFFF`
  and requests synchronized `INTEN=0` through the
  `0x12EAE -> 0x11E46` callback (whose actual write is
  gated by `DAT_0001CD08 != -1`). There is no bus-idle
  readback.
- `IRP_MN_SURPRISE_REMOVAL`: PnP `0x1A6EA`
  case `0x17` selects policy/framework callbacks;
  no unconditional documented FPGA DMA drain sequence
  was established.
- `IRP_MN_SET_POWER` in `0x1B096` has a
  hardware-quiesce virtual `+0x120 ->
  0x10D6E -> 0x138D4` on supported transitions.
  That helper saves software IRQ state, writes
  `ERRM=0xFFFFFFFF` and requests synchronized
  `INTEN=0` (subject to the same callback gate); it
  **does not write an abort or
  poll DMA idle**. The restore virtual
  `+0x11C -> 0x10D62 -> 0x138B2 ->
  0x1381E` may issue `START=1`,
  `ITMODE=7` then `3`, initialize hardware
  and restore IRQ masks. These are
  initialization actions, not a proven
  recovery for an indeterminate in-flight
  DMA after timeout or device removal.
- On an acquisition timeout `0x171DE` merely
  drops completion IRQ enable and returns
  `STATUS_IO_TIMEOUT`; neither
  `0x137C4` nor `0x138D4` is
  called as an unconditional timeout abort.

### Additional focused shutdown and command findings

All MMIO accesses below are **32-bit** `READ_REGISTER_ULONG` or
`WRITE_REGISTER_ULONG`, directly or through shadow-write helper `0x107FE`.
Register identities are tied to the descriptor construction in `0x14847`,
not guessed from decompiler structure offsets. Evidence is the existing local
`ghidra_exports/selected/000<address>_FUN_000<address>.c`, matching `.refs.txt`,
`asm_asm_<address>.txt`, and `raw_10c18.asm.txt` exports. These private exports
are not redistributed. No new whole-program census was performed.

- **Shutdown is unsupported:** the major dispatch table routes
  `IRP_MJ_SHUTDOWN -> 0x18496 -> vtable +0x54 -> 0x10C18`.
  Instructions `0x10C1E..0x10C30` complete the IRP with
  `STATUS_NOT_IMPLEMENTED (0xC0000002)` and return that status. This path
  issues no MMIO, DMA stop, polling or resource release. This is not the
  REMOVE or power-down callback, and does not establish whether Windows
  actually delivers a shutdown IRP to this device.
- **Explicit MTT control, not verified DMA abort:** `CFDC2110` family 2,
  opcode `0x02 -> 0x166A8 -> 0x163B2`. A non-null timer wrapper at object
  `+0x186` supplies the dispatcher object at wrapper `+0x0C`. Constructor
  `0x158EE -> 0x157C4` initializes it with `KeInitializeTimerEx`; opcode
  `0x01 -> 0x1621A` arms it. At `0x163D8` the driver waits with
  `Executive, KernelMode, Alertable=TRUE, Timeout=NULL`. **The return status
  is ignored** before the 32-bit BAR1 `+0x080 MTTCTL` write at `0x163FD`.
  Body byte 0 writes 0; byte 1 writes 1; other values return local status 4
  without writing; null body returns 8. A local status-0 response is built
  by `0x15A88`, not read from hardware. There is no post-write readback or
  idle wait. Even timer expiration is not guaranteed by the unchecked
  alertable return, and timer expiration would not establish DMA idle.
  [KeWaitForSingleObject](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nf-wdm-kewaitforsingleobject)
  explicitly requires checking early alertable returns.
- **Pulses are not stop acknowledgements:** family-2 opcode `0x04`, also
  opcode `0x00` after its timer helper, reaches `0x16414`. It saves SW
  INTEN bit 2, disables that bit through `0x16074`, writes `MTTCTL=1` then
  `0` for the requested WORD count, conditionally restores that IRQ bit,
  and generates a local response. No status polling or drain is involved.
- **Interrupt/status cleanup is not reset/drain:** family-2 opcode `0x40`
  calls `0x16066` (local response 0x10), then
  `0x16490 -> 0x1260E -> 0x126EE -> 0x15A88` (local response 0).
  `0x1260E` reads BAR0 `INTST +0x080`; bit 0 conditionally clears
  `IIMCL +0x048`, bit 1 reads/acknowledges `ERRS +0x004` and maps error
  bits `0x400..0x4000` to BAR1 `CLRERR +0x004` bits 0..4; INTST bits 2/3
  write BAR1 `CLRIRQ +0x008` masks 1/2. Finally it acknowledges the read
  INTST value. `0x126EE` then writes `CLRIRQ=3` at `0x12707`,
  `INTST=0xFFFFFFFF` at `0x1271A`, and reads INTST once at `0x1271E`.
  **The read exists, but its value is discarded.** Neither helper loops
  until idle or clears IIMCL unconditionally. Opcode 0x40 does not itself
  write MTTCTL or START; calling it a complete board/DMA reset is unjustified.
- **REMOVE ordering is hazardous evidence, not an implementation recipe:**
  `0x10D46` calls the sequence above via `0x137C4`, then `0x1082E`.
  The latter calls `0x106A0` for three mapped resource wrappers before
  `0x18648` disconnects the interrupt. `0x106A0` conditionally invokes
  `MmUnmapIoSpace`; BAR unmapping does not revoke the device's host-memory
  DMA addresses. There is no intervening proven DMA-stop condition.
  STOP calls this same unmap/disconnect helper without `0x137C4`.
- **Error handling is source acknowledgement, not certified recovery:**
  ISR `0x108D6` latches ERRS, acknowledges translated CLRERR and ERRS,
  acknowledges INTST, and queues the DPC. Its 1-us stall and status reread
  can mask a persistent error; they do not poll descriptor, FIFO or PCI
  completion. Power restore/init (`0x1381E`, `0x12FDE`) is not invoked as
  a verified timeout recovery sequence. Software success from these
  paths must not promote a launched transfer to `IdleProved`.

### Cross-layer evidence matrix

**2026-10-10 target continuation:** The table below retains the end-to-end
classification. The newer [focused target analysis](pci-fpga-firmware-analysis.md#focused-pci-target-mmio-decode-2026-10-10)
and [JSON register matrix](pci-mmio-register-evidence.json) verify the address
capture bank, six configuration predicates and local DMA-offset write gates.
They identify three inferred IIMCL control replicas. Tile-owned HEX buffer
qualification resolves AD20 and the BAR0/DEVSEL route without choosing a
preferred driver. All three BAR comparator functions and conditional bit-0
status muxes are now verified. IIMCL/IIMST combine local and receive state;
BAR1 retains its previous value until a receive-selection pulse. None is an
accepted-read or idle proof. The dedicated PCILOGIC IRDY/TRDY connections are
now exposed from the pinned verifier; their PCIIOB.PCI tap behavior is not an
I/IQ alias. This Spartan-IIE PCI_W_VE tile has no encoded PCI_DELAY attribute,
unlike the different PCI_W_V tile. The exact applicable PCILOGIC.PCI_CE behavioral/timing model,
effective-clock/CDC contract, byte-qualified write/TBUS ownership, target read
acceptance and remote packet/status identity still block MMIO-to-idle semantics.
Eleven conditional address/payload fields and six fast transmit mux functions
are verified, including correction of address bit 10 (previously mislabeled 8).
Legacy MTTCTL/MTTRGO/MAMRGO offsets have distinct conditional address projections,
not certified packet opcodes or remote commands. Joint eight-edge BAR0 read
diagnostics require assumed CE=1; BAR1 remains waiting without remote edges.
A late-IRDY abstract early-release witness prevents generalizing this fixture
to a PCI wait-state/stability contract. It is not a reachable hardware defect.

Every entry labels the **connection**, not merely the existence of a register.
V = Verified, I = Inferred, U = Unknown. Physical PCI termination at U3 is
verified by package/card wiring; the implementation location of each register
and its downstream effects are separate questions. MTT/MAM are acquisition
functions, but BAR1 alone does not prove which FPGA implements a register.

| Legacy function -> MMIO | Register -> hardware block | Block -> configured FPGA state | Software-visible acknowledgement | DMA safety implication |
|---|---|---|---|---|
| V: `0x171DE`, SGTA/IIMTC, IIMCL=1, MAMRGO/MTTRGO=count | V: host path ends at U3; local storage and BAR1 staging distinguished; U: full remote implementation | V: local CE predicates/control replicas and conditional launch-address fields; U: accepted byte-qualified payload/packet | V: event wait; U: relation to all final host accesses | U: launch recorded, no stop/idle proof |
| V: ISR `0x108D6`, INTST bit 0 -> IIMCL=0 | V: conditional local IRQ bit-0 mux; U: complete accepted word and acquisition meaning | V: local IIMCL control-latch topology; U: clear-to-quiescence implication | V: physical ISR/DPC/event; U: drain semantics | U: completion observation is not release permission |
| V: `0x12D6A`, IIMST&1 -> conditional IIMCL=0, ERRS read | V: mixed local/receive bit-0 mux; U: accepted status semantics | V: conditional read source; U: outstanding/FIFO meaning | V: a single status sample; U: idle predicate | U: no repeat-until-idle or restart exclusion |
| V: `0x163B2`, optional timer wait -> MTTCTL=0/1 | V: BAR1 staging and conditional transmit source; I: MTT acquisition control | U: remote packet validity and producer/descriptor/PCI-start effect | V: locally synthesized response; U: hardware acknowledgement | U: stop request at most, not DMA cancellation proof |
| V: `0x1260E/0x126EE`, IRQ/error acknowledgements, discarded INTST read | V: host-visible status; U: source and forwarding semantics | U: pending/retirement state and clear priority | V: clear/read operations; U: durable idle acknowledgement | U: cleared status can conceal information, not prove drain |
| V: `0x137C4`, MTTCTL=0, ERRM=all ones, gated INTEN=0 | I: acquisition disable plus IRQ/error masking | U: no decoded fail-closed stop transition | V: return 0; U: post-stop hardware state | U: REMOVE cleanup is not a bus-idle certificate |
| V: `0x138D4`, ERRM=all ones, gated INTEN=0 | V: IRQ/error control access; U: physical producer effect | U: no proven transaction-admission guard | V: SW mask update; U: DMA acknowledgement | U: interrupt masking cannot certify DMA stopped |
| V: `0x18496 -> 0x10C18`, no MMIO | V: software-only unsupported handler | V: no FPGA command issued by this path | V: STATUS_NOT_IMPLEMENTED, not an idle ack | V: this path supplies no device quiescence evidence |

The configured model does verify REQ#/FRAME#/IRDY# pad O/T paths and local
PCI-clocked equations, and separate AD[10:22]/CBE output-register mappings.
The target continuation establishes an address-phase latch and partial
BAR/write/read structures, but not their complete transaction qualification or
stop semantics. There is no defensible substitution of a local REQ state FF
for IIMCL, or of an inactive pad for IIMST. The existing Z3 transitions also
allow conditional IIMCL-candidate clear with REQ still active (abstract SAT,
219 unknowns); this is not a hardware trace. No invented stop logic was added:
a free symbol named `stop_ack` would make the proposed safety checks vacuous.

### Six independent release obligations

All six device-specific conclusions remain **Unknown/Inconclusive**. The
following assumptions would have to become evidence; none is enabled in the
solver or driver by this review. No hardware-confirmed stop counterexample
has been obtained. A possible execution allowed by missing evidence is not
a decoded hardware execution.

| Obligation | Verified partial evidence | Missing assumption/dependency | Counterexample or limitation | Software observability |
|---|---|---|---|---|
| Producer stopped | MTTCTL=0 command; separate MAM/MTT and LVDS boundaries | Stop must block both relevant producers and already queued link data | No actual producer-stop trace; MTT command does not prove MAM stop | No correlated producer-empty/stopped status |
| Descriptor engine stopped | SGTA and descriptor layout identify supplied work | Consumption disable must be latched until explicit restart | Descriptor fetch after the command remains unexcluded | No identified descriptor-idle acknowledgement |
| New PCI transactions blocked | Local REQ/FRAME/IRDY equations are decoded | Stop/ack must dominate every start/retry path | Prior bound-16 SAT pin activity has **no stop antecedent**, so is not a post-stop counterexample | Inactive external pins are not a durable software predicate |
| Existing transactions complete | Physical PCI interface and sampled handshake paths | Exact terminal/retry/disconnect/abort FSM, including DEVSEL, must close | Ready-pair sample can belong to another master; outstanding state unknown | IIMST/IRQ relationship unproved |
| Internal buffers drained | Runtime distributed/BRAM arrays are modeled | Occupancy/pointer/valid state and link queues must be tied to ack | Zero memory contents or momentary bus inactivity do not imply empty | No correlated FIFO-empty/readback contract |
| Host-side completion | Windows adapter flush/release contracts are documented | Board completion ordering plus applicable platform/adapter contract | CPU barriers and adapter cache flush are not board-stop operations | DDI result is visible, but not a WR6k stop/bridge-drain certificate |

**Formal question disposition:** new starts after acknowledgement, active
transactions surviving acknowledgement, buffered restart, physical IRQ before
idle, timeout/error escape and reset hiding active state cannot yet be queried
as WR6k stop properties: the acknowledgement, stop decode and relevant state
predicates are uncorrelated. The software-injected completion route is a
source-established counterexample to **event implies physical completion**,
not a hardware counterexample to physical-IRQ ordering. Bounded SAT/UNSAT pin
queries remain exactly the limited observations in
[Formal State Analysis](pci-fpga-firmware-analysis.md#formal-state-analysis).
No bounded UNSAT is promoted to an unbounded stop proof.

### Smallest next evidence set

1. Identify a **single candidate stop command and durable readable ack** from
   original register/RTL documentation, or correlate a targeted target-address
   decode/read-mux cone. Required inputs are address latch, BAR select, write
   strobe/byte enables/data and the status read selection. Include forwarding
   to the acquisition FPGA if present. Additional x86 exports alone cannot
   establish the meaning of a hardware bit; the relevant existing exports
   already contain the exact commands and lack an idle polling loop.
2. Follow only that candidate's dependency cone: producer/link admission,
   descriptor consumption, start/retry guards, transaction termination and
   queue occupancy. Resolve missing routes, hard blocks, clock/reset and BRAM
   behavior **only where this cone depends on them**, not all unrelated logic.
3. Establish the running revision and completion/host-bridge ordering contract
   for the actual Windows DMA adapter path. Match retained descriptors and
   generations to observed transfers; IRQ/event traces alone are insufficient.

Further targeted offline target-decode or original-hardware-document analysis
is productive. Repeating the driver census or increasing an uncorrelated
solver bound is not. Actual executing revision and real ordering need
independent platform evidence, potentially passive hardware observation.
See the separate [read-only observation plan](dma-quiescence-read-only-plan.md).
That plan is not permission to run experiments or change quarantine.

### Offline review validation

Re-run against the unchanged tools and driver source on 2026-10-10:
frame self-test, **39 routing + 36 synthetic SMT tests**, private frame
calibration, **47 native architecture + 19 local equation/alias checks**, and
all eight driver dry suites (**35 source contracts + 276 native C cases**)
pass. No new synthetic stop tests were added because no new verified hardware
stop logic could be modeled; speculative controls are not test fixtures.

Re-running the existing target-response profile at bounds **2, 8 and 16**
reproduces **27 queries: 18 SAT, 9 UNSAT, no solver unknown/timeout**, with
satisfiable base constraints. The twelve separate arbitrary-runtime-cut
queries are SAT. All firmware results remain Inconclusive, and all 27 explicitly
deny an unbounded proof. These are regression observations, not new stop
counterexamples. The input network SHA-256 is
`298591591cafa294c49d0d8a04387f91a0cef7f0c20c40c5f217281f27bab671`;
networks, assignments and traces remain private. No Windows/WDK build,
driver-load test or hardware experiment was performed.

### Original x86 potentially unsafe release after timeout

For MAM, `0x13C84` resets
`hardware+0x100` (selected registered transfer)
after `0x12D6A` returns, **even if that call timed out**.
The separate unregister path `0x172A2`
finds that transfer and checks a software
flag at transfer `+0x0C` bit 0; it
**does not read an FPGA bus-idle register**.
Its cleanup `0x17FD6` can release the
descriptor MDL and allocation and uses
`0x17F60` for source MDLs, which calls
`MmUnlockPages` when previously locked.
Thus a subsequent unregister can reach
resource release without proving
physical bus inactivity. This is an
**original-driver safety gap/assumption**,
not evidence that a register-disable
operation drained transactions.

### x64 review at commit 87ee1638

- **Matched start and IRQ sequence:** active
  `driver/Ioctl.c:LecExecuteLegacyMttTransferLocked`
  (approximately lines 1870–2010) and
  `LecIoctlAcquireBufferedOneChannel`
  (approximately 3510–3830) write SGTA/IIMTC,
  reset the event, enable INTEN bit 0, write
  `IIMCL=1`, trigger MTTRGO/MAMRGO,
  wait five seconds and mask completion
  interrupts. `driver/Acquisition.c:
  LecInterruptService/LecInterruptDpc`
  mirror ISR `IIMCL=0`, INTST
  acknowledgment and completion-event
  signaling. Functional similarity is
  **not** proof of physical completion.
- **Improved unknown-activity safety:**
  `LecMarkDmaUnknownActive` latches
  quarantine before closing admission;
  `LecFreeTransfer` retains poisoned
  source MDLs and descriptor storage.
  `DmaPnpPublication` retains their
  ownership independently of FDO REMOVE.
  This is safer than the old driver's
  freely accessible unregister path,
  but intentionally leaks resources and
  cannot establish recovery.
- **Remaining normal-success hole in
  physical proof:** ISR
  `DmaCompletionIrqSeen` is a
  hardware-interrupt observation,
  **not** a bus-idle certificate.
  Currently a successful event plus
  that marker does *not* quarantine,
  and a subsequent unregister or
  STOP can free descriptors/MDLs.
  Without documented completion ordering,
  this is an **unproven safety premise**,
  not a demonstrated runtime fault.
  Do not use IRQ success as the
  `ProvenIdle` predicate in new DMA
  mapping code. Consider stale/unexpected
  IRQ/transfer-generation attribution
  separately in fake-ISR tests.
- **Only software teardown:**
  `Acquisition.c:LecQuiesceDeferredWork`
  masks BAR0 INTEN when MMIO is available,
  disconnects ISR, drains DPCs and
  cancels the legacy timer. `Device.c:
  LecS65Pnp` drains IOCTLs and retains
  unknown-active transfer storage.
  It does **not** implement the
  original REMOVE's `MTTCTL=0`
  or `ERRM=0xFFFFFFFF` writes;
  adding them blindly would **not**
  create a proven abort. Surprise
  removal deliberately avoids potentially
  inaccessible MMIO.
- **Staged API boundary remains non-live:**
  `DmaMappingOwner` can enter
  `IdleProved` only after external
  evidence. `DmaPnpStage` and
  `DmaSyncStage` only release a
  mapping from their deliberately
  no-launch path; the `ProvenIdle=TRUE`
  passed to `LecDmaReleaseAdapterContext`
  there denotes **no device launch
  capability**, not a measured board idle.
  The asynchronous `DmaScatterGatherStage`
  deliberately refuses all live
  launches/releases. `LecBuildDescriptorTable`
  still uses PFNs instead of
  device-logical DMA addresses; new
  active IOMMU mappings must not be
  enabled or released on IRQ alone.

### Evidence grades and recovery requirements

**Directly established:** register write/read order and
values, ISR/DPC/event relationships, removal cleanup,
MDL/PFN descriptor layout, the absence of an explicit
idle loop in the traced paths, and the x64 software
owner/quarantine behavior.

**Plausible but NOT certified:** IIMCL or MTTCTL
clear may affect future DMA requests; START/ITMODE
may reset some board state; INTST bit 0 may signify
end of a logical FPGA transfer. None proves all
posted or outstanding PCI memory transactions have
retired. A synchronization callback around INTEN
is an **IRQ serialization** domain, not a PCI
bus-memory fence.

**Not established:** documented IIMST bit-0
polarity, a WR6k DMA hardware-abort command,
MAM-vs-MTT abort independence, hardware
drain-on-reset behavior, exact time when IRQ
is generated relative to the final host-memory
write, bus-master disable/drain behavior, and
safe DMA mapping teardown after timeout,
power change or physical removal.

The safe acceptance test for an eventual
physical-idle predicate is **device and
platform supported evidence** that the DMA
engine will issue **no new** host-memory
accesses *and* previously issued reads,
writes and posted PCI transactions have
finished/are fenced for every active engine.
Do not treat IRQ masking, DPC rundown,
`KeWaitForSingleObject`, a five-second
timeout, `CancelMappedTransfer`,
`FlushAdapterBuffersEx`, `FreeAdapterObject`
or `PutScatterGatherList` as substitutes
for a board-specific DMA halt/drain contract.
Microsoft's documentation defines
`CancelMappedTransfer` for **system DMA**
transfer contexts and cautions that
premature `FlushAdapterBuffersEx` on
unfinished transfers may cause undefined
behavior; neither defines a WR6k FPGA
abort/bus-idle procedure.

**Next evidence, without touching the sole
licensed working scope:** look for FPGA
logic/regmap or vendor documentation of
IIMCL/IIMST, INTST assertion timing,
MTTCTL and START/reset, and PCI bus-master
posted-write draining. If none exists,
first create a standalone recoverable
test environment with independent bus
observation and sacrificial
DMA buffers before any hardware validation.
Meanwhile preserve unknown-active
pinned pages/mappings, do not claim
normal IRQ completion proves idle, and
keep WDM v3 live staging disabled.

External WDM contracts:
[CancelMappedTransfer](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pcancel_mapped_transfer),
[FlushAdapterBuffersEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pflush_adapter_buffers_ex).

## PCI-card FPGA and acquisition-link physical boundary (2026-10-06)
**PCI firmware evidence:** [PCI FPGA firmware analysis](pci-fpga-firmware-analysis.md) records the installed XC2S200E update image, checked pin-to-pad mapping and the partial INTA# routing trace. No decoded signal has yet demonstrated physical DMA bus-idle or posted-write drain.


Read-only corroboration against the separately documented, owner-supplied PCI
card schematic (`docs/pci-card-acquisition-board-topology.md`), the manufacturer
WaveRunner 6000 service manual (section 4.3, PCI Card, and section 4.6.1.3,
Controller FPGA), and an independent WaveRunner 6200 repair investigation:

- The conventional PCI connector's command/address/control, `REQ#`, `GNT#`
  and `INTA#` are connected, partly through Pericom `PI5C3861` level/bus
  switches, to PCI-card `U3`, a Xilinx `XC2S200E` Spartan-IIE FPGA.
  The card schematic shows no separate discrete PCI DMA/bus-master ASIC.
  This strongly locates the physical PCI initiator interface at `U3`, not
  the acquisition-board MAM/MTT blocks. The exact FPGA RTL remains unknown.
- Receive `J1` and transmit `J2` are separate 40-pin differential link
  headers (`D0..D11`, clock, sync, reset/error and stable-status nets).
  The acquisition controller FPGA is on the other end of that LVDS link;
  its producer/readout state and link queues must not be equated with PCI
  initiator or upstream host-bridge completion.
- The PCI FPGA contains the host-visible endpoint, while BAR0/BAR1 register
  effects can be implemented locally or forwarded downstream. A host-side
  register address or ISR event does not establish where the signal originates.
- The PCI-card `XC18V02` configuration PROM is shown as an **assembly option**;
  no evidence establishes that it is fitted on this specific card. A public
  S65 hardware report describes acquisition-microcontroller-provided FPGA
  configuration. Neither configuration path documents acquisition DMA drain
  or behavior of an unresponsive FPGA.
- The service manual describes the PCI card as a *transaction repeater*.
  This is the manufacturer's functional description, **not** evidence that
  its custom PCI/FPGA/link implementation implements a standard PCI bridge
  class or a specified Xilinx PCI LogiCORE version.

**Bus-idle proof needs two distinct hardware boundaries:** (1) acquisition
producers, both MAM and MTT, and the receive link can no longer enqueue
payload or DMA work; (2) PCI `U3` has finished descriptor reads and all
PCI host-memory writes, including any internal or upstream-bridge posted
transactions. A precise ownership and visibility contract for each boundary
is unavailable. Even a hypothetical `U3` master-idle bit would need to be
validated against posted bridge writes, and a remote controller idle flag
alone would not account for `U3`'s queued transactions.

PCI Local Bus Specification revision 3.0 section 3.2.5 distinguishes a simple
master from bridge devices and allows posted write queues in bridges, with
separate read/ordering rules. That specification describes permissible bus
behavior; it does **not** establish the exact LeCroy FPGA design or the
necessary ordering of the WR6k completion interrupt. Likewise, Xilinx PCI
IP's generic reset/handshake documentation cannot establish that LeCroy used
that particular IP or correctly wired application-layer drain signals.

A future positive abort/idle proof needs device-specific evidence of a
fail-closed stop of both transfer producers, in-flight descriptor fetches,
initiator state and write-buffer/bridge draining, including under timeout and
fault. No presently identified register (`IIMCL`, `IIMST`, `MTTCTL`,
`START`, `INTST`) provides that documented predicate. No live register or
reset tests are authorized by this research.

The focused offline [FPGA analysis](pci-fpga-firmware-analysis.md#complete-receive-word-and-conditional-pci-return)
now resolves all twelve TX/RX lanes, three temporal TX slots, sixteen address
fields, the complete 32-bit payload/conditional return word, parity and two RX
frame banks. MTTCTL/MTTRGO/MAMRGO offsets project to concrete address-phase
lane positions, but their acquisition-side dispatch and command-specific
acknowledgement remain Unknown. A repeating SYNC, a link-qualified returned word,
or X13,Y17 SLICE[0].YQ response event is not a completed-operation/drain flag.
The original MTTCTL=0 write and unchecked optional wait therefore still cannot
authorize DMA release. All six independent shutdown obligations remain Unknown;
see `pci-mmio-register-evidence.json::shutdown_evaluation` for exact obstructions.

Sources (schematic descriptions only; private drawings are not redistributed):

- [LeCroy WaveRunner 6000 Series Service Manual, section 4.3](https://www.manualslib.com/manual/2455899/Lecroy-Waverunner-6000-Series.html?page=25)
  and [section 4.6.1.3](https://www.manualslib.com/manual/2455899/Lecroy-Waverunner-6000-Series.html?page=30).
- [Independent 6200 PCI FPGA/LVDS inspection](https://www.eevblog.com/forum/repair/lecroy-waverunner-6200-repair/)
  (corroboration for a related unit, not this board's assembly revision).
- [WavePro/S65 platform comparison](https://www.eevblog.com/forum/testgear/lecroy-wavepro-7300-back-from-the-dead-%28and-x64-port%29/)
  (third-party configuration report; not proof of the fitted PROM variant).
- [PCI Local Bus Specification v3.0, section 3.2.5](https://studylib.net/doc/28279212/pci-spev-v3-0)
  (generic PCI ordering only).
- [Xilinx PCI Initiator/Target User Guide UG262, chapter 8](https://docs.amd.com/api/khub/documents/eCaBHdwRMTXeIXAGo49sIg/content)
  (generic later IP description, **not** a WR6k IP identification).
