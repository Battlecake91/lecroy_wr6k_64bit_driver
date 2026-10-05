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
