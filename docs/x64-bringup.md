# x64 bring-up procedure

This document describes the first hardware test of the native x64 compatibility driver.

## Scope of prototype 0.1

Implemented:

- native x64 WDM driver project;
- PCI binding for `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00`;
- legacy service/binary identity `LecS65AcqDrv`;
- legacy DOS device path `\\.\ALADDINAcqDriver0`;
- registration of all four recovered S65 device-interface GUIDs;
- translated PCI resource tracing;
- mapping of the first three translated memory resources as BAR0, BAR1 and BAR2;
- build query `0xCFDC21C8 -> 1002`;
- raw register read `0xCFDC21C0`;
- raw register write `0xCFDC21C4`;
- legacy no-op `0xCFDC2184`;
- legacy not-implemented response for `0xCFDC212C`;
- detailed IOCTL logging;
- first-64-byte hex dump for buffered input/output;
- PID and WOW64 indication for user requests;
- private debug statistics IOCTL;
- small `lecdiag.exe` user-mode test tool.

Not implemented yet:

- interrupts;
- DPC handling;
- active acquisition launch through MAMRGO / MTTRGO;
- active execution of partially understood `0xCFDC2110` firmware packets;
- serial-trigger FPGA programming;
- final `METHOD_NEITHER` acquisition execution;
- remaining low-value legacy IOCTLs.

Implemented since the original prototype:
- Dallas / 1-Wire access;
- legacy event registration;
- decoded BAR1 message-transport helpers, kept gated where command semantics are incomplete;
- x64 transfer registration for `0xCFDC2124/2128` using opaque owner-checked 32-bit tokens;
- MDL locking and board-facing chained DMA descriptor-table construction;
- explicit rejection when a source/table physical address exceeds the legacy 32-bit descriptor format;
- recovered interrupt/DPC infrastructure and completion-event plumbing;
- richer passive IOCTL tracing including METHOD_NEITHER input previews and buffered output previews.

Unknown controls intentionally return `STATUS_INVALID_DEVICE_REQUEST`.

The `METHOD_NEITHER` request is logged but its pointers are deliberately not dereferenced in this prototype.

## Before installing

Keep a restorable image of the working scope disk.

Recommended target:

```text
Windows 10/11 x64
Visual Studio 2022
Desktop C++ workload
Windows Driver Kit matching the installed SDK
Secure Boot disabled for test-signing development
```

Do not start the first test with XStream automatically launching at login. The driver should be validated independently first.

## Build

Open:

```text
LecS65AcqDrv.sln
```

Select:

```text
Debug | x64
```

Build the solution.

Expected primary output:

```text
LecS65AcqDrv.sys
```

The project has not yet been compiled in CI because the current repository environment does not provide a Windows WDK build worker. Treat the first local WDK compile as part of bring-up and commit any WDK-version-specific project fixes.

## Test signing

For an isolated development scope, enable Windows test mode from an elevated command prompt:

```cmd
bcdedit /set testsigning on
```

Reboot.

A PnP x64 package still needs an acceptable test signature/catalog for normal installation. Use Visual Studio/WDK driver-package signing or create a test certificate and catalog. Do not disable integrity enforcement permanently as a substitute for signing.

## Install

After the package is test-signed, install from an elevated prompt:

```cmd
pnputil /add-driver LecS65AcqDrv.inf /install
```

Then inspect:

```cmd
pnputil /enum-devices /instanceid "PCI\VEN_1570&DEV_0005*"
sc query LecS65AcqDrv
```

The Device Manager target should remain:

```text
LeCroy Acquisition Device (S65)
```

## Capture debug output

The prototype sends verbose kernel diagnostics with prefix:

```text
[LecS65x64]
```

Use WinDbg kernel debugging or a kernel-debug capture tool capable of receiving `DbgPrintEx` output.

Expected startup trace includes:

```text
DriverEntry
AddDevice
PNP: IRP_MN_START_DEVICE
translated resource descriptors
BAR0 physical address / length / virtual mapping
BAR1 physical address / length / virtual mapping
BAR2 physical address / length / virtual mapping
interrupt resource
four interface enables
```

Save the complete startup log before launching XStream.

## First safe user-mode test

Build `tools\lecdiag\lecdiag.c` from an x64 Native Tools prompt:

```cmd
cd tools\lecdiag
build.cmd
```

Then run:

```cmd
build\lecdiag.exe build
```

Expected output:

```text
driver build: 1002
```

This proves:

1. the PnP driver loaded;
2. the DOS symbolic link exists;
3. a 64-bit user-mode process can open the driver;
4. `DeviceIoControl` works.

Next:

```cmd
build\lecdiag.exe stats
```

## First hardware reads

Only after the BAR resource log has been reviewed should raw reads be attempted.

Known non-destructive candidates from the recovered register map include:

```text
BAR0 + 0x000  FVER
BAR0 + 0x004  ERRS
BAR0 + 0x080  INTST
BAR1 + 0x00C  ACQFVER
```

Example:

```cmd
build\lecdiag.exe read 0 0x0
```

Do not use register-write tests during the initial bring-up.

## First XStream test

Once build query and safe register reads work:

1. clear/capture the kernel log;
2. start the original 32-bit LeCroy XStream software;
3. do not immediately start an acquisition if the application allows it;
4. capture every `[LecS65x64] IOCTL:` line;
5. save the log before changing the driver.

The most useful trace fields are:

```text
PID
WOW64 flag
IOCTL code
METHOD
input length
output length
first 64 bytes of buffered input
completion status
output length / first 64 output bytes
```

This will establish the actual startup protocol.

## Expected early failures

The first XStream launch is expected to fail once it reaches an unimplemented IOCTL. That is useful.

A trace such as:

```text
IOCTL: ... code=0xCFDCxxxx ...
UNKNOWN IOCTL ...
```

identifies the next compatibility function to implement.

The desired development loop is therefore:

```text
start XStream
-> capture first unsupported request
-> reconstruct that request against the old driver
-> implement it
-> repeat
```

This is safer and much faster than attempting to clone the entire 2008 driver before the first hardware test.


## IOCTL trace capture

The preferred capture format is now JSON Lines so traces can be processed
directly by analysis tooling.

Build `tools\lecdiag` and run:

```powershell
.\scripts\capture-xstream-trace.ps1
```

The script clears the trace and captures for 90 seconds by default:

```text
trace-captures/xstream_trace_YYYYMMDD_HHMMSS.jsonl
```

For a longer startup/session:

```powershell
.\scripts\capture-xstream-trace.ps1 -DurationSeconds 180
```

The live collector polls every 250 ms and appends new sequence numbers, so a
long XStream startup is not limited to the 256-entry in-kernel snapshot.

Each entry contains:

- boot-relative timestamp;
- PID and WOW64 state;
- IOCTL value and transfer method;
- input/output lengths;
- completion NTSTATUS and `IoStatus.Information`;
- Type3InputBuffer/UserBuffer pointer values for correlation;
- up to 256 bytes of input;
- up to 128 bytes of METHOD_BUFFERED output.

For METHOD_NEITHER the tracer probes only the bounded input preview. It does not
blindly dereference arbitrary output user buffers.

Successful generic register-read polling remains suppressed to preserve useful
protocol history.

See [runtime-trace.md](runtime-trace.md) for the exact JSONL schema and manual
`lecdiag trace-save` / `trace-capture` commands.


### Dallas timeout triage

A confirmed reference-system x64 run currently reaches the replacement driver
successfully and returns build 1002, but `lecdiag dallas-id` returns Win32
error 121. In this driver that maps to `STATUS_IO_TIMEOUT` from the ONEWIRE
busy-bit polling loop.

This distinguishes the failure from installation/PnP problems. Use only these
known safe reads to triage the controller state:

```powershell
.\tools\lecdiag\build\lecdiag.exe bars
.\tools\lecdiag\build\lecdiag.exe read 2 0x40
.\tools\lecdiag\build\lecdiag.exe read 0 0x0
.\tools\lecdiag\build\lecdiag.exe read 1 0x0C
```

BAR2+0x40 is the confirmed ONEWIRE register. Bit 0 is the controller-busy bit
and should be clear while idle. If it is already stuck high before issuing a
Dallas command, the likely missing piece is board/startup initialization rather
than the recovered 1-Wire ROM command sequence.


### All-ones MMIO state

A later reference-system check confirmed that the replacement driver is loaded,
the S65 PCI device is bound, and the DOS interface answers the build query, but
all three known-safe MMIO reads currently return `0xFFFFFFFF`:

```text
BAR0 + 0x000  FVER      -> 0xFFFFFFFF
BAR1 + 0x00C  ACQFVER   -> 0xFFFFFFFF
BAR2 + 0x040  ONEWIRE   -> 0xFFFFFFFF
```

The BAR physical addresses and lengths match the known reference resources, so
the immediate question is whether PCI memory decoding/bus mastering is enabled
or whether the FPGA/register fabric still requires legacy startup
initialization.

The driver now exposes a passive PCI config-space diagnostic:

```powershell
.\tools\lecdiag\build\lecdiag.exe pci
```

This reports vendor/device IDs, BDF, PCI command/status, BAR config values and
interrupt line/pin. In particular, inspect:

- PCI Command bit 1: Memory Space Enable
- PCI Command bit 2: Bus Master Enable
- PCI Command bit 10: INTx Disable

Do not add speculative BAR writes while the device still reads all ones.
