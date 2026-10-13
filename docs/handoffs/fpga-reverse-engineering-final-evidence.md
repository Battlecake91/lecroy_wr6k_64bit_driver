# Final FPGA Reverse-Engineering Evidence

Review baseline: research branch `analysis/pci-dma-evidence-20261006`,
`7108ee7638da021094b7d22959d55a7a4ec0cb7b`; no later commits at review start.
Project Combine: `234343d23e737e57f2727630e19008b509d7d522`.
This is the current-phase evidence index, not a hardware qualification or a
production merge recommendation. Private images, full decoded networks and
schematics are not included. **Installed FPGA/MCU image identity: Unknown.**

## Evidence Rules and Navigation

The inventory has **17 Verified, 8 Conditional, 4 Inferred and 10 Unknown**
items, identified below. These count review items, not independent safety proofs.
Unqualified Python tool names below are under `tools/fpga/`; tests are under `tests/dry/`.
Verified means physical drawing facts or configured-image/local software facts
at the stated scope. Conditional records its assumptions; Inferred is a semantic
interpretation; Unknown identifies a missing artifact. No local SMT identity is
a whole-device DMA safety proof.

- [Hardware topology](../pci-card-acquisition-board-topology.md): physical boards, bonds and interconnections.
- [Firmware analysis](../pci-fpga-firmware-analysis.md): image provenance and configured functions.
- [DMA retirement](../pci-dma-retirement-analysis.md): exact equations, timing cuts and formal limitations.
- [Implementation owner](fpga-dma-implementation-owner-handoff.md): software consequences and activation gates.
- [Missing evidence](fpga-dma-missing-evidence.md): provider-specific requests, not new experiment instructions.

## Recovered Architecture

```text
C161PI static loader -- separate serial clocks --> PCI-card U3
                   -- acquisition chain -------> U21 DOUT -> U38 DIN

host PCI <-> U3 local target/BAR logic <-> 12-lane LVDS commands/status <-> U21
                                                                        |
                                                U38 ATC/MTT control; separate MAM/HAD producers
                                                    [last-word/stop contract Unknown]
                                                                        |
U3 receive two256x16 BRAMs <- LVDS data/control <- acquisition-controller TX
             |
       16x32 staging RAM -> conditional PCI payload output alternative

U3 descriptor/counter state -> F -> L -> C -> sticky IRQ -> masks/inhibit -> INTA#
```

The diagram separates physical connectivity from configured data alternatives;
it is not a demonstrated complete acquisition execution. U3 is the PCI-card
XC2S200E, U21 the acquisition controller, U38 the ATC/MTT timebase FPGA.
Neither U3 nor every forwarded BAR access is an ADC/MAM producer. The C161PI
loader has separate PCI and acquisition clock loops, a DONE-high bypass, a
one-edge DIN lag and an acquisition length exceeding the resource by six bytes.
The static reset path enters6D6C, which calls the PCI loop6F1C first
(base+0x180000,180260 source bytes), then the acquisition loop
(base+0x100000,360526 source bytes). The PCI source length exceeds decoded205
by eight bytes; contents and padding are not assumed. Initial base0 is not a
universal bank invariant. This does not prove successful/current configuration.

## A. Verified Findings

| ID | Finding and exact scope | Primary evidence / checker |
|---|---|---|
| V01 | PCI default resource BINARY.205 is180252 decoded bytes; valid packet/frame baseline,2240 main frames plus retained final transfer frame | [Image/frame baseline](../pci-fpga-firmware-analysis.md#spartan-iie-bitstream-packetframe-baseline-private-offline-analysis-2026-10-06); `tools/fpga/virtexe_xc2s200e_decode.py` |
| V02 | BINARY.204 contains two180260-byte configuration streams,360520 bytes total; resource assignment is the device-pack default, not live identity | [Embedded images](../pci-fpga-firmware-analysis.md#confirmed-embedded-configuration-images-in-s65-device-pack-2026-10-06); see hash manifest below |
| V03 | C161PI sparse resource has56792 mapped bytes; static reset/call path and separate acquisition/PCI serial loops recovered | [C161PI loader](../pci-fpga-firmware-analysis.md#c161pi-reset-bank-and-serial-loader); `tools/fpga/c166_firmware.py:inventory,serial_word_loop` |
| V04 | PCI-card U3 is XC2S200E-6PQ208 behind PCI bus switches, with separate12-data-pair receive/transmit headers | [PCI card](../pci-card-acquisition-board-topology.md#directly-visible-pci-interface-card-pci-cardpdf-sheet-1); schematic facts, not installed stuffing proof |
| V05 | Acquisition U21/U38 are separate XC2S200E-FG456 devices; U21 DOUT connects U38 DIN, shared CCLK; U21 has external SDRAM | [Child sheets](../pci-card-acquisition-board-topology.md#acquisition-controller-child-sheets) |
| V06 | U38 START=N2, SACQ=M1; SACQ returns to U21 W3. Stream1 uses N2 input/M1 output and D2/D1 clock outputs | [U38 identity](../pci-fpga-firmware-analysis.md#u38-identity-and-timebase-shutdown); physical/configured fingerprint, not live role certainty |
| V07 | START=BAR0+0x00C, ACQFVER=BAR1+0x00C; SGTA/IIMTC/IIMCL/IIMST/INTST/INTEN are BAR0+0x040/044/048/04C/080/084 | [Register map](../hardware-register-map.md); [target decode](../pci-fpga-firmware-analysis.md#focused-pci-target-mmio-decode-2026-10-10); not every BAR register resides locally |
| V08 | PCI-to-LVDS address/data serialization, twelve lanes, three temporal slots, parity/qualification and conditional return word recovered | [Link muxes](../pci-fpga-firmware-analysis.md#exact-link-muxes-and-legacy-command-projection); `tools/fpga/virtexe_acquisition.py:command_frame,receiver_fields,transmit_fields` |
| V09 | U21 has independent MTT19-bit/MAM24-bit comparisons, command-load and physical START paths | [Command loads](../pci-fpga-firmware-analysis.md#command-loads-and-physical-start); equality is not producer completion |
| V10 | U38 configured START level/edge paths and delayed SACQ SRL retain state; applicable dual SR/REV local behavior modeled | [START/SACQ cones](../pci-fpga-firmware-analysis.md#actual-start-and-sacq-cones); no sampling-stop implication |
| V11 | SGTA30/IIMTC32 capture alternatives, two32-bit descriptor-word banks, counter D equations, pointer step8/master-address step4 recovered | [Descriptor transitions](../pci-dma-retirement-analysis.md#descriptor-capture-and-counter-transitions); `virtexe_dma.py:focused_evidence,capture_map,pointer_map` |
| V12 | Receive storage is two256x16 BRAM halves plus a separate1024x4 auxiliary BRAM; read/write clock domains and retention contracts recovered | [Receive closure](../pci-dma-retirement-analysis.md#receive-to-payload-evidence-closure); not an ideal FIFO assumption |
| V13 | PCI staging is32 RAM16X1D cells,16x32 total; write'=write+W, read'=read+issue-rewind modulo16 | [Staging](../pci-dma-retirement-analysis.md#resolved-longline-route-and-payload-staging); `virtexe_dma.py:payload_map` |
| V14 | Receive modulo256 pointer arithmetic, threshold224, C+W read lookahead, periodic modulo8/modulo4 publication and pre-edge DOB sampling recovered | [Receive closure](../pci-dma-retirement-analysis.md#receive-to-payload-evidence-closure); `virtexe_dma_handshake.py:receive_data_checks,receive_publication_checks` |
| V15 | Segment-zero/load/pending -> F -> L -> C -> IRQ equations and16 FF contracts verified; C is retained while cl1, IRQ ACK is set-dominated | [Completion boundary](../pci-dma-retirement-analysis.md#completion-and-irq-proof-boundary); `virtexe_completion.py:completion_contracts,completion_queries` |
| V16 | INTA# configured low/Hi-Z: O=0,T=G4 OR !OR(source[i] AND INTEN[i]); all8192 source/mask/inhibit assignments checked | [IRQ lifecycle](../pci-dma-retirement-analysis.md#interrupt-lifecycle); `virtexe_completion.py:interrupt_checks`; masking does not clear C/IRQ |
| V17 | Decoder, configured logic and symbolic tests have explicit image/pin/initialization boundaries; current execution evidence is recorded separately below | `tests/dry/test-fpga-decoder.ps1`; `virtexe_routing.py:register_contract`; `virtexe_symbolic.py:System` |

Hash manifest (decoded private resources; no bytes distributed):

| Image | SHA-256 |
|---|---|
| PCI BINARY.205 | `f2195ae57bb87bb3ef6ea27cc70771c451d1be6de972a87b7e861be6dcd970ab` |
| BINARY.204 stream0 | `227cba1ae543cf78b5b6d5700f719340e18df63850ec270eea47b5855c404337` |
| BINARY.204 stream1 | `e3a679c4fa4304ca4d9e7387c099e1245d28c9db56b3d436f6e2a14095fed68b` |
| C161PI sparse address/byte manifest | `c81c6023eda3ac9618f5a4934a4e25ccf39a4d06c7d44601b2c858300f91dcf3` |

## B. Conditional Findings

| ID | Result | Required assumptions / limitation |
|---|---|---|
| C01 | Initialized descriptor-prefix executions reach internal fetch/capture | Configured INIT/reset, explicit PCI-only edges, fixed host schedule, mapped responder and bounded responses at40/52 edges; no full payload-completion execution |
| C02 | Capture/readback alternatives carry selected CPU or descriptor data | Correct TBUS driver ownership/selection and target phase; wired-AND architecture does not imply one-hot ownership |
| C03 | Output FFs hold when PCI_CE=0; diagnostic CE1 advances them | A selected PCI edge and actual FF contract. Diagnostic CE1 is not the recovered hardblock function |
| C04 | Distributed state/pointer replicas agree | Matched initialization, reset, D/CE and effective clock history; arbitrary runtime states may differ |
| C05 | Published pointer/snapshot represents coherent binary state | Applicable CDC hold/setup, phase and no-lap invariant; periodic sampling is not acknowledged publication |
| C06 | BRAM/staging timing preserves the modeled prior DOB word | Applicable startup, enable/reset, collision and effective clock contracts; no global first-word or cross-domain ownership proof |
| C07 | IRDY#/TRDY# labels an accepted external phase | Card ownership, output enable/sampling and complete legal PCI history established independently; delayed internal A is not proved equivalent |
| C08 | Payload token/occupancy observers express conservation | Correct initial valid/wrap state and supplied word identities; observers never constrain hardware or prove full execution |

Sources: [initialized prefix](../pci-dma-retirement-analysis.md#initialized-pci-launch-prefix-and-the-remaining-sequence-boundary),
[defensive evidence review](../pci-dma-retirement-analysis.md#defensive-evidence-review),
`virtexe_dma_reachability.py:initialized_prefix`, `virtexe_dma_handshake.py:projection,word_observer`.
The completion13-query set has12 local mismatch UNSAT and1 runtime-cut SAT;
all bases SAT, no UNKNOWN. Its nonzero bookkeeping witness is not a reachable
remaining-DMA execution. Earlier whole-network timeout results remain UNKNOWN,
not evidence of safety. No bounded UNSAT is promoted to an unbounded theorem.

## C. Inferred Findings

| ID | Interpretation | Why not Verified end-to-end |
|---|---|---|
| I01 | BINARY.204 stream0=U21, stream1=U38 | Strong pin/loader/chain correlation; selected bank, exact serial bytes and successful configuration unknown |
| I02 | Word-A is descriptor address/sentinel and Word-B count | Matches original count/address format and local counters; exact fetched entry identity/units/retirement unproved |
| I03 | Auxiliary1024x4 memory carries metadata/control | Input/output role supports interpretation, not confirmed external sample semantics |
| I04 | MTTRGO/MAMRGO are operation-level producer launches | Driver and load/count paths agree; downstream execution, matched completion and stop/last-word contract absent |

## D. Unknowns and Exact Evidence

Request IDs link to [provider requests](fpga-dma-missing-evidence.md).

| ID | Missing evidence | Request IDs |
|---|---|---|
| U01 | Installed U3/U21/U38/MCU images, effective bank and assembly/configuration success | C1 |
| U02 | Applicable Spartan-IIE PCI_W_VE PCILOGIC/PCIIOB sampling, enable and reset timing | X1, X2 |
| U03 | Independent MTT/MAM/HAD producer stop and last admitted word | A1 |
| U04 | Matched durable command/stop acknowledgement and queued LVDS words | A2 |
| U05 | Descriptor stop-to-no-fetch/chain/reload invariant | P1, P2, X1 |
| U06 | Owned final PCI acceptance/retry/payload retirement before IRQ | P1, X1, X2 |
| U07 | Valid/wrap/CDC occupancy and final RX/staging/reissue conservation | P2, A2, X2 |
| U08 | Actual bridge write drain, ordering, CPU visibility and adapter coherency | H1, H2 |
| U09 | Physical IRQ epoch and stable inactivity across retained state until restart | P1, P2, A1 |
| U10 | General device-logical address/IOMMU and complete future mapping lifetime contract | H2; implementation-owner activation gates |

## Authoritative DMA Release Matrix

The **single authoritative matrix** is
[`pci-mmio-register-evidence.json::shutdown_evaluation`](../pci-mmio-register-evidence.json).
Its seven named rows contain status, strongest local evidence, missing invariant,
smallest artifact/provider, driver consequence and request IDs. The top-level
six-item `release_obligations` and `stable_until_explicit_restart` are summary
indices, not independent criteria. Do not maintain a second release matrix here.

All seven rows are **Unknown**: producer stopped; descriptor engine stopped;
no new master transactions; outstanding transactions terminal; buffers drained;
host writes completed/ordered; stable inactivity until explicit restart.
The current successful legacy PFN cleanup is a compatibility exception, not
an eighth proof path. `CompletionObserved != IdleProved`.

## Validation

Final consolidation rerun on2026-10-13: **374 PASS,0 FAIL,0 SKIP** Python tests
(18 MCU,45 routing,52 MMIO,142 DMA including17 completion/IRQ tests,
65 acquisition,52 synthetic SMT). Decoder self-test/private frame calibration,
**50/50 native architecture** and **19/19 PCI local-equation** checks passed.
Existing private reports/images and the pinned adapter were reused; no new
reconstruction or live transactions were performed. A final11/11 synthetic
DMA/JSON-consumer rerun overlaps the full suite; six local documentation checks
passed (matrix schema/status, finding IDs, provider IDs,42 file/heading links,
totals and UTF-8/publication checks). These are not extra unique FPGA test cases.
The previous332 driver dry cases are historical, not rerun here; WDK build
and hardware execution were not required or performed.

Reproduction: `tests/dry/test-fpga-decoder.ps1` with Python/Z3, the pinned native
adapter and private inputs configured. Required variables are `PYTHON`,
`PYTHONPATH` (symbolic dependencies), `WR6K_FPGA_SYMBOLIC=1`,
`WR6K_PRJCOMBINE`, `WR6K_ROUTING_ADAPTER`, `WR6K_FPGA_BINARY_205`,
`WR6K_MMIO_REPORT` (including INTA#), `WR6K_ACQUISITION_REPORT`,
`WR6K_ACQUISITION_BINARY`, `WR6K_ACQUISITION_SECOND_REPORT` and
`WR6K_C166_MEMORY`. Missing private inputs must be reported as SKIP, not PASS.
See [completion replay](../pci-dma-retirement-analysis.md#completion-and-irq-proof-boundary)
and [routing reproduction](../pci-fpga-firmware-analysis.md#configuration-aware-routing-tool-and-verification-boundary).
No private material is required to read or independently review this package;
private regression replay requires authorized access to the matching inputs.

## Offline FPGA Research Completion Status

- **Completed from currently available evidence:** scoped loader/resource,
  physical topology, routing, MMIO/link, descriptor/counter, receive/staging and
  completion/IRQ reconstruction with explicit local proof boundaries.
- **Partially completed with identified external blockers:** all seven DMA
  release/stability obligations; provider requests identify exact missing inputs.
- **Remaining independently solvable offline work:** no specific investigation
  using only the current evidence is known to materially change the DMA safety
  assessment. Documentation maintenance and regression reruns are not new proofs.

Further general firmware-only reconstruction or deeper-bound symbolic searching
is not currently justified. Resume only for a concrete contradiction or a
new applicable architecture, identity, correlated trace or platform artifact.
PR #9 remains draft/open/unmerged; no production, hardware or quarantine change.
