# 2026-09-28: calibration and ProBus A/B comparison

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
