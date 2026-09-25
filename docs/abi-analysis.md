# ABI and x64 compatibility analysis

## Executive summary

Most of the legacy IOCTL interface is considerably friendlier to an x64 port than expected:

- 26 of 27 recovered IOCTLs use `METHOD_BUFFERED`;
- many requests have fixed byte sizes and consist only of byte/DWORD payloads;
- the generic register-access requests use packed byte/DWORD records, not native pointers.

One request, `0xCFDD219F`, uses `METHOD_NEITHER` and directly consumes user-mode pointers. This is the highest-priority ABI risk.

## IRP fields used by the legacy driver

The disassembly establishes the usual DeviceIoControl data paths:

- buffered requests operate on the system buffer;
- input and output lengths are read from the current IO stack location;
- the `METHOD_NEITHER` request consumes `Type3InputBuffer`;
- that same request also uses `Irp->UserBuffer`.

The x64 replacement must not preserve x86 structure offsets internally. It must reimplement the behaviour using current WDK structures and APIs.

## Raw register read ABI

IOCTL: `0xCFDC21C0`

Two input forms are accepted.

### Legacy form

```c
#pragma pack(push, 1)
typedef struct {
    uint32_t offset;
} LECS65_REG_READ32_LEGACY;
#pragma pack(pop)
```

Input size: 4 bytes.

BAR0 is implied.

Output: one `uint32_t` register value.

### Extended form

```c
#pragma pack(push, 1)
typedef struct {
    uint8_t  bar;       // accepted values 0..2
    uint32_t offset;
} LECS65_REG_READ32_EXT;
#pragma pack(pop)
```

Input size: 5 bytes.

Output: one `uint32_t` register value.

The original driver resolves the selected BAR, adds the supplied offset and calls `READ_REGISTER_ULONG`.

## Raw register write ABI

IOCTL: `0xCFDC21C4`

Two input forms are accepted.

### Legacy form

```c
#pragma pack(push, 1)
typedef struct {
    uint32_t offset;
    uint32_t value;
} LECS65_REG_WRITE32_LEGACY;
#pragma pack(pop)
```

Input size: 8 bytes.

BAR0 is implied.

### Extended form

```c
#pragma pack(push, 1)
typedef struct {
    uint8_t  bar;       // accepted values 0..2
    uint32_t offset;
    uint32_t value;
} LECS65_REG_WRITE32_EXT;
#pragma pack(pop)
```

Input size: 9 bytes.

The original driver verifies DWORD alignment of the offset. It then resolves the BAR, adds the offset and calls `WRITE_REGISTER_ULONG`.

BAR0 offset `0x84` is special-cased because it is the `INTEN` register and the value is cached by the driver.

These packed structures contain no native pointers and can remain byte-for-byte compatible in a native x64 driver.

## Driver version IOCTL

IOCTL `0xCFDC21C8` requires a four-byte output buffer and returns:

```text
0x000003EA = 1002
```

This matches the private-build component of the embedded file version `6.1.1.1002`.

For compatibility, the x64 replacement should initially return the same value unless the caller behaviour proves otherwise.

## METHOD_NEITHER request

IOCTL: `0xCFDD219F`

The top-level handler does the equivalent of:

```text
request = currentStack->Parameters.DeviceIoControl.Type3InputBuffer
status  = InternalHandler(irp, request)
```

The internal handler also reads `Irp->UserBuffer`.

### Recovered request parsing

The input pointer is dereferenced directly. The parser begins with:

1. one byte containing a count;
2. that many one-byte values;
3. a 32-bit value;
4. another 32-bit value.

The routine multiplies the count and the two DWORD values to derive a transfer size and rejects values above `0x00FFFFFF`.

Additional observed checks include a `0x400` boundary/alignment rule.

The request is therefore not merely a no-argument control operation. It configures a substantial transfer path and is associated with MDL/locked-buffer handling elsewhere in the driver.

### x64 implications

The request record seen so far contains fixed-width fields rather than embedded native pointers. That is encouraging.

However, `Type3InputBuffer` and `Irp->UserBuffer` themselves are native user pointers. A replacement x64 driver must:

- treat them as untrusted user addresses;
- correctly handle 32-bit WOW64 callers;
- probe/capture user data before use;
- avoid reproducing the legacy driver's direct unchecked dereferences;
- determine whether the LeCroy x64 application emits the same packed request layout.

A compatibility implementation can use `IoIs32bitProcess` if separate 32-bit and 64-bit request layouts turn out to exist.

## Other fixed-size contracts already recovered

| IOCTL | Contract observed |
|---|---|
| `0x00222C00` | input at least 4 bytes; optional fifth byte is consumed when length is exactly 5 |
| `0x00222C04` | input exactly 1 byte |
| `0x00223000` | input exactly `0x108` bytes |
| `0x00223004` | variable output/query pattern; 4-byte output returns required/current size |
| `0x0022303C` | input exactly `0x10A` bytes |
| `0x00223040` | variable output; 4-byte size-query mode and `0x10A` special mode are present |
| `0x00223044` | output exactly 4 bytes; returns a register value |
| `0x00223080` | output exactly 8 bytes |
| `0x00223084` | output 1..`0x200` bytes |
| `0x00223088` | input 1..`0x200` bytes |
| `0x00223100` | input exactly 12 bytes, no output |
| `0xCFDC2124` | input exactly 12 bytes; successful path returns 4 bytes |
| `0xCFDC2128` | input exactly 4 bytes |
| `0xCFDC212C` | unconditional `STATUS_NOT_IMPLEMENTED` in this build |
| `0xCFDC2190` | input exactly 29 bytes |
| `0xCFDC2194` | output exactly 29 bytes |
| `0xCFDC21C0` | input 4 or 5 bytes; output 4 bytes |
| `0xCFDC21C4` | input 8 or 9 bytes |
| `0xCFDC21C8` | output exactly 4 bytes; value 1002 |

Several other handlers still require semantic naming, but their numeric dispatch values are already known.

## Porting strategy implied by the ABI

The replacement should initially preserve the numeric IOCTL interface exactly.

Recommended approach:

1. implement only validation and deterministic compatibility responses;
2. implement raw BAR read/write;
3. implement version and simple fixed-size controls;
4. implement Dallas/1-Wire operations;
5. implement interrupt path;
6. implement acquisition/DMA requests;
7. implement the `METHOD_NEITHER` path last, with explicit WOW64 handling and safe user-buffer capture.

This lets hardware bring-up happen without depending on the most dangerous legacy ABI first.
