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

## Matching backups obtained: prepare an intentionally invalid test marker (2026-09-29)

The user reports two independent `original-*.bin` backups with
**identical SHA-256 hashes**. This confirms that the saved byte images
match (in addition to the in-command two-read and ROM-ID checks).
Do not record the card-specific SHA-256 fingerprint or raw license
content in this public repository.

The user's next request is to **add an invented license for a
write/read compatibility test**, not to issue a working entitlement.
Before risking the installed license storage, a new purely OFFLINE
PowerShell helper now prepares a **512-byte candidate image**:

[`scripts/create-dallas-dummy-image.ps1`](../scripts/create-dallas-dummy-image.ps1).

It explicitly refuses to overwrite files, checks that both backups
are independently named, exactly 512 bytes long and identical
byte-for-byte, and searches from page 15 down for a page consisting
entirely of thirty-two `0xFF` bytes. If no entire blank page exists,
**it fails and creates no output**. If a candidate page exists, it
copies the entire original image and changes only the 30 bytes
beginning at that page's start to the unequivocally invalid ASCII
marker:

```text
FAKE-XSTREAM-LICENSE-TEST-ONLY
```

The other 482 bytes remain unchanged, including the two `0xFF`
bytes at the end of the selected page. The new filename must
end in `.ds2433.bin` (globally ignored by `.gitignore`),
uses a `CREATE_NEW` file handle, and the script reopens and
compares the entire output image. This **does not** establish
a genuine XStream license record, a valid EEPROM application
slot, or safe electrical write/erase semantics. A page which
reads as all `0xFF` may still be reserved in the license
format. The script issues NO driver IOCTL and does not
program the installed DS2433.

With the scope repository as the PowerShell current directory,
and the two already existing backups:

```powershell
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git pull failed" }

& ".\scripts\create-dallas-dummy-image.ps1" `
  -ImageA ".\license-backups\original-a.bin" `
  -ImageB ".\license-backups\original-b.bin" `
  -OutputImage ".\license-backups\fake-test.ds2433.bin"
```

The script reports only the **selected 32-byte page / EEPROM
offset**, original SHA-256 and candidate-image SHA-256.
Keep the modified file private. Do not boot XStream against
a modified installed license image until a rollback on a
spare DS2433 has been independently proven.

**Critical next gate before touching installed hardware:** current
native x64 driver still lacks original IOCTL `0x00223088`
(`WRITE_DALLAS_MEMORY`); successful read-back alone is not
writer validation. Recover the original scratchpad/copy procedure,
test write and power-cycle restore on a disposable DS2433,
then consider a bounded real-card operation only with a
known-correct full recovery path. Neither writer nor
hardware programming is part of this new offline script.

## Actual dummy-image result and XStream-native license workflow (2026-09-29)

The user ran the offline
`scripts/create-dallas-dummy-image.ps1`
against the two previously matching backups.
It **correctly refused to produce any output**:

```text
No entirely 0xFF-filled 32-byte page found.
No image created; license layout analysis required.
```

The originals were not modified, no fake test image
was saved, and the physical DS2433 was not accessed
by that offline helper. This does **not** mean all
application-level license-record positions are
occupied: the memory layout might include non-FF
padding, metadata, reserved data or a free-record
flag not aligned to an all-FF page. Never replace
the guard with a guess or arbitrarily overwrite
a non-FF section.

**New native-application strategy suggested by the user:**
potentially use XStream's existing **Add License**
UI to learn the intended memory layout/validation
and actual write procedure. The original x86 driver
implements `WRITE_DALLAS_MEMORY` `0x00223088`;
the current replacement x64 driver does NOT.
Therefore, an attempt under current x64 XStream
cannot prove the missing native x64 writer works.
Prefer a **passive, metadata-only trace** of the
original x86 XStream/license workflow (IOCTL
number, input/output buffer *length*, returned
status, order and time, NOT EEPROM/license input
bytes). Entering an **obviously invalid fake
license** into XStream may be rejected before
any EEPROM write and is then only evidence about
input validation. Do not persist a fabricated
license to the actual card or coerce a rejected
key. A legitimate, authorized unused license,
if one exists, may exercise the real XStream
workflow, but can still modify the only card
holding working licenses: treat it as a separate,
explicitly considered hardware-changing action,
not the automatic next step merely because two
backups match.

For a locally redacted view of the 512-byte
image, added an **offline read-only**
[`scripts/inspect-dallas-image.ps1`](../scripts/inspect-dallas-image.ps1).
It prints only per-32-byte-page counts of FF,
zero and printable bytes. With optional
`-After`, it prints changed offset ranges,
changed-byte counts and affected pages without
printing key content, raw memory, strings or
card-specific hashes.

```powershell
# Run locally; no device connection and no memory writes.
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git update failed" }

& ".\scripts\inspect-dallas-image.ps1" `
  -Before ".\license-backups\original-a.bin"

# Only if a real XStream action was deliberately performed and an
# independent *new* 512-byte read-only backup was made afterward:
& ".\scripts\inspect-dallas-image.ps1" `
  -Before ".\license-backups\original-a.bin" `
  -After ".\license-backups\after-xstream.bin"
```

Do the application's license action, if elected,
preferably under the original 32-bit driver to
observe its actual write implementation. Close
XStream before attempting read-only snapshot
commands if driver handles are exclusively
opened. The identical underlying card can be
read again using the x64 backup helper after
booting x64 if capturing directly under the
original x86 environment is inconvenient.
Avoid raw original x86 trace publication:
license-write IOCTL input buffers might
contain active license material.

**No hardware writer has been added, used or
validated by this change.** Follow the spare
DS2433 writer/restore gate below before
any deliberate low-level write or deletion
on the only licensed PCI card.

## Redacted original image inspected; potential native delete/re-add A/B (2026-09-29)

The user ran the read-only redacted
`scripts/inspect-dallas-image.ps1 -Before
license-backups/original-a.bin`. Its result explains why the
previous hypothetical all-`FF` test-image generator correctly
declined the input:

| Page(s) and offset(s) | Observed aggregate only (no secret bytes) |
|---|---|
| 0..5 / 0x000..0x0BF | predominantly printable content mixed with zero/FF and other bytes; no proof yet of license record boundaries |
| 6..10 / **0x0C0..0x15F** | **160 consecutive zero bytes** (five completely zero-filled 32-byte pages) |
| 11 / 0x160..0x17F | 28 zero bytes and four other (non-printable/non-FF) bytes |
| 12..14 / **0x180..0x1DF** | **96 consecutive zero bytes** (three completely zero-filled 32-byte pages) |
| 15 / 0x1E0..0x1FF | 29 zero bytes and three FF bytes |

This image uses a large amount of `0x00` fill, **not**
all-`FF` free pages. Do not interpret a zero-filled region as
an authenticated free license entry: it may be reserved capacity,
terminators, structured storage or covered by a global integrity
check. Likewise, the ASCII-heavy beginning suggests human-readable
data, but does not establish that XStream keys are plaintext without
private format inspection. The original user-specific page-by-page
counts/hash and actual EEPROM contents remain private.

**User-proposed alternative:** delete ONE key using XStream's own
license manager and then re-add the same legitimate key. The
native application would exercise the correct record layout and
possibly the original EEPROM write path. This is only a
**possible controlled hardware-changing experiment**, not an
instruction to erase the installed licensed chip immediately.

Safety and interpretability prerequisites:

1. Confirm the exact existing valid key is **independently available**
   (not solely in the DS2433 and not merely masked by the UI);
   confirm XStream will allow re-adding it and record current
   license/feature status privately. Ideally have a second
   properly backed-up PCI card or a tested spare-chip recovery
   path. Two matching 512-byte images prove read/backup
   consistency, **not successful EEPROM restore**.
2. Prefer the **original 32-bit XStream with original x86 driver**
   for any intentional native delete/re-add write. Its
   `0x00223088` Dallas writer exists; native x64 has **no
   writer** yet. Running the current x64 license UI cannot be
   claimed as a successful DS2433 write test.
3. Establish a **no-op control**: capture an independent
   read-only baseline image; simply launch/close original
   XStream and its license UI without modifying keys; close
   it and capture another independent read-only image.
   Compare both via `inspect-dallas-image.ps1 -Before ...
   -After ...`. This catches unrelated application
   housekeeping updates that might occur without a delete.
4. Only if key re-entry and recovery risks are explicitly
   acceptable: use native XStream to delete **one** known,
   re-enterable entry, close it, and save a third private
   512-byte backup. Run the offline redacted before/after
   comparison. This may remove an important licensed feature
   temporarily; do not delete every key or touch ROM ID.
5. Re-add the **same legitimately owned** key through XStream,
   close, make a fourth read-only 512-byte image and compare
   baseline/delete/restored image pairs. A fully matching
   final image would be powerful evidence of exact restoration;
   a differing final image does not automatically mean failure
   because application bookkeeping might be updated. Verify
   original license recognition and associated XStream feature
   visibly as a separate functional check.
6. If XStream refuses a re-entry, fails, or shows a different
   licensing status, **stop** rather than forcing raw EEPROM
   writes or reprogramming guessed offset ranges. Keep all
   pre-change images and the original key information private.

A safe read-only snapshot on the original x86 environment should
be possible by building the already-present compatible diagnostic
source with:

```powershell
& ".\scripts\build-lecdiag.ps1" -Architecture x86
```

The result is `tools/lecdiag/build/x86/lecdiag.exe`, whose device
enumeration and original legacy Dallas IOCTLs are designed to
support either driver. **This particular backup flow on the original
x86 system has not yet been hardware-tested**; if opening it or
READ_DALLAS_MEMORY fails, do not interpret that as an erased EEPROM.
Alternatively, take independent *read-only* snapshots using the
already-tested x64 `lecdiag dallas-backup` after shutting down
x86 XStream and booting x64, if the same physical PCI card is
present. Keep all outputs in the private ignored
`license-backups/` folder.

For example, comparing only the page/offset structure after two
independent snapshots exist:

```powershell
& ".\scripts\inspect-dallas-image.ps1" `
  -Before ".\license-backups\before-ui.bin" `
  -After ".\license-backups\after-noop-ui.bin"
```

If additionally collecting a passive original-x86 trace during
license editing, **NEVER share the raw
`0x00223088` input payload or a complete EEPROM write IOCTL
trace publicly**: that buffer can contain live licensing
material. Retain only numeric IOCTL IDs, buffer lengths,
status, sequence order and timings in public documentation,
with any actual key bytes redacted before sharing.
Any legitimate actual deletion/addition is a user-controlled
operation through XStream, separate from our still-unimplemented
native x64 writer.

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

## 2026-09-29 01:18: XStream delete attempt proves missing x64 write dispatch

**Decisive new REAL scope evidence:**
The user clicked Delete for a license in current XStream
under the replacement x64 driver, but XStream still showed
the key after application restart. Uploaded private trace
`xstream_trace_20260929_011858.jsonl` records the
attempt directly. It is **SENSITIVE**: the existing
unredacted trace contains a preview of the intended
write image, possibly exposing actual license keys;
do not publish/commit/re-share the raw JSONL or
actual serial/license values.

This is a short, **continuous** 63.0224566-second
capture of **608 IOCTLs**, sequence 1..608,
no snapshot omissions:

| Seq and relative time | Observed transaction | Result |
|---|---|---|
| 1, t=0 | `GET_DALLAS_ID` 0x00223080 | success, Information 8 |
| 3, t~0.804498 s | `READ_DALLAS_MEMORY` 0x00223084 | success, 512 bytes returned |
| **522, t~57.303140 s** | **`WRITE_DALLAS_MEMORY` 0x00223088**, XStream WOW64 process, **512-byte input, zero output** | **0xC0000010 / STATUS_INVALID_DEVICE_REQUEST**, Information=0 |
| **523, t~57.824097 s** | immediate `READ_DALLAS_MEMORY` 0x00223084 | success, 512 bytes returned |
| 607, t~63.022424 s | final `GET_DALLAS_ID` | success, Information 8 |

Of 608 captured IOCTLs **607 returned NTSTATUS
success; the SINGLE failure is XStream's
512-byte Dallas WRITE**. The returned READ response
at seq 523 has the **same captured first 128 bytes**
as baseline seq 3 (the JSONL caps output preview
at 128, so full 512-byte post-state cannot be
deduced solely from this comparison).
The intended write request is 512 bytes (preview
includes first 256, redacted here).
Within the **first 128 bytes available from both
the baseline READ and attempted WRITE**,
93 byte positions differ, across 32-byte
pages 0..3. That comparison is
offset/count-only, and the intended full
new 512-byte image is NOT reconstructible
from the trace preview alone. These differences
support the interpretation that XStream prepared
a modified full-memory candidate, but do not
prove its complete format or actual persistent
write (the kernel rejected the IOCTL first).

**Root cause is directly source-backed:** native
`driver/Ioctl.c` currently has case labels for
Dallas ID 0x80 and READ 0x84, but no
`0x00223088` write case/define; dispatch
initializes `status = STATUS_INVALID_DEVICE_REQUEST`
and leaves that status in the default branch.
The trace matches this exactly. Therefore
the failed XStream Delete action **is not
evidence of an EEPROM write/erase failure,
a wrong license parser or no free slot**.
The current x64 replacement simply does not
implement the original ABI for Dallas memory
writes. The post-failure successful read and
persisting UI key are consistent with the
hardware remaining unchanged.

**Next implementation step, before another live delete:**
The original x86 IOCTL dispatch handler is
documented at decompiler VA `0x11F54`,
with 1..512-byte input and 32-byte chunks
plus readback verification. Added
`11f54`, `asm:11f54`, `xref:11f54`
to `ghidra_scripts/targets.txt` so the
PC-side Ghidra export can recover the
actual DS2433 scratchpad/copy sequence.
Export through `scripts/run-ghidra-analysis.ps1`
on the Ghidra PC, review commands, timing,
authorization, retries and failure semantics,
then implement a separately gated native
x64 write path. Do NOT fabricate a STATUS_SUCCESS
stub, blindly copy a full image, or reattempt
live XStream deletion before the writer and
safe restore procedure are independently validated.
An existing Dallas 512-byte backup is a read
result, not proof of a functioning restore path.

**Privacy improvement committed to the diagnostic
EXE only:** `tools/lecdiag/lecdiag.c`
now writes empty `input_hex` for
WRITE_DALLAS_MEMORY 0x88, empty `output_hex`
for READ_DALLAS_MEMORY 0x84 and
GET_DALLAS_ID 0x80, while preserving
the IOCTL code, input/output lengths, status,
ordering, and adding
`sensitive_payload_redacted=true`.
This applies only to newly **rebuilt lecdiag
trace-save/trace-capture JSONL**, not the
existing private raw trace or kernel
debugger/trace-ring internals. The replacement
driver itself is NOT modified by this redaction.

**Optional strictly read-only follow-up:** after
closing XStream, a new `dallas-backup` to
a unique private filename can compare the
entire persistent 512-byte image with
`original-a.bin` via
`inspect-dallas-image.ps1 -Before ...
-After ...`, without displaying keys.
This full comparison is not needed to explain
the failed write status: the missing x64
dispatch is already decisive.



## 2026-09-29: displayed scope identifier correlation

Private trace `xstream_trace_20260929_011858.jsonl` contains
two successful eight-byte GET_DALLAS_ID results at sequences
1 and 607. They match byte for byte, carry the DS2433
family byte 0x23, and pass Dallas CRC8 validation.
The **first three serial bytes at ROM offsets 1..3,
interpreted as a little-endian 24-bit integer,
exactly match the primary six-digit scope identifier
reported by the user**. The trailing two-digit display
suffix has not been independently mapped to a ROM byte,
and the formatted whole identifier does not appear as
literal ASCII in the captured I/O previews. Do not
publish the card's complete ROM serial or EEPROM contents
in this public file.

ROM identity and writable 512-byte DS2433 EEPROM are
different structures. If the EEPROM contents were lost
but the physical device's ROM identity remained valid,
a private backup from that **same** card would provide
the original memory payload for a separately validated
recovery operation. Feeding a backup image to XStream
through a software-only read view would not, by itself,
program persistent physical memory. A real, verified
write and read-back path is still necessary. Do not
replace the factory ROM identifier with another card's
identifier or treat a simulated application view as
physical EEPROM recovery.

The newly pushed original x86 Ghidra handler
`ghidra_exports/selected/00011f54_FUN_00011f54.c`
confirms bounded input 1..512, 32-byte write chunks,
a full requested-length read-back, a bytewise comparison
and up to three attempts. Calls to internal helpers
`FUN_00016d90` and `FUN_00016f2c` remain unresolved
until their own implementations are exported. The
kernel driver's missing native x64 write handler and
the sensitive-user-data redaction precautions from
trace 011858 remain the current status.
