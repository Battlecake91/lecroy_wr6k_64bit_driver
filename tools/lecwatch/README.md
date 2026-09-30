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

The driver ring contains 256 records and the monitor polls it every 100 ms.
Sequence numbers are used for deduplication and gap detection. The status line
shows the number of records that were missed because the ring advanced past the
last observed sequence.

If real use shows persistent gaps, the next optimization should be a private
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
