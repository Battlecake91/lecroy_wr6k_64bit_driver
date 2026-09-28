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
