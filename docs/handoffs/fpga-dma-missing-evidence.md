# FPGA DMA Missing-Evidence Requests

Provider-specific requests for the final offline phase at research baseline
`7108ee7638da021094b7d22959d55a7a4ec0cb7b`. They support the single authoritative
[`shutdown_evaluation` matrix](../pci-mmio-register-evidence.json), not a new
reconstruction backlog. See the [evidence index](fpga-reverse-engineering-final-evidence.md)
and [implementation handoff](fpga-dma-implementation-owner-handoff.md).

Request existing, legitimately available documentation, records or traces first.
No item instructs anyone to access hardware, inject an IRQ, launch DMA, reset,
program, read back a bitstream, update firmware or write Dallas EEPROM. If an
artifact does not already exist, describe the gap to the owner; any observation
plan needs separate authorization and safety review. Do not distribute private
vendor libraries, full networks, schematics, image bytes or license contents.

## Xilinx Architecture

### X1 - Applicable Spartan-IIE PCILOGIC Contract

- **Provider/artifact:** manufacturer-authoritative simulation/behavioral or
  timing documentation applicable to XC2S200E-6PQ208, Virtex-E PCI_W_VE PCILOGIC;
  identify release/device/speed-grade applicability and provenance. Existing
  licensed ISE/UNISIM/SIMPRIM materials may be inspected privately if available.
- **Minimum content:** effective function of I1/I2/I3 and dedicated IRDY/TRDY
  taps; polarity, PCI_DELAY role, PCI_CE edge/settling behavior, startup/reset,
  setup/hold and regional distribution to output FF enables.
- **Closes/limits:** permits actual output sampling rather than symbolic or
  diagnostic CE. Needed by descriptor stop, no-new-master, transaction-terminal
  and restart invariants; does not by itself prove any release obligation.
- **Already available / do not repeat:** fabric sources, routing, regional
  fanout and all36 AD/CBE FF CE selections. Project Combine revision
  `234343d23e737e57f2727630e19008b509d7d522` describes connectivity, not behavior.
  Spartan-6 PCILOGICSE, guessed NAND logic or a fixed CE value is not a substitute.

### X2 - PCIIOB, Effective Clocks and Memory Timing

- **Provider/artifact:** applicable old-family PCIIOB.PCI input/output/OE timing,
  dedicated tap polarity/sample stage, DLL/BUFGCE startup/reset/phase and BRAM
  enable/reset/registered-output/read-write collision contracts.
- **Minimum content:** which physical pin sample is consumed on each effective
  edge; when AD/CBE/FRAME/IRDY/OE become valid; legal phase relationships and
  simultaneous clock/memory cases for the decoded port modes.
- **Closes/limits:** dependencies of owned PCI acceptance, final output word and
  RX/staging CDC/retention invariants. A single local clock schedule, zeroed RAM
  or schematic clock-frequency label is not a substitute.
- **Source map:** [exact missing contract](../pci-fpga-firmware-analysis.md#pcilogic-source-audit-and-exact-missing-contract),
  [receive closure](../pci-dma-retirement-analysis.md#receive-to-payload-evidence-closure).

## Acquisition Hardware

### A1 - Independent Producer Stop and Last Word

- **Provider/artifact:** applicable LeCroy/custom-ASIC MTT/MAM/HAD639E/HTB645
  execution contract for the assembly/image under review, or an existing
  validated same-epoch producer/internal trace with its interpretation.
- **Minimum content:** effect of MTTCTL/START clear on every producer, independent
  MAM admission, identity/timing of the final sample/word, maximum remaining
  queued output, fault/timeout behavior, and conditions excluding restart.
- **Closes/limits:** producer-stopped and stable-inactivity invariants; contributes
  to buffer-drain proof. START=N2, SACQ=M1 and the local equality/edge equations
  are already known and must not be requested as missing topology.
- **Limit:** SACQ/START transitions alone do not certify no more sampling or no
  outstanding downstream work; a command address match is not execution.

### A2 - LVDS Queue and Matched Stop/Status Contract

- **Provider/artifact:** applicable acquisition-controller/producer protocol
  specification or existing correlated internal/link history for the same epoch.
- **Minimum content:** accepted command versus transmitted frame; unique command
  identity/epoch if any; stop ACK meaning and persistence; last data/control
  words, queue capacity/validity, backpressure/CDC and reset/restart behavior.
- **Closes/limits:** matched producer stop, internal queue drain and stable stop;
  may identify a usable status predicate only if its full meaning is proved.
- **Already available / do not repeat:**12 lanes, three TX slots, parity, address
  projection, U21 receive banks and command predicates. Repeating SYNC,
  link-stability feedback and sticky events are not operation ACKs.

## FPGA Configuration Identity

### C1 - Minimum Installed-Image and Assembly Records

- **Provider/artifact:** existing owner/manufacturer configuration records tied
  to the particular assembly: PCI-card/board revision and FPGA identities,
  selected resource IDs/flash bank, image hashes or equivalent version mapping,
  configuration source/stuffing and successful configuration evidence for U3,
  U21 and U38; include the controlling MCU version/bank where relevant.
- **Minimum correlation:** relate those records to the private resource/hash
  manifest in the [evidence index](fpga-reverse-engineering-final-evidence.md).
  A host DLL containing an update, default CVAR/resource ID, board photograph,
  DONE-high entry bypass or a version string alone is insufficient.
- **Closes/limits:** applicability of all configured-image claims to the actual
  device, not safety by itself. Current installed identity remains Unknown.
- **Already available:** static C161PI reset/call path, separate serial clocks,
  acquisition six-byte excess/one-edge lag, U21 DOUT->U38 DIN and strong stream
  role correlation. Do not repeat loader reconstruction or infer successful boot.

## PCI Transaction and Buffer State

### P1 - Same-Epoch Owned PCI History and IRQ

- **Provider/artifact:** an existing legally obtained correlated trace or
  applicable authoritative core simulation, with revision, initial state and
  timing provenance. Separately authorized observation only if evidence is absent.
- **Minimum fields:** SGTA/IIMTC and exact descriptor count/address/chain table;
  ownership REQ/GNT, AD/CBE and output enables; FRAME/IRDY/TRDY/DEVSEL/STOP;
  address/command and every accepted phase; retry/disconnect/abort histories;
  IIMCL/INTST/INTEN controls, terminal/C/IRQ or a justified correlation; physical
  INTA and source attribution. Include last/stalled/error transitions, not only
  a clean average waveform or an ISR timestamp.
- **Closes/limits:** descriptor stop, no-new-master, outstanding-terminal and
  old-source/restart invariants. Another master's pins, current software
  generation or an IRQ timestamp alone cannot identify final payload retirement.
- **Limit:** a finite successful trace validates that trace, not universal safety
  under all waits/faults. State precisely which invariant is established and
  which assumptions still need architecture or producer contracts.

### P2 - Same-Epoch Queue Validity, CDC and Reissue History

- **Provider/artifact:** existing validated internal trace or RTL/invariant
  evidence, correlated with P1 and A1/A2; privately reviewed, not a published
  complete extracted network.
- **Minimum fields:** initial valid/wrap state, RX write/read/published/snapshot
  pointers and effective clocks; staging write/read, W, issue/rewind/Q/V;
  descriptor pending/load/reload; pending output/DOB and final-word identity;
  queue admissions, consumption, rollback/reissue and collision/hold cases.
- **Closes/limits:** no-lap/valid occupancy, unique word retirement, all internal
  buffers drained, descriptor inactivity and retained-state restart exclusion.
  Modulo pointer equality, memory contents or issue counts alone cannot do this.
- **Already available / do not repeat:** complete RX/staging structure, pointer
  equations, periodic publication, local C/IRQ and all8192 INTA assignments.

## Host Platform

### H1 - Actual Bridge/Chipset Completion and Ordering

- **Provider/artifact:** existing machine topology/bridge/chipset identification
  plus applicable platform specifications and errata; identify conventional
  PCI path, posted-write buffers and relevant ordering domain.
- **Minimum content:** accepted endpoint write to CPU-visible memory guarantee,
  posted-write draining and ordering relative to sideband INTx; documented
  completion/fence mechanism if any, including reset/remove/error limitations.
- **Closes/limits:** host writes completed/ordered after endpoint retirement.
  Endpoint IRQ logic alone, CPU memory barriers or inactive PCI pins cannot
  establish upstream bridge drain. No guessed readback fence is prescribed.

### H2 - Windows DMA Address, Coherency and Ownership Contract

- **Provider/artifact:** selected Windows DMA adapter API/version, actual DMA
  remapping/IOMMU/platform constraints and applicable Microsoft DDI contracts;
  implementation design identifying owned resource/callback lifetimes.
- **Minimum content:** device-logical addresses for both descriptors and payload,
 32-bit limit handling, map/common-buffer ownership, correct completion/cache
  coherency/release operations and IRQL, callback cancellation/rundown, process/
  PnP/power lifetime and stable launch-admission exclusion.
- **Closes/limits:** mapping/address applicability and host coherency/software
  release obligations only after independently proved board retirement. A CPU
  PFN below4GiB is not general device-logical/IOMMU evidence. WDM staging stays
  no-launch and UnknownActive resources stay retained.
- **Source map:** [Windows obligations](../dma-lifetime-and-timeout.md#applicable-release-decision),
  [activation gates](fpga-dma-implementation-owner-handoff.md#exact-release-and-activation-gates).

## Stop Conditions for Further Offline Work

Do not request missing routing, loader pin mapping, descriptor-bank identity,
receive muxes or IRQ-cone extraction that is already in the evidence package.
Do not increase symbolic bounds, equate internal A with accepted writes, assume
empty queues, hard-code PCI_CE or insert an idle variable to obtain a proof.
No specific current-evidence-only investigation is known to close the seven
release/stability obligations. New artifacts must be reviewed for applicability
and scope before any status upgrade or separately approved implementation work.
