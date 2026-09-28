# PCI DS2433 license memory: safe read, backup and future write validation

**Updated:** 2026-09-29.
**Hardware source:** user-provided `PCI Card.pdf` sheet 1 shows
`U11 DS2433` ("ID Chip") wired to the PCI-side Spartan-IIE
`U3 XC2S200E` through net `ID_DATA`. The user additionally
confirms that the **PCI-card 1-Wire EEPROM stores the XStream
license keys**. The user believes the keys appear in plaintext;
the specific bytes, data layout, checksums and any relationship
to the unique 1-Wire ROM ID have **not yet been inspected or
validated**. Do not publish keys or infer their storage format
without a private dump.

This storage is unrelated to the **five front-facing ProBus
probe sockets**: those use an ADC identification value for
initial ProBus classification, then I2C for the front EEPROM
and probe control. It is also distinct from the PCI card's
`U6 XC18V02` Spartan configuration PROM.

## Important current software status

| Function | Original x86 driver | Current x64 driver and tool |
|---|---|---|
| Dallas ROM ID | `0x00223080`, eight bytes, ROM CRC-8 | Implemented in `driver/Ioctl.c`; `lecdiag dallas-id` available. |
| Dallas EEPROM read | `0x00223084`, request 1..512 bytes, read from address `0x0000` | Implemented, including full 512-byte reads; `lecdiag dallas-read` prints hex, and new **`lecdiag dallas-backup`** saves a *binary* private backup. |
| Dallas EEPROM write | `0x00223088`, recovered original x86 handler, 1..512-byte input, 32-byte chunks and read-back verification in legacy analysis | **NOT implemented** in the current native x64 `driver/Ioctl.c`; no `lecdiag` writer or eraser. Do not imply writable x64 memory has already been validated. |

The x64 controller is `BAR2+0x040 ONEWIRE`. The
physical schematic identifies the DS2433 behind
the PCI FPGA's `ID_DATA` net. The schematic does
not expose FPGA RTL to independently prove the
exact BAR-to-pin mapping; the combined source
and schematic information is strong supporting
evidence, not a schematic-labelled BAR address.

The DS2433's immutable 64-bit **ROM identity**
and mutable **512-byte memory image** are
**different data**. A valid ROM-ID CRC proves
the ROM identity was read coherently, not that
EEPROM license data or any XStream license
record is valid.

## Immediate read-only test; do not touch license content

A read-only `dallas-backup <new-file.bin>`
subcommand was added to `tools/lecdiag/lecdiag.c`
on 2026-09-29. It:

1. Obtains the eight-byte Dallas ROM ID (the
   driver verifies Dallas/Maxim ROM CRC-8).
2. Reads the **entire 512-byte EEPROM twice**.
3. Obtains the ROM ID a second time and requires
   both ROM IDs and both complete dumps to match.
4. Only then creates a **new** filename using
   `CREATE_NEW` (never overwrites an existing backup).
5. Writes exactly 512 raw bytes, flushes the
   file and reopens it to check the persisted
   byte-for-byte content and exact EOF.
6. Prints **no license bytes** and issues **no
   EEPROM write/erase IOCTL**.

Build the diagnostic program using the documented
Windows C++ tools on the scope. This source change
is **not yet compiled or executed on the hardware**
at the time of this documentation:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git update failed" }

& ".\scripts\build-lecdiag.ps1"
if ($LASTEXITCODE -ne 0) { throw "lecdiag build failed" }

New-Item -ItemType Directory -Force ".\license-backups" | Out-Null

$diag = ".\tools\lecdiag\build\lecdiag.exe"
& $diag dallas-id | Tee-Object ".\license-backups\dallas-rom-id.txt"

& $diag dallas-backup ".\license-backups\ds2433-original-a.bin"
if ($LASTEXITCODE -ne 0) { throw "Backup A failed" }

& $diag dallas-backup ".\license-backups\ds2433-original-b.bin"
if ($LASTEXITCODE -ne 0) { throw "Backup B failed" }

$files = @(
  ".\license-backups\ds2433-original-a.bin",
  ".\license-backups\ds2433-original-b.bin"
)
$hashes = $files | ForEach-Object { Get-FileHash -Path $_ -Algorithm SHA256 }
$hashes | Format-Table -AutoSize
if ($hashes[0].Hash -ne $hashes[1].Hash) {
    throw "Backup A/B hashes differ: do not attempt writes"
}
Write-Host "Two independently captured 512-byte backups match."
```

Perform the initial backup with XStream closed and the
card/driver in a stable configuration. The files are
sensitive: store a second copy **outside** the public
Git checkout, ideally on another medium. `.gitignore`
excludes `license-backups/` and `*.ds2433.bin`;
that protection is not a substitute for keeping
secrets out of screenshots, terminal transcripts,
issues, attachments and commits.

## Future controlled write test, separate authorization required

**Do not erase the installed license memory as the
first write test.** A license might be required for
software startup or options; inability to restore
could make the otherwise working instrument
unusable. A backup image and valid ROM CRC
alone do not guarantee a working restore procedure.

A sensible validation sequence is:

1. Independently prove two full backups match and
   retain a private copy away from the instrument.
2. Inspect the binary image privately to determine
   whether readable ASCII/UTF-16 license fields,
   checksums, data redundancy, unused bytes or
   other structures actually exist. Treat the
   plaintext hypothesis as unverified until then.
3. Recover the original x86 DS2433 write/scratchpad/
   copy/readback algorithm, including exact page
   boundaries, addressing and power/timing rules.
   The original ABI begins at address zero and
   must not be misrepresented as an arbitrary-offset
   safe scratch-area write.
4. Implement and bench-test the x64 writer **using
   a disposable spare DS2433 or separately
   instrumented test card** first. Read back
   and compare complete images, including after
   a power cycle. Test failed/cancelled writes.
5. Only with a demonstrated recovery path,
   verified image and explicit user go-ahead
   consider a restricted on-card write/restore test.
   No blanket erase or destructive license
   editing is part of the present plan.

This is a hardware compatibility and backup test
for the user's own instrument, **not** a method
for generating, bypassing or sharing license keys.
No actual license contents are committed to the repo.

Related:
[`pci-card-acquisition-board-topology.md`](pci-card-acquisition-board-topology.md),
[`hardware-register-map.md`](hardware-register-map.md),
[`ioctl-map.md`](ioctl-map.md),
[`next-chat-handoff.md`](next-chat-handoff.md).
