# LeCroy x64 driver: compact engineering handoff (2026-09-29)

This file describes the **latest actionable state**, not the historical investigation diary. Read [AGENTS.md](../AGENTS.md) first, then the [current handoff](next-chat-handoff.md), [TODO](TODO.md), and [README](../README.md). Repository and technical documentation are in English; converse with the owner in German.

**Public repository:** https://github.com/Battlecake91/lecroy_wr6k_64bit_driver (branch `main`).

## Actual first scope feedback (2026-09-29 late evening)

The owner has now returned a **partial console result** for the newly
committed read-only `0x00223044` compatibility test:

- `scripts/build-sign-load-driver.ps1` threw
  `Run this script from an elevated PowerShell window.` from its
  first `Assert-Administrator` call (line 13). **This invocation did
  not build, sign, install, or PnP-restart the new kernel driver.**
  A prior pure-build success is not independently verifiable from the
  supplied console fragment because its build log was omitted.
- The following interactive
  `if ($LASTEXITCODE -ne 0) { throw "Installation fehlgeschlagen" }`
  did not stop execution. `$LASTEXITCODE` reflects the last native
  executable, not a reliable success/failure code from a PowerShell
  script throwing an exception. Use an immediate `$?` check (or
  a `try/catch`) for the script invocation.
- The locally present `lecdiag.exe start-register` opened the
  device interface successfully, but its **first**
  `DeviceIoControl(0x00223044, in=0, out=4)` failed with Win32
  error 1 (`ERROR_INVALID_FUNCTION`). The generic BAR0 reference
  request was therefore **not executed**, and no two-register
  comparison occurred. Because the signed reload never ran, an older
  installed driver missing this new dispatcher case is the immediate
  working explanation, **not a proven kernel-code regression**.
  The existing dispatcher defaults unknown IOCTLs to
  `STATUS_INVALID_DEVICE_REQUEST`; further proof requires the
  elevated reload and repeated read-only diagnostic.

### Next action on the real x64 scope

Close XStream first. Open **Windows PowerShell as administrator**
(right-click -> Run as administrator), then run:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; STOP" }

& ".\scripts\build-sign-load-driver.ps1"
if (-not $?) { throw "Build/sign/load script failed; STOP" }

& ".\tools\lecdiag\build\lecdiag.exe" start-register
if ($LASTEXITCODE -ne 0) { throw "0x00223044 read/compare failed; STOP" }
```

The signed-load script rebuilds the kernel driver **and** lecdiag, signs
the SYS/CAT with the configured test certificate, installs the package,
restarts the target PCI device and runs build/passive-PCI checks. Do
not bypass an installation, PnP, or verification error. Report the
**entire installer console output** and then the new `start-register`
result, including both hex DWORDs if they are returned. A simple
`lecdiag build` response of legacy build `1002` is an ABI-compatibility
constant, not a unique fingerprint of the newly loaded binary.
Only after `start-register` reports `PASS` should the normal
XStream waveform/control/AP015 regression test be performed.

No Dallas, BAR register write, hardware isolation, or `CFDC2194`
implementation is part of this troubleshooting step.

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

There has been **no Windows build or hardware test** from the
assistant's development environment. Do not mark the new
feature as verified in `README.md` until the user returns
successful build/hardware output.

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
