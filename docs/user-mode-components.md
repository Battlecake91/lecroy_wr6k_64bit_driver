# User-mode acquisition components

A targeted capture of the working LeCroy installation produced several acquisition-related PE32 DLLs. The goal here is to identify which component actually owns the S65 kernel-driver protocol.

## Strong candidates

### AladdinAcquisitionSvr.dll

Confirmed properties:

```text
PE32 / Intel i386
Target: Windows 5.01
PDB:
h:\mauisoft\main\components\acquisition\aladdinacqboards\aladdinacquisitionsvr\Win32\Release\aladdinacquisitionsvr.pdb
```

Highly relevant embedded strings include:

```text
S65STD
S65_STD
LeCroy.AladdinHWAccessPCI
AladdinHwAccessPCI
AladdinDriver
AladdinAcqBoardDriver
AladdinAcqBoard\KernelPCIRegisters
AladdinAcqBoard\KernelTracesLevel
```

The binary also exposes source-path strings for many original modules, including:

```text
AcqShared\AladdinAcquisitionDriver.cpp
AcqShared\AcqIoRegisterBagImpl.cpp
AcqShared\AAcqJtagIoRegisterBagImpl.cpp
aladdinacquisitionsvr\AladdinAcqBoard.cpp
aladdinacquisitionsvr\MamDriver.cpp
```

This is a very strong semantic match for the S65 hardware/driver family.

The import table contains `CreateFileA/W` and file-mapping APIs. A direct static import of `DeviceIoControl` was not observed in this DLL, so kernel access may be wrapped through another helper/interface or resolved indirectly.

### hornetacquisitionsvr.dll

Confirmed properties:

```text
PE32 / Intel i386
SetupAPI device-enumeration imports
CreateFileA/W
DeviceIoControl
CreateFileMappingA
```

Relevant strings include:

```text
S65STD
LeCroy.AladdinHWAccessPCI
AladdinHwAccessPCI
AladdinDriver
AladdinAcqBoardDriver
AladdinAcqBoard\KernelPCIRegisters
Error opening device: (...) returned from CreateFile
DeviceIoControl returns ...
```

Most importantly, the binary contains the little-endian constant corresponding to legacy IOCTL:

```text
0xCFDC2130
```

which the legacy kernel driver identifies as:

```text
IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA
```

This proves that at least one captured LeCroy acquisition DLL directly speaks the same Aladdin/S65 IOCTL protocol as `LecS65AcqDrv.sys`.

### EagleAcqBoardSvr.dll / Mag12AcqBoardSvr.dll

Both import:

```text
SetupDiGetClassDevsA
SetupDiEnumDeviceInterfaces
SetupDiGetDeviceInterfaceDetailA
SetupDiGetDeviceRegistryPropertyA
CreateFileA/W
DeviceIoControl
CreateFileMappingA
```

and contain explicit DeviceIoControl failure diagnostics.

They demonstrate the general LeCroy acquisition architecture, but have not yet been proven to be the WR6200A/S65 component used at runtime.

## Other captured acquisition components

The analysis package also contains:

```text
AcqMgrSvr.dll
HennesseyAcqBoardSvr.dll
LogicAnalyzerAcqBoardSvr.dll
MSxxAcqBoardSvr.dll
pichuacquisitionsvr.dll
RoadRunnerAcquisitionSvr.dll
simulatoracqboardsvr.dll
SlaveRemoteBoardSvr.dll
TDNAAcqBoardSvr.dll
...
```

Several contain `S65STD` or common Aladdin acquisition infrastructure. Presence in the installation is therefore insufficient to identify the active WR6200A path.

## Important limitation of the first collection script

The first static candidate search included the generic text `DataAcquisition`. That produced false positives such as Windows AppX deployment DLLs and Device Experience components.

The next collection pass should not use `DataAcquisition` as a primary binary signature.

More useful discriminators are:

- the four recovered device-interface GUIDs;
- the known IOCTL constants;
- `AladdinHwAccessPCI`;
- `AladdinDriver`;
- runtime-loaded module lists from the actual LeCroy process.

## Recommended next dynamic capture

Static analysis has reached diminishing returns for choosing the active DLL.

On the working scope, capture the loaded modules while the LeCroy application is running and preferably while performing an acquisition. This immediately tells us whether the active path is `AladdinAcquisitionSvr.dll`, `hornetacquisitionsvr.dll`, or another board-server DLL.

After the active module is known, reverse engineering can focus on that one binary instead of the whole XStream installation.
