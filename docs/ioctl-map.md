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

## Legacy START/FVER read: 0x00223044

Original `FUN_00012D24` requires an output buffer of exactly four
bytes, reads original main-object register pointer `+0x138` and
returns the DWORD with `Information=4`. Original initialization
`FUN_00014847` points this at BAR0+0x000.

**2026-09-29 real-scope verification:** the native x64 read-only
handler returned `0x00000002` to `lecdiag start-register`; its
independent generic register-read reference for BAR0+0x000 returned
the same `0x00000002`. The diagnostic explicitly reported PASS.
This establishes the live positive read path, not the broader
XStream/AP015 regression or every error-length case.

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

**Purpose and implementation distinction (user clarification,
2026-09-29):** the PCI card's U11 DS2433 is the
**XStream license-key EEPROM**, not the EEPROM for
the five I2C-only front ProBus probe sockets.
The original x86 binary implements all three Dallas
IOCTLs below. The current x64 source implements
`0x00223080` (eight-byte ROM ID, validated
CRC-8) and `0x00223084` (1..512 bytes from
EEPROM address zero), but does **NOT** yet
define/dispatch `0x00223088` EEPROM write.
Do not mistake its inclusion in the recovered
original x86 dispatch table for x64 capability.
The content may include sensitive XStream
license keys (user expects possible plaintext,
not yet verified). No raw dumps, license data or
full-memory trace excerpts belong in GitHub.
New `lecdiag dallas-backup` is read-only,
saves 512 binary bytes after two matching
full reads and matching ROM IDs, and verifies
the saved file. It has not yet been compiled
or exercised on the scope. Details:
[`dallas-license-memory-test-plan.md`](dallas-license-memory-test-plan.md).

**Physical board corroboration (user-provided `PCI Card.pdf`, page
1, 2026-09-29):** the PCI interface itself has `U11
DS2433` (`ID Chip`) on net `ID_DATA` into
Spartan-IIE `U3 XC2S200E`. The recovered
`BAR2+0x040 ONEWIRE` protocol and three Dallas
memory/ROM IOCTLs below are therefore consistent
with a **PCI-card-local 1-Wire ID device**, NOT
the distinct front-panel **I2C** EEPROM used
during AP015 recognition following an analog ADC
probe-class value. The schematic alone does
not expose the U3 RTL for BAR2-to-ID_DATA routing.
Further information:
[`pci-card-acquisition-board-topology.md`](pci-card-acquisition-board-topology.md).

### 0x00223080: IOCTL_GET_DALLAS_ID

- non-null system buffer required;
- output length exactly 8 bytes;
- 8-byte ID returned on success.

### 0x00223084: IOCTL_READ_DALLAS_MEMORY

- non-null system buffer required;
- output length `1..0x200`;
- requested byte count returned on success.

### 2026-09-29 runtime proof: XStream really sends unsupported Dallas WRITE on x64

The user's private
`xstream_trace_20260929_011858.jsonl` captures
a **single real** `0x00223088` from XStream
running WOW64 under the replacement x64 driver:
seq **522**, t~**57.303140 s**,
input length **512**, output length **0**,
status **`0xC0000010 STATUS_INVALID_DEVICE_REQUEST`**,
Information 0. The trace has exactly
608 gap-free entries over ~63.0224566 s,
with this **sole failure** (607 success).
Initial READ_DALLAS_MEMORY seq 3
succeeds with output/info 512;
an immediate READ_DALLAS_MEMORY
seq 523 after the failed write
again succeeds with output/info
512, and the two captured 128-byte
read previews are identical. Do
not claim full 512-byte equivalence
from truncated previews alone.
There are 93 differing byte offsets
between intended WRITE and initial
READ in their common first 128
captured bytes, with no raw key
material disclosed here. The
full 512-byte intended write is
not present in JSONL; WRITE input
preview includes only its first
256 bytes.

The current x64 `driver/Ioctl.c`
dispatch initializes
`status = STATUS_INVALID_DEVICE_REQUEST`
and has Dallas 0x80/0x84 cases
but **no 0x88 case**, so this
exactly explains the user's
XStream Delete action not surviving
application restart. This is
a known native x64 compatibility gap,
NOT evidence of a failed actual
physical DS2433 scratchpad write.
No fake STATUS_SUCCESS should be
returned without real EEPROM
programming, since XStream passes
an entire intended memory image.

The source of the original
2008 x86 `0x00223088` handler
is at **VA `0x11F54`** in
the recovered dispatch. Added
`11f54`, `asm:11f54`
and `xref:11f54` to
`ghidra_scripts/targets.txt`
to export precise original writer
C/instructions/references before
porting 32-byte chunks, scratchpad
COPY and readback. Avoid assuming
bare DS2433 write sequences from
the IOCTL buffer shape alone.

**Trace privacy:** The currently
uploaded JSONL contains license
data previews for read/write.
Do not reproduce it publicly;
future rebuilt `lecdiag`
JSONL now redacts `input_hex`
for WRITE, `output_hex`
for READ and ROM-ID while keeping
all numeric status/size fields.
No kernel write support was
added in this documentation/
diagnostic change. See
`docs/dallas-license-memory-test-plan.md`.

### 0x00223088: IOCTL_WRITE_DALLAS_MEMORY (original x86 only)

- non-null system buffer required;
- input length `1..0x200`;
- chunks of at most `0x20` bytes;
- read-back verification using `RtlCompareMemory`;
- retry logic is present.
- **Native x64 replacement has not implemented or hardware-tested
  this IOCTL.** Do not use the original license-bearing
  PCI card as an initial destructive write/erase test;
  validate on a disposable DS2433 and retain verified
  private backup/restore capability first.

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
`this+0x186`, accepts request byte 0 or 1, and writes that value to BAR1
`MTTCTL` (`+0x80`). Invalid values return status 4.

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


## Thirteenth headless export: exact DeviceControl dispatch branch recovered

The raw instruction-window export around `0x1115B` resolves the previously undefined caller of `0x13AE2`.

The legacy DeviceControl dispatch code directly compares `EAX` with `0xCFDC2110` and, on match, calls `FUN_00013AE2` from `0x1115B`. This confirms at instruction level:

```text
IOCTL 0xCFDC2110 -> FUN_00013AE2
```

The same raw window independently reconfirms neighboring branches for Dallas ID/read/write, `0x00223100`, `0xCFDC2124`, `0xCFDC212C`, `0xCFDC2130`, `0xCFDC2138`, `0xCFDC2180`, `0xCFDC2184`, and `0xCFDC218C`.

### Acquisition wrapper details

`FUN_000141DC`, the `0xCFDC2138` handler, passes the first DWORD of the buffered request as the transfer identifier and `buffer + 4` as the channel/config payload to `0x13C84`.

`FUN_000141F8`, the `0xCFDD219F` METHOD_NEITHER handler, forwards the request object to `0x13DC6` and sources the channel/config payload through the pointer stored at request-object offset `+0x60`, subfield `+0x10`.

This confirms the two front-ends share the same acquisition semantics but differ in caller-memory handling.

### Delay-helper hardware side effect

`FUN_00015536` writes a boolean through the register wrapper stored in the delay-helper object. Therefore `FUN_0001557C` asserts a hardware line before `KeDelayExecutionThread` and deasserts it afterwards. The exact named register should be resolved before reproducing this side effect in the x64 driver.


## Fourteenth headless export: late DeviceControl dispatch and completion semantics

The raw windows from `0x11200..0x1138D` now recover the late half of the DeviceControl dispatcher and the common completion epilogue.

Confirmed direct branches:

- `0xCFDC218C` -> `0x12B34`
- `0xCFDC2184` -> inline success with `IoStatus.Status = 0` and `Information = 0`
- `0xCFDC2180` -> `0x128F8`
- `0xCFDC2138` -> `0x141DC`
- `0xCFDC2130` -> `0x11CFF`
- `0xCFDC212C` -> `0x11C5E`
- `0xCFDC2190` -> `0x13A40`
- `0xCFDC2194` -> `0x12BAE`
- `0xCFDC21C0` -> `0x13954`
- `0xCFDC21C4` -> `0x1272A`
- `0xCFDC21C8` -> `0x12832`
- `0xCFDC2400` -> `0x13A2E`
- `0xCFDD219F` -> `0x141F8`

Unrecognized IOCTLs fall through to a logging path and are completed with `STATUS_INVALID_PARAMETER (0xC000000D)` and zero information.

### Common DeviceControl completion

After the selected handler returns, the dispatcher releases its mutex and distinguishes `STATUS_PENDING (0x103)` from completed requests. Non-pending requests are logged on failure and then completed through the common IRP-completion helper. This explains why `0x13AE2`, `0x141DC`, and `0x141F8` branch slightly differently before converging on the same completion tail.

The raw window also shows the next recognized function at `0x11390`, confirming the DeviceControl function ends immediately before the already-decoded deferred/DPC helper.


## Fifteenth headless export: remaining DeviceControl handler semantics

This pass resolves most of the handlers that were previously known only by dispatch address.

### 0xCFDC2124 -> 0x11BDC

The handler requires exactly 12 input bytes. It invokes an indirect main-object method at vtable offset `+0x0C` with the caller buffer plus the process/registration subsystem at `this+0xE98`. On success it replaces the first DWORD of the system buffer with the returned identifier/handle and reports 4 output bytes. This is strongly consistent with creating/registering a process-scoped object used by later acquisition paths.

### 0xCFDC2128 -> 0x11C36

The handler requires exactly 4 input bytes and forwards that token to main-object vtable method `+0x08`, now resolved as `0x172A2`. It removes the matching transfer entry from the list and frees its descriptor MDL/table, source MDL chain, and entry allocation.

### 0xCFDC212C -> 0x11C5E

Confirmed again as a trivial `STATUS_NOT_IMPLEMENTED` handler.

### 0xCFDC2130 -> 0x11CFF

The serial-trigger FPGA handler validates a non-null, non-empty input stream, reads BAR1 `GPIODAT` through the register wrapper at main-object offset `+0x318`, and for each input byte replaces the masked `0xE000` field before writing the register back. This confirms that the programming stream is bit-banged through the GPIO register rather than sent through the BAR1 message transport.

### 0xCFDC2180 and 0xCFDC218C

`0x128F8` and `0x12B34` are parallel event-registration handlers. Both require a four-byte input handle and zero output bytes, reference/associate the event with the current process, clear the event, and set a device-level pending/enable flag. `0x128F8` additionally checks current hardware/status state through `0x157B8` and may immediately signal the event through `0x10816`.

### 0xCFDC2190 -> 0x13A40

This 29-byte input path is now identifiable as an interrupt/error-mask control request. It requires structure field 1 to equal 2. Structure field 2 controls global mask bit 1 and is inverted before being written to BAR0 `ERRM` at offset `0x008`; the updated interrupt/mask state is then committed through the synchronized `0x12EAE` path.

### 0xCFDC2194 -> 0x12BAE

This is the paired 29-byte output/status request. It returns a zeroed 29-byte structure with field 1 set to 2 and field 2 populated from device state at `this+0x116A`, then clears that stored state.

### 0xCFDC2400 -> 0x13A2E

This handler is only a wrapper around `0x12EDE`; the latter is the next semantic target.

### DeviceControl completion helpers

`0x10798` writes the completion status into the IRP, calls the internal IRP bookkeeping helper `0x1955A`, and completes the request with `IofCompleteRequest`.

`0x10750`, `0x1076E`, and `0x1919A` are diagnostic/logging helpers rather than hardware logic.

### 0x00222C00 hardware side effect identified

The delay helper object at main-object offset `+0x10F2` is initialized in `0x14847` with the BAR2 `BUZZER` register wrapper at offset `0x000`. Therefore the previously observed `0x1557C -> 0x15536` side effect is now identified exactly: the legacy delay IOCTL asserts the buzzer register before sleeping and deasserts it afterwards when the helper is enabled.

This removes the ambiguity around the auxiliary hardware toggle. Reproducing that buzzer pulse in the x64 driver remains a compatibility choice rather than an unknown register hazard.


## Sixteenth headless export: event objects and CFDC2400 core helper

### 0x11B18: kernel-event handle referencing

`FUN_00011b18` is the common event-reference helper used by `0xCFDC2180`, `0xCFDC218C`, and the three-handle `0x00223100` path. It calls `ObReferenceObjectByHandle` with desired access `2` (`EVENT_MODIFY_STATE`), `ExEventObjectType`, and the caller requestor mode. On success it retains the referenced event object in the helper state and increments its reference/registration count.

This closes the essential event-registration ABI: user-mode passes 32-bit event handles; the legacy driver converts them into referenced kernel event objects and associates them with the current process.

### 0x157B8: pending status predicate

`FUN_000157b8` returns the bitwise intersection of two 16-bit state fields at offsets `+9` and `+0x0B` of the associated status object. `0xCFDC2180` uses this result immediately after registering/clearing the event; if any enabled status bit is already pending, the event is signalled immediately through `0x10816`.

### 0xCFDC2400 -> 0x12EDE

`FUN_00012ede` requires a non-null four-byte input buffer. It copies the input DWORD to global `DAT_0001CE1C`, executes `LAB_00012EC2` under the driver's interrupt-synchronization helper returned by `0x10A88`, and then invokes main-object virtual method `+0x24` with `(0, 0)`.

The concrete semantic name of this operation is still unresolved because the main-object vtable target at `+0x24` has not yet been mapped. However, the handler is now known to be a synchronized global-control update rather than a passive query.

### Process-registration bookkeeping

`FUN_0001232a` walks the process-registration list, matches the current process, and ORs a supplied flag into that process entry. The event registration handlers pass flag `2`, linking event ownership/status tracking to the process-scoped registration subsystem.

### Main object construction

`FUN_00014212` confirms the main object uses vtable `PTR_FUN_0001C8BC` and initializes the event slots at `+0x10FE` and `+0x110E`, the MAM helper at `+0x1086`, delay/buzzer helper at `+0x10F2`, process/transfer subsystems, tracing, and register lists.

The next step is to decode the main-object vtable itself so `0xCFDC2124`, `0xCFDC2128`, and `0xCFDC2400` can be assigned exact semantic method targets instead of inferred registration/control roles.


## Eighteenth headless export: synchronous transfer start/stop hooks decoded

The raw windows around `0x13914` and `0x13934` resolve the two virtual methods used by `0x171DE` before and after the synchronous acquisition transfer.

### 0x13914: enable transfer interrupt/mask bit 0

`0x13914` sets bit 0 in global mask `DAT_1CE18`, then commits the new mask through the synchronized `0x12EAE -> 0x11E46` path.

### 0x13934: disable transfer interrupt/mask bit 0

`0x13934` clears bit 0 in the same global mask and commits it through the identical synchronized path.

This means the `0x171DE` sequence is now clearer:

```text
program transfer registers / reset completion event
-> enable mask bit 0 through 0x13914
-> start/program transfer register path
-> wait on transfer-entry event
-> disable mask bit 0 through 0x13934
```

The virtual hooks are therefore transfer-interrupt enable/disable hooks rather than opaque DMA start/stop methods.

### 0x104A0: no-op virtual method

The method-table entry at `+0x24`, used after the synchronized `0xCFDC2400` state update, resolves to `XOR EAX,EAX; RET`. In this legacy build the virtual method is therefore an intentional no-op that returns success/zero.

This closes `0xCFDC2400`: the externally meaningful action is the synchronized OR of the caller-supplied DWORD into `DAT_1CE10`; the subsequent virtual call has no hardware side effect in this derived device class.

### 0x12E18: process-owned transfer cleanup

`FUN_00012e18` walks the transfer-entry list rooted at object `+0x104`, removes every entry whose stored process pointer at `+0x34` matches the supplied process, and destroys each entry through `0x17FD6`.

This is the process teardown cleanup path for registered acquisition buffers.

### 0x14122 / 0x134F6: main-object destruction

`0x14122` is the deleting-destructor wrapper. `0x134F6` tears down the main device object, including event references, transfer/configuration helpers, register wrappers, tracing objects and the base transfer manager.

### 0x170EE

The base-object vtable entry at `+0x04` adjusts `this` by `+0x100` and jumps to `0x1829A`; this is an adapter/thunk into a helper operating on the transfer-list subobject. The target `0x1829A` remains to be decoded.


## Nineteenth headless export: transfer register programming mapped

The synchronous acquisition helper `0x171DE` can now be tied to concrete hardware registers by combining its writes with the register-wrapper wiring from `0x14847`.

The relevant embedded wrappers/fields are:

```text
transfer object +0x04 -> BAR0 IIMCL  (offset 0x048)
transfer object +0x2C -> BAR1 MAMRGO (offset 0x064)
transfer object +0x58 -> BAR0 SGTA   (offset 0x040)
transfer object +0x80 -> BAR0 IIMTC  (offset 0x044)
transfer object +0xD0 -> BAR1 MTTRGO (offset 0x084)
transfer object +0x54 -> constant/cached value 1 used for IIMCL
```

This makes the `0x171DE` sequence substantially more concrete:

```text
1. SGTA  <- param_1
2. cache transfer length/count in object +0xFC
3. IIMTC <- selected transfer entry +0x18
4. reset selected transfer-entry event
5. enable global transfer mask bit 0 (0x13914)
6. IIMCL <- 1
7. MAMRGO <- transfer length/count when param_3 != 0
   or
   MTTRGO <- transfer length/count when param_3 == 0
8. wait up to 5 s for selected transfer-entry event
9. disable global transfer mask bit 0 (0x13934)
```

`0x17478` supplies `SGTA` from the selected transfer entry's descriptor-table MDL/page-derived address (`entry +0x14 -> MDL`, then MDL PFN-like field at +0x1C, shifted by 12). The descriptor builder at `0x18194` stores the hardware word count/result at transfer-entry `+0x18`, which is then programmed into `IIMTC`.

This now strongly identifies the two `0x171DE` modes as two hardware launch paths sharing the same descriptor table and interrupt machinery: one launched through `MAMRGO`, the other through `MTTRGO`.

### 0x1829A: transfer-list teardown

The thunk at `0x170EE` enters the transfer-list subobject at `this+0x100` and jumps to `0x1829A`. That helper walks all remaining transfer entries, destroys each through `0x17FD6`, and clears the list head. It is therefore a transfer-manager cleanup/destructor helper, not execution logic.

### Register-wrapper initialization helper

`0x11962` is the generic register-wrapper constructor. It initializes the register pointer/cache metadata and default access/type fields; the actual hardware addresses are wired later by `0x119BC` and direct assignments in `0x14847`.

### Interrupt-mask commit path reconfirmed

`0x11E46` mirrors `DAT_1CE18` into the active mask globals and writes the value to BAR0 `INTEN` through `DAT_1CE20`. `0x12EAE` is the synchronized callback wrapper around that commit.


## Twentieth headless export: transfer completion path narrowed to DPC callbacks

The latest ISR/DPC export makes the acquisition completion path much more concrete.

### ISR 0x108D6

`FUN_000108d6` reads the interrupt-status register, filters it through the active interrupt masks, acknowledges the individual interrupt sources, records secondary status when interrupt bit 1 is present, and queues the DPC at object offset `+0x1515`.

The ISR records pending bits in `DAT_1CE10` and uses the synchronized mask state `DAT_1CE14` / `DAT_1CE44`, matching the transfer-mask logic already reconstructed around `0x171DE`.

### DPC 0x11390

`FUN_00011390` evaluates six synchronized callback predicates:

```text
0x1085E
0x10872
0x10886
0x1089A
0x108AE
0x108C2
```

The most important newly visible branch is the `0x10872` callback. When it reports true and the pointer at main-object offset `+0x2E0` is non-null, the DPC executes:

```text
KeSetEvent(*(main + 0x2E0) + 0x24, ...)
```

A transfer entry created by `0x1731C` contains its completion event at entry offset `+0x24`, exactly the event reset and waited on by `0x171DE`. This is therefore the strongest completion-path match yet: one DPC source directly signals an event at `selected_object + 0x24`.

The remaining missing proof at this stage was the assignment/lifetime of
main-object field `+0x2E0`. That is closed by the next export below.

### Other DPC sources

- `0x10886` and `0x1089A` accumulate status bits `0x80` and `0x800` respectively, signal the shared status event, and may clear a related register when no event listener consumes the status.
- `0x108AE` accumulates status bit `0x100` and signals the same status event.
- `0x108C2` services the BAR1 transport/event object around main-object `+0x10B9`; it clears a register when needed and signals its event when `0x176D0` observes bit `0x8000`.

### Status accumulator 0x157A6

`FUN_000157a6` ORs only enabled bits into the object's pending-status field:

```text
pending |= enabled & new_bits
```

This matches the previously decoded event-registration behavior where `0x157B8` tests `pending & enabled` for immediate notification.

The next pass should decode the DPC-context setup and the six callback
predicates, then tie `main + 0x2E0` back to the selected transfer entry.


## Twenty-first headless export: acquisition completion event proven

The DPC context and callback predicates now close the acquisition completion
chain.

### DPC context

`FUN_000115c4` initializes the DPC as:

```text
KeInitializeDpc(main + 0x1515, 0x1151E, main)
```

The raw thunk at `0x1151E` forwards the DeferredContext argument directly to
`FUN_00011390`, so the `param_1`/`ESI` value inside `0x11390` is the main
device object, not the DPC object.

The apparent `0x14847` decompiler line involving `this + 0x2E0` is therefore
not a competing write to this field. The new `field:2e0` instruction scan finds
only the DPC read at `0x113CB`:

```text
MOV EAX, dword ptr [ESI + 0x2E0]
```

### `main + 0x2E0` is the selected transfer entry

The acquisition subobject starts at `main + 0x1E0`. Both acquisition front-ends
operate on that subobject:

- DeviceControl dispatch calls handlers with `ECX = main + 0x1E0`.
- `0x13C84` (`0xCFDC2138`) sets `*(this + 0x100)` to the transfer entry found
  by `0x18168`.
- `0x13DC6` (`0xCFDD219F`) performs the same assignment after resolving the
  caller/process transfer identifier.
- `0x171DE` resets and waits on `(*(this + 0x100) + 0x24)`.

Since `(main + 0x1E0) + 0x100 == main + 0x2E0`, the DPC branch:

```text
KeSetEvent(*(main + 0x2E0) + 0x24, ...)
```

signals the same selected transfer-entry event that the synchronous transfer
helper reset and waited on. The acquisition-transfer completion path is
therefore confirmed:

```text
front-end selects transfer entry at acquisition-subobject +0x100
 -> 0x171DE programs SGTA/IIMTC/IIMCL and launches MAMRGO/MTTRGO
 -> 0x171DE waits on selected entry +0x24
 -> ISR 0x108D6 records interrupt bit 0 in DAT_1CE10 and queues DPC
 -> DPC 0x11390 callback 0x10872 / 0x11DC2 consumes bit 0
 -> KeSetEvent(selected entry +0x24)
 -> 0x171DE resumes and disables transfer mask bit 0
```

### DPC predicate bits

The six synchronized callbacks are tiny global-pending-bit consumers:

| DPC callback | Predicate helper | Consumed bit in `DAT_1CE10` | DPC action |
|---:|---:|---:|---|
| `0x1085E` | `0x11DD8` | `0x02` | signal device event at `main + 0x12EE` |
| `0x10872` | `0x11DC2` | `0x01` | signal selected transfer-entry event |
| `0x10886` | `0x11DEE` | `0x04` | accumulate status `0x80`, signal status event |
| `0x1089A` | `0x11E04` | `0x10` | accumulate status `0x800`, signal status event |
| `0x108AE` | `0x11E1A` | `0x20` | accumulate status `0x100`, signal status event |
| `0x108C2` | `0x11E30` | `0x08` | service BAR1 transport/status event path |

This proves that the transfer-completion interrupt source is legacy interrupt
bit 0 as recorded in `DAT_1CE10`, gated by the already-decoded transfer mask
enable/disable methods `0x13914` and `0x13934`.

## Twenty-second headless export: MAM/MTT and DMA descriptors

This pass resolves the host-side MAM encoding, the descriptor-table layout,
and the static selection of the two transfer launch registers. Hardware-side
meanings that are not represented in the driver remain explicitly unknown.

### Indexed MAMDAT/MAMPGO protocol

`0x179E2` treats a MAMDAT write as an indexed 16-bit value:

```text
bits 23:16  register/value index
bits 15:0   16-bit value
```

It caches the low 16-bit value independently for each 8-bit index and suppresses
unchanged writes. `0x17BC8`, used by the type-1/type-2 `0xCFDC2110` path, emits
the caller's WORD array as indices `0..count-1`, then writes MAMPGO as:

```text
((mode & 3) << 8) | count
```

Its only caller supplies mode 1. Consequently `0x105` in `0x17C16` is exactly
mode 1 plus five indexed words, not an opaque magic constant. The driver's
static code does not reveal the board-level name of mode 1.

For each acquisition channel, `0x17D20 -> 0x17C16` emits:

| Index | 16-bit MAMDAT value | Host-side meaning |
|---:|---:|---|
| 0 | `0x0E00 | channel_byte` | per-channel command/header |
| 1 | `config_dword & 0xFFFF` | configuration low half |
| 2 | `config_dword >> 16` | configuration high half |
| 3 | `channel_span & 0xFFFF` | per-channel span/count low half |
| 4 | `channel_span >> 16` | per-channel span/count high half |

`channel_span` is `min(0x400, total_transfer_bytes / channel_count)`. It is
derived directly from a byte length, but the driver does not expose whether
the FPGA names this field as bytes, samples, or another acquisition unit. The
temporary `0xA5FB` WORD adjacent to the index-0 value is stack-packing residue
from the source structure; instruction-level analysis confirms that `0x17C16`
consumes only the upper WORD (`0x0E00 | channel_byte`) of that first DWORD.

### MAMSEQ channel entries

`0x17DDC` creates the temporary 0x7C-byte channel configuration. `0x17EE0`
appends each channel byte and emits one 32-bit MAMSEQ value:

```text
bits 24:16  zero-based sequence index (9-bit source value)
bit  6      final channel entry
bits 5:0    channel identifier
```

`0x17CDC` writes those values to MAMSEQ in order. The channel count at temporary
object `+0x78` is the highest inserted sequence index plus one. In the buffered
acquisition path, `0x17D20` first sends the five-word per-channel MAM command
and then writes MAMSEQ. The METHOD_NEITHER acquisition path calls `0x17CDC`
directly, so it refreshes MAMSEQ without repeating the five-word channel setup.

### MAMRGO versus MTTRGO

`0x171DE` selects the launch wrapper solely from its third argument:

```text
nonzero -> MAMRGO
zero    -> MTTRGO
```

The two acquisition IOCTL front-ends converge on `0x12D6A`, which always calls
`0x17478(..., 1)` and therefore always launches through MAMRGO. MTTRGO is not
dormant: family-1 A5FB opcodes `0x50` and `0x51` call `0x160DC`, which resolves
a registered transfer entry and calls `0x17478(..., 0)`. Those commands launch
through MTTRGO while sharing the same SGTA/IIMTC descriptor setup, completion
event, interrupt bit, and timeout path.

The value written to MAMRGO is a launch count, not IIMTC's total transfer
length. For the buffered path `0x12D6A` derives it as
`total_bytes / min(0x400, total_bytes)`; the METHOD_NEITHER path supplies it
explicitly from its second trailing DWORD. `0x160DC` writes its request WORD at
offset `+5` to MTTRGO. The precise FPGA unit of either launch count is not
identifiable from host code alone.

The exact FPGA distinction between the MAM and MTT engines still requires
firmware documentation or passive runtime observation. Statically, their
callers and launch-register selection are now exact.

### Descriptor-table format produced by 0x18194

The descriptor table is a chain of 4 KiB pages. Every entry is eight bytes:

```c
typedef struct {
    uint32_t count_dwords;
    uint32_t physical_address;
} LECS65_DMA_DESCRIPTOR;
```

Data descriptors are generated directly from each source MDL PFN. They never
cross a 4 KiB source-page boundary. The first descriptor of an MDL includes
`MDL.ByteOffset`; later descriptors begin at page offset zero. The count is the
covered byte count shifted right by two, so its unit is definitively 32-bit
words. The returned sum is stored at transfer entry `+0x18` and programmed into
BAR0 IIMTC.

Each table page contains 512 descriptor slots. Slots `0..510` are data slots.
When another table page is needed, slot 511 is a chain descriptor:

```text
count_dwords     = 0
physical_address = physical address of next descriptor-table page
```

After the last data descriptor the builder writes a zero/zero terminator. The
fixed `0x33000` allocation is 51 table pages. Its constructor constant
`0x065CD000` equals `51 * 511 * 4096`, the byte coverage represented by 51
pages with 511 data descriptors per page. The public registration limit is
lower (`0x06000000` bytes), so the descriptor table has sufficient capacity.

`0x17478` obtains SGTA from the first PFN of the descriptor-table MDL and shifts
it by 12, i.e. SGTA receives the physical address of the first table page.
IIMTC receives the total DWORD count returned by `0x18194`. Registration
subtracts the trailing four-byte completion/result field before building the
source MDLs, so the descriptor stream covers only acquisition data.

## Twenty-third headless export: command semantics and WOW64 lifetime

### CFDC2110 local register operations

Register-object wiring from `0x14847` assigns the command-object fields used by
the A5FB helpers:

| Command-object field | Register/helper |
|---:|---|
| `+0x15` | JTAG helper (`JTAGNUM/JTAGDAT/JTAGDIN`) |
| `+0x19` | SPI helper (`SPICTL/SPIDAT/SPIDIN`) |
| `+0x15E` | `ITMODE` |
| `+0x162` | `LEDCTL` |
| `+0x166/+0x16A` | `RMIDIV/RMICUM` |
| `+0x16E/+0x172` | `ACQDIV/ACQCUM` |
| `+0x176` | BAR0 `FVER` |
| `+0x17A` | `PFREG` |
| `+0x17E` | `ACQFVER` |
| `+0x182` | acquisition/transfer object |

This resolves the previously anonymous local opcode helpers:

| Family/opcode | Confirmed host-side action |
|---|---|
| family 0 `0x42` | JTAG write transaction |
| family 0 `0x90` | SPI serial write transaction |
| family 0 `0x91` | write `RMIDIV` or `ACQDIV` by selector |
| family 0 `0x92` | direct 32-bit register write through selected BAR block |
| family 0 `0xA0` | write `PFREG` |
| family 1 `0x42` | JTAG read transaction and local response |
| family 1 `0x50/0x51` | registered-buffer DMA launch through `MTTRGO` |
| family 1 `0x92` | direct 32-bit register read through selected BAR block |
| family 1 `0xA0` | read `RMICUM` or `ACQCUM` by selector |
| family 1 `0xA1` | read `ACQFVER` |
| family 1 `0xA2` | read BAR0 `FVER` |
| family 2 `0x02` | write `MTTCTL` as 0 or 1 |
| family 2 `0x04` | pulse `MTTCTL` 1 then 0 for the requested count |
| family 2 `0x05` | write `ITMODE=7`, then `ITMODE=3` |
| family 2 `0x09` | write `ITMODE=3` |
| family 2 `0x0A` | write `ITMODE=2` |
| family 2 `0x10` | write a two-bit value assembled from two payload booleans to `LEDCTL` |
| family 2 `0x40` | consume/ack current status, reset interrupt state, return local success |

Family-2 opcode `0x00` first performs the software-timer wait used by `0x1634E`
and then deliberately falls through to the same repeated `MTTCTL` pulse helper
as opcode `0x04`. Opcode `0x01` manages the same kernel timer without a direct
BAR write. Opcodes `0x06/0x07/0x08` produce local protocol status `0x10`.

The SPI helper selects a line/mode through `SPICTL`, emits bit-reversed 32-bit
words through `SPIDAT`, and restores the selected line afterwards. The JTAG
helper programs a direction and bit count through `JTAGNUM`, writes packed data
through `JTAGDAT`, and optionally reads `JTAGDIN`.

### Static boundary of CFDC2110

Many startup/runtime opcodes, including observed `0x4A`, `0x81`, `0x84`,
`0x96`, `0x97`, and `0x99` forms, intentionally converge on the generic BAR1
message transmitter. The driver does not interpret their payloads; it forwards
the A5FB signature and payload verbatim to board firmware and later fetches the
reply with `FB 85 40 00`.

Consequently their board-level meanings cannot be recovered from this driver
binary alone. Further static expansion of those wrappers would only repeat the
already-decoded transport. Normal-sequence passive request/response tracing or
firmware analysis is required before these commands can be implemented.

### CFDC2124 persistent transfer registration

The main-object vtable resolves `+0x0C` to `0x1731C` and `+0x08` to `0x172A2`.
The exact 12-byte `0xCFDC2124` input is therefore:

```c
#pragma pack(push, 1)
typedef struct {
    uint32_t user_buffer32;
    uint32_t total_bytes;
    uint32_t reserved;       /* not read by this build */
} LECS65_REGISTER_TRANSFER32;
#pragma pack(pop)
```

The buffer and length must be nonzero and `total_bytes <= 0x06000004`.
Registration subtracts four bytes, probes and locks the remaining data range
with four-byte alignment, builds the DMA table, records `IoGetCurrentProcess()`,
and returns the transfer-entry kernel address as a four-byte token. The final
four bytes are reserved for the transfer result/status and are not described by
DMA descriptors.

`0xCFDC2128` accepts that four-byte token, finds it in the global transfer list,
unlinks it, unlocks/frees the MDLs and descriptor table, and frees the entry.
The legacy remove helper does not compare the entry's owner with the current
process; ownership is enforced only by token secrecy and process-close cleanup.
A replacement must not reproduce the kernel-pointer disclosure and should use
an opaque 32-bit token with an explicit owner check.

### Process ownership and close cleanup

The Create dispatch calls `0x10A8E` with `IoGetCurrentProcess()`. It creates or
references a 16-byte process node:

```c
typedef struct PROCESS_NODE32 {
    uint32_t process_object;
    uint32_t open_reference_count;
    uint32_t resource_flags;
    uint32_t next;
} PROCESS_NODE32;
```

Transfer registration ORs flag 1 into this node. Event registration ORs flag
2. Close dispatch `0x10F30 -> 0x13768` decrements the process-node reference.
On the last reference, flag 1 invokes `0x12E18` to free every transfer entry
whose stored owner matches the process; flag 2 invokes `0x12E72`, which releases
all matching referenced event objects through `0x11B48`. `0x122E2` then removes
and frees the process node.

### CFDD219F exact METHOD_NEITHER shape

`0x141F8` passes `Type3InputBuffer` directly to `0x13DC6`. The packed input is:

```c
#pragma pack(push, 1)
typedef struct {
    uint8_t ignored_or_logical;
    uint8_t channel_id;
} LECS65_CHANNEL_PAIR;

typedef struct {
    uint8_t channel_count;
    LECS65_CHANNEL_PAIR channels[channel_count];
    uint32_t transfer_factor;
    uint32_t mam_launch_count;
} LECS65_NEITHER_CONFIG32;
#pragma pack(pop)
```

The first byte of each pair is skipped by this binary; the second byte supplies
the MAMSEQ channel identifier. Total DMA data bytes are:

```text
channel_count * transfer_factor * mam_launch_count
```

The total must not exceed `0x00FFFFFF`, must equal
`OutputBufferLength - 4`, and when `mam_launch_count > 0x400` must be a multiple
of `0x400`. The handler transiently registers and locks `Irp->UserBuffer`, runs
the MAM acquisition with only MAMSEQ refreshed, writes the completed byte count
to the final DWORD of the output buffer, reports that count plus four as
`IoStatus.Information`, and immediately unregisters the transient transfer.

The legacy code directly dereferences `Type3InputBuffer` without probing or
capturing it. It probes/locks only the data range of `UserBuffer`, excluding the
trailing result DWORD that it later writes directly. Both behaviours must be
replaced with explicit WOW64-aware capture, overflow-safe size validation, and
full output probing in the x64 driver.


## Runtime XStream startup packet set

The first real startup trace from the reference scope confirms that XStream
uses only the already identified type-3 A5FB/85FB families during this early
phase.

Observed requests:

```text
A5FB payload 40 02 40 01 "RESET\0"
A5FB payload 40 01 99 00
A5FB payload 40 00 88 00 DF FF
A5FB payload 40 01 42 00 ...
85FB payload 40 00
```

Multiplicity in the 120-second capture:

- RESET: 1
- family-1 / opcode `0x99`: 2
- family-0 / opcode `0x88`: 6
- family-1 / opcode `0x42`: 2
- matching 85FB fetch records are appended to all non-RESET requests

The current x64 driver deliberately returns
`STATUS_INVALID_DEVICE_REQUEST` for all `0xCFDC2110` calls, so this trace
does not yet provide real board responses. It does, however, reduce the active
startup command surface to these four request forms.


## Observed family-0 opcode 0x85

The second whitelisted startup trace exposes a new request after the first
successful opcode-`0x88` exchange:

```text
A5FB payload: 40 00 85 00 A0 00
followed by:  85FB payload 40 00
```

This request appears five times in the captured startup.

The legacy family-0 dispatcher routes opcode `0x85` through
`FUN_00016962` and then through the generic board-message transmitter.
`FUN_00016962` stores the request WORD, updates internal/global state via
`FUN_00016074`, and calls `FUN_0001573E` and `FUN_00015772`.

The latter two helpers are now explicit Ghidra targets. Opcode `0x85` remains
blocked in the x64 runtime gate until those side effects are fully decoded.


## Opcode 0x85 side effects closed

The two remaining helpers called by legacy `FUN_00016962` are now decoded:

- `FUN_0001573E(this, enable)` toggles global mask bit `0x10` and commits
  the global mask to BAR0 `INTEN`.
- `FUN_00015772(this, enable)` toggles global mask bit `0x20` and commits
  the global mask to BAR0 `INTEN`.

Together with the already decoded `FUN_00016074`, family-0 opcode `0x85`
interprets its control WORD as follows:

- low-byte bit 7 controls INTEN bit `0x04`;
- high-byte bit 3 controls INTEN bit `0x10`;
- high-byte bit 0 controls INTEN bit `0x20`.

The captured startup request uses control WORD `0x00A0`, therefore it sets
INTEN bit `0x04` and clears INTEN bits `0x10` and `0x20`, then forwards
the original A5FB packet to the generic BAR1 board-message transport.

The x64 driver now implements these confirmed side effects and adds this exact
captured opcode-`0x85` packet to the byte-exact runtime whitelist.


## Additional family-1 startup forms

The third staged trace exposes three further family-1 request shapes:

- opcode `0x42`, mode 0, requested response bytes = 12, bit count = 83;
- opcode `0x90`, no extra command payload, followed by an 85FB fetch;
- opcode `0x42`, mode 1, requested response bytes = 8, bit count = 58.

These fit already recovered legacy routing:

- family-1 `0x90` uses the same generic BAR1 transmitter as `0x99`;
- family-1 `0x42` uses the local JTAG-read path.

No new handler semantics were needed. The x64 runtime gate was extended only
with the exact input buffers observed in the trace.


## Newly observed mode-1 83-bit JTAG form

After the admitted mode-1, 256-bit JTAG packet completed on the reference
scope, XStream issued a 54-byte family-1 opcode-`0x42` request with mode 1,
12 requested response bytes and a bit count of 83. The complete input buffer
differs from the admitted mode-0, 83-bit request only at byte offset 11, where
the mode changes from `0x00` to `0x01`.

The existing `LecJtagExecute` path treats this byte as the JTAG mode. Mode 1
adds bit `0x100` to each BAR1 `JTAGNUM` value; the bit-count loop, JTAG data
writes and response reads are otherwise identical. The already admitted
mode-1 58-bit and 256-bit requests exercise that same behavior.

The complete buffer was subsequently admitted byte-exactly and completed
successfully on the reference scope. No opcode-, family- or mode-wide rule was
introduced.


## Newly observed mode-2 256-bit JTAG form

The next startup capture exposes a 94-byte family-1 opcode-`0x42` request with
mode 2, 32 requested response bytes and a bit count of 256. It differs from the
admitted mode-1, 256-bit request only at byte offset 11 (`0x01` to `0x02`).

Existing static analysis already establishes an important safety boundary in
legacy `FUN_00015C7E`: only mode values below 2 enter the JTAG register loop.
For mode 2 the handler performs no `JTAGNUM`, `JTAGDAT` or `JTAGDIN` access,
returns protocol status 4, and installs a local pending response through
`FUN_00015A26` for the following 85FB fetch record.

The current x64 helper also rejects modes above 1 before register access, but
its fallback fetch response is not yet a proven byte-exact reconstruction of
the legacy pending response. The new complete input buffer therefore remains
outside the runtime gate. It must not be treated as a third executable JTAG
mode or admitted through a broader opcode/mode rule.


## Mode-2 JTAG pending response closed

A follow-up Ghidra export of `FUN_00010380` confirms that the legacy temporary
response allocator is a thin wrapper around
`ExAllocatePoolWithTag(NonPagedPool, size, 'Wdm ')`. It does not zero the
allocation.

For family-1 opcode `0x42`, mode 2, `FUN_00015C7E` therefore creates a
`requestedDataBytes + 8` byte pending response without touching JTAG
registers. The defined bytes are:

```text
DWORD 0x00000000
WORD  0x0002
WORD  0x0004
```

For the captured 256-bit request, `requestedDataBytes = 0x20`, so the pending
response allocation is 40 bytes. The remaining 32 bytes are uninitialized pool
contents in the legacy driver and are not protocol-defined data.

The x64 implementation now reproduces the defined 40-byte response shape but
zero-fills the undefined tail instead of leaking kernel-pool contents. The
captured 94-byte mode-2 request is admitted byte-exactly. Mode 2 remains a
local protocol-error path and never reaches `JTAGNUM`, `JTAGDAT`, or
`JTAGDIN`.

### Family 0 opcode 0x42 and 0x92

The latest clean startup trace reached two non-forwarding family-0 commands.

**Opcode `0x42`** is handled by legacy `FUN_00015ACC`. Its request body is
interpreted locally as a JTAG write sequence:

- byte 0: mode, accepted values 0 or 1;
- byte 1: bit count;
- bytes 2..4: legacy header/reserved fields;
- from byte 5: repeated pairs of 16-bit words, one 4-byte chunk per up-to-16
  JTAG bits.

For each chunk the legacy driver writes BAR1 JTAGNUM and JTAGDAT. It does not
use the generic board-message transport for this opcode and it returns a local
8-byte success/status response.

**Opcode `0x92`** is handled by legacy `FUN_0001600E`. Its request body is:

- byte 0: target selector 0, 1 or 2;
- bytes 1..2: 16-bit register offset;
- bytes 3..6: 32-bit value.

Selector 1 is confirmed to address BAR1: the captured startup writes target
offsets `0x00C0` and `0x00C4`, matching BAR1 GPIODIR and GPIODAT exactly.
The x64 driver therefore implements selector 1 as an aligned BAR1 register
write with the final resource-bound check performed by `LecGetBar1Register`.
Selectors 0 and 2 remain unimplemented until their legacy base-object mapping
is confirmed.

### Family 2 opcode 0x02

The clean 2026-09-27 02:56 startup trace reaches family 2 / opcode `0x02`
after the family-0 JTAG and MMIO-write paths were implemented.

Legacy `FUN_000163B2` handles this command locally. The first command-body
byte must be 0 or 1. The legacy driver optionally waits for an active internal
transfer object, then writes that value to the register at offset `0x80` of
the same MMIO base used by family-0 opcode-`0x92` selector 1. That base is
confirmed as BAR1, making the target BAR1 MTTCTL.

The x64 implementation maps an active transfer wait to
`CurrentTransfer->CompletionEvent`, then writes 0 or 1 to BAR1 offset
`0x80` and returns the legacy local status response.

### Family 0 opcode 0x90, 0xA0, and generalized 0x85

After the normal XStream startup began completing, the next application-level
failures exposed three family-0 paths.

**Opcode `0x90`** is handled by legacy `FUN_00015E80`, not by the generic
board-message forwarder. The captured request uses selector `0x0E` and a
144-bit sequence. The function manipulates a helper object at driver-object
offset `+0x19` through `FUN_000155D0`, `FUN_0001236E`, and
`FUN_0001588E`. Those helpers maintain a control shadow, write it to one MMIO
register, bit-reverse 32-bit data words, and write them to another register.
The exact BAR/base mapping must be confirmed before this path is enabled.

**Opcode `0xA0`** is handled by legacy `FUN_00015FD8`. It takes a 16-bit
value from the request and writes it through a single internal MMIO register
object at driver-object offset `+0x17A`, then creates a local status response.
The underlying BAR/register mapping is not yet confirmed.

**Opcode `0x85`** uses legacy `FUN_00016962` for host-side control-bit
updates, then forwards the request to board firmware. The host logic is already
implemented in the x64 driver. Runtime admission is now structural for this
decoded opcode instead of matching only one captured control word.

### Family 0 opcode 0xA0 resolved to PFREG

The command-dispatch object used by `FUN_00015FD8` is the subobject at
board-object offset `+0xEA8`. Legacy `FUN_00015FD8` dereferences its field
at `+0x17A`.

`FUN_00014847` initializes board-object offset `+0x1022` with the register
object for **PFREG**, BAR1 offset `0xF4`. Because
`0xEA8 + 0x17A = 0x1022`, the mapping is exact:

`family 0 / opcode 0xA0 -> BAR1 PFREG (0xF4)`.

The request supplies a 16-bit value which the legacy driver writes directly to
PFREG and then returns a local success/status response. The x64 driver now
implements this path structurally.

### Family 0 opcode 0x90 resolved to SPI helper

The constructor chain now proves the register mapping used by legacy
`FUN_00015E80`.

`FUN_00014212` constructs the CFDC2110 command-dispatch object at board-object
offset `+0xEA8` and calls `FUN_000158EE` with:

- param1 = board `+0x104A` (JTAG helper);
- param2 = board `+0x106A` (SPI helper).

`FUN_000158EE` stores param2 at dispatcher field `+0x19`. Therefore
`FUN_00015E80`, which dereferences dispatcher `+0x19`, operates on the SPI
helper at board `+0x106A`.

`FUN_00014847` initializes that helper through `FUN_0001340C` with:

- BAR1 `SPICTL` at `0xA0`;
- BAR1 `SPIDAT` at `0xA4`;
- BAR1 `SPIDIN` at `0xA8`;
- initial control shadow `0x001FF000`.

The opcode-`0x90` request body contains selector, bit count, a 16-bit control
field, and packed 16-bit data words. Legacy selects one of selector values
0,1,2,3,4,0x0C,0x0E, updates the SPI control shadow, commits it to SPICTL,
bit-reverses each 16-bit payload word into the 32-bit SPIDAT write format, and
finally deasserts the selector and clears the low five control bits.

The captured probe request uses selector `0x0E`, bit count `0x0090` (144),
and exactly nine 16-bit words, matching the legacy loop exactly.

The x64 driver now implements this opcode structurally, including lazy legacy
SPICTL initialization to `0x001FF000`, selector handling, control-shadow
updates, bit reversal, SPIDAT writes, and local protocol status response.

### Family 1 forward-only class and JTAG record-span behavior

Legacy `FUN_000165A6` identifies the following family-1 opcodes as pure
generic board-message forwarders:

`0x4A, 0x81, 0x82, 0x90, 0x91, 0x96, 0x97, 0x99`.

The x64 CFDC2110 semantic validator and execution path now treat this complete
statically confirmed set as forward-only commands rather than requiring
per-packet captures.

Family-1 opcode `0x42` remains a local JTAG command. For modes 0 and 1,
legacy `FUN_00015C7E` derives the number of four-byte input chunks from the
bit count and reads them from the contiguous input record list. It does not
stop at the current record's declared payload boundary. A captured 58-bit
request demonstrates this behavior: the fourth JTAG chunk crosses into the
following packed record.

The x64 implementation reproduces that ABI quirk safely by allowing the JTAG
parser to consume the remainder of the already validated complete IOCTL input
buffer, while still rejecting any required read that would exceed the total
buffer.

### Family 0 pure-forwarding class

Legacy `FUN_00016A66` identifies the following family-0 opcodes as direct
generic board-message forwarders with no additional host-side semantics:

`0x4A, 0x84, 0x86, 0x87, 0x96, 0x97, 0xA1, 0xA2`.

These commands call `FUN_00016168(..., 1)` directly. The x64 driver now
admits and executes this complete statically confirmed class through
`LecTransportSend`.

This is distinct from family-0 opcode `0x85`, which also forwards but first
updates host-side interrupt-mask state, and from local commands such as
`0x90`, `0x92`, and `0xA0`.

### CFDC2110 record types 1 and 2

CFDC2110 is not exclusively a type-3 command protocol.

Legacy `FUN_00015720` accepts packed record type 1 or 2. Such records are
routed by `FUN_00013AE2` to `FUN_00012F30` rather than the type-3 command
dispatcher.

The payload must contain an even number of bytes. Legacy converts each
16-bit payload value into an indexed four-byte temporary entry and passes the
array to `FUN_00017BC8`.

The resulting hardware sequence is:

1. clear bit 16 in BAR1 GPIODAT (`0xC4`) via `FUN_000120DC`;
2. write each 16-bit value to BAR1 MAMDAT (`0x40`);
3. trigger BAR1 MAMPGO (`0x44`) with
   `((record_type & 3) << 8) | value_count`.

The captured type-1 records contain 42 payload bytes, i.e. 21 values, and
therefore issue `MAMPGO = 0x115`.

The x64 implementation now reproduces this behavior for structurally valid
type-1 and type-2 records with at most 255 16-bit values.

### Family 2 opcode 0x05 and 0x10

Legacy family-2 dispatcher `FUN_000166A8` handles both locally:

- opcode `0x05`: write BAR1 ITMODE `7`, then `3`;
- opcode `0x10`: encode two boolean request bytes as
  `(byte0 ? 2 : 0) | (byte1 ? 1 : 0)` and write the result to BAR1 LEDCTL
  (`0xE0`).

Both are now implemented structurally in the x64 driver.

Family-2 opcode `0x01` is a separate internal timer/wait control and remains
gated until the timer object's virtual method is exported and mapped.

### Family 2 opcode 0x01 timer control

The exported `FUN_000157C4` constructor proves that the object used by
legacy `FUN_0001621A` wraps a Windows kernel timer initialized with
`KeInitializeTimerEx`.

The observed command body is:

- byte 0 = 1;
- bytes 1..4 = requested delay in milliseconds.

Legacy `FUN_0001621A` performs a zero-timeout poll of the timer. If the timer
is signaled it arms a new relative one-shot interval. If the timer is still
pending it computes the elapsed interval and only re-arms when the newly
requested duration exceeds the remaining time. A zero request is normalized to
1 ms.

The x64 driver reproduces this behavior using a driver-owned notification
`KTIMER`, `KeWaitForSingleObject` with a zero timeout, `KeQuerySystemTime`,
and `KeSetTimer`. The runtime trace observed 1 ms and 100 ms requests.

### MAMDAT indexed write format

The initial type-1/type-2 implementation was corrected after re-reading
`FUN_00012F30`, `FUN_00017BC8`, and `FUN_000179E2`.

Each temporary MAM entry is not merely the 16-bit payload value. Legacy stores
the entry index in the upper 16 bits and the payload value in the lower 16
bits, then writes that complete 32-bit value to MAMDAT:

`MAMDAT = (index << 16) | value`.

For a 21-word type-1 record, entries therefore use indices 0 through 20 before
`MAMPGO = 0x115` is issued. The x64 implementation now matches that format.

### Outstanding four-byte query IOCTLs after successful arm

The first runtime capture with successful acquisition-board arming still shows
three unsupported METHOD_BUFFERED IOCTLs, each with no input and four-byte
output:

- `0x00222400`;
- `0x00223004` (`QUERY_BUFFER_A`);
- `0x00223040` (`QUERY_BUFFER_B`).

The latter two occur immediately after legacy event registration. XStream does
not subsequently issue transfer-registration or acquisition-launch IOCTLs in
the same capture.

Their exact legacy handler mapping is therefore being recovered from the
earlier portion of the DeviceControl dispatch tree before any value is
invented.

### Legacy mapping for 0x00223004 and 0x00223040

The follow-up DeviceControl export resolves the dispatch entries:

- `0x00223004` -> legacy `FUN_00012A5E`;
- `0x00223040` -> legacy `FUN_00012C18`.

Both are size-query style IOCTLs when called with a 4-byte output buffer.

`FUN_00012A5E` dereferences a driver-owned pointer at object offset
`+0x11EA`. The first DWORD of that pointed buffer is treated as its byte
length. With a four-byte output buffer the legacy driver returns that length
DWORD directly.

`FUN_00012C18` operates on the object at `+0x11EE` and calls
`FUN_00012386` to obtain its current serialized byte size. With a four-byte
output buffer the legacy driver returns that size DWORD directly.

The actual values are not hard-coded in the dispatchers. Their producer/helper
functions still need to be exported before the x64 driver can return the exact
legacy values without guessing.

### Implemented legacy buffer-size queries

The final helper exports resolve the concrete startup values for the two
four-byte query IOCTLs.

`0x00223004` -> legacy `FUN_00012A5E`:
- the backing trace-control object at board-object `+0x11A2` allocates
  `0x110` bytes;
- the first DWORD of that allocation is initialized to `0x110`;
- the four-byte query returns exactly that DWORD.

Therefore the x64 compatibility result is:

`0x00223004 -> 0x00000110`.

`0x00223040` -> legacy `FUN_00012C18`:
- `FUN_00012386` returns
  `(maxInserted + 1) * 0x10A`;
- static registration references show 43 distinct register objects are
  inserted into the legacy register list during setup;
- therefore `maxInserted = 42` and the required serialized size is
  `43 * 0x10A = 0x2CAE`.

Therefore the x64 compatibility result is:

`0x00223040 -> 0x00002CAE`.

Both IOCTLs are now implemented as exact four-byte METHOD_BUFFERED size
queries.

### Second-stage payload retrieval for 0x00223004 / 0x00223040

The legacy size-query IOCTLs are two-stage APIs.

After the initial four-byte size result, XStream reissues the same IOCTL with
an output buffer exactly equal to the returned size.

For `0x00223004`:
- first call out=4 -> size `0x110`;
- second call out=`0x110` -> legacy copies the complete 272-byte
  trace-control descriptor block.

For `0x00223040`:
- first call out=4 -> size `0x2CAE`;
- second call out=`0x2CAE` -> legacy refreshes and returns the complete
  serialized register list.

Each register-list entry is `0x10A` bytes:
- 256-byte zero-terminated register name field;
- one BAR selector byte;
- unaligned 32-bit register offset;
- one register-type byte;
- unaligned 32-bit current/shadow data field.

There are 43 entries, giving `43 * 0x10A = 0x2CAE`.

The x64 driver currently implements only the first-stage four-byte size query.
The full payload retrieval remains to be implemented once the last serializer
details are statically closed.

### Full second-stage payloads implemented

The full payload stage for the two legacy query IOCTLs is now implemented.

For `0x00223004`, the x64 driver returns the reconstructed 0x110-byte
`CKeTraceControl` block:

- DWORD 0x000 = 0x110;
- DWORD 0x004 = 1;
- ANSI name field at 0x008 = `CKeTraceControl`;
- DWORD 0x108 = 2;
- DWORD 0x10C = 0.

For `0x00223040`, the x64 driver now serializes the 43 legacy register-list
entries in the recovered insertion order. Each entry is exactly 0x10A bytes:

- name[256];
- BAR byte at 0x100;
- unaligned DWORD register offset at 0x101;
- type byte at 0x105;
- unaligned DWORD data at 0x106.

The recovered entry order runs from FVER/ERRS/ERRM through the MAM/SPI/JTAG,
MTT, GPIO and acquisition registers, followed by the transport registers
TxControl, RxControl, TxCount, RxCount, SetIRQ and HWInt.

Legacy type-2 entries return their wrapper shadow DWORD rather than reading
MMIO; their initial shadow state is zero. Types 0, 1 and 4 are refreshed from
the mapped register before serialization. INTEN is returned from the x64
driver's maintained interrupt-enable shadow.

This closes both the size-query and full-payload halves of the two-stage ABI.

### 0x00223000 trace-control setter and indexed 0x00223040 query

Legacy DeviceControl dispatch maps `0x00223000` to `FUN_00012ADA`.

Its ABI is:

- METHOD_BUFFERED;
- input length exactly `0x108`;
- no output;
- final two DWORDs are passed as `index` and `level` to
  `FUN_00012290`.

The operation only changes the software trace-control object and has no board
MMIO side effect. The x64 driver validates the structure and completes it
successfully while recording the requested index/level in the debug trace.

Legacy `0x00223040` has a third form in addition to size and full-list reads:

- input length 4;
- output length `0x10A`;
- input DWORD = register-list index;
- output = one refreshed serialized register entry.

Legacy `FUN_00012C18` calls `FUN_000124C2` for this form. The x64 driver now
supports indexed entries 0 through 42 through the same serializer used for the
full 43-entry list.

### Family-2 opcode 0x02 timer barrier corrected

Raw legacy analysis of `FUN_000163B2` shows that family-2 opcode `0x02`
does not write `MTTCTL` immediately.

If the legacy command object's timer exists, the handler first performs an
indefinite `KeWaitForSingleObject` on that timer. The timer is armed by
family-2 opcode `0x01`. Only after the timer becomes signaled does the driver
write `MTTCTL = 0` or `1`.

The previous x64 implementation incorrectly waited on the current DMA
transfer's completion event instead. Before acquisition transfer registration
this usually meant no wait at all, removing the hardware settling delay.

The x64 implementation now waits on `LegacyTimer` with alertable kernel wait
semantics before writing `MTTCTL`.

A second static correction was made to type-1/type-2 packed MAM records:
legacy `FUN_00012F30` always calls `FUN_00017BC8(..., mode=1)` regardless
of whether the packed record type is 1 or 2. The x64 path now always programs
MAMPGO mode 1 for both record types.

### MAMDAT shadow semantics

Legacy `FUN_000179E2` treats MAMDAT as a stateful indexed register window.

For each MAM index, the driver stores the last 16-bit value. A new MAMDAT write
is issued only when that value changes. Repeating the same value for the same
index updates no hardware state. The subsequent MAMPGO write still occurs for
every packed MAM record.

The x64 driver previously emitted all MAMDAT writes unconditionally. This was
observable in trace `xstream_trace_20260927_121112.jsonl`, where the six
type-1 records at seq 893-898 are repeated byte-for-byte at seq 899-904.

The x64 driver now maintains a 256-entry 16-bit MAM shadow plus valid flags and
suppresses duplicate indexed writes, while preserving the MAMPGO launch for
every record.

### Exact MAMDAT shadow initialization

Legacy board construction uses `FUN_00011A00` for the MAMDAT register object.
That constructor initializes all 256 indexed shadow DWORDs to
`0xFFFFFFFF`. `FUN_000179E2` compares only the low 16-bit data field, so
the effective initial shadow value for every MAM index is `0xFFFF`.

This means the first attempt to program value `0xFFFF` for an index is
already suppressed by the legacy driver. The previous x64 implementation used
an explicit invalid state and therefore always emitted the first write.

The x64 driver now lazily initializes all 256 MAM shadows to `0xFFFF` and
compares directly against that state, matching legacy constructor semantics.

### JTAGNUM write sequencing

Raw assembly of legacy `FUN_00015C7E` shows an important sequencing detail.

For a JTAG transfer longer than 16 bits, the legacy driver does not program
JTAGNUM for every data word. Instead it:

1. writes JTAGNUM once with 16-bit mode (low nibble zero) and selected mode bit;
2. streams every complete 16-bit pair through JTAGDAT;
3. reads JTAGDIN after each data write when response data is required;
4. if a partial chunk remains, writes JTAGNUM once more with the remainder;
5. transfers the final partial chunk.

Legacy `FUN_00015ACC` uses the same pattern for write-only JTAG transfers.

The previous x64 code rewrote JTAGNUM before every complete 16-bit chunk,
which can restart the underlying JTAG operation. The x64 implementation now
matches the continuous legacy sequence exactly.

## Passive original-driver trace corrections (2026-09-27)

The injected 32-bit XStream tracer captured the original driver at the Native
API boundary. This runtime evidence corrects several earlier static-only
assumptions.

### 0x00222400

XStream calls `0x00222400` immediately after Dallas ID access with
`InputLength=0` and `OutputLength=4`.

Original driver result:
- NTSTATUS: `STATUS_SUCCESS (0x00000000)`
- Information: `0`
- no returned output bytes.

The x64 replacement previously returned `STATUS_INVALID_DEVICE_REQUEST`.
This is now implemented as a success/no-data compatibility control.

### 0x00223004 trace-control descriptor

The original 0x110-byte descriptor contains the name:

`CKeTraceControl: `

including the colon and trailing space. The x64 replacement previously emitted
`CKeTraceControl`.

### 0x00223040 register-list order

The full 43-entry original-driver list captured at runtime is ordered:

1. TxControl
2. RxControl
3. TxCount
4. RxCount
5. SetIRQ
6. HWInt
7. FVER
8. ERRS
9. ERRM
10. INTST
11. IIMCL
12. CLRIRQ
13. CLRERR
14. INTEN
15. SGTA
16. IIMTC
17. IIMST
18. BUZZER
19. ONEWIRE
20. START
21. ITMODE
22. MAMDAT
23. MAMPGO
24. MAMSEQ
25. MAMRGO
26. SPICTL
27. SPIDAT
28. SPIDIN
29. JTAGNUM
30. JTAGDAT
31. JTAGDIN
32. MTTCTL
33. MTTRGO
34. MTTNUM
35. LEDCTL
36. ACQFVER
37. RMIDIV
38. RMICUM
39. ACQDIV
40. ACQCUM
41. PFREG
42. GPIODIR
43. GPIODAT

The original indexed getter is observed with index zero and returns
`TxControl`. Also, the live list reports `INTEN` as wrapper/access type 2,
not type 0.

Commit `9ec5723466f06d3d60d8846b89b919715e79c010` updates these early
startup ABI details in the x64 driver.

## Recovered interrupt-to-user-event mapping

The event-related control path is now tied directly to the original ISR/DPC
implementation.

DeviceControl dispatch proves:

- `0xCFDC2180 -> FUN_000128F8`, which registers the event stored at
  `main+0x12DE`;
- `0xCFDC218C -> FUN_00012B34`, which registers the event stored at
  `main+0x12EE`;
- `0xCFDC2184` remains an inline success/no-data control.

The original ISR `FUN_000108D6` accumulates enabled INTST sources and queues
the deferred handler `FUN_00011390`. Its recovered event mapping is:

| INTST source | Deferred action |
|---:|---|
| `0x01` | signal the selected acquisition transfer's completion event |
| `0x02` | signal the event registered by `CFDC218C` |
| `0x04`, `0x10`, `0x20` | update internal status and signal the event registered by `CFDC2180` |
| `0x08` | service/signal the driver's internal RX transport event |

The x64 replacement implements the externally visible `0x01`, `0x02`,
and `0x04/0x10/0x20` event deliveries. It does not invent an `0x08`
user-event mapping; its current RX transport polls RX_CONTROL directly.


## 85FB subcommand 1: local command-status query

A standalone type-3 85FB record with payload `40 01` is now fully decoded.
The captured request is:

```text
0A0002000300FB854001
```

`FUN_000169B4` returns a ten-byte local structure:

```text
DWORD 0
WORD  4
WORD  enabled_mask
WORD  pending_mask
```

The masks belong to the command/status object used by family-0 control:

- family-0 opcode `0x85` stores the complete 16-bit enabled mask;
- the deferred interrupt path latches sticky pending bits, gated by that mask:
  INTST `0x04 -> 0x0080`, `0x10 -> 0x0800`, `0x20 -> 0x0100`;
- family-0 opcode `0x88` clears pending bits selected by its mask.

Both full legacy runtime captures repeatedly return
`000000000400BF028000` at the acquisition transition, corresponding to
enabled mask `0x02BF` and pending mask `0x0080`.


## CFDC2138 observed one-channel ABI and execution

The first x64 runtime request is:

```text
0100000001010100000000000C0000
```

The observed 15-byte structure is:

```text
DWORD transfer_token
BYTE  channel_count
BYTE  pair_marker
BYTE  channel_id
DWORD config
DWORD requested_data_bytes
```

Every CFDC2138 call in the complete working legacy runtime capture uses
`channel_count=1` and `pair_marker=1`. The legacy parser consumes the
second byte of the pair as a six-bit channel ID.

For the staged one-channel path:

```text
MAMDAT[0] = 0x0E00 | channel
MAMDAT[1] = config low 16
MAMDAT[2] = config high 16
MAMDAT[3] = min(requested_bytes,0x400) low 16
MAMDAT[4] = same block size high 16
MAMPGO    = 0x105
MAMSEQ[0] = 0x40 | channel
SGTA      = first descriptor-table physical address
IIMTC     = requested_bytes / 4
IIMCL     = 1
MAMRGO    = requested_bytes / min(requested_bytes,0x400)
```

INTEN bit 0 gates the selected transfer's completion interrupt. The handler
waits at most five seconds, then disables bit 0, performs IIMST/IIMCL cleanup,
reads ERRS and returns the requested byte count in the four-byte output.

The x64 implementation requires a process-owned token that was successfully
created by CFDC2124. That registration rejects all source and descriptor-table
physical addresses above 4 GiB before the board can see them.

Only the observed one-channel form is enabled. CFDD219F and unobserved
multi-channel CFDC2138 remain gated.


## Family-1 opcodes 0x50 / 0x51: registered-buffer MTT transfer

`FUN_000165A6` dispatches both family-1 opcodes `0x50` and `0x51` to
`FUN_000160DC`. These commands are **not** generic firmware forwarding.

The payload following the opcode is seven bytes:

```text
BYTE  control/unused
DWORD registered-transfer token
WORD  MTTRGO launch value
```

`FUN_000160DC` resolves the transfer entry through `FUN_00018168` and
requires:

```text
launch_value << 3 >= transfer_data_bytes
```

On a valid request it calls `FUN_00017478(..., launch_value, false)`.
The false selector makes `FUN_000171DE` use BAR1 `MTTRGO` at offset 0x084
instead of MAMRGO:

```text
SGTA  <- descriptor-table physical address
IIMTC <- transfer TotalDwords
reset transfer CompletionEvent
enable INTEN bit 0
IIMCL <- 1
MTTRGO <- launch_value
wait <= 5 s for interrupt-bit-0 completion
disable INTEN bit 0
```

After an attempted transfer the local pending response is eight bytes:

```text
DWORD NTSTATUS
WORD  2
WORD  protocol_status   // 0 success, 8 hardware/timeout failure
```

Runtime evidence:

- both complete original traces use opcode 0x51 only;
- 279 and 286 captured calls respectively;
- payload length is always 10 bytes including `40 01 51`;
- control byte is always zero;
- launch value is normally 0x0080, with one 0x0180 call per trace;
- all referenced CFDC2124 data buffers are 0x400 bytes;
- all captured calls return success and the combined 14-byte response
  `0000000000000000000002000000`.

Trace `xstream_trace_20260927_235712.jsonl` is the first x64 runtime request
for this path: opaque transfer token 2, launch value 0x0080.


## Transfer-completion ISR performs immediate IIMCL clear

The completion path has one additional hardware-side action that was previously
missing from the x64 ISR.

`FUN_000115C4` constructs the acquisition subobject at `main+0x1E0`.
Within `FUN_00014847`:

```text
subobject + 0x1D8 -> BAR0 IIMCL (0x048)
```

Therefore:

```text
(main + 0x1E0) + 0x1D8 = main + 0x3B8
```

Original ISR `FUN_000108D6` handles INTST bit 0 with:

```text
cached IIMCL state <- 0
WRITE_REGISTER_ULONG(*(main + 0x3B8), 0)
```

before the final INTST acknowledge and DPC queue. The DPC later consumes pending
bit 0 and signals the currently selected transfer entry's completion event.

This immediate IIMCL clear is distinct from the later synchronous
`FUN_00012D6A` IIMST/IIMCL cleanup. The x64 ISR now mirrors the immediate
write as well. This change is motivated by trace
`xstream_trace_20260928_000706.jsonl`, where six otherwise valid CFDC2138
transfers wait the full five seconds for completion, while their identical
retries succeed immediately. Both complete original traces have zero captured
CFDC2138 failures.


## Family-0 opcode 0x88 local-vs-firmware split

`FUN_00016A66` gives opcode `0x88` two distinct behaviors.

For every request it first clears the selected bits in the local sticky command
pending mask:

```text
pending_mask &= ~request_mask
```

Then:

```text
if request_mask == 0x0080 or request_mask == 0x0800:
    FUN_00015A88(this, 0)
    // local response only; no firmware send
else:
    FUN_00016168(...)
    // normal board-firmware forwarding
```

`FUN_00015A88(this,0)` installs an eight-byte response:

```text
DWORD 0
WORD  2
WORD  0
```

This distinction is runtime-critical. Before the x64 fix,
`xstream_trace_20260928_002537.jsonl` forwarded 5660 mask-0x0080 commands to
firmware; 352 returned payload 0x002C. The original traces never expose this:
all captured opcode-0x88 responses are zero.

Startup masks such as 0xFFDF and 0x001F remain on the firmware-forwarded path.


## Interrupt source acknowledge and CFDC2180 event lifecycle

The CFDC2180 command-status event is not cleared solely by family-0 opcode
0x88. The original hardware ISR first acknowledges the physical source.

Recovered `FUN_000108D6` sequence:

```text
INTST bit 0x04 -> BAR1 CLRIRQ <- 1
INTST bit 0x08 -> BAR1 CLRIRQ <- 2
INTST bit 0x10 -> BAR1 CLRIRQ <- 4
INTST bit 0x20 -> BAR1 CLRIRQ <- 8
then common INTST write-back acknowledge
then queue DPC
```

The DPC maps 0x04/0x10/0x20 to sticky command bits 0x0080/0x0800/0x0100 and
signals the event registered by CFDC2180. User mode later consumes the sticky
bit through 85FB/0x01 and clears it with family-0 opcode 0x88.

Both layers are required: opcode 0x88 clears the host-side sticky state, while
CLRIRQ clears the underlying board interrupt source.


## Family-1 opcodes 0xA1 / 0xA2: FPGA revision reads

Service -> AladdinAcqBoard -> Revision exercises two local family-1 commands
that are not generic board-message forwarders.

Legacy `FUN_000165A6` dispatch:

```text
0xA1 -> FUN_00015BCE
0xA2 -> FUN_00015C26
```

Object mapping through `FUN_00014847`:

```text
0xA1:
  dispatcher +0x17E
  = board +0x1026
  -> BAR1 ACQFVER, offset 0x00C

0xA2:
  dispatcher +0x176
  = board +0x101E
  -> BAR0 FVER, offset 0x000
```

Both read one DWORD and create the same 12-byte local response:

```text
DWORD 0
WORD  6
WORD  status     // 0 success, 8 missing/unavailable register wrapper
DWORD value
```

Trace `xstream_trace_20260928_014500.jsonl` is the first x64 runtime evidence
for these commands. Before implementation, every observed A1/A2 request was
rejected with STATUS_INVALID_DEVICE_REQUEST and XStream displayed
`HardwarePCI Communication error!`.


## 2026-09-28 implementation-status audit

The 27-entry table at the top of this document is fully *identified*
statically, but that must not be confused with 27 complete x64
implementations. Comparison with the current `driver/Ioctl.c` top-level
DeviceControl switch:

| x64 coverage | Number | Notes |
|---|---:|---|
| Original dispatch values | 27 | 26 buffered, one METHOD_NEITHER |
| Present in x64 switch | 21 | Includes explicitly gated CFDD219F |
| Legacy behavior represented | 20 | Includes CFDC212C, which intentionally returns STATUS_NOT_IMPLEMENTED in both drivers |
| Deliberately gated | 1 | CFDD219F METHOD_NEITHER |
| Original dispatch values with no x64 case | 6 | Enumerated below |

Not yet present in the replacement's top-level switch:

| Original IOCTL | Recovered role |
|---|---|
| `0x0022303C` | trace/control structure, 0x10A-byte input |
| `0x00223044` | four-byte register/status helper |
| `0x00223088` | Dallas/1-Wire memory write |
| `0xCFDC2130` | serial-trigger FPGA programming |
| `0xCFDC2194` | paired CFDC2190 error/interrupt readback |
| `0xCFDC2400` | internal control helper |

The original `CFDC212C` is itself unimplemented by LeCroy and the
replacement deliberately matches that response. `0x00222400`, observed
during real XStream startup and explicitly implemented by the replacement,
is outside the 27-value static dispatch inventory of the captured 2008
binary.

This counts **top-level IOCTLs**, not the many separate subcommands carried
inside CFDC2110, and not all internal C++/DriverWorks helper routines. In
particular the supported CFDC2138 path is limited to the observed one-channel
shape. The primary observed outstanding feature is asynchronous ProBus
hotplug, despite successful AP015 identification when preconnected.


## Recovered asynchronous HWInt status path

Static verification of `FUN_00011390` raw assembly at
`0x114A2..0x114C8` corrects an earlier misleading decompilation:
the argument to `FUN_000157A6` is not zero. The original
`FUN_000176A2` reads BAR1 HWInt (offset `0x410`) as a 16-bit
out-parameter, clears the hardware register when nonzero, then
`FUN_000157A6` ORs `hwIntWord & enabledMask` into the command-status
pending word and signals CFDC2180.

The corresponding receive enable is in `FUN_0001619A ->
FUN_000160A8(1)`: global INTEN bit `0x08` is enabled before normal 85FB
firmware reply fetch. Both steps are required for spontaneous command-status
flags such as the AP015 hotplug bit `0x0200`; the replacement previously
omitted them while handling normal replies by bounded synchronous polling.


## Raw 85FB response length/padding: FUN_000167F4 (2026-09-28)

The 2026-09-28 x64 AP015 Degauss/jaw trace
`xstream_trace_20260928_231656.jsonl` exposes a repeatable discrepancy
independent of the now-working HWInt/0x0200 path. Identical family-0/0x4A
`...47 00` and `...47 12` requests return
`0000000000000000000002000000FFFF` from legacy x86, but
`00000000000000000000040000000000` from pre-fix x64. For identical
family-1/0x82 requests, the legacy 412-byte aggregate result reports
actual raw-response WORD `0x0006` or `0x0016` and fills the rest
with 0xFF, while pre-fix x64 writes the record payload capacity
`0x0190` and zero pads.

The exact original implementation is in
`ghidra_exports/selected/000167f4_FUN_000167f4.c`:

- For a pending firmware reply (object byte +0x24), allocate the requested
  result-record length and fill the **whole record with 0xFF**.
- Call `FUN_0001619A(this, {FB,85,40,00}, 4, result+6,
  requested_record_length-6, &actual_received)`.
- On successful transport completion, place DWORD 0 at result+0 and
  **actual_received** (not maximum capacity) as WORD at result+4.
  On transport failure, set length=2 and the local protocol error word.
- `FUN_000169B4` subsequently copies the entire caller-requested record
  length to the combined IOCTL result. Thus top-level
  `IoStatus.Information` is still the aggregate output capacity and
  cannot substitute for the inner raw payload count.

The previous replacement in `driver/Ioctl.c` zeroed the entire
SystemBuffer, then advertised `recordOutput - 6` in the raw branch
and copied only `LecTransportReceive`'s actual received bytes.
It consequently exposed the wrong raw reply length and zero padding
for short responses. `LecTransportReceive` already returns the true
received byte count in its `Received` out-parameter; this does not
require touching RX_CONTROL, BAR1, ISR, DMA or PCI.

Host-only patch `34907090820a582ac360d02120316c8792d7c888` pre-fills
**only raw-hardware 85FB output records** with 0xFF, uses the smaller of
actual received bytes and that record's payload capacity for the header
and copied bytes, and preserves the existing proven explicit family-1
opcode-0x99 length override. The already-correct local responses and
known-good full-size raw responses remain unchanged.

**Hardware update from `xstream_trace_20260928_233125.jsonl`:**
the patched driver now returns the byte-identical legacy full result
`0000000000000000000002000000FFFF` for the two recorded, identical
`47 00` requests at seq 8573 and 22156 (~20.034 s and ~40.675 s).
The old five-call 47 00 retry burst no longer occurs. These two distinct
commands correlate with user Degauss and subsequently **manual Auto Zero**.
The manual test records no `47 12` command, so its previous automatic
event-associated use must not be declared the universal Auto Zero opcode.

Two subsequent identical family-1/0x4A `...01 0A` status replies at
seq 8574 and 22158 both contain the genuine value `F300`, versus
reference legacy `F200`. This status-bit discrepancy persists independently
of the corrected host raw-response framing. Physical calibration outcome
is not established by these IOCTLs alone.

**Do not prematurely mark 47 12 and 0x82 reply formats hardware-tested:**
there were zero such requests in the `233125` capture, and zero in
the follow-up **`235314` controlled pre/post-calibration jaw capture**.
The expected `0x82` corrected actual-length/FF-tail behavior has not
yet been exercised by real hardware because XStream did not issue
any 0x82 without an authentic pending-0x0200 notification.

`235314` does independently confirm the 85FB host formatting
correction in another packet type: the first captured 128 bytes of
the family-1/0x99 startup reply now **match original x86 exactly**,
including `0000000000000000000002000200FFFFFFFF...`.
Old x64 `231656` used zero padding after that valid short reply.
It also confirms two separate identical `47 00` requests at
seq 23386 (~40.156 s) and seq 31835 (~54.337 s) again produce the
full x86-matching `0000000000000000000002000000FFFF`; related
family-1/0x4A status remains F3 vs reference x86 F2, with
physical calibration status unresolved.

**The requested jaw-state-before/after-calibration A/B is completed:**
all 1,517 standalone status 85FB/0x01 replies from `235314`
reported software-enabled mask 0x02BF and pending **only 0x0080**,
including 622 before Degauss, 381 between Degauss and the manually
triggered Auto Zero, and 514 afterward. There are no family-1/0x82,
0x88-mask-0x0200, pending-0x0200 or pending-0x0280 events in either
phase, whereas the earlier pre-formatter `231656` trace had five.
The last snapshot gap ends at t~48.480 s and the remaining 648 status
reads are gap-free. This excludes Degauss/Auto Zero as a necessary
precursor to the missing jaw event but does not assign cause to the
host serializer.

The 0x02BF software `LegacyCommandEnableMask` is distinct from
hardware **BAR0 INTEN** at offset 0x084 and its command-interrupt
enable bit 0x08. No register read of BAR0 INTEN, INTST or BAR1 HWInt
is present in this JSONL. The existing `LECS65_IOCTL_REGISTER_READ`
(`0xCFDC21C0`) implementation directly reads BAR MMIO for a
correctly addressed register, but the incidental 11 calls captured
in `235314` request BAR0+0x000 or BAR1+0x00C, not INTEN 0x084.
Do not infer INTEN's value from the host software mask.

**This requested physical-hotplug discriminator is now completed:**
`xstream_trace_20260929_001152.jsonl` on the **unchanged
post-`3490709` driver** captures **twelve genuine pending-0x0200-bearing
notifications**, all acknowledged and followed by real family-1/0x82.
Software-enabled mask 0x02BF; pending distribution across 1,361
standalone 85FB/0x01 reads: 0x0080=1,348, 0x0200=11,
0x0280=1, 0x0000=1. Four removal states correlate with final
0x82 state WORD 0x03FF; four reinsertion groups are seen.
The first reconnection transiently reported 0x82 WORD
0x028C then 0x0058, and did NOT request 270-byte AP015 metadata;
the user reported a possibly improperly seated, temporarily
misidentified "1/2 clamp" type. The next three reinsertions
did correctly request AP015 metadata. These state values are
not yet statically decoded as literal probe types.

**Post-fix 0x82 raw response is now genuinely exercised on hardware:**
first disconnect returns raw result length 0x000E (14 bytes);
remaining eleven responses return length 0x0006 (6 bytes), and
every unused captured byte is 0xFF. This replaces the previous
incorrect capacity header 0x0190 / zero tail.
Three identical family-0/0x4A `47 12` requests at seq
26385, 29250, 32096 now each return the full x86-matching
`0000000000000000000002000000FFFF`, once per accepted AP015
reidentification, rather than the old fivefold burst. Follow-up
family-1/0x4A status remains F7 in each observed event.
A separate manually invoked Auto Zero had sent 47 00 in
prior `233125`/`235314`; do not assign all Auto Zero
functions to 47 12 or fabricate F2 status.

The **general** original interrupt path is therefore still
operational after the serializer fix. Jaw-only movement in earlier
post-fix runs `233125` and `235314` produced no captured
0x0200. **Correction from the user and new trace `002051`:**
that absence must not be equated with failure of the XStream
jaw-unlock warning. XStream explicitly warns the user on an opened
jaw, and the same corrected driver records real jaw-open/closed
0x0200/0x88/0x82 events in the new capture (see below).
No synthetic pending bits, extra generic IRQ rewrites, DMA changes
or formatter rollback are justified. Full original and post-fix
trace comparison: `docs/probus-calibration-ab-comparison.md`.


## 2026-09-29 00:20: actual XStream jaw recognition and raw 0x82/0x4A status pairing

User explicitly reports that opening the AP015 generates XStream's
"not locked" warning and cautions measurement accuracy. The earlier
assistant conflated **no captured standalone 0x0200** in two traces
(`233125`, `235314`) with **no UI recognition**; that inference
is retracted. No new firmware polling path is asserted without
evidence, because new unchanged post-`3490709` capture
`xstream_trace_20260929_002051.jsonl` already records authentic
notification and normal status-query handling:

| Relative time | Standalone pending / seq | Family-0/0x88 ack | Family-1/0x82 actual-length-6 raw data | Subsequent 0x4A status |
|---:|---|---|---|---|
| 24.623 s | `0x0200` / 14888 | 14889 / `0x0200` | 14891: `000012005800` = state WORD **0x0058** | 47 12 seq 14913 reply `...02000000FFFF`, status seq 14916 **F7** (open/unlocked-correlated) |
| 35.038 s | `0x0280` / 21435 | 21436 / `0x0280` | 21448: `00001200FE03` = **0x03FE** | none; removal-like transition |
| 37.458 s | `0x0200` / 22833 | 22834 / `0x0200` | 22843: `000012005700` = **0x0057** | AP015 270-byte metadata setup/fetch seq 22877/22878 |
| 49.861 s | `0x0200` / 30553 | 30554 / `0x0200` | 30563: `00001200A700` = **0x00A7** | 47 12 seq 30580 reply `...02000000FFFF`, status seq 30581 **F3** (closed-correlated) |

All four authentic 0x0200-bearing events are **after** the final
snapshot gap (t~23.382 s). In a 58.3684694-second run there are
31,200 captured IOCTLs (seq 1..35955; 32 gaps omitting 4,755
entries), no observed NTSTATUS failures, 1,292 standalone 85FB/0x01
reads with enabled software word 0x02BF (pending: 0x0080=1,287,
0x0200=3, 0x0280=1, 0=1), and 5,839 CFDC2138 calls all returning
the correct requested byte count at input DWORD offset 11.
The two 270-byte AP015 metadata responses at startup seq 506 and
after reinsertion seq 22878 have identical captured first 128 bytes.

The `0x0058 -> F7` (opened/unlocked) and `0x00A7 -> F3`
(closed) pairing **also repeats in pre-formatter trace `231656`**.
Status F7 differs from F3 by 0x04, an evidence-backed candidate
status flag only; its actual vendor semantic has not been recovered.
Do not force unknown 0x03FE/0x0057 bit meanings from earlier
0x03FF/0x0058 sequences. All four current 0x82 replies report
proper actual raw data length `0x0006` and unused 0xFF bytes,
rather than old x64 capacity `0x0190` and zero-fill. The two
47 12 replies are each original-x86-matching
`0000000000000000000002000000FFFF`, without fivefold retries.

This source-and-UI correlation eliminates any demonstrated
jaw-recognition regression that would justify changing `3490709`
or generic INTEN, ISR, CLRIRQ, PCI/DMA. Historical capture-only
observations of missing pending 0x0200 remain valid for those
particular snapshots and must not be promoted into absent UI state.


## Probe physical ADC/I2C sequence versus host SPI and forwarded firmware opcodes (2026-09-29)

The user supplied additional **electrical front-panel hardware
knowledge**: a probe-associated analog ADC reading is first
used to classify the connected device as ProBus; the
front-panel EEPROM is then read via I2C; physical probe
control also operates via I2C.

This must be kept distinct from this file's proven original
**Windows-driver implementation**:

- A5FB **family-0 opcode 0x90** is a local **BAR1 SPI**
  transaction through SPICTL (+0xA0), SPIDAT (+0xA4),
  SPIDIN (+0xA8). Selector 0x0E has the observed 144-bit
  packet form; the original driver's host-side code does
  NOT identify this as an electrical I2C transaction to
  the probe. Whether an additional front-end controller
  bridges an internal host transport to physical I2C is
  not known.
- Other packet classes such as **family-0/1 opcode 0x4A**
  are forwarded to board firmware; the original driver
  does not parse the underlying probe/EEPROM fields.
  0x4A's returned metadata visibly includes ASCII AP015
  in the first 128 captured bytes of a 270-byte aggregate
  reply. EEPROM-derived metadata is a hardware-informed
  candidate, **not** a decoded I2C memory map.
- The firmware command-status **0x0200** is transported
  through BAR0 INTST bit 0x08 and BAR1 HWInt +0x410,
  then latched in the driver's software command mask.
  It is not itself an analog ADC identification value
  or a physical I2C START/address/data event.
- The AP015 jaw-state pair observed on actual hardware,
  `0x82 state 0x0058 -> 0x4A F7` for an opened jaw
  with XStream's unlocked warning and
  `0x82 state 0x00A7 -> 0x4A F3` for closed,
  is a **host-visible state correlation**. Which electrical
  I2C read/response or GPIO supplies that state is unknown.

The first transient wrong "1/2 clamp" recognition in
`xstream_trace_20260929_001152.jsonl` showed 0x82
`0x028C -> 0x0058` without subsequent AP015 metadata.
Do not diagnose a specific ADC conversion, EEPROM/I2C error
or physical connector condition solely from that high-level
packet omission. The exact ADC identification threshold,
I2C controller/address and front EEPROM bytes remain
unrecovered.

See
[`probus-detection-i2c-architecture.md`](probus-detection-i2c-architecture.md)
for the separate hardware account, source provenance and
targeted follow-up questions. Existing `driver/Ioctl.c`
firmware forwarding and SPI register handling are not
modified by this architecture clarification.


## Schematic hardware attribution update (2026-09-29)

Supplied `PCI Card.pdf` and `Overview.pdf` establish
a structural split that previous host-driver analysis alone
could not expose. The PCI card uses
`U3 XC2S200E` Spartan-IIE behind PI5C3861
PCI-side bus switches, with two 40-pin differential
off-card headers: `J1 Receive`, `J2 Transmit`;
clock, twelve data pairs `D0..D11`, SYNC,
RESET_ERR and stable-status wires are separately
labelled for receive and transmit. Its
`U6 XC18V02` is FPGA configuration PROM,
and `U11 DS2433` is local 1-Wire ID storage.
The acquisition-board Overview separately
identifies `Timebase (TB)`, `ADC+MAM (AM/AM2)`,
`FPGA's (FP)`, `UP Control (UP)`, front
ends and a distinct `I2C(0:5)` bus.

The existing driver-visible BAR/IOCTL logic operates
through the PCI interface, but these schematic pages
do NOT give the internal FPGA's BAR implementation
or the protocol/firmware on either end of the RX/TX
link. The acquisition board's AM/AM2/FP FPGA blocks
are not the PCI Spartan U3. In particular,
A5FB family-0 opcode `0x90` is demonstrably
**local BAR1 SPI**, but that should not be
called the physical probe bus: per the user's
hardware information, ADC-classified ProBus
identity is subsequently read from a **front
EEPROM over I2C**, and physical probe control
uses I2C. The relation of host SPI/link to
I2C controller remains open. Family-0/1 0x4A
can return AP015 metadata, but the physical
EEPROM address/content and byte-level map
are not directly available from the trace.

Canonical schematics-based hardware map:
[`pci-card-acquisition-board-topology.md`](pci-card-acquisition-board-topology.md);
probe interfaces:
[`probus-detection-i2c-architecture.md`](probus-detection-i2c-architecture.md).
No driver source changes are implied by these
topology observations.


## Current follow-up: 0x00223044 implemented; CFDC2194 status producer unresolved (2026-09-29)

The user's Ghidra rerun supplied `asm_asm_12d24.txt`,
`asm_asm_12bae.txt`, `field_0x138.refs.txt` and
`field_0x116a.refs.txt`. These resolve the two proposed
compatibility targets to different confidence levels:

- **Original `0x00223044` (FUN_00012D24):** requires
  **exactly four output bytes**, calls
  `READ_REGISTER_ULONG(*(main+0x138))` and returns
  success with `Information=4`. Original startup
  `FUN_00014847` sets `main+0x138 = BAR0 base+0x000`.
  Therefore this is a **read-only BAR0 FVER/START DWORD
  query**. The native x64 driver now implements it
  as `LECS65_IOCTL_READ_START_REGISTER` using
  `LecResolveRegister(DevExt,0,0x000)`, checks exact
  output length and returns four bytes. Source
  is now **confirmed on the real x64 scope for this positive path**:
  `lecdiag start-register` returned `0x00000002` via the legacy
  four-byte `0x00223044` request and `0x00000002` via the separate
  generic `REGISTER_READ BAR0+0x000`, reporting `PASS`.
  No register write occurs; XStream/AP015 regression after this
  specific driver change is not yet reported.
- **Original `0xCFDC2194` (FUN_00012BAE):** requires
  **exactly 29 output bytes**; zero-initializes the
  response, stores `DWORD 2` at byte offset `+0x04`,
  copies the stored value from original
  `main+0x116A` to byte offset `+0x08`,
  then **clears the stored value**. The field-displacement
  scan discovers only the handler's own
  `LEA [ESI+0x116A]`, not the producer of nonzero
  status. Since aliasing/indirect writes can escape
  literal-displacement scans, this does NOT prove
  that the original value is always zero. Do NOT
  fabricate a constant-zero `STATUS_SUCCESS`
  handler just for coverage. This second x64
  implementation remains pending until the
  true latch source and synchronization are mapped.

The older 2026-09-28 audit below remains historical:
`0x00223044` has since received an x64 case,
whereas `0xCFDC2194` remains unported at this point.
