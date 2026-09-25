# IOCTL map

This table is reconstructed from the comparison tree inside `CLecS65AcqDrvDevice::DeviceControl()`.

All values below are **confirmed as dispatch values** in the analysed binary.

## Recovered dispatch table

| IOCTL | Device type | Function | Method | Handler VA | Recovered behaviour |
|---:|---:|---:|---|---:|---|
| `0x00222C00` | `0x22` | `0xB00` | BUFFERED | `0x1286E` | millisecond delay; input >=4; optional fifth byte |
| `0x00222C04` | `0x22` | `0xB01` | BUFFERED | `0x128BC` | input exactly 1 byte |
| `0x00223000` | `0x22` | `0xC00` | BUFFERED | `0x12ADA` | input exactly `0x108` bytes |
| `0x00223004` | `0x22` | `0xC01` | BUFFERED | `0x12A5E` | variable output / size query |
| `0x0022303C` | `0x22` | `0xC0F` | BUFFERED | `0x12CAC` | input exactly `0x10A` bytes |
| `0x00223040` | `0x22` | `0xC10` | BUFFERED | `0x12C18` | variable output / size query |
| `0x00223044` | `0x22` | `0xC11` | BUFFERED | `0x12D24` | output 4 bytes; direct register read |
| `0x00223080` | `0x22` | `0xC20` | BUFFERED | `0x130EA` | **IOCTL_GET_DALLAS_ID** |
| `0x00223084` | `0x22` | `0xC21` | BUFFERED | `0x131B5` | **IOCTL_READ_DALLAS_MEMORY** |
| `0x00223088` | `0x22` | `0xC22` | BUFFERED | `0x11F54` | **IOCTL_WRITE_DALLAS_MEMORY** |
| `0x00223100` | `0x22` | `0xC40` | BUFFERED | `0x12988` | input 12 bytes; event/control path |
| `0xCFDC2110` | `0xCFDC` | `0x844` | BUFFERED | `0x13AE2` | complex transfer path |
| `0xCFDC2124` | `0xCFDC` | `0x849` | BUFFERED | `0x11BDC` | input 12 bytes, result DWORD |
| `0xCFDC2128` | `0xCFDC` | `0x84A` | BUFFERED | `0x11C36` | input 4 bytes |
| `0xCFDC212C` | `0xCFDC` | `0x84B` | BUFFERED | `0x11C5E` | always `STATUS_NOT_IMPLEMENTED` |
| `0xCFDC2130` | `0xCFDC` | `0x84C` | BUFFERED | `0x11CFF` | **IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA** |
| `0xCFDC2138` | `0xCFDC` | `0x84E` | BUFFERED | `0x141DC` | structured transfer helper |
| `0xCFDC2180` | `0xCFDC` | `0x860` | BUFFERED | `0x128F8` | input exactly 4 bytes; event-related |
| `0xCFDC2184` | `0xCFDC` | `0x861` | BUFFERED | inline | success, zero information |
| `0xCFDC218C` | `0xCFDC` | `0x863` | BUFFERED | `0x12B34` | input exactly 4 bytes; event-related |
| `0xCFDC2190` | `0xCFDC` | `0x864` | BUFFERED | `0x13A40` | input exactly 29 bytes |
| `0xCFDC2194` | `0xCFDC` | `0x865` | BUFFERED | `0x12BAE` | output exactly 29 bytes |
| `0xCFDC21C0` | `0xCFDC` | `0x870` | BUFFERED | `0x13954` | **generic register read** |
| `0xCFDC21C4` | `0xCFDC` | `0x871` | BUFFERED | `0x1272A` | **generic register write** |
| `0xCFDC21C8` | `0xCFDC` | `0x872` | BUFFERED | `0x12832` | **get driver build: 1002** |
| `0xCFDC2400` | `0xCFDC` | `0x900` | BUFFERED | `0x13A2E` | forwards to internal control helper |
| `0xCFDD219F` | `0xCFDD` | `0x867` | NEITHER | `0x141F8` | direct user-pointer transfer path |

The access bits decode to `FILE_ANY_ACCESS` for all entries.

## Generic register read: 0xCFDC21C0

This request is now identified with high confidence.

Accepted forms:

```c
// 4-byte legacy form: BAR0 is implied
struct {
    uint32_t offset;
};

// 5-byte packed extended form
struct {
    uint8_t  bar;       // 0..2
    uint32_t offset;
};
```

The driver returns one 32-bit value and uses `READ_REGISTER_ULONG`.

## Generic register write: 0xCFDC21C4

Accepted forms:

```c
// 8-byte legacy form: BAR0 is implied
struct {
    uint32_t offset;
    uint32_t value;
};

// 9-byte packed extended form
struct {
    uint8_t  bar;       // 0..2
    uint32_t offset;
    uint32_t value;
};
```

The driver checks DWORD alignment and writes through `WRITE_REGISTER_ULONG`.

BAR0 offset `0x84` is the `INTEN` register and is special-cased so the written value is cached.

## Driver build query: 0xCFDC21C8

Requires a four-byte output buffer and returns decimal `1002` (`0x3EA`).

That matches the private-build component in file version `6.1.1.1002`.

## Dallas handlers

### 0x00223080: IOCTL_GET_DALLAS_ID

- non-null system buffer required;
- output length exactly 8 bytes;
- 8-byte ID returned on success.

### 0x00223084: IOCTL_READ_DALLAS_MEMORY

- non-null system buffer required;
- output length `1..0x200`;
- requested byte count returned on success.

### 0x00223088: IOCTL_WRITE_DALLAS_MEMORY

- non-null system buffer required;
- input length `1..0x200`;
- chunks of at most `0x20` bytes;
- read-back verification using `RtlCompareMemory`;
- retry logic is present.

## Serial-trigger FPGA handler: 0xCFDC2130

The original diagnostic string identifies this as:

```text
IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA_Handler
```

The handler requires a non-null buffered input and a non-zero input length, then performs direct register I/O while consuming the input byte stream.

## METHOD_NEITHER: 0xCFDD219F

This path passes `Type3InputBuffer` directly to an internal handler and also uses `Irp->UserBuffer`.

Recovered input parsing starts with:

```text
BYTE count
BYTE values[count]
DWORD field1
DWORD field2
```

The derived transfer length is bounded to `0x00FFFFFF`. This path is the primary x86/x64/WOW64 compatibility risk and is documented in detail in [abi-analysis.md](abi-analysis.md).

## Related documentation

- [ABI and x64 compatibility analysis](abi-analysis.md)
- [Hardware register map](hardware-register-map.md)


## User-mode ABI additions discovered in the 2017 hardware-access DLL

Static analysis of the runtime-loaded `lecaladdinhwaccesspcisvr.dll` shows that it can issue two additional Aladdin-family control codes:

```text
0xCFDC2114
0xCFDC21CC
```

These values were not found in the dispatch tree of the captured 2008 `LecS65AcqDrv.sys` build 1002.

They are therefore marked **user-mode observed / kernel support unconfirmed**. The x64 compatibility driver should trace them if XStream sends them, rather than implementing guessed semantics.


## Runtime confirmation from x64 XStream bring-up

After implementing the Dallas/1-Wire path, XStream proceeds beyond hardware
authorization and exercises the following additional controls during startup:

```text
0x00222400
0x00222C04
0x00223004
0x00223040
0x00223100
0xCFDC2110
0xCFDC2180
0xCFDC218C
```

The original 2008 binary confirms the following event/control semantics:

- `0x00222C04`: input exactly one byte; stores a device-level control flag.
- `0xCFDC2180`: input exactly one 32-bit user event handle, no output.
- `0xCFDC218C`: input exactly one 32-bit user event handle, no output.
- `0x00223100`: input exactly three 32-bit user event handles, no output.

The original driver references these event handles with
`ObReferenceObjectByHandle` using `EVENT_MODIFY_STATE`, clears the referenced
events immediately, and keeps device-level references for later interrupt/control
signaling. The x64 compatibility driver now reproduces this behaviour using
`ULongToHandle` so the 12-byte WOW64 ABI remains byte-for-byte compatible.

`0x00222400` does not appear in the captured 2008 S65 dispatch tree and is
therefore intentionally left unsupported until evidence proves otherwise.


### 0x00222C00: millisecond delay

The original handler reads a DWORD millisecond count from the first four input
bytes and accepts an optional fifth control byte. Internally it converts the
millisecond value to a negative 100-ns relative interval and calls
`KeDelayExecutionThread(KernelMode, FALSE, ...)`.

The legacy helper also toggles an auxiliary hardware register around the delay.
The x64 compatibility implementation intentionally reproduces only the
externally visible timing behaviour for now. The hardware toggle is deferred
until runtime evidence shows it is required.
