# PCI DMA initiator and descriptor retirement

## Result and evidence scope

Offline configured-image analysis resolves the named payload-route ambiguity,
all descriptor-pointer/master-address bits, conditional PCI command/data
selection, staging pointers and the completion-source pipeline. It does
**not** establish a safe observable shutdown contract, even for normal IRQ
completion. All six release obligations and stability until explicit restart
remain **Unknown**. Preserve `UnknownActive`; no production source changes,
hardware accesses or mapping-release permission follow from this work.

Evidence: existing private BINARY.205 decoded network, original x86 control
flow and current x64 source. Project Combine is pinned to
`234343d23e737e57f2727630e19008b509d7d522`. `Verified` below describes configured
topology/local equations, not the installed hardware revision. The checks are
in `tools/fpga/virtexe_dma.py`, `tools/fpga/virtexe_dma_handshake.py` and
`tests/dry/test_fpga_dma.py`; complete private
networks and capture maps are not published.

## Initiator state and protocol boundary

The existing 19 PCI local equations remain the canonical control-output model
in [FPGA analysis](pci-fpga-firmware-analysis.md#formal-state-analysis).
The following table adds only the DMA-relevant interpretation and exact cuts.
PCI target BAR/read-mux state is not an initiator completion state.

| Phase | Configured evidence / predicate | Limit |
|---|---|---|
| Arbitration | `REQ_O'=R || Rd || (!H && !E)`; R=`X2,Y12 SLICE[1].YQ`, Rd=`X2,Y16 SLICE[1].YQ`, H=`X2,Y18 SLICE[1].YQ`, E=`X2,Y18 SLICE[0].Y`; `Rd'=R` | REQ is active low; R=1 inhibits a request, not a pending-work bit |
| Request qualification | `R'=B && v && (k || !STOP#)`; B=`X2,Y13 SLICE[0].XQ`, v=`X5,Y5 SLICE[0].YQ`, k=`X3,Y4 SLICE[0].X` | Local equation, not retry retirement |
| Grant acceptance | `S'=FRAME_raw && IRDY_raw && !GNT# && M && N && !S`; S=`X4,Y9 SLICE[1].XQ`, M=`X5,Y3 SLICE[0].YQ`, N=`X5,Y10 SLICE[1].YQ` | N captures configuration AD bit2 under `X8,Y10 SLICE[0].Y`; BME role inferred conditional accepted configuration write |
| Address phase | FRAME data shadow B, drive shadow T=`X3,Y13 SLICE[0].XQ`; `T_CE=!T || (S && M)`; complete aligned master address equation below | Conditional pre-output selection is not a legal owned transaction history |
| Command / byte enables | Conditional master C/BE D=0x6 read / 0x7 write; data-phase D=0; all 32 AD and four C/BE output FFs use `X0,Y13 PCILOGIC.PCI_CE` | Exact family hard-block CE transfer function remains Unknown; command value alone cannot prove a driven write |
| Data / wait | IRDY shadow I=`X2,Y15 SLICE[1].XQ`; acceptance candidate `A'=!TRDY_raw && v && !I`, A=`X4,Y13 SLICE[1].YQ` | No ownership or FRAME test in this local D equation; another master's pins must not be attributed to this card |
| Retry | STOP#=0, TRDY#=1 on first data phase, DEVSEL#=0 | First-phase history and suspended-work/reissue invariant unresolved |
| Disconnect | STOP#=0, DEVSEL#=0 after a phase or with accepted first data | Does not imply descriptor/payload discarded |
| Target abort | STOP#=0, DEVSEL#=1 after prior target claim | Requires claim history, not a one-sample status flag |
| Master abort | No response through protocol response window | Missing exact configured timeout/history correlation; single DEVSEL sample insufficient |
| Final accepted phase | Owned/enabled FRAME#=1, IRDY#=0, TRDY#=0, target selected | A protocol event, not all descriptors/FIFOs/host writes retired |
| Return to idle | FRAME#=1 and IRDY#=1 is a bus observation | No stable readable no-restart predicate identified |

PCI control FFs use `X24,Y29 BUFGCE[1].O`, with async reset from !RST# and
their decoded CE/INIT values. Output tri-state enable and output data are
separate; do not interpret a shadow bit as a driven pin. PCI_CE, dedicated
PCIIOB.PCI and physical DLL/CDC timing remain explicit boundaries. Protocol
classifiers in the synthetic tests are conditional labels, not invented FSM
state assignments. They follow PCI Local Bus Specification 3.0 section 3.3
termination rules; this is not a certificate that this configured core meets
every rule.

## Descriptor capture and counter transitions

The SGTA BAR0+0x040 strobes `X17,Y13 SLICE[0].Y` and
`X17,Y13 SLICE[1].X` select 30 FFs with a bijective AD[31:2] alternative.
IIMTC BAR0+0x044 strobes `X15,Y12 SLICE[0].X` and
`X16,Y12 SLICE[0].Y` select 32 FFs with AD[31:0]. These are **Verified**
capture alternatives, conditional on AD owning the internal TBUS: its driver
disable is `X3,Y14 SLICE[0].X`. Unique active ownership is not assumed.

Let `accept=X19,Y8 SLICE[0].YQ`, `read=X19,Y7 SLICE[1].YQ`,
`p=X23,Y21 SLICE[1].YQ`, `pn=X23,Y22 SLICE[1].YQ`.
`accept'=A && read`. Word-A's two 16-bit CE gates are
`X23,Y21 SLICE[0].X` / `X24,Y21 SLICE[1].X`, both `accept && p`.
Word-B's gates `X23,Y21 SLICE[0].Y` / `X23,Y22 SLICE[1].X` are
`accept && pn`. Each complete 32-bit bank captures one direct FF pipeline
from the AD alternative. `p'=pn` when accept and
`pn'=pending || (pn && !accept)`. Starting p=pn=0, a one-edge pending pulse
followed by two accept edges captures **B then A**, matching software
count/address order (bound3 conditional check). This verifies the local capture
schedule, not accepted/owned two-DWORD PCI fetches or arbitrary stalled schedules.

Word-A feeds address candidates and a whole-word zero detector
`Z=X22,Y21 SLICE[1].X`. Word-B feeds a whole 32-bit counter. Address/length
roles are **Inferred**, consistent with the existing software descriptor
format; the two complete AD mappings and Z equivalence are **Verified**.
The original and x64 descriptor builders supply count/address entries,
chain slots and terminal zero/zero. This software format alone does not
prove the hardware advances or retires every entry correctly.

| Transition | Exact configured result | Not established |
|---|---|---|
| Published | SGTA AD[31:2], IIMTC AD[31:0], IIMCL bit0 capture | Host publication ordering and consumption acknowledgement |
| Fetched | Two 32-bit AD-pipeline banks and qualified capture CEs | Own outstanding read, bus command and legal fetch sequence |
| Interpreted | Whole A/B zero tests; chain when L && Bzero && !Azero; load when L && !Bzero && !Azero | Identity of the fetched table entry and length units |
| Transfer issued | Complete pointer/master-address equations, conditional C/BE and all32 BRAM-to-staging-to-AD alternatives | Actual PCI_CE, output ownership and legal bus history |
| Completed | Segment-zero -> terminal -> done -> IRQ pipeline below | Final accepted PCI phase and upstream write completion |
| Advanced / retired | Full pointer loads SGTA/chain address or steps8; master address loads descriptor/pointer or steps4 | Outstanding-descriptor conservation and stable stop |

Two independent combinational SMT checks establish these D-word equations
modulo 2^32, **not** automatic progress on every clock:

* IIMTC-associated counter: low-to-high `X29,Y13..28 SLICE[1].XQ,YQ`;
  `D=(cl && reload) ? captured_IIMTC : old-1`. Low/high CE gates are
  `X25,Y26 SLICE[0].X` / `X25,Y26 SLICE[1].X` and remain distinct.
* Segment counter: low-to-high `X20,Y10..25 SLICE[0].XQ,YQ`;
  `D=load ? word_B : (pending ? 2 : old-1)`.
  load=`X22,Y19 SLICE[1].YQ`, pending=`X26,Y18 SLICE[1].YQ`.
  The four load replicas have matched D/CE/SR/clock/INIT contracts; the SMT
  equation assumes their state equality, justified after matched initialization
  and clocks, not arbitrary independent power-up states.

### Complete pointer and master-address equations

`pointer_map` identifies all30 unique SGTA-dependent pointer FFs, bits2..31;
bits0/1 are zero. Bit2 is `X25,Y21 SLICE[0].YQ`, bit31
`X25,Y27 SLICE[0].XQ`. The low carry starts at bit3 in
`X27,Y14 SLICE[1]`, continuing through Y28. Let
`initial=cl && reload`, `chain=L && Bzero && !Z`, `step=F && next_phase`.
Including actual CEs, the next aligned pointer is:

```text
pointer' = ((initial || chain) && !step)
           ? ((initial ? captured_SGTA : word_A) & ~3)
           : pointer + (step ? 8 : 0)                 modulo 2^32
pending' = initial || chain || (aux && F)
```

aux=`X26,Y18 SLICE[0].XQ`. Thus IIMCL clear is not a combinational
dominator of pending's chain/retained-state terms. This does **not** establish
a reachable post-stop PCI fetch. Exact shared-D sampling contracts establish
the three cl and three reload replicas; state equality requires common
initialization/reset/clock history. Arbitrary independent runtime replicas
produce counterexamples and are deliberately not treated as aliases.

The master-address alternative contains30 FFs at `X15,Y14..28` and two zero
bits. `master_address_map` recovers every AD producer. With matched load
replicas and RST# deasserted, including both actual CE groups:

```text
address' = (load || pending) ? ((load ? word_A : pointer) & ~3)
                             : address + (A ? 4 : 0)   modulo 2^32
```

A is the local sampled acceptance candidate, not proof of own-master
acceptance. The equation alone does not assign fetch/payload transaction
identity, establish terminal consumption or certify restart exclusion.

Under target-select `X2,Y21 SLICE[1].XQ=0`, snapshot-select
`X3,Y14 SLICE[1].XQ=0`, address-driver-disable `X4,Y9 SLICE[0].YQ=0`
and constant-shadow `X6,Y9 SLICE[1].YQ=0`, the complete C/BE pre-output word
is `0x6 | mode`, mode=`X18,Y10 SLICE[0].YQ`. With address disable=1 it
is 0 (all bytes enabled). Descriptor-read/payload-write role is **Inferred**
from mode/load/pointer dependencies and the software format; the conditional
four-bit equations are **Verified**. All32 AD pre-output D bits equal the
selected address or payload alternative when that producer alone is enabled
and the same target/snapshot selections hold. This includes explicit SAT
base checks, not a silently assumed one-hot bus. PCI_CE and pad drive-enable
semantics still prevent promoting these D-word facts to physical transactions.

Word-A zero is sufficient for the local done term `L && Z`, irrespective of
Word-B. Do not describe hardware termination as requiring both words zero.
Software zero/zero satisfies this condition; count0/address-nonzero selects
the chain path, and count-nonzero/address-nonzero selects segment loading.

## Completion-source pipeline and buffering

All equations below are exhaustively checked at actual named cuts:

```text
segment_gate = !load_replica && !pending
F' = segment_zero && segment_gate
L' = F && next_phase
C' = (cl && C) || (L && Z)
IRQ' = C || (IRQ && !(INTST_write && TBUS_data0))
reload' = !cl && (C || reload)
```

F=`X22,Y10 SLICE[0].YQ`, L=`X26,Y18 SLICE[0].YQ`,
C=`X22,Y11 SLICE[0].YQ`, IRQ=`X22,Y11 SLICE[0].XQ`,
segment_zero=`X20,Y10 SLICE[1].X`, segment_gate=`X20,Y25 SLICE[1].X`,
load_replica=`X22,Y20 SLICE[0].YQ`, next_phase=`X24,Y21 SLICE[1].YQ`,
cl=`X23,Y14 SLICE[1].YQ`, reload=`X25,Y20 SLICE[1].XQ` (INIT1),
INTST_write=`X16,Y8 SLICE[0].X`, data0=`X21,Y1 TBUS.OUT`.
The other listed pipeline FFs have CE1, PCI clock and async RST#/INIT0.
The equations do not include an independently established final accepted
host-memory transaction. IRQ-source state is not automatically physical INTx
assertion, host completion or generation provenance. INTEN masking is not
cancellation of this source pipeline.

## Completion and IRQ Proof Boundary

The strongest supported statement is **internal terminal processing produces a
sticky, untagged interrupt source**. Neither that source nor a deasserted INTA#
certifies DMA retirement. `virtexe_completion.py` reuses the existing decoder
and symbolic FF implementation. Only the documented INTA# output is added to
the private report; three additional logic nodes, not a new routing inventory.
Project Combine remains pinned to `234343d23e737e57f2727630e19008b509d7d522`.
These are configured-image contracts, not confirmation of the running revision.

For an older private target report, call `include_interrupt_output(report, router)`
with the matching existing decoder/router (the helper checks image hash and pin),
keep its result private, and select it with `WR6K_MMIO_REPORT`. With the existing
private binary and symbolic dependencies configured, reproduce the focused audit:
`python -B tests/dry/test_fpga_dma.py PrivateCompletion PrivateCompletionSmt -v`.
The normal `tests/dry/test-fpga-decoder.ps1` runner includes these cases; an older
report without the INTA root explicitly skips the new classes, not a PASS.

### Source and state inventory

All sequential entries below use PCI clock `X24,Y29 BUFGCE[1].O`, asynchronous
SR=`!X12,Y29 IOI[1].I` (RST#), no REV, and INIT0 except reload INIT1.
CE is 1 unless specified. Combinational entries have no independent clock,
CE or retained/reset state; their source FFs supply that history.

| Signal | Exact configured source / D / CE | Set, clear and lifetime |
|---|---|---|
| segment_zero | `X20,Y10 SLICE[1].X`; full32-bit segment-bank zero reduction, bank `X20,Y10..25 SLICE[0].XQ,YQ` | Combinational zero, not buffer empty; segment CE=`load_replica || pending || A` |
| load_replica | `X22,Y20 SLICE[0].YQ`, D=`X22,Y19 SLICE[1].Y` | D=`L && !Z && !(U && V)`; U=`X20,Y19 SLICE[1].Y`, V=`X19,Y21 SLICE[0].Y`; overwritten each edge. Matched replicas require common INIT/clock history |
| pending | `X26,Y18 SLICE[1].YQ`; D=`initial || chain || (aux && F)` | Rearmable descriptor processing, not a pending-write count; initial=`cl && reload`, chain=`L && Bzero && !Z`, aux=`X26,Y18 SLICE[0].XQ` |
| segment_gate | `X20,Y25 SLICE[1].X`; `!load_replica && !pending` | Excludes those two current descriptor qualifiers only |
| F | `X22,Y10 SLICE[0].YQ`; `segment_zero && segment_gate` | Overwritten every edge; can remain high while the condition holds, not necessarily a one-cycle pulse |
| next_phase | `X24,Y21 SLICE[1].YQ`; LUT INIT `0xBA30`, zero-based inputs accept,F,self,phase | D=`(self && !F) || (phase && accept)`; accept=`X19,Y8 SLICE[0].YQ`, phase=`X23,Y21 SLICE[1].YQ`; rearmable, not an idle acknowledgement |
| L | `X26,Y18 SLICE[0].YQ`; `F && next_phase` | Overwritten each edge; terminal pipeline qualifier, not final PCI acceptance |
| Z | `X22,Y21 SLICE[1].X`; full32-bit captured Word-A zero | Descriptor address/sentinel condition; Word-B need not be zero |
| C | `X22,Y11 SLICE[0].YQ`; `(cl && C) || (L && Z)` | Set by terminal term; retained while cl1; clears on an edge only if both terms are false |
| IRQ | `X22,Y11 SLICE[0].XQ`; `C || (IRQ && !(INTST_write && data0))` | Sticky and set-dominant; ACK clears only with C0; no acquisition generation field |
| reload | `X25,Y20 SLICE[1].XQ`; D via `X25,Y20 SLICE[1].Y`, `!cl && (C || reload)` | INIT1; cl1 clears it, cl0 retains/sets it from C; not a drain flag |
| cl | `X23,Y14 SLICE[1].YQ`; D=`X21,Y1 TBUS.OUT`, CE=`X17,Y13 SLICE[1].Y` | Qualified IIMCL BAR0+0x048 bit0 capture; holds otherwise; write0 changes this state, not every downstream state immediately |
| INTST_write | `X16,Y8 SLICE[0].X`; qualified BAR0+0x080 write decoder | Combinational ACK strobe, not a read-clear or completion timestamp |
| data0 | `X21,Y1 TBUS.OUT`; conditional captured AD0 alternative | Shared bus data, not an unconditional owned CPU write; existing TBUS ownership qualifications remain |

### Dependencies, direct versus indirect

```text
segment counter == 0 -- !load && !pending --> F --> L <-- next_phase
captured Word-A == 0 -----------------------------> C <-- cl/self
C -----------------------------------------------> IRQ <-- self / INTST ACK
IRQ & INTEN0 -- other five masked sources --------> INTA low/Hi-Z
               configuration inhibit ------------> INTA disabled
```

| Bookkeeping | Direct completion-source check | Established indirect relationship / limit |
|---|---|---|
| Receive write/read pointers, published pointer and PCI snapshot | None in the six reduced equations | Their comparisons qualify receive availability/staging admission. Periodic publication is not an empty/ACK test; snapshot coherence remains conditional |
| Staging write/read pointers | None | W/issue/rewind update modulo16 indices. Neither equal indices nor stored contents alone establish occupancy or unique retirement |
| Availability V | None | V participates in staging throttling/admission; not a tested final-word predicate in C or IRQ |
| Issue/rewind/Q | None | Prefetch/rollback and pointer arithmetic influence processing; no final retry-retirement check appears in C/IRQ |
| Descriptor pending/load | Direct negative qualification of F | A previously retained L/C/IRQ does not imply present pending0. Counter/descriptor advances depend on delayed A, accept and load history |

The actual count CEs and phase controls establish **indirect** coupling to PCI
progress. The reduced equations do not explicitly test receive-empty,
staging-empty, pending retry, owned final acceptance or host visibility.
This absence is not proof that an outstanding payload is physically reachable
at completion. The runtime-cut SAT witness below is not such a proof either.

### Interrupt lifecycle

The output is configured low/Hi-Z: PAD_O=0 at `X0,Y12 IOI[3]`; PAD_T=1
disables the drive. The outer LUT is `X11,Y8 SLICE[1].Y`, INIT `0xFF04`:
`T=G4 || (!G1 && G2 && !G3)`, correcting the historical provisional inverse.
Exhausting all8192 assignments establishes
`T=G4 || !OR(source[i] && INTEN[i])` for six sources. INTEN0 is
`X16,Y7 SLICE[0].YQ`, D=`X17,Y1 TBUS.OUT`, CE=`X12,Y13 SLICE[0].X`
(BAR0+0x084); its AD0 capture alternative and five sibling AD1..5 captures are
verified. G4=`X7,Y10 SLICE[1].XQ`, D=`X8,Y6 TBUS.OUT`, CE=`X8,Y10 SLICE[0].X`,
captures an AD10 alternative. The local inhibit role is Verified; accepting
and interpreting a whole configuration transaction remains separately scoped.
All these FFs share the PCI/reset contract and INIT0. Do not confuse the INTEN
write-storage FFs with the existing INTEN bit0 read mux, which aliases IRQ.

INTEN0=0 removes this IRQ's contribution without clearing C or IRQ; another
enabled source may still assert INTA. INTST ACK with C1 cannot clear IRQ.
After IIMCL0, C sees the **old** cl at the first edge; with no new L&&Z term,
C clears on the next edge. ACK must then sample C0 to clear IRQ. This is an
equation-level timing example, not a recommended live write sequence.
IIMCL1 while cl/C are already1 retains C even with ACK; there is no automatic
new-acquisition epoch reset. Repeated terminal terms can reassert the source.
RST# resets these FFs to INIT, but does not establish RAM erase, producer stop
or upstream drain. No individual DMA-generation identity occurs in this source
or output cone; software generation checks cannot recover physical provenance.

### Narrow formal results

`PrivateCompletionSmt` has13 queries, every base SAT. Each row uses actual
configured FF contracts with explicit PCI edges, no fairness, no transaction
ownership and no invented idle/accepted-word state. Unless specified, RST# is
deasserted and initial FFs/omitted named-cut histories are arbitrary runtime.
UNSAT is the **mismatch** query, not a whole-device shutdown proof.

| Query | Bound | Fixture / requested property | Result |
|---|---:|---|---|
| 1 | 4 | F=L=C=IRQ=0; segment_zero=Z=next_phase=1, load=pending=cl=ACK=0; F1,L2,C3,IRQ4, not IRQ3 | UNSAT mismatch |
| 2 | 1 | C=IRQ=cl=ACK=data0=1, L0; IRQ remains1 | UNSAT mismatch |
| 3 | 1 | C0,IRQ=ACK=data0=cl=1,L0; IRQ clears | UNSAT mismatch |
| 4 | 1 | As3, data0=0; IRQ retained | UNSAT mismatch |
| 5 | 1 | C=IRQ=cl=mask0=1,L0; qualified mask capture0; other masks/G4=0; C/IRQ retained, pad disabled | UNSAT mismatch |
| 6 | 3 | cl=C=IRQ=1,L0; qualified IIMCL0 then ACK/data1 with cl CE0; old-cl pipeline and reload set | UNSAT mismatch |
| 7 | 2 | cl=C=IRQ=reload=1,L0; qualified IIMCL1 and ACK1; C/IRQ retained, reload cleared | UNSAT mismatch |
| 8 | 5 | C=IRQ=cl=0,Z=ACK=data0=1; L pulses at0/3; IRQ2,not IRQ3,IRQ5 | UNSAT mismatch |
| 9 | 0 | Assert async RST# without clock; selected FFs equal INIT, including reload1 | UNSAT mismatch |
| 10 | 0 | Configured INIT in local projection, reset deasserted; same INIT vector | UNSAT mismatch |
| 11 | 0 | mask0=0; other sources/masks/G4 arbitrary; IRQ cofactor cannot affect T | UNSAT mismatch |
| 12 | 0 | All six masks0; sources/G4 arbitrary; T1 | UNSAT mismatch |
| 13 | 2 | C=IRQ=cl=0,L=Z=1; nonzero V/Q, issue/rewind/pending and differing receive/snapshot/staging pointer cuts; C1,IRQ2 | SAT witness |

There are12 mismatch UNSAT and1 constructed SAT, zero solver UNKNOWN. Query10
is initialized only for this local projection, not a complete initialized
acquisition. Query13 proves compatibility of independent cuts with the local
equations, not simultaneous full-machine reachability, valid occupancy or
physical outstanding DMA. The other source histories are deliberately
unconstrained; the equations have no corresponding generation/host contract.

### Shutdown obligations and next evidence

All six **end-to-end** obligations, and stability until explicit restart,
remain **Unknown**. Verified local facts do not upgrade these classifications.

| Obligation | Strongest current partial evidence | Smallest missing evidence artifact |
|---|---|---|
| Producer stopped | Conditional START/control and count-equality topology | Applicable HAD639E/MTT/MAM stop/last-word contract matched to the installed revision |
| Descriptor engine stopped | Verified pending/load/terminal equations | Same-epoch stop-to-admission invariant including retained reload/phase state and descriptor fetch history |
| No new PCI master transactions | Verified local initiator controls, no stop certificate | Applicable PCIIOB/PCILOGIC sampling/drive contract plus stop-to-no-start invariant |
| Outstanding transactions terminated | Conditional protocol acceptance and delayed A | Same-epoch ownership/final-phase/retry history correlated with IRQ, including final output-register sampling |
| Internal buffers drained | Verified modulo pointer and storage contracts, no drain flag | Valid/wrap/CDC invariant or existing correlated internal trace covering RX, staging, reissue and final word |
| Host DMA writes completed/ordered | No board/platform completion contract | Actual bridge/chipset identity and applicable posted-write/INTx plus Windows adapter visibility contract |
| Stable until explicit restart | Sticky C/IRQ is not stable inactivity | Proven absorbing no-admission/no-issue state across clocks and retained state, released only by qualified restart |

Priorities for further work:

1. **Existing firmware/configuration:** maintain these bounded regressions and
   correct specific inconsistencies. The focused source/INTA audit is complete;
   another general firmware-only round is not justified for release safety.
2. **Manufacturer architecture documentation:** obtain the applicable old-family
   PCIIOB/PCILOGIC timing contract, effective clock/setup requirements and producer
   last-word semantics. Generic/new-family primitives cannot replace it.
3. **Existing correlated traces or separately authorized observation:** identify
   the installed image and correlate a single epoch's final accepts, queued
   words and IRQ. External PCI pins alone cannot certify internal FIFO empty.
4. **OS/host-bridge evidence:** establish write visibility/ordering independently
   of endpoint source logic; no FPGA-only solver can prove this contract.
5. **Not currently actionable:** a complete shutdown/release certificate or
   universal restart exclusion without those artifacts. Stop increasing bounds,
   re-solving isolated LUTs or imposing empty/idle assumptions to obtain UNSAT.

`CompletionObserved != IdleProved`; retain UnknownActive quarantine and inactive
WDM acquisition. No production/hardware behavior is changed by this audit.

PCI receive storage is `X46,Y9 BRAM` and `X46,Y13 BRAM`, two 256x16 halves.
Port A uses remote `X24,Y0 BUFGCE[0].O` and write enable
`X43,Y10 SLICE[1].YQ`. Port B is PCI-clocked, read-only, enabled and not reset
to empty. No remote clock edge or write-enable=0 does not empty its stored
payload or output state. Complete pointer/ownership conservation and retry
retention are not assumed to implement an ideal FIFO.

### Resolved longline route and payload staging

The named zero-based LUT input2 is **G3**, not G2. Pinned
`wire_from_mux` covers HEX, LH and LV helpers. Applying the existing
same-switchbox/same-cell owner-`ProgBuf` guard to LH/LV resolves the alias:
`X27,Y17 SLICE[0].Y` input2 is **X1,Y17 BRAM.DOB[1], noninverted**.
The IO owner guard `(frame2212,bit354,inv0)` is raw0/disabled; BRAM owner
`(2110,355,inv1)` is raw0/active. BRAM mux bits `(2106,355),(2109,355),
(2112,355)=1,0,0` select `BRAM_QUAD_DOUT[6]`. In-memory guard mutations
verify each owner and both active; no IBUF-mode filter or preferred terminal
is invented. Original image and pinned architecture checkout are unchanged.

X1,Y17 is a separate1024x4 remote-clocked auxiliary memory, not either256x16
receive half. Its four input bits come from received-word state; a metadata
role is Inferred, not verified sample semantics. This mux also selects a delayed received-word bit; its output
feeds receive-BRAM data bit1. Resolving the route therefore does not make
every received bit an unmodified sample or a uniquely retired payload.

`payload_map` verifies32 distinct RAM16X1D cells, together a **16x32 staging
store**, between receive BRAMs and the payload TBUS alternative. AD0..15
originate at X46,Y13 DOB0..15; AD16..31 at X46,Y9 DOB0..15. Each bit's write
enable, write address, read address, driver and disable are retained, including
distributed pointer/enable copies. No private full network is published.

All staging write enables are equivalent to
`W=X37,Y7 SLICE[1].X && !X22,Y7 SLICE[1].YQ`. With matched address replicas:

```text
write_index' = write_index + W                    modulo16
read_index'  = read_index + issue - rewind          modulo16
issue       = X19,Y8 SLICE[1].X
rewind      = X20,Y7 SLICE[0].X
```

Read bits0..3 are X29,Y8 SLICE[1].YQ, X29,Y7 SLICE[1].YQ,
X29,Y7 SLICE[0].YQ, X29,Y10 SLICE[1].YQ. The actual backward step means
issued words cannot simply be counted as accepted/retired words. `rewind`
is a local arithmetic role, **not** a decoded empty or retry-ack status.
The RAMs have no SR erase; one PCI-edge UNSAT mismatch check proves all32
memories retain contents when W=0. Modulo pointers without wrap/valid/CDC
conservation do not prove full/empty distinction or prevent premature reuse.

### Lifecycle and proof boundary

#### Receive-to-payload evidence closure

The focused checks in `PrivateReceive` / `PrivateReceiveSmt` use the unchanged
PCI image hash and Project Combine pin enforced by `PrivateDma.setUpClass`.
`receive_data_checks` checks every immediate data mux exhaustively; it does not
export a complete extracted network or assign external sample identities.
The following contracts describe this configured image, not a verified live
hardware revision.

Both receive halves have the same eight address bits, enable/write controls
and port timing. Port A is remote-clocked (`X24,Y0 BUFGCE[0].O`), enabled,
with WE=`X43,Y10 SLICE[1].YQ`; port B is PCI-clocked
(`X24,Y29 BUFGCE[1].O`), enabled and read-only. Both port resets are constant
inactive. Each port is 16 bits wide with an eight-bit word address shifted by
four into the shared 4096-bit storage. BRAM INIT remains unavailable; neither
RAM contents nor output startup are zero-filled by the model.

**All DIA sources.** In this compact table `Xx,Yy/Ss` denotes the SLICE at
that coordinate. Each pair lists delayed bits in **YQ, XQ** order. Four
successive bits use auxiliary **DOB[0..3]** in order. Selector true chooses
the delayed FF; false chooses the auxiliary BRAM output. All 32 truth tables
are checked, including inversions. These auxiliary memories are not the two
receive halves and their contents are not automatically waveform samples.

| Receive half / DIA bits | Delayed FF pairs (low to high) | Auxiliary source |
|---|---|---|
| X46,Y13 / 0..3 | X41,Y17/S0; X43,Y17/S1 | X46,Y17 BRAM |
| X46,Y13 / 4..7 | X31,Y19/S1; X34,Y19/S1 | X1,Y21 BRAM |
| X46,Y13 / 8..11 | X44,Y12/S0; X43,Y13/S1 | X46,Y5 BRAM |
| X46,Y13 / 12..15 | X45,Y18/S1; X43,Y21/S1 | X46,Y21 BRAM |
| X46,Y9 / 0..3 | X28,Y17/S1; X27,Y18/S0 | X1,Y17 BRAM |
| X46,Y9 / 4..7 | X34,Y14/S0; X35,Y13/S0 | X1,Y13 BRAM |
| X46,Y9 / 8..11 | X41,Y8/S1; X41,Y8/S0 | X46,Y1 BRAM |
| X46,Y9 / 12..15 | X41,Y16/S1; X40,Y14/S1 | X46,Y25 BRAM |

The selectors are X38,Y13/S0.X for X46,Y13 and X34,Y13/S1.X for X46,Y9.
Their protocol meaning is not substituted for the configured mux function.

**Addresses and admission.** Low-to-high bit order is retained in
`RX_WRITE`, `RX_READ`, `RX_PUBLISHED`, `RX_SNAPSHOT`, `RX_FEEDBACK` and
`RX_REMOTE_READ` in `virtexe_dma_handshake.py`. These are named recovered
banks, not replacement FIFO state. Let P be RX_WRITE, C be RX_READ and
Cr be RX_REMOTE_READ. Source-backed whole-word identities establish:

```text
Port-A address = P
P_next = P + old_receive_WE                         modulo256, remote edge
Port-B address = C + W                              modulo256, combinational
C_next = C + W                                     modulo256, PCI edge
receive_WE_next = ((P - Cr) modulo256 < 224) && !G   remote edge
G = X43,Y10 SLICE[1].X
```

The receive WE is itself a CE=1, INIT=0, asynchronously reset FF. Its next
qualifier is **not** the WE sampled by RAM or P on that same edge. G remains
the actual upstream inhibit predicate; it is not renamed producer-done,
word-valid or FIFO-full. The 224 threshold is local delayed backpressure,
not a verified maximum global occupancy. C's D-function increments by one,
but its CE equals W; the effective transition above includes that CE.

**Pointer crossings.** P is captured into the remote RX_PUBLISHED bank under
CE=X44,Y6/S0.X, then into PCI RX_SNAPSHOT under CE=X43,Y4/S1.Y. The latter
is X44,Y4/S0.YQ && !X43,Y4/S0.YQ. Its request path includes remote
X44,Y5/S1.YQ followed by PCI X44,Y5/S0.YQ, X44,Y4/S0.YQ,
X43,Y4/S0.YQ. The remote capture CE is X44,Y6/S0.YQ && !X44,Y5/S1.YQ.
Thus a held binary snapshot and delayed publication phase are visible; this
is not a Gray-coded pointer synchronizer. Conversely, PCI RX_FEEDBACK captures C+W
under X40,Y7/S0.X and remote RX_REMOTE_READ captures that bank under
X40,Y8/S1.Y. All six eight-bit banks are INIT=0 with asynchronous !RST#.
The eight existing inequalities compare RX_SNAPSHOT against C, with **no
ninth wrap/phase bit in that predicate**.

`receive_publication_checks` closes both publication-control cones: forward
X44,Y6/S1.XQ, S1.YQ, S0.YQ form a CE=1 modulo8 remote counter. Reverse
X40,Y7/S0.XQ, S0.YQ form a CE=1 modulo4 PCI counter. Their upper-bit edges,
the named delay chains and all clocks/reset/INIT contracts are checked.
Forward capture repeats every eight remote edges; reverse capture every
four PCI edges, absent reset. **Neither divider waits for an acknowledgement
from the other domain.** The reverse CE is the delayed rising edge of its
counter's upper bit. Therefore the precise gap is not an unidentified ACK
register: it is the effective clock ratio/phase, publication pulse sampling,
binary-data hold window and synchronizer/BRAM setup behavior. Arbitrary
independent clock schedules cannot establish coherent snapshots or no-lap.

**Latency and reset.** Port B updates its output latch on a PCI edge. A
staging write on that edge captures the **previous** DOB, not the just-read
word at C+W. A synthetic two-edge check verifies this model timing without
choosing an initialized BRAM output; a private one-edge check verifies all
32 actual staging data inputs against pre-edge DOB. Interpreting the first
staged word still needs priming/validity and collision-free history. The model
keeps cross-port setup-window overlap and read collisions symbolic, including
possible adjacent-edge overlap; omitting a remote edge is not a CDC proof.
Global !RST# clears the checked pointer banks but cannot erase these BRAMs
or the 32 staging RAM16X1Ds. A one-edge private mismatch check verifies
retention during reset; it does not claim output latches are reset or idle.

**Occupancy limit.** P=C can describe zero or 256 modulo advances without
a no-lap invariant. Likewise V=0 can represent zero or 16 modular advances.
The existing V+read_index-write_index conservation is only modulo16.
Simultaneous W/issue/rewind obey their established equations, but neither an
issue nor a rewind supplies a uniquely identified external acceptance.
Delayed throttle at V>=10 is not a direct full flag. Reset-retained contents
are neither valid occupancy nor proof of drain. No reachable overflow,
underflow, duplicate payload or hardware fault is established by these
arithmetic ambiguities.

Classification here is precise: **Verified** means a configured local
function/contract; **Conditional** requires an explicitly stated timing,
selection or state premise; **Inferred** is a plausible semantic role;
**Unknown** has no established end-to-end relation.

| Boundary | Established evidence | Missing evidence | Classification |
|---|---|---|---|
| LVDS word qualification | Existing frame/capture structure; each data mux has exact runtime alternatives | Sample identity and complete external word-valid/end-of-data contract | Conditional |
| Receive-BRAM admission | Shared old WE, remote-clock write ports, P increment and registered threshold224 | Link-word validity and cross-domain no-overwrite guarantee | Verified |
| Receive-BRAM occupancy | Periodic modulo8/modulo4 publication; two binary snapshot directions; eight-bit inequality | Clock ratio/hold-window coherence, no-lap invariant and globally meaningful full/empty state | Unknown |
| Staging-memory write | All32 pre-edge DOB inputs, W gate, PCI clock and actual write pointers | Priming and valid/collision-free received-word history | Conditional |
| Staging-memory occupancy | Modulo16 pointer/V equations and delayed throttle | Unwrapped conservation, bounded occupancy and unique word identity | Unknown |
| Internal payload selection | All32 RAM/TBUS/AD D alternatives under explicit unique-driver selections | Effective output loading/ownership and valid selected token | Conditional |
| Internal consumption | Issue/rewind/read-pointer/Q mechanisms | Bijection between issued/reissued tokens and accepted words | Inferred |
| External transaction acceptance | Conditional PCI observer, distinct from internal bookkeeping | Correlated owned phases, PCILOGIC timing and host-bridge visibility | Unknown |

**Formal audit delta.** The new bound0 word identities are configured
Boolean identities under arbitrary register cuts, with SAT bases before
mismatch UNSAT. Registered admission and reset/retention are bound1 local
properties; the synthetic output pipeline is bound2 under no collision.
They are not initialized LVDS-to-payload executions. Existing runtime SAT
and initialized descriptor-prefix results retain the scopes in the formal
evidence table below; no new PCI stimulus, host-memory transaction, unbounded
proof or physical failure interpretation is introduced. All six release
obligations remain Unknown.

**Remaining work and expected offline value.** Both periodic publication
control cones are now closed; repeating their individual LUTs is not useful.
A targeted next analysis is worthwhile only with the effective remote/PCI
clock frequency, phase/stop relationship and applicable synchronizer timing.
It can then test publication hold/admission bounds while preserving actual
FF clocks and reset history. Proving first-word priming and no-lap across the
link additionally needs the applicable remote
word-valid/backpressure contract and bounded producer response, not just
another arithmetic LUT. BRAM startup/collision and synchronizer timing need
applicable architecture/timing evidence. External retirement still needs
PCILOGIC, owned PCI phase history and the host-bridge/Windows contract;
another receive-pointer-only round cannot close those gaps.

| Transition | Label | Exact scope / missing invariant |
|---|---|---|
| LVDS word received -> receive BRAM | Conditional | Existing receive capture/admission and remote-clock BRAM paths; sample identity, producer end-of-data and CDC coherence remain separate |
| Receive BRAM -> staging RAM write | Conditional | All32 data alternatives, PCI-domain W and RAM16X1D write ports; cross-domain collision/valid-word history Unknown |
| Host publication -> descriptor request | Inferred | Software table, SGTA/IIMCL capture and initial/pending equation; publication/coherency and owned request not closed |
| Descriptor request -> fetch acceptance | Unknown | Conditional address/read command and AD capture; PCI_CE/ownership/history missing |
| Fetch acceptance -> descriptor consumption | Conditional | B then A local capture for the stated pending/accept schedule; legal fetch and stalled/retry schedules Unknown |
| Descriptor consumption -> payload selection | Conditional | Chain/load/zero and complete master-address equations; transaction identity still Inferred |
| Payload selection -> PCI write | Conditional | All32 BRAM/staging/TBUS/AD D alternatives and write-command cofactor; driven accepted write Unknown |
| PCI write -> final data-phase acceptance | Unknown | Own output enables, hardblock sampling and target history must agree |
| Attempt -> rewind / final consumption | Conditional | Actual Q/rollback/read-pointer equations below; universal retry-suffix preservation and unique accepted-token consumption not established |
| Final acceptance -> internal completion | Unknown | No proved bijection from accepted words to counter/terminal events |
| Internal completion -> IRQ source | Verified | Actual C/sticky-IRQ equations, not generation-tagged physical interrupt provenance |
| IRQ source -> physical IRQ | Inferred | Interrupt masks/output path; effective electrical timing and final-transaction association not proved |
| IRQ -> host visibility | Unknown | Sideband IRQ has no general posted-write-drain guarantee |

Retry/STOP/TRDY/DEVSEL/grant logic remains the actual configured initiator
equations, not a fabricated protocol FSM. The staging backward-step equation
is a retention/reissue mechanism candidate, but its one-to-one correspondence
with target retry, disconnect or abort is not established. In particular,
no decoded error/readback predicate currently certifies discard, drain or
reissue completion. Timeout/lost IRQ provide no FPGA cancellation event.

## Handshake, rollback and local word conservation

### Exact configured cuts

The immediate issue cone has six FF cuts; the immediate rewind cone has three.
The complete transitive data/control inventory of either root covers1,249 nodes,
603 registers and49 pad inputs in the current target-root report. Its remaining
architecture leaves are three effective BUFGCE clocks and DLL.LOCKED. This is
graph coverage, not a proof of memory contents, CDC coherence or output timing.
The full inventory stays private. The companion checker validates19 independent
local truth tables, in addition to the prior15 DMA and19 PCI-control equations.

Here `S0/S1` abbreviate `SLICE[0]/SLICE[1]`; lowercase names are coordinate cuts,
not presumed PCI protocol states:

```text
issue = mode && !target && !B && J && v && h
mode=X18,Y10 S0.YQ; target=X2,Y21 S1.XQ
B=X2,Y13 S0.XQ; J=X4,Y13 S0.XQ; v=X5,Y5 S0.YQ; h=X5,Y8 S0.YQ

J' = !target && ((a&&k)||(c&&v)||S||(!TRDY_raw&&!I&&v)
                ||(!IRDY_raw&&k&&!e))
a=X3,Y16 S0.XQ; k=X5,Y15 S0.YQ; c=X3,Y7 S1.XQ
S=X4,Y9 S1.XQ; e=X2,Y14 S0.XQ; I=X2,Y15 S1.XQ
v' = (S&&!GNT_raw)||(v_hold&&v); v_hold=X5,Y5 S0.Y
h'=1

rewind = rollback && (q0||q1)
q0=X18,Y4 S1.YQ; q1=X18,Y4 S0.XQ; Q=q0+2*q1
rollback=X19,Y7 S0.YQ
rollback' = rewind || (ended&&!active&&!T&&(read_mode||write_mode))
ended=X15,Y7 S0.YQ; active=X5,Y5 S0.XQ
ended'=active; T=X8,Y7 S0.YQ
T_D=address_disable&&(K||(!STOP_IQ&&DEVSEL_IQ))
T_CE=active||!address_disable; address_disable=X4,Y9 S0.YQ
K=X3,Y4 S0.X; read_mode=X19,Y7 S1.YQ; write_mode=X15,Y7 S1.YQ

d = mode && !issue && (A||rewind)
t = d || (issue&&!A&&!rewind)
Q' = Q + (t ? (d ? -1 : 1) : 0)                 modulo4
A=X4,Y13 S1.YQ; A'=!TRDY_raw&&v&&!I
```

Q/rollback/ended/active/J/v/h use PCI-clock FFs, CE=1 and asynchronous !RST#;
Q/rollback/h initialize to0. T retains its decoded conditional CE, not CE=1.
`mode` retains CE=`pending||load`; it is not a free synchronous reset for Q.
The raw/IQ distinction is intentional. Dedicated PCIIOB ready taps are neither
silently replaced by raw I nor assumed equal to IQ. Existing B/IRDY/control
equations remain part of the original configured model, not a substitute FSM.

Consequences **Verified locally**, with global interpretation still Unknown:

- `issue` need not coincide with A or current TRDY readiness. It is a prefetch
  advancement, not final consumption of a word by the target.
- Issue and rewind can coexist in an arbitrary-runtime cut; read pointer then
  holds. Whether a legal initialized transaction reaches that overlap is Unknown.
- Mode1, no issue and A=0 drains a nonzero Q by one per rewind and gates rewind
  off at Q=0. Starting locally Q=rollback=0 with no issue, four-edge rewind is
  UNSAT. This is a bounded local exclusion, not a global ownership invariant.
- A and rewind coalesce into one decrement when issue=0. Issue+A+rewind holds
  Q rather than performing naive `+1-1-1`. A with Q=0 can wrap Q in a runtime
  projection; excluding that state requires owned-transaction history.
- Mode0 can retain nonzero Q and rollback across repeated rewind edges. The
  four-edge SAT projection is not a demonstrated reachable underflow defect.

### Availability and retention

An actual four-bit available-word bank has low-to-high bits X22,Y6 S0.XQ,
X22,Y7 S0.XQ, X22,Y8 S0.XQ, X22,Y8 S0.YQ. The configured equations are:

```text
V' = V + W + rewind - issue                       modulo16
available_nonzero = (V!=0)
throttle' = V3 && (V1||V2)                         i.e. V>=10
W = received_available && !throttle
```

This bank initializes to0 and uses CE=1 on the PCI clock. Receive availability
is an OR of eight paired FF inequalities, through four comparators. The producer
side of those comparisons is a retained PCI-clock snapshot; the consumer side
advances with staging admission. The paired comparator truth tables are checked,
but snapshot coherence with the remote-clock receive writer is not assumed.
Equality without an independently valid wrap/history invariant is not a full
versus empty certificate. The delayed threshold is not a proof that V stays in
the nonwrapped range under every issue/rewind/producer schedule.

Together with the established write/read pointer equations, V preserves
`V+read_index-write_index` modulo16. That arithmetic invariant does not prove
word identity, prevent overwriting an unaccepted token, or establish CDC safety.
For issue then rewind without an intervening producer write, the same RAM slot
is selected again; W=0 preserves its contents. Retry preserves the destination
address **conditionally** when load=pending=A=0, because the master-address
equation then holds. Mapping those conditions to every physical retry and to
the AD output pipeline remains Unknown. RAM reset is not a data erase.

`word_observer` is a separate verification token ledger. It can flag unmatched
issue/accept/rewind, duplicate identities or overwrite under a supplied event
interpretation. It never supplies valid/pending constraints to System and is
not evidence that a hypothetical token violation occurs on hardware.

### Counter enables and completion

The recovered full32-bit D-word equations now have validated CE meanings:

```text
write_accept' = A && write_mode
IIMTC_CE = (cl&&reload)||(aux&&write_accept)
segment_CE = load||pending||A
```

Low/high IIMTC reload copies have equal next functions, INIT and sampling
contracts; equality requires common initialization/clock history, not arbitrary
runtime aliasing. With load terms absent, segment decrements on delayed A,
including descriptor-read progress. IIMTC decrements on a further delayed
write-mode A qualified by aux. Neither counts issue, rewind or retry attempts.
An accepted-32-bit-word interpretation is Inferred, conditional on A belonging
to this card's owned data phase; complete descriptor semantics remain Unknown.

The segment1->0 scenario is SAT while **current** TRDY_raw and IRDY shadow
are high, because its CE uses an already registered A. This is pipeline timing,
not proof that a word was lost. The existing completion equations do not directly
test Q, V or PCI_CE. The projected IRQ-rise scenario with nonzero Q is SAT,
but terminal/descriptor-zero inputs and ownership history are external cuts.
Thus it neither proves premature IRQ on valid hardware execution nor proves
IRQ-after-every-acceptance. Host visibility remains an independent obligation.

### PCI outcomes and evidence matrix

The protocol observer follows [PCI Local Bus Specification3.0](https://e2e.ti.com/cfs-file.ashx/__key/communityserver-discussions-components-files/639/1016.PCI.Local.Bus.Specification.Revision.3.0.pdf),
sections3.2.1/3.3.3. Electrical0 means asserted. Address phase needs prior grant
and bus-idle/start history; GNT withdrawal alone does not revoke an active owned
transaction. Accepted data requires owned data phase, enabled outputs and
IRDY=TRDY=0. FRAME=1 then marks the final accepted phase. STOP=0,DEVSEL=0,
TRDY=1 before any transfer is retry; after a transfer it is disconnect without
data. STOP=TRDY=DEVSEL=0 requests disconnect with data, transferred only when
IRDY=0. Target abort needs prior claim then STOP=0,DEVSEL=TRDY=1. Master abort
needs the expired response window with no claim, not one DEVSEL sample.

| Owned protocol outcome | issue/rewind and read pointer | Pending word/address | Counter/completion |
|---|---|---|---|
| Wait | Current readiness does not determine issue; `read'=read+issue-rewind` | W0 retains RAM; AD/CBE hold if PCI_CE0 | A is delayed; previous progress can remain; completion Unknown |
| Accepted phase | Same arithmetic; A/issue may differ by pipeline cycles | Own payload identity requires drive/sampling invariant | Internal A advances address/counts conditionally; physical equivalence Unknown |
| Retry | Rollback/Q mechanism exists; exact first-phase correspondence Unknown | Same slot on matched issue/rewind with W0; address holds if no load/pending/A | No decrement merely because STOP asserted; eventual reissue/retirement Unknown |
| Disconnect | Already accepted prefix and unaccepted tail need distinct history | Tail rewind candidate, not verified universal suffix conservation | A may count accepted prefix; terminal transaction association Unknown |
| Abort | Cannot assign issue/rewind from one bus sample | Retention/discard/restart policy Unknown | No proved abort-to-drained/idle transition |
| Final phase | STOP does not itself cancel a simultaneous transfer | Last word identity and output CE must agree | IRQ-after-last-owned-acceptance and host visibility Unknown |

### Twelve focused bounded scenarios

`virtexe_dma.py --handshake --output <private-path>` reproduces the checks and
private traces. Original FF D/CE/SR/INIT contracts are retained; other named FFs
are symbolic external cuts. PCI edges only, RST# deasserted at all boundaries,
no fairness/remote edges/owned-transaction history are assumed. PCI_CE is left
symbolic; a separate consistent-base mismatch proof shows all36 AD/CBE outputs
hold on an edge with CE=0. Each query checks base consistency before its goal.

| # | Scenario / exact qualification | Bound | Result |
|---|---|---|---|
| 1 | issue with TRDY1/A0 | 2 | SAT |
| 2 | Internal A1 with issue0 | 2 | SAT |
| 3 | Q=rollback=0, mode1/A0; issue then rewind | 2 | SAT |
| 4 | Same local initial state, no issue throughout; any rewind | 4 | UNSAT |
| 5 | Same local initial state; two issues then two rewinds drain Q | 4 | SAT |
| 6 | A then STOP0/TRDY1 with rewind; partial-disconnect-shaped cut | 2 | SAT |
| 7 | B1/segment-zero/STOP0/TRDY0 followed by A1; final-phase-shaped cut | 2 | SAT |
| 8 | Actual segment1->0, no load/pending, delayed A1 and current TRDY1/I1 | 2 | SAT |
| 9 | C=IRQ=0 then IRQ rise with Q nonzero | 2 | SAT |
| 10 | Qualified IIMCL clear with rewind still pending | 2 | SAT |
| 11 | Read15->0 by issue with TRDY1; matched bit0 replica | 2 | SAT |
| 12 | Qualified IIMCL restart with all32 staging memories retained, W0 | 2 | SAT |

All12 bases are SAT; zero solver-unknown results. These are projections, not
boot/valid-launch reachable traces. In particular2 does not assert that a new
owned PCI transfer occurs without any prior issue, and6/7 do not establish the
required physical transaction history. SAT is not hardware failure; bounded
UNSAT is not unbounded retirement or safe shutdown. Repeated retry and final
word stalls still need the missing effective-output/ownership invariant.

Validation on2026-10-13:142 distinct DMA tests (25 synthetic,58 private configured-logic,
59 private SMT), all PASS in the full run; the final28-case rerun passed
(17 completion/matrix and11 synthetic DMA, all overlap) after the final
qualifier/matrix checks were added. Reproducing the INTA extension added exactly
three logic nodes with the original image hash and pinned architecture.
The FPGA/MCU validation has374 distinct Python tests
(159 synthetic,215 private-firmware),
zero failures/skips, decoder self-test/frame calibration,50 native architecture
and19 native PCI local-equation checks PASS. The previously recorded332 driver
dry cases were not rerun for this completion-focused task.
The new initialized bound40/52 prefixes add12 diagnostics (9 SAT,3 UNSAT,
zero solver-unknown results), separately from the earlier ten bound1 runtime
and twelve bound2/4 projected diagnostics. No hardware execution or WDK build
was performed; the production driver and quarantine policy are unchanged.

## Stop/failure matrix and bounded diagnostics

| Condition | Established effect | Safety limit |
|---|---|---|
| Normal IRQ | Actual done/sticky-IRQ equations and software generation checks | No final-transaction/host completion implication |
| MTTCTL=0 | Prior acquisition analysis: E then START clear | Other producer and retained/queued data not drained |
| IIMCL=0 | Three local control replicas clear on qualified strobe | Driven REQ can remain active in a runtime cut |
| INTEN=0 | Masks interrupt delivery | Does not retire descriptor/write work |
| Timeout / delayed IRQ | Host observation / software completion handling | No source-backed FPGA cancellation event |
| Retry / disconnect | STOP/TRDY affect actual control paths | Reissue and retained payload lifecycle Unknown |
| Missing target | Needs response-window history | No recovered master-abort-to-safe-cleanup contract |
| STOP / removal | Closes software admission and performs rundown | Does not establish electrical absence or physical DMA retirement |
| Reset / restart | Decoded reset priority and IIMCL set/load paths | Reset does not certify prior posted writes complete; restart exclusion unproved |

The ten `--queries` diagnostics use the existing System model, bound1, arbitrary
runtime FF/memory state, RST# deasserted, one PCI edge and no other-domain edge.
No clock relationship or fairness is invented. Every base is checked SAT.
218 unresolved symbolic boundaries remain. These are local runtime candidates,
not initialized/legal-PCI reachable traces or universal shutdown proofs.

| # | Actual query / qualification | Solver | End-to-end disposition |
|---|---|---|---|
| 1 | Qualified IIMCL clear and all replicas clear; driven REQ remains active | SAT | No clear-implies-idle proof |
| 2 | Actual IRQ latch rises while FRAME/REQ activity remains | SAT | Final transaction identity/physical reachability Unknown |
| 3 | Pointer bit2 changes on clear | SAT | Stop-associated fetch/retirement Unknown |
| 4 | Receive write disabled, remote clock stopped, PCI activity | SAT | Buffered payload write attribution Unknown |
| 5 | STOP0/TRDY1, R0=1, Rd1=1, driven REQ active | UNSAT | R=1 inhibits REQ; NOT retry-then-write retirement proof |
| 6 | Host timeout observation with pending PCI activity | SAT | Timeout has no FPGA cancellation input |
| 7 | Qualified IIMCL restart with REQ active before/after | SAT | Outstanding work cannot be dismissed |
| 8 | IIMCL clear with receive write admission still enabled | SAT | Not a common admission barrier |
| 9 | Untagged IRQ latch | SAT | Physical generation provenance Unknown |
| 10 | Enabled FRAME/IRDY, FRAME1/IRDY0/TRDY0/DEVSEL0 | SAT | Prior ownership/address assumed; host bridge completion unmodeled |

All ten dispositions are **Inconclusive**. Query3 concerns stop-associated
advancement, despite the now-proved complete local pointer equation.
Queries 3-5, 9 and 10 deliberately
report the missing lifecycle predicate, rather than inventing descriptor
retirement, delayed-write or host-completion variables. No bounded UNSAT is
promoted to an unbounded safe-stop guarantee.

### Additional symbolic checks and dispositions

All new checks use actual decoded D/CE/control expressions. Combinational
checks have bound0, arbitrary state, deasserted RST#, no clock/fairness and
PCI_CE left symbolic. Every property base is checked SAT before its mismatch.
Matched replicas are conditional common-initialization/clock assumptions,
checked against full sampling contracts; they are not arbitrary-state aliases.

| Check | Initial/environment assumptions and schedule | Bound / result | Reachability limit |
|---|---|---|---|
| Full pointer next word | Matched cl/reload copies; actual CEs included | 0 / mismatch UNSAT | Local function, not fetch admission or stop stability |
| Full master address | Matched four load copies; actual CEs included | 0 / mismatch UNSAT | A not automatically an owned accepted phase |
| Chain / segment load | Arbitrary A/B words and terminal state | 0 / mismatch UNSAT | Zero tests, not table-entry identity |
| Master C/BE command and bytes | Explicit target/snapshot/address/constant cuts above | 0 / two mismatches UNSAT | Pre-output D; PCI_CE and pad enables unresolved |
| All32 AD address/payload selection | Explicit selections and unique active producer, both choices separately SAT | 0 / two mismatches UNSAT | No physical command/drive or legal transaction history |
| Staging pointer and enables | Matched address copies; arbitrary issue/rewind state | 0 / joint mismatch UNSAT | Modulo16 arithmetic, not full/empty or conservation |
| Staging retention | Arbitrary memory; all write enables0; one PCI edge/no remote edge | 1 / mismatch UNSAT | Retained data are not drained or retired |
| B then A capture | p=pn=0; pending pulse then two accept edges; three PCI edges/no remote edge | 3 / mismatch UNSAT | Conditional capture order; bus ownership and fairness not proved |
| IRQ without new A | Actual C/IRQ/A contracts; C=IRQ=0, L=Z=1; A=0; two PCI edges | 2 / SAT | Cone projection with named external cuts unknown; retained-state candidate, not initialized legal hardware trace |

The full-network bound2 IRQ extension returned **UNKNOWN (30s timeout)**
during exploration. The small actual-register cone yields SAT without adding
fictitious pending/final-write variables, but cannot settle IRQ-to-final-write
ordering. The distinction is intentional, not a timeout reclassified as proof.

Remaining formal obligations: no fetch after stop, no retained retry after
idle, terminal event under fair target response, unique payload retirement,
no early reuse, IRQ-after-final-acceptance, no new master start, restart
exclusion and stability across reset/timeout all remain **Unknown**. Fairness
alone cannot supply missing owned transaction identity or the family hardblock
contract; bounded equations cannot prove eventual progress. Internal
retirement and host visibility are separate obligations. No candidate idle
predicate is promoted to `IdleProved`.

## Defensive evidence review

The software-safety review changes tests/documentation only. No new initiator,
payload execution, bus stimulus or physical experiment is added. Existing
offline regressions validate their existing equations; passing them does not
authorize DMA-resource release.

| Evidence/test boundary | Reviewed interpretation / safeguard |
|---|---|
| Local truth tables and D equations | Verified Boolean/configured-contract identities at named cuts, not proof that a protocol execution reaches those cuts |
| Runtime projections (`powerup=False`) | Arbitrary initial FFs and omitted state remain explicit; SAT is an abstract candidate. Independently satisfiable cuts cannot be concatenated into one execution |
| Initialized bound40/52 cases | Configured FF INIT/reset and transitive dependencies are retained. PCI-only edges, fixed host schedule, mapped responder and bounded response are environmental restrictions; no LVDS producer completion or host visibility follows |
| PCI_CE, dedicated taps and effective clocks | Named symbolic architecture boundaries, not recovered physical contracts. SAT may choose arbitrary per-edge unknown values; model-completed trace values are observations, not newly verified semantics |
| Missing memory INIT / BRAM output startup / collisions | Remain symbolic, not zero-filled. New synthetic checks require both possible missing-INIT values and reject invented initialization of unknown FF contracts |
| UNSAT / base consistency | Exclusion applies only within the stated bound and assumptions. The synthetic `assertImpossible` helper now requires a SAT base before checking exclusion; a negative fixture verifies rejection of vacuous UNSAT |
| Solver UNKNOWN | Remains inconclusive. Existing deterministic timeout regression is an injected solver-result test, not a real hardware or solver-timeout witness |
| No-IRQ/no-payload prefix | Absence within a bounded PCI-only experiment is not IRQ-after-final-write safety, unbounded liveness or a producer/buffer drain certificate |

New synthetic tests also distinguish runtime SAT from configured initialization
and demonstrate a bounded exclusion followed by reachability at the next edge.
No previously reported SAT/UNSAT result was found to have been promoted to a
physical idle guarantee. The helper's missing explicit base check was a
regression gap, not evidence that a previous private exclusion was false; their
reported bases are independently checked. The source-referenced
[implementation-owner decisions](dma-lifetime-and-timeout.md#defensive-review-and-implementation-owner-decisions)
remain separate from FPGA reconstruction and preserve the legacy-success versus
future-WDM-release distinction.

## Initialized PCI launch prefix and the remaining sequence boundary

`virtexe_dma_reachability.py` adds a **single initialized constraint system**,
not another arbitrary-state cut. Its dependency closure follows configured
FF data, controls, clocks, TBUS producers, RAM and both BRAM ports. The private
PCI-root closure contains 1,513 FFs and 42 memories. No internal runtime FF is
assigned and PCI_CE is never replaced with a guessed function or constant.
Configured FF INIT and one PCI-reset boundary are used; effective PCI edges
are explicit. Missing BRAM INIT/output startup, collision behavior, DLL and
clock-generation contracts remain named symbolic boundaries, not zero-filled
evidence. The firmware/report pin and frame calibration are checked.

The external host performs four single-final-data-phase writes: PCI command
offset4=6 (memory/bus-master enable), SGTA=0x100000, IIMTC=1, IIMCL=1.
BAR0 uses its configured zero initial storage; this is a synthetic bus address,
not the installed BAR address. The host withdraws IRDY after the FPGA's observed
TRDY acceptance and each write must finish within the stated candidate schedule.
This bounded-response restriction admits a candidate prefix, not all target
latencies. After that host sequence, no other initiator drives the bus and grant
is offered while idle. Driven-pad feedback remains tied to actual O/T logic.

In the stalled-target bound40 experiment, base/configuration/launch/master-start
and descriptor-address candidates are SAT. In one **same-execution** address
witness, host acceptances are edges5/14/22/30, and the FPGA drives the aligned
SGTA address with command6 at edge35, after idle/grant. There is no post-launch
internal A acceptance. IRQ assertion through edge40 is UNSAT in this environment
with a satisfiable launch base. This excludes an early IRQ for this bounded,
never-responding descriptor fetch. It does not establish successful transfer
ordering, liveness, legal retry handling or unbounded absence of an IRQ.

A second, independent **whole-prefix** experiment uses bound52 and a mapped
external memory target, one read-turnaround wait, explicit address/data drive
direction, full-DWORD byte enables, idle/grant at a new transaction, and no
overlapping address phases. It serves count1/address0x200000 at 0x100000/4 and
the zero/zero terminator at 0x100008/C; only the single payload DWORD at 0x200000
is mapped for writes. These are external PCI environment/observer states, not
FPGA FFs. Every compared register still uses its original configured contract.
The same execution has the four host accepts, descriptor address at35, external
read accepts at37/38, delayed internal A at38/39, and captured count1/address
0x200000. Thus descriptor acceptance/capture now extends this initialized
**conditional** prefix, rather than a separately seeded capture state.

With no receive/remote edges, no payload write is accepted in that witness.
IRQ by52 and the joint captured-descriptor-plus-IRQ query are UNSAT with a SAT
base. Across the two environments all12 queries terminate:9 SAT,3 UNSAT.
No actual completed acquisition, final payload, retry or host-visible completion
has been reconstructed. This bounded exclusion must not be turned into a
universal IRQ safety statement or an assumption of producer/link quiescence.
Choosing arbitrary RX words or seeding staging/pending state would not supply
the missing same-acquisition producer, CDC, output-sampling and ownership history.

| Sequence boundary | Current evidence classification | Exact unresolved condition |
|---|---|---|
| Initialization -> configuration -> IIMCL launch | Conditional initialized prefix, not joined cuts | Effective clocks/reset and applicable architecture behavior |
| Launch -> descriptor address/command | Conditional same-execution pin observation | PCI_CE can choose a value allowed by the symbolic model but not by the missing primitive |
| Descriptor acceptance -> count/address capture | Conditional configured pipeline under a memory responder; separate from the stalled case | Dedicated ready sampling, actual output timing and owned accepted-data equivalence |
| Descriptor load -> LVDS payload admission | Verified local receive/staging equations, end-to-end Unknown | Matched producer/link history, coherent CDC pointer snapshot and payload identity |
| Payload issue -> accepted write -> counter progress | Verified local equations; physical correlation Unknown | Issue is not A; A is delayed; pad ownership and final-word timing must agree in the same execution |
| Terminal descriptor -> completion IRQ | Verified local retained-state equations, global ordering Unknown | All prior reads/writes/retries/staged words terminal before IRQ, not merely word_A=0 |
| IRQ -> host-visible data -> release/restart | Unknown hardware/platform contract | Posted bridge writes, adapter coherency and exclusion of later accesses |

No trace from the older bound2/4 runtime-cut scenarios is appended to this
initialized prefix. In particular, their IRQ-with-Q witness cannot be used as
its completion suffix. The protocol/host/memory bookkeeping is an observer or
external environment, never a substituted FPGA state register.

The prioritized retry/final-word/disconnect/counter-zero/stop cases therefore
retain the classifications in the handshake matrix: local issue, rewind and
counter transitions Verified; semantic acceptance/retirement Conditional or
Unknown; no legal initialized retry/stop-to-late-write counterexample is proved.
The stalled-fetch experiment closes one narrower independent offline avenue.
No new credible applicable PCI_W_VE source was supplied; the previously audited
missing primitive remains the boundary. Repeating another-family searches
would not resolve it.

### Minimal next evidence request

Firmware-recoverable work and separately authorized observation must stay distinct:

| Evidence class | Minimum artifact / fields | What it can establish |
|---|---|---|
| Existing architecture/library material | Applicable XC2S/Spartan-IIE PCI_W_VE PCILOGIC behavioral **and timing** model, with I1/I2/I3 and dedicated IRDY/TRDY tap polarity, PCI_CE edge/reset and AD/CBE enable behavior; version/device applicability required | Close the first concrete output-sampling gap; no new firmware dump is needed for this |
| Existing firmware/configuration records | Hash/identity linking installed PCI U3 and acquisition U21/U38 images and assembly revision to these decoded images; applicable DLL/BUFGCE/BRAM startup/collision contracts | Apply configured-image behavior and clock/memory assumptions to a real device |
| Core simulation after architecture closure | One reset/enumeration/SGTA/IIMTC/IIMCL-to-IRQ execution with table addresses/counts/chains, LVDS word identities, receive/staging pointers, W/issue/rewind/A, Q/V, segment counters and IRQ; retain retry and disconnect history | Correlate descriptors, staged words and owned transactions without joining SAT cuts; invariants still require proof beyond one successful trace |
| Separately authorized passive PCI observation | Clock/reset, REQ/GNT, FRAME/IRDY/TRDY/STOP/DEVSEL, AD/CBE/byte enables and INTA, plus bus ownership/turnaround and allocation/table identity for the same acquisition; capture pre-launch pending IRQ and final-word waits/retry/disconnect | Falsify or corroborate actual ownership/acceptance timing. A finite quiet interval is not a proof of no future write |
| Separately authorized passive link/internal evidence | Same-epoch LVDS producer/end-of-data, queued words and CDC/pointer state, or a validated internal trace correlated with PCI accepts and IRQ | Resolve retained/reissued payload and pending retry after apparent completion; external PCI pins alone cannot prove internal FIFO empty |
| Platform contract | Actual bridge/chipset identity, applicable posted-write/INTx ordering and Windows adapter completion/coherency contract | Accepted PCI write -> host visibility; logically independent from endpoint IRQ ordering |

For each captured phase record the owning initiator, address/command, descriptor
or payload token, accepted byte mask, whether STOP terminates with/without data,
and replay/retirement identity. An observation request must include stalled
descriptor and final-word cases, initial retry, partial disconnect and stop with
delayed activity. These are **specifications only**, not permission for MMIO,
DMA launch, reset, probing the licensed scope or releasing quarantined memory.

## Windows and PCI ordering implications

DMA logical addresses must come from the applicable adapter mapping; the
current legacy PFN builder does not acquire IOMMU safety from this analysis.
[MapTransferEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pmap_transfer_ex)
requires matching adapter flushing. [FlushAdapterBuffersEx](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pflush_adapter_buffers_ex)
is a coherency/end-of-transfer operation; calling before DMA finishes can
cancel a transfer or cause undefined behavior, not a WR6k stop protocol.
[PutScatterGatherList](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/nc-wdm-pput_scatter_gather_list)
is cleanup after scatter/gather I/O. Adapter teardown is subject to completed
DMA, per [DMA_OPERATIONS](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-_dma_operations).

PCI-SIG's [PCI Local Bus Specification 3.0](https://e2e.ti.com/cfs-file.ashx/__key/communityserver-discussions-components-files/639/1016.PCI.Local.Bus.Specification.Revision.3.0.pdf),
section 3.2.5.2 (printed pp.61-62), distinguishes posted bridge writes and
sideband interrupts: INTx has no transaction-ordering relationship. An IRQ
alone does not prove host-memory visibility. Any proposed ordering read must
have verified side effects and applicability to the actual bridge/adapter;
this analysis neither identifies nor authorizes a live read experiment.
[Surprise removal](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-an-irp-mn-surprise-removal-request)
requires device/software teardown handling, but its notification alone is
not proof that a still-present device cannot DMA.

Pinned MDLs, map registers, descriptor common buffers, adapter ownership and
UnknownActive contexts require device admission closure, every outstanding
source retired, host completion/coherency and software rundown before release.
`CompletionObserved` remains distinct from `IdleProved`. Current successful
legacy release depends on an unverified hardware-ordering premise; do not
copy that premise into the inactive WDM v3 stage. NeverLaunched cleanup is a
separate software ownership proof with launch prevention and rundown.

## Remaining exact evidence and release obligations

All six are **Unknown**, independently: producer stopped; descriptor engine
stopped; no new PCI requests; outstanding transactions terminal; internal
buffers drained; host DMA writes completed. Stability until explicit restart
is also **Unknown**. Local equation proofs are not conditionally proved
end-to-end release obligations.

Targeted missing evidence, not a request to repeat available loader analysis:

| Missing evidence | Exact invariant blocked / targeted request |
|---|---|
| Spartan-IIE PCI_W_VE PCILOGIC | Applicable simulation/RTL truth and timing for I1/I2/I3 plus dedicated IRDY/TRDY -> PCI_CE, including reset/startup. Needed to turn all36 AD/CBE output bits into correctly sampled commands/data; Spartan-6 PCILOGICSE is not a substitute. |
| PCIIOB.PCI and effective clocks/reset | Dedicated tap polarity, sampling phase, DLL/BUFGCE edge/reset behavior and BRAM collision contract. Needed for own-master handshake, CDC admission and simultaneous read/write correctness. The named LH route is resolved; no missing alias evidence is requested for it. |
| Descriptor/transaction execution | Existing trace/core simulation starting from known SGTA/IIMTC and actual count/address/chain table; capture REQ/GNT, AD/CBE, output enables, FRAME/IRDY/TRDY/DEVSEL/STOP, every accepted phase, retry/disconnect/abort and IRQ. Needed for legal fetch, unique entry consumption, terminal/error progress and IRQ ordering. |
| Buffer execution/ownership | Initial valid/wrap state and synchronized receive write/read pointers, staging write_index/read_index, W/issue/rewind and actual accepted-write events for the same trace, including a stalled final word. Needed to distinguish retained/reissued words, wrap/full/empty, unique retirement and premature reuse. |
| Producer/end-of-data | Applicable HAD639E/MTT/MAM stop and last-word contract, including maximum queued data after START clear and independent producer completion. Needed for stopped production and final receive admission; no clock edge alone is not drain evidence. |
| Installed revision | Read-only existing identification/configuration records tying PCI U3 and acquisition U21/U38 images to the assembly under review. Needed to apply configured-image findings to the device, not to prove local Boolean equations. |
| Host bridge/platform | Bridge identity and applicable posted-write drain/interrupt ordering contract, plus Windows adapter mapping/coherency requirements for that platform. Needed for accepted PCI write -> host visibility and safe mapping release, independently of FPGA IRQ ordering. |

No full safe-shutdown proof is possible from the currently recovered cuts.
The remaining contract must cover these architecture, execution and platform gaps jointly. PR #9 stays draft,
open and unmerged; retain quarantine and do not enable live adapter DMA.
