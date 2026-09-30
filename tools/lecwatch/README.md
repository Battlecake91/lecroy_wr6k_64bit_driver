# lecwatch

`lecwatch.exe` is a native x64 Windows GUI for observing the replacement
driver's existing private IOCTL trace ring while XStream is running.

It is intentionally observational. It does not issue legacy hardware-control
IOCTLs, write registers, program the serial-trigger FPGA, inject pending
interrupt bits or modify the Dallas EEPROM.

## Build

From the repository root:

```powershell
.\scripts\build-lecwatch.ps1
```

Output:

```text
tools\lecwatch\build\lecwatch.exe
```

No driver rebuild or reload is required if the currently installed replacement
driver already supports the private `DEBUG_GET_TRACE` ABI used by
`lecdiag trace-capture`.

## Use

Start XStream normally, then run:

```powershell
.\tools\lecwatch\build\lecwatch.exe
```

The monitor attaches to the same device interface used by `lecdiag`. On first
connection it starts at the current trace sequence, so old ring contents are
not replayed as new activity.

### Activity view

The upper table contains one row per known or newly observed top-level IOCTL.

- filled circle: request observed within roughly 750 ms;
- request rate and total session count;
- last completion status;
- semantic confidence;
- most recently decoded `CFDC2110` nested family/opcode.

The indicator means **recently observed**, not "currently executing": the
kernel trace record is appended after the IOCTL handler completes.

### Filters

- **Hide known** hides confirmed/functional controls while retaining
  partial/unknown controls. For `CFDC2110` the nested family/opcode confidence
  is used, so a known JTAG/ACK form can disappear while a partial or unknown
  nested command immediately remains visible.
- **Hide idle baseline** suppresses normal background traffic after an idle
  baseline has been learned.
- **Errors only** shows failing traffic.
- **Pause UI** pauses painting only. The reader still consumes and retains
  trace events, and the log is rebuilt when unpaused.
- the text filter matches numeric IOCTL code, symbolic name and decoded nested
  command text.

### Idle baseline

Press **Learn Idle (5 s)** while not touching XStream. The monitor records both
per-IOCTL rate and up to 32 request payload signatures per top-level IOCTL.
Afterward, recurring baseline traffic can be hidden. A payload variant not seen
during baseline temporarily forces that IOCTL visible even when its overall
rate resembles background polling.

The variant hash deliberately excludes Dallas payload contents.

### Markers and action windows

Type an annotation and press Enter or **Add Marker** to place a QPC-timestamped
user marker in the session.

For controlled experiments:

1. type a label such as `CH1 V/div 200mV -> 500mV`;
2. press **Start Action**;
3. perform exactly one XStream operation;
4. press **End Action**.

The monitor writes start/end markers and a per-IOCTL count summary for the
window.

### Live log and details

The lower table retains recent IOCTLs and annotations. Double-click a normal
IOCTL row to inspect bounded input/output previews and its nested-command
summary.

Dallas ROM/memory/read/write payloads are always redacted from details and from
saved sessions because the PCI-card DS2433 may contain XStream license data.

### Save session

**Save Session** writes an annotated UTF-8 JSONL file containing the in-memory
session, trace metadata, markers and bounded payload previews. Dallas payloads
remain blank and are marked redacted.

## Trace loss

The driver ring contains 256 records. After the first real acquisition-heavy session exposed trace gaps, the monitor polls it every 10 ms.
Sequence numbers are used for deduplication and gap detection. The status line
shows the number of records that were missed because the ring advanced past the
last observed sequence.

The first owner session still reported 810 dropped records with the original 100-ms polling interval, so this is now a measured limitation rather than a hypothetical one. If 10-ms polling still shows persistent gaps, the next optimization should be a private
"records newer than sequence N" diagnostic ABI. The kernel transport is not
changed pre-emptively in this first version.


## First MSVC build attempt

The owner's first Windows/MSVC build attempt on 2026-09-30 correctly exposed
three source-side build omissions:

- duplicate `_CRT_SECURE_NO_WARNINGS` definition (source plus command line);
- missing declaration of `_countof`;
- missing `CTL_CODE`, `METHOD_BUFFERED` and `FILE_READ_ACCESS` definitions.

Commit `a668ee3` fixes these by removing the duplicate source macro and adding
`<stdlib.h>` plus `<winioctl.h>`.

The failed build produced no `lecwatch.exe`, so the subsequent
`tools\lecwatch\build\lecwatch.exe` launch failure was expected and is not
a runtime failure of the monitor. A fresh owner MSVC build after `a668ee3` is
still required before the GUI can be called build-verified.


## Second MSVC build attempt

The owner's follow-up MSVC build on 2026-09-30 passed compilation and then
failed at link time with 28 unresolved Win32 GUI imports. The unresolved
symbols were from User32/GDI32, including `GetMessageW`, `CreateWindowExW`,
`MessageBoxW`, `SetProcessDPIAware` and `GetStockObject`.

Commits `c5bb895` and `979a836` add explicit `user32.lib` and
`gdi32.lib` linkage both through source pragmas and the build helper's linker
arguments. This was a build-system omission, not a runtime/driver failure.

A fresh owner MSVC build after `979a836` is still required before the tool is
build-verified.


## First real live session

The owner successfully built and ran `lecwatch` alongside XStream on the real
x64 WaveRunner system on 2026-09-30. This establishes the GUI/device-interface
connection and live trace path in practice.

Observed behavior:

- `0xCFDC2110` is extremely active while acquisition is running;
- its live activity stops immediately when acquisition is stopped;
- changing XStream's **Use Auxiliary Output for** setting produces
  `0xCFDC2110` activity;
- the first saved session retained 5,000 events and reported 810 sequence gaps,
  demonstrating that the original 100-ms polling interval was too slow for
  acquisition bursts.

Offline review of that session also found a decoder bug in the first GUI build:
50-byte CFDC2110 record-type-1 MAM blocks were incorrectly displayed as
`F0/0x00`, `F0/0x02`, or `F0/0x04`. The record header at offset +4 is
authoritative; these are not command-family opcodes.

The corrected monitor now:

- parses CFDC2110 record type before interpreting family/opcode;
- labels record-type-1/2 traffic as MAM programming;
- labels known acquisition/control commands including family-1 `0x50/0x51`
  MTT transfer, family-0 `0x90` SPI transfer, family-2 `0x02` MTTCTL,
  family-2 `0x01` timer, family-2 `0x05` ITMODE and family-2 `0x10`
  LEDCTL;
- recognizes 85FB pending-response fetch records;
- polls every 10 ms instead of 100 ms;
- retains 20,000 in-memory session events instead of 5,000;
- writes `qpc_frequency` and session format version 2 into new JSONL headers.

These changes are tool-only and do not modify the kernel driver.


## Automated XStream E2E action tracing

The XStream E2E regression can now drive reversible scope actions and mark the
exact action windows directly inside a running `lecwatch` instance.

Build and start the current monitor:

```powershell
git pull --ff-only origin main
.\scripts\build-lecwatch.ps1
.\tools\lecwatch\build\lecwatch.exe
```

Then, in a second PowerShell:

```powershell
.\scripts\test-driver.ps1 -Mode XStream -TraceXStreamActions
```

When `-TraceXStreamActions` is enabled, the E2E script uses a small
`WM_COPYDATA` bridge to send synchronous control messages to the
`LecWatchMainWindow` window class. No additional driver/debug IOCTL is added.

The current traced actions are:

- forced-trigger acquisition;
- C1 vertical-scale apply and restore;
- horizontal timebase apply and restore;
- C1 coupling apply and restore;
- C1 bandwidth-limit apply and restore.

Each action produces normal `ACTION START` / `ACTION END` markers and the
existing per-top-level-IOCTL action count summary in the live log. The E2E
script waits 100 ms before sending `ACTION END` so the 10-ms trace reader can
ingest the tail of the just-completed driver burst.

If `-TraceXStreamActions` is requested without a compatible running
`lecwatch`, the E2E script stops before touching XStream controls.

After the E2E run, save the lecwatch session and summarize it:

```powershell
python .\tools\lecwatch\summarize-actions.py ".\lecwatch_session.jsonl" --markdown ".\lecwatch_action_summary.md"
```

The summarizer sorts marker/IOCTL events by QPC, then reports for every action:

- action duration when the format-v2 QPC frequency is available;
- total retained IOCTL count;
- non-success status count;
- per-top-level-IOCTL counts;
- nested/detail counts;
- number of distinct input-payload variants per detail.

This makes repeated E2E runs directly useful for assigning XStream controls to
driver traffic without manually clicking Start/End Action around every change.
