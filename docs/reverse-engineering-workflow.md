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
