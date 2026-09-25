# Runtime-loaded WR6200A acquisition stack

Captured from the running process `lecroyxstreamdso.exe` (PID 6456).

Relevant loaded modules include:

```text
lecaladdinhwaccesspcisvr.dll
acqmgrsvr.dll
aladdinpersonasvr.dll
aladdinchannelssvr.dll
aladdinacquisitionsvr.dll
lecaladdinadcmonitorsvr.dll
s65hwupgrade.dll
s65devicepacksvr.dll
boardtestsvr.dll
```

This confirms that the WR6200A actively uses the Aladdin/S65 acquisition stack.

## Priority targets

1. `lecaladdinhwaccesspcisvr.dll`
2. `aladdinacquisitionsvr.dll`
3. `acqmgrsvr.dll`
4. `s65devicepacksvr.dll`
5. `s65hwupgrade.dll`

The first DLL is the strongest candidate for the user-mode/kernel boundary and should be analyzed for SetupAPI enumeration, CreateFile targets, DeviceIoControl calls, IOCTL constants, and request-buffer construction.

Current working model:

```text
lecroyxstreamdso.exe
  -> acquisition manager / Aladdin acquisition logic
  -> lecaladdinhwaccesspcisvr.dll
  -> LecS65AcqDrv.sys
  -> PCI VEN_1570 DEV_0005
```

Next step: reverse engineer `lecaladdinhwaccesspcisvr.dll` and map its live IOCTL usage against the already recovered kernel dispatch table.
