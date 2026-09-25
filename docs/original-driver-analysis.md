# Original driver analysis

## Reference binary

| Property | Value |
|---|---|
| Filename | `LecS65AcqDrv.sys` |
| SHA-256 | `5f53de1dea6a58f201290e79a60fab587c039423322faa884bf4c15f0fd89087` |
| Size | 64,384 bytes |
| PE format | PE32 |
| Architecture | Intel i386 / x86 |
| Subsystem | NT Native |
| Driver type | WDM |
| Linker | Microsoft linker 7.0 |
| PE timestamp | 2008-12-24 01:31:59 |
| Image base | `0x00010000` |
| Entry-point RVA | `0x0000DF42` |
| Image size | `0x0000FB80` |

The PE header contains the `WDM_DRIVER` DLL characteristic.

## Version resources

The embedded version resource identifies the binary as:

```text
CompanyName:       LeCroy Corporation
FileDescription:   S65 Acquisition Driver
FileVersion:       6, 1, 1, 1002
InternalName:      LecS65AcqDrv.sys
OriginalFilename:  LecS65AcqDrv.sys
ProductName:       S65 Acquisition Driver
ProductVersion:    1, 0, 0, 1
PrivateBuild:      1, 0, 0, 1002
```

## Build provenance

The binary contains:

```text
DriverWorks (c) Copyright 2003 Compuware Corporation
```

and the CodeView/PDB path:

```text
H:\mauisoft\main\Drivers\S65\LECS65~1\objfre_w2K_x86\i386\LecS65AcqDrv.pdb
```

This strongly indicates that the original driver was implemented with Compuware DriverWorks and built with an old Windows 2000/XP-era x86 DDK toolchain.

## PE sections

| Section | Virtual size | Raw size | Purpose |
|---|---:|---:|---|
| `.text` | `0xBF72` | `0xBF80` | main non-paged code |
| `.rdata` | `0x093B` | `0x0980` | constants/import-related data |
| `.data` | `0x05ED` | `0x0600` | writable globals |
| `.CRT` | `0x0010` | `0x0080` | CRT support |
| `.STL` | `0x0010` | `0x0080` | C++/runtime support |
| `PAGE` | `0x09CA` | `0x0A00` | pageable code |
| `INIT` | `0x0B64` | `0x0B80` | initialization code |
| `.rsrc` | `0x05A0` | `0x0600` | version/dialog resources |
| `.reloc` | `0x0C60` | `0x0C80` | base relocations |

## Kernel API usage

The import table confirms a conventional WDM PnP driver with direct hardware access.

Important imported routines include:

### Device / PnP

```text
IoCreateDevice
IoDeleteDevice
IoAttachDeviceToDeviceStack
IoDetachDevice
IoCreateSymbolicLink
IoDeleteSymbolicLink
IoRegisterDeviceInterface
IoSetDeviceInterfaceState
IofCallDriver
PoCallDriver
PoStartNextPowerIrp
PoSetPowerState
PoRequestPowerIrp
```

The binary also contains text labels for standard PnP minors, including:

```text
IRP_MN_START_DEVICE
IRP_MN_QUERY_REMOVE_DEVICE
IRP_MN_REMOVE_DEVICE
IRP_MN_CANCEL_REMOVE_DEVICE
IRP_MN_STOP_DEVICE
IRP_MN_QUERY_STOP_DEVICE
IRP_MN_CANCEL_STOP_DEVICE
IRP_MN_QUERY_DEVICE_RELATIONS
IRP_MN_QUERY_DEVICE_TEXT
IRP_MN_QUERY_PNP_DEVICE_STATE
IRP_MN_DEVICE_USAGE_NOTIFICATION
```

### MMIO / register access

```text
MmMapIoSpace
MmUnmapIoSpace
READ_REGISTER_ULONG
WRITE_REGISTER_ULONG
READ_REGISTER_BUFFER_ULONG
WRITE_REGISTER_BUFFER_ULONG
```

This establishes that the device exposes memory-mapped hardware registers.

### Interrupts

```text
IoConnectInterrupt
IoDisconnectInterrupt
KeSynchronizeExecution
KeInitializeDpc
KeInsertQueueDpc
```

The original driver therefore uses a classic ISR + DPC style interrupt path.

### DMA / MDL related operations

```text
IoAllocateMdl
IoFreeMdl
MmProbeAndLockPages
MmUnlockPages
MmBuildMdlForNonPagedPool
ProbeForWrite
```

These imports make buffer mapping / locked-memory operations likely. Exact IOCTL ownership and semantics still need to be mapped.

## Internal hardware-register names

The executable contains a register-description table with at least the following names:

```text
FVER     BAR0
START    BAR0
ITMODE   BAR1
BUZZER   BAR2
ONEWIRE  BAR2
ERRS     BAR0
ERRM     BAR0
INTST    BAR0
INTEN    BAR0
CLRIRQ   BAR1
CLRERR   BAR1
SGTA     BAR0
IIMTC    BAR0
IIMCL    BAR0
IIMST    BAR0
MAMDAT   BAR1
MAMPGO   BAR1
MAMSEQ   BAR1
MAMRGO   BAR1
JTAGNUM  BAR1
JTAGDAT  BAR1
JTAGDIN  BAR1
SPICTL   ...
```

This is strong evidence for multiple PCI BAR mappings and for FPGA / JTAG / serial-memory control implemented directly by the driver.

The exact offsets associated with these labels are not yet documented here and must be reconstructed before hardware code is implemented.

## Named handlers recovered from diagnostic strings

The binary contains source-level diagnostic strings that reveal several original method names:

```text
CLecS65AcqDrvDevice::Close()
CLecS65AcqDrvDevice::DeviceControl()

IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA_Handler
IOCTL_WRITE_DALLAS_MEMORY_Handler
IOCTL_GET_DALLAS_ID_Handler
IOCTL_READ_DALLAS_MEMORY_Handler
```

The corresponding known IOCTL mappings are in [ioctl-map.md](ioctl-map.md).

## Dallas / 1-Wire observations

The Dallas handlers expose useful ABI information:

- `IOCTL_GET_DALLAS_ID` requires an 8-byte output buffer.
- `IOCTL_READ_DALLAS_MEMORY` accepts output lengths from 1 to `0x200` bytes.
- `IOCTL_WRITE_DALLAS_MEMORY` accepts input lengths from 1 to `0x200` bytes.
- the write handler writes the requested data, reads it back, and compares the result with `RtlCompareMemory`;
- write operations appear to be chunked in blocks of up to `0x20` bytes and retried;
- the pool tag visible in this path is `LeCD`.

These are implementation observations, not yet a complete public ABI definition.

## Driver-created names

Generic DriverWorks strings for the following prefixes are present:

```text
\Device\
\DosDevices\
\??\
```

A static string for the final S65 device/symbolic-link name has not yet been proven. DriverWorks may construct it dynamically. This must not be guessed when implementing the x64 replacement.

## x64 migration implications

The original image itself cannot be loaded by an x64 Windows kernel because it is an i386 PE32 driver.

A replacement x64 driver can nevertheless remain compatible with LeCroy user-mode software if it preserves the expected external interface.

Items that require special scrutiny during the port:

1. IOCTL numeric values.
2. `METHOD_*` transfer type.
3. exact input/output buffer sizes.
4. structures containing native pointers, handles, `ULONG_PTR`, `SIZE_T`, or alignment-sensitive members.
5. direct user-pointer handling, especially any `METHOD_NEITHER` path.
6. DMA/MDL assumptions.
7. BAR address width and physical-address handling.
8. interrupt resource handling on modern Windows.

Most recovered IOCTLs use `METHOD_BUFFERED`, which is favourable for cross-architecture compatibility. One observed control code ends in method value 3 and therefore appears to be a `METHOD_NEITHER` request; it deserves priority during ABI reconstruction.

## Confidence convention

Future reverse-engineering notes use:

- **Confirmed**: directly visible in PE metadata, imports, resources, strings, or unambiguous machine code.
- **Strong inference**: machine-code behaviour is clear but the original source-level name is unknown.
- **Unknown**: not yet sufficiently established.

This distinction is deliberate. A plausible guess in a kernel driver is merely a crash with good marketing.
