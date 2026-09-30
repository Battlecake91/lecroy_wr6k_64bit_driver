# Original-driver reverse-engineering coverage

Snapshot: 2026-09-30. This is a status/accounting note, not a claim that a
specific proportion of the proprietary original C/C++ source is recovered.

## Separate metrics

| Metric | Current value | Interpretation |
|---|---:|---|
| Reference x86 `LecS65AcqDrv.sys` | 64,384 bytes (62.875 KiB) | **Whole PE file**: headers, executable code, imports, data, alignment, etc.; not 64,384 bytes of original source or instructions. The binary is not redistributed. |
| Original top-level DeviceControl dispatch values identified | 27/27 (100% of the identified dispatch inventory) | The entry-point map is recovered, not every reachable helper, branch, or nested command. |
| Native x64 top-level cases represented | 24/27 (88.9%) | 23 have varying degrees of functionality; one (`0xCFDD219F`) is deliberately gated. This is not a code-byte or fully-working-feature percentage. |
| Top-level original cases absent from native x64 switch | 3/27 | `0x0022303C`, `0x00223088`, `0xCFDC2130`; their original semantics have already been partially or substantially analyzed. |
| Explicit plain function-address entries in `ghidra_scripts/targets.txt` | 85 distinct requested addresses | Selection requests, **not** an independently audited count of successfully decompiled or completely understood original functions. Other ASM/XREF/field/pointer-table requests exist. |
| Original executable machine-code bytes fully understood and ported | **Not measured** | There is currently no defensible `N KiB / M KiB` statistic. The selected Ghidra C exports cannot be compared in file-byte size with the original PE. |

The above snapshot is based on the README reference-binary metadata,
`docs/ioctl-map.md`, and the unique plain-address entries in
`ghidra_scripts/targets.txt` at the snapshot date.

## Register-map reconstruction is a separate compatibility metric

The third owner Ghidra batch `00eb49d` establishes
the original dynamically managed two-array register-list
ABI and the intended full initialization order of
**43 registers**, independently consistent with the
already present native `g_LecLegacyRegisterList[43]`.
See [original register-list and write ABI](
original-register-list-and-write-abi.md).
This proves the **observed 43-entry register-map ordering**,
not that 43 independent driver functions or a measured
proportion of original executable/source bytes has been
recovered. The two remaining hardware-writing original
top-level IOCTLs are intentionally not ported on that
basis. This distinction matters particularly because
the original list-query record encodes physical offset
where the corresponding setter interprets an incoming
DWORD as an unchecked table index.

## For a genuine code-byte percentage

A future read-only Ghidra coverage report should first determine all
executable original section/address ranges, excluding PE headers, data,
imports and padding. Record disjoint byte ranges of functions already
reviewed and semantically understood, distinguishing merely auto-decompiled
functions from manually validated ones. Deduplicate shared helpers and
thunks; treat unresolved indirect-call targets and code embedded in
undiscovered blocks separately. Then calculate:

```text
Reviewed executable bytes / total original executable bytes * 100
```

Track native-port parity and real-scope regression separately; understanding
a handler does not establish that its entire behaviour is safely implemented
or tested. Do not present 24/27 = 88.9% as '89% of original source recovered'.


## Semantic naming audit of selected decompilations

A 2026-09-30 audit of `ghidra_exports/selected/` found **207 distinct
Ghidra C function exports** (`<address>_FUN_<address>.c`). This is a
selected-analysis corpus, not the complete function count of the original
binary.

Cross-checking those 207 addresses against the repository README, AGENTS and
all Markdown files under `docs/` gives:

| Naming/context metric | Count | Meaning |
|---|---:|---|
| Selected decompiled functions | 207 | C exports currently retained for focused review. |
| Already referenced in project documentation | 180 | At least some project-specific context exists. A documentation mention alone does **not** imply complete semantic recovery. |
| Previously absent from all project documentation | 27 | Decompiled code existed, but the address had no project-documentation context. |
| Of those 27, directly reviewable to a useful structural/functional role | 17 | Examples include queued-IRP cancellation, `CKeTraceControl` / `CKeRegisterList` destruction, transfer-buffer cursor helpers, conditional register-write helpers, cleanup/destruction, flag clearing and append helpers. |
| Remaining hard semantic-unknown bucket after direct review | **10** | Machine-level behaviour is visible, but a vendor/domain-level name would currently overstate what is known. |

The **10 hard-unknown / domain-ambiguous functions** are currently:

| Original VA | What is known | Why no semantic rename yet |
|---:|---|---|
| `0x10E3A` | obtains current process, updates an object at main+0x1078, invokes request/completion helper and increments a global on one result | exact lifecycle/event purpose is unresolved |
| `0x11AA6` | stores four caller DWORDs at object offsets +0x166..+0x172 | field meanings are unresolved |
| `0x120FA` | initializes/allocates an array of N records, each exactly 0x10A bytes; called from `CKeRegisterList` construction | likely register-record storage, but exact class/member contract should be confirmed before a vendor-style name |
| `0x137C4` | performs a hardware quiesce/reset-like sequence, clears/masks MMIO state and changes global gating state | exact lifecycle stage is unresolved |
| `0x1381E` | performs a hardware start/reinitialize-like sequence including register/state reset and optional follow-up setup | exact lifecycle stage is unresolved |
| `0x138D4` | saves one global state value, masks a hardware register and synchronizes a callback | exact suspend/stop/error role is unresolved |
| `0x1551A` | initializes one DWORD value and two one-byte flags in a small helper object | object semantics are unresolved |
| `0x159BC` | resets a subobject and stores two parameters; called during the large hardware-subobject constructor | subobject purpose is unresolved |
| `0x16C6A` | stores one DWORD at object offset +0x0C | field meaning is unresolved |
| `0x17A0A` | stores one DWORD at object offset +4 and returns a value with low byte forced to 1 | object/field semantics are unresolved |

This leaves only **10/207 = 4.8%** of the selected decompilation corpus in
the strict category “we can see what the instructions do, but cannot yet give
the function an honest domain-level name.” Conversely, this must **not** be
reported as “95.2% of the original driver is fully understood”: many of the
other 197 functions are only partially understood, structurally named, or
known in one observed call path.

For future cleanup, prefer a separate alias/symbol map with confidence
(`confirmed`, `functional`, `tentative`) over renaming raw evidence files.
Raw `FUN_<address>` filenames should remain stable so documentation can always
trace conclusions back to the original VA.
