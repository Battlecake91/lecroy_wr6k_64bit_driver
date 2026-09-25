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


## 0xCFDC2110 packet-list structure confirmed at runtime

The startup trace with 96-byte input capture confirms that `0xCFDC2110` accepts
one or more packed records concatenated in the input buffer.

Each record begins with:

```c
#pragma pack(push, 1)
typedef struct {
    uint16_t output_length;
    uint16_t payload_length;
    uint16_t type;
    uint16_t signature;
    uint8_t  payload[payload_length];
} LECS65_TRANSFER_RECORD;
#pragma pack(pop)
```

The next record starts at `record + 8 + payload_length`.

The original handler sums `output_length` across all records and rejects a
request if the sum exceeds the DeviceIoControl output-buffer length.

The recovered type dispatch is:

- type `3`: data-transfer record; signatures `0xA5FB`, `0x85FB` and
  `0xC5FB` are recognized by the original binary;
- types `1` and `2`: routed through a separate control/programming helper.

Observed x64 XStream startup examples:

```text
IN=18 OUT=6
06 00 0A 00 03 00 FB A5
40 02 40 01 52 45 53 45 54 00

IN=22 OUT=270
06 00 04 00 03 00 FB A5 40 01 99 00
08 01 02 00 03 00 FB 85 40 00

IN=24 OUT=14
06 00 06 00 03 00 FB A5 40 00 88 00 DF FF
08 00 02 00 03 00 FB 85 40 00

IN=94 OUT=46
06 00 4C 00 03 00 FB A5 <76-byte payload>
28 00 02 00 03 00 FB 85 40 00
```

For the observed records, the first payload byte is `0x40`; the second byte
selects a sub-operation in the original `0xA5FB` / `0x85FB` handlers.

The x64 compatibility driver intentionally does not yet emulate the data-transfer
side effects. The exact output bytes and hardware effects will be captured from
the working x86 reference driver before implementing this path.


## CFDC2110 reference-driver response captures

Direct replay against the working 32-bit reference driver confirmed the packed
record parser and the A5FB/85FB request/response relationship.

Important safety note: `0xCFDC2110` is stateful and can perform real hardware
operations. Replaying arbitrary or malformed requests outside the normal XStream
sequence can destabilize the acquisition hardware. Further reference-system
testing should therefore avoid raw replay unless a specific missing fact cannot
be recovered statically.

Observed reference transactions:

### A5FB RESET command

Input:

```text
06 00 0A 00 03 00 FB A5
40 02 40 01 52 45 53 45 54 00
```

Output, 6 bytes:

```text
00 00 00 00 00 00
```

### A5FB command followed by 85FB response fetch, 270-byte output

Input:

```text
06 00 04 00 03 00 FB A5 40 01 99 00
08 01 02 00 03 00 FB 85 40 00
```

Output begins:

```text
00 00 00 00 00 00
00 00 00 00 02 00 02 00
FF FF FF FF ...
```

The first six bytes are the A5FB command result. The following 264 bytes belong
to the 85FB record.

### A5FB command followed by 85FB response fetch, 14-byte output

Input:

```text
06 00 06 00 03 00 FB A5 40 00 88 00 DF FF
08 00 02 00 03 00 FB 85 40 00
```

Output:

```text
00 00 00 00 00 00
00 00 00 00 02 00 00 00
```

### A5FB command followed by 85FB response fetch, 46-byte output

Input contains an A5FB record with a 76-byte payload followed by an 85FB
`40 00` response-fetch record.

Output:

```text
00 00 00 00 00 00
00 00 00 00 22 00 00 00 FF FF
78 DC 06 C0 78 DC 06 C0 78 DC 06 C0 78 E0 06 D0
78 DC 06 C0 78 DC 06 C0 78 DC 06 C0 78 E0
```

The values after the 85FB response header are hardware/state dependent and must
not be treated as universal constants.

### Handler relationship recovered from the original binary

For type-3 records:

- signature `0xA5FB` dispatches to the command side;
- signature `0x85FB` dispatches to the response side;
- signature `0xC5FB` dispatches to a third related path.

For the startup traffic captured so far, A5FB commands are followed by an 85FB
record whose payload is `40 00`. The 85FB handler retrieves data from an
internal response buffer produced or updated by the preceding command. This
explains why replaying isolated records is not equivalent to observing the
normal XStream sequence.


## CFDC2110 startup routing details

Static disassembly now confirms the routing used by the XStream startup records.

For A5FB records with a payload beginning in `0x40`, `payload[1]` selects
one of three command families and `payload[2]` is the command opcode.

The A5FB side writes a six-byte direct result consisting of a zero DWORD followed
by a 16-bit command status. This matches the six zero bytes observed for the
successful startup commands.

Observed routing:

- `40 02 40 ...`: family 2, opcode `0x40`, board reset / interrupt-cleanup path.
- `40 01 99 00`: family 1, opcode `0x99`, generic BAR1 message transmit.
- `40 00 88 00 DF FF`: family 0, opcode `0x88`, status-mask update plus generic BAR1 message transmit.
- `40 01 42 ...`: family 1, opcode `0x42`, direct JTAG transaction.

For the generic-transmit commands the bytes sent to the board begin at the
record signature, not at the host-side record header. The observed packets are:

```text
FB A5 40 01 99 00
FB A5 40 00 88 00 DF FF
```

The following 85FB record with payload `40 00` retrieves the pending response.
When a board reply is needed, the legacy driver sends the fixed fetch packet
`FB 85 40 00` through the BAR1 message transport and receives the response
from the BAR1 RX window.

The opcode-`0x42` path is confirmed to use the JTAG registers at BAR1 offsets
`0x20`, `0x24`, and `0x28`. For the captured 256-bit request it performs
16 iterations of 16 bits, reads the upper 16 bits of each JTAG input DWORD, and
builds a response containing 32 data bytes. This explains the observed 40-byte
85FB response and its `0x22` response-length field.


## Runtime correction: partial CFDC2110 execution disabled

A broader XStream run after enabling the first CFDC2110 implementation exposed
many additional A5FB subcommands beyond the initially decoded startup set,
including opcodes `0x81`, `0x84`, `0x4A`, `0x92`, and additional
`0x42` forms.

The partial implementation returned `STATUS_SUCCESS` for these records while
often producing only placeholder/fallback response bytes. XStream then advanced
into later initialization and acquisition code with invalid board state,
resulting in multiple application errors and missing/incorrect acquisition
traces.

The x64 driver therefore currently keeps the recovered CFDC2110 parser,
transport helpers, and documentation in-tree but does not execute CFDC2110
hardware side effects. The IOCTL is temporarily trace-only and returns
`STATUS_INVALID_DEVICE_REQUEST` until the full set of startup/runtime
subcommands used by XStream is decoded.

This is a deliberate safety rollback. Returning a truthful failure is preferable
to returning structurally valid but semantically wrong board responses.


## CFDC2110 family dispatchers confirmed from Ghidra

The decompiled A5FB handler confirms that the byte at record offset +8 must be
`0x40`, and the byte at +9 selects one of three families:

```text
family 0 -> 0x16A66
family 1 -> 0x165A6
family 2 -> 0x166A8
```

The command opcode is the byte at record offset +10.

### Family 0 dispatcher (0x16A66)

Observed opcode routing:

- `0x42` -> `0x15ACC`
- `0x4A`, `0x84`, `0x86`, `0x87`, `0x96`, `0x97`, `0xA1`, `0xA2`
  -> generic send helper `0x16168`
- `0x85` -> `0x16962` followed by generic send helper `0x16168`
- `0x88` -> clears bits in an internal 16-bit mask; special handling for masks
  `0x0080` and `0x0800`, otherwise falls through to the generic send helper
- `0x89` and several lower commands -> status/helper `0x16066`
- `0x90` -> `0x15E80`
- `0x91` -> `0x15F7C`
- `0x92` -> `0x1600E`
- `0xA0` -> `0x15FD8`

The generic helper `0x16168` passes `record + 6` and
`payload_length + 2` into the transport object at `this + 0x31`. This
confirms that the board-side packet begins at the A5FB signature and includes
the complete payload.

### Family 1 dispatcher (0x165A6)

Observed routing:

- `0x42` -> `0x15C7E` (JTAG path)
- `0x4A`, `0x81`, `0x82`, `0x90`, `0x91`, `0x96`, `0x97`, `0x99`
  -> `0x15DB8`
- `0x50`, `0x51` -> `0x160DC`
- `0x92` -> `0x15DEA`
- `0xA0` -> `0x15B64`
- `0xA1` -> `0x15BCE`
- `0xA2` -> `0x15C26`
- `0x40`, `0x60`, `0x70`, `0x80`, `0x83`
  -> helper/status path `0x15A88(..., 0x10)`

`0x15DB8` is confirmed as a generic transmit wrapper: it forwards
`record + 6` with `payload_length + 2` to the transport object at
`this + 0x31`; on success it sets the driver's response-pending flag at
`this + 0x24`.

### Family 2 dispatcher (0x166A8)

Observed routing:

- `0x00` -> `0x1634E`, then falls through to `0x16414`
- `0x01` -> `0x1621A`
- `0x02` -> `0x163B2`
- `0x03` -> generic send helper `0x16168(..., 0x19)`; also writes 10 to
  driver state at `this + 0x196`
- `0x04` -> `0x16414`
- `0x05` -> writes 7 then 3 to the register-wrapper object at
  `this + 0x15E`
- `0x06`, `0x07`, `0x08` -> `0x16066`
- `0x09` -> writes 3 to the same `this + 0x15E` register wrapper
- `0x0A` -> writes 2 to the same register wrapper
- `0x10` -> `0x164B8`
- `0x40` -> `0x16066` followed by `0x16490`

The family-2 register-wrapper identity at `this + 0x15E` still needs to be
mapped back to its constructor/register name before it is implemented in the
x64 driver.


## Generic board-message transmitter 0x176E6 confirmed

Ghidra decompilation of `0x176E6` confirms the legacy board-message transmitter.

Inputs:

```text
param_1 = pointer to 16-bit message words
param_2 = byte length
param_3 = timeout scale
```

The helper rejects odd byte counts. For even lengths it converts the byte count
to a 16-bit word count and transmits the message in chunks of at most `0x78`
words.

For each chunk:

1. poll helper `0x17560` until the transport is ready;
2. if not ready, sleep for `100000` 100-ns units = 10 ms and retry;
3. allow at most `param_3 * 100` retries, so `param_3` corresponds to an
   approximately one-second timeout scale;
4. write each 16-bit source word to a 32-bit-spaced BAR1 TX slot beginning at
   offset `0x420`;
5. write the remaining word count through the wrapper object at `this + 0x60`;
6. write the TX command through the wrapper at `this + 0x10`.

The TX command uses:

```text
bit 15    start / submit
bit 14    continuation
bits 7:0  words in this chunk
```

For chunks larger than `0x78` words, the command uses `0x4078`; the final
chunk clears the continuation bit and carries its actual word count.

This confirms that the BAR1 transport reconstruction should preserve the
legacy 16-bit-word / 32-bit-slot layout and continuation protocol.

## Internal response-buffer builder

Ghidra decompilation of `0x15A88` and `0x15A26` confirms the format and
lifecycle of a locally generated response.

`0x15A88(status)` creates this eight-byte record:

```text
DWORD 0x00000000
WORD  0x0002
WORD  status
```

It passes that record to `0x15A26`.

`0x15A26`:

- clears the state byte at `this + 0x24`;
- frees any previously allocated response buffer at `this + 0x1D`;
- allocates and copies the new response;
- stores its byte length at `this + 0x21`;
- sets the state byte at `this + 0x23` to 1.

Therefore `this + 0x23` is the confirmed "local response buffer available"
flag. The separate state byte at `this + 0x24` has a different role and must
not be conflated with the local-response-ready flag.


## CFDC2110 transport-ready and 85FB response routing confirmed

Further Ghidra decompilation clarifies the generic board-message send and
response-fetch paths.

### TX-ready helper 0x17560

`0x17560` reads the transport control register through the wrapper at
transport-object offset `+0x10` and returns ready only when:

```text
bit 15 == 0
low byte == 0
```

This matches the polling condition reconstructed from the transmitter.

### Family-1 generic send wrapper 0x15DB8

`0x15DB8` calls the generic transmitter at `0x176E6` with:

```text
buffer = record + 6
length = record.payload_length + 2
timeout_scale = caller supplied
```

If the transmitter reports failure, the wrapper returns protocol status `8`.
On success it sets the state byte at `this + 0x24` to 1 and returns status 0.

Therefore `this + 0x24` is confirmed as a board-response-pending / transport
state flag, distinct from the local-response-buffer-ready flag at
`this + 0x23`.

### Family-0 generic send wrapper 0x16168

`0x16168` also calls `0x176E6` with `record + 6` and
`payload_length + 2`, using a fixed timeout scale of 1.

The decompiled function does not consume the transmitter return value. Instead,
its third argument controls whether it returns protocol status 8 or marks
`this + 0x24` as pending and returns 0. All currently observed callers pass a
non-zero value. This legacy behaviour should be reproduced only after confirming
the corresponding call sites and not "corrected" merely because it looks odd.

### 85FB handler 0x169B4

The 85FB handler first requires the byte at record offset `+8` to be `0x40`.
The byte at `+9` selects the response form.

#### selector 0

The handler calls `0x167F4` to obtain a response-buffer pointer and length.
If retrieval fails or the returned length is zero, it clears the WORD at output
offset +4.

Otherwise it copies:

```text
min(requested_record_output_length, returned_response_length)
```

bytes from the returned buffer into the CFDC2110 output record.

This is the normal response-fetch path used by the observed `85FB 40 00`
records.

#### selector 1

The handler constructs a direct state response from driver fields:

```text
DWORD 0
WORD  4
WORD  value at this + 0x0B
WORD  value at this + 0x09
```

The resulting response is 10 bytes.

#### selector 2

The handler returns the normal A5FB-style status response with status `0x10`.

Other selectors return the same status-response format with status `2`.

The next critical function for the normal fetch path is `0x167F4`, which
decides whether the response comes from the locally generated buffer or from
the board transport.


## CFDC2110 normal 85FB fetch path fully reconstructed

Ghidra decompilation of `0x167F4` and `0x15A08` closes the normal
A5FB -> board/local response -> 85FB path.

### Status response helper 0x15A08

`0x15A08(record, output, status)` writes the six-byte direct status response:

```text
DWORD 0
WORD  status
```

This is the exact format used by the A5FB family dispatcher.

### Response selector helper 0x167F4

The function uses two independent state bytes:

```text
this + 0x23  local response buffer available
this + 0x24  board response pending
```

and the shared response storage:

```text
this + 0x1D  response-buffer pointer
this + 0x21  response-buffer byte length
```

#### No board response pending (this+0x24 == 0)

If a local response is marked ready (this+0x23 != 0), the helper simply returns
the existing response pointer and length, then clears the local-ready flag.

If no local response is ready, it allocates and returns the default eight-byte
response:

```text
DWORD 0
WORD  2
WORD  0x20
```

and clears both state flags accordingly.

#### Board response pending (this+0x24 != 0)

The caller-provided requested output length is used to allocate a fresh response
buffer, pre-filled with `0xFF`.

The helper then issues the fixed four-byte board fetch request:

```text
FB 85 40 00
```

through `0x1619A`.

The payload destination is:

```text
response_buffer + 6
```

and the maximum board payload length is:

```text
requested_output_length - 6
```

On successful board transport:

```text
DWORD +0 = transport return/status
WORD  +4 = returned payload length
BYTE  +6 = board payload
```

On transport failure the response is converted to:

```text
DWORD +0 = non-zero transport error
WORD  +4 = 2
WORD  +6 = 0x20
```

After either path, both the local-ready and board-pending flags are cleared.

The helper returns success as a boolean-style low byte, while output parameters
carry the response pointer and length.

This means the normal `85FB 40 00` response path is now structurally complete.
The next remaining transport function is `0x1619A`, which performs the
combined board request/response exchange used by the fetch operation.


## Combined request/response transport 0x1619A

Ghidra decompilation of `0x1619A` confirms the synchronous request/response
transaction used by the normal `85FB 40 00` fetch path.

The function:

1. clears the caller's returned-length field;
2. uses the transport object at `this + 0x31`;
3. resets the event object at that same transport-object address;
4. calls `0x160A8(this, 1)` to arm/enable the receive side;
5. transmits the request with `0x176E6(..., timeout_scale=1)`;
6. waits on the transport event with a relative timeout derived from
   `this + 0x196` seconds;
7. immediately after constructing the timeout, copies the default/reload value
   from `this + 0x19A` into `this + 0x196`;
8. if the event wait succeeds, calls `0x17578` to copy the received payload
   into the caller buffer and return its actual length.

Thus the legacy response fetch is event-driven rather than pure polling:

```text
arm RX
-> transmit FB 85 40 00
-> wait for transport event
-> copy RX payload
```

This also explains why a polling-only x64 approximation is insufficient for
full behavioural compatibility: the original driver couples the board transport
to an event signalled by its interrupt/DPC path.

The next transport functions to recover are:

- `0x160A8`: receive-arm / interrupt-enable helper;
- `0x17578`: RX-buffer extraction helper;
- the ISR/DPC path that signals the transport event at `this + 0x31`.


## Receive-arm and RX extraction helpers

Ghidra decompilation of `0x160A8` and `0x17578` completes most of the
transport-side receive path.

### Receive arm helper 0x160A8

`0x160A8(this, enabled)` updates global state bit 3 in `DAT_0001CE18`:

```text
enabled == 0  -> clear bit 3
enabled != 0  -> set bit 3
```

It then calls through the legacy framework/object returned by `0x10A88`,
passing code location `0x12EAE` and the object/state pointer stored at
`this + 0x19E`.

This is consistent with arming/enabling the receive/interrupt path before the
request/response wait, but the exact framework-level meaning of the callback
still needs the `0x12EAE` path to be traced.

### RX extraction helper 0x17578

`0x17578` reads the RX control register through the transport wrapper at
`this + 0x38`.

The RX control word is interpreted as:

```text
bit 15    data ready
bit 14    continuation
bits 7:0  number of 16-bit words in this chunk
```

When bit 15 is set and the low byte is non-zero:

1. validate that the chunk word count is below `0x79`;
2. validate that appending `word_count * 2` bytes does not exceed the caller
   capacity;
3. read one 32-bit-spaced slot per word starting at BAR1 offset `0x600`;
4. keep the low 16 bits of each slot and append them to the caller buffer;
5. reset the transport event;
6. acknowledge/clear the RX-ready and count fields through the wrapper at
   `this + 0x38`.

If bit 14 (continuation) remains set, the helper waits on the transport event
again with a relative timeout of approximately 5 seconds before reading the
next chunk.

Odd output capacities and invalid chunk sizes return `STATUS_INVALID_PARAMETER`
(`0xC000000D`). A continuation wait timeout returns `0x102`.

This confirms the complete RX data layout:

```text
BAR1 + 0x600 + 4*n   -> one 16-bit response word per 32-bit slot
```

and confirms that multi-chunk receive completion is interrupt/event driven.


## Callback target 0x12EAE identified, semantics still open

The receive-arm helper `0x160A8` registers/passes code location
`0x12EAE` through the legacy framework object returned by `0x10A88`.

Ghidra shows many cross-references to `0x12EAE`, including from the
receive-arm helper and several other control/setup routines. The first
instruction at the target is:

```asm
CMP dword ptr [ESP + 0x4], 0
```

This proves that `0x12EAE` is callable code taking at least one stack
argument, but the currently captured first instruction is not sufficient to
determine whether it is an ISR/DPC callback, a framework dispatch thunk, or
another event/interrupt helper.

The full function body beginning at `0x12EAE` must be decompiled before its
role is assigned.
