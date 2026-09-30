# Live IOCTL monitor design

Status: proposed 2026-09-30. This is a user-mode observability tool design;
it does not change hardware semantics or add new original IOCTL implementations.

## Goal

Provide an interactive Windows GUI while XStream is running so an operator can
perform one visible XStream action (for example change V/div, coupling,
timebase, trigger, probe operation or another control) and immediately see
which legacy driver requests occurred around that action.

The UI is an analysis aid. It must describe an IOCTL as **recently observed**,
not literally "currently active": the existing kernel trace record is appended
after the driver's IOCTL handler has completed.

## Existing trace transport is sufficient for v1

The native x64 driver already retains a 256-entry
`LECS65_DEBUG_TRACE_ENTRY` ring. Each retained entry includes:

- monotonically increasing sequence;
- `KeQueryPerformanceCounter` timestamp;
- caller PID and WOW64 flag;
- IOCTL code and transfer method;
- requested input/output lengths;
- completion NTSTATUS and `IoStatus.Information`;
- bounded input preview (up to 256 bytes);
- bounded METHOD_BUFFERED output preview (up to 128 bytes).

Private debug IOCTLs are explicitly excluded from the trace. User mode can read
the ring through `LECS65_IOCTL_DEBUG_GET_TRACE`; current `lecdiag
trace-capture` already polls the same snapshot every 250 ms.

Therefore the first GUI should use the existing read-only debug IOCTL and make
**no driver change**.

## Proposed tool

Working name: `lecwatch`.

Prefer a native Windows user-mode GUI built beside `lecdiag`, reusing the
existing SetupAPI interface discovery and trace structures. This avoids a
Python/.NET runtime dependency on the scope.

### Main view

1. **IOCTL activity grid**
   - one row/tile per observed IOCTL;
   - symbolic name plus hexadecimal code;
   - short "LED" flash for a recently observed request;
   - count and recent request rate;
   - completion-error indicator;
   - configurable persistence (for example 300-1000 ms).

2. **Live event log**
   - relative QPC time;
   - sequence number;
   - IOCTL code and symbolic/semantic name;
   - input/output length;
   - NTSTATUS;
   - optional compact decoded subcommand summary;
   - payload preview only on demand.

3. **Filters**
   - hide semantically known IOCTLs;
   - show only partial/unknown controls;
   - hide selected high-frequency background controls;
   - errors only;
   - per-IOCTL enable/disable;
   - text/code filter.

4. **Operator markers**
   - a text field plus button/Enter to record a user action such as
     `V/div CH1 200mV -> 500mV`;
   - marker uses user-mode `QueryPerformanceCounter`, allowing direct
     relative correlation with the driver's kernel QPC timestamps;
   - markers are saved with the session but never sent to the driver.

5. **Action-window mode**
   - Begin action / End action buttons;
   - report IOCTLs seen inside the marked interval, their counts and payload
     variants;
   - compare against the immediately preceding idle interval.

6. **Idle-baseline suppression**
   - learn a short idle baseline;
   - dim or hide continuously recurring polling traffic;
   - highlight new IOCTLs, changed request rates and changed payload shapes
     during an operator action.

This last mode is important: a simple flashing grid would otherwise become a
Christmas tree because XStream continuously polls several controls.

## Semantic filtering

Do not treat "known top-level IOCTL" as equivalent to "all contained semantics
known". In particular `0xCFDC2110` multiplexes multiple nested command
families/opcodes. A useful monitor should classify both levels where decoding
exists:

- top-level IOCTL confidence: confirmed / functional / partial / unknown;
- nested command confidence for `CFDC2110` and other structured controls.

Thus "hide known" should still be able to surface an unfamiliar nested command
inside a known top-level IOCTL.

## Performance and loss detection

The current ring holds 256 records. At high XStream request rates a GUI thread
that stalls longer than the ring's history can miss sequences. The monitor must:

- read on a worker thread, initially every 100-250 ms;
- use sequence numbers to deduplicate records;
- display an explicit gap/dropped counter when the next observed sequence is
  greater than the expected value;
- decouple trace acquisition from UI painting;
- update the visual display at a lower rate than acquisition if necessary;
- avoid writing every event synchronously to disk on the acquisition thread.

The existing snapshot is roughly 256 bounded records, so v1 can reasonably
reuse it. If practical testing shows trace loss or unacceptable CPU/lock time,
a later private diagnostic ABI can return only records newer than a supplied
sequence number. Do not change the kernel transport pre-emptively.

## Privacy / safety

Keep the existing Dallas redaction policy for persisted data. The monitor must
not save or display license-bearing Dallas payloads by default.

The tool is observational. It must not expose buttons that issue arbitrary
legacy IOCTLs, write registers, program the serial-trigger FPGA or modify the
Dallas EEPROM.

## Useful workflow

1. Start XStream and `lecwatch`.
2. Learn 3-5 seconds of idle/background traffic.
3. Clear/highlight the action window.
4. Enter an operator marker such as `Enable CH2`.
5. Perform exactly one XStream action.
6. End the action window.
7. Inspect new/changed IOCTLs and nested command payloads.
8. Save the compact annotated session for comparison with another action.

This gives substantially better causal evidence than manually searching a
large raw JSONL trace after the fact, while retaining the raw sequence and
timestamps needed for later static analysis.
