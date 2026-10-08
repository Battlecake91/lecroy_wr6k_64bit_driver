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
