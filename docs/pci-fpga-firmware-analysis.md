# LeCroy S65 / WR6k PCI FPGA firmware analysis

**Scope:** Spartan-IIE `XC2S200E-6PQ208` on PCI card
`900890-00`, XStream x64 device-pack update image; analytical
observations only. **Do not publish firmware/bitstream image bytes,
vendor binaries, schematics or license data.**

**Current milestone:** The PCI update image has been identified and
decoded to Xilinx frames; package/pad mapping verified; the
`INTA#` output-enable signal is routed onto `SINGLE_E[3]`
on west I/O row 12, and both locally selectable drivers have been
shown inactive. The upstream X1 column source remains unknown.

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
  <- inter-tile connector to adjacent X1 column SINGLE_W[3]
  <- [specific active X1 source/PIP NOT YET DECODED]
```

The local west-edge candidates `HEX_V6[0]` and
`OUT_TBUF_W[3]` are **both deselected** at this
row. They are not upstream sources of this interrupt
control net. This is an actual configuration-bit
filter, not merely a list of potential routing paths.

The neighboring `X1` column is part of the device's
BRAM-associated fabric. Follow that column's corresponding
`SINGLE_W[3]` interconnect and selected source PIPs
before asserting any connection to a LUT/FF or
interrupt state machine. Do **not** confuse
`SINGLE_W[3]` nets at different row coordinates.

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

### Next investigation

Trace the source of `X1,Y12:SINGLE_W[3]` through the
BRAM-column switchbox into actual enabled fabric
routing and sequential/LUT primitives. Then trace the
`REQ#` output and `GNT#` input arbitration path and
cross-compare each recovered control condition to
the original x86 `IIMCL/IIMST`, `INTST`, and
SG descriptor flow. A device DMA-abort/bus-idle proof
must account for producer/link shutdown, local master
transfers and host PCI(-bridge) outstanding writes.
