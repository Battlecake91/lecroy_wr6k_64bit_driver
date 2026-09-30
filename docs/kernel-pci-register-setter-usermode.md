# KernelPCIRegisters user-mode SetOneRegister caller

Evidence date: 2026-10-01.

This note closes the main ambiguity around original IOCTL
`0x0022303C`: the original XStream Developer/Service
`KernelPCIRegisters` page is the user-mode producer, and its request format
intentionally differs from the `0x00223040` register-query record.

The private vendor DLL and raw trace are **not committed**.

## Runtime proof

Owner action on original 32-bit XStream:

```text
Service -> Development -> AladdinAcqBoard -> KernelPCIRegisters
```

After selecting a register and writing the **same value already displayed**,
the trace contains the first observed real runtime call:

```text
IOCTL       0x0022303C
input       266 bytes (0x10A)
output      0 bytes
NTSTATUS    0x00000000
Information 0
```

Immediately before it, XStream queried register index 2 via `0x00223040`
and received the 266-byte record for `TxCount`. That query record reports
BAR1, physical offset `0x408`, metadata type 4 and current value 2.

The subsequent setter payload is almost completely zero:

```text
record +0x000 .. +0x0FF = all zero (no name)
record +0x100           = 0
record +0x101 DWORD     = 2
record +0x105           = 0
record +0x106 DWORD     = 2
```

So the live user-mode contract is now proved:

- incoming DWORD at `+0x101` = **zero-based register-list index**;
- incoming DWORD at `+0x106` = **new register value**;
- name, BAR and type fields are not required by this caller and are zeroed.

For this capture: index 2 = `TxCount`, value 2 = unchanged write.

This conclusively resolves the previously suspicious difference between
query and setter semantics. A `0x00223040` query record places the
**physical BAR offset** at `+0x101`; a `0x0022303C` setter request
places the **register-list index** at the same byte position. XStream does
not echo the query record. It builds a fresh zero-filled setter record.

## User-mode DLL proof

The only EXE/DLL hit in the owner's installed-XStream binary scan for
little-endian `3C 30 22 00` was:

```text
C:\Program Files\LeCroy\XStream\lecaladdinhwaccesspcisvr.dll
file offset 0x0002001C
size        336472 bytes
SHA-256     4bdfcbe57fb76f40ca5d77f729e1a6662aa3b5b4e3dd2e12a7f13f84ecfc4f86
PE          32-bit x86 DLL
timestamp   2017-06-23 14:05:50
```

The DLL itself is private vendor material and must not be added to the
public repository.

The file offset maps into `.text` to VA `0x10020C1C`. The surrounding
function begins at approximately `0x10020B7C` and contains:

```asm
; allocate local 0x10A request and zero it
mov  ebx, 0x10A
...
call memset

; selected register enum -> request +0x101
mov  eax, [esi+0x310]
...
call dword ptr [ecx+0xD4]
mov  eax, [ebp-0x114]
mov  [ebp-0x0F], eax       ; buffer base ebp-0x110 + 0x101

; requested register value -> request +0x106
mov  eax, [esi+0x314]
lea  edx, [ebp-0x0A]       ; buffer base +0x106
...
call dword ptr [ecx+0xCC]

; DeviceIoControl-style helper
push 0x10A
lea  eax, [ebp-0x110]
push eax
push 0x0022303C
...
call 0x1001D8B8
```

The DLL contains nearby diagnostic/member strings:

```text
m_cvEnumRegisterList
m_cvRegPCIRegister
```

and the field positions are consistent with the two adjacent objects used
above. The setter function is reached from the notification dispatcher at
approximately `0x1001AA5A`. That caller compares the notification source
against the object corresponding to the register-value cvar and requires a
notification flag containing `0x100` before calling the setter. This fits
the observed GUI behavior: editing/submitting the KernelPCIRegisters value
field invokes SetOneRegister.

## Kernel-side behavior already recovered

Original kernel:

```text
0x0022303C -> FUN_00012CAC -> FUN_0001259A -> FUN_000107FE
```

- exact input size: `0x10A`;
- hardware/list ready gate: `DAT_0001CD08 == 0`;
- `+0x101` is used directly as pointer-table index;
- `+0x106` is written as the new DWORD value;
- physical write is `WRITE_REGISTER_ULONG`;
- success returns `Information = 0`;
- no local bounds check is visible before `pointerTable[index]`.

The new runtime and DLL evidence proves this is not a Ghidra artifact. The
user-mode producer intentionally supplies an index rather than a BAR offset.

## Compatibility decision

The IOCTL is a real Developer/Service feature, but it is not required by
ordinary scope operation. Earlier broad normal-UI and read-only
Developer/Service traces never called it. The first runtime call appeared
only after the owner deliberately wrote a value in
`KernelPCIRegisters`.

A future x64 implementation can now reproduce the original **valid**
user-mode ABI without guessing, but it should improve safety instead of
copying the unchecked original:

1. exact 266-byte METHOD_BUFFERED input;
2. accept only indices `0..42` for the current known 43-entry map;
3. resolve index to the native known register descriptor;
4. reject unknown/unmapped/out-of-range indices before any MMIO access;
5. preserve current readiness/state requirements;
6. decide whether all 43 registers should be writable or whether an
   explicit write-allow policy is safer;
7. do not use the query record's physical-offset field as a setter index.

No need to intentionally test a different hardware value on the only
working scope merely to prove the ABI. The unchanged-value write already
proves the complete user-mode request construction and successful original
kernel dispatch.

## Immediate next work

- Integrate this proof into the IOCTL map and handoff.
- Decide/design a hardened native `0x0022303C` implementation.
- Keep `CFDC2130` and licensed Dallas WRITE `0x00223088` separate;
  they remain unresolved/high-risk write paths.
- Do not commit the vendor DLL or private raw trace.
