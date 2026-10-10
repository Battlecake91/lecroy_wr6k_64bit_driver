# LeCroy S65 / WR6k PCI FPGA firmware analysis

**Scope:** Spartan-IIE `XC2S200E-6PQ208` on PCI card
`900890-00`, XStream x64 device-pack update image; analytical
observations only. **Do not publish firmware/bitstream image bytes,
vendor binaries, schematics or license data.**

**Current milestone:** The existing decoder now feeds a tested, bounded Z3
transition model, including registers, distributed RAM, configured BRAM ports,
and architecture-correct internal BUFT resolution. Native firmware calibration
passes 47 checks; 19 PCI local equation/alias checks pass. The expanded network
has 3,359 logic nodes and 9,129 resolved routing paths, with 21 routing paths
still Unknown/ambiguous. See [Formal State Analysis](#formal-state-analysis).
The PCI-clocked Q/P
transition relations, FRAME output/OE shadow registers, delayed IRDY OE,
REQ output and grant-qualified start logic are reconstructed below in
[Verified PCI Control Register Network](#verified-pci-control-register-network).
The INTA# output-enable cone and its PCI-clocked X7/Y10 state remain
independently established. No complete DMA-quiescence predicate is proven.

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

**Current decoding boundary:** The relevant PCILOGIC source-LUT routes
and reachable PCI control registers are resolved in the current
register-network section below. Dedicated carry/TBUS semantics are now
modeled and calibrated, but the expanded cones reach runtime RAM state,
BRAM routing/semantics and unmodeled hard blocks. The local
`REQ#`/`FRAME#`/`IRDY#` data/enable/reset relations do not establish an
acknowledged idle state implying no in-flight write.
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

The direct G1..G4 address-bit ordering, effective PCILOGIC input
polarity and truth-table-relevant upstream drivers are verified below.
Do not treat abstract LUT variables as physical PCI handshake pins
without that routing evidence, or use these reductions as a DMA-idle
predicate.

### Seven selected PCILOGIC source-LUT inputs (offline decode, 2026-10-09)

Using pinned Project Combine commit `234343d23e737e57f2727630e19008b509d7d522`, the local firmware decoder independently reproduces PCILOGIC I1/I2/I3 mux values `1010000 / 1000001 / 0001` and the three G-LUT vectors `F0FF / FFFA / AFAF`. The bit-reversed input packet's Type-2 header is at byte offset 68 and its 18-word frame payload begins at byte offset 72; the CLB X2 MAIN frame base is 2030. Both are cross-checked against known control values.

| Condition | G1 | G2 | G3 | G4 |
|---|---|---|---|---|
| I1: X2,Y14 SLICE[1] G | off | off | SINGLE_S_BUF[1] | SINGLE_N_BUF[14] |
| I2: X2,Y12 SLICE[0] G | SINGLE_N_BUF[23] | off | SINGLE_E_BUF[3] | SINGLE_E_BUF[0] |
| I3: X2,Y3 SLICE[0] G | SINGLE_S_BUF[5] | off | SINGLE_N_BUF[11] | off |

**Verified:** These twelve decoded selections each match Project Combine's enumerated mux settings. Only seven physical LUT inputs are relevant to the recovered truth tables: two for I1, three for I2, two for I3.

**Verified:** Direct G1..G4 LUT address-bit ordering and effective noninverted PCILOGIC inputs give I1 = `G3 | !G4`, I2 = `G1 | G3 | G4`, I3 = `G1 | !G3`. Native configured routing confirms the seven drivers in the current register-network section below.

**Unknown:** PCI transaction completion/write-drain semantics. The resolved input routes do not establish a safe DMA unmap criterion. The x64 `UnknownActive` quarantine remains mandatory.

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

**Verified resolution:** Native canonical wire-tree traversal resolves
`I2.G4` to the single producer X2,Y21 SLICE[1].XQ and `I1.G3`
to X2,Y14 SLICE[0].XQ. Multiple selected routing annotations are not
necessarily multiple electrical producers. The register-network section
below supplies the remaining upstream identities; none proves DMA idle.

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

The calibrated local D path, clock/CE mux selections and reset-control
bits are recorded in the decoder calibration section below. The
upstream signal meanings and reset source remain unknown. A configured
XQ source alone does not prove its protocol role or DMA-drain semantics.
PCI busmaster quiescence is still **not proven**; retain `UnknownActive`.


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
| `SLICE[0].FF_SR_SYNC` | `0` | asynchronous SR; effective reset detailed below |
| `SLICE[0].FF_LATCH` | `0` | control-bit observation |
| `IMUX_CLB_BX[0]` | `001000` | `SINGLE_S_BUF[9]` (not selected by DXMUX) |
| `IMUX_CLB_F1[0]` | `100000001` | `SINGLE_E_BUF[14]` |
| `IMUX_CLB_F2[0]` | `110000100` | `SINGLE_E_BUF[6]` |
| `IMUX_CLB_F3[0]`, `F4[0]` | `000000000` each | off |

**Verified selection chain:** `XQ` receives registered X-path
data rather than the separate BX input; the configured X-path mux
selects F5. Its same-slice F/G inputs, physical FRAME# select,
PCI clock and inverted RST#_BUF asynchronous preset are resolved in
the current register-network section below. CE selects PULLUP; neither
this enable nor the decoded D equation identifies a busmaster phase
or proves write drain.

**Unknown:** Full reachable-machine semantics and upstream
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
| SLICE[0].FF_SR_ENABLE | `1` | SR enabled; inverted RST#_BUF preset, resolved below |
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
The independent inputs, SR effects and local transition conditions are
now resolved below. No DMA bus-idle criterion follows;
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

**Verified local cofactor:** The previously verified
configured routing takes `SLICE[0].XQ` back to `SLICE[0].G2`.
The physical LUT ordering established below maps `G2` to abstract
`b1`; these cofactors describe the combinatorial G branch for
`Q=0` and `Q=1`, respectively. That alone is **not** the full
flip-flop next-state equation: `F5` also selects the F-LUT versus
G-LUT using BX, and the flip-flop reset/set behavior still applies.

The upstream drivers and configured preset are now resolved below.
**Unknown:** Any relationship to a PCI transaction-ending or all-writes-drained
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

Here `f0/f1` and `g0..g3` are zero-based LUT address bits,
mapped to F1/F2 and G1..G4 by the physical-order evidence below.
The above G expression has the exact cofactors
`G(g1=1)=g2` and
`G(g1=0)=majority(g2,!g0,!g3)`,
where `majority(a,b,c)=(a&b)|(a&c)|(b&c)`.

The structural combinational next-data relation, before reset
and clock semantics, is therefore:

```text
D_XQ = MUXF5(I0=G, I1=F, S=effective_BX)
```

The calibrated BX inversion is disabled, so
`D_XQ = BX ? F : G`. With G2 mapped to address bit `g1`,
the verified Q feedback into G2 yields the following local
G-branch cofactors:

```text
G_when_Q0 = majority(g2, !g0, !g3)
G_when_Q1 = g2
```

**Verified:** The MUXF5 structural connections, algebraic truth
tables, pin ordering and disabled BX inversion. Input provenance and
configured preset are resolved below. **Unknown:** Complete PCI
ownership/termination state, DMA FIFO and outstanding-write drain.
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

Upstream sources and effective SR/clock polarity are now resolved below.
**Unknown:** Complete protocol-state meaning. The feedback alone does not
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

**Verified:** The calibrated repository decoder reproduces all five
reference LUTs and reads `MAIN[38][13]=0`. BX inversion is disabled;
the local F5 relation is `BX ? F : G`. See the reproducible decoder
calibration below. The upstream sources and protocol meaning remain
unknown.

The current safe conclusion remains unchanged: no proven PCI
busmaster-quiescent, abort-acknowledged or posted-write-drained
predicate, so the x64 `UnknownActive` quarantine stays in place.


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

The decoder applies the Project Combine CHIP18 column frame order
and per-feature bit ordering, reproducing the calibrated X2/Y14 XQ
feedback/F5 evidence and the frame-map result that BX inversion is
disabled.

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

### Configuration-aware routing tool and verification boundary

`tools/fpga/virtexe_routing.py` extends the calibrated frame reader.
`tools/fpga/prjcombine_graph.rs` is a public-architecture adapter for
the pinned Project Combine source. It accepts architecture queries
only; firmware remains in Python and is never passed to Cargo or the
adapter. Generated manifests and build output reside in the local
temporary directory, outside version control.

The adapter source calls `expand_grid`, `resolve_wire`, `wire_tree`,
`wire_pips_bwd` and `tile_bits` directly. It exports canonical and raw
wire coordinates, fixed connector equivalence trees, BEL pins,
attributes, and absolute configuration-bit addresses. Python evaluates
mux, programmable-buffer, pass, bidirectional-pass and inverter
encodings; disabled connections are retained as evidence but never
traversed. Missing or multiple encodings remain Unknown. Unsupported
bit rectangles or frames outside the decoded first FDRI block remain
Unknown rather than being treated as zero.

**Verified:** Thirty-nine synthetic Python tests pass, covering enabled and
disabled selections, bit polarity, absent/duplicate encodings, unavailable
frames, reconvergent paths, distinct-driver/polarity ambiguity, cycles, bounded
traversal, a 1102-node chain, LUT support, register feedback, symbolic
D-path tables, combinational cycles, source correlation, compact evidence,
disabled-pad driver exclusion and clean/pinned Git checkout guards, plus
carry/XOR modes, rotated TBUS lanes and independent taps, floating and
contending drivers, register reset/CE/clock priority, RAM16X1D runtime state,
shadow equivalence and incomplete-reconstruction reporting. The
PowerShell test distinguishes these from private-image validation and
reports skipped private/native checks explicitly. The Python fallback
through `py -3` now executes the same complete test sequence.

**Verified:** Private-image calibration passes the existing five LUTs
and eleven XQ controls (including BX inversion), plus nine independent PCILOGIC,
IOB and routing-control checks. These include `I1/I2/I3` fields
`1010000/1000001/0001`, REQ IOI[2] `MUX_O/MUX_T=1/1`, GNT
IOI[2] `MUX_O/MUX_T=0/0`, enabled X2,Y14 `OMUX[0] -> SINGLE_S[1]`
and disabled X0,Y11 `SINGLE_E[19] -> HEX_V3[3]`. These reproduce
established observations; they do not establish additional paths.

**Verified build:** A real, clean Git checkout at exactly
`234343d23e737e57f2727630e19008b509d7d522` was used, not an archive.
The explicitly authorized `--allow-dependency-download` Cargo invocation
downloaded public crates only, including `bimap 0.6.3`. Rust/Cargo 1.89
then exposed two upstream `tablegen` compatibility errors. The Python
builder copies `public/` into its temporary build directory and applies
two exact, occurrence-checked substitutions there:

- `emit.rs`: wrap `Punct::new(';', Spacing::Alone)` in `TokenTree::Punct`
  before extending `TokenStream`.
- `eval.rs`: replace unstable `strict_add_signed` with
  `checked_add_signed(...).expect("template index overflow")`.

Neither changes device definitions, routing, serialization or bit geometry.
The original pinned checkout remains clean; no persistent Git safe-directory
exception was installed. The generated public `tools/fpga/Cargo.lock`
pins dependencies; subsequent builds pass with `--locked --offline`.
The native adapter compiles and executes successfully. Native calibration
passes 47 checks: the previous 25 feature/polarity/upstream-source checks,
ten carry/XOR configuration checks, one dedicated CIN relation, one rotated
bus/BRAM-skip check, two configured TBUF sets, and eight RAM configuration
checks. All checks use actual configuration bits or the pinned native grid.

Build and run (downloads require separate explicit authorization):

```powershell
python -B tools/fpga/virtexe_xc2s200e_decode.py --project-combine $env:WR6K_PRJCOMBINE --build-adapter --cargo-offline
# Set WR6K_ROUTING_ADAPTER to the printed executable path.
python -B tools/fpga/virtexe_xc2s200e_decode.py $env:WR6K_FPGA_BINARY_205 --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER --trace '2,14,IMUX_CLB_F1[0]'
python -B tools/fpga/virtexe_xc2s200e_decode.py $env:WR6K_FPGA_BINARY_205 --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER --validate-architecture
python -B tools/fpga/virtexe_xc2s200e_decode.py $env:WR6K_FPGA_BINARY_205 --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER --validate-pci-state --max-logic 10000
python -B tools/fpga/virtexe_xc2s200e_decode.py $env:WR6K_FPGA_BINARY_205 --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER --analyze-pci --max-logic 10000 --compact --output $env:TEMP/pci-control-cones.json
python -B tools/fpga/virtexe_xc2s200e_decode.py $env:WR6K_FPGA_BINARY_205 --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER --register-table '2,14,SLICE[0].XQ' --local-table --max-logic 1200 --compact --output $env:TEMP/x2y14-state-table.json
```

Every routing invocation first validates the existing frame calibration
and independent native BEL/mux checks. `--bel '2,14,SLICE[0]'` reports
decoded local attributes and pin inversion. `--analyze-pci` seeds XQ,
PCILOGIC I1/I2/I3, the six documented PCI control pads, and representative
AD/CBE output/OE paths. The bounded logic worklist follows supported
LUT/F5/carry/XOR/register paths with only truth-table-relevant inputs. It
records clock, CE, SR and FF modes, conditional TBUS producers, and the
decoded RAM16X1D read/write contract. Other RAM/shift modes, F6 and hard-block
semantics remain explicit boundaries. It emits configuration relations, not presumed
PCI protocol state names or host ordering guarantees. Full generated
reports can expose proprietary logic and must remain private.

The native API's `wire_pips_bwd` expands MultiRoot trees but not Regional
trees. The adapter explicitly visits every member of Regional trees;
otherwise GCLK_LEAF paths falsely stop at their canonical root. The
clock-source regression covers this fix. Disabled `IBUF_MODE=NONE` pad
terminals are excluded with their bit evidence retained. This resolves
the apparent DLL.LOCKED/pad double producer at X27,Y27 G3[0] to the
enabled north-east DLL; the unused X23,Y29 IOI[2] input is disabled.

### Verified PCI Control Register Network

All results below derive from offline BINARY/205, the compiled adapter,
and the private PCI schematic/package correlation. Generated full routing,
register and truth-table reports remain local. Source identities are
verified configuration facts, not names recovered from vendor RTL.
`!`, `&`, `|`, and `?:` denote Boolean operations; PCI signal variables
are physical pin levels, not active-high assertions of the # signals.

#### Seven F5/SR Input Drivers

All seven routes at X2,Y14 SLICE[0] resolve to one enabled producer,
without unknown PIPs or traversal limits. Route inversion is false for
each; effective SR includes its separate BEL inversion.

| Input | Actual configured driver | Correlation |
|---|---|---|
| F1 | X2,Y14 SLICE[0].XQ | Q feedback, via local OMUX[4] |
| F2 | X3,Y16 SLICE[1].X | A, combinational F LUT |
| G1 | X2,Y15 SLICE[0].XQ | P, adjacent state register |
| G3 | X3,Y16 SLICE[1].X | Same A as F2, not an independent input |
| G4 | X0,Y15 IOI[3].I | P24, IRDY# |
| BX | X0,Y13 IOI[2].I | P29, FRAME#; effective inversion false |
| SR | X12,Y29 IOI[1].I | P198, RST#_BUF; effective inversion true |

G2 independently resolves to Q. G4 follows SINGLE_N[10] to the west
IOB output tap; BX follows SINGLE_S_BUF[9] to the FRAME# input tap.
The longer SR path crosses HEX_V/LV/LH trees to the north IOB_N12_1.
BOND87 maps this IOB to P198; the private schematic independently
connects P198 to RST#_BUF. CLK resolves through the Regional GCLK tree
to X24,Y29 BUFGCE[1].O. Its CE is constant 1 and I is
GCLK_IOB[1].I, the previously established PCI CLK / P185 source.

Let Q = X2,Y14 SLICE[0].XQ, P = X2,Y15 SLICE[0].XQ,
A = X3,Y16 SLICE[1].X, f = FRAME#, and i = IRDY#.
Q has CE=1, noninverted PCI clock, FF_LATCH=0, FF_REV_ENABLE=0,
FF_SR_ENABLE=1, FF_SR_SYNC=0 and FFX_INIT=1. The pinned
`re/xilinx/v2xdl-verify/src/clb_lut4.rs::make_ffs` maps these settings
to asynchronous preset to 1 when effective SR=`!RST#_BUF` is active.
This is a configured reset behavior, not a demonstrated software reset
command or a guarantee that reset drains PCI writes.

The local D equation exhaustively matches all 32 assignments:

| Current condition, with SR inactive | D_Q |
|---|---|
| Q=1 | A |
| Q=0, f=1 | 1 |
| Q=0, f=0, A=0 | `!P & !i` |
| Q=0, f=0, A=1 | `!P | !i` |

Thus F=`A | !Q`; G at Q=0 is `majority(A,!P,!i)`, and G at
Q=1 is A. The F5 select is physical FRAME#. This replaces the
previous unconstrained F/G-variable relation with a PCI-correlated
next-state relation, but does not justify naming Q "idle".

#### Configuration State and Output Relations

P uses F5, F=0x0A3A, G=0x70FF, select i; its clock, CE, reset and
initial value equal Q's. Let U=X2,Y19 SLICE[1].X,
V=X2,Y19 SLICE[0].Y and W=X2,Y19 SLICE[1].Y. All 128 local
assignments verify:

```text
D_P = i ? (P ? (!U & !V) : f) : (!W | (f & !(Q & P)))
A   = (!C & !D) | (K & !P)
```

Here C=X3,Y18 SLICE[0].Y, D=X3,Y17 SLICE[0].Y,
K=X2,Y17 SLICE[0].XQ. A's F LUT is 0x222F (16/16 assignments
checked). K is another CE=1, reset-to-1 PCI register: its F5 selects
FRAME# between F=0x01FF and G=0x001D. C/D and U/V/W are resolved
combinational nets, not constants or unexplained independent state bits.

The seven relevant PCILOGIC source-LUT inputs also now resolve:

| Source-LUT pin | Driver |
|---|---|
| I1.G3 / I1.G4 | Q / X5,Y15 SLICE[0].YQ |
| I2.G1 / I2.G3 / I2.G4 | X3,Y13 SLICE[1].Y / X4,Y9 SLICE[1].XQ / X2,Y21 SLICE[1].XQ |
| I3.G1 / I3.G3 | X2,Y15 SLICE[1].XQ / X5,Y5 SLICE[0].YQ |

The native reconstruction establishes these output-state relations:

| Register | Configured data and enable | Interpretation |
|---|---|---|
| B = X2,Y13 SLICE[0].XQ | Same X-path F5 data, CE, clock, SR and INIT=1 as FRAME#.FFO | FRAME output-data shadow |
| T = X3,Y13 SLICE[0].XQ | Same X-path F5 data, CE, clock, SR and INIT=1 as FRAME#.FFT | FRAME tristate shadow |
| IRDY#.FFT | D=T, TCE=1, PCI clock, INIT=1, asynchronous preset | One-cycle delayed T, outside reset |
| REQ#.FFO | D=X2,Y18 SLICE[0].X, OCE=1, PCI clock, INIT=1, asynchronous preset | Registered active-low request data |
| REQ#.FFT | D=0, TCE=1, PCI clock, INIT=1, asynchronous preset | High impedance at reset, enabled after a clock |

The shadow equalities follow from matching effective D/CE/CLK/SR and
initialization, not merely proximity. B selects STOP# between F=0x0151
and G=0xC4F5; TRDY# and GNT# occur in its live D inputs. T selects
X5,Y10 SLICE[0].Y between F=0xCEEE and G=0x7F55; its D inputs
include B, TRDY# and STOP#. Neither shadow is a decoded all-writes-drained
bit. The raw PULLUP source of REQ#.T is inverted at the BEL, so its
effective D is **0**, not 1.

Let S=X4,Y9 SLICE[1].XQ, M=X5,Y3 SLICE[0].YQ,
N=X5,Y10 SLICE[1].YQ, and g=GNT#. S has CE=1, the same PCI
clock/reset and INIT=0. Its F=0x2000 and companion G=0x00A0 give:

```text
D_S  = f & !g & i & M & N & !S
CE_T = !T | (S & M)             # X3,Y12 SLICE[0].F = 0x88FF
```

This is a grant-qualified pulse with feedback, not an unqualified
grant sampler: S=1 forces the next D_S to 0. The T update enable is
unconditional while T=0 and requires S&M while T=1. It is therefore
connected to bus-output acquisition/retention, not established as DMA
completion. The F and CE relations were exhaustively checked (16 and
8 assignments); companion G gives `M & N & !S` directly.

For REQ data, let R=X2,Y12 SLICE[1].YQ,
Rd=X2,Y16 SLICE[1].YQ and H=X2,Y18 SLICE[1].YQ. R/Rd/H
all have CE=1, the same PCI clock/reset and INIT=0. Rd.D=R is a
one-cycle delay. With E=X2,Y18 SLICE[0].Y, the F=0xFFAB and
G=0xEE00 output cone reduces to:

```text
D_REQ_FFO = R | Rd | (!H & !E)
E         = N & (M | X4,Y20 SLICE[1].YQ)
```

These match 16/16 and 8/8 local assignments. The active-low REQ
assertion condition is the complement of D_REQ_FFO after its clock;
it is not a present-cycle assertion, and REQ deassertion alone is not
a transaction/FIFO-empty acknowledgement.

#### Actual Verification Boundary

The full offline run at `--max-logic 10000` contains **3,359 logic nodes**:
1,405 registers, 1,480 combinational outputs, 352 tristate drivers, 64 input
pads, 45 clocked BRAM output bits and 13 architecture boundaries. There are
**9,150 routing paths: 9,129 resolved, 10 Unknown and 11 ambiguous**. None hit
traversal limits or unknown PIP encodings. Width-aware BRAM contracts avoid
traversing unused pins; the 240 omitted routes were unused input dependencies,
not 240 newly established data routes. There are 41 distinct decoder boundary
records, 143 conditional bus observations, 32 runtime RAM16X1D arrays and ten
4096-bit BRAM arrays. Counts describe the configured dependency network, not physically
reachable PCI protocol states or proof of one-hot bus ownership.

The emitted `summary` distinguishes local regression PASS from incomplete
reconstruction. `state_model` records each register's D, CE, effective clock,
SR mode/priority, INIT and configuration evidence, plus buses, RAM write/read
contracts and output data versus disable conditions. Feedback is retained
as state rather than flattened into combinational cycles. Full generated
netlists/traces must remain outside the public repository.

#### Carry/XOR and TBUS Reconstruction

**Verified architecture:** The adapter exports CIN using native
`bel_delta` and the actual BEL grid, following pinned
`re/xilinx/rdverify/virtex/src/lib.rs::verify_slice`. Python models
`CYINIT`, shared `CY0`, `CYSELF`, `CYSELG`, FCY/GCY/COUT, XB, YBMUX and
FXOR/GXOR. A carry stage is `select ? carry_in : carry_data`; a CONST_1
select bypasses that stage. PROD uses the respective LUT's first two
inputs, and XOR uses the LUT result and incoming carry. Missing CIN or
invalid controls remain Unknown. Synthetic coverage exhaustively checks
64 control profiles, 8,192 input assignments and 40,960 output comparisons.

The Q -> A -> K path no longer ends at X8,Y27 SLICE[1].YB. That output
selects GCY; its CYINIT selects native X8,Y26 SLICE[1].COUT. Its CY0 is
CONST_0 and both carry selects use their LUTs. The resulting dependency is
explicit, including the actual registered input-pad sources; it is not an
idle indicator.

**Verified architecture:** TBUS topology follows pinned `verify_tbus` and
`verify_tbus_we`, including lane rotation and BRAM-column skips. JOINER_E
ownership follows `ise-hammer/src/virtex/tbus.rs::ClbTbusRight` rather than
the destination tile's name. OUT_A and OUT_B are decoded independently.
The reverse JOINER PIP is represented as the single bidirectional switch
claimed by the pinned verifier. Unknown joiner/tap bits conservatively
retain possible connections and an explicit Unknown reason.

X4,Y2 TBUS.OUT in REQ's N dependency has eleven configured candidate
drivers: TBUF[0] at columns 2/4/6/8/10/12, and TBUF[1] at 3/5/7/11/13.
Each retains its effective I and T expressions; a driver is active only
when effective T=0. Multiple drivers are never arbitrarily collapsed.
The evaluator distinguishes one driver, agreeing multiple drivers,
opposing logical driver data, no enabled driver and Unknown controls/data.
For these **internal** Spartan-IIE BUFT nets, DS077 p15 specifies default
High and Low-dominant resolution of opposing data. Thus the retained diagnostic
status `contention` means conflicting logical drivers, not electrical damage;
the level is zero. Generic/external tristate nets retain unknown levels when
floating or conflicting. The configured set is verified; mutual exclusion of
the runtime enables has not been proved. N also occurs in S and FRAME's enable path, so this
boundary is relevant to transaction initiation and retention.

#### Local Transition Validation

`--validate-pci-state` checks 15 D/CE relations and four alias contracts:
**19/19 PASS**, covering **2,234 exhaustive local assignments**, including
the alias subchecks. Symbolic cuts and coordinate mappings are emitted
with each result. The checks include the previously documented Q/P/A,
REQ, delayed R, grant-qualified S, FRAME data/OE and delayed IRDY OE
relations. New verified local relations include:

| State/output | Local relation with SR inactive | Boundary |
|---|---|---|
| M = X5,Y3 SLICE[0].YQ | `D_M = !S & (M | (N & c))`, c=X5,Y3 SLICE[0].X | N depends on conditional TBUS |
| h = X5,Y8 SLICE[0].YQ | D=1 | D alone does not establish current state or drain |
| O = IRDY#.FFO | D depends on STOP#, TRDY#, B, d, h, v, z and k | Output data is separate from its tristate |
| FRAME B / FFO | D, CE, SR, clock, INIT and FF modes equivalent | Equality requires matched clock events/initialization |
| FRAME T / FFT | Same complete register contract | External pin remains high impedance while disable=1 |
| IRDY O / X2,Y15 SLICE[1].XQ | Same complete register contract | Establishes the local shadow used in CE equations |

The two different-order LUT cones used as symbolic j are exhaustively
equivalent; this is checked before accepting their correlated variable.
Reset priority, CE hold and asynchronous versus synchronous SR are tested
separately. A disabled CE does not sample a floating D bus. RAM16X1D INIT
is only power-up configuration: runtime reads/writes require an explicit
memory value, and SR is write enable rather than a RAM clear/drain.

#### Protocol and DMA Proof Obligations

| Required reconstruction | Supported result | Remaining evidence |
|---|---|---|
| Bus request | Verified registered REQ D and OE relations | Producer/descriptor stop contract |
| Grant recognition | Verified `D_S=f & !g & i & M & N & !S` | Reachability and conditional N bus ownership |
| Transaction initiation | Inferred association of S with FRAME/OE acquisition | Complete externally observable state transition |
| Address/data phases | Verified local FRAME/IRDY data/OE relations | CBE/DEVSEL correlation and global phase decoding |
| Wait states | Verified dependence on sampled TRDY#/STOP# | Global phase guards and reachability |
| Retry/disconnect | STOP#/TRDY# occur in control equations | Distinct terminal/restart states not established |
| Target/master abort | Unknown | DEVSEL#/timeout/error-state reconstruction |
| Transaction termination | Local FRAME/IRDY deassertion conditions only | Every initiated transaction has a terminal outcome |
| Further starts blocked | Unknown | Stable stop acknowledgement binding REQ/S/N and producers |
| Pending requests/write data | Explicit TBUS and RAM dependencies | Occupancy/pointer meaning and complete BRAM contract |

All six **DMA-quiescence obligations remain Unknown**, individually:

| Obligation | Exact blocker to a proof |
|---|---|
| Producer stopped | No decoded software-observable handshake binds the remote acquisition producer to PCI admission; external input registers and conditional TBUS remain live dependencies. |
| Descriptor engine stopped | No established descriptor-consumption enable/ack contract, including restart and pending requests. |
| New PCI transactions blocked | S depends on M/N and conditional bus data; no proved invariant keeps every future start disabled after a software acknowledgement. |
| Existing transactions terminated | Local FRAME/IRDY equations do not classify every retry, disconnect, target-abort and master-abort path or track outstanding transactions to completion. |
| Internal buffers drained | 32 RAM16X1D runtime arrays and BRAM outputs occur in the expanded cones; no proved occupancy-empty/flush acknowledgement. INIT=0 is not runtime empty. |
| Host-side completion | The FPGA update bitstream contains no contract proving host-bridge posted-write retirement or the necessary Windows DMA ordering/completion. |

Concrete remaining model limits include X1,Y21 BRAM_QUAD_DOUT[7]
(upstream of X7,Y22 FCY and X7,Y23 YQ), X46,Y9/X46,Y13 BRAM output paths
feeding runtime RAM writes, and unmodeled PCILOGIC.PCI_CE/DLL behavior.
Some BRAM routes expose competing pad/CLB and BRAM terminals; they remain
ambiguous until native BRAM width/port/quad connectivity is accounted for.
The five prioritized data/non-clock-control cones each retain 50 proof-boundary
nodes, including 32 distributed RAM and ten BRAM arrays. Their 1,375-1,380
register dependencies include the BRAM input-side closure. Runtime memory
state is modeled but is not an occupancy-empty predicate. Hard-clock boundaries are retained
separately in the register model. These are tool/model limitations that
further offline work can reduce, not proof that the hardware is inherently
ambiguous. Global reachability over the resulting memory-bearing machine
has not been established.

**Conclusion:** Carry/XOR and conditional TBUS reconstruction are implemented
and regression-tested. The full busmaster FSM and a software-observable
stop/drain acknowledgement are not closed. Static update-image analysis
also cannot independently establish the running firmware revision or host
bridge retirement. The x64 driver **must not release quarantined DMA
mappings** on this evidence; `UnknownActive` remains required. No hardware
access, production-driver change or merge of PR #9 is part of this analysis.

Offline verification additionally passes all eight driver dry suites:
35 source contracts and 276 native C cases (DMA completion 24, layout 13,
ownership 17, SG bridge 27, SG sync 149, PnP IRP lifetime 8 and PnP
publication 38). These protect the existing quarantine behavior; they
do not count as hardware tests or DMA-quiescence proof.

### Formal State Analysis

`tools/fpga/virtexe_symbolic.py` consumes the existing decoder's private JSON;
it is not a second bitstream decoder. Its optional dependency is pinned in
`tools/fpga/requirements-symbolic.txt` to `z3-solver==4.15.4.0`.
Full generated networks, memory contents, SMT assignments and traces remain
outside this public repository.

#### Implemented Contracts

- All **1,405 decoded FFs** have supported contracts: simultaneous pre-edge D
  sampling, CE hold, INIT, synchronous SR on selected edges, asynchronous SR
  between edges and SR priority over CE. Unsupported latch/reverse-SR contracts
  leave state unconstrained and label the result Inconclusive.
- The **three effective clock domains** have separate symbolic edge events.
  Explicit schedules are supported. Constant clocks cannot edge; opposite
  polarities cannot rise simultaneously. BUFGCE/DLL generation, phase and
  startup relationships are **not** proved, so firmware witnesses are abstract.
- **32 RAM16X1D arrays** retain configuration INIT separately from runtime state,
  asynchronous read addresses and pre-edge writes. **Ten RAMB4 arrays** use
  shared 4096-bit storage and **45 observed output-latch bits**. Widths 1/2/4/8/16
  select the required address and data pins. The pinned `gen_ramb_v` mapping
  omits the lowest `log2(width)` physical address pins; they are not extra
  address bits. X1,Y21 uses 4-bit ports; X46,Y9 and X46,Y13 use 16-bit ports.
- BRAM EN gates operations; reads update clocked output latches, writes mirror
  input data, and synchronous RST clears the output, not the RAM. Other-port
  writes do not asynchronously update an inactive output. Collision results,
  setup-window overlap, same-port RST/WE priority, missing INIT and startup
  latch values remain symbolic. No FIFO-empty claim follows from stored zeros.
- Internal TBUS uses the DS077 Low-dominant/default-High contract. Ownership
  predicates separately cover zero/one/multiple agreeing/conflicting drivers.
  Unknown enables/topology are never assigned a convenient constant.
- PCI sampled input, FPGA intended O, effective T and other-agent O/enable are
  separate. External contention is not resolved using the internal BUFT rule.

Memory references: [DS077 pp15-16](https://docs.amd.com/v/u/en-US/ds077),
[XAPP173 pp2-8](https://docs.amd.com/v/u/en-US/xapp173), and pinned public
`re/xilinx/v2xdl-verify/src/ramb.rs::gen_ramb_v`. INIT extraction still stops
at the adapter's unsupported BRAM_DATA bit rectangle; main-frame data must
not be reinterpreted as BRAM INIT.

#### Solver Scope And Assumptions

The API supports reachability, invariant-violation, explicit enabled-action
deadlock and exact state/array lasso queries. A finite stalled prefix is not
called livelock. Deadlock requires an independently justified enabled-action
predicate. No actual transaction deadlock/livelock query was manufactured from
an unknown terminal-state or acknowledgement signal.

Results carry solver version, bound, timeout, base-constraint consistency,
initialization mode, clock domains, assumptions, Unknown reasons and private
state/input/bus-driver traces. SAT with incomplete semantics is
`Inconclusive / abstract_candidate`, not a hardware counterexample. SAT for a
complete synthetic transition model can establish bounded reachability or a
valid invariant counterexample. Bounded UNSAT is always Inconclusive for the
unbounded question. An inconsistent initial/environment model is also flagged;
it cannot masquerade as successful exclusion of a bad state.

Firmware runs start from decoded FF/distributed-RAM power-up INIT, not an
arbitrary runtime register assignment. BRAM INIT/output startup is unknown.
The effective clocks are independent events. PCI profiles are explicitly
`electrical-only` or `target-response`; the latter additionally requires
externally supplied STOP#/TRDY# assertions to be accompanied by DEVSEL# while
the FPGA releases those target signals. Neither profile assumes a bounded grant,
bounded target response, eventual progress, producer stop, descriptor stop or
DMA acknowledgement. DEVSEL# is present as an **uncorrelated environmental
Unknown**: board-to-IOB mapping and downstream timeout/abort logic have not been
established. This is not a full PCI-specification environment model.

Example offline invocation (all output paths must be private):

```powershell
python -m pip install -r tools/fpga/requirements-symbolic.txt
$env:WR6K_FPGA_SYMBOLIC = '1'
./tests/dry/test-fpga-decoder.ps1
python -B tools/fpga/virtexe_symbolic.py C:/private/network.json --bound 8 --timeout-ms 15000 --environment target-response --output C:/private/smt.json
```

#### Actual Results

The synthetic suites pass **39 routing tests + 36 SMT tests**, including
register/reset/CE behavior, multiple clocks, mutable memories, BRAM collision
unknowns, internal versus external bus semantics, SAT traces, deterministic
mocked timeout handling and contradictory assumptions. Private frame calibration,
native architecture **47/47**, and local equations/aliases **19/19** pass.
All eight driver dry suites also pass: **35 source contracts + 276 native C
cases**. These are offline tests, not hardware or Windows driver-load tests.

The private model contains 1,405 supported registers, 32 distributed arrays,
ten BRAM arrays, 45 BRAM output bits and three effective clock domains.
It retains **232 distinct symbolic Unknown reasons**; these include guarded
collision/priority cases as well as static architecture boundaries, not 232
independent physically active faults.

At bound 2, no multiple-driver TBUS witness exists in the power-up model;
zero/one-driver cases are SAT. At bound 8, agreeing and conflicting multiple
drivers and the mutual-exclusion violation are SAT. Every result remains
Inconclusive. Arbitrary runtime-cut queries are emitted separately and must
never be substituted for power-up reachability. The full traces include the
selected clock events, environmental inputs and active driver indices.

Final runs use bounds **2, 8 and 16**, a 15,000 ms per-check timeout and the
`target-response` profile: **27 bounded queries, 18 SAT / 9 UNSAT / 0 unknown**,
with satisfiable base constraints throughout. All twelve repeated arbitrary-cut
queries are SAT. None of the 27 firmware results establishes an unbounded
hardware property. At bound 16, REQ#/FRAME#/IRDY# driven-low observations become
SAT too; they were UNSAT at bounds 2 and 8. This directly demonstrates why the
shorter bounds cannot establish permanent inactivity.

One bound-8 abstract trace has opposing logical values from X2,Y2 TBUF[0] and
X4,Y2 TBUF[0] at step 4. Its assumptions are decoded power-up FF/LUT-RAM state,
unconstrained BRAM INIT/output startup, independent effective clock events,
symbolic unresolved paths/collisions/hard blocks, the stated external PCI
profile, and no stop/acknowledgement or fairness constraint. Step 4 is where
that particular witness conflicts, not a proved shortest hardware execution.
The internal bus resolves Low; reachable physical one-hot ownership remains
unproved.

REQ#/FRAME#/IRDY# driven-low queries are bounded observations, not named PCI
phases. In particular, seeing sampled IRDY#=TRDY#=0 can be supplied by another
agent and is not proof that this FPGA completed a data phase. No new actual
grant/start/retry/disconnect/timeout/abort/termination transition is established
beyond the clocked local equations already documented above.

#### Minimal Evidence And Driver Handoff

All six quiescence requirements remain independently **Inconclusive/Unknown**;
the obligation table above remains the controlling result. The strongest
supported partial predicate is instantaneous FPGA output inactivity:
REQ# not actively asserted and FRAME#/IRDY# released or driven deasserted.
It says nothing about pending producers, descriptors, FIFO occupancy, later
restarts or posted host writes. It is not a software-observable acknowledgement.

The existing x86 evidence was checked against
`docs/dma-lifetime-and-timeout.md`: IIMST bit 0, IIMCL=0, MTTCTL=0, an IRQ/event
and INTEN masking still have no established idle/retirement contract. No new
MMIO sequence is proposed. The smallest missing evidence set is:

1. Correlate the **actual stop command and a readable acknowledgement bit** to
   decoded producer/descriptor admission and PCI-start guards. Establish that
   the acknowledgement remains true until software explicitly restarts.
2. Close the relevant **unresolved/ambiguous routes** (21 currently retained),
   particularly X1,Y21 quad output and X46,Y9/Y13 RAM-write dependencies,
   plus PCILOGIC.PCI_CE, DLL.LOCKED and
   the three effective clock relationships. Supply BRAM INIT/startup and
   collision timing only where those properties actually depend on them;
   do not expand unrelated cones merely to increase coverage.
3. Correlate **DEVSEL#/timeout/abort states and buffer pointers/occupancy** to the
   same acknowledgement. Prove no new start, every outstanding transaction
   terminal, and no pending internal data under an explicit legal environment.
4. Independently establish the **Windows DMA/host-bridge completion contract**
   and confirm the executing firmware revision. An update image cannot provide
   either fact by itself.

Until those conditions are established, the driver implementation workstream
must preserve `UnknownActive`, pinned mappings and quarantine. This analysis
changes no production driver, performs no hardware access/programming/reset,
and does not authorize release, speculative register writes, or merging PR #9.

The focused [legacy-to-FPGA evidence matrix](legacy-dma-abort-bus-idle-audit.md#cross-layer-evidence-matrix)
and the target continuation below record the correlation boundary. The newer
target analysis adds an address-phase bank, configuration predicates, local
DMA-offset write gates and IIMCL storage candidates. Complete BAR qualification,
target read acceptance and software-visible idle semantics remain unproved.
No stop/ack symbol was introduced or bound to an arbitrary FF. Consequently
post-ack restart, outstanding-transfer, IRQ-before-idle and reset-hiding-status
properties are not yet executable as certified WR6k protocol queries.
The [passive observation plan](dma-quiescence-read-only-plan.md) describes the
smallest independent observations, without authorizing hardware execution.

### Focused PCI Target MMIO Decode (2026-10-10)

**Result: BAR routing resolved; conditional readback reconstructed; no DMA
quiescence acknowledgement.** The
machine-readable [register evidence matrix](pci-mmio-register-evidence.json)
separates legacy access facts, verified local Boolean predicates, inferred BAR
correlations and unknown safety implications. A Verified local predicate is
not a Verified end-to-end MMIO transaction. The remaining blockers below prevent
claiming completion of the full register/stop-status reconstruction.

#### Reproducible Scope

`tools/fpga/virtexe_mmio.py` reuses the existing configuration-aware Router and
LogicAnalyzer, pinned Project Combine `234343d23e737e57f2727630e19008b509d7d522`,
and its public `BOND87` package block. It does not add another bitstream decoder.
All 32 PCI AD pins, four C/BE pins, IDSEL, DEVSEL#, FRAME#, IRDY#, TRDY#, STOP#,
GNT#, REQ# and RST# are assigned from derived card connectivity. Six separately
checked transmit pairs (D0..D4, D10), three receive pairs (D9..D11) and RX_SYNC
are additional focused roots, not a claim of complete LVDS packet decoding.

The resulting private report contains **3,958 logic nodes**, **21 input-fed
capture groups**, and **64 explicitly retained boundaries**. These counts
include intentionally disabled input views of output pins and clock/hard-block
boundaries; they are not 64 missing MMIO registers. Full networks and reports
stay in private TEMP paths. The CLI refuses output within this repository or
overwriting its input image. Public output is limited to derived facts here.

```powershell
$env:WR6K_ROUTING_ADAPTER = python -B tools/fpga/virtexe_xc2s200e_decode.py `
  --build-adapter --project-combine $env:WR6K_PRJCOMBINE
python -B tools/fpga/virtexe_mmio.py $env:WR6K_FPGA_BINARY_205 `
  --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER `
  --output "$env:TEMP/wr6k-target-private.json" --summary "$env:TEMP/wr6k-target-summary-private.json"
$env:WR6K_MMIO_REPORT = "$env:TEMP/wr6k-target-private.json"
python -B tests/dry/test_fpga_mmio.py -v
```

The report is checked against the supplied firmware SHA-256 and successful
frame calibration before private regressions run. `WR6K_FPGA_SYMBOLIC=1` also
enables the existing Z3 model's focused conditional-clear query. Effective
clocks remain explicit; no new clock relationship or remote protocol is assumed.

#### Address Phase, Configuration And BAR Storage

**Verified:** A common PCI-clocked bank captures each sampled AD[31:0] bit
separately. Its gate is `X7,Y11 SLICE[1].Y`, with exact D-path predicate
`!FRAME_sampled && FRAME_previous && !X5,Y5 SLICE[0].YQ`.
The previous-FRAME FF is `X12,Y12 SLICE[1].YQ`. The same Boolean condition
drives `X12,Y15 SLICE[1].X` and `X7,Y12 SLICE[0].X`. Thus the C/BE bank under
the former gate is address-phase **command capture**, not data-phase byte
enables. The capture registers asynchronously reset from PCI RST#.

Six same-gated predicates compare sampled AD[7:0] exactly to
`0x004, 0x00C, 0x010, 0x014, 0x018, 0x03C`, verified over all 256 assignments
each. These are **PCI configuration offsets**, not BAR-relative DMA register
addresses. In particular, config `0x00C` is not the driver's BAR0 START.
The command register `X7,Y25 SLICE[1].YQ` recognizes C/BE nibble `0xB` exactly.
IDSEL and low address bits also enter the configuration-target recognition cone.

**Verified structure / Inferred PCI interpretation:** Separate config-write
storage banks at offsets `0x10`, `0x14`, `0x18` are consistent with BAR0, BAR1,
BAR2. The first/third banks capture address bits 9..31; the second captures
18..31. Common gates also require the captured `0xB` command and the relevant
active-low C/BE byte. This suggests 512-byte, 256-KiB and 512-byte windows,
respectively; full config sizing readback has not been certified.
The `0x18` and `0x14` comparison cones terminate in
`X6,Y26 SLICE[1].XB` and `X8,Y27 SLICE[1].YB`. The BAR0 compare is
`X7,Y25 SLICE[1].XB`. All three functions are now **Verified** by Z3
equivalence (three UNSAT mismatch queries): sampled AD equals the respective
runtime storage bank over bits 9..31 / 18..31 / 9..31, and sampled C/BE command
is exactly one of `0x6, 0x7, 0xC, 0xE, 0xF`. Storage and sampled inputs remain
independent arbitrary variables, not fixed BAR values or reset assumptions.
No bank address is promoted to the actually enumerated hardware BAR value.

#### Tile-Owned HEX Mux Correction

The AD20 ambiguity is **resolved**, including its BAR0 and DEVSEL dependencies.
Pinned `public/virtex/src/defs.rs::wire_from_mux` distinguishes the tile-local
`HEX_*_MUX` helper from the shared conductor. The same tile switchbox has a
`ProgBuf` from that helper to the corresponding conductor; `grid.rs::wire_pips_bwd`
enumerates overlapping tile owners at a shared cell. Without owner qualification,
following one active buffer accidentally combines both pre-buffer mux views.
`rdverify/virtex/src/lib.rs::verify_device` aliases the helper for raw-database
verification; that alias is not evidence that two output buffers are enabled.

The adapter now attaches the **same-switchbox, same-cell matching ProgBuf** to
HEX mux arcs. The router requires that owner buffer to be active. It does not
filter arbitrary BRAM sources, pick a convenient terminal or alter Project
Combine. Missing/duplicate buffer encodings remain Unknown; two active owners
remain ambiguous. Synthetic tests cover each owner alone, both active and
unknown guards. Native feature tests also flip the two guard bits **in memory**
and verify that the formerly disabled BRAM mux becomes active, not suppressed.

For this route, IO_W `(X0,Y24)` selects AD20 IQ with mux bits at
`(2226,434), (2227,434), (2228,432), (2228,434)` equal `1,1,0,0`.
Its output buffer `(2226,436)` is active. BRAM_W `(X1,Y21)` selects quad DOUT[7]
with `(2120,442), (2119,442)` equal `0,0`, but its inverted buffer bit
`(2119,443)` decodes inactive. Thus `X7,Y23 SLICE[0].YQ.D` is exactly
`X0,Y24 IOI[1].IQ`. BAR0 compare and DEVSEL D/disable cones now have no routing
boundary. This is database/configuration-backed evidence, not a second ISE
hardware experiment or confirmation of the instrument's running image.

#### Local DMA-Offset Write Paths

**Verified:** The following local predicates are exactly
`W && H && ((A & 0x1CC) == value)`, exhaustively factored without tying off
other inputs. `A` is the captured 32-bit bank, `W` is
`X6,Y16 SLICE[1].YQ` (captured C/BE command bit 0), and `H` is
`X7,Y12 SLICE[0].YQ` (BAR0-hit candidate state). Bits 0,1,4,5 are absent from
these local predicates. This reveals possible decoder aliases, not permission
to perform unaligned or undocumented accesses. H's comparison is resolved;
its accepted-transaction timing remains a separate obligation.

| Legacy BAR0 value | Verified local gate(s) | Inferred local destination |
|---|---|---|
| `0x040 SGTA` | X17,Y13 SLICE[0].Y / SLICE[1].X | Two TBUS-fed address/load banks; full descriptor-bit layout unknown |
| `0x044 IIMTC` | X15,Y12 SLICE[0].X / X16,Y12 SLICE[0].Y | Two 16-FF TBUS-fed load banks |
| `0x048 IIMCL` | X17,Y13 SLICE[1].Y | Three one-bit PCI-clocked replicas: X23,Y14/Y20/Y25 SLICE[1].YQ |
| `0x080 INTST` | X16,Y8 SLICE[0].X | Feedback/ack candidate, including X22,Y11 SLICE[0].XQ |
| `0x084 INTEN` | X12,Y13 SLICE[0].X | Six enable candidates, including bit-0 candidate X16,Y7 SLICE[0].YQ |
| `0x00C START` | X17,Y12 SLICE[0].X | X17,Y4 SLICE[1].YQ |

The three IIMCL candidates share `X21,Y1 TBUS.OUT` as D and the `0x048` gate
as CE. Their complete decoded FF contracts verify reset priority, CE hold and
clocked data capture. TBUS includes an AD0 producer but also configuration,
status and mutable FIFO producers. Per-byte target write suppression, unique
write-data ownership and target-handshake timing are **not** established by
the CE predicate alone. Wider/partial writes are not certified.

The control candidates have actual configured dependency paths into address/
count-load logic and REQ D logic. One path starts at X23,Y14 SLICE[1].YQ,
passes X26,Y18 SLICE[1].X/YQ, X19,Y10 SLICE[0].YQ and
X19,Y7 SLICE[1].XQ, then reaches the previously recovered REQ D cone.
This is a dependency, not a proof of monotone transfer inhibition or retirement.

#### Readback And PCI/Acquisition Boundary

**Verified topology:** AD0 read data includes
`X11,Y7 SLICE[0].Y -> X10,Y1 TBUF[0].O -> TBUS`, before the pad's output FF.
The TBUF disable is `X11,Y11 SLICE[1].X`, depending on registered read-selection
states. Other TBUF producers include live AD0, configuration data and mutable
RAM16X1D FIFO data. All 32 output cones retain runtime arrays; INIT must not be
substituted for current readback contents.

**Verified conditional readback, not a complete software read:** Let
`C = X22,Y11 SLICE[0].YQ`, `R = X11,Y8 SLICE[0].YQ`,
`B = X13,Y14 SLICE[1].YQ`, `D = X9,Y19 SLICE[0].YQ`,
`P = X11,Y7 SLICE[0].YQ`, and `IRQ = X22,Y11 SLICE[0].XQ`.
With only the named BAR hit asserted and the captured address fixed, five Z3
mismatch queries are UNSAT for the data at `X11,Y7 SLICE[0].Y`:

| Selected access | Exact conditional bit-0 source | Boundary classification |
|---|---|---|
| BAR0 `0x048 IIMCL` | `B ? (D ? C : R) : C` | Mixed local/receive, not control-latch readback |
| BAR0 `0x04C IIMST` | Same | Mixed local/receive, no certified busy/idle meaning |
| BAR0 `0x080 INTST` | `IRQ` | Local bit 0 |
| BAR0 `0x084 INTEN` | `IRQ` | Local INTST bit-0 alias, not enable storage |
| BAR1 `0x080 MTTCTL` | `B && !D ? R : P` | Receive data with local previous-value hold |

Other word bits, accepted-read meaning and read side effects are not implied.
In the BAR0 0x48/0x4C case, `B == D` selects C; only B's rising selection
interval selects R. R captures X32,Y19 SLICE[1].YQ under
X32,Y20 SLICE[1].YQ on `X24,Y0 BUFGCE[0].O`. The PCI-clocked pipeline is
`X13,Y16 SLICE[0].YQ.D = RX_capture_enable || X13,Y17 SLICE[0].YQ`,
then B, then D. Its receiving clocks are distinct; this is not a proven CDC
or remote acknowledgement contract.

The three registered BAR read selectors have D=`hit && !W` and
CE=`!X5,Y15 SLICE[0].XQ || hit`. The status TBUF drives only when that timing
state is true and any registered BAR read selector is true. Other TBUS producers
remain modeled. All **32** AD output FFs use PCI-clock OCLK and
`PCILOGIC.PCI_CE` as OCE. Their actual contracts hold the previous pad data if
PCI_CE is low, regardless of a changed internal read value. DEVSEL#/TRDY# O/T
are PCI-clocked resettable FFs; both T paths capture `X4,Y15 SLICE[0].X`.
This closes their configured routes but not the missing PCI_CE function or
unique ownership at a completed target data edge. Do not treat these mux proofs
as PCI protocol validation or permission to poll a register.

A bounded **constructed-runtime** IIMST read fixture now follows the actual
configured transitions for eight PCI-clock edges: disjoint BARs, memory decode
enabled, DMA/target registers initialized to their decoded idle values, a
command-6 address phase, active byte enables, IRDY wait and feedback from driven
AD/DEVSEL/TRDY/STOP pads. No receive/fast-clock edge or RAM/BRAM INIT is assumed.
At step 5 DEVSEL and TRDY are actively low and AD is enabled. Both queries are
**SAT**: unconstrained hardblock PCI_CE held low returns old AD0=1; PCI_CE held
high returns current AD0=0. Under this fixture, the ownership-mismatch query
at the preceding output-load step 4 is **UNSAT**: X10,Y1 TBUF[0] is the only
enabled producer on AD0's status TBUS. This does not assume runtime FIFO bits.
This is an abstract demonstration of the exact
missing hardblock contract, not a hardware stale-data defect, a boot-reachable
trace or complete PCI compliance proof. The fixture's initial register values
are explicit assumptions, not a substitute for configuration/restart invariants.
The local C equation is **Verified**:
`C_next_D = (IIMCL_candidate && C) || (X26,Y18 SLICE[0].YQ && X22,Y21 SLICE[1].X)`.
A separate verified post-clear equation is
`X25,Y20 SLICE[1].Y = !IIMCL_candidate && (C || X25,Y20 SLICE[1].XQ)`.
Neither equation assigns a completed-host-write or FIFO-empty meaning.

The six checked TX pairs have complementary, registered P/N data driven from
local transmit-mux FFs on `X24,Y0 BUFGCE[1].O`. A newly verified 16-FF bank
retains captured AD[17:2] under
`S = BAR1_hit && (!X16,Y14 SLICE[1].YQ || X10,Y14 SLICE[1].YQ)`.
Two 16-FF TBUS payload banks use S&&W. The staging registers are PCI-clocked;
their transmit consumers use `X24,Y0 BUFGCE[0].O` before fast-clock output muxing.

Eleven exact staging D-functions select retained address versus retained TBUS
data when their outer mux selects its zero branch:

| Transmit staging FF | Retained address bit | TBUS snapshot FF | PCI AD producer on that bus |
|---|---|---|---|
| X28,Y5 SLICE[0].XQ | 2 | X20,Y3 SLICE[0].YQ | 0 |
| X24,Y2 SLICE[1].XQ | 3 | X20,Y3 SLICE[0].XQ | 1 |
| X27,Y3 SLICE[0].XQ | 4 | X23,Y3 SLICE[1].YQ | 2 |
| X28,Y3 SLICE[1].XQ | 5 | X23,Y3 SLICE[1].XQ | 3 |
| X28,Y3 SLICE[0].XQ | 6 | X22,Y3 SLICE[0].YQ | 4 |
| X25,Y2 SLICE[0].XQ | 7 | X24,Y2 SLICE[0].YQ | 5 |
| X27,Y2 SLICE[0].XQ | 8 | X26,Y1 SLICE[1].YQ | 6 |
| X26,Y1 SLICE[0].XQ | 9 | X26,Y1 SLICE[1].XQ | 7 |
| X26,Y5 SLICE[1].XQ | 10 | X26,Y5 SLICE[0].YQ | 8 |
| X24,Y5 SLICE[1].XQ | 11 | X20,Y5 SLICE[0].YQ | 9 |
| X26,Y4 SLICE[0].XQ | 12 | X23,Y4 SLICE[0].YQ | 10 |

Address bits 2..9 use outer selector X28,Y6 SLICE[1].Y and local phase
X25,Y1 SLICE[0].YQ; bits 10..12 use X28,Y5 SLICE[1].Y and phase
X25,Y1 SLICE[1].YQ. Phase zero selects address, one selects retained TBUS data,
verified for all eight local assignments per field (88 assignments). The
previous address-bit-8 label for X26,Y5 SLICE[1].XQ was incorrect: following
the complete capture bank proves address bit **10**, paired with payload bit 8.
All 32 payload snapshot FFs have one identified PCI AD producer each across
their shared TBUS components; this is topology, not proof of enabled ownership.
The AD producer identity
does not prove it is the only enabled TBUS driver during an accepted write.
These are conditional field identities, **not** MTTCTL's exact packet slot/opcode,
command-valid, retry or remote response semantics.
BAR1 `0x080 MTTCTL` remains an inferred acquisition command; its downstream
state and returned acknowledgement are **Unknown**. Local config storage and
the DMA-offset CE/storage candidates demonstrate why treating every BAR access
as either entirely local or entirely acquisition-controller-owned is invalid.

#### PCILOGIC Source Audit And Exact Missing Contract

**Not resolved:** No applicable manufacturer PCILOGIC transfer/timing model is
available in the inspected local evidence. Offline inventory checked the usual
Xilinx installation paths, project/download/private evidence paths, applicable
Verilog/VHDL/library filenames and installed Altium HDL/library text, as well as
the pinned Project Combine source/database/docs. No ISE installation or matching
UNISIM/SIMPRIM primitive was found there. This is a bounded inventory, not a claim
that no such model exists elsewhere. No network lookup or hardware access was
performed. Manufacturer behavior must not be attributed to reverse-engineering
configuration metadata.

Exact available sources at `234343d23e737e57f2727630e19008b509d7d522`:

- `public/virtex/src/defs.rs::PCILOGIC`: three fabric inputs and PCI_CE output;
  class-wide PCI_DELAY declaration, no behavioral equation.
- `databases/virtex.txt::PCI_W_VE,PCI_E_VE` and
  `re/xilinx/ise-hammer/src/virtex/misc.rs::{add_fuzzers,collect_fuzzers}`:
  **this device uses PCI_W_VE**, whose I1/I2 inputs have separate active-low
  inversion encodings and whose I3 input is fixed-polarity. All three effective
  configured input inversions are false. **No PCI_DELAY attribute is encoded
  for VE.** The collector checks all four global PCIDELAY setting differences
  empty for VE; only the different PCI_W_V/PCI_E_V tiles encode a delay selector.
  This is not proof of zero propagation delay or permission to reuse another
  family's behavioral model. The fuzzer itself flags uncertainty about ISE's
  I1/I2 constant-versus-inversion naming.
- `re/xilinx/rdverify/virtex/src/lib.rs::{verify_pcilogic,verify_iob}`:
  two previously omitted **dedicated ready inputs** connect to PCIIOB.PCI:
  IRDY -> X0,Y15 IOI[3].PCI (board pin 24),
  TRDY -> X0,Y14 IOI[1].PCI (board pin 27). The adapter now exposes both endpoints.
  These connectivity claims do not establish whether the dedicated taps equal
  asynchronous I, registered IQ, their inverse, or a delayed internal signal.
- `docs/src/virtex/pcilogic.md`: tile references only, no transfer function.
  The related Virtex-II/Spartan-3/Spartan-6 PCILOGICSE descriptions are not
  interchangeable Spartan-IIE behavioral authority.

The three fabric driving functions are locally Verified:
I1 = `X2,Y14 SLICE[0].XQ || !X5,Y15 SLICE[0].YQ`;
I2 = `X3,Y13 SLICE[1].Y || X4,Y9 SLICE[1].XQ || X2,Y21 SLICE[1].XQ`;
I3 = `X2,Y15 SLICE[1].XQ || !X5,Y5 SLICE[0].YQ`.
These are not a PCI_CE equation. `pcilogic_contract` reports configuration and
the exact missing behavior but **retains Unknown**. Symbolic tests exercise all
eight fabric-input combinations and retain both CE-dependent output outcomes;
no constant, arbitrary Boolean equation, hidden reset or delay is introduced.

#### Joint Bounded Read Diagnostics And Wait-State Obstruction

The eight-PCI-edge fixture is extended to all four BAR0 priority offsets and
BAR1 MTTCTL. Assumptions remain: constructed initial FF state, memory decode
enabled, disjoint BARs, command 6, all data bytes enabled, high RST#/GNT#, low
IDSEL, driven-pad feedback, arbitrary runtime memories, and **no remote/fast
clock edges**. They do not establish boot/configuration reachability. A diagnostic
CE=1 assumption is explicitly not a recovered PCILOGIC model.

For each BAR0 offset 0x48/0x4C/0x80/0x84, the base constraints are SAT and the
joint mismatch query is UNSAT for: captured address and all four command bits,
BAR0
selection, unique AD0 status-TBUS ownership at step 4, active DEVSEL/TRDY/AD
at step 5, AD0 loading the preceding-step local mux value, and pad release at
step 7. **STOP# is actively low together with TRDY# at step 5**, while FRAME#
is high and IRDY# low: the constructed fixture has a data/disconnect phase,
not a proven ordinary target completion or burst-read contract. The proven mux
equations above distinguish C/R from IRQ; this is still
bit 0, not a certified 32-bit software read or read-side-effect contract.
For BAR1 0x80, address capture/selection works but TRDY remains high through
steps 5..8 and the response-selection pipeline does not pulse without remote
edges (base SAT, mismatch UNSAT). No invented acquisition response is supplied.

**Abstract counterexample to extrapolating the fixture:** delaying initiator
IRDY until step 7, even with diagnostic CE=1, permits ready AD data at step 5
while IRDY is high, followed by AD disable at step 6 before initiator readiness.
The SAT witness demonstrates that the current constructed initial state and
remaining unknown contracts do not establish a wait-state/stability invariant.
It is **not** a boot-reachable trace, actual hardware defect or reason to modify
the production driver. Together with the CE-low/CE-high witnesses, it keeps the
accepted-read result **Inconclusive**, not conditionally certified by CE alone.
All UNSAT results here are bounded diagnostics, not unbounded PCI compliance.

#### Exact Link Muxes And Legacy Command Projection

The retained read/write candidate `X12,Y17 SLICE[1].YQ` captures W on PCI clock
under `BAR1_hit && !X16,Y14 SLICE[1].YQ && !X10,Y14 SLICE[1].YQ`.
This differs from the address/payload gate; its eventual wire slot is Unknown.
Within the eleven address-phase fields, driver offsets project as follows:

| Legacy BAR1 access | High conditional staging fields; all other identified fields low |
|---|---|
| MTTCTL 0x080 | X25,Y2 SLICE[0].XQ (address bit 7) |
| MTTRGO 0x084 | Previous plus X28,Y5 SLICE[0].XQ (bit 2) |
| MAMRGO 0x064 | X28,Y5 SLICE[0].XQ, X28,Y3 SLICE[1].XQ, X28,Y3 SLICE[0].XQ (bits 2,5,6) |

These identities agree with the legacy x86 accesses already recorded in
`legacy-dma-abort-bus-idle-audit.md` (0x171DE launch, 0x163B2/0x137C4 MTTCTL).
They are conditional local address projections, **not named wire opcodes** or
proof that a PCI write reaches a remote register exactly once.

Six fast transmit D-functions are exhaustively verified over 64 assignments
each (384 total). They all have form `p == e ? A : select ? C : B`, with lane-
specific control/source identities retained in tests and the JSON matrix.
For lanes D1/D2/D3/D4, C is respectively the identified address/payload stage
2/0, 4/2, 7/5, **10/8**. Thus these conditional fields now reach identified
physical transmit lanes, not only staging FFs. The fast mux FF and complementary
P/N pad FF are consecutive registers on the effective fast clock; simultaneous
pre-edge sampling adds an output-register stage. The source-group labels A/B/C
do not assert temporal slots or packet-valid semantics. Exact scheduling, all
remaining address bits, CDC stability, retries/buffering and the acquisition
decoder remain unknown. RX capture remains remote-clocked before the PCI
response pipeline; no response is equated with drained FIFOs or retired DMA.

#### Exact Blockers And Targeted Next Evidence

1. **Exact missing hardblock input:** Supply the applicable Spartan-IIE
   `PCILOGIC` behavioral/timing model (e.g. its authoritative ISE primitive),
   including I1/I2/I3, dedicated IRDY/TRDY PCIIOB.PCI tap behavior, PCI_CE
   settling/edge behavior and reset/startup. A VE-applicable model is required;
   this tile has no encoded PCI_DELAY setting.
   Pinned `public/virtex/src/defs.rs` declares only pins/attributes;
   `ise-hammer/virtex/misc.rs` collects delay/inversion bits, not its transfer
   function. No Boolean combination of I1/I2/I3 is invented here. This blocks
   proving that an accepted target read loads current AD data. The fully expanded
   AD0 output cone now has **no mixed-route boundary**: remaining hardblock
   boundaries are PCI_CE, three effective BUFGCE clocks and DLL.LOCKED.
2. **Read/packet timing and identity:** Supply the effective clock/CDC contract,
   DLL/reset assumptions and the exact reverse-SR primitive contract for
   X32,Y7 SLICE[0].XQ (FF_REV_ENABLE=1, synchronous SR; retained as Unknown by
   SMT). With these, verify target acceptance, TBUS ownership, output enable,
   TRDY/DEVSEL and completion jointly. Correlate BAR1 `0x080` staging, command
   slot/valid/read-write fields and returned response with the acquisition-side
   decoder or an existing passive timestamped PCI+LVDS capture. Specifically
   identify which remote register bit, if any, controls producer stop and how
   its response survives queued work/restart. No new hardware capture is
   authorized by this analysis request; an existing offline artifact suffices.
3. **Safety closure:** Even with the above, prove descriptor admission blocked,
   no new PCI start, all outstanding transactions terminal, pending buffers
   empty and acknowledgement stable until explicit restart. Actual card/FPGA
   revision and Windows/host-bridge DMA completion remain independent evidence.

The existing symbolic model was reused without an invented stop/ack variable.
An arbitrary-runtime one-step query permits all three IIMCL candidates to
change 1->0 on their actual CE/PCI-clock/data path while REQ remains actively
Low: **SAT, 219 explicit unknown abstractions**. This is an abstract
counterexample to using conditional latch clear as an idle certificate, not
a reachable-hardware counterexample or proof of a stop command's failure.

Actual execution: **12 synthetic MMIO tests + 28 private MMIO tests passed**,
including native enabled/disabled HEX ownership, three BAR equivalences, five
conditional readback equivalences, 64 AD-output hold cases, BAR1 address/payload
staging and response-clock checks, all 32 payload producer identities, eleven
conditional fields, six fast transmit muxes (384 assignments), read/write capture,
four joint local-read diagnostics, BAR1 response absence, the late-IRDY abstract
counterexample, bounded TBUS ownership and two read/PCI_CE witnesses, plus the
existing latch cases and Z3 query. Synthetic tests cover configuration/polarity,
unsupported primitive cases, pre-edge output load/hold/reset, remote-to-PCI
response latency and retention of unknown CE behavior.
The **42 routing** and **39 symbolic** tests, frame calibration, 50 native architecture
checks, 19 PCI equations (2,234 assignments), and eight driver dry suites
(311 assertions) also passed. No WDK build, hardware test, MMIO access,
programming or reset experiment was performed. `UnknownActive` and quarantine
remain unchanged; PR #9 remains open and unmerged.
