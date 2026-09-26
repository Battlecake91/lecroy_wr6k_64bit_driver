# Runtime trace capture

The native x64 driver contains a non-invasive IOCTL trace ring intended for
capturing the exact startup/runtime protocol emitted by the original 32-bit
XStream software.

The trace path is deliberately observational. It does not enable partially
understood `0xCFDC2110` hardware execution.

## Recommended capture

Build `tools/lecdiag` first, then run from the repository root:

```powershell
.\scripts\capture-xstream-trace.ps1
```

The default capture duration is 90 seconds. A different duration can be used:

```powershell
.\scripts\capture-xstream-trace.ps1 -DurationSeconds 180
```

The script writes a timestamped file under the ignored local directory:

```text
trace-captures/xstream_trace_YYYYMMDD_HHMMSS.jsonl
```

Start XStream after the capture command begins.

## JSONL format

The first line is capture metadata. Every following line is one IOCTL record.

Example shape:

```json
{"type":"ioctl","seq":12,"timestamp_ticks":123456789,"pid":1234,"wow64":1,"method":0,"method_name":"BUF","ioctl":"0xCFDC2110","name":"CFDC2110","input_length":24,"output_length":8,"information":0,"status":"0xC0000010","type3_input_buffer":"0x0000000000000000","user_buffer":"0x0000000000000000","input_hex":"...","output_hex":""}
```

Fields:

- `seq`: monotonically increasing trace sequence after the trace is cleared;
- `timestamp_ticks`: monotonic kernel performance-counter ticks;
- `pid`: caller process ID;
- `wow64`: whether the request came from a 32-bit process;
- `method`: IOCTL transfer method;
- `ioctl`: numeric IOCTL code;
- `name`: known symbolic name when available;
- `input_length` / `output_length`: requested buffer lengths;
- `information`: completed `IoStatus.Information`;
- `status`: completed NTSTATUS;
- `type3_input_buffer` and `user_buffer`: pointer values for METHOD_NEITHER
  correlation only;
- `input_hex`: up to 256 captured input bytes;
- `output_hex`: up to 128 captured METHOD_BUFFERED output bytes.

For METHOD_NEITHER requests the tracer probes and copies only the bounded input
preview. A probe failure is logged and leaves `input_hex` empty. The tracer
does not dereference arbitrary output user buffers.

## Live capture versus snapshot

`lecdiag trace-save file.jsonl` writes the current in-kernel ring once.

`lecdiag trace-capture file.jsonl 90` first clears the ring and then polls it
every 250 ms, appending newly observed sequence numbers to the JSONL file. This
is preferred for XStream because it avoids losing early startup traffic when
the ring wraps.

The kernel ring currently stores 256 records. Successful high-frequency generic
register reads are intentionally omitted because they would otherwise evict the
control protocol that is relevant to reverse engineering.

## Handling

Runtime captures are ignored by Git by default because they contain process
IDs and pointer values. Share a selected trace explicitly when analysis is
needed; do not commit raw captures to the public repository by accident.


## Trace format version 3

The timestamp field is now `timestamp_ticks` and comes from
`KeQueryPerformanceCounter`. This avoids relying on a kernel time-query export
that was not available in the current WDK/link environment. The value is
monotonic and is intended for ordering and relative timing inside a capture.


## First real XStream startup capture

A 120-second startup capture from the reference scope was collected after the
legacy START/ITMODE initialization had restored normal MMIO access.

The capture contains 35 traced IOCTLs from a 32-bit XStream process. Relevant
counts are:

- 11 x `0xCFDC2110`
- 9 x `0x00222C00` delay
- 3 x `0xCFDC21C4` register write
- 3 x `0x00222C04` flag-byte set
- 2 x `0x00223100` three-event registration
- one Dallas ID read and one 512-byte Dallas memory read

Because `0xCFDC2110` is intentionally trace-only in the current x64 driver,
all 11 calls complete with `STATUS_INVALID_DEVICE_REQUEST (0xC0000010)`.
Despite that, the capture is valuable because it shows the exact command stream
XStream attempts during startup.

Only four distinct `0xCFDC2110` packet shapes occur:

1. one A5FB family-2/opcode-0x40 RESET packet;
2. two A5FB family-1/opcode-0x99 requests followed by 85FB fetch records;
3. six A5FB family-0/opcode-0x88 requests with mask `0xFFDF`, each followed by
   an 85FB fetch record;
4. two A5FB family-1/opcode-0x42 JTAG requests, each followed by an 85FB fetch
   record.

No C5FB record appears in this startup capture.

A sanitized public fixture containing only these `0xCFDC2110` records is kept
at:

`testdata/traces/xstream_startup_20260926_cfcd2110_sanitized.jsonl`

The raw capture remains local/private because it also contains process IDs,
handles, pointers and device-specific Dallas contents.


The next runtime test uses the whitelisted four byte-exact startup request
forms only. The expected evidence is whether XStream advances beyond the
previous `STATUS_INVALID_DEVICE_REQUEST` startup barrier and what new
`0xCFDC2110` shapes, if any, appear afterward.


## Second whitelisted startup capture

A second 120-second XStream startup capture was taken with the byte-exact
startup whitelist enabled.

Results:

- 47 IOCTL entries were captured.
- 15 calls used `0xCFDC2110`.
- Four whitelisted requests completed successfully:
  - RESET;
  - first family-1 opcode `0x99`;
  - first family-0 opcode `0x88` with mask `0xFFDF`;
  - second family-1 opcode `0x99`.
- Eleven `0xCFDC2110` calls were still rejected with
  `STATUS_INVALID_DEVICE_REQUEST`.

The newly exposed request is family-0 opcode `0x85`:

```text
06 00 06 00 03 00 FB A5
40 00 85 00 A0 00
08 00 02 00 03 00 FB 85 40 00
```

It appears five times before the later JTAG stage.

The six later JTAG calls were also rejected, but review of the whitelist found
a transcription error in the captured 94-byte JTAG packet. That whitelist entry
has been corrected; the JTAG implementation itself was not the cause of those
rejections.

Opcode `0x85` is not enabled yet. Static analysis shows that it performs
additional host-side state changes in `FUN_00016962` before forwarding the
packet to the board. The two remaining helper calls `0x1573E` and `0x15772`
must be resolved before widening the runtime gate.


## Third staged startup capture

The next 120-second trace contains 109 IOCTL entries, including 62
`0xCFDC2110` calls.

Results:

- 21 CFDC2110 calls completed successfully.
- 41 were rejected by the byte-exact safety gate.
- The startup sequence now passes RESET, opcode `0x99`, opcode `0x88`,
  opcode `0x85`, and the first 256-bit family-1 opcode-`0x42` JTAG form.
- XStream repeats the startup sequence three times after later rejected
  requests, matching the observed startup exceptions/errors.

Three additional exact request shapes were exposed:

1. family-1 opcode `0x42`, mode 0, 83-bit JTAG transaction, 12-byte response;
2. family-1 opcode `0x90`, generic board-message transmit plus 85FB fetch;
3. family-1 opcode `0x42`, mode 1, 58-bit JTAG transaction, 8-byte response.

The short mode-0 JTAG request is retried heavily: 33 rejected calls in the
trace. This is consistent with XStream repeatedly attempting the same startup
stage rather than progressing to acquisition.

The exact three newly observed packet buffers are now admitted by the runtime
gate. Opcode `0x90` uses the already-decoded family-1 generic transport path;
both opcode-`0x42` forms use the existing JTAG implementation.


## Fourth staged startup capture

The 2026-09-26 23:45 capture contains 46 IOCTL entries and 21
`0xCFDC2110` calls.

Results:

- 6 CFDC2110 calls completed successfully after the previous whitelist update:
  RESET, `0x99`, `0x88`, `0x85`, the 256-bit JTAG form, `0x90`, and
  both mode-1 58-bit JTAG attempts all reached their implemented paths as
  expected.
- 15 calls were still rejected, and every rejected call is the same
  family-1 opcode-`0x42`, mode-0, 83-bit JTAG request.
- No new opcode family appears in this trace.

Review showed that the previous whitelist entry for this 83-bit JTAG request
was still transcribed incorrectly: the real XStream input is 54 bytes, while
the hand-entered array had only 52 bytes and misplaced two zero bytes before
the `C0 00 00 07` tail. The runtime gate therefore correctly rejected it.

The byte-exact array has now been replaced directly from the captured 54-byte
input. No JTAG semantics were changed.

The capture header still reported `format_version: 2`; the remaining capture
header literal in `lecdiag` has also been corrected to version 3.
