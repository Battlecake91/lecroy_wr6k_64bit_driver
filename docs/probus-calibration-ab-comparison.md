# 2026-09-28: calibration and ProBus A/B comparison

> Historical sections below describe successive driver revisions. In particular,
> the earlier statement that x64 has no pending-0x0200 producer applies only
> before commit `70716ba`, not to the untested current HWInt patch.

This comparison uses four captured XStream sessions on the same instrument.

| Run | Trace file | User workflow |
|---|---|---|
| Legacy calibration | `legacy_xstream_trace_20260928_183122_x86_Caibration_original.jsonl` | CH2 voltage steps from 20 mV/div to 100 V/div |
| x64 calibration | `xstream_trace_20260928_183640_x64_calibration.jsonl` | Corresponding CH2 step sweep |
| Legacy ProBus | `legacy_xstream_trace_20260928_183409_probus_original.jsonl` | Start XStream, plug probe, wait for recognition, change sensitivity, Degauss, Auto Zero, exit |
| x64 ProBus | `xstream_trace_20260928_183807_probus_x64.jsonl` | Plug probe; no XStream reaction |

## Parsing methodology

The legacy format-3 tracer records both `nt_ioctl` and overlapping
user-mode `ioctl` observations. Count the device-level `nt_ioctl` entries
once to avoid double-counting. x64 lecdiag records `ioctl` directly. Relevant
legacy driver IOCTL counts here include the CFDC and 0x0022 device operations,
not unrelated Windows/HID traffic. CFDC2110 is parsed into the packed
8-byte record headers plus A5FB/85FB payloads.

The runs differ in total duration, background polling and number of startup
calls. Packet counts are *descriptive*, not rates or exact calibration counts.

## Ch2 calibration

The user corrected the earlier observation: each newly visited V/div step is
calibrated once and then cached; repeated visits do not cause another
calibration.

| Relevant item | Original x86 | Replacement x64 |
|---|---:|---:|
| Relevant driver IOCTLs | 33,849 | 42,761 |
| Non-success statuses | 0 | 0 |
| Family-1/0x96 | 120 | 120 |
| Family-1/0x81 | 61 | 61 |
| Family-0/0x90, selector 0x0E | 1,147 | 1,181 |
| Selector-0x0E common idle frame (`1200000F` data prefix) | 560 | 570 |
| Other selector-0x0E frames | 587 | 611 |

The 120 family-1/0x96 selectors match, as do their captured first 128 output
bytes for identical requests. This does *not* claim full output equality:
the x64 trace capture truncates stored `output_hex` at 128 bytes, while
legacy stores 526 bytes for this request.

No strong evidence of an x64-only repeated calibration loop remains.
Do not change this working area without a new reproducible observation.

## ProBus: first behavioral divergence

| Relevant item | Original x86 | Replacement x64 |
|---|---:|---:|
| Relevant driver IOCTLs | 20,923 | 20,690 |
| Non-success statuses | 0 | 0 |
| Family-0/0x90 SPI selector 0x0C | 75 | 195 |
| Standalone 85FB/0x01 pending 0x0080 | 642 | 679 |
| Standalone 85FB/0x01 pending 0x0200 | **3** | **0** |
| Family-1/0x82 | **3** | **0** |
| Family-1/0x4A | **19** | **0** |
| Family-0/0x4A | **16** | **0** |

The first legacy detection event occurs at sequence 14552:

```text
seq 14552: standalone command-status query
  input  = 0A0002000300FB854001
  output = 000000000400BF020002
  enabled mask = 0x02BF
  pending mask = 0x0200

seq 14553: acknowledge 0x0200
  input = 060006000300FBA5400088000002080002000300FB854000

seq 14560: first probe-related family-1/0x82
  input = 060004000300FBA540018200960102000300FB854000
```

Further pending-0x0200 detections occur at legacy seq 14588 and 14616.
After the first of these, family-1/0x82 provides the probe handshake and
0x4A traffic follows. Legacy seq 14596 family-1/0x4A response contains:

```text
...4150303135...
    A P 0 1 5
```

The trace documents identification and subsequent probe operations. It does
not by itself assign an exact user-facing Degauss or Auto Zero operation to
each individual 0x4A packet.

**The x64 trace never reaches this branch.** Each of its 679 standalone
85FB/0x01 responses is:

```text
000000000400BF028000
```

It reports pending 0x0080, not the probe-related 0x0200 transition. XStream
therefore never sends family-1/0x82 or family-0/1 opcode-0x4A. These classes
are already admitted by the replacement's existing firmware-forwarding
whitelist; they are not currently failing with STATUS_INVALID_DEVICE_REQUEST.

The x64 capture still sends selector-0x0C probe/SPI traffic, including repeated
`0C280000002A16F03F0000` packets. This rules out the simple explanation
that XStream never attempts any probe-related hardware communication.

## Additional identical-request JTAG difference

There is a second, independent, recurring response mismatch. For the same
family-1 opcode-0x42 mode-1 JTAG request:

```text
060020000300FBA54001420100000A004C000000DFC0000C04000000004C000200000000C00F2000120002000300FB854000
```

example steady-state responses are:

```text
Legacy: 000000000000000000000C00000000144020000000002000
x64:    000000000000000000000C00000000144032000000002000
```

Only captured output byte index 17 differs in this example (`0x20` vs
`0x32`). Similar x64/legacy distributions exist in both calibration and
ProBus traces. Do not assume that this byte is necessarily a probe-present
bit without recovering the bit-field and matching hardware state transitions.

## Current implementation and safe next investigation

The replacement DPC currently latches these command-status pending bits from
hardware interrupts:

```text
INTST 0x04 -> pending 0x0080
INTST 0x10 -> pending 0x0800
INTST 0x20 -> pending 0x0100
```

There is presently **no implemented producer for command pending 0x0200**,
although it appears in the legacy probe-insertion trace. Merely adding a
synthetic 0x0200 to the event map is not supported by the recovered ISR/DPC
logic and may hide a deeper probe-ring/transport behavior mismatch.

Next steps:

1. Run x64 XStream once with the real probe **connected before application
   startup**. Compare recognition against the hotplug-only failure.
2. Recover the original source/callback that latches pending `0x0200`, using
   Ghidra and the 14552/14588/14616 legacy transition as runtime evidence.
3. Inspect probe-ring/interrupt source status and the x64/legacy JTAG response
   bit `0x12` discrepancy without touching known-good acquisition DMA.
4. Only after identification works should sensitivity, Degauss and Auto Zero
   be tested independently.

Keep the <=5% CPU constraint for scope-side diagnostic work. Do not commit
proprietary driver or XStream binaries to the public repository.

## Follow-up: x64 probe preconnected at XStream startup (trace 193741)

The previous hotplug comparison led to a new controlled test. The same AP015
was attached *before* launching XStream on x64. XStream recognized the probe.
The user then invoked Degauss and Auto Zero.

New file: `xstream_trace_20260928_193741.jsonl`, 21,004 IOCTL records,
zero failing NTSTATUS results; 16,151 CFDC2110 calls.

| Relevant operation | Legacy hotplug | x64 hotplug | x64 preconnected |
|---|---:|---:|---:|
| Family-0 0x4A | 16 | 0 | **11** |
| Family-1 0x4A | 19 | 0 | **8** |
| Family-1 0x82 | 3 | 0 | 0 |
| 85FB pending 0x0200 | 3 | 0 | 0 |
| Failed CFDC2110 | 0 | 0 | 0 |

### Verified startup AP015 metadata exchange

```text
x64 seq 505: family-0/0x4A
  06000E000300FBA540004A02020001000001A000A100080002000300FB854000
  output: 0000000000000000000002000000

x64 seq 508: family-1/0x4A
  060004000300FBA540014A01080102000300FB854000
  output Information: 270
  captured prefix: 00000000000000000000020100000108004150303135...
                                             AP015
```

The first 128 output bytes of seq 508 are byte-identical to legacy seq
14596/14812 for the same input. x64 stores only the first 128 output bytes,
so full 270-byte equality cannot be claimed from the available trace.

This is a successful real probe-identification packet flow on x64, with no
pending-0x0200 notification. It proves that pending 0x0200 is needed for the
**asynchronous insertion route**, not as a blanket prerequisite for all
probe communication.

### Degauss / Auto Zero: commands present, one protocol difference remains

The user invoked both actions. The x64 trace records corresponding 0x4A
traffic. A byte-identical family-0 0x4A request ending in `47 00` has
different replies:

```text
Legacy, seq 24205:
  input  06000C000300FBA540004A0201000000010047000A0002000300FB854000
  output 0000000000000000000002000000FFFF

x64, seq 12082,12084,12085,12086,12087:
  input  06000C000300FBA540004A0201000000010047000A0002000300FB854000
  output 00000000000000000000040000000000
```

All NTSTATUS values are success, but different payloads and five x64
repetitions are not proof of identical physical result. The next family-1/0x4A
poll gives `...F700` under x64, whereas the comparable legacy polling gives
`...F200`. These statuses must be decoded before treating every special
probe function as fully compatible.

### Refined missing piece

A probe attached *after* XStream starts is not recognized under x64, and the
x64 hotplug trace never reports 85FB status pending `0x0200`. Legacy detects
three such events, then runs 0x82 followed by 0x4A. No such event is needed
when a probe is present during application initialization; 0x4A then works.

The original ISR/DPC has a distinct receive-transport source INTST `0x08`,
consumed by `FUN_00011390` via `FUN_000176A2` and
`FUN_000176D0`. The x64 DPC currently leaves this internal receive event
unmapped because synchronous firmware replies are polled. A lost unsolicited
probe-ring/receive notification is now a concrete hypothesis; **the source of
the legacy 0x0200 bit has not yet been proven**. Do not inject a synthetic bit.

Next controlled trace: start with recognized AP015, unplug it during the same
XStream session, then replug it; record both visual state transitions and
packet/event transitions. Preserve all known-good waveform/DMA behavior and
the <=5% CPU test constraint.


## Follow-up: unplug/replug while XStream remains running (trace 213834)

The third x64 ProBus run begins with the AP015 already connected and
successfully recognized. XStream did not detect its physical removal, and
the displayed waveform disappeared after reinsertion.

Observed across 73.143 s: 36,700 IOCTL records, zero failed statuses,
6,987 CFDC2138 calls all returning the requested lengths, and 1,613
standalone 85FB/0x01 queries with pending=0x0080 only. Family-0/1 opcode
0x4A occurs only in the startup identification at seq 509/511;
family-1/0x82 and pending=0x0200 are absent.

After t~27.1 s the large (>8 KiB) acquisition pattern gives way to regular
1 KiB / 2 KiB DMA, still successful. The exact user-action timestamp is not
recorded and cannot be inferred from the transition alone.

### Exact legacy event source found in raw assembly

The suspected INTST=0x08 path is now statically resolved, correcting a
Ghidra decompiler mistake:

```text
FUN_0001619A:
    FUN_000160A8(this, 1)
      -> DAT_0001CE18 |= 0x08
      -> FUN_00011E46 -> BAR0 INTEN |= 0x08
    then send firmware-response fetch and wait for internal RX event

FUN_000108D6 ISR:
    INTST & 0x08 -> BAR1 CLRIRQ = 2
    acknowledge INTST and enqueue DPC

FUN_00011390 DPC (raw asm 0x114A2..0x114C8):
    uint16_t hwIntWord;
    if (FUN_000176A2(transport, &hwIntWord)) {
        FUN_000157A6(commandStatus, hwIntWord);
        signal CFDC2180 event;
    }

FUN_000176A2:
    hwIntWord = (uint16_t)READ_REGISTER_ULONG(BAR1 + 0x410);
    if (hwIntWord) WRITE_REGISTER_ULONG(BAR1 + 0x410, 0);

FUN_000157A6:
    pendingMask |= enabledMask & hwIntWord;
```

The exported C for FUN_00011390 displayed `FUN_000157A6(..., 0)`
because it missed the out-parameter that FUN_000176A2 writes through its
stack argument. Raw assembly in `raw_114f2.asm.txt` explicitly loads
the 16-bit out value and pushes it as the call argument.

This is the real hardware bridge between an INTST 0x08 notification,
the 16-bit HWInt value (including observed probe bit 0x0200), and
the command-status result returned by standalone 85FB/0x01.

The x64 replacement previously never enabled INTEN bit0x08 in its polled
receive path and omitted that DPC branch. The implementation now restores
both, keeping the original source-specific CLRIRQ acknowledge and only
delivering genuinely read HWInt bits. The internal RX ready event itself
remains unnecessary for the existing bounded polling helper.

A single controlled post-patch recognition test is required before declaring
ProBus hotplug repaired. It must also verify waveforms remain present after
removal and reinsertion.


## Four-trace ZIP revalidation and post-patch discriminator (2026-09-28)

The new-chat ZIP `lecroy_probus_handoff_20260928(1).zip` contains precisely
these four historical traces: legacy hotplug `183409`, x64 hotplug `183807`,
x64 preconnected `193741`, and x64 removal/reinsertion `213834`. The ZIP
contains **no post-70716ba trace**. A separate offline JSONL parse confirmed
these discriminators (count only legacy `nt_ioctl`, not overlapping `ioctl`):

| Trace | Standalone 85FB/0x01 pending 0x0200 | Family-1/0x82 | Family-0/0x4A | Family-1/0x4A | CFDC2138 |
|---|---:|---:|---:|---:|---:|
| Legacy hotplug 183409 | 3 | 3 | 16 | 19 | 4,250 |
| x64 hotplug 183807 | 0 | 0 | 0 | 0 | 3,824 |
| x64 preconnected 193741 | 0 | 0 | 11 | 8 | 3,875 |
| x64 unplug/replug 213834 | 0 | 0 | 1 | 1 | 6,987 |

The three real legacy pending transitions occur at seq **14552, 14588,
14616**, enabled mask `0x02BF`, pending `0x0200`. The corresponding
family-0/0x88 acknowledgements carrying mask `0x0200` occur at seq
**14553, 14589, 14620**. The first family-1/0x82 follows at seq
**14560**, and the AP015 metadata is returned in subsequent opcode-0x4A
traffic. There is no requirement that a new session reproduce the same
number of physical events or sequence distances.

For the focused first post-patch test, distinguish a genuine unsolicited
status transition from preconnected startup enumeration: inspect the output
of *standalone* 85FB/0x01 status requests (`output_hex` offsets 6..7 =
enabled WORD; 8..9 = pending WORD, both little endian), not arbitrary
occurrences of byte sequence `0002` inside firmware payloads. Inspect the
subsequent family-0/0x88 mask, family-1/0x82 handshake, family-0/1 opcode
0x4A, IOCTL NTSTATUS and continuous CFDC2138 transfers. A successful DMA
return length alone does not prove a visible waveform. Correlate any
acquisition pattern transition to separately noted physical action times.


## Post-70716ba HWInt hardware validation: trace 230614 (2026-09-28)

The first scope-side Debug XStream run after the interrupt-restoration patch
was reported as successful by the user: start with AP015 attached, change its
A/div setting to a higher range, unplug AP015 while XStream remains running,
reconnect it, then close XStream. XStream visibly recognized the changes,
including the physical disconnect/reconnect. Trace:
`xstream_trace_20260928_230614.jsonl` (not stored in the public repo).

**Captured (not reconstructed) results:**

| Evidence | Value |
|---|---:|
| Captured IOCTL records | 19,288 |
| First/last IOCTL sequence | 1 / 21,761 |
| Snapshot omissions in sequence stream | 2,473 entries across 30 gaps |
| Non-success NTSTATUS among captured IOCTLs | 0 |
| CFDC2110 | 14,742 |
| CFDC2138 | 3,633 |
| CFDC2138 reported-length mismatches | 0 |
| Standalone 85FB/0x01 command status | 720 |
| Enabled command mask in all captured status reads | `0x02BF` |
| Pending `0x0080` | 718 |
| Pending `0x0200` | **2** |
| Family-0/0x88 with ack mask `0x0200` | **2** |
| Family-1/0x82 | **2** |
| Family-0/0x4A | 2 (startup, reinsertion) |
| Family-1/0x4A | 2 (startup, reinsertion) |

The trace's first event and second event are respectively consistent with
probe removal and insertion (in the user's stated action order). User action
timestamps were not independently recorded. Relative seconds below use the
same 10-MHz timestamp tick conversion as the preceding trace analyses.

| Relative time | Sequence | Evidence |
|---:|---:|---|
| ~13.908 s | 509 | Startup family-1/0x4A AP015 metadata, Information 270 |
| ~28.422 s | 14951 | Standalone 85FB output `000000000400BF020002`, pending 0x0200 |
| ~28.449 s | 14952 | Family-0/0x88 mask 0x0200, success |
| ~28.480 s | 14961 | Family-1/0x82, Information 412; no follow-on 0x4A |
| ~31.275 s | 16639 | Second standalone pending 0x0200 |
| ~31.299 s | 16640 | Second family-0/0x88 mask 0x0200, success |
| ~31.330 s | 16651 | Second family-1/0x82, Information 412 |
| ~31.469 s | 16678 | Family-0/0x4A setup, success |
| ~31.515 s | 16679 | Family-1/0x4A AP015 metadata, Information 270 |

Both status events carry the *real returned* pending WORD `0x0200`, not a
match of arbitrary bytes within an unrelated firmware payload. After both
acks, later standalone status reads again report pending `0x0080`. The
captured first 128 bytes of the 270-byte family-1/0x4A metadata reply at
startup seq 509 and reinsertion seq 16679 are **byte-identical**; the same
captured 128-byte prefix also matches the original legacy AP015 metadata
response seq 14596/14812. Do not claim equality of uncaptured remaining
142 bytes.

All 3,633 captured CFDC2138 responses have `STATUS_SUCCESS`,
`Information=4` and a returned DWORD equal to the requested byte count.
There are 2,332 DMA records before the first notification, 328 between
notifications and 973 after the second. The closest recorded DMA records
straddle event one with a ~36-ms gap, and event two with a ~397-ms gap;
acquisition transfers resume and continue to trace end (~39.58 s). The last
recorded transfer >=8 KiB is at ~27.067 s, before the first event; thereafter
the trace mostly uses 1,024- and 2,048-byte transfers. The user changed
probe A/div before unplugging, but without independent action timestamps it
is not possible to attribute that transfer-size transition precisely.

**Outcome:** The focused real-hardware test supports that the missing
INTEN-0x08 / INTST-0x08 / HWInt command-status path has been restored and
that asynchronous AP015 removal/reinsertion reaches XStream's ordinary
0x0200 -> 0x88 -> 0x82 route and post-insertion 0x4A identification.
The observed two event notifications do not show a repeated-0x0200 storm;
this IOCTL JSONL alone does not contain raw ISR/DPC counts or CPU usage.
Snapshot gaps mean it cannot prove an absolute absence of any omitted IOCTL
failure. Normal probe recognition is established by both the trace and the
user's visual observation; the trace alone cannot certify displayed waveform
pixel continuity. This is a focused success, not universal ProBus parity.

**Still open:** The separate family-0/0x4A `...47 00` Degauss/Auto Zero
payload discrepancy was not exercised in this run. Preserve current PCI,
DMA, interrupt and transfer behavior. Any future probe-specific test should
isolate each special function without coupling it to another hotplug change.


## 2026-09-28 23:16: Degauss / Auto Zero and four jaw-state transitions

New user-supplied pre-framing-fix x64 trace:
`xstream_trace_20260928_231656.jsonl`. The user started XStream with AP015
connected, invoked **Degauss (with subsequent automatic Auto Zero)**, opened
the clamp, closed it, opened it again, and finally closed it in the locked
position. There are no separate timestamps for individual hand actions.
This trace was captured after the successful HWInt patch `70716ba`, but
**before** the newly identified 85FB response-format correction
`34907090820a582ac360d02120316c8792d7c888`.

### Captured counts and continuity

| Captured fact | Result |
|---|---:|
| IOCTL records / sequence range | 35,458 / 1..39,998 |
| Missing entries due to 39 trace-snapshot gaps | 4,540 |
| Non-success NTSTATUS among captured records | 0 |
| CFDC2110 | 27,100 |
| CFDC2138 | 6,701 |
| CFDC2138 successful, Information=4, returned bytes=requested bytes (input offset 11) | 6,701 |
| Standalone 85FB/0x01 | 1,438 |
| Enabled mask across observed standalone status | 0x02BF |
| Pending exactly 0x0080 | 1,432 |
| Pending exactly 0x0200 | 4 |
| Pending exactly 0x0280 | 1 |
| Pending exactly 0x0000 | 1 |
| Family-1/0x82 | 5 |
| Family-0/0x4A / family-1/0x4A | 36 / 18 |

**DMA parsing correction:** The 15-byte CFDC2138 input is
`DWORD token; BYTE channel_count; BYTE pair_marker; BYTE channel;
DWORD config; DWORD requested_bytes`. The requested byte count is at
input byte offset **11**, not offset 0 (which contains the opaque transfer
token). All 6,701 captured output DWORDs match that actual byte count.
Most frequent requests: 2,048 bytes (3,399), 1,024 bytes (1,409),
8,192 bytes (915), 5,120 bytes (366), 167,936 bytes (309), and
21,504 bytes (255). Transfers continue to the end at ~68.178 s.
No observed loss of the DMA IOCTL stream; screenshot-level waveform
continuity is not established by this kernel trace alone.

### Genuine pending-0x0200 transitions and state correlation

Relative time uses 10-MHz trace ticks, starting at seq 1.
Every recorded status event below has a corresponding acknowledged
family-0/0x88 and a subsequent family-1/0x82 handshake. The interpretation
of mechanical state uses the user's known action order, not a formally
recovered firmware bit-field.

| Approx. time | Status seq / pending | 0x88 seq / ack mask | 0x82 seq / first status pair | Correlated action |
|---:|---|---|---|---|
| 34.163 s | 19902 / 0x0200 | 19903 / 0x0200 | 19916 / `12 00 A7 00` | Post-Degauss notification; clamp still closed |
| 41.812 s | 24430 / **0x0280** | 24431 / **0x0280** | 24443 / `12 00 58 00` | First opening |
| 57.832 s | 33777 / 0x0200 | 33778 / 0x0200 | 33787 / `12 00 A7 00` | First closing |
| 59.820 s | 34908 / 0x0200 | 34909 / 0x0200 | 34918 / `12 00 58 00` | Second opening |
| 61.894 s | 36079 / 0x0200 | 36080 / 0x0200 | 36089 / `12 00 A7 00` | Closing in locked position |

After the combined 0x0280 notification at seq 24430/24431, another
standalone status at seq 24437 returns 0x0000 and XStream issues a
family-0/0x88 mask-0x0000 request at seq 24438. The two jaw states
alternate `0x0058 -> 0x00A7 -> 0x0058 -> 0x00A7`, in perfect order
with the physical transitions. The `0x00A7` value is also seen in
the post-Degauss notification. This is an empirical state correlation,
not yet a field-by-field decoding (e.g. whether lock state uses other bits).
No gaps occur immediately around the five captured transitions.

Measured gaps between the nearest recorded DMA IOCTLs straddling the
five notifications: approximately 185, 112, 34, 35 and 44 ms
respectively. DMA record counts: 2,884 before the first notification;
900 / 1,720 / 224 / 232 between successive notifications; 741 after
the last. Do not read these scheduler/UI delays as proof of board DMA
stalls. Continuous successful transfers and the absence of an NTSTATUS
failure are the supported observations.

### Exact Degauss / Auto Zero comparison remains divergent

The input `family-0/0x4A ... 47 00` appears five times at new x64
seq **19883..19887**, at t~33.946..34.070 s, each returning
`00000000000000000000040000000000`. The following family-1/0x4A
`...01 0A` status at seq 19895 returns `...04000000F700`.

Five `family-0/0x4A ... 47 12` requests follow at seq
19938, 19939, 19941, 19942, 19943 (t~34.349..34.475 s),
each with the same `...040000000000` response; family-1/0x4A status
seq 19947 returns `...04000000F100`. The association of this burst
with automatically invoked Auto Zero is supported by the user's action
order.

Four more five-request `47 12` bursts follow the four jaw events:
seq 24460..24464 (status F7), 33802..33806 (status F3),
34933..34937 (status F7), and 36104..36108 (status F3).
**Total in this capture: five `47 00` and 25 `47 12`.** These bursts
show that XStream invokes or retries an automatic adjustment upon each
mechanical transition; they do not by themselves prove the physical
calibration outcome.

The byte-identical legacy x86 requests in
`legacy_xstream_trace_20260928_183409_probus_original.jsonl` returned:

| Identical request | Original x86 captured occurrences / reply | New x64 captured occurrences / reply |
|---|---|---|
| `...47 00...` | seq 24205, once: `0000000000000000000002000000FFFF` | five: `00000000000000000000040000000000` |
| `...47 12...` | seq 14895 and 14900, twice: `0000000000000000000002000000FFFF` | 25: `00000000000000000000040000000000` |
| Family-1/0x4A `...01 0A...` | seq 14897, 14901, 24207: `...04000000F200` | six: F7, F1, F7, F3, F7, F3 |

For identical family-1/0x82 input, legacy original responses have a
variable actual host-header count `0x0016` or `0x0006`, with unused
bytes pre-filled `0xFF`; the current x64 output invariably reports
capacity `0x0190` (400 bytes) with unused zero bytes. Both IOCTL calls
report full `Information=412`, which is the caller's aggregate output
buffer size, not the actual response payload count.

### Static root cause: original 85FB raw-response framing

The original decompilation
`ghidra_exports/selected/000167f4_FUN_000167f4.c` explicitly:

1. Allocates the caller-requested output record size and fills **every
   byte with 0xFF** before soliciting the firmware response.
2. Calls `FUN_0001619A` with destination `response + 6` and capacity
   `requested_record_bytes - 6`, obtaining an **actual received byte
   count** via the out-parameter.
3. On success, sets response DWORD 0 to status and response WORD +4
   to that actual received byte count, preserving unused 0xFF padding.
   `FUN_000169B4` then copies the whole output record to the caller.

The previous x64 `driver/Ioctl.c` raw 85FB branch instead zeroes the
aggregate SystemBuffer up front and advertises
`recordOutput - 6` regardless of the received byte count. It copies
only the actually received words, leaving the rest as zero. This
source-level defect explains the *shape* of both the `0x4A` and `0x82`
response differences. It is separate from real firmware-state values,
which remain to be compared after this formatting defect is corrected.

**Host-only patch `34907090820a582ac360d02120316c8792d7c888`** now
fills only a raw-hardware 85FB result record with 0xFF and writes
`min(actual_received, payload_capacity)` into its length header, while
retaining the separately validated family-1/0x99 explicit override.
The existing original response-fetch trigger, INTEN/HWInt ISR/DPC,
BAR1 RX transfer, DMA, PCI, command whitelist, normal local responses
and other acquisition logic were not changed.

**This patch has NOT been compiled or exercised on the scope yet.** It is
expected to correct both reply formats if the actual received 16-bit words
are the same as the legacy run, but the currently available IOCTL
`output_hex` cannot independently expose `LecTransportReceive`'s
internal `received` count. Do not mark Degauss / Auto Zero or reply
parity as proven until a fresh post-patch capture confirms the result.


## 2026-09-28 23:31: first real regression of raw 85FB framing patch

User workflow in new trace `xstream_trace_20260928_233125.jsonl`:
start x64 XStream with AP015 attached, invoke Degauss, subsequently trigger
Auto Zero **manually**, then open/close the clamp several times, and exit.
This is the **first observed hardware capture after host-only
`driver/Ioctl.c` patch `3490709`**. Do not conflate this test with the
preceding automatic post-Degauss Auto Zero run `231656`. Hand-action
timestamps and XStream's visual response to clamp opening/closing were
not separately logged.

| Captured result | New trace 233125 |
|---|---:|
| Duration from first to last captured IOCTL | ~55.628 s |
| Captured IOCTL records | 27,947 (seq 1..30,856) |
| Unobserved entries in 29 capture-snapshot gaps | 2,909 |
| Last snapshot gap ends | ~28.646 s (none later) |
| Non-success NTSTATUS among captured calls | 0 |
| CFDC2110 | 21,366 |
| CFDC2138 | 5,219 |
| DMA return DWORD equals request DWORD at input byte offset 11 | 5,219/5,219 |
| Standalone 85FB/0x01 | 1,135 |
| Enabled command mask | 0x02BF on every captured standalone status |
| Pending command mask | **0x0080 on all 1,135 captured status reads** |
| Pending containing 0x0200 | **0** |
| Family-1/0x82 | **0** |
| Family-0/0x4A | 8 |
| Family-1/0x4A | 10 |

The AP015 was correctly recognized at startup through family-1/0x4A seq
**507**, `Information=270`, with ASCII `AP015`. The captured 128-byte
response prefix is byte-identical to startup/reinsertion in run `230614`,
startup in `231656`, and the legacy x86 seq 14596/14812. Full 270-byte
equality cannot be inferred because the x64 output preview is truncated.

### Requested-length/0xFF-padded raw reply now matches legacy exactly

The same family-0/0x4A `...47 00` complete request is recorded at:
- seq **8573**, t=**20.034487 s**, output
  `0000000000000000000002000000FFFF`;
- seq **22156**, t=**40.675207 s**, identical output.

Each is followed by family-1/0x4A `...01 0A` at seq 8574 and 22158,
respectively. Both status responses now report
`0000000000000000000004000000F300`.

This is byte-for-byte equality for the **47 00 response** with the original
legacy x86 seq 24205. The old x64 run `231656` sent this request five
times in a short burst (seq 19883..19887), each with incorrect
`00000000000000000000040000000000`. In this new run there are only
the two separately timed 47 00 requests; **there is no five-attempt burst
for either action**. The corrected framing is therefore hardware-exercised
and effective for this packet. The first and second requests correlate with
the user-requested Degauss and manual Auto Zero, in that order. This also
invalidates any overly broad claim that opcode subcommand `47 12` is the
necessary representation of every Auto Zero action: **no 47 12 request
was captured in this manual run**. Its earlier bursts in `231656`
were associated with automatic follow-up/status notifications; exact
firmware semantics remain to be recovered.

The separate family-1/0x4A follow-up status is **F3 in both new requests**.
The compared legacy x86 examples report **F2**. The one-bit difference
(`0xF3` vs `0xF2`) is a real observed firmware/status payload difference,
not the host reply-length and FF-padding mismatch just fixed. The physical
probe state and full semantics of that flag are not established; do not
force the value to F2 or claim physical Degauss/Auto Zero calibration
equivalence based only on successful IOCTL responses.

The new run contains **zero `47 12` requests**, so it does not directly
validate FF/length parity for that individual subcommand; it shares the
now-tested 85FB host formatting code. It also contains **zero
family-1/0x82 replies**, so the predicted original-like 0x0006/0x0016
length and FF-padded tail for 0x82 have **not** yet been hardware-tested.
Do not mark that part independently validated.

### Separate concern: no captured jaw event notifications

The previous post-HWInt / pre-framing run `231656` had five spontaneous
status events containing `0x0200`, each followed by family-0/0x88
acknowledgement and family-1/0x82. With the corrected formatter, **all
1,135 captured standalone status queries** still show enabled `0x02BF`
but only pending `0x0080`. There are no family-1/0x82 or
mask-0x0200 0x88 acknowledgements anywhere in the captured stream,
although the user reports multiple physical open/close operations.
Importantly, the final trace snapshot gap ends at ~28.646 s; **447 status
reads after t=40 s are captured with no gaps and all report pending
0x0080**. This cannot be dismissed as a missing snapshot around a later
mechanical event. It does not by itself show whether the device failed
to assert INTST/HWInt, the ISR/DPC missed it, XStream saw stale probe
state, or physical actions fell into a different operating state. The
host-only framing patch does not directly alter INTEN, INTST, HWInt, PCI,
or DMA, but an indirect host protocol state change remains possible.
There is no raw ISR/HWInt counter in this JSONL.

Continuous acquisition remains recorded: all 5,219 captured CFDC2138
calls have status success, Information 4 and a returned DWORD matching
the correctly parsed requested byte count. The last recorded DMA at
~55.627 s is a successful 1,024-byte transfer; **1,666 DMA records occur
after the second 47 00 command**. This is not independent proof of
visible waveform pixels or all mechanical UI state transitions.

**Decision:** preserve driver patch `3490709` as a narrowly verified
host formatting correction. Do not make another speculative interrupt,
PCI or DMA change on the basis of one missing-notification capture.
Next run should first exercise exactly one AP015 open/close pair with
no preceding Degauss or Auto Zero, record whether XStream visibly changes
its jaw state and note relative action times, then repeat one pair after
a controlled calibration if stable. Check standalone 0x0200, 0x88,
0x82, actual raw reply lengths/FF tails and DMA. This discriminates a
jaw-notification regression from a calibration-dependent state without
confounding another source change.


## 2026-09-28 23:53: jaw-only before and after calibration (235314)

New user-supplied scope trace `xstream_trace_20260928_235314.jsonl`,
running the **same post-`3490709` driver**, follows the precise
controlled procedure prescribed after run `233125`: start with AP015
preconnected and recognized, open/hold/close/hold once **before any
calibration**, invoke Degauss and then **manual** Auto Zero, open/hold/
close/hold again, and exit. The user explicitly confirms having performed
the requested sequence; separate physical-action timestamps and
screen-visible probe-jaw status were not supplied.

| Captured measurement | Result |
|---|---:|
| Duration from first to last IOCTL | 72.009226 s |
| Captured IOCTLs; sequence range | 36,366; 1..42,227 |
| Trace-snapshot gaps; omitted sequence entries | 36; 5,861 |
| Last snapshot gap | seq 28,753..28,793, t~48.439..48.480 s |
| Captured NTSTATUS failures | **0** |
| CFDC2110 | 27,834 |
| CFDC2138 | 6,818 |
| CFDC2138 returns exactly requested input-offset-11 byte count | **6,818/6,818** |
| Last CFDC2138 | t~72.008570 s, 1,024 requested / 1,024 returned |
| Standalone 85FB/0x01 reads | **1,517** |
| Enabled command mask in every standalone read | 0x02BF |
| Pending command mask in every standalone read | **0x0080 only** |
| Pending 0x0200 or 0x0280 | **0** |
| Family-1/0x82 responses | **0** |
| Family-0/0x88 mask-0x0200 acknowledgements | **0** |
| Family-0/0x4A `47 00` | 2 distinct user-action-correlated calls |
| Family-0/0x4A `47 12` | 0 |

The chronological split by the actual recorded `47 00` control/query
requests, which correlate with Degauss and manual Auto Zero in the
user's known action order, removes the prior calibration-state ambiguity:

| Phase of the requested procedure | Observed time interval | Standalone status reads | 0x0200-bearing replies | DMA replies |
|---|---:|---:|---:|---:|
| AP015 identified, **before Degauss** | ~17.000..40.156 s | **622** | **0** | 3,207 |
| Between Degauss and manual Auto Zero | ~40.156..54.337 s | **381** | **0** | 1,622 |
| After manual Auto Zero | ~54.337..72.009 s | **514** | **0** | 1,989 |
| Uninterrupted last-snapshot interval | ~48.480..72.009 s | **648** | **0** | 2,569 |

Note that the last-snapshot interval overlaps the middle and last
phases; it is not an additional disjoint total. No missing sequence
snapshot after ~48.480 s can explain the complete absence of a
post-Auto-Zero mechanical status transition. All **1,521**
steady-state family-0/0x88 acknowledgements carry mask 0x0080, not
0x0200 (the other two captured family-0/0x88 controls are initial
mask 0xFFDF and mask 0x001F). No 0x82 is sent because XStream never
receives a status flag that would direct it into that path.

### Short raw-reply parity is independently reconfirmed

- At **seq 508**, t~17.084 s, AP015 is recognized by startup family-1/
  0x4A metadata, Information=270. Its captured first 128 reply bytes
  equal the same prefix in pre-formatter trace `231656` and prior
  legacy-equivalent startup captures. This does not compare any bytes
  beyond the trace tool's capture limit.
- First identical family-0/0x4A `...47 00` request at **seq 23386**,
  t~**40.155959 s** returns exactly
  `0000000000000000000002000000FFFF`; family-1/0x4A `...01 0A`
  at seq **23388**, t~40.186 s, returns
  `0000000000000000000004000000F300`.
- Second `...47 00` request at **seq 31835**,
  t~**54.336670 s** returns the same full original-x86-matching
  `...02000000FFFF`. Corresponding family-1/0x4A status seq
  **31836**, t~54.368 s, again contains **F3** (reference x86 F2).
  The two `47 00` requests are separated by ~14.181 s and
  **do not** repeat in five-call bursts.
- The original x86 first family-1/0x99 raw firmware-read response
  has 0xFF padding immediately after its short valid data; the
  **new first 128 captured bytes match exactly**, including the
  `0000000000000000000002000200FFFFFFFF...` prefix. Pre-formatter
  x64 `231656` instead zero-padded the tail. Thus the host-only
  `FUN_000167F4` serializer fix is demonstrated by more than one
  request form.
- No `47 12` and no family-1/0x82 in `235314`. The actual post-fix
  hardware formatting of those exact short-response subcommands
  is still not independently observed.

### Updated inference and remaining unknowns

The fact that **no status transition occurs in 622 captured status
reads BEFORE any Degauss**, as well as 381 between calibration actions
and 514 afterward, rules out *calibration being a necessary trigger of
the absence of 0x0200* in this test. It does **not** prove that raw
response formatting causes the missing probe notifications.

Timeline across controlled driver builds:

- `230614`, HWInt patch `70716ba` but old raw serializer: true
  pending 0x0200 twice on unplug/replug, normal AP015 reidentification.
- `231656`, same old raw serializer: five authentic pending-0x0200
  events correlating with Degauss plus clamp open/closed transitions;
  family-1/0x82 replied with jaw-state WORDs 0x0058/0x00A7.
- `233125`, after host-only raw serializer patch `3490709`:
  zero pending 0x0200, despite multiple reported jaw actions.
- **`235314`, same patched driver and intentional pre/post-calibration
  open/close comparison**: still zero pending 0x0200 in every phase.
  DMA and the corrected `47 00`/0x99 response parity remain stable.

The standalone reported enabled **0x02BF is
`LegacyCommandEnableMask` (software event bits)**, not the actual
BAR0 INTEN MMIO DWORD at offset `0x084`. The relevant board-side
receive-interrupt enable bit is INTEN **0x08**. The current JSONL
contains neither that physical register value, the corresponding
`InterruptEnableShadow`, BAR0 INTST snapshots, raw BAR1 HWInt, nor
ISR/DPC counters. Its absence of 0x0200 cannot distinguish board
notification not generated, hardware INTEN bit lost, ISR not seeing
INTST 0x08, DPC missing HWInt 0x410, or an indirect protocol-state
change. No such cause is yet established. The user's visual observation
of whether XStream indicated jaw state has not been separately supplied.

**Proposed smallest next discriminator, with the driver unchanged:**
one AP015 **unplug/replug** during a short post-`3490709` XStream
session, avoiding Degauss/Auto Zero/jaw movements. Note whether XStream
visibly detects removal and subsequent AP015 reconnection, and capture
status `0x0200 -> 0x88 -> 0x82 -> 0x4A` alongside DMA as in the
pre-formatter `230614` baseline. If even a physically disconnected
probe no longer generates 0x0200, measure BAR0 INTEN `0x084`,
INTST `0x080`, and BAR1 HWInt `0x410` through bounded/passive
diagnostics before proposing any interrupt change. Avoid blind
register writes and synthetic pending masks; maintain the <=5% CPU
requirement. If unplug/replug remains functional, investigate the
mechanical-jaw notification path separately.


## 2026-09-29 00:11: repeated physical hotplug on current post-formatter driver

The user performed several physical AP015 unplug/replug cycles during a
single XStream session, with the **unchanged post-`3490709` driver**,
without the earlier pre/post-calibration jaw-only sequence. They report
correct recognition on the later insertions; the first insertion
temporarily appeared in XStream as a different ("1/2 clamp") probe type,
possibly owing to connector seating. There are no independently noted
physical-action timestamps or screenshot. Private user trace:
`xstream_trace_20260929_001152.jsonl` (not committed to the public repo).

### Quantitative result

| Captured measurement | Value |
|---|---:|
| First/last seq; elapsed duration | 1..37623; 61.381146 s |
| Actually captured `type=ioctl` records | **32,675** |
| Capture-snapshot gaps / omitted seq entries | 34 / 4,948 |
| End of final snapshot gap | **t=32.792085 s** |
| Captured non-success NTSTATUS | **0** |
| CFDC2110 | 24,976 |
| CFDC2138 | **6,135** |
| CFDC2138 returned DWORD equal to requested DWORD at input offset 11 | **6,135 / 6,135** |
| Last DMA | seq 37611, t=61.380523 s, 1,024 requested/returned |
| Standalone 85FB/0x01 status reads | **1,361** |
| Software command-enable word | **0x02BF** throughout |
| Pending 0x0080 / 0x0200 / 0x0280 / 0x0000 | **1,348 / 11 / 1 / 1** |
| Genuine notifications containing bit 0x0200 | **12** |
| Corresponding family-0/0x88 mask 0x0200 / 0x0280 | **11 / 1** |
| Subsequent family-1/0x82 | **12** |
| Full AP015 metadata family-1/0x4A (`Information=270`) | **4** (startup + later three reinsertions) |
| Family-0/0x4A `47 12` | **3** (one after each later successful reidentification) |

All observed notifications have nearby 0x88 and family-1/0x82
sequences; no trace-snapshot gap occurs in the actual event clusters.
**Nine of the twelve 0x0200-bearing notifications happen after the
last snapshot gap**, so they are contiguous even at trace-capture level.
They are real returned pending bits from the standalone status result
(`000000000400BF020002` or `000000000400BF028002`), not a
synthetic probe-presence heuristic or a random firmware-payload match.

### Physical disconnect/reconnect event reconstruction

Times below are relative to the trace's first IOCTL, using the known
10-MHz capture tick conversion. There are four event groups consistent
with the user's four physical unplug/replug cycles. The user described
several cycles but did not separately timestamp the actions.

| Apparent action | t (s) | Status seq / pending | 0x88 seq / ack | 0x82 seq / actual decoded raw reply tail |
|---|---:|---|---|---|
| First removal | 27.557 | 16747 / 0200 | 16748 / 0200 | 16754: raw data length 0x000E, last status WORD **0x03FF** |
| First insertion | 31.662 | 19245 / 0200 | 19246 / 0200 | 19248: **0x028C** |
| Additional immediate event | 31.718 | 19249 / **0280** | 19250 / **0280** | 19257: **0x0058** (intervening standalone pending 0 and ack 0 at seq 19253/19255) |
| Second removal | 40.564 | 25308 / 0200 | 25309 / 0200 | 25318: **0x03FF** |
| Second insertion | 42.336 | 26308 / 0200 | 26309 / 0200 | 26318: **0x00A9** |
| Additional immediate event | 42.386 | 26320 / 0200 | 26321 / 0200 | 26325: **0x0058** |
| Third removal | 44.929 | 27775 / 0200 | 27776 / 0200 | 27782: **0x03FF** |
| Third insertion | 47.306 | 29173 / 0200 | 29174 / 0200 | 29183: **0x00AA** |
| Additional immediate event | 47.367 | 29187 / 0200 | 29188 / 0200 | 29193: **0x0058** |
| Fourth removal | 49.510 | 30380 / 0200 | 30381 / 0200 | 30390: **0x03FF** |
| Fourth insertion | 52.275 | 32018 / 0200 | 32019 / 0200 | 32021: **0x00A9** |
| Additional immediate event | 52.405 | 32062 / 0200 | 32063 / 0200 | 32065: **0x0058** |

`0x03FF` is consistently seen after a physical removal and is an
empirical sentinel correlation; the exact meaning of each subsequent
state word (`0x028C`, `0x00A9`, `0x00AA`, `0x0058`) has not
yet been statically decoded. Do not call `0x028C` a proved literal
"1/2 clamp" firmware identifier.

**First-insertion anomaly:** In the first reinsertion event, the board
returns `0x028C`, an immediate combined pending `0x0280`, then
`0x0058`; there is **no post-event 270-byte AP015 metadata request**
before the next removal. This differentiates that transition from
the next three reinsertions and is consistent with the user's transient
wrong-probe-type display. Whether connector insertion or firmware
settling caused it is unknown. At each of the other three reinsertions,
XStream executes the normal family-0/0x4A setup and family-1/0x4A
270-byte AP015 metadata query:

```text
startup:        family-1/0x4A seq   509  t~13.881 s
reinsertion 2:  family-0/0x4A seq 26342 -> family-1/0x4A seq 26343  t~42.572 s
reinsertion 3:  family-0/0x4A seq 29207 -> family-1/0x4A seq 29208  t~47.553 s
reinsertion 4:  family-0/0x4A seq 32067 -> family-1/0x4A seq 32068  t~52.531 s
```

All four report `Information=270`, carry literal ASCII `AP015`,
and have **byte-identical captured 128-byte metadata prefixes**.
The recorder does not contain the remaining 142 output bytes.

### Post-formatter 0x82 and 47 12 parity now exercised

This is the first real-hardware observation of family-1/0x82 **after**
the narrow original-`FUN_000167F4` response-format fix `3490709`.
All twelve 0x82 outputs now put the **actual raw firmware data length**
into the second-result header and preserve **0xFF padding** in all
recorded unused bytes. The first removal seq 16754 reports raw length
`0x000E` (14 bytes) and data
`00001A000500190005001200FF03`. The other eleven 0x82 responses
report raw length **0x0006** and their actual six bytes, e.g.
`00001200A900` or `000012005800`, followed by FF padding.
This confirms the raw 85FB serializer fix beyond 47 00 and startup
family-1/0x99. It supersedes the earlier "0x82 untested post-fix"
qualification in preceding historical sections.

Following the three successful reidentifications, XStream issues
family-0/0x4A `...47 12` exactly once each at seq
**26385, 29250, 32096**, each with the complete output
`0000000000000000000002000000FFFF`. This is identical to
the matching original x86 result and differs from the old fivefold
pre-fix bursts with `...040000000000`. The respective family-1/
0x4A `01 0A` statuses at seq 26386, 29251, 32099 still return
`0000000000000000000004000000F700`; the status-bit meaning
and physical Auto Zero result remain unknown. These **event-associated**
47 12 commands must not be confused with the separately user-triggered
**manual** Auto Zero, which appeared as 47 00 in trace 233125/235314.

### Revised engineering conclusion

The earlier **hypothesis of a global INTST 0x08 / HWInt 0x410 regression
after host formatter commit 3490709 is falsified for physical hotplug**:
the same driver successfully reports 12 genuine pending-0x0200-bearing
notifications, normal matching 0x88 acks and twelve 0x82 replies,
plus three post-reinsertion AP015 identifications. The hardware command
interrupt path is not globally disabled by 0xFF padding or the corrected
raw-response length. The absence of such notifications during purely
mechanical jaw movements in traces `233125` and `235314` remains
**separate and unexplained**. It may be related to probe state, event
generation, timing, or operation-specific handling; no specific cause
is yet proved, and do not reverse a working host formatter solely on
temporal association.

All 6,135 captured CFDC2138 transfers have success and return exactly
their requested size. DMA was observed throughout each unplug/replug
interval and after the last reinsertion (over 1,000 additional records);
the last DMA is still successful at trace end. This does **not** by
itself prove screen-visible waveform quality. There is no CPU or raw
ISR/DPC rate in this IOCTL-only capture; <=5% CPU is still the
operating constraint.

For a further jaw-specific discriminator, use the same unchanged
driver, with AP015 stably identified, and compare **one** controlled
jaw open/hold/close/hold pair with **one** pair after a single
successful physical unplug/replug, without Degauss or Auto Zero.
Record XStream visible jaw/probe state and approximate elapsed action
times. Check genuine 0x0200/0x88/0x82 from jaw movements separately
from the known-good physical hotplug sequence. Do not synthesize
pending bits, force 0x82 state words or change PCI/DMA/interrupt masks
without specific measured evidence.
