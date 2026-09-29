# LeCroy x64 driver: compact engineering handoff (2026-09-29)

This file describes the **latest actionable state**, not the historical investigation diary. Read [AGENTS.md](../AGENTS.md) first, then the [current handoff](next-chat-handoff.md), [TODO](TODO.md), and [README](../README.md). Repository and technical documentation are in English; converse with the owner in German.

**Public repository:** https://github.com/Battlecake91/lecroy_wr6k_64bit_driver (branch `main`).

## Exactly what to expect from the next user turn

The previous chat made a narrow, **source-only** implementation of the
original 32-bit IOCTL `0x00223044` on the replacement x64 kernel driver
and added a read-only `lecdiag start-register` validation command.
**The user has NOT yet reported a Windows build, installed this new
version, or returned hardware diagnostic output.**

The user was asked to:

1. On their working Windows x64 scope, update the checkout at
   `C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`:
   `git pull --ff-only origin main`.
2. First build without installing:
   `& ".\scripts\build-driver.ps1" -BuildLecdiag`.
3. **Only if the build succeeds** and after closing XStream, run
   the established signed install/reload from an **elevated PowerShell**:
   `& ".\scripts\build-sign-load-driver.ps1"`.
   It builds again, signs SYS/CAT using the existing configured test
   certificate, installs via the Driver Store, restarts the PnP PCI
   device, then performs build and passive PCI checks.
4. Run the **read-only** new comparison:
   `& ".\tools\lecdiag\build\lecdiag.exe" start-register`.
   It invokes legacy `0x00223044` with no input and a four-byte
   output buffer, then independently invokes the existing generic
   BAR0+0 register read and checks **both DWORD values are identical**.
5. **Report the actual console output / error and exit code** in the
   next conversation. If there is a build, signing, PnP, IOCTL, or
   register-value mismatch, stop at that step and diagnose before
   continuing. If successful, perform the ordinary XStream waveform,
   controls and AP015 recognition regression. Do not assume either
   build or hardware validation has happened merely because source
   commits exist.

Commands to paste into elevated PowerShell **on the x64 scope**:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed; stop" }

# Read and inspect build output before installing anything:
& ".\scripts\build-driver.ps1" -BuildLecdiag
if ($LASTEXITCODE -ne 0) { throw "Build failed; stop" }

# Close XStream BEFORE the next command. Requires Administrator:
& ".\scripts\build-sign-load-driver.ps1"
if ($LASTEXITCODE -ne 0) { throw "Driver signing/load failed; stop" }

# New read-only compatibility test:
& ".\tools\lecdiag\build\lecdiag.exe" start-register
if ($LASTEXITCODE -ne 0) { throw "Start-register comparison failed; stop" }
```

The last step is a passive read; it must **not** write to the
DS2433, BAR registers, or license storage. Installation/reload itself
does restart the PCI device, so run only on the agreed test scope
after closing the XStream application.

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
  emulation, Device Manager recovery exploration and physical
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
