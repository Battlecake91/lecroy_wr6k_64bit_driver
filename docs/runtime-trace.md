# Runtime trace capture
## 2026-09-30 original-x86 Developer/Service-menu trace: hazardous setters still absent

Private owner capture: `legacy_xstream_trace_setregister_2.jsonl`.
The owner traversed the Developer/Service menus and read as many exposed
diagnostic/status pages as practical. The private raw trace is not committed.

The capture is complete JSONL: **13,076 valid records**, including
**9,338 `nt_ioctl` calls** and 2,020 Win32-level `ioctl` records.
The original main acquisition/control handle carries **7,141 native IOCTLs;
all 7,141 return `STATUS_SUCCESS`**.

Most importantly, exhaustive parsing finds **zero occurrences** of:

- `0x0022303C` SetOneRegister;
- `0xCFDC2130` serial-trigger FPGA/GPIODAT programmer;
- `0x00223088` Dallas EEPROM WRITE;
- `0xCFDC21C4` direct raw register WRITE.

This extends the previous normal-UI negative evidence substantially:
neither the broad normal oscilloscope-control session nor this
read-oriented Developer/Service traversal uses the two remaining hazardous
register/FPGA top-level writers. It still does not prove that factory,
calibration, firmware-update, explicit write/edit, or other rare code paths
never use them.

### Exact UI path identified from owner screenshot

The service trace's indexed register-list reads are now tied to a specific
original-XStream page:

```text
Service -> Development -> AladdinAcqBoard -> KernelPCIRegisters
```

The owner screenshot shows the GUI list beginning exactly with
`TxControl, RxControl, TxCount, RxCount, SetIRQ, HWInt, FVER, ERRS, ERRM,
INTST, IIMCL, ...`, matching the recovered 43-entry kernel register list
in order. With `TxControl` selected, the page displays address
`400 H`, a hexadecimal value field, and a visible `Read` button.
This is direct UI-level corroboration that the page consumes the
`0x00223040` register-list/read ABI observed in the service trace.

No visible `Write` button is present in the supplied screenshot. Do not
assume that editing the value field is harmless: if the page has an
implicit property setter (Enter/focus-loss/script callback), it could be a
candidate producer for the still-unseen `0x0022303C` SetOneRegister path.
Until the user-mode page implementation is statically traced, treat the
value field as read-only and do not test speculative edits on the physical
scope.

### Service-specific traffic that *was* observed

Compared byte-for-byte with the previous broad normal-UI trace, this
Developer/Service capture introduces only **one additional top-level
LeCroy IOCTL code**: `CFDC21C8`, queried twice and returning legacy
driver build **1002** each time.

It also adds eleven `CFDC2110` request shapes not present in the normal
trace:

- eight distinct family-1 opcode-`0x42` JTAG read/query buffers,
  each issued twice;
- family-1 opcode `0xA2` twice;
- family-1 opcode `0xA1` twice;
- one family-0 opcode-`0x84`, selector `0x17`, forwarded through
  the already-recovered generic board-message class.

The A1/A2 cluster matches the already recovered Revision-page behavior:

```text
family1/A1 -> BAR1 ACQFVER (0x00C)
family1/A2 -> BAR0 FVER    (0x000)
```

The same service region then issues `CFDC21C8`, making this a strong
runtime marker for the Service -> AladdinAcqBoard revision/identification
path. All of these calls succeed on the original driver.

The service traversal also exercises read-only register diagnostics more
deeply than the prior normal trace:

- `0x00223040` indexed register-list reads for index **1 = RxControl**
  and index **2 = TxCount** in addition to the startup/full-list/index-0
  queries;
- `CFDC21C0` direct BAR0 reads at offsets
  `0x00, 0x04, 0x08, 0x0C, 0x10, 0x14`, repeated twice.

These are all already represented by the current x64 compatibility design.
No new physical-write handler is implied.

### Cross-check against the previously documented trace corpus

A repository/history review plus the available saved trace excerpts found
**no previously documented runtime occurrence of `0x0022303C` either**.
The current broad normal-UI and Developer/Service captures are therefore
consistent with all trace evidence available to the project so far.

Do not confuse this with `CFDC21C4`: several early x64 bring-up traces
did contain direct `CFDC21C4` register writes (for example BAR0+0x0C
START programming), but that is a distinct top-level IOCTL and does not
provide evidence for the original 266-byte `0x0022303C` SetOneRegister
caller ABI.

This statement is limited to traces currently documented/searchable in
the project and saved conversation/library evidence; it is not a claim
about arbitrary unarchived vendor runs that have never been captured.

### Consequence

A user-visible read-oriented Developer/Service traversal still does not
produce a real `0x0022303C` request. Do **not** deliberately invoke an
unknown register-write or FPGA-programming service action on the only
working/licensed scope merely to force one. The safest and highest-value
next evidence remains the owner's pending static binary scan for
little-endian `3C 30 22 00` in the installed XStream EXE/DLL set.
If a binary contains it, trace its user-mode caller and request construction
before deciding whether the native driver should ever expose this writer.


## 2026-09-30 original-x86 broad UI trace: SetOneRegister/serial-FPGA paths absent

Private owner capture: `legacy_xstream_trace_setregister.jsonl`.
The raw file is **not committed** because the trace also contains
device-specific Dallas/license traffic and host/device identifiers.

The owner deliberately exercised a broad set of normal XStream controls
while tracing the original 32-bit driver:

- channels enabled/disabled;
- all channels set to 20 mV/div;
- channel coupling changes;
- channel bandwidth changes;
- timebase changes;
- sample-rate changes;
- four-channel to two-channel / 10 GS/s operation;
- trigger source CH2 -> CH1;
- trigger slope positive -> negative;
- trigger type Edge -> Width;
- Width condition Less Than -> Out Of Range.

The supplied file contains **199,741 valid JSON records** followed by one
incomplete final JSON line. The valid records include **184,862
`nt_ioctl` calls**. The main acquisition/control interface contributes
**182,588 native IOCTL calls and every one completes with
`STATUS_SUCCESS`**. Pending/non-success native records in the overall file
belong to other non-acquisition handles and are not evidence of a LeCroy
acquisition-driver error.

### Important negative evidence

Across all parsed Win32/native IOCTL records, the following original
top-level controls occur **zero times**:

- `0x0022303C` SetOneRegister;
- `0xCFDC2130` serial-trigger FPGA/GPIODAT programming;
- `0x00223088` Dallas EEPROM write.

Thus this broad normal-operation sequence does **not** use either of the two
remaining hazardous register/FPGA writers. This does not prove those IOCTLs
are never used by XStream, but it strongly narrows their likely role away
from ordinary channel scale/coupling/bandwidth, timebase/sample-rate,
2-channel/10-GS/s switching, and the exercised trigger source/slope/type/
width-condition controls.

The same trace also contains **no `0xCFDC21C4` raw register-write IOCTL**;
normal UI changes are carried through the already-observed command,
event and acquisition paths rather than this direct register-write surface.

### Fresh original-runtime confirmation of the 43-register list

The trace performs four `0x00223040` register-list calls on the main
acquisition handle:

1. required-size query -> 4-byte reply;
2. full-list query -> **11,438 bytes = 43 * 266 = 0x2CAE**;
3. a second required-size query;
4. indexed entry query for index 0 -> 266-byte `TxControl` record.

The full 11,438-byte original-driver reply was parsed as 43 consecutive
`0x10A` records. Comparing only the public ABI metadata
(name, BAR number, physical register offset and serialized type) gives
**43/43 exact matches** against the current native
`g_LecLegacyRegisterList[43]` and the independently recovered
Ghidra construction order. Runtime register **values** are intentionally
not copied into this public document.

This is independent runtime confirmation of the complete normal
full-initialization register-map ordering documented in
[original register-list and write ABI](
original-register-list-and-write-abi.md).

### Consequence for the remaining writer work

Do **not** implement `0x0022303C` or `0xCFDC2130` merely to reach
27/27 top-level source representation. The new runtime evidence says neither
was needed for this extensive normal oscilloscope-control session. The
highest-value missing evidence is now the original **user-mode producer**
of a real `0x0022303C` request (if one exists in normal/service software):
the setter's incoming DWORD at record `+0x101` is an array index, whereas
the query record places the physical BAR offset at that same location.

The owner's separate binary scan for the little-endian IOCTL constant
`3C 30 22 00` is still pending at this checkpoint. If it identifies an
XStream EXE/DLL, statically trace that caller before considering a guarded
native writer. No scope-side synthetic write is justified by this capture.


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

## Device-interface mapping recovered from NtCreateFile trace

Trace `legacy_xstream_trace_20260927_195608.jsonl` (format version 3) finally
maps native file opens to the IOCTL handles used by original 32-bit XStream.

Recovered interface roles:

- `{958695A4-693A-435E-8297-66F805D8E46A}`
  - main acquisition/control interface;
  - carries CFDC2110, CFDC2138, CFDC2124, CFDC2128, CFDC2184,
    CFDC21C0, 0x00223040, CFDC2180/218C/2190 and related controls.
- `{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}`
  - Dallas/board-identification interface;
  - carries 0x00223080, 0x00222400, 0x00223084.
- `{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}`
  - trace-control interface;
  - carries 0x00223004 and 0x00223000.
- `{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}`
  - three-event registration interface;
  - carries 0x00223100.
- `{7AC34BE9-F766-4F15-9E88-854BA5E2146E}`
  - delay/flag helper interface;
  - carries 0x00222C00 and 0x00222C04.

The x64 driver had registered only the latter four interfaces. The actual
acquisition GUID `958695A4-...` was completely missing. This is a high-value
root-cause candidate for XStream taking a different startup/control path under
x64 despite many individual IOCTL handlers being correct.

Commits:
- `45a4abe9cf08ec055b6b631881ec139575d80ca1`: increase interface count to 5;
- `65525e75c557c0e58b685b6c64c1a5d340c8c6aa`: register the recovered main
  acquisition GUID.


## Five-interface x64 retest

Fresh x64 trace: `xstream_trace_20260927_201339.jsonl`, captured after adding
the recovered main interface GUID `958695A4-693A-435E-8297-66F805D8E46A`
and while the system remained limited to 5% maximum processor state.

The corrected driver is active: the trace still shows the previously corrected
early ABI details (0x00222400 succeeds, the trace-control descriptor is exact,
and the register list begins with TxControl).

However, adding the fifth interface did not materially change XStream's startup
control flow. The trace contains 930 CFDC2110 calls and still reaches the same
76-bit family-1/opcode-0x42 poll state. Starting at sequence 906, that poll is
repeated 43 times with the identical response:

`1400 3040 0000 0050 0020`

Therefore the missing acquisition-interface registration was a real
compatibility defect but is not sufficient to resolve the startup failure.

Next discriminator: run the x86 injected user-mode tracer against the 32-bit
XStream process on the x64 replacement system as well. Because the tracer now
records NtCreateFile paths, this will show exactly which of the five interface
GUID paths XStream opens under x64 and allow a same-layer comparison against
the original 32-bit trace.

## x64 user-mode trace: acquisition traffic uses the DOS device link

Trace `legacy_xstream_trace_20260927_203017.jsonl` was captured by injecting
the x86 user-mode tracer into the 32-bit XStream process while it was running
against the x64 replacement driver.

All five recovered PCI interface GUID paths are successfully opened under x64,
including `{958695A4-693A-435E-8297-66F805D8E46A}`. However, the main
acquisition/control IOCTL stream does **not** run on that interface handle in
this trace.

Instead XStream opens the legacy DOS device name:

`\??\ALADDINAcqDriver0`

and sends the main control traffic there. The first such open is sequence 584
(handle 0x5CC); XStream later reopens the same DOS path at sequence 5056
(handle 0xBF4).

Observed on those DOS-link handles:
- approximately 922 x `0xCFDC2110`;
- approximately 512 x `0xCFDC21C0`;
- 3 x `0xCFDC21C4`;
- 4 x `0x00223040`;
- event controls including `0xCFDC2180` and `0xCFDC218C`.

The recovered helper-interface routing is also confirmed under x64:
- `8D1103B8...`: Dallas / board identification;
- `9007C2BC...`: trace control;
- `FC5DF040...`: three-event registration;
- `7AC34BE9...`: delay/flag helper.

The `958695A4...` path is opened successfully, but no LeCroy IOCTL traffic is
observed on its handle in this x64 trace. Therefore the earlier interpretation
that this GUID itself is the main DeviceIoControl endpoint was too strong. It
may instead be a discovery/classification interface while the legacy DOS link
remains the actual acquisition-control endpoint.

This also explains why adding the fifth GUID did not alter the failing
CFDC2110/JTAG startup behavior: XStream was already sending that traffic through
`ALADDINAcqDriver0`, which the x64 driver has exposed from the beginning.

No active acquisition IOCTLs such as `0xCFDC2138` are reached in this failing
x64 startup trace, consistent with XStream stalling earlier during board
initialization.

## Root-cause candidate: x64 published a DOS alias that the original system does not

A direct comparison of the injected user-mode traces exposed a crucial endpoint-selection difference.

On the original 32-bit system, XStream probes:

`\??\ALADDINAcqDriver0`

twice, but both opens fail with:

`STATUS_OBJECT_NAME_NOT_FOUND (0xC0000034)`

After that failure, XStream opens the PCI device interface
`{958695A4-693A-435E-8297-66F805D8E46A}` and sends the main acquisition /
control stream through that interface handle.

On the x64 replacement system, the driver explicitly created a DOS symbolic
link named `ALADDINAcqDriver0`. Therefore the same probe succeeds and XStream
continues on a different endpoint-selection path. In the x64 user-mode trace,
the main CFDC2110/register stream is then sent through the DOS-link handle.

This is the first proven startup control-flow difference caused directly by the
replacement driver rather than by board state.

Commit `0534482a6e00463b12420e03a28c83e9465801a6` removes publication of the
legacy DOS alias from the x64 driver. The internal device name remains unchanged
for the driver object itself, but no user-visible `\\.\ALADDINAcqDriver0`
symbolic link is created.

Expected result after this change:
1. XStream's DOS-name probe should fail as on the original system.
2. XStream should fall back to the 958695A4 PnP interface.
3. The main CFDC2110/control stream should move onto that interface handle.
4. Only after this endpoint-selection behavior matches should later JTAG /
transport responses be compared again.

## DOS-alias removal changed x64 startup path; next missing IOCTL exposed

Fresh x64 trace: `xstream_trace_20260927_204139.jsonl`, captured after commit
`0534482a6e00463b12420e03a28c83e9465801a6` stopped publishing the
`ALADDINAcqDriver0` DOS alias.

This change materially altered startup behavior. The previous x64 path that
issued three raw BAR0 START writes (4, 2, 1) is no longer present. XStream now
enters the interface-based path that more closely matches the original system.

The next proven ABI mismatch appears at sequence 20:

`0xCFDC2190 -> STATUS_INVALID_DEVICE_REQUEST (0xC0000010)`

Request bytes:

`0000000002000000FF7F00000000000000000000000000000000000000`

The original driver implements this control and accepts exactly 29 input bytes.
Recovered startup-relevant semantics:
- DWORD at +0x04 controls global interrupt-mask bit 1;
- DWORD at +0x08 is written to BAR0 ERRM (offset 0x008);
- no output is returned.

The observed startup request therefore enables bit 1 and programs ERRM=0x7FFF.

Implemented in commits:
- `3d393570ee35c1b014c5093b167581c3091d7615`: add IOCTL constant;
- `803670576886117541848b81c867872814e39a10`: implement the 29-byte startup
  path and preserve the recovered interrupt-mask semantics.

This is now the next x64 retest target before further JTAG analysis.

## First successful-status semantic divergence: 85FB framing

Fresh x64 trace `xstream_trace_20260927_210024.jsonl` contains 953 IOCTLs and
no failing NTSTATUS values. `0xCFDC2190` now succeeds at sequence 20 with the
expected 29-byte startup request.

The earliest remaining byte-level mismatch against the original 32-bit trace is
already visible at the first family-1 opcode-0x99 forward/fetch transaction.

Request:
`060004000300FBA540019900080102000300FB854000`

Original 270-byte output begins:

`0000000000000000000002000200FFFFFFFF...`

x64 output begins:

`0000000000000200000000000000...`

The original 0x88 and 0x85 forward/fetch responses show the same structural
pattern. This proves that the legacy 85FB path prepends a six-byte host response
header to the raw BAR1 firmware response:
- DWORD 0
- WORD 2
- raw firmware response bytes follow.

The x64 implementation had been copying the raw firmware response directly into
the 85FB output record, shifting the protocol layout by six bytes.

Commit `33e24638d72bd5ae587ff80d6bebea4702a112cb` fixes this for hardware-backed
85FB responses while leaving locally constructed JTAG/timer responses unchanged.

After this framing fix, any remaining opcode-0x99 payload mismatch (notably
original 0xFF bytes versus current 0x00 bytes) can be evaluated independently
as transport/state rather than host-record framing.

## Acquisition-link error after 85FB framing fix exposes opcode-0x88 admission bug

Fresh x64 trace: `xstream_trace_20260927_211141.jsonl`.

Visible XStream behavior changed materially: instead of the previous startup
stall it now reports:

`Stopped the Acquisition, Go to service Menu, Internals to Reset the Link`

The trace shows why. Immediately before the new failure state, XStream sends the
same family-0 opcode-0x88 request ten times:

`060006000300FBA5400088001F00080002000300FB854000`

All ten x64 calls return `STATUS_INVALID_DEVICE_REQUEST (0xC0000010)`.

The original 32-bit trace contains the exact same request once (legacy sequence
3375) and returns success with 14 output bytes:

`0000000000000000000002000000`

The actual x64 opcode-0x88 handler was already present and implements the
recovered semantics:
- read the trailing WORD mask;
- clear those bits from `LegacyTransferMask`;
- forward the command to board firmware;
- fetch the firmware response.

The failure was caused solely by the pre-dispatch structural admission check,
which did not admit generic opcode-0x88 records and only allowed one old captured
literal containing mask 0xFFDF.

Commit `cf90d85d3ee6d13710251ffc4edd6a2735ada579` updates structural admission to
accept family-0 opcode 0x88 whenever the recovered minimum payload length is
present. This admits both observed masks (0xFFDF and 0x001F) without weakening
record framing validation.

This is now the next x64 retest target.

## Trigger starts; probe failures gone; runtime poll blocked by 85FB length field

Fresh x64 trace: `xstream_trace_20260927_212113.jsonl`.

Visible XStream state improved materially:
- previous acquisition-link error is gone;
- channel probe failure messages are gone;
- trigger can be started;
- no waveform/acquisition data is transferred yet.

The trace contains 3153 IOCTLs, of which 3125 are `0xCFDC2110`. There are
still **no** `0xCFDC2124`, `0xCFDC2138`, or `0xCFDD219F` calls. This means
XStream has not yet entered the DMA/acquisition-buffer path.

After trigger start, XStream polls family-1 opcode `0x96` 2473 times with:

`06000A000300FBA540019604002000000002080202000300FB854000`

The original 32-bit trace contains the exact same request. Comparing the first
128 response bytes shows only one differing byte:

- original: `...0000000002020000546573745F496E666F726D6174696F6E...`
- x64:      `...0000000002000000546573745F496E666F726D6174696F6E...`

The `Test_Information` payload and all following captured bytes are otherwise
identical. Therefore transport/content are correct; the remaining difference is
the 16-bit payload-length field in the 85FB host response header.

Cross-checking other original responses establishes the legacy rule:
- family1/op81: fetch output 10 -> header length 4;
- family1/op90: fetch output 12 -> header length 6;
- family1/op96: fetch output 520 -> header length 514 (0x0202);
- local JTAG responses follow the same `recordOutput - 6` rule;
- family1/op99 is a special legacy case and reports length 2 despite a larger
  output record.

Commit `d51dbe2f5098c2797094b7c1681bf7a2e65d8acd` updates raw-hardware 85FB
framing to emit `recordOutput - 6` and preserves opcode-0x99 as an explicit
length-2 exception.

This is now the next x64 retest target. If XStream accepts the corrected opcode
0x96 status response, it should be able to progress toward transfer registration
and the first `CFDC2124/2138` acquisition requests.

## First near-functional x64 startup: probes OK, trigger starts, no waveform data

Fresh x64 trace: `xstream_trace_20260927_212113.jsonl`, captured after
commit `cf90d85d3ee6d13710251ffc4edd6a2735ada579` admitted generic
family-0 opcode-0x88 requests.

Visible XStream behavior improved substantially:
- the previous "Stopped the Acquisition ... Reset the Link" error is gone;
- channel probe read failures are gone;
- XStream allows acquisition/trigger to be started;
- no waveform/acquisition data appears.

The trace contains no `0xCFDC2124`, `0xCFDC2138`, or `0xCFDD219F`
requests. Therefore the current problem is not yet active DMA-buffer
registration or transfer execution. XStream is still waiting in the CFDC2110
control/status layer before entering the acquisition-buffer path.

The late runtime traffic is dominated by family-1 opcode-0x96 CFDC2110
transactions. This is now the primary next investigation target: compare the
opcode-0x96 request/response semantics byte-for-byte against the original
32-bit reference trace and determine which returned state prevents XStream from
advancing to transfer registration and CFDC2138 acquisition.

At this point the legacy endpoint selection, early startup ABI, probe
enumeration, and trigger-control path are substantially functional.

### 2026-09-27 x64 runtime trace: Family 1 opcode 0x96 dominates pre-DMA state

The x64 kernel trace `xstream_trace_20260927_212113.jsonl` was inspected after the DOS alias removal, `CFDC2190` implementation, 85FB host-header fix, and generic Family-0 opcode `0x88` allowance.

Key observations:

- 3,125 of 3,154 captured records are `CFDC2110` requests.
- 2,473 `CFDC2110` requests are the same Family-1 opcode-`0x96` transaction:

  ```text
  06000A000300FBA540019604002000000002080202000300FB854000
  ```

- These calls complete with NTSTATUS success and report `Information = 526` bytes.
- The in-kernel trace stores only the first 128 response bytes, so it is not sufficient for a byte-for-byte comparison of the complete 526-byte opcode-`0x96` response.
- The common 128-byte prefix begins with the expected six-byte host framing followed by a firmware payload containing the ASCII string `Test_Information`.
- Later 128-byte previews contain varying `2C00` words at multiple positions. Do not treat those changes as the blocker until a complete user-mode response capture is compared with the legacy system; the kernel preview is incomplete by design.
- No IOCTL in this trace failed at the NTSTATUS layer.
- `CFDC2124`, `CFDC2138`, and `CFDD219F` are still absent. XStream therefore remains in the pre-DMA control/status phase.

Current next step: capture or recover the complete original 32-bit and x64 user-mode `CFDC2110` responses for Family 1 opcode `0x96`, then compare request, full 526-byte response, and transition sequence. Do not modify the DMA/MDL path before that comparison identifies the first semantic divergence.


### 2026-09-27 correction: opcode-0x96 difference is the 85FB host length

The initial interpretation of output offset 11 as a firmware readiness byte was
incorrect. `CFDC2110` returns concatenated per-record outputs. For the captured
opcode-`0x96` request the first A5FB record contributes six bytes, so total
output offsets 6..11 are the host header of the following 85FB fetch record.
The differing word at offsets 10..11 is therefore the 85FB **host payload
length**, not firmware state.

The full legacy traces make the rule explicit:

- opcode-`0x90`: legacy output contains 85FB length `0x0006`;
- opcode-`0x96`: legacy output contains 85FB length `0x0202` (514);
- opcode-`0x99`: legacy special case reports length `0x0002`.

`xstream_trace_20260927_212113.jsonl` was captured at 21:21 local time before
commit `d51dbe2f5098c2797094b7c1681bf7a2e65d8acd` at 21:31 local time. That
commit replaces the old hard-coded raw-hardware 85FB length `2` with
`recordOutput - 6`, while keeping the opcode-`0x99` override at 2. Therefore
`212113` is evidence for the bug fixed by `d51dbe2`, not evidence for a
remaining post-fix firmware mismatch.

A second misleading observation is also resolved: the legacy user-mode trace
shows repeated successful `CFDC21C0` reads between nearby control operations,
while the x64 kernel trace does not. The x64 kernel trace intentionally drops
successful `LECS65_IOCTL_REGISTER_READ / CFDC21C0` entries to avoid flooding
its 256-entry ring. Their absence cannot be used to claim that XStream skipped
that polling path.

The two independent original captures agree on the opcode-`0x96` sequence.
They perform 120 contiguous opcode-`0x96` fetches beginning with selector
`0x20`, then `0x22`, `0x24`, `0x26`, and so on. In the large functional
legacy captures the application later reaches `CFDC2124` transfer registration
and then `CFDC2138` acquisition calls.

Current next test: build/install `main` including `d51dbe2` and retest XStream.
If acquisition still stalls, the next trace must be explicitly post-`d51dbe2`;
the existing `212113` trace cannot answer that newer-state question.


## Preferred x64 capture workflow on the scope

For normal x64 regression/retest captures, the scope has a local helper
`Run-LeCroy-XStream-Trace.ps1`. Prefer it whenever the in-kernel `lecdiag`
trace contains the required evidence.

The helper updates the checkout, builds/signs/loads the selected driver
configuration, starts `lecdiag trace-capture`, launches XStream, and finalizes
the trace when XStream is closed. The local output is written below the scope
checkout as:

```text
C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver\trace-captures\xstream_trace_YYYYMMDD_HHMMSS.jsonl
```

Normal invocation from an elevated PowerShell, while the current directory is
the folder containing the helper:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
.\Run-LeCroy-XStream-Trace.ps1 -Configuration Debug
```

This remains a kernel-ring capture: output previews are limited to 128 bytes and
successful `CFDC21C0` register reads are intentionally omitted. Use the
injected user-mode XStream tracer instead when complete IOCTL output buffers or
`NtCreateFile` handle/interface mapping are required.


## Post-85FB-length retest: xstream_trace_20260927_222010.jsonl

This capture is the first x64 hardware run after
`d51dbe2f5098c2797094b7c1681bf7a2e65d8acd` changed normal raw-hardware
85FB response lengths from the hard-coded value 2 to `recordOutput - 6`.

The fix is confirmed on hardware. Family-1 opcode `0x96` now executes exactly
120 times and advances through the legacy selector sequence starting with
`0x20, 0x22, 0x24, 0x26, ...`. The first returned record contains the
expected 85FB payload length `0x0202`. The former 2473-call selector-`0x20`
loop from `xstream_trace_20260927_212113.jsonl` is gone.

The trace still contains no `CFDC2124`, `CFDC2138`, or `CFDD219F`. At the
late runtime stage it reaches the familiar 76-bit family-1 opcode-`0x42` JTAG
status request at seq 819 and repeats it 47 times with the same
`1400 3040 0000 0050 0020` response state, separated by a 10 ms delay after
six polls.

A new comparison with `legacy_xstream_trace_20260927_195608.jsonl` shows that
this JTAG state must not be treated as the acquisition blocker by itself. The
original stack also observes the `...0050...` state repeatedly and begins
`CFDC2124` / `CFDC2138` acquisition traffic while that state is still
present. A representative legacy cycle is:

```text
family2/op02 = 1
85FB status
family0/op88
CFDC2184(1)
family2/op02 = 0
family1/op42 JTAG status -> ...0050...
CFDC2124
CFDC2138
...
```

The current x64 application path instead reaches family2/op02 = 1 and then
continues polling JTAG without entering the acquisition IOCTL sequence.
`CFDC2184` itself is not a missing driver-side action: static analysis proves
that the original DeviceControl handler is an inline success/no-data branch,
and the x64 implementation already matches it.

The next comparison needs the successful `CFDC21C0` register/status traffic
between the late JTAG status result and the acquisition branch. Older x64
kernel traces omitted successful `CFDC21C0` entries by design. Commit
`ca75f36d17ba0789f818e3f9f2bd599370b58ba1` removes that suppression without
changing driver/hardware semantics. The next normal kernel capture can therefore
show those reads directly. A new injected user-mode trace is only needed if the
expanded kernel trace still cannot explain the branch.


## Diagnostic trace change: retain successful CFDC21C0 reads

Commit `ca75f36d17ba0789f818e3f9f2bd599370b58ba1` removes the early bring-up
optimization that discarded successful `CFDC21C0` register reads from the
kernel diagnostic ring. That optimization was useful when a divergent startup
path produced heavy polling, but it now hides the exact status-read sequence
needed to compare the post-`d51dbe2` x64 flow with the original acquisition
transition.

This commit changes tracing only. Register-read behavior, hardware accesses,
and all acquisition/control semantics are unchanged. Existing traces remain
valid, but successful CFDC21C0 absence in any capture made before this commit
must be treated as an instrumentation limitation.


## Trigger restart trace exposes missing interrupt-to-event delivery

Trace `xstream_trace_20260927_223910.jsonl` includes successful
`CFDC21C0` reads after the diagnostic suppression was removed.

Those reads occur only during initialization: BAR0/FVER returns `2`, and one
extended BAR1/ACQFVER read at offset `0x00C` returns `3`. There are no
successful register reads in the late Acquiring/JTAG loop, so the earlier
instrumentation gap is not the cause of the stall.

The user changed Trigger from Auto to Stop and back to Auto during this
capture. The transition is visible: XStream exits the poll, disables
family-2/opcode-`0x02`, performs another configuration sequence, re-enables
it, and returns to the identical `...0050...` 76-bit JTAG status. No
`CFDC2124`, `CFDC2138`, or `CFDD219F` is issued in either arm cycle.

Static legacy analysis identifies a missing host wakeup path in the x64
interrupt implementation. Original `FUN_000108D6` accumulates all enabled
INTST sources and queues `FUN_00011390` for every accepted interrupt.
`FUN_00011390` then performs these relevant event deliveries:

```text
INTST 0x01          -> selected transfer CompletionEvent
INTST 0x02          -> CFDC218C registered event
INTST 0x04/10/20    -> CFDC2180 registered event
INTST 0x08          -> internal RX transport event
```

The previous x64 ISR queued its DPC only for bit `0x01`, and its DPC only
signalled the transfer completion event. Before transfer registration,
`CurrentTransfer` is null, so non-transfer interrupts could be acknowledged
without ever waking the XStream event threads.

The replacement now coalesces enabled INTST sources in a pending bitmap, queues
the DPC for every accepted source, signals the proven CFDC2180/CFDC218C event
mappings, and synchronizes event-object replacement/release with a spin lock.
The bit-`0x08` internal RX event is deliberately not emulated because the
current board transport uses direct RX_CONTROL polling.

Active DMA remains disabled. The purpose of the next hardware run is to see
whether XStream finally progresses into `CFDC2124` / `CFDC2138`.


## Post-event-fix trace: standalone 85FB/0x01 status gate

`xstream_trace_20260927_225338.jsonl` changes behavior immediately after the
restored interrupt-to-event delivery. XStream no longer only waits in the late
JTAG status loop. It issues the standalone CFDC2110 packet

```text
0A0002000300FB854001
```

eleven times. Before this handler was implemented, the x64 whitelist rejected
all eleven calls with `0xC0000010 STATUS_INVALID_DEVICE_REQUEST`. XStream
then terminated acquisition setup with "Unable to arm acquisition board" and
"PCI Communication failed!".

The exact request is already present 280 times in
`legacy_xstream_trace_20260927_195608.jsonl` and 289 times in
`legacy_xstream_trace_20260927_185222.jsonl`. Every captured original call
returns:

```text
000000000400BF028000
```

This is a local driver status structure, not a firmware response. Recovered
`FUN_000169B4` proves the ten-byte layout:

```text
DWORD 0
WORD  4
WORD  command enable mask
WORD  sticky command pending mask
```

The original command object stores the complete enable word from family-0
opcode `0x85`. The DPC latches pending command bits `0x0080`, `0x0800`,
and `0x0100` from INTST sources `0x04`, `0x10`, and `0x20`
respectively, masked by that enable word. Family-0 opcode `0x88` clears the
requested sticky pending bits.

At the observed acquisition transition, the earlier opcode-`0x85` command
has programmed enable mask `0x02BF`; the event-producing interrupt latches
pending bit `0x0080`, producing the captured original response
`...0400 BF02 8000`.

The x64 implementation now mirrors that state machine rather than returning a
constant capture. The original trace then proceeds directly through
opcode-`0x88 / 0x0080`, `CFDC2184`, family-2 opcode-`0x02 = 0`, one
JTAG status request, `CFDC2124`, and `CFDC2138`.

DMA execution remains gated; the purpose of the next test is to verify that
XStream reaches the already implemented transfer registration and then the
still-disabled acquisition handler.


## Trace 230317: CAAcqDescBuilder reaches the gated CFDC2138 path

`xstream_trace_20260927_230317.jsonl` reaches real transfer registration and
buffered acquisition for the first time. XStream's new fatal dialog is:

```text
CAAcqDescBuilder::Init
FAILED to Initialize Memory! NumSeg: 40
```

The failure is directly explained by the trace rather than by an unknown
application inconsistency. `CFDC2124` successfully registers a 0x0C04-byte
WOW64 buffer and returns opaque token 1. Its following 15-byte `CFDC2138`
request asks for a one-channel 0x0C00-byte acquisition, but the x64 handler was
still deliberately returning `STATUS_NOT_SUPPORTED`.

The equivalent legacy request succeeds and returns `000C0000`.
All 1902 CFDC2138 calls in the complete 19:56 original capture use a 15-byte
one-channel request and a four-byte completed-byte output.

The replacement now enables that exact proven path. It reuses the already
validated CFDC2124 MDL/descriptor chain, programs the recovered one-channel MAM
configuration, SGTA/IIMTC, IIMCL and MAMRGO sequence, waits up to five seconds
for INTST bit 0 to signal the selected transfer event, disables transfer
interrupt bit 0 and performs the legacy IIMST/IIMCL cleanup.

Successful CFDC2124 remains a hard safety gate: source and descriptor-table
physical addresses above 4 GiB are rejected before a token is published.
Multi-channel CFDC2138 and CFDD219F remain disabled.


## Trace 235712: first CFDC2138 success and next MTT gate

The visible XStream fatal dialog remains
`CAAcqDescBuilder::Init / FAILED to Initialize Memory! NumSeg: 40`, but the
new trace proves that its cause has moved.

The first real x64 MAM DMA now succeeds:

```text
seq 739 CFDC2124 -> token 1, SUCCESS
seq 740 CFDC2138 -> SUCCESS, Information=4, output=000C0000
```

This matches the legacy four-byte completed-byte result for the 0x0C00-byte
one-channel request.

XStream then registers a second transfer with 0x404 total bytes / 0x400 data
bytes and receives token 2. Its next request is family-1 opcode `0x51`:

```text
06000A000300FBA540015100020000008000
080002000300FB854000
```

The previous x64 validated gate rejected this packet with
`STATUS_INVALID_DEVICE_REQUEST`.

The two full original runtime captures contain 279 and 286 successful
opcode-0x51 calls respectively. Their transfer-token DWORD is the legacy
registered-transfer pointer; the x64 packet correctly carries opaque token 2
in the same field. The final WORD is normally 0x0080 and once per trace 0x0180.
Every referenced transfer contains 0x400 data bytes.

Recovered `FUN_000160DC` validates that
`launch_word << 3 >= registered_data_bytes`, then
`FUN_00017478 -> FUN_000171DE` programs SGTA/IIMTC, resets the transfer
event, enables interrupt bit 0, writes IIMCL=1 and launches via BAR1 MTTRGO.
The synchronous wait is the same five-second event wait used by the MAM path.

The x64 driver now admits and executes both statically equivalent family-1
opcodes 0x50 and 0x51 with that local MTTRGO path.


## Trace 000706: calibration runs, but six completion interrupts time out

`xstream_trace_20260928_000706.jsonl` is the first capture after the local
family-1 opcode-0x50/0x51 MTTRGO path was enabled.

The earlier `CAAcqDescBuilder::Init / FAILED to Initialize Memory` fatal
error is gone. XStream advances into a long `Calibrating...` phase and issues
large amounts of acquisition and front-end traffic. The user's CH1 coupling
change does not expose a rejected IOCTL.

The trace contains 804 CFDC2138 calls: 798 succeed and six return
`0xC00000B5 STATUS_IO_TIMEOUT`. All 114 family-1 opcode-0x51 MTTRGO
operations succeed. There are no unsupported/rejected CFDC2110 requests.

Every timeout is a channel-0x30, config-0x0C00 acquisition. The requested byte
counts are 0x20, 0x20, 0x24, 0x28, 0x54 and 0x10. Each failure lasts the full
five-second legacy wait and is followed by an identical retry that succeeds
roughly 30 ms later. These six retries alone add about 30 seconds to the
calibration run.

This behavior differs from both full original captures: all 1902 CFDC2138 calls
in the 19:56 trace and all 1984 in the 18:52 trace succeed.

Static analysis provides a concrete missing completion-path operation. Original
`FUN_000108D6` writes zero to the register pointer at `main+0x3B8` whenever
INTST bit 0 is observed. Because `FUN_00014847` is instantiated at
`main+0x1E0` and places BAR0 IIMCL at subobject offset `0x1D8`,
`main+0x3B8` is definitively BAR0 IIMCL (0x048). Thus the original ISR clears
IIMCL immediately on transfer completion, before deferred event signalling.

The x64 ISR now reproduces that immediate `IIMCL=0` write. Its existing
post-wait IIMST/IIMCL cleanup remains as the later defensive cleanup path.


## Trace 002537: no DMA failures, but calibration loops due to wrong opcode-0x88 routing

`xstream_trace_20260928_002537.jsonl` contains no failed IOCTL. The ISR-side
IIMCL completion acknowledge fixed the intermittent transfer problem from the
previous run: all 1261 CFDC2138 calls and all 114 family-1 opcode-0x51 MTTRGO
calls complete successfully.

XStream still remains in `Calibrating...`, producing sustained acquisition
and front-end traffic for the remainder of the 254-second capture.

The decisive new divergence is family-0 opcode `0x88` with mask `0x0080`.
The x64 trace issues it 5660 times. Because the previous implementation
forwarded all 0x88 requests to board firmware, 352 of those calls return raw
firmware payload `0x002C`. Trace 000706 similarly exposed `0x002D`.

This does not occur on the original stack. Both complete original runtime
captures return zero for every opcode-0x88 request.

Static `FUN_00016A66` explains why: it clears the local pending mask for all
0x88 requests, then treats masks `0x0080` and `0x0800` specially. Those
two values call `FUN_00015A88(this,0)`, creating a local
`{DWORD 0, WORD 2, WORD 0}` response, and skip firmware forwarding entirely.
Other masks use the normal board transport.

The x64 driver now implements this exact split. This is the next required
hardware retest before any further calibration or DMA changes.

Type-1/type-2 record outputs were also compared because legacy captures often
show `...0400` where x64 returns zero. Static analysis shows the original
type-1/type-2 MAM-config path does not initialize those output bytes; they are
allocator residue from the temporary output pool. The x64 driver's zeroing is
intentional and should remain unless application dependence is proven.


## Trace 004553: calibration completes, then command-event source retriggers continuously

After the opcode-0x88 local-vs-firmware correction,
`xstream_trace_20260928_004553.jsonl` finally exits `Calibrating...`.
There is still no waveform.

Every captured IOCTL succeeds. The meaningful transition occurs at about
181.43 s, after the final calibration DMA transfers. XStream then repeats:

```text
85FB/0x01 -> enabled 0x02BF, pending 0x0080
family0/0x88 mask 0x0080 -> local zero response
CFDC2184(1)
```

at an extreme rate. The kernel ring records roughly 91k entries while trace
sequence numbers exceed 1.5 million by the end of the 270-second capture.

This proves that the host correctly clears local pending bit 0x0080, but the
underlying interrupt source immediately asserts again.

Static legacy ISR analysis identifies the omitted hardware acknowledge.
`FUN_000108D6` writes BAR1 CLRIRQ before its common INTST acknowledge:

```text
INTST 0x04 -> CLRIRQ 1
INTST 0x08 -> CLRIRQ 2
INTST 0x10 -> CLRIRQ 4
INTST 0x20 -> CLRIRQ 8
```

`FUN_00014847` maps CLRIRQ to BAR1 offset 0x008. The x64 ISR now reproduces
these source-specific writes. The next hardware test should determine whether
the post-calibration 0x0080 event storm disappears and XStream proceeds to
normal acquisition/waveform traffic.


## Trace 005808: first confirmed waveform acquisition on x64

`xstream_trace_20260928_005808.jsonl` is the first known-good x64 runtime
capture with **visible waveforms in XStream**.

It was captured after the BAR1 CLRIRQ ISR fix in
`6cc5a238684c217f1238aae2069950ec99a39672`.

Key results:

- 16,827 IOCTL records are captured; all return NTSTATUS success.
- 3,134 CFDC2138 acquisition calls succeed.
- Every CFDC2138 returns Information=4 and a DWORD exactly equal to the
  requested data-byte count.
- Successful acquisition sizes range from small helper/calibration buffers to
  167,936-byte transfers.
- The first CFDC2138 occurs about 18.31 s into the capture.
- A 167,936-byte CFDC2138 succeeds about 18.33 s into the capture.
- Successful CFDC2138 and family-1 opcode-0x51 traffic continues through the
  end of the capture.
- All 678 captured family-0 opcode-0x88 calls return the expected zero response.
- The post-calibration pending-0x0080/CFDC2184 event storm from trace 004553 is
  gone.

Representative successful transition:

```text
85FB/0x01 -> enabled=0x02BF, pending=0x0080
family0/0x88 / 0x0080 -> local zero response
CFDC2184
JTAG/status
CFDC2138 0x0C00 -> 0x0C00
family1/0x51 -> success
CFDC2138 0x5400 -> 0x5400
...
CFDC2138 0x29000 -> 0x29000
```

This trace changes the project phase from basic x64 acquisition bring-up to
feature/stability regression. It is the reference trace to protect against
future regressions.


## Trace 011938: normal oscilloscope controls remain stable

`xstream_trace_20260928_011938.jsonl` is a broad feature-regression capture
taken after visible waveform operation was established.

The user verified during this run that waveform amplitude/frequency look
correct and that timebase, vertical scale, coupling, bandwidth,
2-channel/10-GS/s mode switching and trigger-type changes all work.

The trace contains 156,970 IOCTL records with no non-success NTSTATUS.
43,157 CFDC2138 acquisitions complete successfully while the application
changes these operating modes. Observed acquisition channel IDs are
`0,1,2,0x30,0x31,0x32`, which demonstrates stable operation across a much
broader configuration set than the initial one-channel DMA milestone.

The trace also heavily exercises the statically recovered probe/SPI helper:
1,700 family-0 opcode-0x90 calls are present, including 1,436 selector-0x0E
144-bit transactions matching the previously recovered probe request shape.
This validates the host-to-acquisition-board SPI programming path.

At the time of this early broad-operation trace, no connected-ProBus
test was available. **Later actual AP015 tests now demonstrate successful
XStream identification, physical hotplug and an unlocked-jaw warning.**
The user additionally confirms that physical probe detection begins with
an ADC identification value, then the front EEPROM is read via I2C and
physical probe control also uses I2C. The statically recovered opcode-0x90
BAR1 SPICTL/SPIDAT/SPIDIN path is the **host-to-board SPI layer**, not
proof that the external probe bus is SPI. This Windows IOCTL trace still
does not show raw SDA/SCL, slave addresses or EEPROM fields. See
`docs/probus-detection-i2c-architecture.md` and the dated later
AP015 trace sections below.


## Developer menu: Run Link Tests currently rejected by XStream

On the known-good x64 waveform baseline, XStream's developer menu action
`Run Link Tests` currently logs `WaveRunner Driver Not Supported`.

The replacement already implements `CFDC21C8` and returns legacy build 1002,
so the visible rejection is not explained by the public build number alone.
No trace has yet been captured around the menu action, so it is not known
whether XStream sends a link-test IOCTL and receives an unsupported result, or
rejects the replacement entirely in user mode before touching the driver.

The next required evidence is a passive x64 trace containing one deliberate
`Run Link Tests` invocation. Preserve the current working waveform baseline
while investigating this.


## Trace 013644: Run Link Tests is rejected before a driver link-test request

XStream's Developer -> Run Link Tests requires acquisition to be stopped.
`xstream_trace_20260928_013644.jsonl` captures the action in that required
state.

XStream logs:

```text
WaveRunner Driver Not Supported
```

The kernel trace contains 572 IOCTL calls and all succeed. There is no new
IOCTL code, no unsupported CFDC2110 packet and no driver error around the
attempt. The stopped interval contains only existing helper calls
(`SET_FLAG_BYTE`, `DELAY_MS`) plus the known family-1 opcode-0x42 JTAG
status polling.

Notably, XStream does not issue `CFDC21C8` during this captured session.
Therefore the developer-menu rejection is not directly caused by the
replacement's public build-query response 1002 at click time.

The current evidence strongly favors an XStream-side driver-type/capability
gate that prevents the actual link test from being dispatched. Investigation
should move to the XStream user-mode binary containing the literal message
rather than changing working PCI, DMA or interrupt semantics.


## Trace 014500: Revision-page PCI communication error is family-1 A1/A2

Opening Service -> AladdinAcqBoard -> Revision produces
`HardwarePCI Communication error!` on the pre-fix x64 driver.

The trace contains exactly 11 failed IOCTLs. All are CFDC2110 requests for
family-1 opcodes 0xA1 or 0xA2; all other calls succeed.

Recovered legacy semantics:

```text
family1/0xA1 -> BAR1 ACQFVER (0x00C)
family1/0xA2 -> BAR0 FVER    (0x000)
```

Each is a local host-side register read and installs a 12-byte pending response:

```text
DWORD 0
WORD  6
WORD  status
DWORD register_value
```

The x64 driver now implements this exact behavior. The Revision page is the
next focused regression test.

The user also located the Developer-link-test rejection string in
`lecaladdinhwaccesspcisvr.dll`, making that DLL the next user-mode analysis
target for `WaveRunner Driver Not Supported`.


## Run Link Tests final disposition: vendor-disabled for S65/WaveRunner

Static analysis of the installed `lecaladdinhwaccesspcisvr.dll` explains the
developer-menu message conclusively.

The link-test routine checks two booleans in its embedded driver-connection
object:

```text
connected?
    no  -> "Driver Not Connected"
S65/WaveRunner driver?
    yes -> "WaveRunner Driver Not Supported"
else
    -> execute link tests
```

The constructor for that connection object recognizes `Null`, `S65`,
`FE2` and `CENTAUR`. Selecting `S65` sets the exact flag tested by the
link-test routine.

This means the rejection is intentional LeCroy user-mode behavior for the S65
driver family and is not caused by the 64-bit replacement driver. No new IOCTL
needs to be added for this menu action.

Forcing the flag off is unsafe as a compatibility strategy: the same flag
selects alternate IOCTL/driver paths elsewhere in the DLL, including a
`0xCFDC219C` versus `0xCFDD219F` branch.

The installed DLL was analyzed locally only and must not be added to the public
repository.


## 2026-09-28 controlled calibration and ProBus A/B comparison

The user corrected the earlier calibration concern. The apparent repetition
comes from one initial calibration per vertical voltage step, with later
visits reusing the step. Ch2 was swept 20 mV/div to 100 V/div in matching
legacy and replacement-driver runs.

The two calibration captures contain zero failed relevant driver IOCTLs.
Family-1 0x96 occurs 120 times in each; family-1 0x81 occurs 61 times in each.
Family-0 0x90 selector 0x0E occurs 1,147 times (legacy) versus 1,181 times
(x64); 560 versus 570 are the common idle-frame pattern. Differences in total
trace lengths prevent an exact cycle-count or timing conclusion. The previous
claim of a confirmed excessive-calibration regression is superseded.

The separate ProBus hotplug captures identify a much sharper compatibility
gap. Legacy standalone 85FB/0x01 status returns enabled=0x02BF,
pending=0x0200 at seq 14552, 14588 and 14616. These prompt family-0/0x88
mask 0x0200 acknowledgements, family-1/0x82 probe queries (three calls), then
family-0/1 opcode 0x4A metadata and control traffic. The returned metadata
contains ASCII `AP015`.

On x64, all 679 standalone status reads report pending=0x0080, never 0x0200.
Consequently no family-1/0x82 or family-0/1 opcode-0x4A commands appear, and
the plugged probe is not recognized. Yet the x64 trace issues 195 SPI-selector
0x0C transactions versus 75 in the legacy capture, proving the host still
attempts the surrounding probe-control/polling workflow.

No captured relevant IOCTL fails in either ProBus run. The missing behavior
precedes actual probe identification, sensitivity, Degauss and Auto Zero.
It must not be fixed by fabricating 0x0200 or by invoking 0x82 proactively.

The same exact family-1/0x42 JTAG request yields a recurring steady-state
difference in both A/B pairs: output byte index 17 can be legacy 0x20 versus
x64 0x32 (XOR difference 0x12). The causal relation to ProBus is unknown.

Full packet and count details are recorded in
`docs/probus-calibration-ab-comparison.md`.


## Trace 193741: ProBus works at XStream startup, hotplug still missing

`xstream_trace_20260928_193741.jsonl`: the user started x64 XStream with
an AP015 probe already connected and XStream recognized it. Degauss and Auto
Zero were subsequently invoked. All 21,004 captured IOCTLs returned success.

At seq 505 family-0/0x4A probe setup succeeds. At seq 508 family-1/0x4A
returns AP015 metadata (Information=270). The captured first 128 output bytes
are identical to legacy seq 14596 / 14812. Therefore the actual startup
probe-communication pathway works, rather than merely the user interface
displaying a remembered probe.

Unlike original hotplug, neither x64 run ever shows 85FB pending 0x0200.
The preconnected x64 session does not require that notification and performs
successful discovery directly via 0x4A without a family-1/0x82 request.
The issue is therefore narrowed to hotplug/unsolicited notification rather
than a globally nonfunctional probe bus.

A separate protocol discrepancy remains during subsequent probe control:
the identical family-0/0x4A `...4700` request returns
`0000000000000000000002000000FFFF` in the original reference but
`00000000000000000000040000000000` on x64 (repeated five times).
All IOCTL NTSTATUS values are success; payload-level equality is not proven.
Follow-on family-1/0x4A replies also vary (`F2` versus `F7` in selected
responses). Keep Degauss/Auto Zero result equivalence separately open.

Legacy `FUN_00011390` consumes INTST 0x08 through internal receive/
probe-ring handling; x64 presently acknowledges INTST 0x08 but does not
deliver that internal RX event because normal replies are polled
synchronously. This is now a prioritized candidate for the lost asynchronous
probe-insertion path, not a proven source of pending 0x0200. See the dedicated
ProBus comparison document.


## Trace 213834: repeated ProBus reinsertion disrupts visible acquisition

The user started x64 XStream with a connected AP015, unplugged it, observed
that XStream did not detect removal, then reinserted it and the waveform
disappeared.

The capture contains 36,700 IOCTL records across 73.143 s and zero failed
NTSTATUS results. All 6,987 CFDC2138 calls return the requested byte count;
DMA traffic continues to the end. All 1,613 standalone 85FB/0x01 queries
return enabled 0x02BF and pending 0x0080, never 0x0200. Only the startup
family-0/1 0x4A exchanges (seq 509/511) appear; no further 0x4A or 0x82
is dispatched on physical probe removal/reinsertion.

At t~27.1 s the captured traffic changes: the last >=8192-byte acquisition
is seq 10912 at 27.087873 s, and the last SPI opcode-0x90 is seq 10937 at
27.09982 s. Repetitive small channel-0 1,024-byte and channel-0x30..0x32
2,048-byte DMA transfers subsequently continue successfully. The identical
mode-1/0x4C JTAG query changes from `...144030...` at seq 10886 to
`...144032...` at seq 11016. This is a state change, not a driver-reported
timeout; user-action timestamps are unavailable.

After analyzing this trace, the legacy raw DPC assembly yields a direct fix:
`FUN_0001619A -> FUN_000160A8(1)` enables global INTEN bit 0x08 before the
real response fetch. Original `FUN_00011390` handles an INTST 0x08 event by
calling `FUN_000176A2(transport, &word)` to read/clear BAR1 HWInt 0x410.
Its returned low 16-bit word is then given to `FUN_000157A6` to set
`commandPending |= commandEnabled & word`, waking the command-status
event. The earlier C decompilation misleadingly showed a zero argument,
whereas raw asm 0x114A2..0x114C8 proves it is the HWInt out-parameter.

The x64 patch restores both missing parts without changing the existing
synchronous polling of solicited RX data or the proven DMA paths. A focused
post-patch hardware run must check spontaneous pending 0x0200, subsequent
0x88/0x82/0x4A flow, uninterrupted acquisition and absence of event storms.


## 2026-09-28 trace 230614: HWInt/ProBus hotplug works after patch 70716ba

User procedure: AP015 connected before XStream startup, change to higher
A/div, remove AP015 once, reinsert once, close XStream. User reports all
changes were recognized normally. This is the first real-hardware run of
the restored original INTEN `0x08` / INTST `0x08` / BAR1 HWInt `0x410`
command-status path (driver commit `70716ba`). The capture
`xstream_trace_20260928_230614.jsonl` is user-supplied and not public.

**Trace hygiene:** 19,288 `type=ioctl` entries with seq range 1..21761;
2,473 sequence entries are missing in 30 inter-snapshot gaps, so counters
refer only to captured calls. All captured NTSTATUS fields are success.
14,742 CFDC2110 calls, 3,633 CFDC2138 calls, zero DMA requested/result
byte-count mismatches, and 720 standalone 85FB/0x01 reads. All 720 read
command-enabled `0x02BF`; 718 report pending `0x0080` and two report
real pending `0x0200`:

```text
~28.422s  seq 14951  85FB standalone: 000000000400BF020002
~28.449s  seq 14952  family-0/0x88 mask 0x0200, success
~28.480s  seq 14961  family-1/0x82, response Information=412
           no subsequent 0x4A in this first event sequence (removal)
~31.275s  seq 16639  85FB standalone: 000000000400BF020002
~31.299s  seq 16640  family-0/0x88 mask 0x0200, success
~31.330s  seq 16651  family-1/0x82, response Information=412
~31.469s  seq 16678  family-0/0x4A setup, success
~31.515s  seq 16679  family-1/0x4A reply, Information=270, AP015
```

Elapsed seconds use the previously established 10-MHz trace-tick conversion
from the first recorded IOCTL; exact physical-action timestamps were not
recorded separately. The first and second events correspond to unplug and
replug respectively according to the user's stated order and the packet
sequence. Subsequent standalone status reverts to pending `0x0080`, not a
repeated `0x0200` storm. The startup AP015 `0x4A` reply (seq 509) and
post-reinsertion reply (seq 16679) both have Information 270 and the
same captured first 128 output bytes; this prefix also matches the
original-driver legacy trace metadata at seq 14596/14812.

DMA distribution across the two pending events: 2,332 before event 1;
328 between them; 973 after event 2, continuing through trace end
(~39.58 seconds). The closest measured DMA gaps crossing those events are
~36 ms and ~397 ms. The last observed >=8192-byte transfer occurs at
~27.067 seconds, preceding the first pending-0x0200 event; later
transfers are predominantly 1024/2048 bytes. The preceding higher-A/div
user action is consistent with an earlier acquisition-mode transition,
but exact cause/timing should not be invented from the trace. DMA return
status alone is not visual evidence of waveform pixels; the user's visual
observation confirms normal probe-state recognition. CPU use and raw ISR/
DPC rates are not recorded by this IOCTL-only trace.

**Milestone:** The first real-hardware regression shows that patch
`70716ba` restores the missing asynchronous AP015 command notification
and allows natural XStream handling of disconnect/reconnect through
pending-0x0200 / 0x88 / 0x82 / 0x4A. This is distinct from the still-open
Degauss/Auto Zero opcode-0x4A `...47 00` response-parity problem, which
was not exercised in trace 230614. No new PCI, DMA, polling or fake-pending
behavior was introduced. See `docs/probus-calibration-ab-comparison.md`.


## Trace 231656: ProBus jaw transitions and special-function reply framing

User action sequence: XStream with AP015 already attached; Degauss followed
automatically by Auto Zero; open clamp; close; open; close in locked
position. Captured x64 `xstream_trace_20260928_231656.jsonl`, running
the **verified HWInt driver** before response-format patch `3490709`.
User actions were not timestamped separately.

The 68.178-second capture has 35,458 recorded IOCTLs (sequence range
1..39998; 4,540 sequence entries were omitted in 39 snapshot gaps).
All captured NTSTATUS fields are successful. CFDC2110 occurs 27,100
times; CFDC2138 occurs 6,701 times. All 6,701 output DWORDs match the
requested bytes at **CFDC2138 input offset 11**; no recorded DMA byte-count
error. The last observed DMA is at ~68.177 s.

The 1,438 standalone command-status 85FB/0x01 reads all report enabled
0x02BF: 1,432 pending 0x0080; four pending 0x0200; one pending
0x0280; one pending 0x0000. There are five genuine notifications with
bit 0x0200 set, each followed by family-0/0x88 ack and family-1/0x82:

```text
t=34.163  seq 19902  pending=0200 -> 19903 ack 0200 -> 19916 0x82: 12 00 A7 00
t=41.812  seq 24430  pending=0280 -> 24431 ack 0280 -> 24443 0x82: 12 00 58 00
t=57.832  seq 33777  pending=0200 -> 33778 ack 0200 -> 33787 0x82: 12 00 A7 00
t=59.820  seq 34908  pending=0200 -> 34909 ack 0200 -> 34918 0x82: 12 00 58 00
t=61.894  seq 36079  pending=0200 -> 36080 ack 0200 -> 36089 0x82: 12 00 A7 00
```

The first event follows Degauss while the clamp remained closed. The
remaining `58/A7/58/A7` alternation is consistent with the reported
open/close/open/locked-close sequence. The exact status-bit definition
and whether `locked` needs any other register are not proven. After
the combined 0x0280 ack, seq 24437 reports pending 0 and seq 24438
issues a mask-0 ack. No obvious repeating 0x0200 loop is recorded.

The Degauss-related family-0/0x4A `...47 00` request repeats five
times (seq 19883..19887), every output ending
`...040000000000`, followed by a family-1/0x4A status `...F700`.
Automatic `...47 12` appears in five bursts of five calls: the initial
Auto Zero seq 19938,19939,19941..19943 (subsequent status F100), plus
a burst after each of the four jaw events (F700, F300, F700, F300).
Identical legacy original `47 00` and `47 12` requests previously
returned `...02000000FFFF`; the legacy family-1/0x4A status was
`...F200` in those reference cases.

A source audit identifies a **host-side serialization defect** independent
of HWInt and DMA: original `FUN_000167F4` fills its solicited raw
response allocation with 0xFF and writes actual bytes received into
header WORD +4, whereas current `driver/Ioctl.c` advertises the
requested payload capacity and leaves zero padding. This also explains
the x64 family-1/0x82 response `...90010000...` (reported 0x0190)
versus legacy `...0600...` / `...1600...` with FF-padded tails.
A minimal raw 85FB output-format patch was committed as `3490709`
**after** this trace. It neither injects pending bits nor touches
BAR, MMIO, synchronous RX logic, or DMA. The patch still awaits a
compile and real-hardware regression. Do not infer physical Degauss or
Auto Zero success solely from successful IOCTL NTSTATUS.


## Trace 233125: post-framing Degauss/manual Auto Zero; no jaw HWInt observed

The scope's first test of driver commit `3490709` includes startup with
AP015 preconnected, Degauss, subsequently **manual** Auto Zero, multiple
physical open/close movements, and XStream exit. Captured JSONL:
`xstream_trace_20260928_233125.jsonl`. Duration 55.628 seconds from
first to last record; 27,947 captured IOCTLs, seq 1..30856; 2,909 entries
omitted in 29 trace-snapshot gaps (last gap ends at t=28.646 s).
Every captured NTSTATUS is successful. 21,366 CFDC2110 and 5,219
CFDC2138 calls; all 5,219 successful DMA replies carry Information=4
and match requested bytes at CFDC2138 input offset 11. Last recorded
DMA at t~55.627 s, request and return both 1,024 bytes. 1,666
DMA calls follow the second 47 00 control request.

```text
seq 507    ~13.797 s   startup family-1/0x4A AP015, Information=270
seq 8573   ~20.034 s   family-0/0x4A ...47 00 -> 0000000000000000000002000000FFFF
seq 8574   ~20.066 s   family-1/0x4A ...01 0A -> 0000000000000000000004000000F300
seq 22156  ~40.675 s   family-0/0x4A ...47 00 -> 0000000000000000000002000000FFFF
seq 22158  ~40.706 s   family-1/0x4A ...01 0A -> 0000000000000000000004000000F300
```

The two separate 47 00 requests correlate with Degauss and manually
triggered Auto Zero, in that reported order (individual hand actions were
not timestamped independently). This is the first real-hardware proof
that the source-correct `FUN_000167F4` actual-received-length header
and 0xFF output padding introduced by `3490709` produce byte-identical
47 00 results to original x86 seq 24205. The old x64 `231656`
produced five identical requests in a burst with the wrong result
`...040000000000`. This time there is no such retry burst. The new
capture contains no 47 12, so **manual** Auto Zero cannot universally be
identified with 47 12; earlier 47 12 bursts followed automatic/probe
event handling. It also has no 0x82 packet, so 0x82 framing parity is
not individually re-tested here.

The separate returned firmware/status payload remains **F3** on both
family-1/0x4A `...01 0A` status requests, compared with F2 in the
historical x86 reference. That difference is not caused by the previously
incorrect raw 85FB header length and FF padding, and has no proven
interpretation or physical calibration outcome yet.

**Jaw-event caveat:** 1,135 standalone status 85FB/0x01 calls all report
enable mask `0x02BF` and pending **only 0x0080**. No pending
0x0200/0x0280, no family-1/0x82 and no opcode-0x88 mask-0x0200 ack
were captured, despite the user's stated open/close operations. The
capture has **447 standalone status reads after t=40 s and zero
snapshot gaps after t=28.646 s**. The first post-HWInt hardware tests
`230614` and `231656` did produce spontaneous pending 0x0200 and
proper 0x82 follow-up. Do not claim the formatter patch directly
disabled HWInt: it changes only host response serialization. The absence
is a real behavioral observation warranting one jaw-only controlled
baseline before any additional driver change. Raw interrupt/CPU rate
and screen-visible clamp state are not captured by JSONL.

More details, exact legacy comparison, and next test:
`docs/probus-calibration-ab-comparison.md` and
`docs/next-chat-handoff.md`.


## Trace 235314: pre/post-calibration jaw actions still yield no 0x0200

User performed the exact controlled post-`3490709` protocol described
in the previous handoff: XStream start with AP015 preconnected; jaw
open/hold/close/hold **before** Degauss; Degauss; manually trigger Auto
Zero; another open/hold/close/hold pair; close XStream. Private trace
`xstream_trace_20260928_235314.jsonl`. Separate clock timestamps
for physical movements and explicit visual jaw-state observations were
not included in the user report.

The capture spans **72.009226 s**, has **36,366** IOCTL entries and
sequence range 1..42,227. Its 36 snapshot gaps omit 5,861 sequence
numbers; the final one ends at **t~48.479610 s**, so the whole
post-Auto-Zero period at t>=54.337 s is captured without gaps.
No captured NTSTATUS failure. CFDC2110: 27,834. CFDC2138: 6,818.
All 6,818 DMA output DWORDs equal their requested input-offset-11
byte counts (including the last 1,024-byte return at t~72.008570 s).

All **1,517 standalone 85FB/0x01** calls returned
`000000000400BF028000`: enable 0x02BF, pending **0x0080**
(no 0x0200 and no 0x0280). No family-1/0x82 and no family-0/0x88
mask 0x0200 appear. Apart from initial setup masks 0xFFDF and
0x001F, the captured family-0/0x88 acknowledgements are 1,521
repetitions of mask 0x0080. The physical jaw phase split can be
bounded by the two user-action-correlated 47 00 requests:

```text
t~17.084      seq   508 AP015 recognized (family-1/0x4A), Information=270
t17.000..40.156       622 status reads, 0 events containing 0x0200,
                       3,207 successful DMA replies
t~40.155959   seq 23386 family-0/0x4A ...47 00:
                       0000000000000000000002000000FFFF
t~40.186      seq 23388 family-1/0x4A ...01 0A:
                       0000000000000000000004000000F300
t40.156..54.337       381 status reads, 0 events containing 0x0200,
                       1,622 successful DMA replies
t~54.336670   seq 31835 family-0/0x4A ...47 00:
                       0000000000000000000002000000FFFF
t~54.368      seq 31836 family-1/0x4A ...01 0A:
                       0000000000000000000004000000F300
t54.337..72.009       514 status reads, 0 events containing 0x0200,
                       1,989 successful DMA replies
t48.480..72.009       648 status reads with zero snapshot gaps
```

The two 47 00 results are byte-for-byte the original x86 response
and do not show the earlier fivefold retry burst. The corrected
startup family-1/0x99 output also matches the original x86's
captured first 128 bytes, including unused 0xFF padding:
`0000000000000000000002000200FFFFFFFF...`.
The pre-`3490709` x64 raw wrapper had wrongly zero-padded that
part. AP015 startup metadata seq 508 has Information 270 and the
captured first 128 bytes match `231656`. The two later follow-up
firmware-status replies are both F3 versus historical x86 F2;
that state-bit meaning is not established.

**Inference:** Degauss/Auto Zero cannot be a necessary cause of
the absence of jaw events in this run, because the first deliberate
open/close pair occurred before Degauss and 622 associated status
reads already lack 0x0200. The same absence also holds in 381
intermediate and 514 post-manual-Auto-Zero reads. This repeats the
post-`3490709` zero-event behavior in `233125`, whereas the
earlier HWInt patch plus old raw wrapper in `231656` captured five
real pending-0x0200/0x88/0x82 notifications. Temporal correlation
is not proof that the host-only formatter directly disables board
interrupts.

**Do not mistake standalone software-enable `0x02BF` for the
physical BAR0 `INTEN[0x08]` bit.** The actual BAR0 INTEN DWORD
(offset 0x084), BAR0 INTST (0x080), BAR1 HWInt (0x410), and raw
ISR/DPC counters are not present in IOCTL-only JSONL. Next minimally
discriminating unchanged-driver test: AP015 physically unplug/replug
once without any calibration or jaw-switch action. If that no longer
produces pending 0x0200, investigate physical INTEN/INTST/HWInt
with bounded passive instrumentation; otherwise inspect the
jaw-specific trigger/state. Preserve known-good acquisition,
no synthetic pending flags, <=5% CPU. Full context in
`docs/probus-calibration-ab-comparison.md` and
`docs/next-chat-handoff.md`.


## Trace 001152 (2026-09-29): four physical hotplug cycles still generate HWInt after response-format fix

User procedure on unchanged post-3490709 driver: unplug/replug AP015
several times, no separate Degauss/Auto Zero exercise. The first
reconnection was reportedly displayed temporarily as a different
"1/2 clamp" type, perhaps due to connector seating; subsequent
reconnections were recognized correctly. Private JSONL
`xstream_trace_20260929_001152.jsonl`, ~61.381146 seconds.
32,675 captured IOCTLs (seq 1..37623), 34 snapshot gaps omitting
4,948 entries; last gap ends t=32.792085 s. All captured NTSTATUS
success. CFDC2110=24,976, CFDC2138=6,135; all requested-vs-returned
DMA byte counts match (requested DWORD at input byte offset 11).
The last DMA at seq 37611, t~61.380523 s, returns the requested
1,024 bytes, and 1,062 DMA calls occur after the last insert event.

Standalone 85FB/0x01: 1,361 captured queries, software-enable mask
0x02BF throughout. Observed pending masks: 1,348 x 0x0080,
11 x 0x0200, 1 x **0x0280**, 1 x 0x0000. Thus twelve genuine
0x0200-bearing transitions, each with family-0/0x88 ack
(11 x mask 0x0200, 1 x mask 0x0280) and family-1/0x82.
Nine transitions occur **after** the final snapshot gap.
The earlier concern about a global post-3490709 HWInt regression
is NOT supported by this physical-hotplug test.

```text
time s   pending seq / mask  ack seq / mask   family-1/0x82 seq / raw state   correlated action
27.557   16747 / 0200       16748 / 0200    16754 / 03FF; raw length 000E   unplug 1
31.662   19245 / 0200       19246 / 0200    19248 / 028C; raw length 0006   reconnect 1
31.718   19249 / 0280       19250 / 0280    19257 / 0058; raw length 0006   additional first event
40.564   25308 / 0200       25309 / 0200    25318 / 03FF; raw length 0006   unplug 2
42.336   26308 / 0200       26309 / 0200    26318 / 00A9; raw length 0006   reconnect 2
42.386   26320 / 0200       26321 / 0200    26325 / 0058; raw length 0006   additional event
44.929   27775 / 0200       27776 / 0200    27782 / 03FF; raw length 0006   unplug 3
47.306   29173 / 0200       29174 / 0200    29183 / 00AA; raw length 0006   reconnect 3
47.367   29187 / 0200       29188 / 0200    29193 / 0058; raw length 0006   additional event
49.510   30380 / 0200       30381 / 0200    30390 / 03FF; raw length 0006   unplug 4
52.275   32018 / 0200       32019 / 0200    32021 / 00A9; raw length 0006   reconnect 4
52.405   32062 / 0200       32063 / 0200    32065 / 0058; raw length 0006   additional event
```

After first reinsertion, 0x82 returns the distinctive transient
0x028C then 0x0058 and XStream does **not** query the 270-byte
AP015 metadata (unlike later reinsertions). This is correlated
with the user's ambiguous/wrong first probe display but cannot
establish that 0x028C formally encodes "1/2 clamp". The next three
reinsertions do query family-0/1 0x4A AP015 metadata:

```text
startup  seq 509
replug2  seq 26342 (setup), 26343 (metadata)
replug3  seq 29207 (setup), 29208 (metadata)
replug4  seq 32067 (setup), 32068 (metadata)
```

All four metadata replies return Information=270 and share
byte-identical captured **128-byte prefixes**, including "AP015".
The remaining 142 bytes were not exposed in the JSONL. Raw 0x82
formatting introduced in commit 3490709 is now hardware-tested:
the first disconnect reply reports actual 0x000E (=14) and
11 subsequent replies actual 0x0006 (=6), all with 0xFF
unused bytes, rather than old fixed-capacity 0x0190/zero padding.

After each normal successful reinsertion, the same family-0/0x4A
`47 12` packet is sent exactly ONCE: seq 26385, 29250,
32096. All three full result records match original x86:
`0000000000000000000002000000FFFF`; subsequent status
family-1/0x4A `01 0A` at seq 26386, 29251, 32099 returns
`0000000000000000000004000000F700` each time.
The old fivefold 47 12 retry burst before the 85FB fix is absent.
Firmware meaning/physical calibration of F7 remains open.

**Decision:** With unchanged 3490709 formatting, physical hotplug
does still generate authentic pending-0x0200, 0x88/0x82 and repeated
AP015 metadata; keep the source code intact. The two previous
`233125`/`235314` jaw-only runs still lack 0x0200 and are
a **jaw-specific unresolved question**, not proof of globally
lost INTEN-0x08. Next useful contrast: one jaw-only open/hold/
close/hold pair before and after ONE successful physical
disconnect/reconnect, with no Degauss/Auto Zero. Record the displayed
state and manual relative action times. Continue <=5% CPU and
preserve stable acquisition/PCI/DMA; never synthesize pending bits.
See `docs/probus-calibration-ab-comparison.md` for the full A/B.


## Trace 002051 (2026-09-29): user-visible jaw unlock IS recognized

**Correction of interpretation:** The user explicitly confirms XStream
displays an unlocked-clamp warning when AP015 is opened, explaining
that the measurement may be inaccurate. It was incorrect to conclude
from earlier `233125` and `235314` zero captured pending-0x0200
events that XStream did not recognize jaw changes. Missing an event
in those particular kernel IOCTL snapshots is not a user-interface
observation. The new real-scope
`xstream_trace_20260929_002051.jsonl` captures genuine jaw-open
and jaw-closed status events under the same corrected
post-`3490709` 85FB serializer.

Quantities: 31,200 captured IOCTL records in sequence 1..35955,
58.3684694-second duration, 32 trace-snapshot gaps omitting
4,755 entries, last gap ends ~23.382490 s. All four command
notifications below occur AFTER that gap. No captured NTSTATUS
failure; 23,896 CFDC2110 and 5,839 CFDC2138 calls. All DMA
outputs return requested DWORD from input byte offset 11;
last transfer at ~58.367987 s, 1,024/1,024.

Standalone 85FB/0x01: 1,292; software enable `0x02BF`;
pending `0x0080` x 1,287, `0x0200` x 3, `0x0280` x 1,
`0x0000` x 1. Genuine 0x0200-containing events with matched
family-0/0x88 and family-1/0x82:

```text
t=24.623  status seq 14888 pending 0200; ack seq 14889 mask 0200;
          0x82 seq 14891 actual raw length 0006,
          raw 000012005800 = state WORD 0058 (jaw opened).
          0x4A 47 12 seq 14913 -> ...02000000FFFF;
          0x4A status seq 14916 -> ...F700.

t=35.038  status seq 21435 pending 0280; ack seq 21436 mask 0280;
          intermediate status seq 21442 pending 0000, ack seq 21443;
          0x82 seq 21448 raw 00001200FE03 = state WORD 03FE.

t=37.458  status seq 22833 pending 0200; ack seq 22834 mask 0200;
          0x82 seq 22843 raw 000012005700 = state WORD 0057;
          family-0/1 0x4A AP015 re-identification
          seq 22877 / 22878, Information=270.

t=49.861  status seq 30553 pending 0200; ack seq 30554 mask 0200;
          0x82 seq 30563 raw 00001200A700 = state WORD 00A7
          (jaw closed).
          0x4A 47 12 seq 30580 -> ...02000000FFFF;
          0x4A status seq 30581 -> ...F300.
```

Every new family-1/0x82 output has header raw length `0x0006`
and 0xFF unused bytes, rather than the pre-`3490709` fixed
capacity/zero padding. Startup metadata seq 506 and post-reconnect
seq 22878 have identical captured 128-byte AP015 prefixes,
both with Information=270. These are physical/firmware event
correlations, not independent clock timestamps of each hand action.
The 0x03FE/0x0057 words may encode a state different from
the earlier 0x03FF/0x0058 observations, but their bits are not
yet statically decoded.

**New reliable functional correlation** with user report and older
pre-formatter trace `231656`:

```text
open/unlocked : family-1/0x82 0x0058 -> family-1/0x4A F7
closed        : family-1/0x82 0x00A7 -> family-1/0x4A F3
```

The final status words differ by bit `0x04`; it is a plausible
mechanical-status indicator, but do not assert the vendor's field
definition without further raw-protocol reconstruction.
The user-visible XStream warning corroborates the probe jaw
recognition. **There is no demonstrated general regression in
jaw-state handling, HWInt or DMA from the raw 85FB formatter patch.**
Previous 233125/235314 remain valid observations of absent
capture-visible 0x0200 only, not proof of an invisible UI warning.
No new driver change or another generic hotplug-only test is
indicated by this trace. Full context and historical correction:
`docs/probus-calibration-ab-comparison.md`,
`docs/next-chat-handoff.md`.


## Physical ProBus bus-layer distinction (user hardware update, 2026-09-29)

The user confirms that ProBus **physical** identification and
control are a staged process: an ADC identification value
first establishes ProBus-class presence; the front-panel
EEPROM is subsequently read over **I2C**; probe control also
occurs over **I2C**. These are hardware-layer facts provided
by the user, not something the current Windows IOCTL JSONL
can directly decode.

The capture records the XStream-to-driver and driver-to-board
high-level protocol. It does **not** include raw analog
samples/ADC channels or I2C SDA/SCL edges, slave addresses
or EEPROM byte dumps. The separately source-proven A5FB
family-0/0x90 local BAR1 SPICTL/SPIDAT/SPIDIN operation
is **not** proof that the probe's physical electrical bus
is SPI, nor is its relationship to the front I2C bus
yet mapped.

In particular:

- A family-1/0x4A result containing ASCII `AP015`
  verifies the metadata XStream received; even with the
  user's EEPROM-first identification information, the
  exact 0x4A-to-EEPROM transaction and field correspondence
  cannot be reconstructed from this JSONL alone.
- Standalone 85FB/0x01 pending `0x0200`, 0x88 ack,
  and family-1/0x82 are genuine firmware notification/
  status **at the host boundary**. They are not direct
  timestamps for a physical ADC sample, EEPROM transaction
  or individual I2C sensor read.
- The first anomalous physical AP015 reconnection in
  `001152` has `0x82=028C -> 0058` and no ensuing
  270-byte AP015 metadata. That does not identify a bad
  ADC threshold, failed EEPROM read, invalid EEPROM
  contents or exact incomplete-contact cause without
  additional physical/firmware evidence.
- The user's visible XStream unlocked-jaw warning and
  `002051` `0058/F7` (open) versus `00A7/F3`
  (closed) packets do establish high-level functioning
  jaw detection. Their physical I2C command/register
  mapping is still not recovered.

Keep raw host trace interpretation separate from
electrical-bus inference; no driver source changes are
justified by this architecture update alone.
Full description:
[`probus-detection-i2c-architecture.md`](probus-detection-i2c-architecture.md).


## 2026-09-29: Dallas write ioctl not implemented in native x64 (011858)

The user's private `xstream_trace_20260929_011858.jsonl` captures
a license Delete action in XStream. The entry remains visible
after application restart. **608** IOCTLs were recorded,
seq 1..608, duration **63.0224566 s**, no missing sequences.
Exactly **one** IOCTL failed:

| Sequence | Relative time | Control | Input/Output | Result |
|---:|---:|---|---:|---|
| 1 | 0.000 s | GET_DALLAS_ID `0x00223080` | out 8 | success, Information 8 |
| 3 | 0.804 s | READ_DALLAS_MEMORY `0x00223084` | out 512 | success, Information 512 |
| **522** | **57.303 s** | **WRITE_DALLAS_MEMORY `0x00223088`** | **in 512, out 0** | **`0xC0000010` STATUS_INVALID_DEVICE_REQUEST, Information 0** |
| 523 | 57.824 s | READ_DALLAS_MEMORY `0x00223084` | out 512 | success, Information 512 |
| 607 | 63.022 s | GET_DALLAS_ID `0x00223080` | out 8 | success, Information 8 |

The two READ previews (first 128 bytes only) are identical.
The attempted whole-image WRITE and the initial READ
differ at 93 byte offsets within the shared first 128
preview bytes, affecting memory pages 0..3.
No complete WRITE payload or complete post-operation READ
content is available from this recorder preview.

The current `driver/Ioctl.c` has handlers for Dallas ID
and READ, but no WRITE `0x00223088`; its dispatch
defaults to `STATUS_INVALID_DEVICE_REQUEST`.
This explains the unsuccessful XStream action and demonstrates
the next native x64 ABI feature to recover.
Original x86 handler VA is documented as `0x11F54`;
its C/ASM/XREF are now requested by
`ghidra_scripts/targets.txt` for review before any
new driver write code.

**Privacy:** current uploaded trace stores Dallas input/output
hex previews that may include sensitive user data; do not
reproduce its raw contents in public docs. The diagnostic
program's newly updated JSONL exporter now omits Dallas
WRITE input and READ/ROM-ID output previews, while preserving
metadata and status. This requires rebuilding `lecdiag` and
does not modify already-created traces or the kernel.


## Proposed live GUI monitor

For interactive feature correlation, a native user-mode `lecwatch` GUI is
proposed on top of the existing read-only debug trace ring. It should show
recently observed IOCTLs as activity indicators, maintain a live log, support
"hide known" filtering, operator action markers, action-window comparison and
idle-baseline suppression. No kernel change is required for the first version.

Important: the current trace entry is recorded after IOCTL completion, so the
UI must call an illuminated indicator "recently observed", not literally
"currently executing". Because `0xCFDC2110` multiplexes nested commands,
semantic filtering should operate on decoded subcommands as well as the
top-level IOCTL where possible.

Full design: [live IOCTL monitor](live-ioctl-monitor-design.md).


## Native live monitor implementation

A native x64 GUI named `lecwatch` is now implemented in source on top of the
same private trace ring used by `lecdiag trace-capture`. It does not require a
kernel change or driver reload.

Build:

```powershell
.\scripts\build-lecwatch.ps1
```

Run:

```powershell
.\tools\lecwatch\build\lecwatch.exe
```

The monitor polls every 100 ms on a worker thread, starts at the current
sequence rather than replaying stale ring contents, reports sequence gaps,
shows recent IOCTL activity and a live log, decodes selected `CFDC2110`
family/opcode forms, supports semantic filtering, learns an idle baseline,
records operator markers/action windows, and saves annotated JSONL.

Dallas payloads are redacted in UI details and persisted sessions. The monitor
never sends arbitrary legacy controls.

**Verification status:** source implemented; MSVC build and real-scope live use
have not yet been reported. See [live monitor design and implementation](live-ioctl-monitor-design.md)
and [tool README](../tools/lecwatch/README.md).
