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
