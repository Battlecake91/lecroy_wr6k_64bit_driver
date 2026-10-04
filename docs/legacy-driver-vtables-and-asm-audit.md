# Legacy virtual dispatch and raw-assembly audit (2026-10-04)

This note audits the focused Ghidra batch committed as
`d567bb0bc1210778cf8af8b4853614e9d82ae6d1`, using raw x86 instructions
as the authoritative source. It complements
[legacy-driver-architecture.md](legacy-driver-architecture.md) and
[legacy-driver-function-map.md](legacy-driver-function-map.md).

The pass resolves both *control-flow* warnings from the 420-function snapshot
but does not establish that Ghidra has identified every executable thunk or
every indirect virtual target in the binary. No native driver source or real
hardware was changed/tested in this pass.

## Important table-boundary caveat

The exporter directive `dwords:ADDRESS:COUNT` emits a fixed count of DWORDs.
It does **not** detect C++ vtable boundaries. Several 64-DWORD results extend
directly into ASCII messages, other adjacent vtables or GUID data. **Do not
interpret all returned DWORDs as virtual function slots.**

The following are confirmed from direct constructor/vtable assignments and
pointer values; the exact parent-class C++ names remain partially inferred.

## Main LeCroy device vtable: 0x1C500

Selected entries from `dwords_dwords_1c500_64.txt`:

| Offset | Target | Meaning |
|---|---|---|
| +0x04 | 0x11532 | main-device deleting destructor |
| +0x08 | 0x1A420 | generic DriverWorks IRP dispatcher |
| +0x0C | 0x10E3A | create dispatch path |
| +0x24 | 0x118E4 | IRP queue/forward helper |
| +0x5C | 0x10F30 | close path |
| +0x6C | 0x1B096 | power IRP dispatcher |
| +0x80 | 0x1A6EA | PnP IRP dispatcher |
| +0x84 | 0x11894 | guarded cancel/dispatch path |
| +0x88 | 0x115C4 | StartDevice / PCI resource initialization |
| +0x8C | 0x10D3C | interrupt/resource cleanup wrapper |
| +0x94 | 0x10D46 | hardware cleanup then resource cleanup |
| +0xE0 | 0x10D7A | this-adjusting hardware-subobject thunk |
| +0xF8 | 0x194BA | DriverWorks state/capability policy init |

Other entries include inherited framework/default handlers and short unrecognized
thunks; individual slot semantics require the relevant call site.

## Hardware base vtable: 0x1C8BC

Confirmed ten function slots (`+0x00` through `+0x24`):

| Offset | Target | Meaning |
|---|---|---|
| +0x00 | 0x12E18 | remove all transfer registrations for process |
| +0x04 | 0x170EE | acquisition/transfer helper, thunk to examine |
| +0x08 | 0x172A2 | unregister one transfer |
| +0x0C | 0x1731C | register a transfer |
| +0x10 | 0x14122 | deleting destructor |
| +0x14 | 0x13914 | board/helper virtual method, target pending |
| +0x18 | 0x13934 | board/helper virtual method, target pending |
| +0x1C | 0x104A0 | zero-return default |
| +0x20 | 0x104A0 | zero-return default |
| +0x24 | 0x104A0 | zero-return default |

`0x104A0` is directly shown as `XOR EAX,EAX; RET`, not hardware work.
The data after `+0x24` contains human-readable strings, **not** additional
entries in this table.

## Derived hardware vtable: 0x1C62C

The first four entries and `+0x14/+0x18` are inherited/shared with the base.
The changed entries are:

| Offset | Base target | Derived target | Status |
|---|---|---|---|
| +0x10 | 0x14122 | 0x10C8C | destructor/thunk, audit implementation |
| +0x1C | 0x104A0 | 0x10C62 | derived override, target pending |
| +0x20 | 0x104A0 | 0x10C34 | derived override, target pending |
| +0x24 | 0x104A0 | 0x114F2 | DPC forwarding thunk, verified |

The `+0x24` thunk is directly proven:

```asm
000114f2 PUSH [ESP+8]
000114f6 ADD  ECX,0xFFFFFE20    ; ECX -= 0x1E0
000114fc PUSH [ESP+8]
00011500 PUSH ECX
00011501 CALL 0x11390          ; existing DPC core
00011506 RET  0x8
```

This independently corroborates the original `CFDC2400` derived-object
dispatch and synchronous reuse of the original DPC core. The derived object's
`+0x20` implementation (`0x10C34`) is **not** yet semantically recovered
merely by identifying its address.

## DriverWorks driver-object area: 0x1C4C4 / 0x1C4D8

The fixed-length dump exposes short contiguous virtual tables. The base-like
view beginning at `0x1C4C4` includes `0x10582`, `0x104A0`,
`0x1D3D0`, `0x10490`. A second view beginning at `0x1C4D8`
contains `0x1DE3C`, `0x104F4` and `0x10636` (device creation and
interrupt synchronization). Neither should be treated as a single 16-entry
vtable without reference to the constructors and virtual call sites.

The same caution applies to the contiguous tiny helper vtables at
`0x1C990`, `0x1C994`, `0x1C9A4`, `0x1C9A8` and to
`0x1CBB4`, after whose four no-op callbacks GUID data follows.

## ASM warning 1: 0x11894 (resolved)

`FUN_00011894` first invokes `FUN_0001BC0E` to check the eligible/cancel
state of the IRP. It obtains the IRP's major code from
`IRP->Tail.Overlay.CurrentStackLocation->MajorFunction` and selects:

- **Major 0x0E** (`IRP_MJ_DEVICE_CONTROL`): set
  `IoStatus.Information=0` and
  `IoStatus.Status=0xC0000010` (`STATUS_INVALID_DEVICE_REQUEST`),
  then **tail-jump** to the hardware object's virtual `+0x20`.
- **All other majors:** push the IRP and use an ordinary `CALL` to the
  same hardware vtable slot `+0x20`, followed by `RET 4`.

The tail jump is intentional; after `POP ESI` the caller's original IRP
argument is still in its correct stack position. Ghidra's
`Could not recover jumptable` and `Treating indirect jump as call`
warnings are false indications of missing switch logic in this case.
The exact derived implementation at `0x10C34` remains an independent
semantic audit target.

## ASM warning 2: 0x19BB8 (resolved)

```asm
00019bb8 MOV EAX,ECX
00019bba MOV DL,[EAX+0x10]       ; saved old KIRQL
00019bbd LEA ECX,[EAX+0x0C]     ; KSPIN_LOCK
00019bc0 JMP [0x1C308]         ; KfReleaseSpinLock import
```

This is a deliberate **tail jump into `KfReleaseSpinLock`**. There is no
jumptable and no missing ordinary return path.

## Other non-function callback/thunk observations

The raw windows exported around `0x10D68` and `0x10D74` show that these
requested addresses are the *jump instructions*, not the true starts of their
enclosing thunks:

```asm
00010d62 ADD ECX,0x1E0
00010d68 JMP 0x138B2     ; hardware resume/start wrapper

00010d6e ADD ECX,0x1E0
00010d74 JMP 0x138D4     ; hardware quiesce/stop wrapper

00010d7a ADD ECX,0x1E0
00010d80 JMP 0x11B70
```

Thus the starts to mark/create as functions are `0x10D62`,
`0x10D6E` and `0x10D7A`, rather than only the jump instructions.
More small executable regions can exist beyond the 420 recognized functions.

The external cancel callback at `0x1150A` reads the device extension through
the x86 `DEVICE_OBJECT+0x28` field and calls `FUN_00010DA0`, using two
incoming stack arguments and `RET 8`. The raw window also directly
reconfirms the `0x114F2` DPC thunk.

## SEH-sensitive transfer-function audit

### `FUN_0001807A` (build/probe/lock MDL chain)

Raw instructions confirm:

- input byte length must be <= `0x06000000` (96 MiB), or
  `STATUS_INVALID_PARAMETER` (`0xC000000D`);
- each `IoAllocateMdl` chunk is capped at `0x02000000` (32 MiB);
- `ProbeForWrite(buffer,length,4)` runs only on the selected
  user-write/lock path;
- optionally `MmProbeAndLockPages` for each new MDL;
- `STATUS_INSUFFICIENT_RESOURCES` (`0xC000009A`) when MDL allocation
  fails;
- `__SEH_prolog`/`__SEH_epilog` protect probing, not ordinary
  C++-style object construction.

### `FUN_00017FD6` (transfer resource destruction)

The raw sequence confirms cleanup of:

1. the fixed descriptor-buffer MDL (`+0x14`, `IoFreeMdl`);
2. the fixed descriptor buffer (`+0x10`, `ExFreePool`);
3. the user MDL chain (`+0x08`, `FUN_00017F60`);
4. the transfer object itself and caller's pointer reset.

`FUN_0001C0A8` is the x86 SEH prolog (manipulates `FS:[0]` and saved
registers), not a LeCroy-specific operation.

The x64 port must preserve resource ownership and balanced unlocking, not
mechanically translate x86 SEH-frame setup.

## Next verification work

- Analyze **unrecognized** derived virtual targets `0x10C34`,
  `0x10C62`, `0x10C8C`, `0x170EE`, `0x13914`, `0x13934`.
- Recover the complete main-device vtable tail at `0x1C600` before the
  next table at `0x1C62C`.
- Recover the actual function starts/ends of the x86 this-adjusting thunks
  from `0x10C18` through `0x10D90`.
- Continue class/vtable coverage audits. **420/420 classified means coverage
  of Ghidra-recognized entries, not full binary code coverage.**

## Follow-up: unrecognized virtual functions (2026-10-04)

Focused Ghidra pass `9b6d3ccad7865a79efccd6343ae9fe697c1e6ec8`
verified virtual targets that were executable but absent as independent
functions from the original 420-entry inventory. Raw-window results are
authoritative at the shown instruction boundaries, not automatic decompiler
source.

### Hardware virtual method table

Derived hardware table `0x1C62C` overrides **four** entries relative to
base `0x1C8BC`, not just the three normal callback methods: deleting
destructor at `+0x10`, plus `+0x1C`, `+0x20`, `+0x24`.

| Derived slot | Function | ASM-verified behavior |
|---|---|---|
| `+0x10` | `0x10C8C` | `ECX -= 0x1E0`; jump to device deleting destructor `0x11532` |
| `+0x14` | `0x13914` | set global `DAT_0001CE18` bit 0; call synchronization object with `FUN_00012EAE` |
| `+0x18` | `0x13934` | clear global bit 0; perform the same synchronized callback |
| `+0x1C` | `0x10C62` | return pointer at `this+0x12D5` (default/type 0), `+0x12F5` (type 1) or `+0x1315` (type 2) |
| `+0x20` | `0x10C34` | adjust `this -= 0x1E0`, call `IoStartNextPacket(device+4, TRUE)`; if IRP nonnull, complete IRP and decrement outstanding-I/O through `0x1955A` |
| `+0x24` | `0x114F2` | adjust `this -= 0x1E0`; call `0x11390` DPC dispatcher |

The `+0x14/+0x18` implementations are **shared** with base hardware
rather than derived overrides. They directly explain how global
`DAT_0001CE18` bit 0 is toggled before synchronizing the device.
The `+0x1C` object pointers are spaced by 0x20 bytes. They are
plausibly event or control helper records, but their concrete field/class
types require caller/layout corroboration; their addresses are proven.

### Main-device vtable tail and non-inventory thunks

The fixed-length dump from `0x1C600` confirms additional entries **up to
`0x1C628`**; the next table starts at `0x1C62C`.

| Actual slot in `0x1C500` | Target | Inferred role |
|---|---|---|
| `+0x100` | `0x10D06` | `PoStartNextPowerIrp` and forward power IRP to lower device |
| `+0x104` | `0x1B00E` | framework power-related path; further audit |
| `+0x108` | `0x1A1D4` | queue drain/cancellation |
| `+0x10C` | `0x19614` | outstanding-I/O wait helper |
| `+0x110` | `0x19662` | outstanding-I/O wait helper |
| `+0x114` | `0x10D86` | `this += 0x1E0` and no-op helper `0x10D9A` |
| `+0x118` | `0x10D9A` | no-op, returns zero |
| `+0x11C` | `0x10D62` | `this += 0x1E0`, jump to hardware resume `0x138B2` |
| `+0x120` | `0x10D6E` | `this += 0x1E0`, jump to hardware quiesce `0x138D4` |
| `+0x124` | `0x197D0` | access cached power-state array/fallback |
| `+0x128` | `0x19734` | initialize power-policy state |

These offsets are relative to the **main-device vtable** at `0x1C500`.
The `0x1C62C` DWORD is already the first slot of a *different* vtable.
Other confirmed non-inventory thunks:

- `0x10C18`: complete an IRP with `STATUS_NOT_IMPLEMENTED`.
- `0x10CEA`: advance IRP stack pointer and pass IRP to lower device.
- `0x10D3C`: resource cleanup through `0x1082E`.
- `0x10D7A`: adjust `this += 0x1E0` then delegate to `0x11B70`.
- `0x170EE`: adjust `this += 0x100` and jump to `0x1829A`,
  which frees an entire transfer list.

Additional `0x19A54..` through `0x19B7A` short virtual targets
perform DriverWorks PnP/power policy-flag checks (primarily offsets
`+0xFC`, `+0x100`, `+0x138`) before tail-dispatching to further
virtual handlers; the method-level Windows minor-code mapping is a further
cross-reference task.

The 420-function Ghidra census **under-counts executable call targets**.
Future census improvements should enumerate executable addresses referenced
by virtual tables/dispatch tables, distinguish import thunks from actual
internal functions, and identify short `this`-adjuster regions.

### Auxiliary virtual timer/destructor thunks

- `0x1BC86`: adjust `this+0x18` and invoke `KeSetTimer`.
- `0x1BC9E`: adjust `this+0x18` and invoke `KeSetTimerEx`.
- `0x16C74`: 1-Wire object deleting-destructor wrapper, optionally `ExFreePool`.
- `0x170D0`: acquisition helper deleting-destructor wrapper, optionally
  `ExFreePool`.
- `0x19D38`: null-tolerant virtual deleting-destructor dispatcher.
- `0x1DE3C`: zero state at `this+0x18` and return success.
- `0x19B7A`: unconditional virtual tail-forward to slot `+0xFC`.

The `0x19A54`, `0x19A94`, `0x19AB6`, `0x19ACC`,
`0x19AEC`, `0x19B02`, `0x19B22`, `0x19B38` and
`0x19B4E` gate further virtual callbacks based on policy bits in
`this+0xFC/+0x100/+0x138`; individual WDM minor-code identities are
not yet attributed.

### Open detailed verification

- Confirm which public/callback operations select the three pointers
  returned from `0x10C62`.
- Trace the main `0x1B00E` power callback and the `0x19A54..`
  policy-gated forwarders to their owning PnP/power minor codes.
- Re-inventory actual executable function starts not recovered by
  Ghidra, particularly the short non-inventory thunks.
- Do not claim complete binary reverse engineering from the
  **420/420 recognized-function semantic classification** alone.

