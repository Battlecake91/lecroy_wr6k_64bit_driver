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


## Callback thunk at 0x12EAE narrowed

Ghidra decompilation shows that `0x12EAE` is only a very small callable
thunk:

```c
uint FUN_00012eae(int param_1)
{
    if (param_1 == 0) {
        return in_EAX & 0xffffff00;
    }

    return FUN_00011e46();
}
```

Therefore `0x12EAE` itself is not the interrupt/event-signalling routine.
It acts as a conditional gate: a zero argument returns immediately with the low
byte of EAX cleared; a non-zero argument forwards to `0x11E46`.

The previous working hypothesis that `0x12EAE` might directly signal the
transport event is not supported by the decompilation. The next function that
must be analysed for the callback path is `0x11E46`.


## Global interrupt-mask write helper 0x11E46

Ghidra decompilation shows that `0x11E46` is not an event-signalling routine.
It conditionally commits the global mask/state value `DAT_0001CE18` to a
hardware register.

Behaviour:

```text
if DAT_0001CD08 != -1:
    DAT_0001CE14 = DAT_0001CE18
    DAT_0001CE44 = DAT_0001CE18
    WRITE_REGISTER_ULONG(DAT_0001CE20, DAT_0001CE18)
    return true-like value
else:
    return false-like value
```

Together with `0x160A8`, this means the receive-arm helper toggles bit 3 in
the global mask and then invokes `0x11E46` through the `0x12EAE` thunk to
push the updated mask into hardware.

Therefore `0x160A8(this, 1)` should be interpreted as enabling the relevant
interrupt/mask bit before the request/response wait, rather than directly
signalling an event.

The actual event signal path is still elsewhere and should be located by
finding references to `KeSetEvent` and tracing callers that operate on the
transport object/event at `this + 0x31`.


## Selected headless-export findings

The first compact headless export closed several previously open handler details.

### Family 0 opcode 0x92 -> 0x1600E

`FUN_0001600e` selects one of three register blocks from object offsets
`+0x25`, `+0x29`, or `+0x2D` using request byte 0/1/2. It then writes a
32-bit value from request offset +3 to:

```text
selected_block->register_base(+0x10) + request_u16(+1)
```

Invalid selector values return status 4. A local status response is generated
through `0x15A88`.

### Family 1 opcode 0x92 -> 0x15DEA

`FUN_00015dea` is the corresponding 32-bit register-read operation. It uses the
same selector mapping (0/1/2 -> object offsets +0x25/+0x29/+0x2D) and the same
16-bit register offset from request +1.

Its local response is 12 bytes:

```text
DWORD +0 = 0
WORD  +4 = 6
WORD  +6 = status
DWORD +8 = register value
```

The response is stored through `0x15A26`.

### Family 2 opcode 0x02 -> 0x163B2

`FUN_000163b2` optionally waits on an object-derived synchronization object at
`this+0x186`, accepts request byte 0 or 1, and writes that value to register
offset `+0x80` of the block referenced by `this+0x29`. Invalid values return
status 4. The exact semantic name of this register/action remains unresolved.

### Family 2 opcode 0x04 / opcode 0 fallthrough -> 0x16414

`FUN_00016414` temporarily disables the state controlled through
`FUN_00016074(this, 0)`, pulses register offset `+0x80` in the
`this+0x29` block by alternating writes of 1 then 0 for the requested 16-bit
count, and restores the state if it had previously been enabled.

This is a pulse-generation/reset-style helper, but the hardware-level meaning
of the pulse is not yet proven.

### Family 2 opcode 0x40 tail -> 0x16490

`FUN_00016490` calls helpers `0x1260E` and `0x126EE` on the object at
`this+0x19E`, then generates a local success response through `0x15A88`.
The two helper semantics remain to be decoded.

### Family 1 opcode 0x50/0x51 -> 0x160DC

`FUN_000160dc` operates on the object referenced by `this+0x182`. It calls
`0x18168` with a request-supplied 32-bit value, validates a returned object's
size against a request-supplied bit/byte count, then calls `0x17478`.
On success it stores an 8-byte local response through `0x15A26`.

The operation is clearly buffer/object-management related, but its acquisition
or DMA semantics are not yet proven.

## Interrupt-path targets identified by headless XREF export

The compact symbol-XREF export identified concrete next-stage functions:

- `KeSetEvent` callers: `0x10816`, `0x1955A`, `0x19966`, `0x11390`
  plus references outside currently defined functions.
- `KeInsertQueueDpc` caller: `0x108D6` plus one undefined-function reference.
- `IoConnectInterrupt` caller: `0x1860E` plus one undefined-function reference.

These are now the primary targets for reconstructing the ISR/DPC/event
completion path. The earlier `0x12EAE -> 0x11E46` path is confirmed to be an
interrupt-mask commit path, not the event-signal path itself.


## Second headless export: interrupt/DPC path and supporting helpers

The second compact export confirmed the primary interrupt pipeline and several
supporting helpers.

### ISR / DPC path

`FUN_000108d6` is the interrupt service routine candidate. It reads an
interrupt/status register at object offset `+0x390`, filters it through the
global masks `DAT_0001CE44` and `DAT_0001CE14`, acknowledges individual
sources, captures secondary status from `+0x340`, and finally queues the DPC
at object offset `+0x1515` via `KeInsertQueueDpc`.

`FUN_0001860e` is a thin `IoConnectInterrupt` wrapper that forwards the
stored interrupt object/configuration fields.

`FUN_00011390` is part of the deferred/event-completion path. It dispatches
multiple callback conditions, signals registered kernel events, and includes
the transport-event path around object offset `+0x10B9`.

`FUN_00010816` is a small helper that signals the event in `param_1[0]`
when `param_1[2] == 1`.

This confirms that the legacy driver follows the expected sequence:

```text
hardware interrupt -> ISR status/ack -> KeInsertQueueDpc -> deferred callback
processing -> KeSetEvent -> waiting request/response path resumes
```

### Global mask bit 2

`FUN_00016074(this, enable)` controls bit 2 (`0x04`) of
`DAT_0001CE18` and commits the mask through the previously decoded
`0x12EAE -> 0x11E46` path.

### Family 2 opcode 0x40 tail helpers

`FUN_0001260e` reads and acknowledges interrupt/status state from the object
at `this+0x19E`, including secondary status bits `0x400..0x4000`.

`FUN_000126ee` writes command/state value 3, then clears/enables the associated
status register with `0xFFFFFFFF` and performs a readback.

Together, the family-2 opcode-0x40 tail is an interrupt/status reset or
reinitialization sequence, though the exact hardware semantic name remains
unproven.

### Family 1 opcode 0x50/0x51 support

`FUN_00018168` walks a linked list using next pointer offset `+0x3C` and
returns the entry whose first DWORD matches the request-supplied identifier.

`FUN_00017478` delegates to `FUN_000171DE` using a size/address quantity
derived from the selected object and translates `STATUS_TIMEOUT (0x102)` into
`0xC00000B5`-style failure. This path remains a strong acquisition/buffer
candidate and `0x171DE` is the next function to decode.


## Third headless export: synchronous transfer engine details

The transfer helper behind family-1 opcode 0x50/0x51 is now substantially clearer.

### 0x171DE synchronous transfer/wait helper

`FUN_000171de(this, param_1, param_2, param_3)`:

1. writes `param_1` through the register wrapper at object offset `+0x58`;
2. stores `param_2` at `this+0xFC`;
3. writes a value from the selected object at `this+0x100` through the register
   wrapper at `+0x80`;
4. resets the event at `selected_object + 0x24`;
5. calls the indirect method at vtable offset `+0x14`;
6. writes the cached register value from `this+0x54` through the register
   wrapper at `+0x04`;
7. writes `param_2` either through the wrapper at `+0x2C` or `+0xD0`
   depending on `param_3`;
8. waits up to 5 seconds on `selected_object + 0x24`;
9. calls the indirect method at vtable offset `+0x18`;
10. translates `STATUS_TIMEOUT (0x102)` to `0xC00000B5`.

This is confirmed to be an event-driven synchronous hardware-transfer operation,
not a passive buffer lookup. The exact meanings of the programmed registers and
the two indirect methods remain to be resolved.

### Register-wrapper helper 0x107FE

`FUN_000107fe` caches the written value at wrapper offset `+0x24` and writes
the same value to the hardware register pointer stored at wrapper offset `+0`.

### Synchronization helper 0x10636

`FUN_00010636` invokes a callback directly when no interrupt object is stored,
otherwise it uses `KeSynchronizeExecution`. This explains how several
deferred-path operations are serialized against the ISR.

### Deferred event helpers

`FUN_000176a2` reads a register from object offset `+0xD8`, returns whether
it was non-zero, and clears it through the register wrapper when set.

`FUN_000176d0` reads the register at object offset `+0x38` and reports
whether bit `0x8000` is set.

`FUN_000157a6` accumulates selected status bits into a 16-bit field at object
offset `+9`, masked by the 16-bit field at `+0x0B`.


## Fourth headless export: transfer-object implementation clues

Commit `c900441` added several functions around the transfer-object
implementation.

### 0x170FA constructor-like initializer

`FUN_000170fa` initializes a large object with several embedded helper objects
via `FUN_00011962`, assigns small configuration values (2 or 4) to the
embedded blocks, initializes another subobject through `FUN_00017f4e`, and
initializes a tail object through `FUN_000170ba`.

This strongly indicates a constructor/initializer for the transfer/acquisition
manager object used by the synchronous path.

### 0x1731C transfer-entry allocation

`FUN_0001731c` validates an input descriptor, allocates a 0x40-byte entry from
nonpaged pool with tag `'Wdm '` in little-endian form, zeroes it, copies
descriptor fields, performs additional setup through `0x1807A`, `0x17F8C`,
and `0x18194`, appends the entry to the linked list rooted at object
`+0x104`, initializes an event in the entry, and stores the current process.

If a mode flag is set it additionally calls `0x1232A` with the caller-supplied
object and current process.

This is now a high-priority acquisition/buffer registration path.

### 0x172A2 transfer-entry removal

`FUN_000172a2` looks up a list entry through `0x18168`, rejects entries with
bit 0 set in field `+0x0C`, unlinks the entry from the linked list rooted at
`this+0x104`, then calls `0x17FD6`.

### 0x16FAC

`FUN_00016fac` is the already-known Dallas/1-Wire ID read path (reset,
`0x33`, read eight bytes, CRC validation with retries) and is unrelated to the
transfer object despite falling inside the broad address sweep.

The next transfer-analysis targets should therefore focus on
`0x18194`, `0x1807A`, `0x17F8C`, `0x17FD6`, `0x1232A`, and the
constructor callers around `0x14212`.


## Fifth headless export: MDL-backed transfer registration

The transfer registration path is now confirmed to use Windows MDLs and locked
user pages.

### 0x1807A: MDL chain construction

`FUN_0001807a` validates the requested size (maximum about 0x06000000 bytes),
optionally probes the caller buffer for write access, then splits the range into
chunks of at most 0x02000000 bytes.

For each chunk it:

- allocates an MDL with `IoAllocateMdl`;
- appends the MDL to a linked chain;
- when operating on user memory, locks the pages with
  `MmProbeAndLockPages`.

This is direct evidence that the legacy acquisition path is built around
pinned user buffers rather than only internal kernel buffers.

### 0x17F8C: nonpaged descriptor/table allocation

`FUN_00017f8c` allocates a fixed 0x33000-byte nonpaged buffer, allocates an
MDL for it, and calls `MmBuildMdlForNonPagedPool`.

This buffer is associated with a transfer-entry object and is likely a hardware
descriptor/scatter-gather table or staging structure. Its exact format is not
yet proven.

### 0x18194: page/segment descriptor builder

`FUN_00018194` walks a chain of memory descriptors and emits pairs of
`{word_count, physical/page-derived address}` into a caller-provided table.
It advances across 4 KiB page boundaries and groups entries in blocks of 0x200,
with one slot used for a continuation/page-pointer style value.

The function stops when it reaches the transfer object's configured word count.
This strongly identifies the 0x33000-byte buffer as a hardware-facing transfer
descriptor table.

### 0x17FD6: transfer-entry cleanup

`FUN_00017fd6` releases, in order:

- the MDL for the fixed 0x33000-byte buffer;
- the nonpaged 0x33000-byte buffer itself;
- another descriptor/MDL chain via `0x17F60`;
- the 0x40-byte transfer entry.

### 0x1232A: process-registration flag update

`FUN_0001232a` walks a process-related list, matches an entry by process
identifier/object, and ORs a supplied flag into the matched entry.

### 0x14212: top-level device/object constructor

`FUN_00014212` is a large constructor for the main device object. It invokes
`0x170FA`, initializes many embedded register-wrapper objects, sets up several
subsystems, and wires shared pointers between them.

This constructor is an important anchor for mapping object offsets to hardware
register blocks and for resolving the indirect methods used by the synchronous
transfer helper.


## Sixth headless export: MDL cleanup and main-object wiring

The latest export further confirms the transfer-memory model.

### 0x17F60: MDL-chain teardown

`FUN_00017f60` walks an MDL chain using the MDL next pointer. For each entry it
optionally calls `MmUnlockPages` and then `IoFreeMdl`.

Together with `0x1807A`, this closes the lifetime of the pinned user-buffer
MDL chain: allocate MDLs, optionally probe/lock user pages, use them for the
transfer, then unlock/free the chain.

### 0x18194 uses the 32-bit MDL layout directly

The fields accessed by `FUN_00018194` match the legacy 32-bit MDL layout:
the function consumes ByteCount/ByteOffset and then walks the PFN array that
starts immediately after the MDL header. It converts those PFNs plus the page
offset into hardware-facing address/length pairs.

This is strong evidence that `0x18194` is the descriptor-table builder for the
board DMA engine.

### 0x11A88 / 0x11AD2: subsystem pointer wiring

`FUN_00011a88` copies one shared subsystem pointer into several fields of the
main object.

`FUN_00011ad2` stores another shared pointer into two fields and forwards it
into an embedded subobject through `0x15348`.

These functions are object-wiring helpers, not transfer execution themselves.

### 0x14142 / 0x1329E / 0x13434

`FUN_00014142` resets the global interrupt masks and initializes several
trace/register-list entries.

`FUN_0001329e` constructs the `CKeTraceControl` subsystem.

`FUN_00013434` constructs the `CKeRegisterList` subsystem.

These are useful for object-layout reconstruction but are not part of the
high-priority DMA execution path.


## Seventh headless export: broad 0x17380-0x17740 sweep

The broad sweep did not reveal a new DMA-programming routine. It mainly
rediscovered the already-decoded board-message transport:

- `0x17578`: RX extraction from BAR1 + 0x600 with continuation/event wait.
- `0x176E6`: TX chunking into BAR1 + 0x420 with TxCount/TxControl submission.
- `0x17F4E`: initializes two constants in an embedded transfer-related object.

The useful conclusion is negative but important: the board DMA execution path
is not located in this contiguous address range. Further work should follow the
transfer-entry fields and vtable/call references rather than continue blind
address sweeps.

In particular, `0x1731C` creates a 0x40-byte transfer entry whose +0x10
buffer and +0x14 MDL are used by `0x18194` to build the chained hardware
descriptor table. The next target is to find every function that consumes those
transfer-entry fields or the descriptor-count/result stored at entry +0x18.


## Eighth headless export: transfer-adjacent register objects

The transfer-entry consumer sweep exposed several support objects around the
board transport and register abstraction.

### 0x1785B: board-message transport register map

`FUN_0001785b` initializes register wrappers for the BAR1 transport block:

- TxControl at +0x400
- RxControl at +0x404
- TxCount at +0x408
- RxCount at +0x40C
- SetIRQ at +0x100
- HWInt at +0x410

It also stores the parent register block at object +0x100 and initializes an
event in the transport object itself.

### 0x179E2: indexed cached register write

`FUN_000179e2` caches 16-bit values selected by bits 16..23 of the requested
32-bit value and writes the full value only when the cached 16-bit value
changes.

Its callers (`0x17BC8`, `0x17C16`, `0x17CDC`) are now high-priority
targets because they are likely to expose the semantics of the indexed control
register programmed through this helper.

### 0x17A36 / 0x17A98 / 0x17B00

These functions implement generic dynamically-sized pool-backed array/buffer
helpers. They are infrastructure rather than direct DMA execution.

The next analysis set therefore follows the callers of `0x179E2` and related
initialization helpers instead of continuing broad address sweeps.


## Ninth headless export: indexed control programming

The caller-focused export clarified the helper at `0x179E2`.

### 0x17BC8

`FUN_00017bc8` writes an array of 32-bit indexed values through `0x179E2`
and then commits a compact control word through another register wrapper.

Its only current caller is `0x12F30`, the already-known type-1/type-2
`CFDC2110` command path. This directly links the indexed-register helper to
the packed command ABI rather than the MDL acquisition-buffer path.

### 0x17C16

`FUN_00017c16` decomposes three 32-bit arguments into five indexed register
writes, using index values 0 through 4 in bits 16..23, then commits `0x105`
through a second register wrapper.

The next caller `0x17D20` is therefore important for identifying the semantic
meaning of this five-word indexed command.

### 0x17CDC

`FUN_00017cdc` validates a structured object through `0x17B72`, then writes
an array at object +0x6C through the indexed register helper. It is called by
`0x12D6A` and `0x17D20`.

### 0x1785B / 0x159E2 wiring

`FUN_000159e2` stores three parent pointers/objects and constructs the BAR1
message-transport subobject through `0x1785B`. The latter registers named
TxControl/RxControl/TxCount/RxCount/SetIRQ/HWInt wrappers and an event.

### DMA API symbol check

The legacy image has no exported/imported symbols named
`MmGetPhysicalAddress`, `IoMapTransfer`, `GetScatterGatherList`, or
`PutScatterGatherList` in the current Ghidra symbol table.

Combined with the direct PFN-array walk already observed in `0x18194`, this
supports the conclusion that the driver constructs the board descriptor table
directly from locked MDLs instead of using the classic Windows scatter/gather
DMA helper API.

### Generic helpers

`0x17DAC`, `0x17DDC`, and related `0x17Axx` functions are generic
pool-backed dynamic buffer/array infrastructure and are lower priority than
their callers.

The next pass follows `0x12F30`, `0x17D20`, `0x12D6A`, `0x17B72`,
and the large hardware-register initializer `0x14847`.


## Tenth headless export: acquisition request programming path

The latest caller-focused export finally ties several previously separate
pieces together.

### 0x12D6A: acquisition transfer orchestration

`FUN_00012d6a` is a high-value acquisition path:

1. derives a chunk/transfer count from the requested byte count;
2. prepares the indexed hardware-control structure through either
   `0x17CDC` or `0x17D20`;
3. calls `0x17478`, which leads into the event-driven synchronous transfer
   helper `0x171DE`;
4. clears pending interrupt/status state through the BAR0 IIM status/clear
   registers;
5. returns the requested byte count through the caller-provided result pointer.

This is now a strong bridge between the user-facing acquisition request,
indexed register programming, and the synchronous hardware transfer/wait path.

### 0x17D20 / 0x17C16: per-channel indexed programming

`FUN_00017d20` walks a channel/configuration list. For each entry it builds a
command beginning with the known `A5FB` signature and opcode-like value
`0x0A/0xE0` in the low bytes, then calls `0x17C16`.

`FUN_00017c16` splits three 32-bit arguments into five indexed writes
(indices 0..4) through `0x179E2`, then commits `0x105` through another
register wrapper.

This appears to be board-side acquisition/channel setup rather than the BAR1
message transport.

### 0x13C84 / 0x13DC6: two acquisition request front-ends

Both functions:

- resolve a registered transfer entry through `0x18168`;
- construct a temporary 0x7C-byte channel/configuration object;
- validate the channel count and total byte size;
- require the requested size to match the registered transfer-entry size;
- call `0x12D6A`;
- tear the temporary object down afterwards.

`0x13C84` receives the transfer identifier directly. `0x13DC6` first maps
caller/process state through indirect methods on the main device object.

These are likely close to the actual IOCTL-facing acquisition read/write
handlers and are high-priority for mapping the public ABI.

### 0x14847: authoritative BAR/register initialization

`FUN_00014847` is the authoritative register-object initializer. It confirms
the BAR0/BAR1/BAR2 offsets already reconstructed elsewhere, including START,
INTST, INTEN, IIMTC/IIMCL/IIMST, MAM*, JTAG*, SPI*, MTT*, GPIO*, clock/divider
registers, SetIRQ/HWInt transport registers, and ONEWIRE/BUZZER.

It also wires the indexed-control object at main-object offset `+0x1086` to
the MAM register group, strongly suggesting the indexed writes above program
the acquisition-memory engine.

The next pass should therefore trace `0x13AE2` (caller of `0x12F30`),
the IOCTL dispatch callers of `0x13C84`/`0x13DC6`, and the MAM helper
`0x17BA6` plus its immediate callees.


## Eleventh headless export: IOCTL-side command and acquisition validation

The latest export closes several important gaps around the user-facing command
and acquisition paths.

### 0x13AE2: IOCTL-side packed command processor

`FUN_00013ae2` parses an input stream into records, iterates over them, and
routes each record either through the local/Dallas path (`0x16C14`) or through
`0x12F30` for the type-1/type-2 indexed command path. It accumulates the
returned byte count and copies the generated response stream back to the caller.

Its only incoming reference is from an as-yet undefined function at
`0x1115B`, making that address a strong candidate for the actual IOCTL
dispatch branch for the packed command request.

### 0x13C84 / 0x13DC6 acquisition front-ends

Both acquisition front-ends now clearly show the request validation contract:

- a transfer entry is resolved through `0x18168`;
- a temporary channel/configuration object is built;
- the requested byte count must equal the registered transfer-entry size;
- invalid channel/count/size combinations return parameter or length errors;
- the validated request is handed to `0x12D6A`;
- the temporary structure is then released with `0x17E58`.

`0x13DC6` additionally resolves/matches caller process state through indirect
main-object methods before the transfer lookup.

### 0x17EE0 channel descriptor encoding

`FUN_00017ee0` appends a channel/configuration byte to an internal dynamic
array, then builds a compact encoded value containing:

- the low 6 bits of the channel/config value;
- a 9-bit index shifted into the upper field;
- a final-entry flag.

That encoded value is inserted into another internal register/configuration
collection through `0x13F3C`.

### 0x17BA6 MAM object wiring

`FUN_00017ba6` simply wires four register wrappers into the MAM helper object:
MAMDAT, MAMPGO, MAMSEQ and MAMRGO, and marks the helper enabled.

This confirms that the indexed control path at main-object +0x1086 is backed by
the MAM register group.

### 0x120DC

`FUN_000120dc` clears bit 16 in the BAR1 GPIODAT register before both the
packed type-1/type-2 command path and the acquisition-transfer path.

### 0x12FDE startup/probe behavior

`FUN_00012fde` writes START=1, waits briefly, reads START back, and selects one
of two initialization sequences depending on whether the bit remains set. It
also programs ITMODE and invokes another helper at `0x1557C`.

The next pass should prioritize the undefined caller at `0x1115B` to recover
the exact IOCTL dispatch branch, plus `0x15710`, `0x15720`, `0x16C14`,
and `0x1557C` to close the packed-command local path and startup side effects.


## Twelfth headless export: packed-record parser and acquisition IOCTL mapping

The latest export confirms the internal structure used by the packed
`0xCFDC2110` front-end and ties both acquisition front-ends to known dispatch
handlers.

### 0x153A2 / 0x15364: packed-record iterator

`FUN_000153a2` initializes an iterator over a caller buffer containing
concatenated records. It walks the stream until the configured end pointer,
counts records, and accumulates the sum of each record's first WORD
(`output_length`).

`FUN_00015364` advances exactly by:

```text
8 + payload_length
```

using the WORD at record +2. This independently reconfirms the previously
recovered packed record layout.

### 0x15710 / 0x15720: type routing predicates

`FUN_00015710` returns true only when record type == 3.

`FUN_00015720` returns true only when record type is 1 or 2.

Together with `0x13AE2`, this makes the top-level command split explicit:

```text
type 3   -> 0x16C14 -> A5FB / 85FB / C5FB signature dispatch
type 1/2 -> 0x12F30 -> indexed control programming
other    -> reject malformed/unsupported record stream
```

### 0x16C14: type-3 signature dispatcher

`FUN_00016c14` directly checks record signature WORD +6:

- `0x85FB` -> `0x169B4`
- `0xA5FB` -> `0x16BBA`
- `0xC5FB` -> `0x16594`

This is the compact dispatcher that connects the packed stream parser to the
three already-decoded type-3 command/response families.

### 0x153xx / 0x154xx output-buffer helpers

The remaining helpers exported in this pass are iterator/buffer plumbing used by
`0x13AE2` to allocate the response buffer, advance through input records and
copy the accumulated output back to the caller.

### Acquisition front-ends map to known dispatch handlers

Incoming references now identify the two acquisition front-ends exactly:

- `0x13C84` is called by `0x141DC`, the handler already mapped to
  `0xCFDC2138`.
- `0x13DC6` is called by `0x141F8`, the handler already mapped to
  `0xCFDD219F` (METHOD_NEITHER).

This is an important ABI result: both the buffered structured-transfer IOCTL
and the METHOD_NEITHER path converge on the same `0x12D6A` acquisition
orchestrator after different user-buffer/process handling.

The exact dispatch branch invoking `0x13AE2` is still represented only by an
undefined incoming reference at `0x1115B`. The exporter has therefore been
extended to emit a raw instruction window when an address is not part of a
defined Ghidra function.
