# LeCroy S65 / WR6k PCI FPGA firmware analysis

**Scope:** Spartan-IIE `XC2S200E-6PQ208` on PCI card
`900890-00`, XStream x64 device-pack update image; analytical
observations only. **Do not publish firmware/bitstream image bytes,
vendor binaries, schematics or license data.**

**Current milestone:** The installed PCI update image is decoded to
XC2S200E configuration frames and the `INTA#` output-enable cone has
been traced through configured routing to a concrete **SLICE[1] G LUT
at CLB X11/Y8**. All four LUT inputs have been traced to specific CLB
logic sources; one input is a registered state bit at **X7/Y10**, and
that state flip-flop is now independently proven to run from the
**PCI CLK input on U3 pin 185 / GCLKPAD3 / GCLK3**.

**Important:** The update image is not proof of the currently
running firmware revision, and an IRQ does not establish DMA bus idle.

## Photographic assembly evidence and configuration extraction path (2026-10-06)

**Source:** user-supplied photographs of a physically removed PCI interface
card, front and back, plus the WaveRunner 6000 Series Service Manual,
version B, August 2004, uploaded privately. Only derived component
identification is recorded here; original photographs, identifiers and the
manual are not redistributed.

- The photographed PCI assembly label reads `900890-00`. Its PCI endpoint
  is visibly a `XC2S200E-6` in a `PQ208` package, consistent with the
  separately analyzed PCI electrical schematic. The PCB shows unpopulated
  header `J4`, which the schematic names JTAG.
- The `U6` configuration-PROM footprint is **visibly unpopulated**. This
  directly rules out simply reading an on-card `XC18V02` from this pictured
  assembly. The schematic specifies `R60/R63/R66` versus `R88/R89`
  population choices for onboard PROM versus **remote configuration via
  link**. The missing U6 strongly supports the link-configured variant;
  resistor population and actual upstream bitstream provenance still need
  electrical/documentary confirmation.
- The manufacturer manual's section **6.3.4, PDF page 114** presents a
  `LeCroy S65 Hardware Programmer` UI with **three distinct version and
  upgrade rows**: `Microcontroller`, `Acq/Atc Fpgas`, and `Pci Fpga`.
  Section 6.3.5 (PDF page 115) names `hwprogrammer.exe` as the tool for
  checking versions. This proves there is a managed PCI FPGA firmware
  update mechanism, but neither that its update image is a separately
  stored `.bit` file nor that upgrades are readable/reversible.
  The manual explicitly warns that interrupted programming can require
  factory recovery. Do **not** run firmware upgrades to obtain evidence.
- The manual §4.6.1.4 (PDF p30) states that the acquisition
  microcontroller programs acquisition-board Spartan-IIE FPGAs.
  Given remote PCI-card configuration, the PCI bitstream may be stored
  in the acquisition-side non-volatile memory or supplied/updated using
  host application data, but **its actual storage and routing are unproved**.
- Xilinx `DS077` and `XAPP176` document Spartan-IIE configuration
  readback through JTAG/boundary scan (`CFG_OUT`). `XAPP176` §Security
  says `Level1` prohibits external readback and `Level2` prohibits
  external configuration and readback. The external JTAG port remains
  accessible in principle but that is *not* proof that the fitted card
  permits readback. JTAG interaction is active signaling, not a passive
  observation; any procedure requires a separate risk assessment,
  independent hardware power, verified J4 pinout and a controlled,
  recoverable test specimen, never the sole licensed production board.
- A recovered configuration stream is **not RTL, Verilog or VHDL**:
  it encodes placement/routing/LUT/control data (and readback may also
  include stateful data). A bitstream alone does not certify
  DMA/posted-write drain or completion IRQ ordering. Static inspection
  of installed `hwprogrammer.exe`, firmware files and FPGA update
  resources is the safest immediate research path.

**Preferred next steps:** (1) read-only file inventory on the existing
Windows LeCroy installation for `hwprogrammer.exe` and associated
versioned firmware assets; (2) **offline** binary/resource inspection of
copies kept private; (3) trace firmware update/control code statically
only, never executing an update; (4) corroborate JTAG/configuration
routing from the existing complete PCI schematic and manual; (5) only
after a spare/recoverable test setup, consider approved readback testing.

Sources:
- [Xilinx Spartan-IIE datasheet DS077](https://docs.amd.com/v/u/en-US/ds077)
- [Xilinx configuration and readback XAPP176](https://docs.amd.com/v/u/en-US/xapp176)
- [LeCroy WaveRunner 6000 service manual (online reference)](https://www.manualslib.com/manual/2455899/Lecroy-Waverunner-6000-Series.html?page=114)

## Installed x64 XStream firmware-file inventory (2026-10-06)

A read-only Windows PowerShell enumeration of four LeCroy installation roots,
supplied by the project owner, indexed 2,432 files (overlapping roots were
deduplicated by full path). No binaries were uploaded to or committed to this
public repository. This is an installed-file **filename/size inventory**, not
PE static analysis, firmware execution, device readback or an exhaustive scan
of the entire Windows volume.

Directly confirmed XStream filenames (sizes in bytes):

| Candidate | Size | Evidence and limits |
|---|---:|---|
| `hwprogrammer.exe` | 69,208 | Present; manufacturer service manual names the hardware programmer interface; precise implementation not yet inspected. |
| `xstreamhwprogrammer.exe` | 59,992 | Present; relationship to legacy programmer not yet inspected. |
| `s65hwupgrade.dll` | 295,512 | **Primary platform-specific update candidate**, on filename grounds only. |
| `s65devicepacksvr.dll` | 1,854,040 | S65-specific support; possible image-provider relationship unknown. |
| `aladdinhwupgrade.dll` | 325,720 | Alternate/general hardware upgrade component; S65 applicability unproven. |
| `lecaladdinhwaccesspcisvr.dll` | 336,472 | Existing host-PCI component previously analyzed in `lecaladdinhwaccesspcisvr-analysis.md`; not yet linked to bitstream delivery. |
| `ConfigMgrSvr.bin` | 256 | Filename not enough to assign to FPGA config. |
| `MSI.bin` | 13,904,780 | Filename not enough to assign to FPGA config. |
| `MSO.bit` and `MSO_Mag.bit` | 1,171,502 each | Not named for S65/WR6k PCI; no evidence they target the XC2S200E. |

The visible `.mcs`/`.rbf` assets are named for `LabMaster10`,
`GTX`, `GTX2`, `Hennessey`, `Mag`, `Mag12`, or `Yater`
serial trigger FPGA families. **None is established as the photographed
XC2S200E PCI image.** This does **not** prove the PCI image is absent:
it may be a PE resource, library-packed bytes, a private updater format,
located in an unscanned directory, or acquired from a separate storage source.
No current evidence identifies the actual PCI bitstream.

Next **offline/passive-only** step: metadata (file sizes, digital signatures,
SHA-256, PE versions), ASCII/UTF-16 string references, PE resources and
linkage among `s65hwupgrade.dll`, `hwprogrammer.exe`,
`xstreamhwprogrammer.exe`, `s65devicepacksvr.dll` and existing
hardware-access DLL. Do not execute a programmer, update firmware, read
JTAG, invoke hardware-write APIs or publish binary images. Analyze only
locally saved copies of vendor files. Do not infer DMA drain behavior
from presence of a firmware-upgrade component.

## S65 update components: verified static strings (2026-10-06)

**Source:** owner-provided read-only ASCII/UTF-16LE string and PE-version
report for six installed x64 XStream files, all build version
`0.23.66.84`. This is *string evidence*, not a recovered implementation
or extracted firmware. The third-party DLLs were not uploaded or run;
only derived descriptions appear here.

- `s65hwupgrade.dll` (295,512 bytes, SHA-256
  `0a1b6ee7d64dd6c7dd4a8117b801886be0b4e6366969e4176d675c3ee3011ec1`)
  identifies `CS65HwProgrammer`, three upgrade targets
  (microcontroller, Acq FPGAs, PCI FPGA), `UpgradePciFpga`, and
  explicit **two PCI image sources**: *using device pack* / *using file*.
  It also mentions `FlashDrv.cpp`, flash erase/programming failures
  and JTAG-chain errors. These do not yet establish which physical
  storage is erased/programmed for PCI FPGA upgrades.
- `s65devicepacksvr.dll` (1,854,040 bytes, SHA-256
  `4eb12f74466d7e7cc6232651b346cc08d07901f58b558a293bc8f1a89bdbe9d`)
  contains `PciFpgaResId`, `PciFpgaVerResId`, and messages
  `Could not load Pci Fpga resource data.` /
  `Pci Fpga resource data invalid.`. This is strong evidence of
  resource-ID-driven PCI image lookup, **not proof** the actual bytes
  are stored inside this DLL rather than an auxiliary module/resource.
- `hwprogrammer.exe` is a UI (69,208 bytes) with PCI FPGA version and
  update warnings. `xstreamhwprogrammer.exe` (59,992 bytes) refers to
  `S65HwProgrammer`/`S65HwProgrammer2` and PCI FPGA status.
- `aladdinhwupgrade.dll` references broader generations and PCI
  FPGA JTAG/flash operations, including the string
  `JTAG programation is not supported by this version of the PCI card`.
  This is **not** evidence that the owner's S65 hardware has that
  limitation: the DLL is not proven to select the same code path.
- `lecaladdinhwaccesspcisvr.dll` contains link-reset and DMA diagnostic
  strings; it is not yet established as part of the S65 updater.
  Updater/programmer linkage needs independent static disassembly or
  resource-reference analysis.

**Research priority:** inspect PE resource *metadata only* (resource
types, names/IDs, sizes, version selectors) in `s65devicepacksvr.dll`
and `s65hwupgrade.dll`, without executing either library, copying
bitstreams into a public repository, extracting license contents or
performing JTAG/MMIO/programming. A confirmed resource table can identify
candidate PCI images for later **private** offline study. Even an identified
or extracted bitstream does not establish an RTL-level PCI bus-idle proof.

## Confirmed embedded configuration images in S65 device pack (2026-10-06)

The owner provided a **private local copy** of the installed
`s65devicepacksvr.dll` (PE32/i386, 1,854,040 bytes, SHA-256
`4eb12f74466d7e7cc6232651b346cc08d07901f58b558a293bc8f1a89bdbe9d`).
It was analyzed *offline* by a read-only PE resource parser without
DLL execution, firmware writes, JTAG access or any scope interaction.
The complete binary and extracted image data are **not** stored in
this public repository.

The DLL has three custom `BINARY` resources in locale ID `1033`:

| PE resource | Encoded bytes (Intel HEX ASCII) | Valid decoded data bytes | Evidence |
|---|---:|---:|---|
| `BINARY/203` | 139,676 | 56,792 | Intel HEX, 1,840 data records plus extended-address records, one EOF |
| `BINARY/204` | 1,014,084 | 360,520 | Intel HEX, two distinct Xilinx sync sequences at decoded offsets 4 and 180,264; 2 × 180,260-byte contiguous streams |
| `BINARY/205` | 507,026 | 180,252 | Intel HEX, one Xilinx sync sequence at decoded offset 4; 180,252 contiguous bytes |

**All records have valid Intel-HEX lengths and checksums, and all three
resources have a single EOF record.** Both FPGA-like resources begin with
dummy words followed by `55 99 AA 66`, consistent with the encoding
used in the installed Xilinx configuration streams. None of the binary
data needs hardware readback to obtain its private offline copy.

The manufacturer Xilinx Spartan-IIE datasheet `DS077` Table 8, and
application note `XAPP176` Table 4, specify **1,442,016 configuration
bits for XC2S200E**. This is exactly **180,252 bytes**, matching resource
`205` after Intel-HEX decoding. The device installed on the photographed
PCI card is `XC2S200E-6 PQ208`. This is **strong evidence** that
`BINARY/205` is the PCI-FPGA image. Two independent, same-family
configuration sync points in `204` strongly suggest a combined
Acquisition/ATC FPGA image; `203` is structurally unlike FPGA data
and is a likely microcontroller flash image. The three image-source assignments have now been confirmed as
the **default resource IDs** registered by the S65 device-pack
component, as documented immediately below. The user-configurable
resource-ID CVARs may nevertheless be overridden at runtime.

**Remaining follow-up:** Privately inspect the PCI image's Xilinx
configuration commands and placement details, and identify whether the
running card's firmware revision matches the installed update image.
The registered default resource IDs do not establish that the running
device uses that exact revision. No firmware loader or upgrade should be
called. The bitstream is not RTL or proof of DMA completion/PCI drain.

References:
- [Xilinx DS077, configuration-file size table](https://home.agh.edu.pl/~jamro/xsb/spartan2E.pdf)
- [Xilinx XAPP176, frame/data format](https://docs.amd.com/api/khub/documents/Hcm12rAU9l9qlRD43FOX3g/content)

## Confirmed S65 device-pack resource-ID mapping and loader references (2026-10-06)

A second offline-only disassembly examined the owner-provided
`s65hwupgrade.dll` (295,512 bytes; SHA-256
`0a1b6ee7d64dd6c7dd4a8117b801886be0b4e6366969e4176d675c3ee3011ec1`)
alongside the previously analyzed, identical-hash
`s65devicepacksvr.dll`. Neither was executed or copied into the repo.

**Confirmed defaults in `s65devicepacksvr.dll` (PE32 image base
`0x10000000`):**

| x86 instruction VA | Immediate | CVAR string | PE resource | Decoded data size |
|---|---|---|---|---:|
| `0x100047F6` | `push 0xCB` | `MicroResId` (`0x1001A304`) | `BINARY/203/1033` | 56,792 |
| `0x100048FD` | `push 0xCC` | `AcqFpgaResId` (`0x1001A32C`) | `BINARY/204/1033` | 360,520 |
| `0x10004A04` | `push 0xCD` | `PciFpgaResId` (`0x1001A35C`) | `BINARY/205/1033` | 180,252 |

The immediate values above are registered **CVAR defaults**, not
immutable hardcoded values. Dynamic configuration can potentially
override them. Three resource retrieval paths in the device-pack
binary call Win32 `FindResourceA` at `0x100074E1`,
`0x100075CA` and `0x100076BC`, respectively. Each supplies the
literal resource type `BINARY` (string VA `0x1001A4C4`) and follows
with `LoadResource`. Nearby error strings identify the microcontroller,
Acq FPGA and PCI FPGA paths, respectively.

`s65hwupgrade.dll` includes a second generic, resource-ID-driven loader
that calls `LoadLibraryA` (`0x10006675`), `FindResourceA`
(`0x10006688`) and `LoadResource` (`0x10006692`), with resource type
`BINARY` at `0x100257E8`. Its user-visible strings separately
advertise PCI FPGA upgrading **using a device pack** or **using a file**.
This corroborates the update-image selection chain, but *does not*
prove the specific firmware bytes currently running in the production
device, nor establish the bitstream-to-hardware programming transport.

The same read-only parser independently rechecked the resource payload
integrity:

- `BINARY/203`: 1,968 Intel-HEX records, one valid EOF, 56,792
  decoded bytes in multiple address ranges.
- `BINARY/204`: 22,540 records, valid checksums, contiguous 360,520
  decoded bytes; two Xilinx sync markers at offsets 4 and 180,264.
- `BINARY/205`: 11,270 records, valid checksums, contiguous 180,252
  decoded bytes; one Xilinx sync marker at offset 4.

**Conclusion:** `205` is explicitly the installed device pack's
default `PciFpgaResId`, not merely a size-based guess. The decoded
180,252-byte image also has the expected XC2S200E configuration length.
This identifies a **PCI FPGA update image** for this XStream release,
but determining the actual *live* firmware revision requires additional
safe evidence. Preserve the private payloads offline; no reflash or JTAG
on the only functional licensed PCI card.

### Device-pack image version strings (read-only verification)

The same `s65devicepacksvr.dll` constructor also registers default
`MicroVerResId = 101` (`0x100043E6`), `AcqFpgaVerResId = 102`
(`0x100044EA`), `PciFpgaVerResId = 103` (`0x100046F2`),
and `AcqAtcFpgaVerResId = 104` (`0x100045EE`).

The corresponding *PE STRING-table* entries (block `7`, language
`1033`) decode exactly as follows:

| STRING ID | Registered role | Exact string |
|---|---|---|
| 101 | Microcontroller | `00.03#02` |
| 102 | Acq FPGA | `00.03#00` |
| 103 | **PCI FPGA** | **`00.02#00`** |
| 104 | Acq/ATC FPGA | `00.18#00` |

These identify **the installed XStream device-pack's advertised
firmware versions**, not verified revisions running on the
production oscilloscope. Neither these version strings nor the
successfully decoded update images prove device-internal bus-idle
semantics, and firmware upgrades remain prohibited on the sole
functional device.

## Spartan-IIE bitstream packet/frame baseline: private offline analysis (2026-10-06)

**Scope and provenance:** The previously recovered `BINARY/205`
(PCI update image), and both embedded streams in `BINARY/204`
(Acquisition/ATC update image), were parsed **strictly offline**.
Original binaries/bitstreams remain private and are **not** part
of the public Git history. Analysis did not load Windows vendor
DLLs, execute an updater, probe JTAG, or use PCI BARs.

**Bit-order discovery:** The packed Intel-HEX-decoded bytes represent
**bit-reversed bytes**, not the canonical big-endian packet encoding
printed in Xilinx `XAPP176`. Reversing bits **within each byte**
yields:

- Raw byte sync `55 99 AA 66` at `BINARY/205` offset `0x4`
  becoming documented `AA 99 55 66`.
- Following command header becomes `0x30008001`, followed by
  `RCRC=7`; `FLR=0x11` (18 32-bit words/576 bits per frame),
  `COR=0x00813D2D`, `MASK=0`, `SWITCH=9`,
  `FAR=0`, `WCFG=1`; trailer includes `LFRM=3`,
  `START=5`. The `COR` internal field meaning is not decoded.

**Complete PCI configuration structural check:**

| Feature | Observed |
|---|---|
| `BINARY/205` decoded size | **180,252 bytes** |
| Bitstream SHA-256 (decoded but still bit-reversed per-byte) | `f2195ae57bb87bb3ef6ea27cc70771c451d1be6de972a87b7e861be6dcd970ab` |
| Parsed header + payload | **45,063 × 32-bit words** |
| FDRI payload | **45,018 × 32-bit words**, i.e. **2,501 × 18-word transfer slots (72 bytes per slot; 540 configuration bits and 36 pad bits per slot)** |
| First Type-2 FDRI block | **40,338 words = 2,241 frames** |
| Subsequent FDRI blocks | **1,170 + 1,170 + 1,170 + 1,152 + 18 words = 260 frames** |
| Subsequent FAR selectors | `0x02020000`, `0x02040000`, `0x02060000`, `0x02080000` |
| Leading/intermediate commands | `RCRC`, `FLR`, `COR`, `MASK`, `SWITCH`, `FAR`, `WCFG` |
| Trailing commands | `CRC`, `LFRM`, final `FDRI`, `START`, `CTL`, final `CRC` |
| Parser validity | Consumes entire `BINARY/205` with no unsupported/truncated packets |
| All-zero 72-byte frame payloads | **271 / 2,501** (descriptive only, not a resource-utilization statistic) |

The first (2,241-frame) span is the **CLB frame** region described
in Xilinx XAPP176; later blocks correspond to its Block-RAM address
regions. The split and the `0x50009D92` Type-2 header match
`XAPP176` Table 16 for `XC2S200E` exactly.

**Comparison:** `BINARY/204` contains two separate, structurally
valid XC2S200E-size streams with `FLR=0x11`, each having
2,501 FDRI frames and the same FDRI packet lengths. The first
stream's `COR=0x008B3D2D`, the second stream's
`COR=0x01C05E55`. They have, respectively, 207 and 281
all-zero 72-byte frames. Versus the `205` PCI image,
2,250 and 2,184 frame payloads differ. **There is an
additional two-word inter-stream region** after the first
204 bitstream's trailing words, before the second stream's
dummy/sync preamble. Its semantics are not yet resolved:
do not treat the combined file as an immediately programmable
`*.bit` image or infer netlist similarity from identical chip geometry.

**Limits:** Successful packet decoding yields frame data but **not
placed-and-routed FPGA logic, LUT equations, net connectivity,
signal names, PCI state machines or DMA guarantees**. The two
CRC fields were read but not independently recalculated with
Xilinx's polynomial. Reconstructing DMA control logic would
require a validated XC2S200E bit-to-tile/LUT/IOB/PIP mapping
and backtracking the physically known PCI/LVDS pins.
The tools/data for later Xilinx 7-Series and UltraScale
architectures do not automatically support this older FPGA.

**Next research priorities (offline-only):**

1. Correlate PCI physical pin list from the `900890-00` schematic
   and PCI PnP enumeration to a suitable *architecture-specific*
   tile/IOB/CLB mapping. Seek legacy Xilinx ISE/JBits-compatible
   architecture databases or verified academic work, checking
   whether `XC2S200E` is actually supported; avoid assuming
   any newer FPGA decoder is compatible.
2. Where possible, construct **synthetic, separate development-board
   test bitstreams** for the same FPGA part using legacy ISE tools
   and compare changed configuration bits for individual LUTs,
   IOBs, routing PIPs and sequential logic. Do not alter or program
   the production LeCroy board and do not publish its actual
   proprietary bitstream.
3. Derive the PCI DMA/interrupt *logical* requirements from the
   existing Windows x86 driver disassembly and bus-level
   evidence; use FPGA reverse engineering to verify narrow
   hypotheses (e.g. what makes `IIMST` change), not to assert
   bus-idle without proof.
4. Keep Windows x64 `UnknownActive` quarantine/fail-closed
   behavior as-is. Neither a conventional IRQ nor a status bit
   by itself establishes that all posted PCI DMA writes have drained.

Primary configuration reference:
[Xilinx XAPP176, §§Bitstream Format, Configuration Registers and
Readback](https://docs.amd.com/v/u/en-US/xapp176).


### Verified bitstream-to-device coordinate mapping (Project Combine)

**Independent open reference:** [Project Combine](https://github.com/prjunnamed/prjcombine),
`databases/virtex.txt` / `public/virtex/src/{chip.rs,expand.rs,expanded.rs}`.
This family database explicitly lists the exact `xc2s200e-pq208` part,
speed grade `-6`. `CHIP18` is a **Virtex-E-compatible** architecture
model with **48 columns × 30 rows**, main columns for BRAM at
`X1/X14/X33/X46`, 8 central spine frames, and `BOND87` PQ208
package mappings. Its modeled disabled resources include the primary
DLLs and BRAM at `X14/X33`; these modeled disables have *not* been
validated for each physical LeCroy board. Project Combine's published
database is a research reconstruction, not vendor source RTL or a
verified replacement FPGA image.

**Precisely reconciled frame totals:**

| Transfer area | Actual geometry frames | Zero dummy/padding frames | Observed 18-word slots |
|---|---:|---:|---:|
| Central spine (8), 42 normal columns × 48 (2016), 4 BRAM columns × 27 (108), 2 edge I/O columns × 54 (108) | **2240** | **1** | **2241** |
| 4 BRAM-data blocks × 64 | **256** | **4** | **260** |
| **Total** | **2496** | **5** | **2501** |

This count comes independently from the project geometry expansion in
`public/virtex/src/expand.rs` (`fill_frame_info`) and from the
private LeCroy FDRI packet lengths. The five dummy transfer slots are
all-zero in the LeCroy stream: the final slot in the 2241-slot main
region, then one at the end of each 65-slot BRAM region. A 18-word
transfer slot encodes a **540-bit** effective configuration frame
(`rows × 18`): its final DWORD is zero and the low 4 bits of its
17th DWORD are zero for all 2,501 observed transfer slots. The
previous shorthand “2,501 configuration frames of 576 bits” meant
packet-aligned slots, not 2,501 real device frames; this table
supersedes that interpretation.

**Package-to-resource mapping** (from `BOND87`; exact board-level nets
and configured features require *separate* schematic correlation):

| PQ208 pin | Candidate FPGA IOB | Config frame span (0-based within main FDRI payload) | 18-bit row slice (0-based) |
|---|---|---|---|
| `P24` | `IOB_W15_3` | **2186..2239** | **270..287** |
| `P27` | `IOB_W14_1` | **2186..2239** | **252..269** |
| `P129` | `IOB_E14_1` | **2132..2185** | **252..269** |
| `P132` | `IOB_E15_3` | **2132..2185** | **270..287** |

Those four package pins have PCI-compatible alternate `IRDY`/`TRDY` pad capabilities. Independent correlation to the original LeCroy electrical schematic identifies **P24=IRDY# and P27=TRDY#**; P129/P132 are not the corresponding wired PCI handshake nets. See the verified PCI-net table below. The row
slices follow `btile_main` from `expanded.rs`: one tile occupies
18 bits for its row; edge I/O columns occupy 54 consecutive frames.
Additional package-level dedicated clock pad pins: `P77` (`CLK5`),
`P80` (`CLK4`), `P182` (`CLK1`), `P185` (`CLK0`).

Project Combine also lists architecture-specific IOB/LUT/interconnect
feature bitfields in `databases/virtex.txt`, including `IOB_W_VE`
feature patterns. Their polarities and layout should be cross-checked
against a known synthetic ISE reference before drawing conclusions
about **actual** LeCroy signal direction, net routing or DMA logic.
The internal PCI protocol state machine, interrupt timing, outstanding
PCI master cycles and PCI bridge posted-write drain remain unproved.

**Follow-up without hardware access:** Select exact PCI physical
pins from the already-held `900890-00` schematic, correlate to
`BOND87`, inspect their `BitRect` and primitive/mux features in
this offline bitstream using Project Combine's database and decoder,
then trace neighboring interconnect features stepwise. Any ambiguous
feature or actual PCI bus-idle criterion remains explicitly unknown.
Vendor binary/bitstream and schematic material must remain private.

References:
- [Project Combine family DB text](https://github.com/prjunnamed/prjcombine/blob/main/databases/virtex.txt)
- [Project Combine frame layout](https://github.com/prjunnamed/prjcombine/blob/main/public/virtex/src/expand.rs)
- [Project Combine IOB-to-frame bit rectangles](https://github.com/prjunnamed/prjcombine/blob/main/public/virtex/src/expanded.rs)
- [Project Combine PCI/package bonds](https://github.com/prjunnamed/prjcombine/blob/main/public/virtex/src/bond.rs)


### PCI pin-to-pad evidence and interpretation limits

The original private electrical schematic correlates the
`XC2S200E-6PQ208` package pins with these signals:
`REQ#=P18`, `GNT#=P22`, `STOP#=P23`,
`IRDY#=P24`, `TRDY#=P27`, `FRAME#=P29`,
`INTA#=P30`. Project Combine's `BOND87` specifies
the corresponding physical IOB positions, as listed
below. These are verified **external wiring and package
bonds**, not statements about internal DMA logic.

Earlier exploratory pad-mode and `REQ#` output
descriptions depended on provisional FDRI word/feature-bit
interpretations. They are not accepted as validated
netlist evidence. The `INTA#` pad and routing selections
were independently recomputed from the correct frame
word indexing in the section immediately below.
Additional pads require the same verification before
publishing any live mux, inversion, FF or tristate
settings.

### Validated PCI INTA# output-enable path and excluded local drivers

The analysis uses the **PCI update image**, not a live FPGA
readback. All private LeCroy firmware and schematic files were inspected
offline, without running any firmware loader or touching the working
licensed board. Data are interpreted with the *actual* Project Combine
`xc2s200e` Virtex-E frame geometry: west I/O major starts at frame
**2186**, each row has **18 effective configuration bits**.

Electrical drawing -> device-package bond:

| PCI net | U3 PQ208 pin | Project Combine pad |
|---|---:|---|
| `REQ#` | `18` | `IOB_W18_2` |
| `GNT#` | `22` | `IOB_W16_2` |
| `STOP#` | `23` | `IOB_W15_2` |
| `IRDY#` | `24` | `IOB_W15_3` |
| `TRDY#` | `27` | `IOB_W14_1` |
| `FRAME#` | `29` | `IOB_W13_2` |
| `INTA#` | `30` | `IOB_W12_3` |

For `INTA#` (west I/O column `X0`, row `Y12`,
`IOI[3]`), the following **specific selections** were
recomputed directly from the installed image, with 32-bit
FDRI word placement matching `insert_virtex_frame`.
The bit patterns below preserve the order of bit positions in
the Project Combine `virtex.txt` feature definition;
do not treat the first listed position as the low-order bit.

| Feature | Configuration field, hex-free bit pattern | Selection |
|---|---|---|
| IOB `IBUF_MODE` | `11` | Input buffer disabled |
| `IOI[3].MUX_O` / `MUX_T` | `0` / `0` | Direct O and T paths, no output register |
| `IMUX_IO_O[3]` | `0000000011` | `PULLUP` |
| `IOI[3]` output-data inversion `@!MAIN[0][14]` | `0` | Inverted constant 1 -> driven low |
| `IMUX_IO_T[3]` | `001000` | **`SINGLE_E_BUF[3]`** |
| `SINGLE_E_BUF[3]` | permanent buffer | `SINGLE_E[3]` |
| `SINGLE_E[3] = HEX_V6[0]` `@MAIN[39][8]` | `0` | Local programmable pass not selected |
| `SINGLE_E[3] = OUT_TBUF_W[3]` `@MAIN[41][8]` | `0` | Local programmable pass not selected |

The correct direction on the **west I/O** tile is **`SINGLE_E`**.
Previous scratch interpretations naming `SINGLE_W` here used
the *east* I/O tile and were wrong. Likewise, earlier
scratch notes used an off-by-one FDRI word index while
reading raw frame bits; the table above was independently
recomputed with the correct 540-bit-to-17-word packing,
including the four unused low bits of the final partial
word, and checked against `IMUX_IO_T[3]`'s unique
`001000` enumerated selection. Previous speculative
internal path claims based on those mistaken bit indices
must not be relied upon.

**Constrained electrical path:**

```text
PCI INTA# (U3 pin 30)  [0 or Hi-Z]
  <- IOB_W12_3 output enable T
  <- west IOI[3] IMUX_IO_T[3]
  <- SINGLE_E_BUF[3] = SINGLE_E[3]
  <- west-to-east PASS connection (X1 is skipped for main fabric)
  <- X2,Y12 SINGLE_W[3], with X1 BRAM_W output taps on this net
  <- [actual enabled upstream driver NOT YET ESTABLISHED]
```

The local west-edge candidates `HEX_V6[0]` and
`OUT_TBUF_W[3]` are **both deselected** at this
row. They are not upstream sources of this interrupt
control net. This is an actual configuration-bit
filter, not merely a list of potential routing paths.

The `X1` column is the **intervening BRAM column**, not
the next main-fabric connector endpoint. Project Combine's
`fill_main_passes` skips BRAM columns when connecting
`PASS_W`/`PASS_E` between main-fabric cells. The west
I/O column `X0,Y12` is therefore paired with `X2,Y12`
across `X1`, while `BRAM_W` has its own programmable
output taps into the `X0` west-cell wire. The active
source search is tracked in the next section. Do **not**
confuse similarly numbered `SINGLE_W[3]` tracks in
different rows/tiles.

**Safety limitation:** Nothing above establishes
DMA termination, descriptor-read completion, a
busmaster-idle acknowledgement, or upstream posted-write
drain. `INTA#` and completion IRQ alone remain insufficient
to free DMA mappings and pinned buffers. No source changes
or hardware experiments are authorized by this finding.

Project Combine reference:
- [west I/O programmable interconnect: `IO_W`, `IMUX_IO_T`, `pass SINGLE_E`](https://github.com/prjunnamed/prjcombine/blob/main/databases/virtex.txt)
- [Frame decoding](https://github.com/prjunnamed/prjcombine/blob/main/public/xilinx-bitstream/src/parse.rs)
- [Tile geometry and interconnect](https://github.com/prjunnamed/prjcombine/blob/main/public/virtex/src/expanded.rs)

### Reconstructed INTA# control cone to a concrete CLB LUT

The earlier six-candidate negative search was incomplete because it
did not include all configured `bipass` alternatives on the
Project Combine switchbox graph. It is superseded by the route below.

Using the Project Combine `resolve_wire`/connector semantics and
re-reading every selected mux/pass bit from the private
`BINARY/205` update image, the `INTA#` output-enable route is:

```text
PCI INTA# (U3 P30; low or Hi-Z)
 <- X0,Y12 IOB_W12_3 T
 <- X0,Y12 SINGLE_E[3]
 <- X2,Y12 SINGLE_W[3]
 <- X2,Y12 SINGLE_N[22]
 <- X2,Y13 SINGLE_S[22]
 <- X2,Y14 SINGLE_S[22]
 <- X2,Y14 SINGLE_W[20]
 <- X2,Y14 SINGLE_N[19]
 <- X2,Y14 HEX_W3[3]
 <- ... horizontal branch ...
 <- X5,Y14 HEX_W0[3]  -> HEX_W6[3]
 <- ... horizontal branch ...
 <- X11,Y14 HEX_W0[3] -> HEX_N6[3]
 <- ... vertical branch ...
 <- X11,Y8 HEX_N0[3] -> OMUX[7]
 <- X11,Y8 OUT_CLB_Y[1]
 <- X11,Y8 SLICE[1] G LUT
```

Representative configured decisions along that path:

| Location | Configured decision |
|---|---|
| `X2,Y12 SINGLE_W[3]` | `SINGLE_N[22]` bipass enabled |
| `X2,Y13 SINGLE_S[22]` | north/south continuation enabled |
| `X2,Y14 SINGLE_S[22]` | `SINGLE_W[20]` bipass enabled |
| `X2,Y14 SINGLE_W[20]` | `SINGLE_N[19]` bipass enabled |
| `X2,Y14 SINGLE_N[19]` | inverted-polarity `HEX_W3[3]` pass enabled |
| `X5,Y14 HEX_W0[3]` | source `HEX_W6[3]` |
| `X11,Y14 HEX_W0[3]` | source `HEX_N6[3]` |
| `X11,Y8 HEX_N0[3]` | source `OMUX[7]` |
| `X11,Y8 OMUX[7]` | source `OUT_CLB_Y[1]` |
| `X11,Y8 SLICE[1].GYMUX` | direct G-LUT result |

This is the first recovered path that reaches a concrete logic primitive
rather than stopping at an interconnect wire.

#### Interrupt LUT inputs

The same private update image configures the four inputs of
`X11,Y8 SLICE[1] G` as:

| G input | Local mux selection | Recovered source |
|---|---|---|
| `G1` | `OMUX_W6` | **X12,Y8 SLICE[1] F LUT** |
| `G2` | `SINGLE_E_BUF[9]` | **X13,Y8 SLICE[0] G LUT** |
| `G3` | `SINGLE_E_BUF[18]` | **X16,Y7 SLICE[1] G LUT** |
| `G4` | `SINGLE_N_BUF[14]` | **X7,Y10 SLICE[1] XQ flip-flop** |

The G-LUT's recovered logical INIT vector, after applying the database's
per-bit inversion markers, is:

```text
1111111100000100
```

All four inputs affect this truth table. Under the conventional Xilinx
LUT input-index convention it corresponds to
`!G4 OR (G1 & !G2 & G3)`; keep that Boolean form **provisional**
until the old-family LUT input-order convention is independently checked.
The raw 16-bit logical vector and source connectivity above do not depend
on that Boolean simplification.

### PCI-clocked state input into the interrupt LUT

The `G4` input comes from `X7,Y10 SLICE[1].XQ`.
Its recovered sequential configuration is:

- `DXMUX = BX`: the flip-flop data input is the routed `BX` input,
  not the local combinational X output.
- `BX = SINGLE_S_BUF[12]`.
- `CLK = GCLK_LEAF[3]`.
- `CE = SINGLE_N_BUF[23]`.
- `SR = HEX_V0[1]`.
- `FF_LATCH=0`, `FF_SR_ENABLE=1`, `FF_REV_ENABLE=0`,
  `FF_SR_SYNC=0`.

The **clock-domain identity is independently established**, not inferred
from a GCLK number:

1. The private LeCroy PCI schematic routes conventional PCI `CLK`
   through a PI5C3861 bus switch onto net `CLK_BUF`.
2. Vector connectivity in that drawing connects `CLK_BUF` directly to
   U3 **pin 185**, whose Xilinx symbol name is `GCK3_185`.
3. Project Combine `BOND87` maps P185 to `CLK0`.
4. Project Combine's raw-device converter shows that raw pad
   `GCLKPAD3` becomes `BondPad::Clk(0)`.
5. Its Virtex naming layer maps north-clock-tile
   `GCLK_IOB[1]` to **`GCLKPAD3`** and `BUFGCE[1]` to
   **`GCLKBUF3`**.
6. In the private update image, north-clock-tile
   `IMUX_BUFGCE_CLK[1]` is `00000000001`, selecting
   **`OUT_CLKPAD[1]`**. `GCLK_IOB[1]` is configured as a CMOS input.
7. The same tile permanently maps `BUFGCE[1].O` to
   **`GCLK[3]`**, then to `GCLK_LEAF[3]`.
8. `BUFGCE[1]` CE's source mux selects `PULLUP`
   (`0000000`); its dedicated inversion configuration bit is zero.

Therefore the `X7,Y10` state bit feeding the interrupt LUT is in the
**PCI clock domain** in this installed update image.

For comparison, U3 P182 is the other north global-clock pad
(`GCK2_182` / Project Combine `CLK1` / raw `GCLKPAD2`);
the electrical drawing routes it from the acquisition-link
`RX_CLOCK_N` net, not PCI `CLK_BUF`.

### Evidence boundary

This is strong evidence that the physical PCI interrupt output is
generated by a small logic cone containing at least one **PCI-clocked
state register** plus three combinational-LUT inputs. It still does not
prove what event that state bit represents.

In particular, none of the recovered signals is yet identified as:

- descriptor fetch complete;
- PCI request deasserted/accepted;
- DMA write FIFO empty;
- no outstanding target/master transaction;
- link producer stopped;
- or upstream host-bridge posted writes drained.

Consequently, `INTA#` assertion/deassertion remains **insufficient**
to authorize DMA mapping/page release.

### Pending interrupt-cone work

The `X7,Y10 SLICE[1]` state flip-flop's **BX data source, CE and SR**
and the three combinational sources feeding the interrupt LUT's
`G1..G3` remain open. This work is still required, but the discovery
of the configured hard `PCILOGIC` block makes its input cone and
`PCI_CE` fanout the more direct PCI-busmaster lead.

References:

- [Project Combine Virtex switchbox database](https://github.com/prjunnamed/prjcombine/blob/main/databases/virtex.txt)
- [Project Combine wire-tree resolver](https://github.com/prjunnamed/prjcombine/blob/main/public/interconnect/src/grid.rs)
- [Project Combine Virtex naming, including GCLKPAD/GCLKBUF mapping](https://github.com/prjunnamed/prjcombine/blob/main/re/xilinx/naming/virtex/src/lib.rs)
- [Project Combine Virtex bond reconstruction](https://github.com/prjunnamed/prjcombine/blob/main/re/xilinx/rd2db/virtex/src/bond.rs)


### Hard PCI timing block is configured in the installed update image

Project Combine models a dedicated west-side `PCILOGIC` BEL at
`X0,Y13` for the `xc2s200e` Virtex-E architecture. Its three
fabric inputs are selected through `IMUX_PCI_I1/I2/I3`; the BEL
outputs the regional `PCI_CE` signal used by the device's PCI
timing resources.

The private `BINARY/205` update image was decoded at the exact
`PCI_W_VE` bit rectangle (`X0` frame base 2186, row 13).
The configured mux values are:

| PCILOGIC input | Raw mux field | Selected routing wire |
|---|---|---|
| `I1` | `1010000` | `HEX_V5[3]` |
| `I2` | `1000001` | `HEX_V1[3]` |
| `I3` | `0001` | `HEX_V4[1]` |

The companion input-polarity bits at `MAIN[52][3]` and
`MAIN[53][3]` are both set in the update image. Their final
logical inversion meaning has not yet been independently
validated and is therefore not interpreted here.

This is the first direct evidence that the installed PCI FPGA image
uses the device's dedicated **hard PCI timing/clock-enable block**,
rather than implementing all PCI timing purely in ordinary LUT fabric.
It narrows the busmaster investigation substantially: the next trace
target is no longer the whole west-edge routing fabric, but the three
specific vertical nets `HEX_V5[3]`, `HEX_V1[3]`, and
`HEX_V4[1]` feeding `PCILOGIC`.

**What this proves:** a configured hard PCI timing block and its three
selected fabric inputs in the installed update image.

**What it does not prove:** the semantic role of those three inputs,
which physical PCI handshake/control nets feed them, whether `PCI_CE`
means DMA idle, or whether all initiated/posted PCI writes have drained.
No safe DMA-unmap criterion follows yet.

**Status:** the selected `PCILOGIC` input nets and the installed
`PCI_CE` fanout are traced in the following section. Their semantic
connection to PCI arbitration/transaction state remains open.

References:
- [Project Combine Virtex feature database](https://github.com/prjunnamed/prjcombine/blob/main/databases/virtex.txt)
- [Project Combine PCILOGIC placement](https://github.com/prjunnamed/prjcombine/blob/main/public/virtex/src/expand.rs)


### PCILOGIC fabric inputs terminate at concrete G LUTs

The three configured west-side `PCILOGIC` fabric inputs were traced
backward through the **active** Project Combine routing graph in the
installed `BINARY/205` image. All three terminate at ordinary CLB
G-LUT outputs rather than directly at package inputs:

| PCILOGIC input | Configured entry wire | Active route summary | Concrete source |
|---|---|---|---|
| `I1` | `X0,Y13 HEX_V5[3]` | root `X0,Y11 HEX_V3[3]` -> active `X0,Y14 HEX_V6[3]` endpoint -> `LV[6]` tree -> `SINGLE_E_BUF[5]` -> `X2,Y14 SINGLE_W[5]` -> `OMUX[1]` -> `OUT_CLB_Y[1]` | **`X2,Y14 SLICE[1] G`** |
| `I2` | `X0,Y13 HEX_V1[3]` | root `X0,Y15 HEX_V3[3]` -> active `X0,Y18 HEX_V6[3]` endpoint -> same long-line tree, driven at `X0,Y12 LV[0]` -> `SINGLE_E_BUF[22]` -> `X2,Y12 SINGLE_W[22]` -> `OMUX[7]` -> `OUT_CLB_Y[0]` | **`X2,Y12 SLICE[0] G`** |
| `I3` | `X0,Y13 HEX_V4[1]` | root `X0,Y12 HEX_V3[1]` -> active `X0,Y9 HEX_V0[1]` endpoint -> `LV[0]` -> `SINGLE_E_BUF[22]` -> `X2,Y9 SINGLE_W[22]` -> `HEX_N6[3]` tree -> `X2,Y3 HEX_N0[3]` -> `OMUX[7]` -> `OUT_CLB_Y[0]` | **`X2,Y3 SLICE[0] G`** |

At the final CLBs, the configured `GYMUX` selection is direct `G`,
not a registered or alternate source. The decoded G-LUT attribute
vectors are:

- `I1` source, `X2,Y14 SLICE[1] G`: `1111000011111111`;
- `I2` source, `X2,Y12 SLICE[0] G`: `1111111111111010`;
- `I3` source, `X2,Y3 SLICE[0] G`: `1010111110101111`.

These vectors are recorded as configuration evidence only. No Boolean
meaning is assigned until the old-family LUT input-order convention and
the four input routes of each LUT have been independently checked.

This replaces the earlier broad `HEX_V*` search boundary with three
specific combinational cones in the main fabric.

### PCI_CE drives 29 registered PCI datapath outputs

The installed image uses the `PCILOGIC.PCI_CE` output as an actual
I/O output-register clock enable, not merely as an instantiated but
unused hard-block output.

In the west I/O column, 17 `IMUX_IO_OCE` instances select the
dedicated regional `PCI_CE` source directly. Every corresponding
`MUX_O` selects `FFO`, proving that these are registered-output
clock-enable consumers. Package/schematic correlation identifies them
as:

| Package pins | PCI nets |
|---|---|
| P4, P5, P6, P7, P8, P9, P10 | `AD22..AD16` |
| P11, P15, P16, P17 | `AD15..AD12` |
| P33, P34, P35, P36 | `C/BE3#..C/BE0#` |
| P42, P43 | `AD11..AD10` |

Both west-side corner injection PIPs are also enabled:

- southwest: `PCI_CE -> HEX_H0[3]`;
- northwest: `PCI_CE -> HEX_H0[3]`.

The southwest branch spans its first horizontal HEX segment and does
not continue past `X7`. Four bonded I/O output registers select that
CE branch, all with `MUX_O = FFO`:

- P55 = `AD3`;
- P56 = `AD2`;
- P57 = `AD1`;
- P58 = `AD0`.

The northwest branch is repeated once at `X7` and stops after the
second segment at `X13`. Eight bonded output registers select it, all
with `MUX_O = FFO`:

- P199 = `AD31`;
- P200 = `AD30`;
- P201 = `AD29`;
- P202 = `AD28`;
- P203 = `AD27`;
- P204 = `AD26`;
- P205 = `AD25`;
- P206 = `AD24`.

Therefore the installed `PCI_CE` network has **29 proven functional
FFO clock-enable consumers**:

- `AD31..AD24`: 8;
- `AD22..AD10`: 13;
- `C/BE3#..C/BE0#`: 4;
- `AD3..AD0`: 4.

`AD23` and `AD9..AD4` are not among these recovered
`PCI_CE`-gated output registers. This statement does **not** imply
that those nets are unregistered or unused; it only describes the
decoded `PCI_CE` fanout.

The active north/south CE segments were also checked against their
available `ICE`, `OCE`, and `TCE` muxes. Only the `OCE`
selections listed above use the CE branch; no active `ICE` or
`TCE` selection was found.

One additional configured north-edge routing stub exists:

```text
PCI_CE
  -> X4,Y29 HEX_H3[3]
  -> SINGLE_S[22]
  -> X4,Y28 SINGLE_N[22]
  -> SINGLE_E[18]
  -> X5,Y28 SINGLE_W[18]
```

At `X5,Y28`, no CLB input mux selects that `SINGLE_W[18]` signal
and no further bypass is enabled. The branch therefore terminates
without a configured logic/state consumer and is not counted among the
29 functional sinks.

The result matches the architectural purpose of the hard PCI timing
resource: `PCI_CE` is demonstrably part of the registered PCI
datapath-output timing path. It is **not** an idle indication. Its
assertion/deassertion cannot by itself prove that descriptor reads,
DMA writes, internal FIFOs, remote producers, or upstream posted writes
have drained.

**What this proves:** each configurable `PCILOGIC` fabric input now
has a concrete CLB G-LUT source, and `PCI_CE` is proven to gate 29
registered PCI AD/CBE output paths in the installed image.

**What it does not prove:** the semantic meaning of the three G-LUT
conditions, the state of the hard block's dedicated PCI handshake
inputs, the role of `REQ#/GNT#`, or any safe DMA-unmap/bus-idle
predicate.

**Next analysis:** decode and trace the four inputs of
`X2,Y14 SLICE[1] G`, `X2,Y12 SLICE[0] G`, and
`X2,Y3 SLICE[0] G` to identify the three `PCILOGIC` conditions.
Then reconstruct `REQ#`/`GNT#` and `FRAME#`/`IRDY#`/`TRDY#`
and compare those transaction-state cones with the already recovered
interrupt state and, only afterward, with `IIMCL/IIMST`,
`INTST`, `SGTA/IIMTC`, and the legacy SG descriptor flow.

References:

- [Project Combine Virtex feature database, pinned analysis reference](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt)
- [Project Combine Virtex wire-tree definitions, pinned analysis reference](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/public/virtex/src/defs.rs)
- [Project Combine wire-tree resolver, pinned analysis reference](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/public/interconnect/src/grid.rs)


### PCILOGIC regional PCI_CE routing: database-verified scope

Project Combine at pinned commit `234343d23e737e57f2727630e19008b509d7d522`
was re-opened successfully through GitHub's REST contents endpoint, after the
ordinary file reader returned an empty body for the large
`databases/virtex.txt`. This is an independent *architecture database*
inspection, not a recovered active netlist.

- `PCI_W_VE` explicitly wires `PCILOGIC.I1/I2/I3` to
  `IMUX_PCI_I1/I2/I3` and the BEL output to regional `PCI_CE`
  (database lines 109202-109255). The previously decoded input
  selections `HEX_V5[3]`, `HEX_V1[3]` and `HEX_V4[1]` are valid
  enumerated selections for this exact Virtex-E tile class.
- In `public/virtex/src/expand.rs`, `fill_pcilogic` assigns the
  `PCI_CE` region root of every cell in each west/east edge column
  to that column's clock row (lines 307-323). This is regional edge
  distribution, **not** evidence of a DMA-completion or bus-idle signal.
- The feature database exposes potential `PCI_CE` destinations in
  edge routing: `HEX_H0[3]` programmable-buffer sources
  (`MAIN[7][0]` or `MAIN[9][0]`, dependent on edge tile class)
  and `HEX_H6[3]` (`MAIN[37][0]`). These are *possible*
  source-to-routing-wire taps. The corresponding feature bits have
  **not yet been checked per row against BINARY/205**, so no consumer
  or registered PCI output is claimed to be active.
- Other enumerated `PCI_CE` options occur in edge switchbox muxes.
  They likewise require per-tile bit decode and path traversal.
- The two `PCILOGIC.I1/I2` polarity-bit annotations use
  `@!MAIN[52][3]` and `@!MAIN[53][3]` syntax in the database.
  Their actual effective signal sense remains pending verification
  against the bit encoding and family semantics.

**Unresolved:** Trace the three selected `HEX_V*` nets to concrete
sources and validate each proposed `PCI_CE` tap against the actual
BINARY/205 frame bits. A routing possibility is not a configured
consumer, and neither constitutes a safe DMA-unmap predicate.

Sources:
- [Pinned Project Combine Virtex feature database](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt)
- [Pinned PCI region expansion](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/public/virtex/src/expand.rs)


### Configured west-edge PCI_CE output-register clock-enable selections

A fresh offline decode of private `BINARY/205`, with each byte bit-reversed
before parsing the Virtex configuration packets, identified the main FDRI
payload at offset 72 (`0x50009d92`: 40,338 32-bit words, or 2,241
18-word transfer slots). Frame bits were decoded using
`insert_virtex_frame` in pinned Project Combine: the 540-bit frame's
first 28 logical bits come from the final packed DWORD at bit offsets
4..31; subsequent 32-bit chunks are read in reverse DWORD order.
West I/O starts at main-frame slot 2,186, with tile bit address
`Y * 18 + local_bit`.

**Independent bit-addressing control:** At `X0,Y13`,
`PCILOGIC.I1=1010000` (`HEX_V5[3]`),
`I2=1000001` (`HEX_V1[3]`) and
`I3=0001` (`HEX_V4[1]`), matching the earlier milestone.
The I3 control bits reside at local bit 4, whereas I1/I2 are at
local bit 3. The I1 and I2 input-polarity configuration bits are both
1. This establishes a reproducible local frame-bit reader without
claiming semantic signal polarity.

The pinned database's `IO_W` switchbox defines
`IMUX_IO_OCE[0..3]` as six-bit mux fields, where `010001`
selects the regional `PCI_CE` net. These fields were independently
decoded for every `X0,Y0..Y29` west I/O row. The following *actual
configured selections* were observed:

| West tile row | OCE indices selecting PCI_CE |
|---|---|
| Y6 | 1 |
| Y7 | 3 |
| Y8 | 1, 2 |
| Y11 | 2, 3 |
| Y18 | 1 |
| Y21 | 1, 2, 3 |
| Y22 | 1, 3 |
| Y23 | 3 |
| Y24 | 1, 2 |
| Y25 | 3 |
| Y26 | 1 |

No `OCE[0]` selected `PCI_CE` in this decoded west-column
scan. All other OCE selections are other nets or `PULLUP`, not
`PCI_CE`. This table describes selected **I/O output-register
clock-enable muxes**, not proof that their corresponding output FFs
are enabled and clocked, that the package pins are connected to PCI
signals, or that the selected clock enable is an idle indication.

The database also exposes `PCI_CE` programmable taps on
`HEX_H0[3]` or `HEX_H6[3]` in **corner tile classes** (`CNR_SW`,
`CNR_NW`, `CNR_SE`, `CNR_NE`), not arbitrary west-edge
`IO_W` rows. The prior generic candidate description must not
be used as a claim of configured row-wise routing.

**Outstanding:** Correlate each configured OCE with the matching IOB
output-FF activation, physical package bond and schematic PCI net.
Trace the `HEX_V5[3]`, `HEX_V1[3]`, `HEX_V4[1]` input
drivers through active PIPs. No busmaster-idle, FIFO-empty,
posted-write-drain or DMA-unmap predicate follows.

Evidence: private LeCroy `BINARY/205` (not published);
[Project Combine feature fields](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt),
[Project Combine frame packing](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/public/xilinx-bitstream/src/parse.rs),
[main bit-rectangle geometry](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/public/virtex/src/expanded.rs).


### PCI_CE clock-enable muxes cross-checked against registered output selection

A second offline per-bit decode of private `BINARY/205` cross-checked every
previously reported `X0` west-edge `IMUX_IO_OCE[1..3]=PCI_CE`
selection against that same I/O's `IOI.MUX_O` output-path selection.
For all **17** configured PCI_CE OCE selections, `MUX_O=1`
selects `FFO` (registered output rather than direct combinational O).
This applies to the following exact (row, IOI) pairs:

```text
Y6:  1
Y7:  3
Y8:  1,2
Y11: 2,3
Y18: 1
Y21: 1,2,3
Y22: 1,3
Y23: 3
Y24: 1,2
Y25: 3
Y26: 1
```

`IO_W` feature definitions use `IOI[1].MUX_O=MAIN[40][16]`,
`IOI[2].MUX_O=MAIN[25][16]`, and
`IOI[3].MUX_O=MAIN[10][16]` (bit value 1 means `FFO`).
The complete local control test also reproduces at `X0,Y13`:
`PCILOGIC.I1=1010000`, `I2=1000001`,
`I3=0001`.

**Interpretation limit:** This verifies that all 17 selected OCE
inputs belong to **configured registered output data paths**. Further
work must still validate the OCLK source and FFO's actual operational
enable, map each IOB to the PQ208 package and PCI schematic nets,
and trace the PCILOGIC input cones. Registered output selection does
not establish any PCI DMA drain/idle predicate.

The calculation used exclusively a private, offline parsed
`BINARY/205`; no board access, writes, licensing data or bitstream
bytes were published.


### PCI handshake pin comparison: no direct PCI_CE OCE selection

The same offline `BINARY/205` bit reader was applied to the
electrical-schematic-to-package-to-IOB mappings for seven PCI pins:

| PCI net | West IOB | `IMUX_IO_OCE` | `IOI.MUX_O` |
|---|---|---|---|
| REQ# | `Y18 IOI[2]` | `000000` (PULLUP) | FFO |
| GNT# | `Y16 IOI[2]` | `000000` (PULLUP) | direct O |
| STOP# | `Y15 IOI[2]` | `000000` (PULLUP) | FFO |
| IRDY# | `Y15 IOI[3]` | `000000` (PULLUP) | FFO |
| TRDY# | `Y14 IOI[1]` | `000000` (PULLUP) | FFO |
| FRAME# | `Y13 IOI[2]` | `000010` (SINGLE_E_BUF[16]) | FFO |
| INTA# | `Y12 IOI[3]` | `000000` (PULLUP) | direct O |

**Negative but concrete finding:** None of these seven specific
IOI output-register clock-enable muxes directly selects `PCI_CE`.
For `GNT#`, an input, output path configuration is not evidence that
the output is actually enabled. Likewise `INTA#`'s direct O field
does not supersede the previously documented constant-low/output-
tristate behavior. Other PCI bus signals, including AD/CBE,
may still use `PCI_CE`; the 17 configured west-side clock enables
must be correlated to their actual package pins before concluding
which signals they gate.

This rules out the overly broad statement that `PCI_CE` directly
gates *these named handshake pins' OCE muxes*; it does not rule out
`PCI_CE` being involved elsewhere in the PCI master state machine,
nor establish DMA bus idle.


### Physical PCI bus mapping of the 17 configured PCI_CE registered outputs

A pin-by-pin correlation of the **17 previously verified**
`IO_W IMUX_IO_OCE=PCI_CE` and `MUX_O=FFO` occurrences with the
exact `xc2s200e-pq208` `BOND87` package map and the private
LeCroy assembly `900890-00` schematic establishes their physical
PCI bus connectivity:

| FPGA west IOI | U3 pin | PCI schematic net |
|---|---|---|
| X0,Y26 IOI[1] | P4 | AD22_BUF |
| X0,Y25 IOI[3] | P5 | AD21_BUF |
| X0,Y24 IOI[1] | P6 | AD20_BUF |
| X0,Y24 IOI[2] | P7 | AD19_BUF |
| X0,Y23 IOI[3] | P8 | AD18_BUF |
| X0,Y22 IOI[1] | P9 | AD17_BUF |
| X0,Y22 IOI[3] | P10 | AD16_BUF |
| X0,Y21 IOI[1] | P11 | AD15_BUF |
| X0,Y21 IOI[2] | P15 | AD14_BUF |
| X0,Y21 IOI[3] | P16 | AD13_BUF |
| X0,Y18 IOI[1] | P17 | AD12_BUF |
| X0,Y11 IOI[2] | P33 | CBE3#_BUF |
| X0,Y11 IOI[3] | P34 | CBE2#_BUF |
| X0,Y8 IOI[1] | P35 | CBE1#_BUF |
| X0,Y8 IOI[2] | P36 | CBE0#_BUF |
| X0,Y7 IOI[3] | P42 | AD11_BUF |
| X0,Y6 IOI[1] | P43 | AD10_BUF |

**Milestone:** The actual configured `PCI_CE` OCE+FFO paths have
now been tied to **PCI AD[10:22] and C/BE#[0:3] output
registers**, not merely anonymous regional routes. Both the device
package and source-card wiring are independently cross-checked.
This is evidence of PCI bus output-timing usage, not an identified
PCI transaction-state/idle signal.

The net-to-pin map was derived locally from the private schematic;
the proprietary schematic itself and configuration payload remain
private. Remaining uncertainty includes OCLK timing, output-enable
control, input provenance of `PCILOGIC.I1/I2/I3`, and the completion
conditions for busmaster DMA and posted host writes. Do not free
DMA mappings on the basis of this finding.

Public references:
- [Pinned Project Combine Virtex package database](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt)
- [Pinned Project Combine Virtex device geometry](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/public/virtex/src/expanded.rs)


### PCI clock confirmed for the PCI_CE-gated AD/CBE output registers

The private `BINARY/205` configuration was independently re-decoded
to examine `IO_W IMUX_IO_CLK[n]` for all 17 package pins correlated
above. All 17 local I/O clock muxes select **`GCLK_LEAF[3]`** with
exact 11-bit selection `00000111011`.

The control path for each is now confirmed at the configuration level:

```text
PCI CLK -> card CLK_BUF -> U3 P185
        -> GCLKPAD3 -> GCLKBUF3 -> GCLK_LEAF[3]
        -> IO_W IOI[n] OCLK
PCI_CE -> IO_W IMUX_IO_OCE[n]
        -> IOI[n] OCE
IOI[n] MUX_O=FFO -> PCI AD[10:22], C/BE#[0:3] physical pins
```

This is strong source-backed evidence that the dedicated PCILOGIC
clock-enable signal controls **PCI-clocked, registered output
data paths** for the 13 specified multiplexed AD bus lines and four
C/BE lines. The clock decoder was independently checked against the
known `PCILOGIC` fields at `X0,Y13`:
`1010000 / 1000001 / 0001`.

This does **not** establish the exact temporal relation between
PCILOGIC inputs and particular PCI transactions, the direction/drive
enable of every pad, any busmaster idle state, completion of all PCI
writes, or safe DMA unmap.

Reference: pinned Project Combine
[`IO_W` `IMUX_IO_CLK` definitions](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt);
board clock provenance documented earlier in this file.


### PCI_CE corner injection and HEX_V corner source bitfields

An additional read-only decode inspected the **configured west corner
tile classes** `CNR_SW` at `X0,Y0` and `CNR_NW` at `X0,Y29`
in the same private `BINARY/205` image. Both use Project Combine's
Virtex-E (not Spartan-II-specific `_S2`) corner definitions.

- Southwest corner `Y0`: `PCI_CE -> HEX_H0[3]` programmable
  tap at `MAIN[7][0]` is **enabled**; `HEX_V5[3]` mux
  field `01` selects `HEX_H0[3]` and its progbuf at
  `MAIN[12][2]` is **enabled**.
- Southwest corner `Y0`: `HEX_V1[3]` and `HEX_V4[1]`
  both have mux field `01`, but their output programmable buffers
  are **disabled**. Their mux fields alone do not constitute net drivers.
- Northwest corner `Y29`: `PCI_CE -> HEX_H0[3]` tap at
  `MAIN[9][0]` is **enabled**.
- Northwest corner `Y29`: `HEX_V1[3]` selects `LV[11]`
  (field `11`) and its progbuf is **enabled**.
  `HEX_V4[1]` selects `HEX_H3[1]` (field `01`) and
  `HEX_V5[3]` selects `HEX_H4[3]` (field `01`);
  their respective progbufs are **enabled**.

These are actual corner-routing configuration bits, but **do not yet
prove connectivity from either corner to the PCILOGIC I1/I2/I3
inputs at Y13**. Project Combine defines `HEX_V1` as north-directed
multi-branch and `HEX_V4/5` as south-directed multi-branch; wire
segment traversal and active intermediate routes are required before
attributing a corner's source to the Y13 PCI input. In particular,
do not infer that the southwest `PCI_CE` corner tap creates
feedback into `PCILOGIC.I1` merely because both touch
similarly numbered `HEX_V5[3]` resources at different rows.

No DMA bus-idle or posted-write-drain proof has been obtained.
Reference: pinned Project Combine `CNR_SW`, `CNR_NW` and
`HEX_V` wire definitions, plus private offline `BINARY/205`.


### Uniform PCI output-register control bits on the 17 PCILOGIC-gated pins

A further independent, private offline decode tested each of the **17
PCI AD/CBE IOI entries** for three local control fields beyond the
previously recorded `PCI_CE` OCE and `GCLK_LEAF[3]` clock selection.
Every one of the 17 has the **same observed raw bit pattern**:

| Local IO_W IOI field | Every tested PCI AD/CBE pin |
|---|---|
| `OCLK` optional inversion bit | `0` |
| `OCE` optional inversion bit | `1` |
| `MUX_T` (tristate path) | `0` = direct `T`, **not** `FFT` |
| `FFO_SR_ENABLE` | `1` |

The individual coordinates are `OCLK`:
`IOI[1] MAIN[33][14]`, `IOI[2] MAIN[14][14]`,
`IOI[3] MAIN[11][14]`;
`OCE`: `MAIN[14][10]`, `MAIN[13][9]`,
`MAIN[10][9]` respectively;
`MUX_T`: `MAIN[35][16]`, `MAIN[30][16]`,
`MAIN[5][16]`, respectively.
This confirms uniform treatment of those output paths within the
update-image configuration.

**Important limit:** `MUX_T=direct` shows the output tri-state
selection comes from the separate `T` fabric route, not an
output-enable flip-flop. It does **not** prove the pad is permanently
driven. The two `^...` optional inversion annotations must not
be assigned effective voltage polarity without validating Project
Combine's inversion-bit semantics for this device family. This raw
bit-table is not a DMA quiescence criterion.


### PCI_CE-gated AD/CBE tri-state sources decoded per pad

A further offline decode of private `BINARY/205` resolved
`IO_W.IMUX_IO_T[n]` for **all 17** previously mapped `PCI_CE`-
gated and PCI-clocked FFO outputs. The observed `MUX_T=direct T`
setting is paired with these exact configured tri-state input routes:

| Routed source | PCI signal(s) | Configuration encoding |
|---|---|---|
| `SINGLE_E_BUF[0]` | AD12, AD18, AD22 | `000001` |
| `SINGLE_E_BUF[2]` | AD10, AD11, AD13–AD17, AD19–AD21, C/BE#2, C/BE#3 | `000100` |
| `SINGLE_E_BUF[3]` | C/BE#0, C/BE#1 | `001000` |

All 17 local `IMUX_IO_T` fields resolved to one of the
above defined Project Combine selections, with no unknown encodings.
Their `IOI.MUX_T` bits were already verified as `0` (direct T).
This is concrete configuration-level evidence of **shared routing
classes for PCI pad output-enable control**, but identical wire names
in different IO_W tiles do **not** prove the same originating logic:
the switchbox row coordinates and programmed upstream connections
must be traversed individually.

This result does not establish the drive value/polarity of T, the
PCI transaction-state condition that enables each output, or an
idle/posted-write drain acknowledgement. No hardware access occurred.

Source: owner-provided private PCI image `BINARY/205`,
[Project Combine `IO_W.IMUX_IO_T`](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt).


### Registered tri-state control on named PCI handshake pins

An offline verification of seven electrically documented handshake
pins (private schematic + XC2S200E package bond) checked their
`IO_W.IMUX_IO_T`, `IOI.MUX_T` and `IOI.MUX_O` settings:

| Net | T-input mux | Output data MUX_O | Tristate MUX_T |
|---|---|---|---|
| REQ# | `PULLUP` | `FFO` | **`FFT`** |
| GNT# | `PULLUP` | direct `O` | direct `T` |
| STOP# | `SINGLE_E_BUF[2]` | `FFO` | **`FFT`** |
| IRDY# | `SINGLE_E_BUF[3]` | `FFO` | **`FFT`** |
| TRDY# | `SINGLE_E_BUF[3]` | `FFO` | **`FFT`** |
| FRAME# | `SINGLE_E_BUF[0]` | `FFO` | **`FFT`** |
| INTA# | `SINGLE_E_BUF[3]` | direct `O` | direct `T` |

For `REQ#`, `STOP#`, `IRDY#`, `TRDY#` and
`FRAME#`, both `MUX_O=1` (registered output data) and
`MUX_T=1` (registered tristate/output-enable) were observed.
Unlike the 17 previously decoded AD/CBE pins, these named PCI
control/handshake pins therefore have a configured **FFT**
tri-state path, with a separately clocked/stateful enable.
For `GNT#`, this is not evidence of driving the pin; the
electrical role is an input. `INTA#` retains its already
independently traced direct combinational tri-state route.

**Important:** `REQ#` selecting a PULLUP source to the
*input* of the FFT must not be interpreted as a static output-drive
state without analyzing register initialization, set/reset and clock
sequences. The registered tristate control must be traced before
deducing PCI busmaster ownership or bus-idle. No such completion
criterion follows yet.

Evidence: local read-only decode of owner-provided `BINARY/205`;
pinned Project Combine `IO_W` `IOI` and T-mux fields.


### PCI-clock timing and TCE gating for registered PCI handshake tristates

A further read-only bitfield check covered the five named handshake
nets previously verified to use `FFT` registered tristate paths:

| Net | IOI clock-mux selection (ICLK/OCLK/TCLK) | Tristate clock enable TCE |
|---|---|---|
| REQ# | `00000111011` = `GCLK_LEAF[3]` | `000000` = `PULLUP` |
| STOP# | `00000111011` = `GCLK_LEAF[3]` | `000000` = `PULLUP` |
| IRDY# | `00000111011` = `GCLK_LEAF[3]` | `000000` = `PULLUP` |
| TRDY# | `00000111011` = `GCLK_LEAF[3]` | `000000` = `PULLUP` |
| FRAME# | `00000111011` = `GCLK_LEAF[3]` | `000100` = `SINGLE_E_BUF[12]` |

`GCLK_LEAF[3]` was independently established as PCI CLK via
U3 P185 and the north BUFGCE mapping earlier in this document.
Thus the five `FFT` register clock paths are PCI-clocked.
The `TCE` routing on FRAME# notably differs from the four
other named outputs; its actual source remains to be traced.
A TCE mux selecting `PULLUP` is a constant clock-enable
source, not proof of a constant driven PCI output.

The selected `IMUX_IO_CLK` field is the **shared source**
for ICLK, OCLK and TCLK, but per-pin optional clock inversions
remain separate and should not be presumed identical.
This is an offline configuration fact, not a live transaction
trace and not a DMA-drain proof.


### PCILOGIC HEX_V input segment roots from Project Combine connector traversal

The west-side PCILOGIC tile is `X0,Y13` for the 30-row
Virtex-E device model. Using Project Combine's explicit
`HEX_V*` `MultiBranch N/S` declarations, adjacent-row
`PASS_N/PASS_S` connector mappings in `virtex.txt`, and
`resolve_wire`'s connector traversal in
`public/interconnect/src/grid.rs`, the **passive segment
resolution** of its three selected input wires is:

```text
I1 <- X0,Y13 HEX_V5[3]
   -> X0,Y12 HEX_V4[3]
   -> X0,Y11 HEX_V3[3]   (MultiRoot)

I2 <- X0,Y13 HEX_V1[3]
   -> X0,Y14 HEX_V2[3]
   -> X0,Y15 HEX_V3[3]   (MultiRoot)

I3 <- X0,Y13 HEX_V4[1]
   -> X0,Y12 HEX_V3[1]   (MultiRoot)
```

`PASS_S` defines `HEX_V5 = HEX_V4` and
`HEX_V4 = HEX_V3`, while `PASS_N` defines
`HEX_V1 = HEX_V2` and `HEX_V2 = HEX_V3`.
Unlike the former corner-tile exploration, this identifies
the nearby **coordinate-specific resolved routing roots**
that must be searched for actual enabled drivers. The
segment passes are fixed architecture connectors, not
individually selected programmable PIPs.

**Boundary:** The three `HEX_V3` root coordinates are not
yet identified as logic outputs, physical PCI inputs,
or active programmable source PIPs. A proper `wire_tree` /
`wire_pips_bwd` search, including BRAM-adjacent west-edge
tiles and programmed pass connections, remains necessary.
In particular, do not assert a long-distance corner path
or logical signal role from shared wire names.

This graph deduction uses the pinned Project Combine
architecture, with PCILOGIC source-select bit values already
independently validated against private `BINARY/205`.
It does not establish PCI/DMA idle.

References:
- [Pinned Project Combine connector definitions](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt)
- [Pinned wire resolver](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/public/interconnect/src/grid.rs)


### Local west-edge pass candidates at the three PCILOGIC input roots

A bit-exact recheck of private `BINARY/205` using the pinned Project
Combine `IO_W` feature definitions tested both programmable
`SINGLE_E` pass connections on each resolved `HEX_V3` root.
The reader was independently controlled against **all three** known
`PCI_W_VE` mux selections at `X0,Y13`:
`I1=1010000`, `I2=1000001`, `I3=0001`.
The mux encoding uses the listed bit order, most significant first,
with west main frames starting at slot 2186.

| Input / resolved root | Candidate IO_W pass | Feature bit | Observed raw bit |
|---|---|---|---|
| I1 / X0,Y11 `HEX_V3[3]` | `SINGLE_E[19]` | `MAIN[9][8]` | 0 |
| I1 / X0,Y11 `HEX_V3[3]` | `SINGLE_E[22]` | `MAIN[1][8]` | 0 |
| I2 / X0,Y15 `HEX_V3[3]` | `SINGLE_E[19]` | `MAIN[9][8]` | 0 |
| I2 / X0,Y15 `HEX_V3[3]` | `SINGLE_E[22]` | `MAIN[1][8]` | 0 |
| I3 / X0,Y12 `HEX_V3[1]` | `SINGLE_E[7]` | `MAIN[33][8]` | 0 |
| I3 / X0,Y12 `HEX_V3[1]` | `SINGLE_E[10]` | `MAIN[25][8]` | 0 |

**Verified configuration result:** None of these six west-edge
programmable pass bits is set. Therefore the two direct
`SINGLE_E` pass connections enumerated for each root are not
evidence of an active signal driver in this image.

**Not yet established:** These are only the explicitly enumerated
local `IO_W` passes, **not** a complete enumeration of the root's
incoming switchbox PIPs or all architecture connectors. Upstream
`HEX_V3` connectivity, other tile classes, and actual logic/IOB
drivers require graph traversal. The input signal meanings, active
board firmware identity, busmaster idle, FIFO-empty and posted-write
drain are still unknown. Do not use this negative result as a DMA
release predicate.

Evidence: locally decoded private `BINARY/205` (not published) and
[pinned Project Combine IO_W pass definitions](https://github.com/prjunnamed/prjcombine/blob/234343d23e737e57f2727630e19008b509d7d522/databases/virtex.txt).


### Adjacent HEX_V0/HEX_V6 west-edge pass candidates

The same read-only decoder also tested the two additional `IO_W`
`SINGLE_E` pass locations at each three-row segment endpoint,
using the Project Combine `HEX_V0/HEX_V3/HEX_V6` wire geometry.
Together with the six root-local checks above, **all 18 candidate
west I/O pass bits tested on these three seven-row vertical
segments are zero**:

| PCILOGIC input | Segment row / west wire | Passes | Raw configuration bits |
|---|---|---|---|
| I1 | Y8 `HEX_V6[3]` | `SINGLE_E[18]`, `[21]` | 0, 0 |
| I1 | Y11 `HEX_V3[3]` | `SINGLE_E[19]`, `[22]` | 0, 0 |
| I1 | Y14 `HEX_V0[3]` | `SINGLE_E[20]`, `[23]` | 0, 0 |
| I2 | Y12 `HEX_V6[3]` | `SINGLE_E[18]`, `[21]` | 0, 0 |
| I2 | Y15 `HEX_V3[3]` | `SINGLE_E[19]`, `[22]` | 0, 0 |
| I2 | Y18 `HEX_V0[3]` | `SINGLE_E[20]`, `[23]` | 0, 0 |
| I3 | Y9 `HEX_V6[1]` | `SINGLE_E[6]`, `[9]` | 0, 0 |
| I3 | Y12 `HEX_V3[1]` | `SINGLE_E[7]`, `[10]` | 0, 0 |
| I3 | Y15 `HEX_V0[1]` | `SINGLE_E[8]`, `[11]` | 0, 0 |

The tested `IO_W` pass features are not configured active.
These results are **not** proof that the corresponding PCI inputs
are unconnected: the seven-row segment assignment and all other
incoming switchbox / inter-tile paths still require a full
`wire_tree` and configured-PIP traversal. In particular, do
not infer a constant value, signal polarity, or DMA safety
condition from these disabled `SINGLE_E` passes.


### PCI busmaster quiescence: evidence-gated handoff (2026-10-09)

This section consolidates the current **safety conclusion** without
claiming new firmware-derived PIPs or a verified busmaster-state decode.
The active routes for `PCILOGIC.I1/I2/I3` already terminate at
`X2,Y14 SLICE[1] G`, `X2,Y12 SLICE[0] G`, and
`X2,Y3 SLICE[0] G`, respectively (see the active-route table above).
They are not direct PCI package inputs. The decoded LUT vectors are
configuration evidence, not identified PCI protocol state predicates.

| Protocol or DMA condition | Evidence status in BINARY/205 | Safe DMA mapping release? |
|---|---|---|
| Master requests access (`REQ#`) | Verified: PCI-clocked registered output-enable path; no decoded upstream state cone | **No** |
| Master has grant (`GNT#`) | Verified: physical pin/IOB mapping; grant-to-master state transition not established | **No** |
| Transaction is active (`FRAME#`, `IRDY#`, `TRDY#`, `STOP#`) | Verified: mapped PCI pins and registered handshake output paths; no complete handshake FSM | **No** |
| Transaction terminated or target aborted | **Unknown**: no proven decode of final PCI transfer and retry/disconnect/abort state | **No** |
| Internal DMA producer/descriptor engine stopped | **Unknown**: no proven complete producer-state-to-empty predicate | **No** |
| Last PCI write accepted/retired, no outstanding write remains | **Unknown**: neither source-side outstanding-write count nor upstream bridge/posting ordering is proven | **No** |
| Acquisition completion IRQ or `IIMST` | Not demonstrated to imply all preceding conditions | **No** |

**Verified:** The hard `PCILOGIC.PCI_CE` drives 29 registered AD/CBE
output clock enables. This demonstrates an active PCI datapath timing
function, **not** a quiescence/abort acknowledgement. The physical
`REQ#`, `FRAME#`, `IRDY#`, `TRDY#`, and `STOP#` tristate
registers are PCI-clocked, but their actual state sequencing and
release semantics remain unresolved.

**Inferred:** A conclusive idle predicate would need an independently
validated relationship between (1) stopping the remote/acquisition DMA
producer and descriptor consumption, (2) absence of further busmaster
requests/starts, (3) completion of every outstanding PCI transaction,
including termination/error/retry paths, and (4) appropriate host bridge
and DMA API completion/ordering. Neither `REQ#` deassertion nor a final
`FRAME#` deassertion alone supplies this proof. No visible indication
currently binds all four conditions.

**Unknown / necessary next decoding:** Trace the configured G1..G4
input muxes and active upstream routes for the three named PCILOGIC
source LUTs, then correlate their final registers/IOBs with physical
`REQ#`, `GNT#`, `FRAME#`, `IRDY#`, `TRDY#`, and `STOP#`.
Decode the `REQ#`/`FRAME#`/`IRDY#` FFT data, CE, set/reset and
TCE cones, including the PCI-clocked state-transition logic, and test
whether an acknowledged idle state really implies no in-flight write.
An installed *update* resource is not proof that this firmware is the
version executing on the scope. This task needs a complete pinned
Project Combine architecture graph plus per-bit active-PIP validation;
there is no justified shortcut from the three LUT INIT strings.

**x64 implementation consequence:** Keep the existing
`UnknownActive` quarantine and pinned DMA mappings when transfer
ownership cannot be resolved. Do not release them on IRQ, `REQ#`,
`PCI_CE`, or an unverified inferred state. Do not change the production
driver on the strength of this documentation update.


### PCILOGIC source LUT truth-table reduction (2026-10-09)

The three 16-bit G-LUT vectors already recovered from private
`BINARY/205` can be minimized *without any assumption* about physical
signal names. Number the sixteen truth-table positions 0..15 with
position zero the rightmost printed bit. Define abstract truth-table
address bits `b3 b2 b1 b0` (where `b0` is the least significant
position-address bit). Exhaustive truth-table reduction gives:

| PCILOGIC input / LUT source | 16-bit vector | Reduced truth-table function |
|---|---|---|
| I1 / X2,Y14 SLICE[1] G | `0xF0FF` | `b2 OR NOT b3` |
| I2 / X2,Y12 SLICE[0] G | `0xFFFA` | `b0 OR b2 OR b3` |
| I3 / X2,Y3 SLICE[0] G | `0xAFAF` | `b0 OR NOT b2` |

**Verified (as a mathematical consequence of the recorded table
vectors):** Only two independent truth-table address bits influence
I1, three influence I2, and two influence I3. These equations are
exact for the published vectors with the explicit address-bit
convention above.

**Unknown:** Which physical `G1..G4` pins correspond to these abstract
address bits, whether the bitstream's serialized LUT field requires
additional permutation/inversion, the polarity of the PCILOGIC inputs,
and the active upstream input-net sources. Accordingly, **do not** label
these abstract variables as PCI handshake signals yet or use any of
these reductions as a DMA-idle predicate. The exact architectural
input-bit mapping and four upstream configured routes per source LUT
remain the next verification tasks.

### Seven selected PCILOGIC source-LUT inputs (offline decode, 2026-10-09)

Using pinned Project Combine commit `234343d23e737e57f2727630e19008b509d7d522`, the local firmware decoder independently reproduces PCILOGIC I1/I2/I3 mux values `1010000 / 1000001 / 0001` and the three G-LUT vectors `F0FF / FFFA / AFAF`. The bit-reversed input packet's 18-word frame array begins at byte offset 68; the CLB X2 MAIN frame base is 2030. Both are cross-checked against known control values.

| Condition | G1 | G2 | G3 | G4 |
|---|---|---|---|---|
| I1: X2,Y14 SLICE[1] G | off | off | SINGLE_S_BUF[1] | SINGLE_N_BUF[14] |
| I2: X2,Y12 SLICE[0] G | SINGLE_N_BUF[23] | off | SINGLE_E_BUF[3] | SINGLE_E_BUF[0] |
| I3: X2,Y3 SLICE[0] G | SINGLE_S_BUF[5] | off | SINGLE_N_BUF[11] | off |

**Verified:** These twelve decoded selections each match Project Combine's enumerated mux settings. Only seven physical LUT inputs are relevant to the recovered truth tables: two for I1, three for I2, two for I3.

**Inferred:** Under the standard direct G1..G4 LUT address-bit ordering the reduced functions are I1 = `G3 | !G4`, I2 = `G1 | G3 | G4`, I3 = `G1 | !G3`. Independently validating the old-family LUT pin ordering and PCILOGIC input polarity is still required.

**Unknown:** The configured upstream drivers of these seven selected SINGLE routing wires, their ties to PCI control pins/registered state, and the PCI transaction completion/write-drain predicate. This establishes no safe DMA unmap criterion. The x64 `UnknownActive` quarantine remains mandatory.

### Immediate CLB-local programmable connections for seven PCILOGIC inputs (2026-10-09)

A passive bit-level scan using the pinned Project Combine `CLB` tile
pass/bipass feature definitions, decoded `BINARY/205`, X2 MAIN
frame base 2030, and tile bit index `Y*18+bit`, examined the
seven selected input routing wires. `SINGLE_*_BUF` is an architectural
permanent buffer from the respective `SINGLE_*` wire, so its local
programmable producers were evaluated at the same tile coordinate.

| Target | Active tested local pass/bypass annotations |
|---|---|
| I1 G3 at X2,Y14, `SINGLE_S[1]` | `OMUX[0] -> SINGLE_S[1]` (`MAIN[47][5]=1`) |
| I1 G4 at X2,Y14, `SINGLE_N[14]` | No tested local direct pass enabled |
| I2 G1 at X2,Y12, `SINGLE_N[23]` | No tested local direct pass enabled |
| I2 G3 at X2,Y12, `SINGLE_E[3]` | No tested local direct pass/bypass enabled |
| I2 G4 at X2,Y12, `SINGLE_E[0]` | `HEX_S3[0] -> SINGLE_E[0]` (`!MAIN[42][2]`, raw 0), and `SINGLE_S[6] -> SINGLE_E[0]` (`MAIN[39][7]=1`) |
| I3 G1 at X2,Y3, `SINGLE_S[5]` | `HEX_S6[2] -> SINGLE_S[5]` (`!MAIN[34][3]`, raw 0) |
| I3 G3 at X2,Y3, `SINGLE_N[11]` | No tested local direct pass enabled |

**Verified scope:** The values above are direct tests of the enumerated
local `pass` and `bipass` configuration annotations. They are not
a complete source-tree resolution, and a disabled local pass does
not mean that the corresponding routing net is undriven.

**Unknown:** The apparent dual selected annotations for `I2.G4`
must be resolved against Project Combine's pip semantics and physical
routing-tree/connector representation before assigning an effective
single electrical producer. Likewise, even the selected `OMUX[0]`
source of `I1.G3` has not yet been traced to a specific F/G LUT,
registered output or relevant PCI signal. This is **not** a reconstructed
PCI arbitration/transaction-state machine and provides no DMA idle proof.

**Driver consequence:** Keep `UnknownActive` quarantine unchanged.

### I1 G3 source register: X2,Y14 SLICE[0] XQ (2026-10-09)

The selected I1.G3 upstream local pass from the previous section is
`OMUX[0] -> SINGLE_S[1]`, at `X2,Y14`.
A bit-exact decode of the same CLB's
`mux OMUX[0] @[MAIN[39][17], MAIN[45][17], MAIN[43][17], MAIN[40][17], MAIN[44][17], MAIN[41][17], MAIN[42][17]]`
gives the configured field **`0011011`**, matching the pinned
database selection **`OUT_CLB_XQ[0]`**.

**Verified configured routing:**
`X2,Y14 SLICE[0].XQ -> OMUX[0] -> SINGLE_S[1] -> SINGLE_S_BUF[1] -> X2,Y14 SLICE[1].G3`.
This is the first recovered *registered logic source* in one of the
three PCILOGIC source-LUT cones, and is distinct from any physical
PCI pin. `XQ` denotes the slice X flip-flop path in the architecture.

**Unknown:** The register's D input, CE, clock, synchronous/asynchronous
set/reset and actual protocol meaning have not yet been traced.
A configured XQ source alone does not prove its active transition
sequence, handshake role, or DMA-drain semantics. PCI busmaster
quiescence is still **not proven**; retain `UnknownActive`.


### FDRI packet-boundary cross-check and XQ source verification caveat (2026-10-09)

A fresh examination of the privately supplied `BINARY_205_decoded.bin`
shows that, after reversing each byte's bit order, the four bytes at
offset **68..71** are `50 00 9d 92`, i.e. the packet header
`0x50009D92`; bytes at **72..75** are `00 12 00 00`,
the beginning of the FDRI packet payload. Thus **offset 68 is a packet
header, not the first FDRI frame word**. Previous text describing a
frame array beginning at offset 68 is corrected here.

**Verification warning:** In a separate newly implemented local bit
reader, CLK/CE/SR mux values decoded for the reported
`X2,Y14 SLICE[0].XQ` source did **not** all map to valid mux entries
under the currently assumed CLB/frame address mapping. This mismatch
means that decoder's register-control results are **not verified**.
The earlier documentation's `XQ` routing attribution is retained as
a previously reported result, but is **pending independent
reproduction** with fully cross-checked frame alignment, geometry,
and bit ordering. No fresh register clock, CE, reset, state transition,
or handshake semantics can be asserted from this attempted decode.

**Safety consequence:** No PCI busmaster-idle / abort acknowledgement
criterion has been established. Do not relax `UnknownActive` or
unmap indeterminate DMA buffers based on any of these candidates.


### Independent LUT frame-map audit: prior decode requires correction (2026-10-09)

An independent offline reader checked the supplied private decoded
`BINARY/205` stream against the pinned Project Combine parser,
rather than assuming the previous feature-decoding conclusions.

**Verified packet bytes:** after reversing bits within each byte,
offset `0x44` (decimal 68) contains the big-endian word `0x50009D92`;
offset `0x48` (decimal 72) starts with `0x00120000`.
The first word is the Type-2 FDRI header, not frame contents.

**Independent negative control:** With an 18-DWORD/540-bit frame
interpretation, frame-word reversal and the 28-bit partial last
word according to `insert_virtex_frame`, a straightforward
linear frame/row reader at asserted CLB X2 main-frame base 2030
does **not** reproduce the three previously recorded G-LUT vectors:
- `X2,Y14 SLICE[1].G`: expected `F0FF`, obtained `FBEF`;
- `X2,Y12 SLICE[0].G`: expected `FFFA`, obtained `0000`;
- `X2,Y3 SLICE[0].G`: expected `AFAF`, obtained `7DF8`.

As an additional control, the exact three 16-bit vectors were not
matched separately at any candidate linear base from 0 to 2192
under **that same naive model**. This is evidence that the naive
frame-to-tile mapping, frame data ordering, or current assumptions
are incomplete; it does **not** establish different firmware or
disprove the earlier reverse-engineered routes.

**Status correction:** Do not treat the later reported seven
G-input selections, local pass bits, or XQ/OMUX registered source
as *independently replicated* by this audit. They remain **reported
earlier findings pending reconciliation** with an independently
verified geometry/packet/frame-coordinate model. No fresh
PCI-state-register or DMA-idle interpretation follows.

**Next action:** Construct the frame lookup using Project Combine's
actual device `expanded` geometry, column-width and frame ownership
and parse the complete FDRI packet sequence as the reference parser
does; cross-check several unrelated known IOB and PCILOGIC fields
and all three G-LUT vectors before continuing driver tracing.
Keep `UnknownActive` quarantine unchanged.


### Resolved FDRI transfer-word off-by-one and verified XQ control sources (2026-10-09)

The earlier independent negative LUT check was caused by a **decoder
bug**, now identified against pinned Project Combine
`public/xilinx-bitstream/src/parse.rs` `insert_virtex_frame`.
For a 540-bit Virtex frame, 17 packed data words are decoded from each
18-word transfer slot: words 0..15 map descending 32-bit chunks,
word 16 contributes its high 28 bits to positions 0..27, while the
additional transfer word 17 is **not** a data word. The packet header
is at byte offset 68, with the FDRI payload beginning at byte 72.

The earlier independent reader incorrectly mapped low frame bits to
transfer word 17 and all remaining positions one word too late.
That error fully explains its failure to reproduce the expected LUT
vectors and does **not** invalidate the prior source locations.
Using the corrected word mapping and MAIN frame base 2030 at X2,
all three source LUTs now match the original vectors exactly:

| Source LUT | Previously established | Independently re-decoded |
|---|---|---|
| X2,Y14 SLICE[1].G | `F0FF` | `F0FF` |
| X2,Y12 SLICE[0].G | `FFFA` | `FFFA` |
| X2,Y3 SLICE[0].G | `AFAF` | `AFAF` |

The same corrected reader also reproduces **all twelve** earlier
`IMUX_CLB_G1..G4` selections and the X2,Y14 `OMUX[0]` value
`0011011 = OUT_CLB_XQ[0]`. Thus the configured register-source
routing `X2,Y14 SLICE[0].XQ -> OMUX[0] -> SINGLE_S[1] ->
SLICE[1].G3` is independently replicated rather than merely
reported. Note that a routed register output is not an established
PCI transaction-state predicate.

Fresh **verified mux source selections** at X2,Y14:

| CLB control | Configured value | Project Combine mux source |
|---|---|---|
| `IMUX_CLB_CLK[0]` | `001000` | `GCLK_LEAF[3]` |
| `IMUX_CLB_CLK[1]` | `001000` | `GCLK_LEAF[3]` |
| `IMUX_CLB_CE[0]` | `000000` | `PULLUP` |
| `IMUX_CLB_SR[0]` | `000010` | `HEX_V5[1]` |

The documented package/BUFG mapping identifies `GCLK_LEAF[3]`
with PCI CLK. The clock and enable **mux source** are now verified;
per-slice optional inversion, actual X FF D mux, SR semantics,
reset polarity and the origin of `HEX_V5[1]` still require decoding.
`PULLUP` is an enabled CE source, not DMA-idle acknowledgement.
Nothing yet proves a complete PCI busmaster stop/abort transaction
drain or absence of upstream posted writes. Keep `UnknownActive`.


### X2,Y14 SLICE[0].XQ D-path selection (2026-10-09)

A further offline bit-exact decode of the same verified CLB frame
rectangle identifies the upstream X-flipflop D mux and local X-data
path for the XQ state feeding PCILOGIC I1.G3:

| Attribute/input mux | Configured bits | Decoded selection |
|---|---|---|
| `SLICE[0].DXMUX` (`MAIN[46][16]`) | `0` | **X**, not BX |
| `SLICE[0].FXMUX` (`MAIN[29][15],MAIN[31][16]`) | `10` | **F5** |
| `SLICE[0].FF_SR_SYNC` | `0` | control-bit observation; reset behavior not yet established |
| `SLICE[0].FF_LATCH` | `0` | control-bit observation |
| `IMUX_CLB_BX[0]` | `001000` | `SINGLE_S_BUF[9]` (not selected by DXMUX) |
| `IMUX_CLB_F1[0]` | `100000001` | `SINGLE_E_BUF[14]` |
| `IMUX_CLB_F2[0]` | `110000100` | `SINGLE_E_BUF[6]` |
| `IMUX_CLB_F3[0]`, `F4[0]` | `000000000` each | off |

**Verified selection chain:** `XQ` receives registered X-path
data rather than the separate BX input; the configured X-path mux
selects F5. It is **not** yet proven how the F5 combinational
function is formed from SLICE[0]/SLICE[1] F-LUT results, which inputs
are live through other architecture muxes, or how the selected SR
signal alters transitions. The XQ register is PCI-clock-muxed and
its CE source selects PULLUP as documented above, but this does
not identify a PCI busmaster phase or prove write drain.

**Unknown:** F5 logic cone, full state transitions, and upstream
transaction termination/posted-write conditions. No DMA unmap
predicate is established. Keep `UnknownActive`.


### X2,Y14 XQ F5 mux: same-slice F/G sources and BX select (2026-10-09)

A further independent bit-exact read of the corrected 540-bit frame
decode resolves the remaining local *F5 source selection* for the
previously identified XQ register at `X2,Y14 SLICE[0]`.

| Configured item | Recovered value | Meaning / limit |
|---|---|---|
| SLICE[0].F LUT | `0xDDDD` | Truth-table function `b1 OR NOT b0` |
| SLICE[0].G LUT | `0xD0F1` | Full 16-entry truth table recovered; no PCI net labels yet |
| SLICE[0].FXMUX | `10` | `F5` |
| SLICE[0].DXMUX | `0` | `X` (not direct `BX`) |
| SLICE[0].BX input | `001000` | `SINGLE_S_BUF[9]` |
| SLICE[0].F1 | `100000001` | `SINGLE_E_BUF[14]` |
| SLICE[0].F2 | `110000100` | `SINGLE_E_BUF[6]` |
| SLICE[0].F3 / F4 | `000000000` | Off / off |
| SLICE[0].FF_SR_ENABLE | `1` | SR logic enabled, polarity/source behavior not yet traced |
| SLICE[0].FFX_INIT | `1` | Configured initial value bit; operational initialization still contextual |

**Verified configuration:** The relevant XQ input is selected as
`FXMUX=F5 -> X -> DXMUX -> XQ`. Pinned Project Combine's
Virtex-family `gen_muxf5` reference in
`re/xilinx/v2xdl-verify/src/clb_lut4.rs` connects MUXF5
`I1` to the **same slice's F LUT**, `I0` to the **same slice's
G LUT**, and `S` to `BX`. Thus the F5 path combines
`SLICE[0].F`, `SLICE[0].G`, and selected
`SLICE[0].BX = SINGLE_S_BUF[9]`; the independently recovered
`SLICE[1].F = 0x0300` is **not** automatically a source for this F5.

**Important interpretation boundary:** The two LUT functions are
indexed with abstract truth-table address bits; physical F/G
pin ordering and any inversion at the BX connection need independent
cross-checking before asserting an exact logic-level state transition.
Tracing the three selected F1/F2/BX lines and all genuinely relevant G
pins remains necessary. No physical PCI busmaster/posted-write
termination or DMA quiescence proof follows, and `UnknownActive`
must remain in force.

### F5 G input selection audit (2026-10-09)

The calibrated offline bit reader reports the following candidate selections at X2,Y14 SLICE[0].G, whose INIT was previously decoded as 0xD0F1:

| Input | Selected wire |
|---|---|
| G1 | SINGLE_N_BUF[15] |
| G2 | SINGLE_E_BUF[23] |
| G3 | SINGLE_N_BUF[4] |
| G4 | SINGLE_N_BUF[10] |

These selections require independent upstream PIP and wire-tree verification. They are not physical PCI signal identities and do not establish busmaster idle or write drain. The existing UnknownActive quarantine is unchanged.


### Independently verified XQ feedback into local F5 G-LUT (2026-10-09)

At `X2,Y14`, the `SLICE[0].G2` input is configured to
`SINGLE_E_BUF[23]`. The pinned `CLB` tile database lists
`OMUX[7] -> SINGLE_E[23]` on `MAIN[1][5]`, which
is **1** in decoded private `BINARY/205`. The same tile's
`OMUX[7]` source-select field
`MAIN[8,2,4,7,3,6,5][17]` is **0011011**, exactly
`OUT_CLB_XQ[0]`. Thus there is a bit-verified feedback route:

`X2,Y14 SLICE[0].XQ -> OMUX[7] -> SINGLE_E[23] ->
SINGLE_E_BUF[23] -> SLICE[0].G2`.

The same decoder checked the listed local direct driver candidates
for the other G-LUT wires: `G1=SINGLE_N[15]` has
`!MAIN[17][3]` **disabled** (raw 1);
`G3=SINGLE_N[4]` has `MAIN[38][4]` **disabled**
(raw 0) and `!MAIN[38][3]` **disabled** (raw 1);
`G4=SINGLE_N[10]` has `MAIN[26][4]` **disabled**
(raw 0) and `!MAIN[26][3]` **disabled** (raw 1).
For G2, other locally enumerated `HEX_H6[3]` and
`SINGLE_S[21]/SINGLE_N[23]` entries are disabled.
These local checks do not exhaust neighboring-tile connectivity.

**Verified:** The configured register output is routed back into the
same slice's G-LUT, which participates in the selected F5 XQ D path.
**Inferred:** This is a plausible sequential-state feedback cone,
not a decoded PCI arbitration state or a proven hold/advance equation.
**Unknown:** The G2 truth-table pin ordering, F5 BX selection polarity,
the independent inputs, SR effects, and the conditions under which
this feedback actually determines XQ. No DMA bus-idle criterion follows;
retain `UnknownActive`.


### Algebraic cofactors of the XQ-feedback G-LUT (2026-10-09)

The already recovered `X2,Y14 SLICE[0].G` truth table is
`0xD0F1`. An offline exhaustive enumeration of all sixteen
entries, independently simplified to sum-of-products, gives
the following exact expression using **abstract LUT address bits**
`b0..b3` (the rightmost printed truth-table bit is entry 0):

```text
G = (b1 & b2) | (b2 & !b0) | (b2 & !b3) |
    (!b0 & !b1 & !b3)
```

Cofactoring on `b1` gives:

```text
G(b1=0) = (b2 & !b0) | (b2 & !b3) | (!b0 & !b3)
G(b1=1) = b2
```

**Verified mathematics:** The two cofactors are exact consequences of
the 16-bit INIT vector. The distinct `b1=1` branch is particularly
simple: it passes `b2`. No external physical signal is needed to
verify this truth-table property.

**Inferred, conditional on pin ordering:** The previously verified
configured routing takes `SLICE[0].XQ` back to `SLICE[0].G2`.
If Project Combine's physical LUT addressing maps `G2` to abstract
`b1`, these cofactors describe the combinatorial G branch for
`Q=0` and `Q=1`, respectively. That alone is **not** the full
flip-flop next-state equation: `F5` also selects the F-LUT versus
G-LUT using BX, and the flip-flop reset/set behavior still applies.
If the physical LUT pin permutation differs, do not equate `b1`
with the XQ feedback signal.

**Unknown:** The upstream drivers for G1/G3/G4, BX, F1/F2,
the physical LUT pin-to-address-bit ordering, set/reset transitions,
and any relationship to a PCI transaction-ending or all-writes-drained
acknowledgement. No DMA bus-idle or safe unmap predicate is
demonstrated; preserve `UnknownActive`.


### F5 structural netlist and conditional XQ transition (2026-10-09)

An independent inspection of pinned Project Combine commit
`234343d23e737e57f2727630e19008b509d7d522`,
`re/xilinx/v2xdl-verify/src/clb_lut4.rs`, function
`gen_muxf5`, confirms that the Virtex-family MUXF5
connects `I0` to the **same slice G LUT**, `I1` to the
**same slice F LUT**, and `S` to the slice `BX` input.
The verifier also explicitly models optional BX inversion
(`BXMUX` for original Virtex, `BXINV` in later variants).
This verifies the functional source topology without determining
the programmed BX inversion or external PCI meanings.

For X2,Y14 SLICE[0], the documented LUT truth tables independently
reduce to:

```text
F = f1 | !f0
G = (g1 & g2) | (g2 & !g0) | (g2 & !g3) |
    (!g0 & !g1 & !g3)
```

Here `f0/f1` and `g0..g3` are *abstract* zero-based LUT
address bits, not yet proven electrical F1/F2 or G1..G4
pin names. The above G expression has the exact cofactors
`G(g1=1)=g2` and
`G(g1=0)=majority(g2,!g0,!g3)`,
where `majority(a,b,c)=(a&b)|(a&c)|(b&c)`.

The structural combinational next-data relation, before reset
and clock semantics, is therefore:

```text
D_XQ = MUXF5(I0=G, I1=F, S=effective_BX)
```

If the standard mux selection convention and un-inverted BX apply,
`D_XQ = effective_BX ? F : G`; otherwise its select polarity
must be adjusted. If `G2` really maps to address bit `g1`,
the verified Q feedback into G2 yields the following **conditional**
state transition:

```text
G_when_Q0 = majority(g2, !g0, !g3)
G_when_Q1 = g2
```

**Verified:** The MUXF5 structural connections and algebraic truth
tables. **Inferred:** The XQ state-dependent cofactors conditional on
physical LUT pin ordering. **Unknown:** BX polarity, complete
g/f input signal provenance, flip-flop reset operation,
PCI ownership/termination state, DMA FIFO and outstanding-write drain.
No DMA-bus-idle or abort acknowledgement is proven.
Do not change the x64 UnknownActive quarantine.


### LUT4 physical input ordering resolves XQ feedback variable (2026-10-09)

Pinned Project Combine `re/xilinx/v2xdl-verify/src/clb_lut4.rs`,
`make_lut4`, connects source `LUT4.I0..I3` directly to
physical target `F1..F4` or `G1..G4` (loop index + 1).
Its `compile_lut` maps truth-table entry index `i` directly
to bit `1 << i` in the 16-bit INIT value. Thus the reference
architecture's normal LUT indexing is
`g0=G1, g1=G2, g2=G3, g3=G4`; similarly
`f0=F1, f1=F2`.

Together with the independently bit-verified local feedback route
`XQ -> OMUX[7] -> SINGLE_E[23] -> G2`, this permits a
**specific combinatorial XQ-feedback cofactor** in the G half of F5:

```text
G(Q=0) = majority(G3, !G1, !G4)
G(Q=1) = G3
F      = F2 | !F1
D_XQ   = MUXF5(I0=G, I1=F, S=effective_BX)
```

**Verified:** Normal LUT pin order, exact LUT truth-table
cofactors, the configured feedback to G2, and F5 input wiring.
The cofactor relation is valid for the configured G branch
without speculative PCI signal naming.

**Unknown:** Selected BX inversion in this actual slice, the
upstream sources/meaning of G1/G3/G4/F1/F2/BX, and the
SR and clock polarity details. The feedback alone does not
identify arbitration ownership, transaction completion or an
absence of outstanding posted writes; therefore no DMA-unmap
criterion exists and UnknownActive must remain in effect.


### F5 BX polarity: exact architecture bit and independent decode boundary (2026-10-09)

The pinned Project Combine `databases/virtex.txt` CLB definition
specifies the actual polarity control of `X2,Y14 SLICE[0].BX`:

```text
input BX = ^IMUX_CLB_BX[0] @MAIN[38][13];
```

The earlier `IMUX_CLB_BX[0]` mux selection
(`SINGLE_S_BUF[9]`) therefore establishes only the **source wire**.
The effective MUXF5 select polarity depends additionally on
`MAIN[38][13]`, and cannot be concluded from the routing mux alone.
The verifier's `gen_muxf5` construction confirms
`I0=G`, `I1=F`, `S=BX`; the architecture's input inversion
annotation must be applied before interpreting `S`.

**Verification boundary:** A fresh, minimal standalone packet reader
recognizes the FDRI Type-2 header `0x50009D92` at offset 68
and payload at 72, but its independently reconstructed CLB frame
reader has **not yet reproduced** the three reference G-LUT words
simultaneously. Consequently, this attempt supplies **no verified
firmware value** for `MAIN[38][13]`. In particular, neither BX
polarity nor an exact XQ next-state equation is established by the
current recheck. Reconcile transfer-slot ownership and reference
`insert_virtex_frame` geometry before publishing such a bit value.

The current safe conclusion remains unchanged: no proven PCI
busmaster-quiescent, abort-acknowledged or posted-write-drained
predicate, so the x64 `UnknownActive` quarantine stays in place.


### Independent BX bit read gate: triple-LUT calibration failed (2026-10-09)

The owner-provided Project Combine source archive and private
`BINARY_205_decoded.bin` have been reopened and inspected locally.
The image contains the expected Type-2 FDRI header `0x50009D92`
at byte 68 after reversing the bit order within each byte.
The upstream Project Combine `insert_virtex_frame` reads
`ceil(540/32)=17` frame data words from 18-word Virtex
transfer slots and ignores the extra transfer word.

A fresh standalone checker with packet payload offset 72,
assumed CLB X2 frame base 2030, and a simple frame-indexing
model **does not reproduce** the previously reported three
G-LUT reference values together. Consequently a value
obtained by that checker for `MAIN[38][13]` is not verified
and must not be promoted to an actual configured BX
inversion bit. The authoritative architecture location is
still `input BX = ^IMUX_CLB_BX[0] @MAIN[38][13]`
in the pinned CLB definition.

**Next reproducibility requirement:** Explicitly derive FDRI
frame-to-column assignments from the pinned expanded-device
frame geometry (including block types, any padding/skip slots
and exact XC2S200E column offsets) and simultaneously validate
three G-LUT INITs, the PCILOGIC input fields and at least one
independent west-side IOB feature. Only then decode the BX
polarity and upstream arbitration state.

This audit changes no hardware or driver code; PCI busmaster
quiescence and posted-write draining remain unproven. Preserve
`UnknownActive`.


### Independent FDRI/LUT validation audit (2026-10-09)

A new standalone offline reader was implemented against private
`BINARY_205_decoded.bin` and pinned Project Combine source
`234343d23e737e57f2727630e19008b509d7d522`. The file
is 180252 bytes and is bit-reversed per byte to recover the
configuration packet stream. The Type-2 FDRI header at byte
68 decodes to `0x50009D92`, and the FDRI payload begins at
byte 72. The Type-2 word count is 40338, exactly
`2241 * 18` 32-bit transfer words.

The pinned `insert_virtex_frame` implementation places 540
configuration bits from the **first 17 words** of each
18-word transfer slot: word 16's bits 4..31 provide
logical frame positions 0..27, and words 0..15 fill
positions 28..539 in reverse word order. Word 17 is not
part of the 540-bit frame data.

Under a direct sequential-slot interpretation with
the previously asserted X2 CLB MAIN base 2030,
and pinned `CLB` G-LUT bit coordinates
(`SLICE[1].G = !MAIN[0..15][15]`,
`SLICE[0].G = !MAIN[47..32][15]`),
the independent decode produces:

| LUT | Earlier documented | Fresh independent reading |
|---|---|---|
| X2,Y14 SLICE[1].G | `F0FF` | `FF0F` |
| X2,Y12 SLICE[0].G | `FFFA` | `5FFF` |
| X2,Y3 SLICE[0].G | `AFAF` | `F5F5` |

A scan of candidate sequential frame bases found no single
base which reproduces all three stored LUT values at once.
This is a **calibration failure** for the simple sequential
frame index mapping, not proof that the FPGA has different
logic. The reference `fill_frame_info` in
`public/virtex/src/expand.rs` constructs frame addresses
in an interleaved center-out column order with device-specific
column widths and separate BRAM-related sections; a simple
uniform column/slot assumption is not independently established.

**Status:** The earlier reported XQ feedback routes, LUT
vectors, F5 selections and upstream muxes are prior analytical
claims but their exact firmware bit locations have not yet been
reproduced by this independent audit. Do not extend them into
a claimed complete PCI state machine, BX polarity, or DMA
shutdown acknowledgement until actual device geometry and
frame-address placement are validated against multiple
independent fields. The immutable driver rule remains:
`UnknownActive` mappings must not be released without a real
DMA-quiescence proof.

### Reproducible CHIP18 decoder calibration (2026-10-09)

A repository-local offline decoder now exists at
`tools/fpga/virtexe_xc2s200e_decode.py`. It contains no private
firmware bytes; the private `BINARY_205_decoded.bin` is supplied
only as an input path during local validation. The decoder implements
the pinned Project Combine `CHIP18`/`xc2s200e-pq208` frame order from
commit `234343d23e737e57f2727630e19008b509d7d522`, including the
spine frames and center-out column sequence from `fill_frame_info`.
For this device, Project Combine gives `X2` main-frame base **2030**.

The same tool parses the bit-reversed Xilinx packet stream, locates
the first Type-2 FDRI packet at byte 68, starts the payload at byte
72, decodes the `40338` transfer words as `2241 * 18` Virtex transfer
slots, and inserts only the first 2240 main frames. Each 540-bit frame
uses the pinned `insert_virtex_frame` layout: word 16 bits 4..31 form
frame positions 0..27, words 15..0 fill positions 28..539 in LSB-first
word order, and transfer word 17 is not frame data.

Local validation against private `BINARY_205_decoded.bin` passes these
independent calibration points:

| Feature | Re-decoded value |
|---|---|
| X2,Y14 SLICE[1].G | `0xF0FF` |
| X2,Y12 SLICE[0].G | `0xFFFA` |
| X2,Y3 SLICE[0].G | `0xAFAF` |
| X2,Y14 SLICE[0].F | `0xDDDD` |
| X2,Y14 SLICE[0].G | `0xD0F1` |
| X2,Y14 SLICE[0].BX inversion, `MAIN[38][13]` | `0` |
| X2,Y14 `IMUX_CLB_CLK[0]` | `001000` = `GCLK_LEAF[3]` |
| X2,Y14 `IMUX_CLB_CE[0]` | `000000` = `PULLUP` |
| X2,Y14 `IMUX_CLB_SR[0]` | `000010` = `HEX_V5[1]` |
| X2,Y14 `SLICE[0].DXMUX` | `0` = `X` |
| X2,Y14 `SLICE[0].FXMUX` | `10` = `F5` |
| X2,Y14 `OMUX[0]` | `0011011` = `OUT_CLB_XQ[0]` |
| X2,Y14 `OMUX[7]` | `0011011` = `OUT_CLB_XQ[0]` |

This supersedes the immediately preceding simple sequential-frame
audit failure: the failure came from not applying the Project Combine
CHIP18 column frame order and per-feature bit ordering. The corrected
decoder re-establishes the calibrated X2/Y14 XQ feedback/F5 evidence
and the attached frame-map result that BX inversion is disabled.

**Current X2,Y14 XQ equation boundary:** With the verified LUT pin
ordering and `MAIN[38][13]=0`, the local combinational D path is:

```text
D_XQ = BX ? F : G
F    = F2 | !F1
G(Q=0) = majority(G3, !G1, !G4)
G(Q=1) = G3
```

This remains a local state-register equation, not a decoded PCI
busmaster-idle acknowledgement. The upstream sources and meanings of
`F1`, `F2`, `G1`, `G3`, `G4`, `BX`, and `SR` are still not traced far
enough to classify the register as PCI arbitration, transaction
control, DMA producer/fifo state, or something else. No FPGA-visible
predicate has been proven to cover outstanding PCI transactions,
internal pending data, or host bridge posted-write drain. Preserve the
x64 `UnknownActive` quarantine.
