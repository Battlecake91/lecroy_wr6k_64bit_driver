# PCI DMA initiator and descriptor retirement

## Result and evidence scope

Offline configured-image analysis establishes additional capture banks, two
32-bit counter data equations and the completion-source pipeline. It does
**not** establish a safe observable shutdown contract, even for normal IRQ
completion. All six release obligations and stability until explicit restart
remain **Unknown**. Preserve `UnknownActive`; no production source changes,
hardware accesses or mapping-release permission follow from this work.

Evidence: existing private BINARY.205 decoded network, original x86 control
flow and current x64 source. Project Combine is pinned to
`234343d23e737e57f2727630e19008b509d7d522`. `Verified` below describes configured
topology/local equations, not the installed hardware revision. The checks are
in `tools/fpga/virtexe_dma.py` and `tests/dry/test_fpga_dma.py`; complete private
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
| Address phase | FRAME data shadow B, drive shadow T=`X3,Y13 SLICE[0].XQ`; `T_CE=!T || (S && M)` | Address ownership/complete address-generation invariant not proved |
| Command / byte enables | All 32 AD and four C/BE output FFs use `X0,Y13 PCILOGIC.PCI_CE` | Exact family hard-block CE transfer function remains Unknown; command value alone cannot prove a write |
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
from the AD alternative. `p'=pn` when accept; complete phase scheduling and
accepted two-DWORD descriptor-fetch ownership remain unresolved.

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
| Interpreted | Word-A zero; Word-B counter source | Every chain/terminal case and byte/DWORD units |
| Transfer issued | Initiator controls, address-candidate mux and receive BRAM outputs reach AD paths | Complete selected address, command and payload provenance |
| Completed | Segment-zero -> terminal -> done -> IRQ pipeline below | Final accepted PCI phase and upstream write completion |
| Advanced / retired | Address-candidate bit2 can change at IIMCL clear | Full address increment, outstanding-descriptor conservation and stable stop |

Two independent combinational SMT checks establish these D-word equations
modulo 2^32, **not** automatic progress on every clock:

* IIMTC-associated counter: low-to-high `X29,Y13..28 SLICE[1].XQ,YQ`;
  `D=(cl && reload) ? captured_IIMTC : old-1`. Low/high CE gates are
  `X25,Y26 SLICE[0].X` / `X25,Y26 SLICE[1].X` and remain distinct.
* Segment counter: low-to-high `X20,Y10..25 SLICE[0].XQ,YQ`;
  `D=load ? word_B : (pending ? 2 : old-1)`.
  load=`X22,Y19 SLICE[1].YQ`, pending=`X26,Y18 SLICE[1].YQ`.
  The three load replicas have matched D/CE/SR/clock/INIT contracts; the SMT
  equation assumes their state equality, justified after matched initialization
  and clocks, not arbitrary independent power-up states.

The pointer bit2 candidate `X25,Y21 SLICE[0].YQ` selects SGTA bit2
(`X25,Y20 SLICE[0].YQ`) when cl2 and reload-shadow are set, else Word-A bit2
(`X25,Y18 SLICE[1].YQ`); CE=`X26,Y21 SLICE[1].X`.
This is not a proof of the full pointer's increment or of no fetch after clear.

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

PCI receive storage is `X46,Y9 BRAM` and `X46,Y13 BRAM`, two 256x16 halves.
Port A uses remote `X24,Y0 BUFGCE[0].O` and write enable
`X43,Y10 SLICE[1].YQ`. Port B is PCI-clocked, read-only, enabled and not reset
to empty. No remote clock edge or write-enable=0 does not empty its stored
payload or output state. Complete pointer/ownership conservation and retry
retention are unresolved. The payload/address mux cone includes the explicit
boundary `X27,Y17 SLICE[0].Y` input2: ambiguous alias
`X0,Y19 IOI[3].IQ` versus `X1,Y17 BRAM.DOB[1]`. Do not pick a source merely
because it makes a DMA interpretation convenient.

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
| 3 | Pointer candidate bit2 changes on clear | SAT | Full descriptor advancement Unknown |
| 4 | Receive write disabled, remote clock stopped, PCI activity | SAT | Buffered payload write attribution Unknown |
| 5 | STOP0/TRDY1, R0=1, Rd1=1, driven REQ active | UNSAT | R=1 inhibits REQ; NOT retry-then-write retirement proof |
| 6 | Host timeout observation with pending PCI activity | SAT | Timeout has no FPGA cancellation input |
| 7 | Qualified IIMCL restart with REQ active before/after | SAT | Outstanding work cannot be dismissed |
| 8 | IIMCL clear with receive write admission still enabled | SAT | Not a common admission barrier |
| 9 | Untagged IRQ latch | SAT | Physical generation provenance Unknown |
| 10 | Enabled FRAME/IRDY, FRAME1/IRDY0/TRDY0/DEVSEL0 | SAT | Prior ownership/address assumed; host bridge completion unmodeled |

All ten dispositions are **Inconclusive**. Queries 3-5, 9 and 10 deliberately
report the missing lifecycle predicate, rather than inventing descriptor
retirement, delayed-write or host-completion variables. No bounded UNSAT is
promoted to an unbounded safe-stop guarantee.

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

1. Family-correct Spartan-IIE PCILOGIC.PCI_CE/PCIIOB.PCI simulation or RTL
   contract and applicable dedicated-route alias selection for the named mux
   boundary; generic PCI descriptions or Spartan-6 PCILOGICSE are insufficient.
2. Original core RTL or an existing command-tagged PCI trace with initial
   SGTA/IIMTC/table, output enables, own-master address/CBE, accepted data,
   retry/disconnect/abort, pointer progression, counters and IRQ. Needed to
   close legal launch/fetch/advance/retirement invariants, not just pin activity.
3. Producer/end-of-data contract plus complete receive-buffer CDC pointer and
   valid/ownership conservation tied to the PCI consumption path; stopped
   remote traffic alone is insufficient.
4. Installed firmware/assembly identity and actual host bridge/DMA-adapter
   write-completion ordering contract. Update-image logic and normal x86
   operation do not establish these platform facts.

No full safe-shutdown proof is possible from the currently recovered cuts.
The remaining contract must cover all four gaps jointly. PR #9 stays draft,
open and unmerged; retain quarantine and do not enable live adapter DMA.
