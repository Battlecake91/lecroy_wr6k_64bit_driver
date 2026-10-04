# Legacy LecS65AcqDrv.sys architecture

This document records the architectural reconstruction of the original 32-bit
LeCroy `LecS65AcqDrv.sys` used by the WR6k family.

The goal is broader than the minimum required for the x64 replacement driver:
the complete legacy driver is being classified, including LeCroy-specific
hardware logic, WDM/DriverWorks infrastructure, PnP/power state machines,
allocation/string/container helpers and compiler/runtime support.

A complete pseudocode snapshot for every function currently recognized by
Ghidra was exported in commit
`4e622dc2528127cce13ae35146554a9b2ada2982`. Ghidra currently recognizes
420 functions. Raw x86 assembly remains the authority when the decompiler loses
calling-convention, stack-argument, fallthrough or bit-level detail.

## Top-level architecture

The binary is a DriverWorks-style WDM driver rather than a minimal hand-written
WDM driver. Its major layers are:

```text
PE entry / DriverWorks runtime
    |
    +-- global driver object / registry-path support
    |
    +-- CLecS65AcqDrvDevice factory
            |
            +-- generic DriverWorks/WDM device base
            |     +-- generic IRP dispatch
            |     +-- PnP state machine
            |     +-- power state machine
            |     +-- queue/cancel/remove handling
            |     +-- interfaces / symbolic links
            |
            +-- LeCroy hardware subobject at device +0x1E0
                  +-- PCI resource mapping
                  +-- register descriptors / register list
                  +-- ISR + DPC
                  +-- event objects
                  +-- transfer/DMA objects
                  +-- acquisition MAM/MTT engine
                  +-- CFDC2110 protocol
                  +-- JTAG / SPI
                  +-- 1-Wire / Dallas DS2433
```

## Driver entry and device creation

### `entry` at 0x1DF42

The PE entry point initializes the support runtime, creates two class/registry
helper objects and then calls `FUN_0001DE46`.

### `FUN_0001DE46`

This is the DriverWorks-level driver initialization path. It:

- stores the incoming `DRIVER_OBJECT`;
- makes a private copy of the service registry path;
- initializes the global registry-path state;
- initializes the framework driver object;
- invokes the driver's virtual initialization callback.

### `FUN_000104F4`

This is the LeCroy device factory/AddDevice path. It constructs a device named
from `CLecS65AcqDrvDevice`, allocates the device object and calls
`FUN_00010B3C`.

### `FUN_00010B3C`

This is the large `CLecS65AcqDrvDevice` constructor. It:

- initializes the DriverWorks/WDM base device;
- constructs the LeCroy hardware subobject at `this + 0x1E0` via
  `FUN_00014212`;
- installs the derived device and hardware-subobject vtables;
- creates/initializes lower-device attachment state;
- initializes PnP/power state and synchronization objects;
- registers the device interfaces used by the original stack.

### Destruction

`FUN_00010C98` is the matching main-device destruction path. It unmaps mapped
resources, destroys the LeCroy hardware subobject through `FUN_000134F6`,
destroys framework support state and releases the base DriverWorks device.

`FUN_000134F6` is the large LeCroy hardware-subobject destructor. It tears
down, in reverse order, CFDC2110 state, JTAG/SPI/trace/event objects, register
descriptors, transfer/acquisition helpers and other embedded objects.

## Generic DriverWorks / WDM dispatch layer

### `FUN_0001A420`: generic IRP dispatcher

This function reads `IO_STACK_LOCATION.MajorFunction`, selects a handler from
a DriverWorks dispatch table and wraps the call with the framework's
device-state, outstanding-I/O, remove/power and completion logic.

The observed special cases correspond to standard WDM major functions,
including:

- `IRP_MJ_CREATE` (0x00);
- `IRP_MJ_CLOSE` (0x02);
- `IRP_MJ_SHUTDOWN` (0x10);
- `IRP_MJ_CLEANUP` (0x12);
- power and PnP dispatches routed into their dedicated state machines.

### `FUN_0001A6EA`: PnP dispatcher

This is the full PnP minor-function state machine. Its switch uses the standard
WDM PnP minor codes:

```text
0x00 START_DEVICE
0x01 QUERY_REMOVE_DEVICE
0x02 REMOVE_DEVICE
0x03 CANCEL_REMOVE_DEVICE
0x04 STOP_DEVICE
0x05 QUERY_STOP_DEVICE
0x06 CANCEL_STOP_DEVICE
0x07 QUERY_DEVICE_RELATIONS
0x08 QUERY_INTERFACE
0x09 QUERY_CAPABILITIES
0x0A QUERY_RESOURCES
0x0B QUERY_RESOURCE_REQUIREMENTS
0x0C QUERY_DEVICE_TEXT
0x0D FILTER_RESOURCE_REQUIREMENTS
0x0F READ_CONFIG
0x10 WRITE_CONFIG
0x11 EJECT
0x12 SET_LOCK
0x13 QUERY_ID
0x14 QUERY_PNP_DEVICE_STATE
0x15 QUERY_BUS_INFORMATION
0x16 DEVICE_USAGE_NOTIFICATION
0x17 SURPRISE_REMOVAL
```

The function contains the expected DriverWorks policy around forwarding,
waiting, canceling queued IRPs, disabling interfaces, detaching the lower
device and deleting symbolic links/device state.

### `FUN_0001B096`: power dispatcher

This is the full power-IRP state machine. It handles the standard power minor
functions and distinguishes system-power from device-power requests through
the power IRP stack parameters. It coordinates:

- `PoStartNextPowerIrp`;
- `PoCallDriver`;
- requested system/device power transitions;
- pending power IRPs;
- device-power state caching;
- wake/power sequencing and framework policy.

### Framework helpers

Important recovered support functions include:

| Address | Role |
|---|---|
| `0x1BED2` | synchronous forward-and-wait helper for an IRP |
| `0x1BF5A` | synchronous lower-device `IoBuildDeviceIoControlRequest` helper |
| `0x1DD1E` | event / synchronous-I/O helper object |
| `0x1B98E` | lower-device attachment via `IoAttachDeviceToDeviceStack` |
| `0x1BB4E` | queued-IRP cancellation/removal helper |
| `0x193B8` | cancel-spinlock acquire wrapper |
| `0x1919A` | IRP trace formatter |
| `0x1D8A2`, `0x1DC24` | registry-key open/create helpers |
| `0x1D572` | DriverWorks device creation / symbolic-link helper |

## StartDevice and PCI resources

### `FUN_000115C4`

This is the LeCroy StartDevice/resource-initialization path.

It:

1. obtains translated resource information through framework helpers;
2. initializes/maps three hardware resource regions using
   `FUN_000106E6`;
3. calls `FUN_00014847` to bind the LeCroy register model to those mapped
   resources;
4. creates trace/register-control objects;
5. connects the interrupt through `FUN_0001060C -> FUN_00018660 ->
   IoConnectInterrupt`;
6. initializes the driver's DPC.

`FUN_000106A0` is the matching mapped-resource cleanup helper and ultimately
uses `MmUnmapIoSpace`.

## Register descriptor architecture

The original driver does not scatter all MMIO accesses as naked offsets.
`FUN_00014847` constructs descriptor objects containing:

- mapped register address;
- human-readable register name;
- BAR/resource identity;
- physical register offset;
- access/type metadata;
- cached/shadow value where appropriate.

`FUN_00013FA6` serializes those descriptors into the legacy 266-byte
(`0x10A`) register-list record exposed to user mode.

The main register-list/query path is:

```text
descriptor construction
    -> FUN_00013FA6 InsertRegister
    -> dynamic 0x10A-byte record array
    -> FUN_00012C18 query-all/query-one
```

`FUN_00012CAC` is the outer SetOneRegister IOCTL wrapper and
`FUN_0001259A` performs the original unchecked pointer-table-index write.

### Important descriptor helpers

| Address | Role |
|---|---|
| `0x11962` | initialize register-descriptor object |
| `0x119BC` | copy register-descriptor metadata |
| `0x107FE` | write register and update descriptor shadow |
| `0x120FA` | allocate array of 0x10A-byte records |
| `0x121FA` | grow 0x10A-byte record array |
| `0x12184` | grow DWORD/pointer array |
| `0x12166` | free/reset dynamic-array object |
| `0x13F3C` / `0x13F70` | append to dynamic arrays |
| `0x13FA6` | insert named MMIO register into public register list |

## Recovered register groups

`FUN_00014847` binds these known registers.

### BAR0 / interrupt-DMA region

Known descriptors include:

```text
+0x000  START / acquisition firmware/start register
+0x004  CLRERR
+0x008  CLRIRQ
+0x040  SGTA / acquisition DMA descriptor-table address
+0x044  IIMTC
+0x048  IIMCL
+0x04C  IIMST
+0x080  INTST
+0x084  INTEN
```

### BAR1 / acquisition-control region

```text
+0x00   ITMODE
+0x0C   ACQFVER
+0x20   JTAGNUM
+0x24   JTAGDAT
+0x28   JTAGDIN
+0x40   MAMDAT
+0x44   MAMPGO
+0x60   MAMSEQ
+0x64   MAMRGO
+0x80   MTTCTL
+0x84   MTTRGO
+0x90   MTTNUM
+0xA0   SPICTL
+0xA4   SPIDAT
+0xA8   SPIDIN
+0xC0   GPIODIR
+0xC4   GPIODAT
+0xE0   LEDCTL
+0xE4   RMIDIV
+0xE8   RMICUM
+0xEC   ACQDIV
+0xF0   ACQCUM
+0xF4   PFREG
```

### BAR2 / auxiliary region

```text
+0x00   BUZZER
+0x40   ONEWIRE
```

The CFDC2110 transport block additionally uses BAR-backed offsets documented in
`reverse-engineering-workflow.md`, including Tx/Rx control/count/data,
SetIRQ and HWInt.

## Interrupt architecture

### `FUN_000108D6`: ISR

The ISR:

1. reads the interrupt-status register;
2. masks it against the enabled interrupt state;
3. ORs enabled pending bits into the software pending bitmap;
4. performs source-specific immediate hardware acknowledgement;
5. on the error source, reads ERRS, accumulates the software error latch and
   converts hardware error bits into compact clear/ack bits;
6. writes back/acknowledges the interrupt source;
7. performs the original stuck-error check after a 1-us stall;
8. queues the DPC.

### `FUN_00011390`: DPC

The DPC consumes individual software-pending sources and:

- wakes the acquisition transfer event for completion source bit 0;
- wakes legacy user events;
- latches CFDC2110 asynchronous command/status bits;
- reads/clears HWInt where appropriate;
- signals the internal transport receive event.

### Pending-bit helpers

`FUN_00011DC2`, `11DD8`, `11DEE`, `11E04` and adjacent small helpers
atomically test/consume individual software-pending bitmap bits.

`FUN_00011E46` publishes the software enable mask to the real INTEN register.

## Transfer/DMA object model

### Transfer registration

`FUN_0001731C` validates and registers a user transfer. It:

- validates token/user-size input;
- subtracts the four-byte API overhead from the requested buffer size;
- allocates a 0x40-byte transfer object;
- builds the board's 32-bit DMA descriptor representation;
- links the transfer object into the per-device transfer list;
- initializes a completion event;
- records the owning process.

`FUN_000172A2` removes a transfer from the linked list.

The x64 replacement intentionally improves this ownership model by using
process-owned opaque tokens rather than exposing legacy pointer-like values.

### Common acquisition orchestrator: `FUN_00012D6A`

This function:

- clears GPIODAT bit 16 through `FUN_000120DC`;
- either programs full MAM channel state or refreshes MAMSEQ only;
- computes/defaults the MAM launch count;
- calls the synchronous hardware launch/wait helper;
- performs IIM status cleanup;
- reads the error/status register;
- returns the requested transferred-byte count through the result slot.

### `FUN_000171DE`: synchronous hardware transfer

This helper:

- programs SGTA and IIMTC from the selected transfer object;
- resets the selected transfer completion event;
- enables the relevant interrupt state;
- writes the selected launch register (MAMRGO or MTTRGO);
- waits for completion for up to five seconds;
- disables/cleans the interrupt state after completion.

## CFDC2138 / CFDD219F acquisition frontends

`FUN_000141DC -> FUN_00013C84` is the METHOD_BUFFERED acquisition frontend.

Its variable request form is:

```text
DWORD transfer_token
BYTE  channel_count
repeat channel_count:
    BYTE ignored_pair_byte
    BYTE channel_id
DWORD config
DWORD requested_bytes
```

The original contains a divide/modulo-by-zero bug when
`channel_count == 0`; the x64 replacement must reject zero explicitly.

`FUN_000141F8 -> FUN_00013DC6` is the METHOD_NEITHER frontend. It uses the
same acquisition engine but creates a transient transfer from caller memory and
has the original unsafe direct-user-pointer assumptions.

See `reverse-engineering-workflow.md` for the complete MAMDAT/MAMSEQ format.

## CFDC2110 protocol object

The CFDC2110 state object is embedded in the hardware subobject at offset
`+0xEA8`.

`FUN_000158EE` constructs it and initializes:

- JTAG and SPI helper pointers;
- direct register-region pointers;
- the common TX/RX transport object;
- response-buffer state;
- timer/event state;
- family-2 special-register pointers.

`FUN_000159E2` binds its three register regions and initializes the common
transport block.

`FUN_00016544` destroys buffered response/timer/transport state.

The full family/opcode and TX/RX protocol reconstruction is documented in
`reverse-engineering-workflow.md`.

## Serial FPGA/GPIO programming

`FUN_00011CFF` is the original `0xCFDC2130`
`IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA` handler.

For every input byte it:

1. reads BAR1 GPIODAT;
2. replaces only mask `0x0E00` (bits 15:13);
3. takes input bits 7:5 and shifts them into that field;
4. writes the resulting full GPIODAT DWORD.

This is a byte-stream hardware-programming interface and remains a risky live
write path.

## Dallas / 1-Wire subsystem

The ONEWIRE controller is BAR2 +0x40.

### Low-level operations

`FUN_00016CB0` performs reset/presence detection.

`FUN_00016CF0` writes one byte LSB-first.

`FUN_00016D22` reads one byte LSB-first.

### ROM ID

`FUN_00016FAC` sends Dallas command `0x33 READ ROM`, reads eight bytes and
validates the Dallas CRC. It retries up to ten times.

Outer IOCTL handler: `FUN_000130EA`.

### Memory read

`FUN_00016F2C` executes:

```text
CC  SKIP ROM
F0  READ MEMORY
00  address low
00  address high
... requested bytes ...
```

Outer IOCTL handler: `FUN_000131B5`, accepting 1..0x200 bytes.

### Memory write

`FUN_00011F54` is the top-level Dallas write IOCTL handler
(`0x00223088`). It writes the supplied range in chunks of at most 0x20 bytes
through `FUN_00016D90`, then reads the complete range back with
`FUN_00016F2C` and compares it byte-for-byte. The outer operation is retried
up to three times when verification fails.

`FUN_00016D90` implements the DS2433 scratchpad/copy sequence:

```text
reset
CC                  SKIP ROM
0F                  WRITE SCRATCHPAD
TA1, TA2            target address
<data>

reset
CC                  SKIP ROM
AA                  READ SCRATCHPAD
read TA1, TA2, ES
read data and verify against source

reset
CC                  SKIP ROM
55                  COPY SCRATCHPAD
TA1, TA2, ES
delay about 100 ms for programming
```

The function itself retries its scratchpad/copy sequence up to three times.

This confirms that Dallas WRITE is an intentionally verified production path
in the original driver. Live testing on the licensed original DS2433 remains a
separate safety decision.

## Trace infrastructure

The binary contains its own configurable trace system in addition to
DriverWorks diagnostics.

`FUN_0001329E` constructs `CKeTraceControl`.

`FUN_00018A1C` is the common formatted trace/logging backend.

`FUN_00012290` changes trace levels.

The public legacy trace/config IOCTLs use 0x108-byte/related trace records and
the same dynamic-array infrastructure used by the register metadata system.

## Support classes and runtime

The high-address region is not one homogeneous compiler-runtime blob. It
contains three categories.

### Concrete DriverWorks/WDM support

Examples:

- device attachment, creation and destruction;
- synchronous IRP forwarding;
- PnP state machine;
- power state machine;
- cancel-safe device-queue manipulation;
- interface/symbolic-link control;
- registry-key and Unicode-string wrappers;
- synchronization/event wrappers.

### Generic container/string/allocation support

Examples include:

- tagged pool allocation/free wrappers;
- dynamic arrays;
- Unicode string builders;
- linked reference-count records;
- list/queue helpers.

### Compiler runtime / exception support

True compiler/runtime helpers include known routines such as:

- `__alldiv`;
- `__allshr`;
- SEH/unwind helpers;
- imported/micro-thunk CRT functions.

These are classified as implementation infrastructure rather than LeCroy
hardware semantics, but they are retained in the full reconstruction scope.

## Current reconstruction status

The major architectural subsystems are now identified:

- driver entry/runtime;
- AddDevice/device construction/destruction;
- PnP;
- power;
- generic IRP dispatch;
- StartDevice/resource mapping;
- register descriptor/list architecture;
- ISR/DPC;
- event registration;
- transfer registration and DMA;
- buffered and METHOD_NEITHER acquisition;
- MAM/MTT;
- CFDC2110;
- JTAG;
- SPI;
- serial FPGA/GPIO programming;
- Dallas/1-Wire read and write;
- trace infrastructure;
- DriverWorks support classes.

The original PE32 code is now classified at both function and subsystem level.
All **420 Ghidra-recognized functions** have semantic entries in
[legacy-driver-function-map.md](legacy-driver-function-map.md). The
2026-10-04 focused ASM pass resolved the two control-flow warnings
(`0x11894`, `0x19BB8`), bringing the recognized inventory to
**420 classified / 0 pending ASM warnings**.

The [vtable and ASM audit](legacy-driver-vtables-and-asm-audit.md) now confirms:

- the main LeCroy device vtable at `0x1C500`;
- the hardware base and derived vtables at `0x1C8BC` and `0x1C62C`;
- the deliberate DPC-forwarding override at derived slot `+0x24`;
- the IRP/cancel tail-dispatch through hardware slot `+0x20`;
- a direct `KfReleaseSpinLock` tail jump, not a broken jump table;
- small this-adjustment thunks and the SEH-sensitive MDL code paths.

**Important coverage limit:** "420/420 classified" is not a claim that the
complete binary has been reverse engineered. Vtables identify executable
this-adjusting thunks (notably `0x10C34`, `0x10C62`, `0x10C8C`,
`0x10D62`, `0x10D6E`) not necessarily present as separate
Ghidra-recognized functions. Fixed-length DWORD dumps also extend into
ASCII strings and unrelated adjacent objects. Further work must identify
all such code ranges/indirect targets, establish class/method semantics
and verify the important binary paths, not just rename functions.

The initial whole-executable instruction census (commit
`cabdc5f9f882044bb3928545278024b1aad5ac9d`) now identifies
**90 decoded unowned code clusters** totaling **3,207 bytes**, including
the 888-byte main-device vtable target `0x11018..0x1138F` (slot
`0x1C54C`, relative offset `+0x4C`). These are not accounted for in
the 420 recognized internal function inventory. All 90 clusters are listed
in [executable-code coverage](legacy-executable-code-coverage.md).

The function-manager count of **507** includes the **420** internal
functions plus **87** external/imported entries and is not evidence that
87 new internal functions appeared. The next static pass exports all
orphan-cluster assembly and separately classifies the 6,493 bytes of
executable-block content not decoded as instructions. This is source-only
reverse engineering; no x64 hardware regression is implied.

## Complete executable audit and missed device dispatch

The 2026-10-04 full orphan ASM export has now conclusively
identified `0x11018..0x1138F`, main LeCroy device vtable slot
`0x1C500+0x4C`, as the **original top-level 27-case IOCTL
dispatcher**. Unlike the earlier manually pieced-together IOCTL case
list, this is a direct raw-assembly proof of the whole dispatch tree.
See [IOCTL map](ioctl-map.md) and
[complete decoded executable audit](legacy-executable-code-coverage.md).

The first whole-executable byte-classification pass shows
48,035 decoded-instruction bytes, 5,377 bytes of defined data and
1,116 undefined bytes across 54,528 executable-section bytes. All
90 Ghidra-unowned decoded-code clusters have been semantically
categorized by family, but short independent functions/virtual thunks
still require function-boundary recovery and separate decompilation.
This is not yet 100% executable-byte provenance.
