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
