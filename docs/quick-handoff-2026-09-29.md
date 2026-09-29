# LeCroy x64 driver: compact engineering handoff (2026-09-29)

This file describes the **latest actionable state**, not the historical investigation diary. Read [AGENTS.md](../AGENTS.md) first, then the [current handoff](next-chat-handoff.md), [TODO](TODO.md), and [README](../README.md). Repository and technical documentation are in English; converse with the owner in German.

**Public repository:** https://github.com/Battlecake91/lecroy_wr6k_64bit_driver (branch `main`).

## Latest actual scope result: read-only 0x00223044 PASS (2026-09-29 late)

The owner ran `tools/lecdiag/build/lecdiag.exe start-register` on the
real x64 scope and supplied this output (non-sensitive):

```text
Opened device interface: \\?\pci#ven_1570&dev_0005&subsys_00000000&rev_00#...#{7ac34be9-f766-4f15-9e88-854ba5e2146e}
0x00223044 START/FVER: 0x00000002
BAR0+0x000 reference: 0x00000002
PASS: legacy 4-byte output matches physical register read.
```

**Verified:** the newly implemented legacy 4-byte output request
`0x00223044` is accepted by the driver currently serving the real PCI
device. Its returned DWORD (`0x00000002`) equals the result of the
separate generic register-read IOCTL for physical BAR0+0x000,
also `0x00000002`. This is read-only, with no BAR write or Dallas access.
It verifies this request's live successful path and consistency with the
generic BAR0 read, not every edge case or a separate original-x86
side-by-side measurement. The user's current message does not include
the complete pure-build/signed-install log or the scope's Git HEAD;
do not infer those individual steps or claim a completed XStream
regression from the successful diagnostic alone.

**Prior blocked attempt, now superseded for this IOCTL:** the first
`scripts/build-sign-load-driver.ps1` invocation used a non-elevated
PowerShell and stopped at its administrator assertion, without installing
a driver. The subsequent diagnostic failed its first new IOCTL with
Win32 `ERROR_INVALID_FUNCTION` (1) before any reference comparison.
The new PASS establishes that this earlier error is not the current
result. The precise corrective installation commands used were not
included in the latest console excerpt. When running PowerShell scripts,
check immediate `$?` or use `try/catch`; a later `$LASTEXITCODE`
does not reliably catch a PowerShell `throw`.

### Follow-up XStream / AP015 regression: owner reports PASS

After `start-register` passed, the owner was asked to check genuine
waveforms, ordinary vertical/timebase/coupling/bandwidth/trigger
operations, 2-channel/10-GS/s where practical, preconnected AP015,
physical removal/reinsertion and the known open-jaw warning.
Reply: `Ja klappt soweit alles.` Record no observed regression in
the exercised existing baseline, not an automated exhaustive
per-feature acceptance matrix. The immediate `0x00223044`
hardware + XStream check is now complete.

### 2026-09-30 source advance (not a completed Ghidra rerun)

Original `FUN_000115C4` initializes the hardware subobject at
`main+0x1E0`. The START/FVER register member `this+0x138`
is initialized/read on that subobject by `FUN_00014847` and
`FUN_00012D24`. Thus the `CFDC2194` latch
`this+0x116A` could also be accessed as `main+0x134A`
if the same original dispatcher receiver is used. Its four
bytes span `116A..116D` or `134A..134D`. Added
dispatch-wrapper, overlapping field and callback export
targets in `ghidra_scripts/targets.txt` to search both
representations. Exact scans do not prove the absence of
indirect writers. Full analysis:
[CFDC2194 provenance](cfdc2194-status-latch-investigation.md).
The Ghidra PC rerun is still required; no kernel patch or
new hardware test performed.

### Next task: map original CFDC2194 latch producer

`FUN_00012BAE` returns exactly 29 bytes, including the saved
status DWORD from `this+0x116A` at +8, then clears the latch.
The direct field scan only found that read-and-clear site.
Paired original `FUN_00013A40` (CFDC2190) calls
`FUN_000107FE` on `this+0x188` and registers callback
`FUN_00012EAE` which can call `FUN_00011E46` for a
global register write. This adjacent path **does not prove**
the origin of `this+0x116A`. Inspect aliases/indirect writes,
callback/IRQ and initialization before any kernel implementation.
A constant-zero successful response is not a substitute.

Do not disturb working PCI/IRQ/DMA/AP015, or resume deferred
virtual Dallas, hardware isolation or licensed-chip writing.

## Source facts just recovered from the user's Ghidra push

New original x86 selected exports:

- `ghidra_exports/selected/asm_asm_12d24.txt`
- `ghidra_exports/selected/asm_asm_12bae.txt`
- `ghidra_exports/selected/field_0x138.refs.txt`
- `ghidra_exports/selected/field_0x116a.refs.txt`
- Already available corresponding C:
  `00012d24_FUN_00012d24.c`,
  `00012bae_FUN_00012bae.c`,
  and initializer `00014847_FUN_00014847.c`.

**Newly implemented x64 `0x00223044`:**
original `FUN_00012D24` checks only exact output
length four, calls `READ_REGISTER_ULONG` on
the register pointer held in main object `+0x138`,
returns a DWORD with `Information=4`.
Original `FUN_00014847` initializes that pointer to
`BAR0 base + 0x000`, the START/FVER register location.
Replacement code now defines
`LECS65_IOCTL_READ_START_REGISTER` in
`driver/LecS65Drv.h`, dispatches it in
`driver/Ioctl.c`, resolves BAR0 offset zero,
performs `READ_REGISTER_ULONG`, returns the
actual DWORD. It is a read-only change.

**`0xCFDC2194` is deliberately NOT implemented:**
original `FUN_00012BAE` requires exact 29-byte
output; zero-fills that buffer, places DWORD `2`
at response offset 4, copies the software status
latch at original `this+0x116A` to offset 8,
then clears the original latch. The new displacement
scan `field_0x116a.refs.txt` finds only
the handler's own `LEA [ESI+0x116A]` read/clear
access, not an identified writer. An indirect or
aliased writer is possible. Map where/when the
status latch becomes nonzero before implementing
`CFDC2194`; **never fabricate a permanently zero
success response** to inflate coverage.

Recent relevant source additions:
- `driver/LecS65Drv.h`: IOCTL definition `0x00223044`.
- `driver/Ioctl.c`: read-only dispatcher case.
- `tools/lecdiag/lecdiag.c`: `start-register` command and equality test.
- `docs/ioctl-map.md`: original reference and pending build/hardware validation.
- `docs/TODO.md`: remaining work.

The assistant did not build or run Windows binaries in its own environment.
The owner has now supplied the successful real-scope `start-register`
read comparison above. Only the XStream/AP015 post-change regression
remains unreported; do not treat it as passed.

## Stable verified baseline / boundaries

- Native x64 driver currently starts XStream and displays genuine
  waveforms on the real WaveRunner scope.
- Observed one-channel acquisition and family-1 transfer paths,
  DMA descriptor handling, legacy interrupts/DPC and event delivery
  have been hardware-tested. Do not casually change them.
- Normal timebase, vertical scale, coupling, bandwidth,
  trigger, two-channel/10-GS/s selection were tested.
- The AP015 identifies correctly, physical hotplug works and
  XStream displays the unlocked-jaw warning. All five front
  physical ProBus sockets are I2C for probe communications;
  PCI-board Dallas 1-Wire EEPROM is a separate device.
- U11 DS2433 has factory 64-bit ROM ID and separate writable
  512-byte EEPROM holding XStream license data. The user
  privately saved two byte-identical 512-byte backups and
  later separately saved the complete original eight-byte
  ROM identity. **No private data is present in this public
  handoff**. Current x64 ID/read/backup works; physical
  WRITE_DALLAS_MEMORY `0x00223088` is not yet implemented.
- A genuine XStream license-delete attempt sent the complete
  512-byte write to missing `0x00223088`, returned
  `0xC0000010 STATUS_INVALID_DEVICE_REQUEST`; no physical
  EEPROM write resulted from that attempt. The original
  32-byte DS2433 scratchpad/write/verify/copy and full
  readback code is recovered in Ghidra exports `11f54`,
  `16d90`, `16f2c` but has **not** been ported/tested
  on spare hardware yet. Do not experiment on the sole
  licensed card.
- The principal six-hex-digit displayed scope-ID component
  corresponds to factory Dallas ROM serial bytes 1..3
  (24-bit little-endian); two-digit suffix unresolved.
  Replacement DS2433 would have a new immutable ROM identity.
  Whether every actual license binds to this ID is unproven.
- User deliberately **DEFERRED** virtual Dallas ROM/EEPROM
  emulation and physical
  chip isolation tests. These remain in [TODO](TODO.md) and
  [Dallas recovery design](dallas-device-manager-recovery-design.md),
  NOT the next action.
- Any original or older replacement startup JSONL trace may
  expose unique Dallas ID and real license memory previews.
  Keep raw traces and backups private. Rebuilt user-mode
  `lecdiag` now suppresses direct Dallas hex data from
  newly generated JSONL; that does NOT sanitize old captures
  or arbitrary kernel debug streams.

## Two-machine layout

- PC with Ghidra and engineering checkout:
  `C:\Users\steve\Projekte\NEUE_STRUKTUR\Messtechnik\LeCroy\lecroy_wr6k_64bit_driver`.
  Established export command:
  `& ".\scripts\run-ghidra-analysis.ps1" -CommitMessage "analysis: ..."`.
  The needed `12d24`/`12bae` and field scans are already
  **pushed**; do not ask the owner to repeat the same export.
- Real x64 scope, XStream and PCI device:
  `C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`.
  This is where driver build/sign/load and the new
  `lecdiag start-register` hardware comparison must run.
- Original x86 installation is available as a separate boot
  setup for comparing legacy-driver behavior, not needed for
  the immediate read-only test.

## Documentation hygiene

The owner's explicit request: README must describe the latest
**current, verified** state only, NOT chronological mistakes,
old milestones, superseded hypotheses or a per-trace diary.
Keep detailed evidence in `docs/`; keep `AGENTS.md`
and `docs/next-chat-handoff.md` current as the source evolves.
No real license keys, actual full ROM identifiers, raw EEPROM
images or private trace byte previews in public GitHub.
