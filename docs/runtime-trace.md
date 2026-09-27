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

## Nineteenth staged startup capture

Trace `xstream_trace_20260927_025005.jsonl` is the first capture after
replacing byte-exact admission for the confirmed pure forwarding classes with
semantic structural validation.

It contains 439 trace records, including 427 CFDC2110 calls:

- 416 CFDC2110 calls succeeded;
- 11 were rejected;
- no rejection is an opcode-`0x81` or opcode-`0x84` selector variant.

The final blocked stage is instead:

- family 0 / opcode `0x42`: 5 identical requests;
- family 0 / opcode `0x92`, selector 1, offset `0x00C0`, value
  `0xFFFFFFFF`: 5 requests;
- family 0 / opcode `0x92`, selector 1, offset `0x00C4`, value
  `0x00000460`: 1 request.

Static analysis confirms that these are not generic forwarders.
Legacy `FUN_00015ACC` implements opcode `0x42` as direct BAR1 JTAGNUM/JTAGDAT
writes and returns a local status response. Legacy `FUN_0001600E` implements
opcode `0x92` as a selector-based direct MMIO write. Selector 1 is confirmed
as BAR1 by the observed `0xC0`/`0xC4` GPIO register offsets.

The x64 driver now implements the decoded opcode-`0x42` JTAG-write semantics
and opcode-`0x92` selector-1 BAR1 writes structurally rather than admitting
only the captured packets.

## Twentieth staged startup capture

Trace `xstream_trace_20260927_025600.jsonl` contains 432 IOCTL records and
421 CFDC2110 calls.

- 420 CFDC2110 calls succeeded;
- exactly one CFDC2110 call was rejected;
- the rejected request is family 2 / opcode `0x02` with command-body value
  `0x00`.

This confirms that the newly implemented family-0 opcode-`0x42` JTAG write
path and opcode-`0x92` selector-1 BAR1 MMIO write path both progress through
startup successfully.

Static analysis of legacy `FUN_000163B2` shows that family-2 opcode `0x02`
is local MTT control: accept body value 0 or 1, optionally wait for an active
transfer object, write that value to BAR1 MTTCTL at offset `0x80`, and
publish a local status response.

The x64 driver now implements this command structurally for both valid values
rather than admitting only the observed zero-valued request.

## Twenty-first staged startup capture

Trace `xstream_trace_20260927_025927.jsonl` is the first capture where
XStream reaches its main UI without the former startup error dialog.

Observed application-level failures now split into two distinct groups:

1. **Probe read failure for channels 1 through 5**
   - five rejected family-0 opcode-`0x90` requests appear as one contiguous
     block;
   - each request carries selector `0x0E` and a 144-bit data sequence;
   - legacy `FUN_00015E80` handles this command locally through the helper
     object at driver-object offset `+0x19`, not through the generic board
     transport;
   - the helper chain `FUN_000155D0`, `FUN_0001236E`,
     `FUN_0001588E` programs a control-shadow register and a data register.
     The exact BAR1 register mapping of that helper object still needs to be
     proven before enabling the path.

2. **Trigger/arm failure**
   - family-0 opcode `0xA0` is rejected repeatedly while trigger-related
     GPIO/JTAG commands around it now succeed;
   - legacy `FUN_00015FD8` performs a local 16-bit register write through an
     internal register object at driver-object offset `+0x17A`;
   - the precise MMIO target represented by that object is not yet mapped;
   - the final rejected family-0 opcode `0x85` carries control word
     `0x02A0`. Its host semantics were already decoded: update the legacy
     interrupt-enable shadow bits and then forward the request to board
     firmware. The x64 gate has now been generalized for structurally valid
     opcode-`0x85` requests instead of admitting only the earlier
     `0x00A0` capture.

No `CFDC2138` or `CFDD219F` acquisition launch IOCTL occurs in this trace.
Therefore the current "unable to arm acquisition board" message happens before
the still-gated DMA acquisition path is entered.

## Probe SPI path resolved after constructor export

The follow-up Ghidra export after trace
`xstream_trace_20260927_025927.jsonl` resolves the previously ambiguous
family-0 opcode-`0x90` helper mapping.

The constructor chain proves that dispatcher field `+0x19` points at the SPI
helper initialized from BAR1 registers `SPICTL 0xA0`, `SPIDAT 0xA4`, and
`SPIDIN 0xA8`.

The five rejected probe requests in the trace all use the same structurally
valid opcode-`0x90` form:

- selector `0x0E`;
- bit count 144;
- nine 16-bit payload words.

This matches legacy `FUN_00015E80` exactly. The x64 driver now implements the
decoded SPI-write/control sequence semantically instead of rejecting the
request.

Together with the already resolved opcode-`0xA0` -> PFREG mapping, the next
runtime test should show whether the application-visible probe errors disappear
and whether the trigger/arm sequence progresses beyond its previous failure
point.

## Twenty-second runtime capture: probe path passes, arm advances

Trace `xstream_trace_20260927_094813.jsonl` contains 485 IOCTL records and
460 CFDC2110 calls:

- 449 CFDC2110 calls succeeded;
- 11 were rejected.

The previously failing probe-related family-0 opcode-`0x90` path now
completes successfully. Six opcode-`0x90` requests are present and all six
succeed. The resolved family-0 opcode-`0xA0` PFREG path also executes five
times successfully.

The remaining rejected commands are:

- one family-1 opcode-`0x42`, mode 1, 58-bit JTAG request;
- ten family-1 opcode-`0x96` requests.

Legacy family-1 dispatcher `FUN_000165A6` routes opcode `0x96` directly
through the same generic board-message transport as the already-supported
forward-only family-1 commands. Static analysis also identifies family-1
opcodes `0x4A`, `0x82`, `0x91`, and `0x97` as the same pure-forwarding
class. The semantic gate now admits that complete confirmed class.

The rejected 58-bit opcode-`0x42` request differs from an already successful
58-bit request only in JTAG payload data. Legacy `FUN_00015C7E` reads the
required 4-byte JTAG chunks from the contiguous packed IOCTL record buffer and
does not constrain those reads to the current record's `payload_length`.
For this request, the final JTAG chunk overlaps bytes belonging to the
following packed record. The x64 implementation now preserves that legacy
behavior while bounding the read by the complete validated IOCTL input buffer,
so no out-of-buffer access is possible.

No acquisition launch IOCTL is reached before these final rejections.

## Twenty-third runtime capture: family-0 0x4A is the only blocker

Trace `xstream_trace_20260927_095816.jsonl` contains 559 IOCTL records and
535 CFDC2110 calls:

- 524 CFDC2110 calls succeeded;
- 11 were rejected;
- every rejected request is family 0 / opcode `0x4A`.

The rejected forms are selector 0 (five times), selector 1 (five times), and
selector 2 (once). No acquisition-launch IOCTL is reached before this point.

All previously implemented probe/arm support continues to execute
successfully, including family-0 opcode `0x90` SPI requests and opcode
`0xA0` PFREG writes.

Legacy family-0 dispatcher `FUN_00016A66` routes opcode `0x4A` directly to
`FUN_00016168(..., 1)`, the generic board-message transport. The same
statically confirmed pure-forwarding class is:

`0x4A, 0x84, 0x86, 0x87, 0x96, 0x97, 0xA1, 0xA2`.

The x64 semantic gate and execution path now admit that complete family-0
forward-only class rather than requiring individual captured packets.

## Twenty-fourth runtime capture: MAM configuration records reached

Trace `xstream_trace_20260927_100938.jsonl` contains 810 IOCTL records and
790 CFDC2110 calls:

- 779 CFDC2110 calls succeeded;
- 11 CFDC2110 calls were rejected;
- CFDC2138 / CFDD219F acquisition launch IOCTLs are still not reached.

The rejected requests are now:

- family 2 / opcode `0x10` once;
- family 2 / opcode `0x05` once;
- family 2 / opcode `0x01` three times;
- six CFDC2110 **type-1** records carrying 42-byte payloads.

The family-2 local commands decode as:

- opcode `0x10`: write a two-boolean encoded value to BAR1 LEDCTL (`0xE0`);
- opcode `0x05`: pulse BAR1 ITMODE `7 -> 3`;
- opcode `0x01`: legacy internal restartable timer/wait control, observed with
  1 ms and 100 ms delays.

The six type-1 records are the first non-type-3 CFDC2110 records reached in the
runtime sequence. Legacy `FUN_00015720` accepts record types 1 and 2 and
`FUN_00012F30` interprets their payload as an array of 16-bit values.

For the captured 42-byte type-1 records, the payload is 21 values. Legacy
clears BAR1 GPIODAT bit 16, writes each value through MAMDAT, then writes
MAMPGO with `(mode << 8) | count`. For type 1 / 21 values that is
`MAMPGO = 0x115`.

The x64 driver now implements structurally valid type-1/type-2 MAM
configuration records, family-2 opcode-`0x05`, and family-2 opcode-`0x10`.

Family-2 opcode-`0x01` remains gated pending export of the legacy timer
object implementation.

## Family-2 timer path resolved before next hardware run

The follow-up Ghidra export provides `FUN_000157C4`, the constructor for the
timer object referenced by family-2 opcode `0x01`.

The object contains a Windows notification `KTIMER` initialized by
`KeInitializeTimerEx`. Combined with the already exported
`FUN_0001621A`, this resolves the command as non-blocking restartable timer
control rather than a hardware register access.

The x64 driver now implements the observed body form (mode byte 1 plus a
32-bit millisecond duration), including the legacy behavior of polling the
timer, arming/re-arming only as needed, and normalizing zero to 1 ms.

During the same verification, the type-1/type-2 MAM path was corrected:
legacy writes `(entry_index << 16) | payload_word` to MAMDAT, not the raw
16-bit payload word alone. This correction was made before the next scope
hardware run.

## Twenty-fifth runtime capture: arm succeeds, no acquisition data

Trace `xstream_trace_20260927_104718.jsonl` corresponds to the first run where
XStream no longer reports that it cannot arm the acquisition board.

The capture contains 1050 IOCTL records. Of these, 1033 are CFDC2110 calls and
all 1033 complete successfully. The previously reconstructed probe, JTAG, SPI,
MAM, timer, PFREG and board-forwarding paths therefore no longer produce a
runtime rejection in this capture.

Only three IOCTLs fail:

- `0x00222400`: zero input, four-byte output, STATUS_INVALID_DEVICE_REQUEST;
- `0x00223004` (`QUERY_BUFFER_A`): zero input, four-byte output,
  STATUS_INVALID_DEVICE_REQUEST;
- `0x00223040` (`QUERY_BUFFER_B`): zero input, four-byte output,
  STATUS_INVALID_DEVICE_REQUEST.

The two buffer-query IOCTLs occur immediately after event registration during
initialization. No `CFDC2124` transfer registration, `CFDC2138` buffered
acquisition, or `CFDD219F` METHOD_NEITHER acquisition call appears later in
the trace.

This makes the missing four-byte query results the current leading blocker for
measurement data: XStream can configure and arm the board, but it never enters
the registered-buffer/acquisition execution path.

Additional Ghidra targets around the earlier DeviceControl dispatch tree and
the candidate handler functions were added to resolve the exact semantics of
`0x00222400`, `0x00223004`, and `0x00223040`.

## Twenty-sixth runtime capture: calibration passes, acquisition waits

Trace `xstream_trace_20260927_112352.jsonl` is the first capture where XStream
progresses past the visible "Calibrating" phase and then remains at
"Acquiring".

The capture contains 943 IOCTL records after the trace header. All 924
CFDC2110 calls succeed.

The two previously implemented size queries now behave exactly as intended for
their first four-byte calls:

- `0x00223004` returns `0x00000110`;
- `0x00223040` returns `0x00002CAE`.

XStream immediately follows each size query with a second call to the same
IOCTL using the returned size as the output length:

- `0x00223004`, output length 272;
- `0x00223040`, output length 11438.

Those second-stage full-buffer retrieval calls still fail with
`STATUS_INVALID_BUFFER_SIZE (0xC0000206)` in the x64 driver.

No `CFDC2124` transfer registration, `CFDC2138` buffered acquisition, or
`CFDD219F` METHOD_NEITHER acquisition call appears afterward. This makes the
missing full-buffer query responses the current leading blocker between
successful board calibration/arming and actual acquisition data.

Static analysis confirms the 272-byte first buffer is the legacy trace-control
descriptor block. The 11438-byte second buffer is the serialized register list
with 43 entries of 0x10A bytes each.

Additional Ghidra targets were added for the remaining serialization helper
functions needed to reconstruct these payloads byte-for-byte.

## Full query payloads implemented before next hardware capture

The second-stage failures seen in `xstream_trace_20260927_112352.jsonl` have
now been addressed in the x64 driver.

`0x00223004` supports both:
- out=4 -> 0x110;
- out=0x110 -> reconstructed CKeTraceControl descriptor block.

`0x00223040` supports both:
- out=4 -> 0x2CAE;
- out=0x2CAE -> reconstructed 43-entry register list with 0x10A-byte entries.

The next hardware capture should determine whether XStream proceeds from the
visible "Acquiring" wait to CFDC2124 transfer registration and then to
CFDC2138 / CFDD219F acquisition execution.

## Twenty-seventh runtime capture: full buffers succeed, indexed query reached

Trace `xstream_trace_20260927_114152.jsonl` contains 1102 IOCTL events.

Important progress:

- all four 0x223004 calls succeed, including both 272-byte full payload reads;
- all full 0x223040 register-list reads succeed, including the 11438-byte
  serialized list;
- 1072 CFDC2110 calls are present and succeed.

Only three IOCTL failures remain:

- `0x00222400`, out=4, still unmapped;
- `0x00223000`, in=264, out=0;
- `0x00223040`, in=4, out=266.

The new `0x00223040` form is the indexed single-register query. Legacy
`FUN_00012C18` accepts a four-byte register index in the shared buffered IOCTL
buffer and calls `FUN_000124C2` to refresh and serialize exactly one 0x10A-byte
register entry into that same buffer. The captured request asks for index 0.

The new `0x00223000` form maps to legacy `FUN_00012ADA`. It accepts exactly
0x108 input bytes and passes the final two DWORDs to
`FUN_00012290(traceControl, index, level)`. This changes only driver tracing
verbosity; the captured input is entirely zero, i.e. index 0 / level 0.

The x64 driver now implements both forms. No CFDC2124, CFDC2138 or CFDD219F is
present in this capture yet.

## Twenty-eighth runtime capture: only 0x00222400 still fails

Trace `xstream_trace_20260927_114916.jsonl` contains 959 IOCTL events after
the trace header.

All newly implemented query forms now succeed:
- 0x223000 trace-control setter;
- 0x223004 size and full 0x110-byte payload;
- 0x223040 size, full 0x2CAE-byte register list, and indexed 0x10A-byte
  single-entry query.

There are 937 CFDC2110 calls and all succeed.

Exactly one IOCTL fails in the entire capture:
- `0x00222400`, input length 0, output length 4,
  STATUS_INVALID_DEVICE_REQUEST.

No CFDC2124, CFDC2138 or CFDD219F appears afterward. At this point
`0x00222400` is the only remaining visible startup/acquisition gate.

The existing raw DeviceControl export has a gap from 0x10F8F to 0x11018,
which is the only remaining region that can contain the dispatch handling for
the lower 0x222400 code before the 0x222C00 branch. Additional Ghidra raw
targets were added at 0x11008, 0x11010 and 0x11014 to recover that final gap.

## Twenty-eighth trace re-evaluation: the real hang is JTAG polling

Further inspection of `xstream_trace_20260927_114916.jsonl` changes the
current blocker assessment.

Although `0x00222400` is the only remaining IOCTL that returns an error, the
fully reconstructed legacy DeviceControl switch does not contain a handler for
that code either. It is therefore likely a tolerated probe rather than the
cause of the acquisition stall.

The actual runtime hang is visible at the end of the trace: XStream repeatedly
issues the same CFDC2110 family-1 opcode-`0x42` JTAG transaction.

The repeated request is:
- mode = 1;
- requested data bytes = 10;
- bit count = 76;
- five JTAG chunks (4 x 16 bits + 12 bits).

The current x64 implementation returns the following 10 data bytes, interpreted
as five 16-bit words:

`0000 4014 0030 5000 0020`

XStream immediately repeats the identical scan, indicating that the response
does not satisfy the state transition it is waiting for.

Static review of legacy `FUN_00015C7E` plus `FUN_0001586E` shows that the
JTAGDIN value is returned through an unusual stack output parameter that Ghidra
does not currently decompile correctly. The x64 implementation's direct
JTAGDIN read is conceptually correct, but the exact word selection/bit shift is
still uncertain.

Raw assembly targets were added inside FUN_00015C7E at 0x15CC0, 0x15CF0,
0x15D20, 0x15D50 and 0x15D70 to recover the exact response-word formation.

## JTAG poll follow-up: upstream timing mismatch identified

Full assembly for legacy `FUN_00015C7E` confirms that the x64 JTAG response
bit extraction is already correct:

- full 16-bit chunks return `JTAGDIN >> 16`;
- the final partial chunk returns
  `JTAGDIN >> (32 - remaining_bits)`;
- JTAGNUM mode/bit-count programming and JTAGDAT word ordering match the
  legacy implementation.

The repeated mode-1 76-bit JTAG poll is therefore downstream of an earlier
state/timing problem rather than an output-word extraction bug.

The immediately preceding runtime sequence is:
- multiple type-1 MAM configuration records;
- family-2 opcode 0x02 writing MTTCTL=1;
- repeated family-1 opcode 0x42 JTAG poll.

Static review of legacy `FUN_000163B2` found that opcode 0x02 must wait on
the timer armed by opcode 0x01 before writing MTTCTL. The x64 implementation
had instead waited on a DMA completion event, which is not equivalent and is
normally absent at this stage.

This timer barrier is now corrected. Type-1/type-2 MAM records are also fixed
to always use legacy MAM mode 1.

## Twenty-ninth runtime capture: JTAG poll unchanged after timer fix

Trace `xstream_trace_20260927_121112.jsonl` contains 950 IOCTL events.

Results:
- 949 successful IOCTLs;
- the only failing IOCTL is still the tolerated `0x00222400` probe;
- 930 CFDC2110 calls;
- no CFDC2124 / CFDC2138 / CFDD219F.

The family-2 opcode-0x02 timer-barrier correction does not by itself resolve
the acquisition stall. The trace still enters the same repeated family-1
opcode-0x42 JTAG scan after sequence 905.

The exact pre-poll sequence is now especially informative:
- seq 893-898: six type-1 MAM configuration records;
- seq 899-904: the same six type-1 records repeated byte-for-byte;
- seq 905: family-2 opcode 0x02, MTTCTL=1;
- seq 906 onward: repeated mode-1 76-bit JTAG status scan.

Legacy `FUN_000179E2` maintains a 16-bit shadow per MAM index and suppresses
MAMDAT writes when the indexed value is unchanged. The previous x64 driver
wrote every indexed value again on the repeated 899-904 block.

The x64 driver now tracks a per-index MAM shadow plus validity and suppresses
duplicate MAMDAT writes while still issuing MAMPGO for each record, matching
legacy semantics.

## Twenty-ninth trace follow-up: exact MAM shadow constructor recovered

Trace `xstream_trace_20260927_121601.jsonl` still reaches the same repeated
family-1 opcode-0x42 76-bit JTAG scan with response words:

`0000 4014 0030 5000 0020`

No CFDC2124, CFDC2138 or CFDD219F appears.

Further static analysis of the MAMDAT register object resolved the constructor
semantics. The MAMDAT object at board offset +0x368 is built by
`FUN_00011A00`, not the simpler generic register constructor.

`FUN_00011A00` initializes all 256 per-index shadow DWORDs to
`0xFFFFFFFF`. Since `FUN_000179E2` compares only the low 16-bit data field,
the effective initial value is `0xFFFF` for every MAM index.

The x64 implementation previously treated every index as initially invalid and
therefore always issued the first MAMDAT write. It now starts with the exact
legacy 0xFFFF shadow state and suppresses a first write when the requested data
is already 0xFFFF.

## Thirtieth runtime capture: root-cause candidate in JTAG chunk sequencing

Trace `xstream_trace_20260927_122739.jsonl` still reaches the same repeated
family-1 opcode-0x42 76-bit JTAG poll and returns:

`0000 4014 0030 5000 0020`

The exact 0xFFFF MAM shadow initialization does not affect the six MAM records
immediately preceding the poll because none of those records contains a
0xFFFF data word.

A stronger legacy mismatch was then identified in both JTAG handlers.

Legacy `FUN_00015C7E` and `FUN_00015ACC` program JTAGNUM once before a run
of complete 16-bit chunks. JTAGDAT is then streamed repeatedly while JTAGNUM
is left untouched. Only the final partial chunk reprograms JTAGNUM with the
remaining bit count.

The previous x64 implementation rewrote JTAGNUM before every single 16-bit
chunk. On hardware where writing JTAGNUM arms/restarts the shift operation,
that changes a continuous 76-bit scan into multiple restarted sub-scans.

The x64 driver now exactly matches legacy sequencing:
- one JTAGNUM write for all full 16-bit chunks;
- stream all corresponding JTAGDAT words;
- one additional JTAGNUM write only for the final partial chunk;
- same behavior applied to both read/write JTAG transactions and write-only
  JTAG transactions.

This is the current primary root-cause candidate for the repeated JTAG status
poll.

## Thirty-first runtime capture: JTAG response still unchanged

Trace `xstream_trace_20260927_123731.jsonl` contains 962 IOCTL events.

Summary:
- 961 succeed;
- only the tolerated `0x00222400` probe still fails;
- 937 CFDC2110 calls;
- no CFDC2124 / CFDC2138 / CFDD219F.

The corrected continuous JTAG chunk sequencing does not change the observed
family-1 opcode-0x42 status. The repeated 76-bit scan still returns:

`0000 4014 0030 5000 0020`

XStream performs several polls, then issues `0x00222C00` DELAY_MS with a
10 ms delay, and resumes the same poll. This is therefore an intentional
user-mode wait loop on a hardware status condition rather than an IOCTL
failure loop.

At this stage the most valuable discriminator is a reference response from
the original 32-bit driver on the same hardware state. The existing x64 trace
facility cannot run against the legacy driver because its DEBUG_GET_TRACE
IOCTL is private to the replacement driver.

`lecdiag` now includes:
- `raw-ioctl <code> <input-hex> <output-bytes>`;
- `legacy-jtag-poll`, which sends the exact repeated 76-bit CFDC2110 request
  and prints the returned 24-byte response.

The build helper also supports `-Architecture x86` so the same diagnostic can
be run on a 32-bit Windows installation with the original driver.

## Legacy-driver reference for the repeated 76-bit JTAG poll

The new x86 `lecdiag legacy-jtag-poll` command was run successfully against
the original 32-bit LeCroy driver through interface
`{7AC34BE9-F766-4F15-9E88-854BA5E2146E}`.

The exact CFDC2110 request used by the x64 stall returned 24 bytes:

`000000000000000000000C00000000144020000002002000`

The second-record payload header is identical to the x64 response. Interpreting
the ten JTAG data bytes as five little-endian 16-bit words gives:

- legacy: `1400 2040 0000 0002 0020`
- x64:    `1400 3040 0000 0050 0020`

Therefore words 1, 3 and 5 match exactly. Only words 2 and 4 differ:
- `0x2040` vs `0x3040`;
- `0x0002` vs `0x0050`.

This strongly indicates that the JTAG transfer framing/readback path is now
correct and that the FPGA/device is in a different state before the poll. The
remaining work should focus on upstream initialization/state transitions rather
than response packing.

## Legacy state transition confirmed around XStream startup

Repeated direct legacy-driver polling before XStream is stable across multiple
runs:

`1400 2040 0000 0002 0020`

After XStream starts with the original driver, the same direct poll becomes:

`1400 2040 0000 0000 0020`

The current x64 driver remains at:

`1400 3040 0000 0050 0020`

Therefore:
- legacy word 2 remains stable at 0x2040 across startup;
- legacy word 4 transitions from 0x0002 to 0x0000 when XStream initializes the
  board;
- x64 additionally has bit 0x1000 set in word 2 and bits 0x0010/0x0040 set in
  word 4.

This confirms that the x64 stall is caused by upstream device state rather than
JTAG response packing.

A new `lecdiag legacy-prepoll-replay` diagnostic replays the exact immediate
pre-poll CFDC2110 sequence captured from the x64 startup and probes the 76-bit
JTAG status after each individual operation. It is intended for the original
32-bit driver on a freshly initialized board, before XStream, to identify the
specific state-changing command.

## Legacy pre-poll replay isolates two hardware state transitions

Running `lecdiag legacy-prepoll-replay` on a fresh board with the original
32-bit driver produced a stable initial status:

`1400 2040 0000 0002 0020`

The first seven replayed operations leave that state unchanged.

Step 8, the first family-0 opcode-0x42 JTAG write, changes only word 2:

`1400 2040 0000 0002 0020`
-> `1400 3040 0000 0002 0020`

Steps 9 through 32 leave that state unchanged.

Step 33, family-2 opcode-0x02 with MTTCTL=1, changes only word 4:

`1400 3040 0000 0002 0020`
-> `1400 3040 0000 0000 0020`

The user also heard physical relays switching during this replay, confirming
that the sequence is changing real board state.

This is a critical discriminator because the current x64 startup reaches:

`1400 3040 0000 0050 0020`

Therefore:
- the first JTAG write causing word2 0x2040 -> 0x3040 is normal and also occurs
  under the legacy driver;
- the real divergence is around the MTTCTL transition: legacy reaches word4
  0x0000, while x64 reaches 0x0050.

The next investigation should focus tightly on family-2 opcode 0x02 / MTTCTL
and the state immediately feeding that operation, rather than on JTAG framing
or the earlier GPIO/SPI/MAM commands.

## x64 pre-poll replay started from an already-diverged board state

Running `legacy-prepoll-replay` against the x64 replacement driver produced:

Initial:
`1400 3040 0000 0050 0020`

and every replay step, including MTTCTL=1, remained at that same value.

This run cannot be compared step-for-step with the fresh-board legacy replay
because the x64 board state was already divergent before the first replayed
operation. The current evidence therefore does not prove that the x64
family-2 opcode-0x02 implementation itself creates 0x0050; the state may have
persisted from a previous XStream/acquisition attempt.

Static re-check of legacy FUN_00012FDE versus LecRunLegacyStartupProbe shows
the startup MMIO sequence matches:
- START <- 1;
- 100 us delay;
- read START bit0;
- buzzer 300 ms;
- ITMODE <- 7;
- 500 us delay;
- buzzer 300 ms;
- ITMODE <- 3.

A true cold-hardware baseline is required next. Restarting/reloading the driver
is insufficient if FPGA/JTAG state survives PCI driver reload. Power-cycle the
scope/hardware, do not start XStream, and immediately run only
`lecdiag legacy-jtag-poll` against the x64 driver.

## True cold x64 baseline after full power cycle

After fully powering the scope off and back on, with XStream not started, the
first direct x64 `legacy-jtag-poll` returned:

`000000000000000000000C00000000FFFFFFFFFFFFFFFFFF0F`

The 10-byte JTAG data field is therefore all ones apart from the final unused
high bits. This is not the fresh legacy-driver baseline
`1400 2040 0000 0002 0020`.

Important consequence: the x64 replacement does not establish the same JTAG /
board state as the original driver before user-mode initialization. The earlier
x64 replay that started at `1400 3040 0000 0050 0020` was therefore already
after some later state-changing activity.

Next discriminator: perform another full hardware power cycle, do not run any
other lecdiag command and do not start XStream, then immediately run
`lecdiag legacy-prepoll-replay`. This will show which replayed initialization
step first moves the x64 board away from the all-ones cold state.

## Cold x64 replay reaches the correct final hardware state

A full power-cycle followed immediately by `lecdiag legacy-prepoll-replay`
against the x64 replacement driver produced this initial state:

`1400 2000 0000 0000 0020`

This differs from the fresh legacy baseline
`1400 2040 0000 0002 0020`.

However, the replay then behaved coherently:
- steps 1-7: unchanged at `1400 2000 0000 0000 0020`;
- step 8, first JTAG write: `1400 3040 0000 0000 0020`;
- steps 9-32: unchanged;
- step 33, MTTCTL=1: unchanged at `1400 3040 0000 0000 0020`.

The critical result is that the final x64 replay state exactly matches the
legacy replay's final post-MTTCTL state:

`1400 3040 0000 0000 0020`

Therefore the individual replayed GPIO/SPI/JTAG/MAM/MTTCTL implementations are
capable of establishing the expected final board status on x64. The previously
observed XStream stall state `1400 3040 0000 0050 0020` is specific to the
real XStream execution path and is not reproduced by the controlled replay.

This shifts the investigation away from the basic implementations of these
operations and toward differences between the replay and real startup:
preceding commands, command batching/record boundaries, timing, timer state,
or interactions with other host-side IOCTLs/events.

## Trace after successful controlled pre-initialization

Trace `xstream_trace_20260927_151006.jsonl` was captured after the controlled
x64 pre-poll replay had reached the correct legacy-equivalent state
`1400 3040 0000 0000 0020`.

The real XStream startup nevertheless returns to the known stalled state:

`1400 3040 0000 0050 0020`

The repeated 76-bit family-1 opcode-0x42 status poll occurs 46 times in this
trace and every response is identical.

Trace summary:
- 962 IOCTL records plus header;
- CFDC2110: 933 calls;
- only failed IOCTL: sequence 2, `0x00222400`, status
  `STATUS_INVALID_DEVICE_REQUEST (0xC0000010)`;
- no CFDC2124 persistent transfer registration;
- no CFDC2138 acquisition front;
- no CFDD219F METHOD_NEITHER acquisition;
- immediate pre-poll block at sequences 879-911 remains the same relevant
  GPIO/SPI/JTAG/timer/MAM/MTTCTL sequence.

Therefore the correct manually established board state is overwritten earlier
during the real XStream startup. The next useful comparison must cover commands
before the extracted pre-poll block rather than modifying that block itself.

Current UI behavior also differs from early testing: the previous startup dialog
with a "Beenden" option no longer appears. The remaining visible startup warning
reports that Channel 1 through 5 probes cannot be read and is dismissed only
with "OK". This may belong to the probe/ProBus detection path, but the current
trace does not establish that it causes the later JTAG polling loop.

## Planned legacy user-mode IOCTL capture

The next comparison should capture XStream's actual traffic while it talks to
the original 32-bit driver, rather than infer legacy behaviour from active
probes.

Preferred approach: inject a 32-bit instrumentation DLL into the original
32-bit XStream process and hook Win32 `DeviceIoControl` (and optionally
`CreateFileA/W` / `CloseHandle`) in user mode. The hook should call the real
API unchanged and log, for each call:
- timestamp / sequence number / thread id;
- device handle and resolved device path when available;
- IOCTL code;
- input length and exact input bytes before the call;
- output capacity;
- BOOL return value and GetLastError;
- returned byte count and exact output bytes after the call;
- optional duration.

This avoids modifying or instrumenting the original kernel driver and gives a
passive ground-truth trace of XStream's legacy request/response stream. A
Detours-style x86 API hook is suitable because XStream itself is 32-bit.

ProcMon is useful for identifying device opens but is not sufficient as the
primary capture mechanism because the required IOCTL payload bytes must be
recorded. WinDbg can be used for spot checks, but an automated user-mode hook
is preferable for the hundreds of startup calls.



## Passive original-XStream user-mode IOCTL capture

A dedicated x86 tracer is now in `tools/xstream-ioctl-trace`.

It uses a small launcher plus injected x86 DLL instead of modifying the legacy
kernel driver. The DLL hooks the user-mode import path for `DeviceIoControl`
and records the exact request/response bytes seen by original 32-bit XStream.
It also records CreateFile/CloseHandle activity so the LeCroy device handle can
be identified directly.

Build:

```powershell
.\scripts\build-xstream-ioctl-trace.ps1
```

Run on the original 32-bit system:

```powershell
.\xstream_trace_launcher.exe "C:\Program Files\LeCroy\XStream\lecroyxstreamdso.exe"
```

The launcher creates a timestamped `legacy_xstream_trace_*.jsonl` in the
current directory by default. Input/output capture is capped at 1 MiB per
direction. Synchronous DeviceIoControl calls have complete returned output;
pending overlapped calls are marked but completion APIs are not yet hooked.

This is now the preferred ground-truth source for comparing the original driver
with the x64 replacement, especially the startup traffic before the known
pre-poll block.

## First legacy user-mode hook capture and native-API escalation

First successful user-mode capture:
`legacy_xstream_trace_20260927_165243.jsonl`

Summary:
- 2016 JSONL records total;
- 2014 `DeviceIoControl` records;
- only five distinct IOCTL codes were observed;
- all observed codes are in the `0x004708xx` family;
- captured payloads contain HID device paths;
- no LeCroy IOCTL such as `0xCFDC2110`, `0xCFDC21C8`, or `0x00222C00`
  appeared through the Win32 `DeviceIoControl` hook.

Therefore the LeCroy software path is not using the hooked Win32 API surface for
its acquisition-driver traffic. The likely next layer is the Native API.

Commit `994a875641e88adb9f3bcec5a3b8c1ed91fbca8d` extends the injected x86
hook to intercept `ntdll!NtDeviceIoControlFile` as well. The new `nt_ioctl`
records contain:
- native IOCTL code;
- input/output sizes and exact buffers;
- returned NTSTATUS;
- `IO_STATUS_BLOCK.Information`;
- event/APC pointers and pending state;
- duration, sequence and thread id.

Trace format version is now 2. Rebuild both x86 tracer binaries and repeat the
legacy XStream capture.

## CPU-frequency sensitivity and tracer timing perturbation

The original 32-bit reference system has a known timing sensitivity: with the
Core i5-3450 running above its minimum clock, XStreamDSO regularly reports
acquisition-board errors, including failures around trigger-level programming
and driver communication. The reference Windows 32-bit installation is
therefore normally limited to 5% maximum processor state. The current 64-bit
system has been running at 100% maximum processor state.

This is potentially relevant to the x64 compatibility work. It suggests that at
least part of the original software/driver/firmware stack contains
CPU-speed-sensitive polling or delay assumptions. The x64 test environment
should therefore be compared at the same low processor-state setting before
drawing conclusions from timing-dependent startup behavior.

The first native-API legacy trace also demonstrated substantial tracer overhead.
The hook measured the kernel call itself before writing the log, so its
`duration_us` values do not include the synchronous log flush performed after
each record. The first implementation used both `FILE_FLAG_WRITE_THROUGH` and
`FlushFileBuffers` after essentially every traced call, which can seriously
slow XStream and distort a timing-sensitive path.

Commit `668133737be58adc7f209aa3d126ee5aaaad9c9b` changes the tracer to buffered
logging:
- removes `FILE_FLAG_WRITE_THROUGH`;
- flushes every 256 records instead of every record;
- still flushes the header immediately and flushes on normal DLL detach.

This reduces timing perturbation while keeping bounded trace loss on a crash.

The captured native trace `legacy_xstream_trace_20260927_185222.jsonl`
contains real LeCroy traffic, including approximately:
- 9158 x `0xCFDC2110`;
- 1984 x `0xCFDC2138`;
- 90 x `0xCFDC2124`;
- 74 x `0xCFDC2128`.


## First semantic legacy-vs-x64 comparison

A new standard-library-only comparator is available at
`tools/trace-diff/compare_xstream_traces.py`.

Comparing `legacy_xstream_trace_20260927_185222.jsonl` against
`xstream_trace_20260927_151006.jsonl` shows the first important differences
very early in startup:

1. Both streams begin with Dallas ID, 0x00222400, and Dallas memory requests.
2. The original driver returns success for 0x00222400; x64 returned
   `STATUS_INVALID_DEVICE_REQUEST`.
3. Immediately after that, the x64 XStream path contains three raw
   `0xCFDC21C4` writes to BAR0 offset 0x0C with values 4, 2 and 1. These calls
   are absent from the original-driver stream.
4. For the same family-1/opcode-0x99 CFDC2110 request, the original driver
   returns real board payload while the captured x64 response differs
   substantially. This may be downstream of the already-diverged startup state
   and should be retested after the early ABI fixes.
5. Original XStream performs opcode-0x88, opcode-0x85 and CFDC2184 earlier than
   the x64 path.
6. The original 0x00223040 register list starts with TxControl; the x64 list
   started with FVER.
7. Original trace-control descriptor string is `CKeTraceControl: `, while x64
   emitted `CKeTraceControl`.

These are earlier and more plausible control-flow causes than the later
76-bit JTAG poll. The x64 driver has been corrected for the proven local ABI
differences first; board-forward response differences will be re-evaluated
after a new trace.

## Legacy trace reveals multiple distinct device handles

A deeper review of `legacy_xstream_trace_20260927_185222.jsonl` shows that
LeCroy-related IOCTL families are not all issued through one handle.

Observed handle grouping:

- `0x00000690`: main acquisition/control path
  - 9156 x `0xCFDC2110`
  - 1984 x `0xCFDC2138`
  - 290 x `0xCFDC2184`
  - 90 x `0xCFDC2124`
  - 74 x `0xCFDC2128`
  - register/status controls including `0xCFDC21C0`, `0x00223040`,
    `0xCFDC2180`, `0xCFDC218C`, `0xCFDC2190`
- `0x000006A4`: trace-control path
  - `0x00223004`
  - `0x00223000`
- `0x00000698`: Dallas / board-identification path
  - `0x00223080`
  - `0x00222400`
  - `0x00223084`
- `0x000006C4`: three-event registration
  - `0x00223100`
- additional handles `0x00000308`, `0x00000AF0`, `0x00000D88`
  carry the `0x00222C00` / `0x00222C04` delay/flag family.

This is important because the current x64 reconstruction effectively exposes
the recovered interfaces on one compatibility device path and handles all of
these controls in one dispatch surface. The original stack may instead be
routing them through distinct device objects, distinct interface paths, or
separate logical endpoints.

Therefore the runtime success of `0x00222400` must not yet be interpreted as
proof that it belongs to the main acquisition DeviceControl switch. Static
analysis had found no such branch there. The successful call is on a different
legacy handle than `0xCFDC2110`.

Commit `c3af835647f749564a18c9c589c10b53da349da7` extends the injected legacy
tracer with `ntdll!NtCreateFile` interception. New `nt_create_file` records
capture the native object path and returned handle so the next legacy run can
map each IOCTL family to its actual device/interface path. Trace format version
is now 3.