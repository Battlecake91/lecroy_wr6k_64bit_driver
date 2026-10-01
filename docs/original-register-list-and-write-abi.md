# Original register-list architecture and remaining write IOCTLs

Evidence checkpoint: 2026-09-30, after owner Ghidra export
`00eb49db5efe98042df47ba07a570017cd37419d`. This document
cross-checks the static original x86 assembly with the existing
native x64 register-list implementation. It does NOT report any
new scope-side driver installation or full XStream regression.

## Summary of established behavior

The original register-list constructor `FUN_00013434` is invoked
at hardware-subobject `this+0x11EE` by `FUN_00014212`. It
owns **two independent dynamic arrays**: one holding the original
32-bit pointers to register wrappers and another holding their
serialized 266-byte (`0x10A`) metadata records. All array
indices below are **zero based**.

The newly exported `FUN_00012184` and its assembly prove
that the pointer table grows dynamically; existing pointers
are copied to the replacement pool allocation, the prior
allocation is freed using `ExFreePool`, and the new pointer
and capacity are stored. The metadata growth is the parallel
`FUN_000121FA` routine (266 bytes per slot). The supported
first-level original IOCTL inventory remains **24/27 x64
source-represented, one gated**, and the two missing register/
FPGA write paths have not been implemented.

### Exact in-memory list layout

Offsets relative to the `CKeRegisterList` object at
`hardwareSubobject + 0x11EE`:

| Offset | Role | Supporting routine |
|---|---|---|
| `+0x00` | Pointer-array capacity in entries, initially 0 | `FUN_00017A16` / `FUN_00017A98` |
| `+0x04` | Pointer-array growth increment, initially 1 | `FUN_00013434` |
| `+0x08` | Allocator argument / pool tag | `FUN_00012184` |
| `+0x0C` | Last populated pointer index, initially `-1` | `FUN_0001326E` / `FUN_00013F3C` |
| `+0x10` | Dynamically allocated pointer array | `FUN_0001259A` |
| `+0x14` | Pointer-array status; out-of-memory `0xC000009A` | `FUN_00012184` |
| `+0x1C` | Start of second array object containing 266-byte records | `FUN_00013434` / `FUN_00013230` |
| `+0x28` | Last populated 266-byte-record index | `FUN_00013230` |
| `+0x2C` | Dynamically allocated record-array pointer | `FUN_000123B4` / `FUN_000124C2` |

Both subarrays use `lastIndex=-1` before any insertion and
incrementally allocate capacity. When the requested pointer
index `i >= capacity`, `FUN_00012184` computes

```c
newCapacity = capacity
    + ceil((i + 1 - capacity) / growIncrement) * growIncrement;
bytesToAllocate = newCapacity * sizeof(uint32_t);
```

and calls `FUN_0001039A`. The constructor supplies a nonzero
growth increment of 1. The routine explicitly handles allocation
failure with `STATUS_INSUFFICIENT_RESOURCES (0xC000009A)`.
The arithmetic uses 32-bit registers, with no separate overflow
guard visible in the exported code. Normal insertion generates
the next index internally; the dangerous **setter** bypasses
this growth/validation path entirely.

Insertion is performed in two stages:

1. `FUN_00013F3C -> FUN_0001326E -> FUN_00012184` appends
   a register-wrapper pointer at `lastIndex+1`.
2. `FUN_00013F70 -> FUN_00013230 -> FUN_000121FA`
   appends a constructed `0x10A`-byte metadata record
   at its independent `lastIndex+1`.

`FUN_00013FA6` checks the first append's result. However,
after calling the second append at original `0x140FA` it
sets its local success byte at `0x140FF` without checking
the second append's returned status. In a hypothetical
metadata-allocation failure, the arrays could disagree.
This is a source-level risk; no such failure has been observed
on the working scope. A native port should not reproduce
this failure mode intentionally.

## The 43 intended index entries, now statically cross-checked

Original `FUN_00014847` starts by pushing the **same list address**
`this+0x11EE` through the call to `FUN_000159E2`; its callee
`FUN_0001785B` uses that address as its `FUN_00013FA6`
receiver and inserts six transport registers first.
The first 15 direct `FUN_00013FA6` calls in `FUN_00014847`
then append common registers. Finally its conditional
initialization branch appends another 22 registers.

The conditional block is entered if `FUN_00012FDE`
returns success after programming/reading START and ITMODE;
the original readback tests START bit 0. The block also
sets the global gate `DAT_0001CD08` to zero, enabling
normal register-list I/O. The corresponding constructor
and teardown set that gate to `-1`. Thus the steady-state
**success path intends 6 + 15 + 22 = 43 entries**,
indexed 0 through 42, **provided each insertion succeeds**.
A failed/partial setup must not blindly assume a 43-entry table.
The early six `FUN_0001785B` call sites are NOT a separate
registration table; the owner-supplied original
`asm_asm_14847.txt` stack argument sequence and
`asm_asm_159e2.txt` prove the shared receiver.

The existing native table `g_LecLegacyRegisterList`
in `driver/Ioctl.c` has exactly these 43 entries, in
this order. Its names, BAR numbers, offsets and type
bytes were already grounded by legacy live captures;
the new static C/ASM registration analysis provides
independent constructor/order corroboration.
The type column is the serialized metadata byte,
not authorization to write through the register.

| Index | Register | BAR | Register offset | Metadata type | Registration path |
|---:|---|---|---:|---:|---|
| 0 | TxControl | BAR1 | `0x400` | 4 | transport setup |
| 1 | RxControl | BAR1 | `0x404` | 4 | transport setup |
| 2 | TxCount | BAR1 | `0x408` | 4 | transport setup |
| 3 | RxCount | BAR1 | `0x40C` | 4 | transport setup |
| 4 | SetIRQ | BAR1 | `0x100` | 2 | transport setup |
| 5 | HWInt | BAR1 | `0x410` | 4 | transport setup |
| 6 | FVER | BAR0 | `0x000` | 1 | common setup |
| 7 | ERRS | BAR0 | `0x004` | 4 | common setup |
| 8 | ERRM | BAR0 | `0x008` | 2 | common setup |
| 9 | INTST | BAR0 | `0x080` | 4 | common setup |
| 10 | IIMCL | BAR0 | `0x048` | 2 | common setup |
| 11 | CLRIRQ | BAR1 | `0x008` | 2 | common setup |
| 12 | CLRERR | BAR1 | `0x004` | 2 | common setup |
| 13 | INTEN | BAR0 | `0x084` | 2 | common setup |
| 14 | SGTA | BAR0 | `0x040` | 2 | common setup |
| 15 | IIMTC | BAR0 | `0x044` | 2 | common setup |
| 16 | IIMST | BAR0 | `0x04C` | 1 | common setup |
| 17 | BUZZER | BAR2 | `0x000` | 2 | common setup |
| 18 | ONEWIRE | BAR2 | `0x040` | 4 | common setup |
| 19 | START | BAR0 | `0x00C` | 4 | common setup |
| 20 | ITMODE | BAR1 | `0x000` | 2 | common setup |
| 21 | MAMDAT | BAR1 | `0x040` | 2 | START-conditional setup |
| 22 | MAMPGO | BAR1 | `0x044` | 2 | START-conditional setup |
| 23 | MAMSEQ | BAR1 | `0x060` | 2 | START-conditional setup |
| 24 | MAMRGO | BAR1 | `0x064` | 2 | START-conditional setup |
| 25 | SPICTL | BAR1 | `0x0A0` | 2 | START-conditional setup |
| 26 | SPIDAT | BAR1 | `0x0A4` | 2 | START-conditional setup |
| 27 | SPIDIN | BAR1 | `0x0A8` | 1 | START-conditional setup |
| 28 | JTAGNUM | BAR1 | `0x020` | 2 | START-conditional setup |
| 29 | JTAGDAT | BAR1 | `0x024` | 2 | START-conditional setup |
| 30 | JTAGDIN | BAR1 | `0x028` | 1 | START-conditional setup |
| 31 | MTTCTL | BAR1 | `0x080` | 2 | START-conditional setup |
| 32 | MTTRGO | BAR1 | `0x084` | 2 | START-conditional setup |
| 33 | MTTNUM | BAR1 | `0x090` | 1 | START-conditional setup |
| 34 | LEDCTL | BAR1 | `0x0E0` | 2 | START-conditional setup |
| 35 | ACQFVER | BAR1 | `0x00C` | 1 | START-conditional setup |
| 36 | RMIDIV | BAR1 | `0x0E4` | 2 | START-conditional setup |
| 37 | RMICUM | BAR1 | `0x0E8` | 1 | START-conditional setup |
| 38 | ACQDIV | BAR1 | `0x0EC` | 2 | START-conditional setup |
| 39 | ACQCUM | BAR1 | `0x0F0` | 1 | START-conditional setup |
| 40 | PFREG | BAR1 | `0x0F4` | 2 | START-conditional setup |
| 41 | GPIODIR | BAR1 | `0x0C0` | 2 | START-conditional setup |
| 42 | GPIODAT | BAR1 | `0x0C4` | 4 | START-conditional setup |

The size of a complete successful list is
`43 * 266 = 11,438 bytes (0x2CAE)`. This is a
**maximum normal intended full-init list**, not a
guarantee of successful registration in every
initialization, nor permission to write all entries.
Original `FUN_00012386` computes the dynamically
required output size as
`(recordLastIndex + 1) * 0x10A`. Existing native
`LECS65_LEGACY_REGISTER_COUNT=43` and
`LecFillLegacyRegisterList` already implement
the observed fully initialized profile.

## User-mode setter ambiguity resolved by runtime + DLL

The first real original `0x0022303C` runtime call is now captured and
its producing user-mode code has been statically recovered from
`lecaladdinhwaccesspcisvr.dll`.

Observed request after writing the unchanged `TxCount` value:

```text
+0x000..+0x100 = zero
+0x101 DWORD   = 2  (list index for TxCount)
+0x105         = zero
+0x106 DWORD   = 2  (new/current value)
```

The DLL builds this request from scratch and explicitly writes the selected
register enum/index to `+0x101`, proving that the query-offset versus
setter-index mismatch is intentional ABI, not a decompiler artifact.
Detailed private-binary evidence and disassembly:
[kernel PCI register setter user-mode proof](
kernel-pci-register-setter-usermode.md).

A hardened x64 port can therefore validate a real index against the known
43-entry table instead of guessing. The remaining design question is
write authorization/policy, not request-layout recovery.

## Runtime setter ABI now proven

The remaining user-mode ambiguity was closed by the owner's
`legacy_xstream_trace_setregister_3.jsonl` capture from the original
32-bit system. On the KernelPCIRegisters page, the owner wrote the same
displayed value for `TxCount`.

The original software first queried index 2 through `0x00223040`,
receiving `TxCount` with current value 2, then sent one successful
`0x0022303C` request whose 266-byte input is all zero except:

```text
DWORD +0x101 = 2   # table index
DWORD +0x106 = 2   # new value
```

Thus the original user-mode setter explicitly overwrites the query record
semantics at `+0x101`: for SET it is a table index. The query record's
name, BAR byte and type byte are not required by the kernel setter and were
zero in this real request.

This confirms that a compatible x64 implementation can use the recovered
43-entry table with a hardened bounds check. It should not reproduce the
original unbounded pointer dereference.

The only scanned XStream binary containing the literal IOCTL is the
owner-supplied private `lecaladdinhwaccesspcisvr.dll` at file offset
`0x2001C`; local disassembly shows `push 0x0022303C` at VA
`0x10020C1B` followed by a call to `0x1001D8B8`.
Do not commit the vendor DLL.

## Original XStream GUI page now identified

Owner screenshot evidence maps the recovered register-list ABI to the
original Service UI page:

```text
Service -> Development -> AladdinAcqBoard -> KernelPCIRegisters
```

The visible register list starts with the same ordered entries as the
43-entry kernel list (`TxControl`, `RxControl`, `TxCount`,
`RxCount`, `SetIRQ`, `HWInt`, `FVER`, ...). Selecting
`TxControl` shows address `400 H`, a hexadecimal value field and a
`Read` button. The corresponding service trace uses `0x00223040` for
indexed register retrieval.

This page is therefore the strongest current user-mode candidate for the
unseen SetOneRegister companion. However, the screenshot exposes no
explicit `Write` control and no runtime `0x0022303C` call was observed
during extensive read-only traversal. Do not edit/submit the value field
until the page implementation or a binary constant/call site proves how
writes are initiated and how record+0x101 is populated.

## Runtime and user-mode caller now resolve SetOneRegister completely

The earlier uncertainty about the original user-mode setter convention is
now resolved by a real original-x86 call plus static analysis of the unique
caller in the installed `lecaladdinhwaccesspcisvr.dll`.

Owner trace `legacy_xstream_trace_setregister_3.jsonl` contains exactly one
successful `0x0022303C` request after writing the unchanged value through
`KernelPCIRegisters`. The immediately preceding indexed query selects
**index 2 = TxCount**, whose current value is 2. The setter input is exactly
266 bytes and is zero-filled except:

```text
request +0x101 : DWORD 2   // zero-based register-list INDEX
request +0x106 : DWORD 2   // desired register value
```

All name bytes, BAR byte and metadata type are zero. Thus the peculiar ABI
is intentional at the user-mode boundary: the READ record's physical-offset
field and the WRITE request's array-index field occupy the same byte
position but are different logical structures.

Static analysis of the sole `0x0022303C` literal in
`lecaladdinhwaccesspcisvr.dll` confirms this construction. The setter
routine at image VA `0x10020B7C` zeroes 0x10A bytes, gets the selected
`RegisterList` CVar (class member +0x310) into +0x101, gets the requested
`PCIRegister` CVar value (class member +0x314) into +0x106, then sends
`0x0022303C`. Its notification dispatcher calls this routine specifically
for `PCIRegister` change notification flag 0x100.

The previous "exact original user-mode convention not recovered" blocker is
therefore CLOSED. Any native x64 implementation should still harden the
original by validating `index < activeKnownRegisterCount` and mapping the
index only to the known native register table before any MMIO write.

## Native x64 implementation status (2026-10-01)

The compatibility decision has now been turned into source code. Native
`0x0022303C` support is staged with the following hardened behavior:

- exact 266-byte METHOD_BUFFERED input and zero output;
- DWORD `+0x101` is parsed only as the zero-based register-list index;
- DWORD `+0x106` is the requested value;
- indices `>= 43` are rejected with `STATUS_INVALID_PARAMETER`;
- the target descriptor comes only from `g_LecLegacyRegisterList[index]`;
- normal BAR presence/alignment/range validation is applied before MMIO;
- INTEN is routed through the existing interrupt-mask helper;
- existing ERRM and SPICTL native shadows are synchronized before their MMIO
  write;
- successful completion returns `Information = 0`.

This deliberately does not reproduce the original x86 unchecked
`pointerTable[index]` access. The source has not yet been Windows-built,
installed or validated on the physical scope. The first eventual live check
must read the current value of a known register and write exactly that freshly
read value back.

## Critical ABI distinction: register OFFSET is not register INDEX

This is especially important for the missing
`0x0022303C` SetOneRegister handler. The
serialized 266-byte register record generated by
`FUN_00013FA6` has the following layout, confirmed
against its instruction stores and the native
`LecFillLegacyRegisterEntry` implementation:

| Record offset | Length | Meaning in list-query output |
|---:|---:|---|
| `0x000` | `0x100` | NUL-terminated ASCII register name, zero-padded |
| `0x100` | 1 | BAR number |
| `0x101` | 4 | **physical register offset in that BAR**, from wrapper field `+0x1C` |
| `0x105` | 1 | serialized register type |
| `0x106` | 4 | current readback/cache DWORD |

For example, the query record for `GPIODAT` contains
physical offset `0xC4` at `+0x101`, while the same
register's full-initialization **list index is 42**.
These are different concepts.

Nevertheless, original setter `FUN_0001259A`
interprets the DWORD at incoming
`request+0x101` as an **array index**:

```asm
000125CB MOV ECX,[ESI+0x10]       ; pointer array
000125CF MOV EDI,[EAX+0x101]     ; caller's DWORD
000125D5 MOV ESI,[ECX+EDI*4]     ; unchecked pointer-array access
000125D8 PUSH DWORD PTR [EAX+0x106] ; requested new value
000125E0 CALL 0x000107FE        ; physical register write
```

`FUN_000107FE` overwrites the selected wrapper's
cached DWORD at wrapper `+0x24` and calls
`WRITE_REGISTER_ULONG` on its mapped register
pointer. The sender must not merely echo an unchanged
list-query record into `0x0022303C`: the setter
treats the record's physical-offset field as an index.
The exact original user-mode convention for populating
a setter record has not yet been recovered.

The outer `FUN_00012CAC` checks
`DAT_0001CD08==0` and exact input length `0x10A`
(`STATUS_DEVICE_NOT_READY` or
`STATUS_INVALID_BUFFER_SIZE` otherwise), but
neither it nor `FUN_0001259A` visibly enforces
`index <= lastIndex`, `index < capacity`,
valid non-null wrapper, or allowed write type.
The helper is void; the wrapper sets
`STATUS_SUCCESS` and `Information=0` after
invoking it. Invalid-index experiments on the
single physical scope are not appropriate.

**Native port design gate:** only consider this
IOCTL if an actual original XStream caller is
identified. If needed, validate exact length, true
ready state, explicit index-to-known-register
mapping, BAR range, actual hardware-write
authorization, and concurrency. Do not equate
physical BAR offset with list index, or add a
success-only stub to inflate coverage. This
handler is currently intentionally absent in x64.

## Separate serial-trigger writer: CFDC2130 and GPIODAT ownership

The third Ghidra export confirms the following
original functions share a register-wrapper at
`hardwareSubobject+0x318`, physically BAR1 `+0xC4`:

- `FUN_00011CFF` (`CFDC2130`) reads the
  GPIODAT DWORD ONCE and for every supplied
  byte `b` performs
  `v = (v & ~0xE000u) | (((uint32_t)b << 8) & 0xE000u)`
  then immediately calls `FUN_000107FE` to
  write the **entire 32-bit register**. Only
  incoming byte bits 7:5 affect field 15:13;
  lower five bits of the input byte are ignored
  by that masked update.
- `FUN_000120DC` also reads the same wrapper,
  clears bit **16** with `v & 0xFFFEFFFF`,
  and writes the resulting full DWORD.
- **New caller attribution:** `FUN_000120DC` is invoked
  by `FUN_00012D6A` and `FUN_00012F30` immediately
  before their respective existing data-transfer/
  acquisition-related command paths. Consequently the
  bit-16 clear is not an isolated setup-time action:
  normal transfer traffic can own another bit of the
  same full-DWORD GPIODAT register. This strengthens
  the need to serialize any future `CFDC2130` stream
  against ongoing acquisition. The exact physical pin
  meaning of bit 16 has not been independently proved.
- Original `FUN_00014847` constructs the
  `GPIODIR` wrapper at BAR1 `+0xC0` and
  `GPIODAT` at BAR1 `+0xC4`.
- Exact direct `+0x318` field-scan references:
  original `FUN_00011CFF`, `FUN_000120DC`,
  destructor `FUN_000134F6`, constructor
  `FUN_00014212` and device initializer
  `FUN_00014847`. Generic register-list
  writes can also select GPIODAT by pointer
  without using the literal `+0x318` offset,
  so field scans alone are not exhaustive.

The serial programming loop contains no explicit
delay instruction between its MMIO writes. Its
**ordered, per-byte edge sequence is the operation**:
do not simplify a stream to its final value.
The non-masked bits come from one initial read
and are reused through the loop. If another
writer changes bit 16 or other GPIO bits
concurrently, a later full-DWORD stream write
could overwrite that change. Do not assume an
interlock without further evidence. A careful
native implementation needs defined GPIO
ownership/serialization and safe FPGA-state
handling, not an arbitrary live test payload.

`FUN_00016C92`, by contrast, writes the
register-wrapper pointer at another object's
`+0x0C` and is not independently proved to
target BAR1 GPIODAT. `FUN_000179E2` performs
separate cached-word/IO writes. It would be
incorrect to label every caller of
`FUN_000107FE` a GPIODAT writer.

## New original-runtime evidence from broad normal UI exercise

The owner's private original-x86 trace
`legacy_xstream_trace_setregister.jsonl` was captured while deliberately
changing channel enable state, all channels to 20 mV/div, coupling,
bandwidth, timebase, sample rate, four-to-two-channel 10-GS/s mode, and
multiple trigger source/slope/type/Width-condition settings.

Across **184,862 valid native IOCTL records**, neither
`0x0022303C` nor `0xCFDC2130` occurs at all. The main original
acquisition interface carries **182,588 native IOCTL calls, all with
NTSTATUS success**. This is meaningful negative evidence: the two hazardous
writers are not part of the exercised ordinary UI path. It is not proof
that service/developer/rare modes never use them.

The same trace returns a complete original `0x00223040` list of
**11,438 bytes (43 x 266)**. Parsed public metadata for all 43 records
(name, BAR, physical offset, type) matches this document's recovered
order and the native `g_LecLegacyRegisterList[43]` **43/43**.
The raw trace stays private because it also contains device-specific
Dallas/license and identifier data. See [runtime trace notes](runtime-trace.md).

## Test status and decision

- Real native `CFDC2400` zero-mask ABI success reported.
- Grouped safe ABI suite `9/9 passed; 0 failed`
  on actual scope, with only the original
  PowerShell 5.1 expected-stderr presentation
  issue. Harness correction has not itself
  been separately rerun.
- Original top-level native representation
  remains `24/27` (including one gated);
  missing: `0x0022303C` indexed hardware write,
  `0x00223088` licensed Dallas WRITE,
  `0xCFDC2130` serial FPGA/GPIO programming.
- No driver source changed during the second or
  third static Ghidra batches. The complete
  post-CFDC2400 XStream/AP015 practical regression
  is postponed to one combined milestone at the
  owner's explicit request.

### Source evidence

- `ghidra_exports/selected/00013434_FUN_00013434.c`;
  `00014212_FUN_00014212.c`
- `ghidra_exports/selected/asm_asm_14847.txt`;
  `asm_asm_159e2.txt`; `asm_asm_1785b.txt`
- `ghidra_exports/selected/00013fa6_FUN_00013fa6.c`;
  `asm_asm_13fa6.txt`
- `ghidra_exports/selected/00013f3c_FUN_00013f3c.c`;
  `00013f70_FUN_00013f70.c`
- `ghidra_exports/selected/0001326e_FUN_0001326e.c`;
  `00013230_FUN_00013230.c`
- `ghidra_exports/selected/00012184_FUN_00012184.c`;
  `asm_asm_12184.txt`; `000121fa_FUN_000121fa.c`
- `ghidra_exports/selected/00012386_FUN_00012386.c`;
  `000123b4_FUN_000123b4.c`;
  `000124c2_FUN_000124c2.c`
- `ghidra_exports/selected/00012cac_FUN_00012cac.c`;
  `0001259a_FUN_0001259a.c`;
  `000107fe_FUN_000107fe.c`
- `ghidra_exports/selected/00011cff_FUN_00011cff.c`;
  `asm_asm_11cff.txt`;
  `000120dc_FUN_000120dc.c`;
  `asm_asm_120dc.txt`;
  `field_0x318.refs.txt`
- Native comparator `driver/Ioctl.c`:
  `g_LecLegacyRegisterList`,
  `LecFillLegacyRegisterEntry`,
  `LecFillLegacyRegisterList`.

**Remaining uncertainties:** exact original XStream
call-site/use of `0x0022303C`, actual write
authorization policy, outcome when the second
array allocation fails, GPIO serial-protocol
edge/pin meanings and synchronization, and
licensed Dallas write safety. None is resolved
by fabricating a hardware write.
