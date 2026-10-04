# Reverse-engineering workflow for the legacy x86 driver

The preferred path for the remaining x64 port is static reconstruction of the
legacy x86 driver, supported by passive runtime traces where needed.

Do not treat decompiler output as source code that can simply be rebuilt for
x64. The goal is to recover semantics, types, hardware accesses and protocol
state machines, then re-implement them safely in the native x64 driver.

## Recommended tooling

Use Ghidra on a modern Windows or Linux workstation.

Import the original `LecS65AcqDrv.sys` as a PE32 x86 kernel driver and run the
default analysis.

Recommended analysis options:

- Windows PE loader
- x86:LE:32:default processor
- aggressive instruction finder enabled
- function ID / reference analysis enabled
- demangler enabled
- stack analysis enabled

No symbols are expected to resolve automatically. The legacy PDB path embedded
in the binary is historical and is not required for the analysis.

## First functions to label

The following functions are already identified from prior disassembly and
runtime work:

```text
0x16BBA  A5FB command handler
0x169B4  85FB response handler
0x16594  C5FB related handler

0x16FAC  Dallas READ ROM / ID
0x16F2C  Dallas memory read
0x16CB0  1-Wire reset / presence
0x16CF0  1-Wire write byte
0x16D22  1-Wire read byte
```

Suggested names:

```text
LecHandleA5fbCommand
LecFetch85fbResponse
LecHandleC5fb
LecDallasReadIdLow
LecDallasReadMemoryLow
LecOneWireReset
LecOneWireWriteByte
LecOneWireReadByte
```

## CFDC2110 analysis target

For every A5FB command seen in XStream, follow the decompiler call tree from
`LecHandleA5fbCommand` until one of these concrete endpoints is reached:

- BAR register read/write
- BAR buffer read/write
- JTAG helper
- SPI helper
- DMA/MDL helper
- interrupt/event helper
- internal response-buffer builder

Record for each command:

```text
family
opcode
input structure
called helper(s)
BAR number
register offset(s)
read/write direction
response structure
side effects / state variables
```

Current high-priority runtime commands:

```text
family 1  opcode 0x81
family 0  opcode 0x84
family 0/1 opcode 0x4A
family 0  opcode 0x92
family 0/1 opcode 0x42
family 2  opcode 0x05
family 2  opcode 0x02
```

### Recovered CFDC2110 packet and transport semantics

Static Ghidra analysis on 2026-10-03 established the following original-driver
packet layout for the A5FB command path:

```text
+0x08  packet type, expected 0x40
+0x09  family
+0x0A  opcode
+0x0B  family/opcode-specific payload
```

The A5FB family dispatch is:

```text
family 0 -> FUN_00016A66
family 1 -> FUN_000165A6
family 2 -> FUN_000166A8
```

Known high-priority runtime commands now map as follows:

```text
family 1 / opcode 0x81 -> FUN_00015DB8 -> FUN_000176E6
family 0 / opcode 0x84 -> FUN_00016168 -> FUN_000176E6
family 0 / opcode 0x4A -> FUN_00016168 -> FUN_000176E6
family 1 / opcode 0x4A -> FUN_00015DB8 -> FUN_000176E6

family 0 / opcode 0x92 -> FUN_0001600E -> WRITE_REGISTER_ULONG
family 1 / opcode 0x92 -> FUN_00015DEA -> READ_REGISTER_ULONG

family 0 / opcode 0x42 -> FUN_00015ACC
family 1 / opcode 0x42 -> FUN_00015C7E

family 2 / opcode 0x02 -> FUN_000163B2
family 2 / opcode 0x05 -> direct transport-register write sequence in FUN_000166A8
```

The original transport-register group constructed by `FUN_0001785B` is:

```text
BAR-backed base + 0x100  SetIRQ
BAR-backed base + 0x400  TxControl
BAR-backed base + 0x404  RxControl
BAR-backed base + 0x408  TxCount
BAR-backed base + 0x40C  RxCount
BAR-backed base + 0x410  HWInt
BAR-backed base + 0x420  TX data window
BAR-backed base + 0x600  RX data window
```

`FUN_000176E6` is the transmit path. It rejects odd byte lengths, converts
the byte length to 16-bit words, splits larger requests into chunks of at most
0x78 words, writes each 16-bit word through `WRITE_REGISTER_BUFFER_ULONG`
into successive DWORD-spaced addresses starting at offset 0x420, writes the
chunk word count through the TxCount register helper, and writes TxControl to
start/continue the transfer. Transfers larger than one chunk use the 0x4000
control bit; the transmitted control word is then marked with 0x8000 before
the TxControl write.

`FUN_00017560` polls TxControl and reports ready only when the high-byte sign
bit is clear and the low byte is zero. `FUN_000176E6` polls this state before
and between chunks and uses a bounded delay loop before failing the transfer.

The request/response helper `FUN_0001619A`:

1. resets the transport event;
2. enables the receive/interrupt state via `FUN_000160A8`;
3. transmits the request via `FUN_000176E6`;
4. waits for the transport event with the configured timeout;
5. receives the response through `FUN_00017578`.

`FUN_000160A8` toggles global state bit 0x8 in `DAT_0001CE18` and
synchronizes callback `FUN_00012EAE` through the existing hardware object.

`FUN_00017578` is the receive path. It reads RxControl, requires a nonzero
chunk count no greater than 0x78 words, validates that the resulting
two-bytes-per-word payload fits the caller buffer, and reads successive
16-bit values from DWORD-spaced addresses starting at offset 0x600 through
`READ_REGISTER_BUFFER_ULONG`. After each block it resets the event and
acknowledges/clears the consumed RxControl state. If the continuation bit is
set, it waits for another event and repeats until the complete response is
received or the wait times out.

The family-0 opcode-0x92 direct register-write payload is:

```text
byte  +0  region selector 0, 1 or 2
word  +1  register offset
dword +3  value
```

The original implementation resolves the selected mapped base and executes
`WRITE_REGISTER_ULONG(base + offset, value)`. Family 1 opcode 0x92 is the
corresponding direct read path using the same region/offset selector and
returns the DWORD value through the normal response-buffer staging path.

### Recovered JTAG and family-2 special-register semantics

Further Ghidra analysis on 2026-10-03 resolves family 0/1 opcode 0x42 as the
board JTAG path. The board register descriptors are named directly by the
original driver:

```text
BAR-backed base + 0x20  JTAGNUM
BAR-backed base + 0x24  JTAGDAT
BAR-backed base + 0x28  JTAGDIN
```

`FUN_00015802` programs JTAGNUM, `FUN_00015848` programs JTAGDAT and
`FUN_0001586E` reads JTAGDIN. The low-level encoding is:

```text
JTAGNUM = ((selector != 0) << 8) | (bit_count & 0x0F)
```

A 16-bit block is therefore encoded with a zero low nibble. Transfers longer
than 16 bits are split into 16-bit blocks. Each input data block supplies two
16-bit values; `FUN_00015848` writes them as one DWORD using
`(first_word << 16) | second_word`. Family 0 opcode 0x42 performs the JTAG
shift/write operation without staging returned JTAG data. Family 1 opcode 0x42
uses the same JTAGNUM/JTAGDAT sequence and additionally reads JTAGDIN into the
response buffer. The exact electrical/domain meaning of the two 16-bit halves
is not yet proven and should not be named speculatively.

The CFDC2110 object is embedded at board-object offset `+0xEA8`. This resolves
two previously opaque family-2 fields through their outer-object aliases:

```text
CFDC2110 +0x15E == board +0x1006 -> pointer to ITMODE descriptor
CFDC2110 +0x162 == board +0x100A -> pointer to LEDCTL descriptor
```

`FUN_00014847` wires these aliases after creating the named board register
descriptors. Consequently the family-2 direct-register commands are now
identified as:

```text
family 2 / opcode 0x02 -> INTST write value 0 or 1 at region 0 + 0x80
family 2 / opcode 0x05 -> ITMODE write sequence 7, then 3
family 2 / opcode 0x09 -> ITMODE write value 3
family 2 / opcode 0x0A -> ITMODE write value 2
family 2 / opcode 0x10 -> LEDCTL two-bit control write from payload bytes
```

The region mapping is also statically anchored: `FUN_000115C4` creates three
resource/register-region objects for indices 0, 1 and 2 and passes them to
`FUN_00014847` in that order. `FUN_000159E2` stores the resulting region-0
object at CFDC2110 field `+0x29`; `FUN_000163B2` writes
`region0.base + 0x80`. The same board setup names that exact register
`INTST`, so family 2 opcode 0x02 is an interrupt-test register control path.

The original register descriptor keeps the mapped MMIO address at descriptor
offset 0 and a cached/shadow value at descriptor offset +0x24. The family-2
ITMODE cases update that shadow and issue the corresponding
`WRITE_REGISTER_ULONG`.

Additional family-2 dispatch semantics from the same static analysis:

```text
opcode 0x00 -> arm/start the internal timer object from payload, wait for it,
               then fall through to the opcode-0x04 INTST pulse operation
opcode 0x01 -> arm/re-arm the internal timer using the payload delay while
               preserving/recomputing remaining time across an active timer
opcode 0x02 -> write INTST = payload[0] (0 or 1)
opcode 0x03 -> send the packet through the common TX transport with mode/value
               0x19 and change the subsequent response timeout state to 10
opcode 0x04 -> temporarily mask the corresponding software interrupt-state bit,
               pulse INTST high/low N times, then restore the bit if needed
opcode 0x05 -> ITMODE 7 -> 3 sequence
opcode 0x06 -> returns status 0x10 (not implemented/supported by this path)
opcode 0x07 -> returns status 0x10 (not implemented/supported by this path)
opcode 0x08 -> returns status 0x10 (not implemented/supported by this path)
opcode 0x09 -> ITMODE = 3
opcode 0x0A -> ITMODE = 2
opcode 0x10 -> LEDCTL two-bit write
opcode 0x40 -> re-run the original board interrupt/error-state service and
               reset/acknowledgement helpers, then return success
```

The timer object at CFDC2110 field `+0x186` is allocated by `FUN_000158EE`
and initialized through `KeInitializeTimerEx`. Opcodes 0x00 and 0x01 use its
timer/event methods; fields `+0x18A/+0x18E` store the last system-time sample
and `+0x192` stores the tracked delay/remaining-time value. This establishes
timer/state-machine behavior, but not a higher-level acquisition-domain name.

These findings recover the common CFDC2110 physical request/response transport,
JTAG path and several family-2 board-control operations, but they do not by
themselves assign complete domain-level meanings to every family/opcode
payload. Continue opcode-specific analysis only where native x64 compatibility
or an observed runtime difference requires it.

## CFDC2138 variable buffered-transfer ABI

Focused static analysis of original `FUN_000141DC -> FUN_00013C84` on
2026-10-03 resolves the previously unobserved variable-length buffered
`0xCFDC2138` request parser.

The original METHOD_BUFFERED request layout is:

```text
DWORD transfer_token
BYTE  channel_count

repeat channel_count times:
    BYTE ignored_pair_byte
    BYTE channel_id

DWORD config
DWORD requested_bytes
```

The total input size implied by this structure is therefore:

```text
13 + 2 * channel_count bytes
```

The one-channel runtime form observed previously is the 15-byte special case.

Important parser behavior:

- `channel_count` is stored as an unsigned byte, so the packet syntax can
  represent at most 255 list entries.
- The first byte of each two-byte channel pair is skipped by the original
  parser and is not validated by `FUN_00013C84`.
- The second byte is the actual channel value passed to the transfer-list
  builder.
- Hardware sequence encoding masks the channel value to six bits
  (`channel_id & 0x3F`).
- The final channel-list entry is marked separately through the boolean
  "last entry" argument to `FUN_00017EE0`.
- The list containers are dynamically allocated/resized; there is no smaller
  fixed C-array limit in this parser path.
- A zero-channel request does not produce a useful transfer-list object and
  must not be treated as an enabled native form merely because the field is a
  byte.

After the channel list, `FUN_00013C84` stores one global `config` DWORD and
one global `requested_bytes` DWORD. These are not repeated per channel.

A zero-channel request is a legacy bug case rather than a valid empty transfer:
`FUN_00013C84` skips the channel loop and later executes an unsigned division
by `channel_count` without first checking for zero. With
`channel_count == 0` this reaches a divide-by-zero fault instead of returning a
defined NTSTATUS. The x64 replacement must reject zero explicitly and must not
reproduce this failure mode.

The parser then enforces transfer-size constraints before invoking the common
acquisition orchestrator:

- `requested_bytes <= 0x00FFFFFF`;
- `requested_bytes % channel_count == 0`;
- for larger aggregate transfers, the original also requires the
  count/size product to satisfy the 0x400-byte block alignment path;
- the resolved registered transfer entry must exist and its stored data-byte
  size must equal `requested_bytes`.

The transfer-list helper `FUN_00017EE0` builds per-channel hardware sequence
entries containing the low six channel bits, an internal list index and the
last-entry flag. `FUN_00017D20` subsequently programs one acquisition setup
entry per parsed channel, derives the per-channel block portion from
`requested_bytes / channel_count`, then emits the sequence list before the
common launch/wait path.

`FUN_00013C84` zeroes the caller's four-byte output before starting the
transfer, calls `FUN_00012D6A`, and reports `Information = 4`.
`FUN_00012D6A` stores the requested byte count into that output DWORD after
the acquisition path.

The per-channel MAM programming helper `FUN_00017C16` is now decoded exactly.
For each channel it writes five indexed values through the MAMDAT register
wrapper and then launches the programming sequence with `MAMPGO = 0x105`:

```text
MAMDAT slot 0 = 0x0E00 | channel_id
MAMDAT slot 1 = config & 0xFFFF
MAMDAT slot 2 = (config >> 16) & 0xFFFF
MAMDAT slot 3 = per_channel_bytes & 0xFFFF
MAMDAT slot 4 = (per_channel_bytes >> 16) & 0xFFFF
MAMPGO         = 0x105
```

The indexed MAMDAT helper `FUN_000179E2` uses bits 16..23 of the write value
as the cache/index selector and the low 16 bits as the actual slot payload;
it suppresses redundant hardware writes when the cached 16-bit value for that
slot already matches. Instruction-level reconstruction confirms slot 0 as `0x0E00 | channel`.
`FUN_00017D20` constructs the four input bytes `FB A5 <channel> 0E`; on x86
little-endian the upper WORD consumed by `FUN_00017C16` is therefore
`0x0E00 | channel`.

The per-channel size supplied to this helper is derived from
`requested_bytes / channel_count`.

The remaining MAM register wiring and sequence encoding are also recovered.
`FUN_00014847` constructs the acquisition MAM helper with these BAR1 register
wrappers:

```text
BAR1 +0x40  MAMDAT
BAR1 +0x44  MAMPGO
BAR1 +0x60  MAMSEQ
BAR1 +0x64  MAMRGO
```

`FUN_00017EE0` builds one MAMSEQ DWORD for each parsed channel:

```text
bits 24:16  zero-based sequence index
bit  6      final-entry flag
bits 5:0    channel_id
```

The sequence index originates from a 9-bit source value, but the CFDC2138
packet's one-byte `channel_count` limits the buffered parser to at most 255
entries and therefore to sequence indices 0..254. No additional smaller
hardware limit is exposed by this path. `FUN_00017CDC` writes the generated
MAMSEQ values in order through BAR1+0x60.

For the buffered multi-channel acquisition path, the complete hardware setup is
therefore mechanically derived from the request:

1. parse `channel_count` channel pairs;
2. program five MAMDAT slots per channel and issue `MAMPGO=0x105`;
3. emit one MAMSEQ entry per channel, marking only the final entry;
4. program the registered transfer/DMA state;
5. launch through MAMRGO and wait on the selected transfer-entry event.

No additional hidden per-channel config or size record was found.

These findings explain how the original driver accepts multi-entry CFDC2138
requests, but they do **not** justify enabling arbitrary multi-channel forms in
the x64 replacement. Only the one-channel form has runtime evidence on the
working scope. A native multi-channel implementation should validate the exact
derived input size, reject channel values outside the intended hardware range
rather than silently reproducing the original six-bit truncation, and require
an independently registered transfer object of the matching byte size before
touching acquisition hardware.

## Complete legacy-driver reconstruction goal

The reverse-engineering scope now intentionally extends beyond the minimum
needed for the x64 port. The project goal is to reconstruct and classify the
entire captured legacy `LecS65AcqDrv.sys` as far as static analysis reasonably
allows, including DriverWorks/PnP/power/runtime support rather than discarding
it merely because it appears framework-like.

Commit `4e622dc2528127cce13ae35146554a9b2ada2982` exports decompiled C and
compact call references for every function currently recognized by Ghidra.
Raw assembly remains a second-pass tool for functions whose decompilation is
ambiguous, bit-sensitive, or dependent on calling-convention details.

The current subsystem-level architecture is maintained in [legacy driver architecture](legacy-driver-architecture.md).

**Coverage update (2026-10-04):** the original 420-function census
was only Ghidra's first recognized-function set. Two reviewed
recovery runs successfully created **107** additional internal
functions, bringing the current inventory to **527**. All now
have individual descriptions and selected pseudocode in
[legacy-driver-function-map.md](legacy-driver-function-map.md).
Only three previously decoded x86 SEH/filter fragments totaling
**22 bytes** remain outside recognized function bodies, plus one
three-byte SEH restoration instruction pending whitelisted
disassembly. See [whole-executable audit](legacy-executable-code-coverage.md).
Older descriptions of "420 functions" below are historical
baseline measurements, not the current total.

The reconstruction therefore has two deliverables:

1. recover LeCroy-specific hardware, ABI, acquisition and firmware behavior;
2. classify the remaining support functions by concrete role, including
   DriverWorks/PnP/power/IRP, allocation/string/container support, compiler
   runtime, exception support and other framework infrastructure.

Functions are no longer omitted merely because they appear unimportant to the
current x64 implementation.

### Initial low-address architecture classification

The first complete-export review establishes several previously implicit roles:

- `FUN_00010380` and `FUN_0001039A` are pool-allocation wrappers;
  `FUN_000103B6` is a null-safe pool-free wrapper.
- `FUN_000103C8` clears and closes a stored kernel handle with `ZwClose`.
- `FUN_000104F4` creates the named `CLecS65AcqDrvDevice` object and delegates
  initialization to `FUN_00010B3C`.
- `FUN_00010B3C` is the large device-object constructor. It initializes the
  DriverWorks base object, constructs the LeCroy hardware subobject at
  `this+0x1E0` through `FUN_00014212`, installs derived vtables, attaches
  the lower device stack and initializes PnP/power/device state.
- `FUN_00010A8E` maintains a linked 16-byte record list keyed by an integer
  and increments a per-key reference count.
- `FUN_00010DA0` is an IRP cancellation path for the active or queued request,
  removing queued entries from a `KDEVICE_QUEUE` before completion.

This confirms that the low-address region intentionally mixes LeCroy device
construction with DriverWorks IRP/lifecycle glue; both are part of the
reconstruction scope.

## Full legacy-function inventory status

The initial 2026-10-03 Ghidra function census recognized **420** internal
functions. Subsequent carefully selected function-boundary and opcode-checked
recovery raised that to **527**; all 527 have selected decompiled C,
reference summaries and semantic entries in
[legacy-driver-function-map.md](legacy-driver-function-map.md).
The final byte/SEH audit verifies three separate compiler-managed filter
and cleanup regions outside ordinary function bodies, not missing
C++ driver methods. See [current code coverage](legacy-executable-code-coverage.md)
and [28 IRP-major dispatch entries](legacy-driver-architecture.md).

The address region beginning around `0x182D0 (__alldiv)` is dominated by
compiler/runtime and DriverWorks infrastructure, but the complete export shows
that it also contains concrete PnP, power, IRP queue/cancel and object-support
logic. It therefore remains part of the classification effort rather than
being discarded wholesale.

## User-mode analysis

After the kernel handlers are understood, inspect
`lecaladdinhwaccesspcisvr.dll` and the acquisition-server DLLs for the same
packet constants.

The user-mode side is useful for recovering semantic names such as channel
configuration, gain, timebase, trigger setup and acquisition control.

Search for:

```text
A5 FB
85 FB
40 00 84
40 00 92
40 00 4A
40 01 4A
40 01 81
40 01 42
40 02 02
40 02 05
```

## Runtime safety

Do not replay arbitrary CFDC2110 packets against the working x86 reference
instrument.

If runtime evidence is required, prefer passive logging of XStream's normal
DeviceIoControl traffic and compare the resulting input/output buffers with the
static decompilation.

The x64 driver currently keeps CFDC2110 hardware execution disabled until the
startup/runtime command set is sufficiently understood.
