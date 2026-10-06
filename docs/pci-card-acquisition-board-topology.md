# PCI interface card and acquisition-board hardware boundaries

**Evidence reviewed:** the user's two **one-page, A2** uploads:
`PCI Card.pdf` (PCI interface schematic, title area names Perigee LLC
and dates it 12 March 2003), and `Overview.pdf` (LeCroy Corporation
acquisition-board **top-level overview**, drawing/model area shows
`901586-XX`). Both were inspected as rendered schematic images,
not merely as flattened PDF text. `Overview.pdf` contains vector
drawing primitives with no ordinary extractable text. This page
records **derived factual observations and bounded inferences**,
not a copy or republication of either source drawing. Do not
upload the user's original schematic PDFs to the public repository;
the Overview itself carries a LeCroy proprietary-information notice.

**Document scope:** the PCI card is not the same PCB as the LeCroy
acquisition board or the ProBus probe/front-panel module.
All three layers must be distinguished during driver reconstruction.

## Directly visible PCI interface card (PCI Card.pdf, sheet 1)

| Schematic reference | Observed role / part | What is actually established |
|---|---|---|
| `U3` | Xilinx **XC2S200E** Spartan-IIE FPGA | Physical endpoint of the buffered conventional PCI AD/control signals and of the RX/TX link signals on this card. It is the PCI-side FPGA, **not** an `ADC+MAM` FPGA from the acquisition-board overview. |
| `U1/U2/U4/U5/U7` | Pericom **PI5C3861** bidirectional bus switches | Buffer/translate the conventional PCI-side signal groups between 5 V and the 3.3 V FPGA domain. The original schematic explicitly notes that they do not actively boost 3.3 V back to 5 V; 3.3 V is accepted as a PCI high level. |
| `U6` | Xilinx **XC18V02** configuration PROM | Spartan configuration source, **not** the AP015 I2C identification EEPROM or the Dallas ID device. Assembly options shown for PROM, JTAG and remote configuration via link are alternatives; actual stuffing on the user's board must not be guessed. |
| `U11` | Dallas/Maxim **DS2433**, labelled `ID Chip` | PCI-card-local **1-Wire** ID/memory device. Its `DATA` pin uses net `ID_DATA` connected to FPGA `U3`. The user confirms the EEPROM **contains XStream licensing records**; their suspected plaintext format remains unverified. |
| `U9` | **LM1085-ADJ** | Generates the card's 1.8 V supply from its regulator network, alongside 3.3 V power. |
| `OSC1` | FPGA oscillator | Nets include `SPARTAN_CLK`; the exact fitted frequency is not identified from this one-page schematic. |
| `J4` | JTAG header | Spartan configuration/debug access. |
| `J1` | **40-pin Receive Header** (`HDR_40PIN_LECROY`) | Differential `RX_CLOCK_P/N`, `RX_D0..D11_P/N`, `RX_SYNC_P/N`, `RX_RESET_ERR_P/N`, status `TXSTABLE/RXSTABLE`, and grounds. |
| `J2` | **40-pin Transmit Header** (`HDR_40PIN_LECROY`) | Corresponding differential `TX_CLOCK_P/N`, `TX_D0..D11_P/N`, `TX_SYNC_P/N`, `TX_RESET_ERR_P/N`, status lines, and grounds. |

The schematic explicitly shows different receive-termination and
transmit signal resistor networks. It also connects PCI
`INTA#` into the buffered FPGA-side control signals. These
are the **electrical PCI-to-link boundary** for our host driver.
The existence of differential wire pairs does **not**, by itself,
establish their encoding, clock frequency, framing, protocol,
bitstream semantics or a particular standards-compliant link PHY.

**Configuration assembly options shown in the PCI schematic:**

- PROM configuration: fit `R60/R63/R66`, omit `R88/R89`.
- Remote configuration via link: omit `R60/R63/R66`, fit `R88/R89`.
- The schematic separately describes `R73/R76/R78/R80`
  for its JTAG-related configuration path.

These are **schematic assembly instructions**, not proof which
option is physically installed or how a running firmware image
is obtained. They may become relevant to future driver/firmware
startup analysis; do not change bitstream/configuration code based
only on option labels.

## Directly visible acquisition-board overview (Overview.pdf, sheet 1)

The LeCroy top-level drawing separates at least these functional
blocks. The exact child sheets and detailed circuitry are **not**
included in this one-page Overview:

| Top-level block label | Observed connections / separation |
|---|---|
| `Power Conv/Filters (PC)` | Dedicated power/line synchronization section. |
| `UP Control (UP)` | Has `UC_SPI(0:4)`, `I2C(0:5)` and `Voltage_Monitor(0:32)` interfaces, alongside additional ADC/control-related nets. |
| `Timebase (TB)` | Separate block carrying `FE_MTT(0:11)`, `MTT_FPGA(0:35)`, `JTAG_MTT(0:4)`, `ACQ_CNTL(0:25)`, `DELAY_CAL(0:7)` and clock/reference/control connections. |
| `ADC+MAM (AM)` and `ADC+MAM (AM2)` | **Two distinct ADC/acquisition-memory-manager blocks**, connected to their respective front-end and FPGA paths. Do not conflate either with PCI-card Spartan `U3`. |
| `FPGA's (FP)` | An additional dedicated acquisition-board FPGA block between the two ADC+MAM groups and control/data connections. |
| Channel front-end blocks `Channel 1` through `Channel 4`, plus `EXT` | Separate channel circuitry; the overview labels front-end ADC, voltage monitor, I2C and other serial/control nets. |

Useful **net/bus names explicitly printed in the Overview** include
`I2C(0:5)`, `SPI_IO(0:40)`, `Voltage_Monitor(0:32)`,
`FE_CH1_ADC(0:1)` through `FE_CH4_ADC(0:1)`,
`ADC_CNTL(0:25)`, `MTT_FPGA(0:35)`,
`ADC_FPGA1(0:20)` / `FPGA_ADC1(0:20)`,
`ADC_FPGA2(0:20)` / `FPGA_ADC2(0:20)`,
`JTAG_ADC1(0:5)`, `JTAG_ADC2(0:5)`,
and `DELAY_CAL(0:7)`.

**Important new electrical clue:** the overview shows the
`I2C(0:5)` bus across the `UP Control` and channel front-end
region, while `SPI_IO(0:40)` and the UP's `UC_SPI(0:4)`
are **separately named**. This is consistent with the user's
physical probe interface description: initial **analog ADC
identification value** classifies ProBus, the front EEPROM is
then read via **I2C**, and physical probe control also uses
**I2C**. The one-page overview does **not** separately identify
the exact ProBus EEPROM IC, its slave address, an ADC channel
number/threshold for probe class, a front I2C master pinout or
the AP015 mechanical-status I2C register. Do not invent these
from generic `FE_CHx_ADC` or `Voltage_Monitor` bus names.

## Reconciliation with the already recovered x64 driver

This new schematic evidence improves the **hardware-layer attribution**
of our previous source and trace results:

| Host driver / recovered feature | Hardware interpretation with the new drawings | Evidence limit |
|---|---|---|
| `BAR2+0x040 ONEWIRE` and `IOCTL_GET_DALLAS_ID`, `READ/WRITE_DALLAS_MEMORY` | A concrete **PCI-card DS2433** now exists on `U11`/`ID_DATA` wired to PCI FPGA `U3`; it is a physically plausible and strongly matching device behind the original-driver Dallas/1-Wire functions. | The electrical schematic does not label an FPGA-internal `BAR2+0x040` register or contain its RTL, so the complete address-to-ID_DATA mapping is a source-plus-schematic correlation, not a printed schematic net. |
| The FPGA responds to conventional PCI signals and provides buffered `INTA#`. | The physical PCI endpoint is Spartan-IIE `U3` on the **interface card**. The driver-visible BAR register map and IRQ are host-facing abstractions of this PCI endpoint and/or its downstream logic. | The schematics do not alone assign every individual BAR register to PCI-FPGA RTL or downstream acquisition FPGA logic. |
| `CFDC2110` family-0 `0x90`, local `BAR1 SPICTL/SPIDAT/SPIDIN` (source-confirmed) | An already recovered **host/board SPI register path**. Acquisition overview independently names `SPI_IO` and `UC_SPI`, while the external probe/front-panel uses I2C per the user. | Neither PDF establishes a one-to-one connection from host `0x90` or BAR1 SPI to a physical front I2C EEPROM transaction. |
| `CFDC2110` forwarded firmware `0x4A` AP015 metadata | High-level result **after** some board/front-end identification process, consistent with the user's ADC-first / EEPROM-over-I2C sequence. | Exact ADC value, low-level I2C addresses/bytes, EEPROM field mapping and possible caching/firmware translation are unobserved in the IOCTL JSONL. |
| `BAR0 INTST/INTEN 0x08`, `BAR1 HWInt 0x410`, software pending `0x0200` | Recovered host-side asynchronous board event delivery. Its existence and functioning for physical hotplug and jaw events are supported by the actual traces. | It is **not** a physical ADC sample or a raw I2C transaction, and a missing newly captured event does not imply XStream failed to display its existing unlocked-jaw warning. |
| `CFDC2138`/MAM and MTT transfer programming | Acquisition overview physically separates timebase TB, the two `ADC+MAM` regions, FPGA block FP and channel front-ends. | Do not identify card Spartan `U3` as the AM/AM2 FPGA or assert an exact link command format or low-level allocation/ownership without further RTL/sheets. |

**Three separate non-volatile storage categories, not one:**

1. `U6 XC18V02`: **PCI Spartan configuration PROM**.
2. `U11 DS2433`: **PCI-card Dallas 1-Wire ROM ID and 512-byte XStream licensing EEPROM** (memory purpose user-confirmed; plaintext hypothesis unverified).
3. Front-panel/probe-identification **I2C EEPROM**, after ADC
   class detection (**user's hardware information; individual
   component not identified in the one-page board Overview**).

Confusing `U11` with the front-panel I2C EEPROM would make the
recovered Dallas IOCTLs appear to implement AP015 classification,
which the physical device locations and bus types do not support.

## Practical next investigations (no source change implied)

1. **PCI-card-to-acquisition-board link:** inspect schematic child
   sheets or board RTL, if legitimately available, for J1/J2
   link endpoints, framing, programming/configuration and the
   ownership of mapped BAR register effects. The PCI schematic
   has **separate** receive and transmit 40-pin headers with
   12 differential data pairs each; it is not evidence of
   ordinary PCI signaling continuing onto the acquisition board.
2. **Dallas mapping:** correlate the known `BAR2+0x040`
   ONEWIRE access with U3's `ID_DATA` behavior, without
   probing/writing the identification device unnecessarily.
3. **ProBus front-end:** locate the exact ADC class-discriminator,
   physical I2C master and EEPROM/controller on appropriate
   child sheets. Do not assume any `FE_CHx_ADC` line in the
   overview is that discriminator without a net-level proof.
4. **Probe event words:** interpret 0x82 open-correlated 0x0058 /
   0x4A F7 and closed-correlated 0x00A7 / F3 only at the
   measured **host-notification layer** until correlated with
   physical I2C/status data.

The current `3490709` short-reply serializer and `70716ba`
HWInt recovery passed real AP015 detection/hotplug/jaw tests.
The schematics clarify topology, **not** a new regression
requiring PCI/IRQ/MAM/DMA driver changes.

Related existing documents:
[`hardware-register-map.md`](hardware-register-map.md),
[`ioctl-map.md`](ioctl-map.md),
[`probus-detection-i2c-architecture.md`](probus-detection-i2c-architecture.md),
[`probus-calibration-ab-comparison.md`](probus-calibration-ab-comparison.md),
[`next-chat-handoff.md`](next-chat-handoff.md).


## User-verified bus purpose and DS2433 licensing role (2026-09-29)

**Clarification from the user, superseding uncertain earlier language:**

- **All five front-side ProBus probe connectors communicate with the
  probes exclusively through I2C; there is NO probe-side SPI.**
  The analog ADC identification value is the initial
  ProBus-class discriminator, then the front/probe EEPROM is
  identified over I2C, and physical probe control also uses I2C.
  The drawing's `I2C(0:5)` is an independently visible bus
  label; do not equate the signal indices with connector numbers
  without a child sheet.
- The board's separate SPI networks are believed by the user to be
  primarily for **internal ADC, references and related hardware
  configuration**. Their exact chip-selection allocation is not
  mapped by this top-level drawing. The recovered family-0
  opcode 0x90 remains a *local BAR1 SPI helper*, but it
  must not be called a front ProBus SPI protocol.
- The PCI-card **U11 DS2433** (schematic label `ID Chip`) has
  a more specific purpose according to the user: its 512-byte
  **1-Wire EEPROM stores XStream license keys**. Plaintext storage
  is the user's current expectation and is NOT yet confirmed by
  inspecting the actual private image. Its eight-byte 1-Wire
  ROM identity and writable memory contents are distinct.
  The DS2433 is not one of the front I2C EEPROMs and
  not the U6 XC18V02 Spartan PROM.

**Current x64 coverage:** original x86 IOCTL 0x00223080/84/88
provides Dallas ROM/read/write; x64 currently implements 0x80
and 0x84 only, not the write 0x88. A non-destructive
`lecdiag dallas-backup` binary export was added to
`tools/lecdiag/lecdiag.c`: it reads the full 512-byte
memory twice, confirms stable ROM ID, saves to a new
private file and verifies the saved bytes. It has not yet
been WDK/Windows-compiled or tested on the scope.
Write/erase on the installed licensing device is
**not** a first-line regression test; use a spare DS2433
and a validated private restore path if that capability
is later needed. Details:
[`dallas-license-memory-test-plan.md`](dallas-license-memory-test-plan.md).

This information refines the schematic's `ID Chip` label,
whose contents cannot be inferred from the picture alone.

## DMA-related physical-layer findings and provenance (2026-10-06)

Independent manufacturer documentation confirms that the separate PCI board
is a **transaction repeater** between conventional PCI and the acquisition
controller over LVDS, with its digital PCI/LVDS logic in a Xilinx
Spartan-IIE FPGA (WaveRunner 6000 service manual, sections 4.3, 4.6.1.3).
A public WaveRunner 6200 investigation independently reports three
`XC2S200E` FPGAs: one `PQ208` PCI-card device and two `FG456`
acquisition-board devices. These observations corroborate the owner's
schematic, but do not identify the package/speed grade or fitted PCB
revision of the actual working unit.

The PCI board's `U3` terminates the buffered conventional PCI interface
(including requester arbitration lines `REQ#/GNT#`) and terminates both LVDS
headers `J1/J2`. The acquisition controller FPGA has separate MAM and
MTT/timebase producer/control domains. The host's common `SGTA/IIMTC/IIMCL`
register path and source-specific `MAMRGO/MTTRGO` launches are evidence of a
shared **driver-visible** transfer interface, not evidence of the exact
number of RTL engines, internal FIFOs or a common abort operation.

No reviewed schematic or public service/manual excerpt documents the PCI
FPGA's descriptor-fetch state machine, outstanding initiator requests,
posted-write FIFO, remote LVDS work queue, interrupt-to-write-retirement
ordering or a device-specific halt/drain acknowledgement. See
[`legacy-dma-abort-bus-idle-audit.md`](legacy-dma-abort-bus-idle-audit.md)
for the DMA-lifetime consequence: neither completion IRQ nor any inferred
reset/disable sequence proves that host memory can be unmapped or reused.

Additional public documentation:

- [Manufacturer PCI description, section 4.3](https://www.manualslib.com/manual/2455899/Lecroy-Waverunner-6000-Series.html?page=25)
- [Acquisition controller, section 4.6.1.3](https://www.manualslib.com/manual/2455899/Lecroy-Waverunner-6000-Series.html?page=30)
- [LeCroy acquisition main-board drawing index and public schematic source](https://www.ko4bb.com/getsimple/index.php?dir=LeCroy/LeCroy_6000_series_Digital_Storage_Oscilloscope_Service_Manual/Schematics&id=manuals)
- [Independent XC2S200E/link inspection, related WaveRunner 6200](https://www.eevblog.com/forum/repair/lecroy-waverunner-6200-repair/)

Do not publish owner-supplied drawing PDFs, bitstreams or license memory.

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

Those four package pins have PCI-compatible alternate
`IRDY`/`TRDY` pad capabilities, but the *actual* PCI net assignment
on the LeCroy PCB is not established by that naming alone. The row
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
