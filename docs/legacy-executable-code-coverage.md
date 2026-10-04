# Executable code coverage audit (2026-10-04)

This is a census of **decoded executable instructions not owned by a recognized Ghidra function**, not a claim of full binary code coverage.

Evidence: full `coverage` export from commit
`cabdc5f9f882044bb3928545278024b1aad5ac9d`,
`ghidra_exports/selected/CODE_COVERAGE.txt` and
`UNOWNED_CODE_REFS.txt`. Follow-up address/ASM evidence is collected in
[legacy-driver-vtables-and-asm-audit.md](legacy-driver-vtables-and-asm-audit.md).

## Quantitative baseline

| Measurement | Result |
|---|---:|
| Internal Ghidra functions in `FUNCTION_INVENTORY.txt` | 420 |
| All Ghidra function-manager entries including external functions | 507 |
| External function-manager entries (difference) | 87 |
| Executable memory-block bytes | 54,528 |
| Decoded instructions inside recognized function bodies | 44,828 bytes / 15,292 instructions |
| Decoded instructions outside recognized function bodies | 3,207 bytes / 974 instructions |
| Distinct contiguous unowned instruction clusters | 90 |
| Executable bytes not counted as decoded instructions | 6,493 |

**The 6,493-byte remainder is not automatically executable code.** It may
include literal data, tables, padding, undefined bytes or undiscovered code.
The baseline exporter only measures decoded instructions, not whether the
remainder is actually data. The next improved `coverage` pass also writes
`EXECUTABLE_BYTE_CLASSIFICATION.txt`.

The total of 507 is not an increase from 420 internal functions: the former
includes 87 external/imported function-manager entries. This reconciles the
apparently inconsistent numbers in earlier documentation.

## Prioritized findings

- **`0x11018..0x1138F` (888 bytes, 236 instructions):** a large
  *functionless* code region referenced directly by the LeCroy device vtable
  at `0x1C54C`, i.e. slot `+0x4C` relative to `0x1C500`.
  A device-control/IRP dispatch role is likely from placement and
  surrounding handlers, but requires the actual raw ASM before assigning a
  definitive semantic name. This is the **top priority**.
- **`0x10EAF..0x10F2E` (128 bytes):** another main-device vtable target
  (`0x1C518`). Requires entire-body ASM/classification.
- **`0x1B00E..0x1B095` (136 bytes):** vtable slot `+0x104`;
  DriverWorks power/PnP callback candidate. Must inspect instructions.
- **`0x1C118..0x1C1C5` (174 bytes):** referenced from the SEH prolog at
  `0x1C0A8`; likely compiler SEH machinery, but still needs verification.
- **`0x10406..0x104A2` (156 bytes across two clusters):**
  global DriverWorks dispatch/default callbacks, vtable entrypoints.
- **`0x1085E..0x108D4` (six 19-byte clusters):**
  individually referenced by ISR/DPC service routines, likely synchronous
  interrupt helpers.
- **`0x183EE..0x184BE` (numerous small clusters):**
  many individually referenced addresses, likely static/runtime initializer
  thunks. Keep these in the complete-binary audit.
- **`0x18067..0x18156` (multiple short fragments):**
  references from SEH scope/cleanup metadata near the transfer MDL
  helpers; they require exception-table rather than independent-function
  treatment where appropriate.
- Other substantially sized unowned regions include `0x19A1E`,
  `0x19A54`, `0x19A94`, `0x19AEC`, `0x19B22`,
  `0x19C44`, `0x19C80`, `0x1BE72`, `0x1C204`,
  `0x1C2D4`, `0x1D380` and `0x1D3C4`.

## Complete list of all 90 decoded unowned clusters

Cluster boundaries are *disassembler boundaries*, not automatically function
boundaries. A legitimate entry may be in the middle of a cluster. Incoming
references to every decoded orphan instruction are preserved separately in
`UNOWNED_CODE_REFS.txt`.

| Start | End | Bytes | Instructions | Start XREFs | Review |
|---|---|---:|---:|---:|---|
| `0x10406` | `0x1044A` | 69 | 21 | 1 | Audit |
| `0x1044C` | `0x104A2` | 87 | 28 | 10 | Audit |
| `0x1085E` | `0x10870` | 19 | 6 | 1 | Short thunk/fragment |
| `0x10872` | `0x10884` | 19 | 6 | 1 | Short thunk/fragment |
| `0x10886` | `0x10898` | 19 | 6 | 1 | Short thunk/fragment |
| `0x1089A` | `0x108AC` | 19 | 6 | 1 | Short thunk/fragment |
| `0x108AE` | `0x108C0` | 19 | 6 | 1 | Short thunk/fragment |
| `0x108C2` | `0x108D4` | 19 | 6 | 1 | Short thunk/fragment |
| `0x10B30` | `0x10B3B` | 12 | 3 | 1 | Short thunk/fragment |
| `0x10C18` | `0x10C32` | 27 | 6 | 79 | Short thunk/fragment |
| `0x10C34` | `0x10C60` | 45 | 14 | 1 | Audit |
| `0x10C62` | `0x10C8A` | 41 | 13 | 1 | Audit |
| `0x10C8C` | `0x10C96` | 11 | 2 | 1 | Short thunk/fragment |
| `0x10CEA` | `0x10D04` | 27 | 8 | 2 | Short thunk/fragment |
| `0x10D06` | `0x10D45` | 64 | 27 | 1 | Audit |
| `0x10D62` | `0x10D6C` | 11 | 2 | 1 | Short thunk/fragment |
| `0x10D6E` | `0x10D78` | 11 | 2 | 1 | Short thunk/fragment |
| `0x10D7A` | `0x10D84` | 11 | 2 | 1 | Short thunk/fragment |
| `0x10D86` | `0x10D99` | 20 | 5 | 1 | Short thunk/fragment |
| `0x10EAF` | `0x10F2E` | 128 | 38 | 1 | Priority 2 |
| `0x11018` | `0x1138F` | 888 | 236 | 1 | **Priority 1** |
| `0x114F2` | `0x11508` | 23 | 6 | 1 | Short thunk/fragment |
| `0x1150A` | `0x1151C` | 19 | 5 | 2 | Short thunk/fragment |
| `0x1151E` | `0x11531` | 20 | 5 | 1 | Short thunk/fragment |
| `0x118E4` | `0x11912` | 47 | 18 | 2 | Audit |
| `0x11A7A` | `0x11A87` | 14 | 6 | 1 | Short thunk/fragment |
| `0x12EC2` | `0x12EDC` | 27 | 8 | 1 | Short thunk/fragment |
| `0x13914` | `0x13932` | 31 | 12 | 2 | Audit |
| `0x13934` | `0x13952` | 31 | 12 | 2 | Audit |
| `0x16C74` | `0x16C90` | 29 | 10 | 1 | Short thunk/fragment |
| `0x170D0` | `0x170EC` | 29 | 10 | 1 | Short thunk/fragment |
| `0x170EE` | `0x170F8` | 11 | 2 | 3 | Short thunk/fragment |
| `0x18067` | `0x1806D` | 7 | 4 | 1 | SEH review |
| `0x180BD` | `0x180C0` | 4 | 3 | 1 | SEH review |
| `0x1814C` | `0x18156` | 11 | 5 | 1 | SEH review |
| `0x183EE` | `0x18408` | 27 | 10 | 1 | Short thunk/fragment |
| `0x1840A` | `0x1840E` | 5 | 2 | 1 | Initializer audit |
| `0x18410` | `0x18414` | 5 | 2 | 1 | Initializer audit |
| `0x18416` | `0x1841A` | 5 | 2 | 1 | Initializer audit |
| `0x1841C` | `0x18420` | 5 | 2 | 1 | Initializer audit |
| `0x18422` | `0x18426` | 5 | 2 | 1 | Initializer audit |
| `0x18428` | `0x18434` | 13 | 4 | 1 | Initializer audit |
| `0x18436` | `0x1843A` | 5 | 2 | 1 | Initializer audit |
| `0x1843C` | `0x18440` | 5 | 2 | 1 | Initializer audit |
| `0x18442` | `0x18446` | 5 | 2 | 1 | Initializer audit |
| `0x18448` | `0x1844C` | 5 | 2 | 1 | Initializer audit |
| `0x1844E` | `0x18452` | 5 | 2 | 1 | Initializer audit |
| `0x18454` | `0x18458` | 5 | 2 | 1 | Initializer audit |
| `0x1845A` | `0x1845E` | 5 | 2 | 1 | Initializer audit |
| `0x18460` | `0x18464` | 5 | 2 | 1 | Initializer audit |
| `0x18466` | `0x1846A` | 5 | 2 | 1 | Initializer audit |
| `0x1846C` | `0x18470` | 5 | 2 | 1 | Initializer audit |
| `0x18472` | `0x18476` | 5 | 2 | 1 | Initializer audit |
| `0x18478` | `0x1847C` | 5 | 2 | 1 | Initializer audit |
| `0x1847E` | `0x18482` | 5 | 2 | 1 | Initializer audit |
| `0x18484` | `0x18488` | 5 | 2 | 1 | Initializer audit |
| `0x1848A` | `0x1848E` | 5 | 2 | 1 | Initializer audit |
| `0x18490` | `0x18494` | 5 | 2 | 1 | Initializer audit |
| `0x18496` | `0x1849A` | 5 | 2 | 1 | Initializer audit |
| `0x1849C` | `0x184A0` | 5 | 2 | 1 | Initializer audit |
| `0x184A2` | `0x184A6` | 5 | 2 | 1 | Initializer audit |
| `0x184A8` | `0x184AC` | 5 | 2 | 1 | Initializer audit |
| `0x184AE` | `0x184B2` | 5 | 2 | 1 | Initializer audit |
| `0x184B4` | `0x184B8` | 5 | 2 | 1 | Initializer audit |
| `0x184BA` | `0x184BE` | 5 | 2 | 1 | Initializer audit |
| `0x184F6` | `0x184F8` | 3 | 1 | 2 | Initializer audit |
| `0x197D0` | `0x197EC` | 29 | 8 | 2 | Short thunk/fragment |
| `0x19A1E` | `0x19A52` | 53 | 14 | 1 | Audit |
| `0x19A54` | `0x19A72` | 31 | 8 | 2 | Audit |
| `0x19A74` | `0x19A92` | 31 | 8 | 1 | Audit |
| `0x19A94` | `0x19AEA` | 87 | 23 | 2 | Audit |
| `0x19AEC` | `0x19B20` | 53 | 14 | 2 | Audit |
| `0x19B22` | `0x19B81` | 96 | 26 | 2 | Priority 2 |
| `0x19C44` | `0x19C7E` | 59 | 16 | 1 | Audit |
| `0x19C80` | `0x19C9A` | 27 | 8 | 3 | Short thunk/fragment |
| `0x19D38` | `0x19D43` | 12 | 6 | 4 | Short thunk/fragment |
| `0x1A07E` | `0x1A098` | 27 | 8 | 1 | Short thunk/fragment |
| `0x1A09A` | `0x1A0B4` | 27 | 8 | 1 | Short thunk/fragment |
| `0x1A3FC` | `0x1A40E` | 19 | 5 | 1 | Short thunk/fragment |
| `0x1A410` | `0x1A41F` | 16 | 4 | 1 | Short thunk/fragment |
| `0x1B00E` | `0x1B095` | 136 | 42 | 2 | Priority 2 |
| `0x1BC86` | `0x1BC9C` | 23 | 7 | 2 | Short thunk/fragment |
| `0x1BC9E` | `0x1BCB8` | 27 | 8 | 2 | Short thunk/fragment |
| `0x1BE72` | `0x1BED0` | 95 | 37 | 2 | Priority 2 |
| `0x1C118` | `0x1C1C5` | 174 | 70 | 1 | SEH review |
| `0x1C204` | `0x1C225` | 34 | 9 | 1 | Audit |
| `0x1C2D4` | `0x1C2F1` | 30 | 5 | 1 | Audit |
| `0x1D380` | `0x1D3A3` | 36 | 9 | 2 | Audit |
| `0x1D3C4` | `0x1D3DB` | 24 | 8 | 1 | Short thunk/fragment |
| `0x1DE3C` | `0x1DE44` | 9 | 3 | 1 | Short thunk/fragment |

## Verification plan

1. Export raw instruction text for **every** orphan cluster, not only
   hand-picked address windows, in `UNOWNED_CODE_ASM.txt`.
2. Identify direct-call, vtable, exception-unwind, constructor/destructor
   and callback entrypoints in each cluster; do not merge unrelated functions.
3. Classify executable-block bytes as decoded instructions, defined data
   and undefined bytes. Manually audit any plausible code hidden in data or
   undefined regions.
4. Only after the code boundaries are proven, create/rename functions in a
   dedicated Ghidra analysis copy and export additional pseudocode.
5. Re-run the inventory to ensure no new function remains unclassified.

The next `targets.txt` uses `coverage` and `inventory` with the expanded
coverage exporter. Hardware is not involved in these steps.

## Full orphan-ASM analysis (2026-10-04)

Export commit `d5e08317e515ea46cb525b5628ea3e67de9c5e46`
added `UNOWNED_CODE_ASM.txt` for all 90 orphan decoded instruction clusters
and `EXECUTABLE_BYTE_CLASSIFICATION.txt` for executable section contents.

### Decoded bytes versus defined data

| Executable block | Size | Decoded instructions | Defined data | Undefined |
|---|---:|---:|---:|---:|
| `.text` | 49,024 | 45,483 | 2,615 | 926 |
| `PAGE` | 2,560 | 1,967 | 508 | 85 |
| `INIT` | 2,944 | 585 | 2,254 | 105 |
| **Total** | **54,528** | **48,035** | **5,377** | **1,116** |

Thus the previously unaccounted **6,493** bytes split into **5,377 bytes
of defined data** and **1,116 still-undefined bytes**. The latter require
separate examination for possible hidden code, padding, or tables. In
particular, executable permissions do not imply every byte holds instructions.

### Master IOCTL dispatcher: 0x11018..0x1138F (confirmed)

This is not an opaque control routine but the full LeCroy device-control
dispatcher, referenced by the main-device vtable `0x1C500+0x4C` at
`0x1C54C`. The raw instruction sequence establishes:

1. check device synchronization object at device `+0x1463`; absent state
   produces `STATUS_DEVICE_NOT_READY (0xC00000A3)`;
2. wait via an imported five-argument kernel synchronization function;
3. read the IOCTL from `IRP->Tail.Overlay.CurrentStackLocation` at
   stack location `+0x0C`;
4. dispatch to the matching handler using the embedded hardware object at
   `device+0x1E0`;
5. release the synchronization object;
6. complete the IRP through `0x10798`, **except** when the resulting status
   is `STATUS_PENDING (0x103)` (completion is deferred);
7. log errors and return `STATUS_INVALID_PARAMETER (0xC000000D)` for
   unknown IOCTL codes.

The switch covers all **27** original top-level IOCTL cases. Its exact
case-to-handler mapping is now reproduced in
[ioctl-map.md](ioctl-map.md). Dallas WRITE (`0x00223088`) and serial
FPGA programming (`0xCFDC2130`) are confirmed regular, reachable
handlers, not experimental or detached helper code.

The `0xCFDC2184` case is handled inline as success with
`IoStatus.Status=0` and `IoStatus.Information=0`; other branches call
dedicated handlers. `0xCFDC2110`, `0xCFDC2138`, and `0xCFDD219F`
collect the IRP status from their specialized frontends rather than
assuming a generic success result.

### Recovered semantic classes among the 90 clusters

The cluster number is **not** equal to the function count: many clusters
contain multiple independent routines or EH landing pads.

| Unowned code range or region | Static classification |
|---|---|
| `0x10406..0x1044A` | two global DriverWorks virtual dispatch/driver-state callbacks at `0x10406`, `0x1041C` |
| `0x1044C..0x104A2` | global DriverWorks IRP dispatcher/default trampoline (`0x1044C`), virtual forwarder (`0x10490`), zero-return default (`0x104A0`) |
| `0x1085E..0x108D4` | six **separate** DPC pending-bit conditional wrappers; gate `0x11DD8`, `0x11DC2`, `0x11DEE`, `0x11E04`, `0x11E1A`, `0x11E30` on a nonzero argument |
| `0x10B30` | short ISR trampoline to `0x108D6` |
| `0x10C18..0x10D99` | missing virtual/default callbacks, IRP completion and forwarding, embedded helper selectors, DPC callback, power forwarding, deleting-destructor and `this` adjustment thunks, already detailed in the [vtable audit](legacy-driver-vtables-and-asm-audit.md) |
| `0x10EAF..0x10F2E` | device close/cleanup-state path: decrements global `0x1CE0C` usage counter when nonzero; at zero clears hardware registers/shadows and completes an IRP; exact public lifecycle callback name needs calling-context audit |
| `0x11018..0x1138F` | full 27-case original device-control IOCTL dispatcher |
| `0x114F2..0x11531` | DPC forwarding and cancel/ISR callback adapters (`0x114F2`, `0x1150A`, `0x1151E`) |
| `0x118E4..0x11912` | IRP queuing/packet-start helper with zero-length completion path and cancel routine `0x1150A` |
| `0x11A7A` | zero DWORD at object start |
| `0x12EC2` | conditional ISR/DPC pending-latch helper updating globals `0x1CE10` from `0x1CE1C` |
| `0x13914`, `0x13934` | synchronized global interrupt-mask bit-0 set and clear |
| `0x16C74`, `0x170D0`, `0x170EE` | optional pool-free deleting destructors and transfer-list cleanup thunk |
| `0x18067`, `0x180BD`, `0x1814C` | x86 SEH exception handler/landing-pad fragments in MDL/transfer code; do not blindly promote each sub-fragment to a standalone function |
| `0x183EE` | DriverWorks table dispatch by `IO_STACK_LOCATION.MajorFunction`, using table at `0x1CD10` |
| `0x1840A..0x184BE` | **multiple** short virtual dispatch trampolines (`MOV EAX,[ECX]`, tail jump to vtable slot); these address clusters often contain more than one routine |
| `0x184F6` | trivial return/default handler |
| `0x197D0` | read cached system/device power value with fallback from `this+0x1A4` |
| `0x19A1E..0x19B81` | multiple PnP/power policy gating methods: examine bitfields `this+0xFC/+0x100/+0x138`, then tail-call virtual slot `+0xFC` or `+0x100` |
| `0x19C44..0x19C9A`, `0x1A07E..0x1A0B4` | short completion/callback adapters forwarding to power completion methods `0x197EE`, `0x196B4`, `0x199AA`, `0x19966`, `0x19D44`, `0x19D84` |
| `0x19D38` | null-tolerant virtual deleting-destructor call |
| `0x1A3FC..0x1A41F` | cancel/Power callback thunks toward `0x1A196` and `0x1A342` |
| `0x1B00E..0x1B095` | queued IRP cancellation/requeue helper, owns cancel-spinlock path, returns `STATUS_CANCELLED (0xC0000120)` or `STATUS_PENDING (0x103)`; invokes `0x19FE0` and installs cancellation callback at `0x1A3FC` |
| `0x1BC86..0x1BCB8` | two separate timer wrappers: `KeSetTimer`, `KeSetTimerEx` |
| `0x1BE72..0x1BED0` | DriverWorks synchronous/forward-completion support: optional callback invocation, `STATUS_MORE_PROCESSING_REQUIRED (0xC0000016)` path, completion-status copy and event signaling |
| `0x1C118..0x1C1C5` | x86 compiler exception-frame handler/unwind dispatcher; follows SEH scope tables and calls `0x1C1E4`/`0x1C226` |
| `0x1C204..0x1C225` | exception-filter flag handler, returns 1 or 3 depending on exception flags |
| `0x1C2D4..0x1C2F1` | two static initializer/teardown registration thunks |
| `0x1D380..0x1D3A3` | global DriverWorks singleton shutdown plus static destructor runner |
| `0x1D3C4..0x1D3DB` | object ownership/conditional destruction helpers, two short entrypoints |
| `0x1DE3C` | DriverWorks default reset of `this+0x18`, returns success |

The original 90 clusters can be fully categorized into these groups on
the available raw ASM. That is **semantic classification**, not yet
individually decompiled C or verified function-boundary coverage. Some
clusters contain multiple tiny callable functions. The next stage should
recover high-confidence function boundaries in Ghidra and export decompiled
C for each, while inspecting still-undefined executable bytes separately.


## First function-recovery pass prepared

The first explicit `recover:` pass has been added to
`ghidra_scripts/targets.txt`. It selects high-confidence independently
referenced starts (including `0x11018`) for Ghidra's function-creation
command, exports decompiled C/references, then refreshes
`FUNCTION_INVENTORY.txt` and `CODE_COVERAGE.txt`. The new
`RECOVER_<address>.txt` records distinguish successful creations from
rejections/failures.

This pass **mutates the local Ghidra analysis database**. The runner
automatically creates a complete project backup in a timestamped
directory outside Git before invoking headless Ghidra. Stop/close the
interactive Ghidra GUI before execution. Do not claim any recovered
functions until their generated records and pseudocode are reviewed.

Ghidra-recognized function counts should grow beyond 420 after successful
recovery; the existing 420-classification map must then be extended.
SEH fragments and ambiguous split boundaries remain manual review items.

The same future pass additionally writes
`UNDEFINED_EXECUTABLE_RANGES.txt`, recording contiguous undefined
ranges with 32-byte hex prefixes. These bytes have not yet been
inspected or disassembled, so **do not assume that all 1,116 undefined
bytes are padding or code**.
