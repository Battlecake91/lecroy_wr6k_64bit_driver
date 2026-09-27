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


## Fifth staged startup capture

The 2026-09-26 23:50 capture contains 38 IOCTL entries and 21
`0xCFDC2110` calls.

Results:

- 10 CFDC2110 calls completed successfully.
- 11 calls were rejected by the byte-exact safety gate.
- The corrected family-1 opcode-`0x42`, mode-0, 83-bit JTAG request now
  completes successfully, confirming that the previous failure was only the
  whitelist transcription error.
- The newly exposed request is a family-1 opcode-`0x42`, mode-1, 256-bit JTAG
  transaction. It is byte-for-byte identical to the already admitted 256-bit
  mode-0 request except for the JTAG mode byte changing from `0x00` to
  `0x01`.
- That new 94-byte request was rejected 11 times. No other new command shape
  appears in this capture.
- After those rejections, XStream still reaches the already admitted
  family-1 opcode-`0x90` transport request and the known mode-1 58-bit JTAG
  request.

The existing `LecJtagExecute` implementation already supports both JTAG
modes. Mode 1 sets bit `0x100` in BAR1 `JTAGNUM`; the data path and bit
count handling are otherwise shared with mode 0. Because mode-1 operation was
already exercised by the admitted 58-bit startup transaction, the newly
observed 256-bit form is admitted as one additional byte-exact captured packet.
No broader opcode or mode whitelist has been introduced.

The exact newly admitted request is:

```text
06 00 4C 00 03 00 FB A5
40 01 42 01 00 00 20 00
00 01 00 00 FF 0B 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00
28 00 02 00 03 00 FB 85 40 00
```

The next hardware run should therefore capture what XStream attempts after this
mode-1 256-bit JTAG stage. Trigger and DMA behavior remain unchanged.


## Sixth staged startup capture

The 2026-09-27 00:49 capture contains 40 IOCTL entries and 23
`0xCFDC2110` calls.

Results:

- 12 CFDC2110 calls completed successfully.
- 11 calls were rejected with `STATUS_INVALID_DEVICE_REQUEST` by the
  byte-exact safety gate.
- The admitted 94-byte family-1 opcode-`0x42`, mode-1, 256-bit JTAG request
  now completes successfully, so startup progressed past the previous gate.
- The first newly rejected request is a 54-byte family-1 opcode-`0x42`,
  mode-1, 83-bit JTAG transaction requesting 12 response bytes. All 11
  CFDC2110 rejections are this same exact buffer.
- XStream subsequently still reaches the admitted opcode-`0x90` transport
  request and two admitted mode-1, 58-bit JTAG requests. No acquisition-launch
  IOCTL appears in the capture.

The exact newly observed request is:

```text
06 00 24 00 03 00 FB A5
40 01 42 01 00 00 0C 00
53 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 C0 00 00
07 00 00 00
14 00 02 00 03 00 FB 85 40 00
```

It differs from the already admitted mode-0, 83-bit buffer at exactly byte
offset 11, where the JTAG mode changes from `0x00` to `0x01`.
`LecJtagExecute` accepts both modes and only adds bit `0x100` to BAR1
`JTAGNUM` for mode 1; bit-count, data-word and response handling are shared.
Mode 1 has already executed successfully in the admitted 58-bit and 256-bit
transactions. These facts support proposing this one complete buffer for a
future byte-exact admission, but this analysis round does not modify the gate.
Unknown packets therefore continue to be rejected before hardware side
effects. Trigger and DMA behavior remain unchanged.


## Seventh staged startup capture

The 2026-09-27 00:59 capture contains 46 IOCTL entries and 23
`0xCFDC2110` calls.

Results:

- 12 CFDC2110 calls completed successfully.
- 11 calls were rejected with `STATUS_INVALID_DEVICE_REQUEST` by the
  byte-exact safety gate.
- The newly admitted 54-byte family-1 opcode-`0x42`, mode-1, 83-bit JTAG
  request completed successfully, proving the exact new gate entry on hardware.
- The first and only newly rejected request shape is a 94-byte family-1
  opcode-`0x42`, mode-2, 256-bit form. It repeats 11 times.
- XStream subsequently still reaches the admitted opcode-`0x90` transport
  request and two mode-1, 58-bit JTAG requests. No acquisition-launch IOCTL
  appears in the capture.

The exact newly rejected request is:

```text
06 00 4C 00 03 00 FB A5
40 01 42 02 00 00 20 00
00 01 00 00 FF 0B 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00
28 00 02 00 03 00 FB 85 40 00
```

It differs from the admitted mode-1, 256-bit request only at byte offset 11,
where the mode changes from `0x01` to `0x02`. This is not another already
implemented JTAG hardware mode. Legacy `FUN_00015C7E` accepts only modes 0 and
1 for register execution; for mode 2 it returns protocol status 4 and prepares
a local response without touching JTAG registers. The exact pending-response
and 85FB fetch behavior must be reproduced before this complete buffer can be
considered for admission. It therefore remains rejected, with trigger and DMA
paths unchanged.


## Mode-2 response implementation

Static analysis is now sufficient to reproduce the mode-2 request without
hardware speculation. `FUN_00010380` uses non-zeroing
`ExAllocatePoolWithTag`, so the legacy 40-byte pending response contains a
defined 8-byte header followed by 32 undefined pool bytes.

The replacement driver intentionally normalizes that undefined tail to zero.
The exact captured 94-byte family-1 opcode-`0x42`, mode-2, 256-bit request is
now admitted. It produces local protocol status 4 and a 40-byte pending
response; it performs no JTAG MMIO. The next scope capture should determine
what XStream attempts after this local-error stage.

## Eighth staged startup capture

The 2026-09-27 01:35 capture contains 93 IOCTL entries and 71
`0xCFDC2110` calls.

Results:

- 60 CFDC2110 calls completed successfully.
- 11 calls were rejected with `STATUS_INVALID_DEVICE_REQUEST`.
- The exact mode-2 / 256-bit local-response path now completes successfully.
- XStream then issues the same 94-byte family-1 opcode-`0x42` shape with
  mode values 3 and 4. Each appears 16 times and completes successfully.
- Modes 2, 3 and 4 all return the same deterministic local response shape:
  outer command status 4 followed by the 40-byte pending response whose defined
  header is `00 00 00 00 02 00 04 00` and whose replacement-driver tail is
  zero-filled.
- The first rejected request is the same 94-byte packet with mode 5. It repeats
  11 times.
- XStream subsequently still reaches the admitted family-1 opcode-`0x90`
  request and the known mode-1 / 58-bit JTAG request.
- No acquisition-launch IOCTL appears in this capture.

The apparent runtime/source discrepancy was subsequently resolved. The scope
checkout was still at commit
`f119e4df0df5a1906016ec7a1a56fcb8a78a2705` with local uncommitted
`driver/Ioctl.c` changes. Those local changes already contained exact
mode-2, mode-3 and mode-4 long-packet whitelist entries and a local
`mode > 1` status-4 response implementation. The SHA-256 of the signed
package SYS matched the installed
`C:\Windows\System32\drivers\LecS65AcqDrv.sys`, proving the trace came
from that dirty local build.

The exact observed mode-3 and mode-4 packets are now carried on `main` as
byte-exact entries. They remain local protocol-error paths and do not touch
JTAG registers. Mode 5 is the next rejected packet and remains blocked.

## Ninth staged startup capture

The 2026-09-27 01:53 capture was taken from a clean checkout at
`9b8cf11555b88ee49d65ab94fd08f5c1a4fb4ca6`. The freshly signed package SYS
and the installed `System32\drivers\LecS65AcqDrv.sys` had identical SHA-256
`085AF5586FEA3D56215B4FA6FFE3AC2CE57F7B74CE63FDB1C052248382493CF4`.

The capture contains 92 IOCTL records, including 71 `0xCFDC2110` calls:

- 60 CFDC2110 calls completed successfully;
- 11 were rejected by the byte-exact gate;
- mode-2, mode-3 and mode-4 256-bit local-error packets each completed 16 times;
- all three returned the deterministic 40-byte status-4 pending-response form;
- the first and only rejected CFDC2110 shape is the corresponding mode-5
  94-byte packet, repeated 11 times;
- XStream then still reaches the admitted opcode-`0x90` request and the known
  mode-1 / 58-bit JTAG requests;
- no acquisition-launch IOCTL appears.

Because legacy `FUN_00015C7E` treats every mode value above 1 as the same
local protocol-error path without JTAG MMIO, and this exact mode-5 request is
now captured, the byte-exact runtime gate adds only this complete 94-byte
packet. No general `mode >= 2` admission rule is introduced.

## Tenth staged startup capture

The 2026-09-27 01:57 capture contains 176 IOCTL records, including 155
`0xCFDC2110` calls.

Results:

- 144 CFDC2110 calls completed successfully.
- 11 were rejected by the byte-exact gate.
- The newly admitted mode-5 / 256-bit local-error packet now completes
  successfully.
- XStream then leaves the mode-count sequence and issues family-1 opcode
  `0x81`.
- Three exact 22-byte opcode-`0x81` forms were observed:
  - selector byte `0x00`: 5 rejected calls;
  - selector byte `0x01`: 5 rejected calls;
  - selector byte `0x02`: 1 rejected call.
- Each form contains a following 85FB fetch record requesting a 10-byte
  record output.
- After the rejected `0x81` block, XStream still reaches the admitted
  family-1 opcode-`0x90` request and the known mode-1 / 58-bit JTAG requests.
- No acquisition-launch IOCTL appears.

Legacy `FUN_000165A6` routes family-1 opcode `0x81` through the same generic
board-message transmitter used by the already exercised `0x90` and `0x99`
forms. No separate host-side register or state update is performed by the
legacy dispatcher. The x64 driver therefore adds only the three exact captured
22-byte packet forms to the runtime gate and routes opcode `0x81` through the
existing generic transport path. Board-firmware semantics remain unknown and no
opcode-wide admission rule is introduced.

## Eleventh staged startup capture

The 2026-09-27 02:01 capture contains 180 IOCTL records, including 158
`0xCFDC2110` calls:

- 147 CFDC2110 calls completed successfully;
- 11 were rejected;
- the previously admitted family-1 opcode-`0x81` selector forms
  `0x00`, `0x01`, and `0x02` each complete successfully;
- the only rejected shapes are the same 22-byte opcode-`0x81` packet with
  selector bytes `0x03`, `0x04`, and `0x05`;
- rejection counts are 5, 5, and 1 respectively.

This strongly indicates that XStream is walking a deterministic opcode-`0x81`
startup sequence. Legacy routing for family-1 opcode `0x81` is already known
to use the generic BAR1 board-message transport without extra host-side
register handling. The replacement therefore adds only these three newly
captured exact buffers to the existing byte-exact gate.

The long-standing XStreamDSO startup error that appears at the same point on
every run is likely related to this incomplete startup sequence. The trace
alone cannot prove that the dialog is caused by these specific rejections, but
the repeatable timing and the progression to the next blocked packet after each
admission make the connection operationally significant.

## Twelfth staged startup capture

The 2026-09-27 02:09 capture shows the next family-1 opcode-`0x81`
selector forms after selectors 3, 4 and 5 were admitted.

Observed newly rejected exact packets use selector bytes `0x06`, `0x07`
and `0x08`. Their structure is unchanged: each is a 22-byte A5FB command
followed by an 85FB fetch record requesting 10 output bytes.

The application was previously allowed to continue past its reproducible
startup error dialog, which introduces retry/recovery traffic after the actual
startup failure point. Future captures should terminate XStream from that
dialog instead. Analysis should treat the sequence up to the first rejected
startup block as authoritative and anything after a manually skipped dialog as
recovery-path traffic.

Selectors 6, 7 and 8 are now added as exact captured runtime-gate entries.
Legacy family-1 opcode `0x81` still uses the already decoded generic board
transport; no broader opcode rule is introduced.

## Thirteenth staged startup capture

The 2026-09-27 02:25 capture was terminated from the reproducible XStreamDSO
startup error dialog instead of continuing past it. This produces a clean stop
at the actual startup failure point with no recovery-path traffic afterwards.

The trace contains 172 IOCTL records, including 161 `0xCFDC2110` calls:

- 150 CFDC2110 calls completed successfully;
- 11 were rejected by the byte-exact gate;
- every rejected call is family-1 opcode `0x81`;
- selector `0x09`: 5 rejections;
- selector `0x0A`: 5 rejections;
- selector `0x0B`: 1 rejection;
- no later recovery traffic appears because XStream was terminated at the
  dialog.

This confirms that terminating at the dialog yields the authoritative normal
startup boundary. The exact selector-9, selector-10, and selector-11 packet
forms are now added to the byte-exact runtime gate and continue to use the
already decoded generic BAR1 board-message transport.

## Fourteenth staged startup capture

The 2026-09-27 02:29 capture was again terminated at the XStreamDSO startup
error dialog. The final rejection block is therefore the authoritative normal
startup boundary.

The trace contains 175 IOCTL records. The last 11 rejected `0xCFDC2110`
calls are all family-1 opcode `0x81`:

- selector `0x0C`: 5 rejections;
- selector `0x0D`: 5 rejections;
- selector `0x17`: 1 rejection.

The jump from selector `0x0D` to `0x17` shows that the selector byte is not
simply a monotonically increasing counter. It is more likely an index or
selection value from a fixed startup list. Legacy still routes all observed
family-1 opcode-`0x81` forms through the generic BAR1 board-message transport.

The exact observed selector-12, selector-13, and selector-23 packets are now
added to the byte-exact runtime gate. Unknown selector values remain rejected.

## Fifteenth staged startup capture

The 2026-09-27 02:31 capture was terminated at the XStreamDSO startup error
dialog and therefore ends at the authoritative startup failure boundary.

The trace contains 181 IOCTL records, including 170 `0xCFDC2110` calls. The
final 11 rejected calls are all family-0 opcode `0x84` and occur as three
exact 54-byte packet forms:

- form 0: 5 rejections;
- form 1: 5 rejections;
- form 2: 1 rejection.

Legacy family-0 dispatcher `FUN_00016A66` routes opcode `0x84` directly to
the generic BAR1 board-message transmitter `FUN_00016168` without additional
host-side register or state handling. This is the same generic transport class
already used for several previously admitted firmware-forwarded startup
commands.

The replacement driver therefore adds only the three exact captured opcode
`0x84` packet buffers to the byte-exact runtime gate and forwards them through
the existing generic transport path. Their board-firmware meaning remains
unknown.

## Sixteenth staged startup capture

The 2026-09-27 02:34 capture was terminated at the XStreamDSO startup error
dialog and ends cleanly at the next blocked startup stage.

The trace contains 200 IOCTL records, including 188 `0xCFDC2110` calls:

- 177 CFDC2110 calls completed successfully;
- 11 were rejected by the byte-exact gate;
- every rejection is family-0 opcode `0x84`;
- selector `0x03`: 5 rejections;
- selector `0x04`: 5 rejections;
- selector `0x05`: 1 rejection.

These packets use the same 54-byte opcode-`0x84` structure as the already
admitted forms, with the selector byte changed and the payload matching the
captured runtime values. Legacy `FUN_00016A66` still routes opcode `0x84`
through the generic BAR1 board-message transport without additional host-side
state changes.

The exact selector-3, selector-4, and selector-5 packet buffers are now added
to the byte-exact runtime gate. Unknown opcode-`0x84` forms remain rejected.

## Seventeenth staged startup capture

The 2026-09-27 02:35 capture was terminated at the XStreamDSO startup error
dialog and ends cleanly at the next blocked startup stage.

The trace contains 218 records, including 206 `0xCFDC2110` calls:

- 195 CFDC2110 calls completed successfully;
- 11 were rejected by the byte-exact gate;
- every rejection is family-0 opcode `0x84`;
- selector `0x06`: 5 rejections;
- selector `0x07`: 5 rejections;
- selector `0x08`: 1 rejection.

The complete packet payload matters, not only the selector byte. Selector
`0x06` carries payload words `0x0036` and `0x0069`, while selectors
`0x07` and `0x08` carry `0x00A5` and `0x00D5`. The remaining repeated
`0x03FF` words and the trailing 85FB fetch shape match the captured startup
pattern.

Because legacy family-0 opcode `0x84` is already known to use the generic
BAR1 board-message transport without extra host-side state handling, the x64
driver adds only these three exact 54-byte buffers. No selector-only or
opcode-wide admission rule is introduced.

## Eighteenth staged startup capture

The 2026-09-27 02:37 capture was terminated at the XStreamDSO startup error
dialog and ends cleanly at the next blocked startup stage.

The trace contains 235 IOCTL records, including 224 `0xCFDC2110` calls:

- 213 CFDC2110 calls completed successfully;
- 11 were rejected by the byte-exact gate;
- every rejection is family-0 opcode `0x84`;
- selector `0x09`: 5 rejections, payload words `0x00A5`, `0x00D5`;
- selector `0x0A`: 5 rejections, payload words `0x0043`, `0x0075`;
- selector `0x0B`: 1 rejection, payload words `0x0000`, `0x0000`.

As in the preceding opcode-`0x84` stage, the complete packet contents matter:
the payload changes along with the selector. The x64 driver therefore admits
only these three exact 54-byte packet buffers through the already decoded
generic BAR1 board-message transport.

## Semantic gate for confirmed forward-only commands

The runtime gate no longer enumerates every captured payload for command
classes that the legacy driver is statically confirmed to forward unchanged to
the board firmware.

The following CFDC2110 A5FB command classes are now admitted after structural
record validation:

- family 0, opcode `0x84`;
- family 1, opcode `0x81`;
- family 1, opcode `0x90`;
- family 1, opcode `0x99`.

The validator walks the complete packed record list and requires valid type-3
record framing, in-bounds `8 + payload_length` spans, A5FB command payloads
beginning with `0x40`, and only the statically confirmed family/opcode pairs.
A companion 85FB fetch record is accepted only with the expected `40 00`
payload prefix. At least one confirmed forward-only A5FB command must be
present.

This replaces the accumulated byte-exact packet arrays for opcode `0x81` and
`0x84`. Commands with decoded host-side side effects, such as family-0 opcode
`0x85`, remain on the stricter path.

The final byte-exact trace before this change,
`xstream_trace_20260927_023951.jsonl`, contained 252 IOCTL records and 241
CFDC2110 calls. 230 CFDC2110 calls succeeded. The final 11 blocked requests
were opcode-`0x84` selector `0x0C` five times, selector `0x0D` five
times, and family-1 opcode-`0x81` selector `0x0E` once. These requests are
now covered by the semantic forward-only gate without adding more per-packet
whitelist entries.