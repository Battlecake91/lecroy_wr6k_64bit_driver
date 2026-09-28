# Active handoff: five-slot ProBus I2C and private PCI Dallas license backup (2026-09-29)

**Read this file and `AGENTS.md` before changing the driver.**
Conversation in German, repository documentation and source comments in English.
Repository: https://github.com/Battlecake91/lecroy_wr6k_64bit_driver;
active branch: `main`.

**LATEST ENGINEERING STATE:** The user's attempted
offline dummy-image generation **failed safely**:
both independently verified 512-byte DS2433
license backups match, but no entire
32-byte page is all FF. No new image,
no original-file change and NO
EEPROM write resulted. Do not infer
all application-level license slots
are occupied or disable the guard.
User can attempt XStream Add License,
but a fake input might fail validation
before any 1-Wire write. Original x86
driver supports write IOCTL 0x00223088;
native x64 still does not. New strictly
**read-only redacted**
`scripts/inspect-dallas-image.ps1`
reports per-page occupancy counts and,
given a later post-XStream private
backup, only changed byte ranges,
not keys. Avoid public raw licensing
traces/dumps and any speculative
on-device erase/write. Full current
workflow:
`docs/dallas-license-memory-test-plan.md`.
Five front ProBus slots remain
**I2C-only**, separate from
PCI DS2433 1-Wire licensing.

**Current scope/driver regression state:** The user's own visible XStream behavior
corrects the previous diagnosis: **opening the AP015 generates an
explicit not-locked warning about measurement accuracy.** Absence of
pending-0x0200 in earlier particular IOCTL captures `233125` and
`235314` never proved XStream failed to recognize the mechanical
state. In the new user-supplied scope trace
`xstream_trace_20260929_002051.jsonl` (same unchanged post-`3490709`
driver), four authentic 0x0200-bearing events all have matching
0x88/0x82; state `0x0058` at ~24.623 s correlates with opened
jaw and family-1/0x4A `F7`, while state `0x00A7` at ~49.861 s
correlates with closed jaw and family-1/0x4A `F3`. That identical
open/F7 and closed/F3 pairing already appeared in `231656`.
The event stream also includes `0x03FE` (~35.038 s, an apparent
removal state) and `0x0057` (~37.458 s), followed by proper AP015
reidentification. No generic HWInt, jaw recognition, PCI or DMA
regression is established. **Do not request another routine
jaw/hotplug test or modify driver code because of the superseded
inference.** The formatter `3490709` remains working.

## Update: no all-FF page; consider native XStream license dialogue (2026-09-29)

The user executed the previously added offline
`scripts/create-dallas-dummy-image.ps1` against
two identical 512-byte backups. The script
**correctly aborted** because **no entire
32-byte page is all `0xFF`**. Hence
`fake-test.ds2433.bin` was **NOT created**.
The two private source images and original
PCI licensing DS2433 were unchanged.
This observation does NOT prove no free
application-level license slot: possible
alternate allocation/padding/record structure
requires actual data-format evidence.
Do NOT remove the FF-page safety check
to overwrite unknown live license records.

The user offers to try the **XStream Add License**
UI. This may reveal the application's
validation/storage process more faithfully
than inventing arbitrary EEPROM offsets.
Key discriminator: original x86 implements
`0x00223088 WRITE_DALLAS_MEMORY`;
native replacement x64 `driver/Ioctl.c`
still has no write case. An invalid/fake
license may be rejected by user-mode
validation **before** a write IOCTL,
which proves nothing about EEPROM writes.
Do not claim a real write, install working
credentials or erase licensed data merely
because read-only backups are consistent.
If passive original-x86 trace is captured,
extract only IOCTL number, in/out LENGTH,
status, chronological order. Raw
`0x00223088` input could contain keys;
never publish full input buffers.

Added independent **redacted read-only local
image analyzer**:
`scripts/inspect-dallas-image.ps1`:
before-only mode prints each of 16 pages'
FF, 00, printable and other byte COUNTS;
before/after mode additionally prints
changed byte offsets/ranges and affected
page indices, no secret byte values.
Use it on original backups now and, if
a user-authorized native XStream action
actually changes storage, on a newly
read-only-captured post-image to learn
the real layout without sharing keys.
Because `lecdiag` uses original legacy
interface GUIDs, an x86 build can also
access the original-driver ABI; otherwise,
after running original x86 XStream
and closing it, boot current x64
and re-run read-only backup on the
same physical DS2433. Avoid
concurrent exclusive driver opens.

**No kernel code or EEPROM writes were
changed by this finding.** A protected
spare DS2433 writer/power-cycle-restore
validation remains mandatory before
deliberate low-level modifications
to only licensed card. Details:
`docs/dallas-license-memory-test-plan.md`.

## Latest Dallas test: two matching backups, offline dummy image only (2026-09-29)

The user has now **run `lecdiag dallas-backup` twice**
and supplied the two matching **SHA-256** results.
This is positive validation of the backup path on
their real scope, subject to the fact that hashes
do not identify license-record formatting or
establish a safe write/restore path. **Never
store the particular image hash or raw bytes in
the public repository.**

The user asks for an invented extra license to
test EEPROM writing. Do **not** confuse a test
marker with a real XStream license or immediately
overwrite the only functioning installed license
storage. Added a safe private **offline-only**
preparation script:
[`scripts/create-dallas-dummy-image.ps1`](../scripts/create-dallas-dummy-image.ps1).
It takes two independent 512-byte backups,
verifies exact byte equality, searches for the
last completely `0xFF`-filled 32-byte page,
and creates a new 512-byte copy in which
**only 30 bytes** on that page hold the ASCII
marker `FAKE-XSTREAM-LICENSE-TEST-ONLY`.
It refuses no-page/invalid-length/mismatched
inputs and existing output filenames,
requires the Git-ignored `.ds2433.bin`
suffix and verifies the persisted file.
No secret bytes are printed; NO
DeviceIoControl, chip write or erase is
performed by this offline script.

Sample private invocation from scope repo,
assuming previous user backups were named
`original-a.bin`/`original-b.bin`:

```powershell
git pull --ff-only origin main
& ".\scripts\create-dallas-dummy-image.ps1" `
  -ImageA ".\license-backups\original-a.bin" `
  -ImageB ".\license-backups\original-b.bin" `
  -OutputImage ".\license-backups\fake-test.ds2433.bin"
```

**Remaining hardware gate:** x64
`driver/Ioctl.c` does not implement
original x86 `WRITE_DALLAS_MEMORY`
0x00223088. Even an all-`FF`
page is not proven to be a safe
license slot or excluded from any
global checksum. Recover the actual
DS2433 scratchpad/copy algorithm and
test write/read/power-cycle restore on
a disposable chip before exposing
the only licensed card to a write.
Details:
[`docs/dallas-license-memory-test-plan.md`](dallas-license-memory-test-plan.md).
The new PowerShell script has been
committed but not executed in the user's
Windows environment by the assistant;
do not claim on-hardware test-image
creation or write success.

## New Dallas licensing and five-slot I2C facts (2026-09-29)

The user explicitly corrects the architecture: the actual five
front-facing probe connectors use **I2C exclusively**; neither
probe EEPROM reads nor physical probe controls use SPI.
The separate SPI networks are believed to be primarily
used for internal ADC/reference/configuration ICs. This
is user physical-hardware knowledge, independent of
the source-proven host family-0/0x90 BAR1
`SPICTL/SPIDAT/SPIDIN` helper. The full
mapping of that helper to internal devices is
still open. The Overview's `I2C(0:5)`
signal range should not be equated with an
exact socket count without a child sheet.

The user's second correction is that the PCI card's
`U11 DS2433` is the **XStream license EEPROM**.
It has an independently readable eight-byte
one-wire ROM ID and 512-byte memory. The
user expects license keys may appear in plaintext;
do not assert/print/publish actual contents
before obtaining and examining a **private dump**.
The DS2433 is distinct from front-probe EEPROM
(I2C) and FPGA configuration PROM XC18V02.

**Existing x64 read-only coverage:**
`0x00223080` Dallas ID with CRC-8,
`0x00223084` 512-byte memory read.
Original-driver x86 `0x00223088`
write is recovered but **not in current x64
driver/Ioctl.c**; the CLI also previously had
only `dallas-id` and console hex `dallas-read`.
A strictly read-only `dallas-backup` binary
output command has now been implemented in
`tools/lecdiag/lecdiag.c`, source only:
it reads ROM ID, 512-byte EEPROM image twice,
ROM ID again, compares and persists to a
CREATE_NEW binary path, then verifies actual
saved bytes/EOF. No secret bytes printed,
no write/erase. Must still compile and undergo
first actual scope test. The `license-backups/`
folder and `*.ds2433.bin` are now Git-ignored.
Backup instructions, distinct ROM/memory roles,
and future guarded spare-chip write tests:
[`docs/dallas-license-memory-test-plan.md`](dallas-license-memory-test-plan.md).
**Never test erase/restore first on the only
working XStream-license chip**: a full dump
does not by itself validate the unimplemented
writer or recovery procedure. Keep scope CPU
and DMA constraints as before.

## New primary hardware sources: supplied PCI schematic and acquisition top level

The user has now supplied two one-page A2 PDFs as actual hardware
reference material:
- `PCI Card.pdf`: Perigee LLC schematic, sheet 1, dated
  2003-03-12; this shows the **separate PCI interface card**.
- `Overview.pdf`: LeCroy Corporation acquisition board
  **top-level** drawing (model header `901586-XX`).

Both were inspected visually, including the schematic/vector
content, not just the PDF text extraction. The Overview has
no regular extractable words. **These files are user uploads,
NOT source artifacts to be copied into the public GitHub repo:**
the Overview carries an explicit proprietary-information notice.
Derived, carefully qualified hardware observations are now
documented in the canonical
[`pci-card-acquisition-board-topology.md`](pci-card-acquisition-board-topology.md).

**Critical new PCI-side topology:**

- `U3 XC2S200E` **Spartan-IIE FPGA** receives conventional
  PCI signals through several `PI5C3861` bidirectional
  bus switches (schematic explicitly describes 5-V/3.3-V
  interfacing). `INTA#` is within the PCI/FPGA signal group.
- `U11 DS2433` is a physical PCI-card
  **Dallas 1-Wire ID/memory** IC, marked `ID Chip`, on
  net `ID_DATA` wired to FPGA U3. This gives the
  recovered original-driver `BAR2+0x040 ONEWIRE`
  and GET/READ/WRITE Dallas IOCTLs a concrete
  **PCI-card-local device candidate**. BAR2-to-ID_DATA
  FPGA RTL is not depicted, so treat the exact mapping
  as source-plus-schematic inference.
- `U6 XC18V02` is separate PCI Spartan
  **configuration PROM**, neither Dallas 1-Wire
  nor front-panel/probe I2C EEPROM. The schematic
  has **optional**, mutually alternative PROM
  boot (`R60/R63/R66`) and remote configuration
  **via link** (`R88/R89`) resistor stuffing;
  actual stuffing was not determined.
- `J1` is a **40-pin Receive Header**, `J2` a
  **40-pin Transmit Header**. Each has differential
  CLOCK, twelve differential D0..D11 data pairs,
  SYNC and RESET_ERR plus status/ground connections;
  separately drawn RX termination and TX resistor
  networks. These define a distinct FPGA-to-
  acquisition-board **physical link**, not ordinary
  PCI continuing through the front ends. Link
  encoding/protocol and exact endpoint/RTL are
  not shown.

**Separate acquisition-board overview:**
`Power Conv/Filters (PC)`, `UP Control (UP)`,
`Timebase (TB)`, two different `ADC+MAM`
(`AM` and `AM2`), `FPGA's (FP)`, four
channel front ends plus `EXT` appear as
distinct blocks. It explicitly labels
`I2C(0:5)` in UP/front-end region,
plus **separate** `SPI_IO(0:40)`,
`UC_SPI(0:4)`, `Voltage_Monitor(0:32)`,
`MTT_FPGA(0:35)`, `ADC_CNTL(0:25)`,
`FE_CHx_ADC(0:1)`, FPGA/ADC and JTAG
bus groups. This independently corroborates
I2C distribution and hardware separation, **not**
the precise ADC channel for the user's ProBus
recognition value or the front EEPROM address.

**Three memories and multiple FPGAs MUST NOT be confused:**
PCI `U11 DS2433` = 1-Wire board ID;
PCI `U6 XC18V02` = configuration PROM;
front-panel/probe identification EEPROM = I2C after
ADC ProBus classification (user-provided).
PCI Spartan U3 is **not** acquisition-board
AM/AM2/FP. Family-0/0x90 host BAR1 SPI
register operations are not automatically the
external physical probe I2C bus. Nor do
0x0200 HWInt, 0x82 state or 0x4A metadata
directly reveal ADC/I2C electrical transaction bytes.
No driver source changes follow from this topology
alone; keep working verified HWInt, 85FB
framing and DMA untouched.

## Front-panel probe hardware architecture (user clarification, 2026-09-29)

The user has supplied **physical hardware/protocol information**
that changes how we should interpret the trace, not the working
driver implementation:

1. The first probe-class detection happens via an **analog ADC
   identification value**. XStream interprets that value as
   designating a **ProBus** probe.
2. **After** the ADC/ProBus decision, the **front-panel EEPROM**
   is read over **I2C** to identify the device.
3. Physical **probe control also uses I2C**.

These are separate stages. The exact ADC channel/value/threshold,
front I2C controller, EEPROM slave address/bytes, and any
host-to-I2C firmware bridge remain **unidentified in our data**.
XStream initiates the high-level identification, but there is no
proof that the Windows kernel driver itself drives SDA/SCL.

**Essential host-versus-probe-layer distinction:** original
family-0/0x90 is *statically proven* to use BAR1
`SPICTL/SPIDAT/SPIDIN`, including selector-0x0E 144-bit
host-to-board traffic. This is not the same assertion as
"the AP015 electrical bus is SPI", and must NOT override
the user's information that the physical probe-side
control is I2C. The exact relationship (if any) of BAR1
SPI to the physical I2C master remains to be recovered.
Firmware-forwarded 0x4A returns AP015 metadata (270-byte
aggregate Information, first 128 bytes captured); an
EEPROM origin is a hardware-informed candidate, not a
byte-proven mapping. Likewise, 0x0200 is the host-side
notification bit from the BAR0 INTST-0x08 / BAR1 HWInt
path; its presence/absence is not itself an ADC value
or a raw I2C transaction.

**First wrong "1/2 clamp" reinsertion in `001152`:**
0x82 state `028C -> 0058` appeared and the normal
270-byte AP015 reidentification packet did not follow.
The new ADC-before-EEPROM knowledge separates possible
physical contact, early analog classification,
EEPROM/I2C access and firmware-state timing hypotheses.
Do not assert that EEPROM contents were corrupt or that
a specific I2C failure occurred. The subsequent three
correct reinsertions produced normal AP015 metadata.

**Other current behavior stays validated:** the user-visible
unlocked-jaw warning and trace `002051` pair
0x82 `0058`/0x4A F7 for opened/unlocked and
0x82 `00A7`/0x4A F3 for closed. Neither 0x82 nor the
F7/F3 bit can yet be assigned to a particular physical
I2C register or EEPROM field. Refrain from further
generic IRQ, PCI or DMA changes.

Full canonical page:
[`docs/probus-detection-i2c-architecture.md`](probus-detection-i2c-architecture.md).
The source-backed host commands are documented in
`docs/ioctl-map.md`, and the runtime event comparisons
in `docs/probus-calibration-ab-comparison.md`.

## Current verified status

The targeted original-interrupt-path restoration in driver commit
`70716bace9ec874cf9b2f123a7946288215e1810` now has its first successful
reported real-hardware XStream regression. In the user's scope-side test,
AP015 was already attached at startup, was switched to a higher A/div range,
was disconnected once while XStream remained running, then was reconnected
once. The user reports that XStream detected the changes correctly and then
closed the application. The corresponding new kernel IOCTL capture is:

`xstream_trace_20260928_230614.jsonl` (user upload, **not** stored in the
public GitHub repository).

The pre-patch trace `xstream_trace_20260928_213834.jsonl` had missed probe
removal and lost the displayed waveform on reinsertion despite successful
DMA returns. The new test establishes the previously missing genuine
asynchronous command-status route. Do not claim that every ProBus special
function or every possible hotplug timing is now exhaustively validated.

### New capture: objective markers

| Measurement | Captured result |
|---|---:|
| IOCTL entries | 19,288 (seq 1..21761) |
| Missing sequence entries between trace snapshots | 2,473 across 30 gaps |
| Non-success NTSTATUS among captured calls | 0 |
| CFDC2110 | 14,742 |
| CFDC2138 | 3,633 |
| DMA returned DWORD differs from requested bytes | 0 |
| Standalone 85FB/0x01 status | 720 |
| Status enable mask | 0x02BF |
| Status pending 0x0080 | 718 |
| Status pending 0x0200 | **2** |
| Family-0/0x88 acknowledge with mask 0x0200 | **2** |
| Family-1/0x82 | **2** |
| Family-0/0x4A | 2 (startup / reinsertion) |
| Family-1/0x4A | 2 (startup / reinsertion) |

**Relative timeline** (10-MHz trace tick conversion, from first captured IOCTL;
precise physical-action timestamps were not separately recorded):

- ~13.908 s / seq 509: startup family-1/0x4A identifies AP015, response
  `Information=270`, captured output prefix contains ASCII `AP015`.
- ~28.422 s / seq 14951: standalone 85FB/0x01 returns
  `000000000400BF020002` (enabled 0x02BF, **pending 0x0200**).
- ~28.449 s / seq 14952: real family-0/0x88 mask-0x0200 acknowledge.
- ~28.480 s / seq 14961: family-1/0x82 (Information 412); no subsequent
  0x4A metadata query in this first event sequence. This is consistent
  with the user's first action: AP015 removal.
- ~31.275 s / seq 16639: standalone 85FB/0x01 again pending **0x0200**.
- ~31.299 s / seq 16640: corresponding 0x88 mask-0x0200 acknowledge.
- ~31.330 s / seq 16651: family-1/0x82 (Information 412).
- ~31.469 s / seq 16678: family-0/0x4A setup.
- ~31.515 s / seq 16679: family-1/0x4A again identifies AP015,
  `Information=270`; the first 128 captured response bytes are byte-identical
  to startup seq 509 and to the legacy AP015 metadata prefix in seq
  14596/14812. The uncaptured remaining 142 bytes are not compared.

All 3,633 captured CFDC2138 calls report NTSTATUS success,
`Information=4` and a returned requested byte count. Recorded DMA calls:
2,332 before event 1, 328 between the events, 973 after event 2; the
nearest cross-event DMA gaps are ~36 ms and ~397 ms. DMA resumes and
continues to trace end (~39.58 s). The last captured transfer >=8 KiB is
at ~27.067 s; the later 1,024-/2,048-byte pattern starts *before* the
first pending-0x0200 event. The user changed the probe's A/div setting
before disconnecting, but the exact trigger for transfer-size changes is
not independently timestamped. The kernel JSONL does not certify on-screen
waveform pixels, instantaneous CPU usage or raw ISR/DPC rate. The two
0x0200 events do not indicate a repeating probe-event storm; do not treat
snapshot omissions as proof that no unrecorded call ever failed.

The successful post-patch capture plus user-visible XStream behavior
supports closing the original **missing AP015 hotplug notification** bug
for the tested disconnect/reconnect scenario. Keep the patch intact.

## Second post-HWInt scope trace: 231656 (before 3490709)

The user ran another x64 session with AP015 preconnected, executed Degauss
(and the subsequent automatic Auto Zero), opened/closed the clamp, opened
it again, and closed it in the locked position. The uploaded capture is
`xstream_trace_20260928_231656.jsonl`, **not** committed publicly.
Its first and last captured IOCTL seq are 1..39998, 35,458 actual entries;
39 trace-snapshot gaps omit 4,540 sequence numbers. All captured calls
have `STATUS_SUCCESS`. There are 27,100 CFDC2110 calls, 6,701 CFDC2138
calls and zero returned-byte-count mismatches. CFDC2138's requested
DWORD is at **input offset 11**, not token DWORD offset 0.

The 1,438 standalone 85FB/0x01 status queries all report enable mask
`0x02BF`: pending exactly 0x0080 = 1,432; 0x0200 = 4; 0x0280 = 1;
0x0000 = 1. Therefore **five** real pending status events include 0x0200.
All five are acknowledged by family-0/0x88 and followed by family-1/0x82.
Timeline relative to first captured IOCTL (10-MHz timestamp):

| Event/time | Status seq/pending | 0x88 seq/mask | 0x82 seq/pair | User-action correlation |
|---|---|---|---|---|
| ~34.163 s | 19902 / 0200 | 19903 / 0200 | 19916 / `12 00 A7 00` | Post-Degauss; clamp still closed |
| ~41.812 s | 24430 / **0280** | 24431 / **0280** | 24443 / `12 00 58 00` | First open |
| ~57.832 s | 33777 / 0200 | 33778 / 0200 | 33787 / `12 00 A7 00` | First close |
| ~59.820 s | 34908 / 0200 | 34909 / 0200 | 34918 / `12 00 58 00` | Second open |
| ~61.894 s | 36079 / 0200 | 36080 / 0200 | 36089 / `12 00 A7 00` | Locked close |

The observed `0x0058` and `0x00A7` state words correlate with opened
and closed clamp respectively, but the bit-field and a separate lock bit
have **not** been statically decoded. After seq 24431 ack 0x0280,
seq 24437 returns pending 0 and seq 24438 acknowledges mask 0.
No trace snapshot gap falls immediately across any of these five events.
The last DMA IOCTL occurs near t=68.177 s; the 6,701 captured returns
all match the requested size. Do not confuse IOCTL return success with
pixel-level waveform proof, and do not claim CPU/ISR counts from JSONL.

### Persistent special-function difference versus original x86

In trace 231656, family-0/0x4A Degauss-associated request `...47 00`
appears **five times** (seq 19883..19887), all with response
`00000000000000000000040000000000`; the immediate family-1/0x4A
status is `...F700` (seq 19895). Automatically invoked Auto Zero
request `...47 12` occurs five times at seq 19938,19939,19941..19943
(status F100) and again in four five-request bursts after the jaw events
(status F700/F300/F700/F300), **25 occurrences total**.

The byte-identical legacy original 47 00 request at seq 24205 and the
legacy 47 12 requests at seq 14895 and 14900 return
`0000000000000000000002000000FFFF`; the comparable legacy 0x4A status
is `...F200`. Existing x64 prepatch trace 193741 already had the
same repeated 47 00 replies; the restoration of authentic HWInt 0x0200
alone does not change this response-format issue.

Original legacy family-1/0x82 answers use actual raw-response length
`0x0006` or `0x0016` and trailing 0xFF padding; the previous x64
host wrapper advertises full capacity `0x0190` and zero pads. All have
aggregate `IoStatus.Information=412`, so that number is **not** the
firmware reply count.

### Exact source-proven cause and targeted patch

Original `ghidra_exports/selected/000167f4_FUN_000167f4.c`:
allocates the entire requested output record and fills it `0xFF`;
then invokes `FUN_0001619A` with destination at result+6 and record
payload capacity (record size minus six) and obtains `actual_received`
as an out-param. On success it writes `actual_received` (not capacity)
as WORD at result+4 and retains FF padding. `FUN_000169B4` copies the
whole record. The x64 wrapper already has `pendingResponseLength` from
`LecTransportReceive`, but its old raw branch wrote `recordOutput-6`
and SystemBuffer was initialized to zeros.

**Driver commit `34907090820a582ac360d02120316c8792d7c888`** changes
only raw-hardware 85FB output serialization in `driver/Ioctl.c`: prefill
raw result record with 0xFF; use actual copied byte count
`min(pendingResponseLength, recordOutput-6)` for header WORD+4, leaving
unused bytes as FF. The established family-1/0x99 override stays in place.
There is no change to actual BAR access, device IRQ, HWInt, receive polling,
DMA, MAM, or the original firmware command packet. **Unbuilt/untested.**

## First test after raw 85FB host formatting patch: 233125

Scope trace: `xstream_trace_20260928_233125.jsonl` (private user upload,
**not committed**). User sequence: start XStream with AP015 attached,
Degauss, then manually trigger Auto Zero, then open/close the clamp
several times, exit. This is a hardware run of driver patch
`34907090820a582ac360d02120316c8792d7c888`, *not* the older
pre-fix trace `231656`.

Observed 55.628 seconds, 27,947 captured IOCTLs (seq 1..30856),
2,909 unobserved sequence entries in 29 trace-snapshot gaps, **none
after t=28.646 s**. Zero non-success NTSTATUS among captured IOCTLs.
21,366 CFDC2110, 5,219 CFDC2138; all DMA returns match requested byte
count from CFDC2138 input offset **11** and acquisition IOCTLs continue
until ~55.627 s. There are 1,666 successful DMA calls after the second
special `47 00` request (~40.675 s). AP015 startup metadata at seq 507,
Information 270, has the same captured first 128 bytes as `230614`,
`231656` and the original x86 legacy trace.

**Framing patch succeeds for the exact legacy `47 00` packet:**

| Trace | Same 47 00 input | Returned complete output |
|---|---|---|
| Original x86 183409, seq 24205 | one captured | `0000000000000000000002000000FFFF` |
| Pre-fix x64 231656, seq 19883..19887 | five near-duplicate requests | `00000000000000000000040000000000` |
| New x64 233125, seq **8573** (~20.034 s) | first user-action correlation | `0000000000000000000002000000FFFF` |
| New x64 233125, seq **22156** (~40.675 s) | second user-action correlation | `0000000000000000000002000000FFFF` |

The two new requests are ~20.64 s apart and correlate respectively
with Degauss and the user's later **manual** Auto Zero. Unlike the
earlier automatic follow-up/jaw-event run, new capture has **zero
`47 12` requests**. Hence a blanket claim that 47 12 is every Auto
Zero trigger is inaccurate. The old five-retry bursts do not recur
for 47 00 in this run. The actual full 16-byte reply parity establishes
the original `FUN_000167F4` actual-received-length and FF-tail fix
for this particular request, *not* the still-unobserved post-fix 47 12
or 0x82 reply shapes.

The following identical family-1/0x4A `...01 0A` status requests
occur at seq 8574 (~20.066 s) and 22158 (~40.706 s), both returning
`0000000000000000000004000000F300`. The legacy matched examples
gave `...F200`. The F3/F2 one-bit payload difference persists after
the host serializer correction. Firmware meaning, physical calibration
outcome and exact probe state are unproven. Do not falsify this board
status to force legacy parity.

**Independent missing-notification observation:** all **1,135**
standalone 85FB/0x01 status reads report enable mask 0x02BF and
pending **only 0x0080**; no 0x0200/0x0280, no family-1/0x82, no
0x88 acknowledgement for mask 0x0200. There are **447 status reads
after t=40 s and no snapshot gaps after t=28.646 s**, despite the
reported several physical clamp opening/closing operations. This is
different from trace 231656, which recorded five real 0x0200-bearing
notifications during an otherwise similar physical sequence. The
fix in 3490709 changes only host-side raw reply serialization, with no
direct IRQ, INTEN, HWInt, firmware send, DMA or PCI modifications.
Whether changed host control flow/calibration state indirectly relates
to the missing notifications is **unknown**. This capture has neither
raw ISR counters nor physical action timestamps/screenshots. Do not
patch interrupts or hardware registers on assumption alone.

Detailed A/B evidence:
`docs/probus-calibration-ab-comparison.md` and `docs/runtime-trace.md`.

## Second post-formatter scope capture: jaw-state before/after calibration (235314)

The user confirmed performing exactly the prior handoff's controlled
protocol: with AP015 connected and XStream running, **open/hold/close/hold
BEFORE any Degauss or Auto Zero**, then Degauss and manually trigger Auto
Zero, then repeat open/hold/close/hold, and close the session. File
`xstream_trace_20260928_235314.jsonl` (user upload, **not** public).
Exact hand-action seconds and independent UI outcome were not supplied.

Measured 72.009226 seconds, 36,366 captured IOCTLs (seq 1..42227),
36 trace-snapshot gaps / 5,861 omitted entries; the last gap ends at
t~48.480 s. Zero recorded failing NTSTATUS. 27,834 CFDC2110,
6,818 CFDC2138, every DMA output DWORD equals its requested byte count
(input DWORD at offset 11), and transfers continue to t~72.008570 s.

**All 1,517 standalone 85FB/0x01 replies contain software enable
0x02BF and pending ONLY 0x0080.** No 0x0200 / 0x0280, no family-1/0x82,
and no family-0/0x88 with ack mask 0x0200.

| Deliberate protocol phase (time relative to first IOCTL) | Status reads | Containing pending 0x0200 | Recorded DMA replies |
|---|---:|---:|---:|
| AP015 identified, **before Degauss** (~17.000..40.156 s) | **622** | **0** | 3,207 |
| Degauss to manually invoked Auto Zero (~40.156..54.337 s) | **381** | **0** | 1,622 |
| After manual Auto Zero (~54.337..72.009 s) | **514** | **0** | 1,989 |
| Gap-free ending (~48.480..72.009 s; overlaps two rows above) | **648** | **0** | 2,569 |

First user-action-correlated family-0/0x4A `...47 00`: seq
**23386**, t~40.155959, response
`0000000000000000000002000000FFFF`, follow-up 0x4A status
**F3** at seq 23388. Second `...47 00`: seq **31835**,
t~54.336670, identical **byte-for-byte original-x86-matching**
response, follow-up status again F3 at seq 31836. They are ~14.181 s
apart; no previous five-attempt burst. The first 128 captured bytes
of startup family-1/0x99 also now **match original x86 including all
0xFF tail bytes**; pre-formatter x64 had wrongly zero-filled them.
AP015 is detected at startup family-1/0x4A seq 508,
Information=270; its captured metadata prefix matches `231656`.
No `47 12` packet in this manual procedure.

**Inference:** missing jaw notifications are not *necessarily*
caused by the Degauss/Auto Zero action: the no-event behavior was
already present before any calibration, unlike earlier pre-formatter
`231656` which reported genuine 0x0200 events. Two consecutive runs
after `3490709` show the difference; this temporal correlation does
not prove that FF padding or actual-response-length directly disables
hardware. Software command-enable mask 0x02BF must NOT be confused
with hardware BAR0 INTEN offset 0x084 and its receive-interrupt bit
0x08. The IOCTL JSONL does not expose physical INTEN, INTST, HWInt,
raw ISR/DPC rate, or UI jaw-state pixels. See the full A/B table in
`docs/probus-calibration-ab-comparison.md` and exact runtime events
in `docs/runtime-trace.md`.

## New decisive scope trace: physical AP015 hotplug on post-formatter driver (001152)

The user repeatedly unplugged/replugged the AP015 in one XStream session,
using unchanged driver after `3490709`. They report the first
reconnection was temporarily displayed as a different "1/2 clamp"
(possibly seating), whereas later reinsertions were recognized
correctly. Private trace:
`xstream_trace_20260929_001152.jsonl`. Elapsed time ~61.381146 s;
32,675 IOCTL entries with seq 1..37623, 34 trace-snapshot gaps omitting
4,948 entries, last gap ends ~32.792085 s. No captured NTSTATUS
failure. CFDC2110 24,976; CFDC2138 6,135, all return exactly the
requested bytes from input DWORD offset 11. Last DMA at ~61.380523 s
is 1,024/1,024; 1,062 DMA calls occur after the final insertion
notification. IOCTL success is not an independent pixel-level waveform
or CPU/ISR measurement.

Standalone 85FB/0x01: 1,361 queries, enabled mask **0x02BF** in
all of them, pending `0x0080` x 1,348, `0x0200` x 11,
`0x0280` x 1, `0x0000` x 1. **Twelve real 0x0200-bearing
notifications**, matched by family-0/0x88
(11 mask-0200, one mask-0280) and 12 family-1/0x82;
NINE occur after the final snapshot gap.

| Apparent physical action | t (s) | Status seq/pending | family-1/0x82 seq/raw final state WORD |
|---|---:|---|---|
| Unplug 1 | 27.557 | 16747/0200 | 16754/**03FF** (special 14-byte raw data) |
| Replug 1 | 31.662 + 31.718 | 19245/0200; 19249/0280 | 19248/**028C**; 19257/**0058** |
| Unplug 2 | 40.564 | 25308/0200 | 25318/**03FF** |
| Replug 2 | 42.336 + 42.386 | 26308/0200; 26320/0200 | 26318/**00A9**; 26325/**0058** |
| Unplug 3 | 44.929 | 27775/0200 | 27782/**03FF** |
| Replug 3 | 47.306 + 47.367 | 29173/0200; 29187/0200 | 29183/**00AA**; 29193/**0058** |
| Unplug 4 | 49.510 | 30380/0200 | 30390/**03FF** |
| Replug 4 | 52.275 + 52.405 | 32018/0200; 32062/0200 | 32021/**00A9**; 32065/**0058** |

These are correlated with hand-action order, not individually
timestamped hand actions or proven field-by-field bit meanings.
`03FF` consistently follows a removal. **First reinsertion
differs materially**: transient `028C`, combined pending `0280`,
then `0058`, with intervening status pending 0 / ack 0; XStream
does not request any post-event 270-byte AP015 metadata. This
correlates with the reported wrong "1/2 clamp" display, but there
is no evidence that `028C` literally means "1/2". Subsequent
three reinsertions trigger normal family-0/1 0x4A metadata:
seq 26342/26343, 29207/29208 and 32067/32068.
Together with startup seq 509, all four family-1/0x4A
Information=270 metadata replies contain "AP015" and have
byte-identical **captured first 128 bytes** (rest not captured).

**Post-3490709 reply-format validation is now complete for the
previously untested short 0x82 and 47 12 examples:**
0x82 seq 16754 has actual raw payload length 0x000E (14 bytes),
and the other eleven 0x82 replies have actual raw length 0x0006
(6 bytes). All unused captured record bytes are 0xFF, not the
pre-fix zero padding, and none advertises the old full capacity
0x0190. One and only one identical `...47 12` setup follows each
successful reidentification: seq **26385, 29250, 32096**;
all return the full legacy-x86-matching
`0000000000000000000002000000FFFF`, with follow-on family-1/0x4A
firmware/status F7 at seq 26386, 29251, 32099. No old
five-attempt 47 12 burst. Do not conflate these reidentification-
associated 47 12 requests with manually pressed Auto Zero,
which was correlated with 47 00 in traces 233125 and 235314.

**Engineering conclusion:** the repeated absence of spontaneous
jaw-only pending 0x0200 in post-formatter traces 233125/235314 is
not a global HWInt/INTEN-0x08 failure, since the exact same
formatter code in trace 001152 successfully handles four physical
unplug/replug cycles and twelve genuine notifications. No reason
to rollback `3490709`, synthesize events, or modify PCI/DMA.
The first reinsertion's transient state remains a separate,
non-reproduced cause-unknown observation. The meaning of F7
versus historical F2 and any actual physical calibration quality
also remain open.

## Latest correction: jaw-open warning observed and traced (002051)

The user explicitly reports XStream displays a not-locked warning
when the physical AP015 jaw is opened, cautioning that measurement
accuracy may be affected. Earlier assistant statements equating zero
captured standalone pending-0x0200 with "XStream does not recognize
the jaw" were **incorrect**. Preserve the counts in prior captures
as historical IOCTL observations but do not repeat the alleged UI
failure or infer an additional polling/status mechanism without
evidence.

Private real-scope trace:
`xstream_trace_20260929_002051.jsonl`, on the same `3490709`
corrected-response build, with **no further driver change**.
31,200 captured IOCTLs (seq 1..35955), 58.3684694 seconds,
32 snapshot gaps omitting 4,755 sequence entries, last gap ends
~23.382490 s; every following relevant event is gap-free. No
observed NTSTATUS failures. 23,896 CFDC2110, 5,839 CFDC2138,
all exact requested-vs-returned DMA byte counts (requested DWORD
at input byte offset 11), last 1,024-byte transfer
t~58.367987 s.

Standalone 85FB/0x01: 1,292 reads, software enabled mask 0x02BF;
pending `0x0080` x 1,287, `0x0200` x 3,
`0x0280` x 1, and `0x0000` x 1. Thus four authentic
notifications containing 0x0200, each with family-0/0x88
acknowledgement and family-1/0x82. Every 0x82 result returns
actual raw length `0x0006`, with 0xFF unused bytes:

| Time | Pending status/ack seq | 0x82 seq and raw status | Correlation |
|---:|---|---|---|
| ~24.623 s | 14888 / 14889, mask 0200 | 14891: `000012005800`, **0x0058** | Jaw-open, XStream unlocked warning |
| ~35.038 s | 21435 / 21436, mask 0280 | 21448: `00001200FE03`, **0x03FE** | Apparent removal-like status; pending briefly zero at seq 21442 |
| ~37.458 s | 22833 / 22834, mask 0200 | 22843: `000012005700`, **0x0057** | Reconnect-like transition and normal AP015 0x4A metadata seq 22877/22878 |
| ~49.861 s | 30553 / 30554, mask 0200 | 30563: `00001200A700`, **0x00A7** | Jaw-closed state |

The first opening-like event is followed by one family-0/0x4A
`47 12` request at seq 14913 -> exact original-x86-matching
`0000000000000000000002000000FFFF`, then family-1/0x4A
status seq 14916 ends **F7**. The final closing-like event
similarly has `47 12` at seq 30580 with exact response, then
status seq 30581 ends **F3**. The **same pairing** was previously
captured in `231656`: 0x0058/open -> F7;
0x00A7/closed -> F3, each repeated twice.
F7 XOR F3 = 0x04; the meaning of this exact bit has **not**
been statically recovered, so document it as a correlated
candidate, not a formal vendor register definition.

AP015 startup metadata seq 506 and subsequent metadata seq 22878
each have Information=270, and their captured first 128 bytes
are identical, including literal "AP015". The two other raw
0x82 words 03FE and 0057 differ by one from earlier
removal 03FF and open 0058 observations; no proven per-bit
meaning and no independently provided action timestamps.

**Outcome:** the jaw-unlock UI warning is real and the same
corrected driver can process genuine 0x0200/0x88/0x82 for
jaw-state transitions as well as physical reconnect.
No demonstrated problem justifies reverting `3490709` or
altering BAR/ISR/PCI/DMA. Earlier `233125` and `235314`
remain factually zero captured pending-0x0200, but the
previous interpretation as failed XStream warning/display
must be retired. See `docs/probus-calibration-ab-comparison.md`
and `docs/runtime-trace.md` for supporting packet details.

## Recovered and now exercised original path

- `FUN_0001619A -> FUN_000160A8(1)` enables BAR0 INTEN bit `0x08`
  before a real 85FB firmware response fetch; new code in
  `driver/Ioctl.c` restores this missing enable.
- ISR `FUN_000108D6` handles `INTST 0x08`, acknowledges source via
  BAR1 `CLRIRQ = 2`, records/acks INTST and schedules DPC.
- Original `FUN_00011390` assembly at `0x114A2..0x114C8` proves the
  actual call `FUN_000176A2(transport, &hwIntWord)`. Its decompiled C
  incorrectly shows a zero argument for the subsequent
  `FUN_000157A6(commandStatus, hwIntWord)`; trust the assembly.
- `FUN_000176A2` reads the low WORD of BAR1 HWInt `0x410` and clears
  a nonzero value by writing zero. `FUN_000157A6` then performs
  `pendingMask |= enabledMask & hwIntWord`. CFDC2180 event is signalled
  on a nonzero source word. New `driver/Acquisition.c` DPC mirrors this.
- The existing standalone 85FB/0x01 returns the enable/pending WORDs and
  family-0/0x88 acknowledges the same sticky status under
  `LegacyEventLock`. The genuine `0x0200` bit now drives user-mode
  0x82/0x4A naturally, without synthetic probe state or another poller.
- Solicited RX remains bounded/synchronous; single-channel MAM/CFDC2138,
  family-1 MTT/0x51, immediate IIMCL completion acknowledgement and
  existing INTST/CLRIRQ handling remain unchanged.

No additional PCI/DMA changes were needed. The earlier source-only review
identified a theoretical interleaving of full interrupt-mask commits around
the initial enable of bit 0x08 versus acquisition bit 0x01; the successful
focused test supplied no concrete reason to redesign that working path.
Do not refactor it speculatively.

## Historical comparison assets

Four files in the previously uploaded ZIP
`lecroy_probus_handoff_20260928(1).zip` (none committed to GitHub):

1. `legacy_xstream_trace_20260928_183409_probus_original.jsonl`:
   original x86 hotplug, three pending-0x0200 notifications at seq
   14552/14588/14616; opcode-0x88 acks seq 14553/14589/14620;
   family-1/0x82 and family-0/1 opcode-0x4A AP015 metadata.
2. `xstream_trace_20260928_183807_probus_x64.jsonl`:
   pre-patch x64 hotplug, no pending-0x0200, no recognition.
3. `xstream_trace_20260928_193741.jsonl`:
   pre-patch x64 AP015 already connected at startup, recognized by 0x4A;
   Degauss and Auto Zero requests present, but one result differs.
4. `xstream_trace_20260928_213834.jsonl`:
   pre-patch AP015 connected, then removed/reinserted; no removal
   notification; no pending-0x0200; DMA continues yet waveform disappeared.

Full historical A/B data, including the new post-patch trace markers, are
in `docs/probus-calibration-ab-comparison.md`. The separate old Ch2
calibration traces are optional: each newly visited V/div setting calibrates
once and then caches, so this is not an active bug.

## Current engineering decision: no further jaw regression patch or repeat test

The user's XStream warning and trace `002051` **resolve the premise**
that the mechanical jaw was no longer recognized. The latest trace
has both genuine open-like 0x0058/F7 and closed-like 0x00A7/F3
sequences on the corrected host response serializer, along with
a separate physical reconnect and unchanged acquisition. Do not
demand another generic physical hotplug/jaw exercise to reproduce
an alleged UI malfunction which the user has contradicted.

Retain driver commits `70716ba` (HWInt recovery) and `3490709`
(actual received-length and 0xFF raw reply padding). Do not change
IRQ, PCI, MAM or DMA without a concrete new failing observation.
A separate protocol-research topic remains: decode the actual
meaning of the changing family-1/0x4A F7/F3 status byte,
0x82 probe-state words (0x0058/0x00A7; 0x03FE/0x03FF;
0x0057/0x0058), and the transient wrong-probe first insertion
in `001152`. Compare original x86 and x64 packets, avoid making
up firmware meanings, and log screen-observed effects when available.
Existing safety constraints: <=5% scope CPU, no synthetic pending
0x0200, preserve below-4-GiB descriptor checks and gated transfer
IOCTL variants.

## Reusable scope workflow

Scope repo: `C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver`.
Use an **elevated** PowerShell window.

For a separate build-only check (do not install if build fails):

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
Set-Location "C:\Users\LeCroyUser\Git\lecroy_wr6k_64bit_driver"
git pull --ff-only origin main
if ($LASTEXITCODE -ne 0) { throw "Git update failed." }
& ".\scripts\build-driver.ps1" -Configuration Debug
```

For an authorized scope-side XStream hardware retest, the user keeps the
following local helper. It pulls, builds, test-signs, installs/reloads the
driver, captures the IOCTL stream and attempts NAS trace copy; the local
trace remains available if NAS copy fails:

```powershell
Set-ExecutionPolicy -Scope Process Bypass -Force
& "C:\Users\LeCroyUser\Desktop\Run-LeCroy-XStream-Trace.ps1" -Configuration Debug
```

User action timings are **not** automatically annotated in JSONL.
Request them when correlating physical transitions. Never commit proprietary
original EXE/DLL/SYS or the user's private raw scope traces into this public
repo. Keep `AGENTS.md`, this handoff file, README and relevant detailed docs
in sync with further verified findings.
