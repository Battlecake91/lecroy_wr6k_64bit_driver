# lecaladdinhwaccesspcisvr.dll analysis

This DLL is confirmed to be loaded by the running WR6200A XStream process and is the low-level user-mode wrapper around the acquisition kernel driver.

## Binary identity

```text
Filename:  lecaladdinhwaccesspcisvr.dll
SHA-256:  4bdfcbe57fb76f40ca5d77f729e1a6662aa3b5b4e3dd2e12a7f13f84ecfc4f86
Format:    PE32 / Intel i386
Target OS: Windows 5.01
Timestamp: 2017-06-23 14:05:50
Linker:    Microsoft linker 14.0
ImageBase: 0x10000000
```

Embedded PDB/source paths include:

```text
h:\mauisoft\main\components\acquisition\aladdinacqboards\lecaladdinhwaccesspcisvr\AladdinHWAccessPCI.cpp
h:\mauisoft\main\components\acquisition\aladdinacqboards\lecaladdinhwaccesspcisvr\AAcqInterruptEventThread.cpp
h:\mauisoft\main\components\acquisition\aladdinacqboards\lecaladdinhwaccesspcisvr\RRIntThread.cpp
h:\mauisoft\main\components\acquisition\aladdinacqboards\lecaladdinhwaccesspcisvr\Win32\Release\lecaladdinhwaccesspcisvr.pdb
```

## Kernel device path

The DLL contains and uses the UTF-16 string:

```text
\\.\ALADDINAcqDriver0
```

This is passed to `CreateFileA` after internal string conversion.

The open call is reconstructed as approximately:

```c
CreateFileA(
    "\\\\.\\ALADDINAcqDriver0",
    GENERIC_READ | GENERIC_WRITE,
    0,
    NULL,
    OPEN_EXISTING,
    flags,
    NULL
);
```

The flags field is normally zero or conditionally includes `0x40000000` (`FILE_FLAG_OVERLAPPED`).

This is the first direct proof of the legacy DOS device name expected by the LeCroy user-mode stack.

The x64 replacement should therefore expose a compatible DOS symbolic link resolving:

```text
\\.\ALADDINAcqDriver0
```

## Driver version handshake

The DLL directly issues:

```text
IOCTL 0xCFDC21C8
Input:  none
Output: 4 bytes
```

This is the driver-build query already recovered from the legacy SYS. On success the returned DWORD is used as the driver version/build value.

The original SYS returns:

```text
1002
```

This handshake should be implemented in the very first x64 prototype.

## Directly observed DeviceIoControl calls

The DLL contains immediate calls for at least:

```text
0xCFDC2110
0xCFDC2114
0xCFDC2124
0xCFDC2128
0xCFDC212C
0xCFDC2138
0xCFDC2180
0xCFDC2184
0xCFDC218C
0xCFDC2190
0xCFDC2194
0xCFDC21C0
0xCFDC21C4
0xCFDC21C8
0xCFDC21CC
```

It also contains references to the LeCroy-specific controls:

```text
0x00223000
0x00223004
0x0022303C
0x00223040
0x00223100
```

Not every kernel IOCTL appears as an immediate constant here. Some are issued by higher-level acquisition modules through the DLL's generic pass-through method.

### Newly observed values

Two controls were not present in the first recovered dispatch list from the 2008 SYS:

```text
0xCFDC2114
0xCFDC21CC
```

They are emitted by this 2017 user-mode DLL. This may indicate:

- a newer kernel-driver ABI than the captured 6.1.1.1002 SYS;
- optional controls handled by a different LeCroy/Aladdin driver variant;
- compatibility paths not exercised by the S65 driver.

They must therefore be logged and handled deliberately rather than assumed valid for the 2008 S65 driver.

## Generic DeviceIoControl pass-through

A particularly important function in this DLL forwards the eight DeviceIoControl arguments essentially unchanged to the Win32 API.

When internal tracing/serialization flags are inactive, the function tail-jumps directly to `DeviceIoControl`.

This explains why higher-level DLLs may contain IOCTL values such as `0xCFDC2130` even when that value is absent as an immediate constant in this DLL.

For reverse engineering, the new x64 kernel driver can therefore act as an excellent protocol tracer: every request made through the entire user-mode acquisition stack eventually arrives at one common kernel boundary.

## Raw register access

The DLL directly issues:

```text
0xCFDC21C0  register read
0xCFDC21C4  register write
```

For the observed read helper:

```text
InputLength  = 4
OutputLength = 4
```

This matches the legacy BAR0-implied request structure already recovered from the SYS.

The DLL also contains diagnostic format strings such as:

```text
Reading Reg 0x%x Data : 0x%08x
```

which further confirms the purpose of these controls.

## SetupAPI usage

The DLL dynamically loads:

```text
SetupApi.dll
```

and resolves at runtime:

```text
SetupDiGetClassDevsA
SetupDiGetDeviceInterfaceDetailA
SetupDiGetDeviceRegistryPropertyA
SetupDiSetClassInstallParamsA
SetupDiEnumDeviceInterfaces
SetupDiDestroyDeviceInfoList
SetupDiEnumDeviceInfo
SetupDiCallClassInstaller
```

So the DLL supports both legacy fixed-device opening and PnP/SetupAPI enumeration paths.

The four interface GUIDs recovered from `LecS65AcqDrv.sys` do not appear verbatim in this DLL. The interface selection is therefore likely supplied by another object/component or constructed through shared acquisition infrastructure.

## Additional GUID

The DLL constructs this GUID in code:

```text
{958695A4-693A-435E-8297-66F805D8E46A}
```

Its exact semantic role is not yet proven. It is passed into an internal object-construction path near hardware-access initialization and should not yet be treated as one of the kernel device-interface GUIDs.

## Hardware-family selection

The DLL contains runtime strings:

```text
Null
S65
FE2
CENTAUR
```

and selects between internal hardware access objects based on these identifiers.

This confirms that the DLL is a multi-generation Aladdin hardware-access layer rather than a one-off S65-only wrapper.

## Implication for the x64 compatibility driver

The first prototype can now reproduce the most important outer contract with high confidence:

1. bind to `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00`;
2. create/enable the PnP interfaces already recovered from the SYS;
3. expose the legacy DOS symbolic link `\\.\ALADDINAcqDriver0`;
4. accept a read/write `CreateFile` open from the 32-bit XStream process;
5. implement `0xCFDC21C8` returning build `1002`;
6. log every unknown IOCTL with process/WOW64 status and buffer sizes;
7. implement raw register read/write next.

This is enough to let the original 32-bit LeCroy software become the protocol probe for the new x64 driver.
