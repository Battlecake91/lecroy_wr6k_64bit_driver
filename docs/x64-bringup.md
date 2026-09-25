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
- Dallas / 1-Wire;
- FPGA programming;
- acquisition DMA / MDL path;
- legacy `METHOD_NEITHER` transfer implementation;
- other legacy IOCTLs.

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
