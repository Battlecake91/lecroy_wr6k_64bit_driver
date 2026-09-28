# ProBus probe detection, identification and I2C control

**Updated:** 2026-09-29. **Evidence type:** user-provided hardware
architecture, with separately identified original-driver and runtime-trace
observations. This page describes the *probe/front-panel electrical
interface*, not the Windows IOCTL transport implementation.

## Hardware sequence clarified by the user

The user has clarified the actual LeCroy ProBus probe handling order:

1. **Analog probe-class detection.** An ADC reads a probe-associated
   electrical identification value. That value allows XStream to
   recognize that a connected device is a **ProBus-type probe**.
2. **Probe identity from the front-panel EEPROM.** After ProBus
   classification, the EEPROM at the front is read over **I2C**.
3. **Probe control over I2C.** Actual control of the connected probe
   also takes place over **I2C**, including the physical probe
   interface rather than a directly exposed Windows-driver I2C API.

This sequence is based on the user's direct hardware information.
The exact ADC channel, identification values/thresholds, I2C
controller/master, physical SDA/SCL routing, EEPROM address and
contents, and the boundary between XStream's high-level decisions
and board/firmware execution have **not** yet been recovered from
the available IOCTL captures. In particular, do not assume XStream
bit-bangs I2C directly just because it initiates the operation.

## Distinguish all three observable protocol layers

| Layer | Present knowledge | What the existing evidence does *not* prove |
|---|---|---|
| Front-end physical detection | ADC identification value first determines ProBus class (user-provided information). | Exact raw ADC sample, comparator/threshold or numeric probe-type mapping. |
| Physical probe identification and control | Front-panel EEPROM and the probe itself are accessed/controlled via I2C (user-provided information). | Specific I2C slave address, command bytes, transaction acknowledgements or whether the probe's lock state is reported via I2C, a front-panel GPIO or a firmware aggregate. |
| XStream/Windows driver/board transport | CFDC2110 `A5FB/85FB` packets, family-1/0x82 and family-0/1 0x4A, plus BAR1 HWInt notifications produce the higher-level records observed in JSONL. | A one-to-one mapping of any host packet to a physical I2C START/address/data/STOP sequence or an EEPROM field. |

**Important SPI/I2C distinction:** Static reverse engineering maps
CFDC2110 family-0 opcode `0x90` to the **local BAR1 SPI helper**
using `SPICTL` (`BAR1+0xA0`), `SPIDAT` (`+0xA4`),
`SPIDIN` (`+0xA8`). Its selector-0x0E captures include a
144-bit serialized packet. These are *verified host-to-board register
operations*, **not evidence that the electrical bus to the probe is
SPI rather than I2C**. It is presently unknown whether/how this
local SPI route is related to the front-panel I2C controller.
Do not call the probe's physical control bus SPI based on this
opcode and do not silently identify the SPI selector packet with
an EEPROM I2C transfer.

The forwarded `0x4A` family-1 metadata result contains ASCII
`AP015` (Information=270; JSONL records only the first 128
returned bytes). The user's hardware description makes an
EEPROM-derived identification an important candidate to trace,
but **the driver/IOCTL evidence alone does not prove which 0x4A
result bytes originated from EEPROM, or reveal the physical I2C
transactions**. Family-1 `0x82` likewise reports board/
firmware-produced probe status words; their original bit meanings
remain unknown. Authentic `0x0200` command-pending events come
via BAR0 INTST `0x08` / BAR1 HWInt `0x410`; `0x0200` is the
host notification path, not necessarily the probe's physical
sensor or I2C transfer itself.

## Additional PCB evidence from supplied PCI and acquisition-board schematics

Two user-supplied one-page PDFs were inspected as vector/rendered drawings
(`PCI Card.pdf`, `Overview.pdf`). The latter is a
LeCroy acquisition-board **top-level** diagram, *not* the detailed
front-EEPROM circuit. It does explicitly show a named
`I2C(0:5)` bus in the `UP Control (UP)` /
channel-front-end region, while separately naming
`SPI_IO(0:40)` and `UC_SPI(0:4)`. This aligns
with the user's ADC-first, I2C-second probe interface
information without proving a particular EEPROM address
or exact SDA/SCL mapping.

**The PCI interface card is a distinct PCB.** Its
`U3 XC2S200E` Spartan-IIE FPGA connects to
the conventional PCI-side signal groups and two distinct
40-pin receive/transmit link headers, `J1/J2`.
Critically, it also contains a **`U11 DS2433` 1-Wire
ID chip** wired to the FPGA via net `ID_DATA`, and
a separate **`U6 XC18V02` FPGA configuration PROM**.

Thus we now have **three functionally different memories**:

1. PCI-card `U11 DS2433`: **Dallas 1-Wire ID/memory**,
   strongly consistent with original-driver `BAR2+0x040
   ONEWIRE` and the GET/READ/WRITE Dallas IOCTLs.
   The FPGA-internal BAR-to-`ID_DATA` RTL is not
   shown in the schematic and must not be invented.
2. PCI-card `U6 XC18V02`: **Spartan configuration
   PROM**, not probe EEPROM and not Dallas ID storage.
3. Front-panel **I2C EEPROM** in the probe-identification
   chain, reached **after** the analog ADC indicates
   ProBus class (user-provided hardware information).
   This individual EEPROM, its address and physical
   connection are not drawn/labeled in the top-level
   board Overview.

The recovered Windows-driver opcode `0x90` local
BAR1 `SPICTL/SPIDAT/SPIDIN` path and the separate
acquisition-board `SPI_IO` names **must not be
mistaken for** the physical AP015 I2C bus. No
one-to-one host-link-command-to-I2C transaction
mapping is supplied by either PDF. The PCI card's
RX/TX headers also demonstrate a distinct hardware
link layer between PC/PCI and the acquisition hardware;
the exact packet framing is not decoded from this schematic.

Full derived schematic/driver reconciliation:
[`pci-card-acquisition-board-topology.md`](pci-card-acquisition-board-topology.md).
The private original PDFs were not copied into the public repository.

## Relevance to recent AP015 tests

- `xstream_trace_20260929_001152.jsonl`: after the first physical
  reconnection, a transient `0x028C` then `0x0058` status
  occurred, with **no subsequent recorded 270-byte AP015
  reidentification**; XStream reportedly showed a different
  "1/2 clamp" type temporarily. The following three reinsertions
  did complete normal AP015 metadata. This does not establish an
  EEPROM failure. ADC classification, contact settling, front
  I2C/EEPROM communication and higher-level firmware state are
  **separate, currently unverified hypotheses**. It is not known
  which stage actually caused the transient.
- `xstream_trace_20260929_002051.jsonl`: XStream visibly warned
  when the jaw was opened/unlocked, and the normal status path
  captured `0x82 = 0x0058` followed by 0x4A status `F7`;
  a closed-jaw-correlated event captured `0x82 = 0x00A7`
  followed by status `F3`. The same pairing occurred in
  pre-formatter trace `231656`. Do not yet label the
  `F7 ^ F3 = 0x04` difference as a proven EEPROM, I2C
  or vendor-specific lock bit.
- The corrected raw 85FB reply serializer in driver commit
  `3490709` reproduces legacy received-data lengths and 0xFF
  padding and has passed physical probe hotplug tests. The
  additional hardware architecture information by itself is
  **not a reason to modify working INTEN, ISR, PCI or DMA code**.

## Next targeted reverse-engineering questions

1. Recover the ADC identification *source* and the value/class
   decision path that selects ProBus probing, ideally from
   board firmware/front-end schematics or passive measurement.
2. Identify the front-panel I2C master, EEPROM address and raw
   read/write transaction shapes, including which device reports
   the jaw lock/unlock state. Record observations; do not invent
   addresses or EEPROM fields.
3. Correlate an actual EEPROM read and later I2C control
   transactions with the upstream `0x82`, `0x4A`,
   `47 00` and `47 12` host packets in original x86 and
   replacement x64 captures. Treat BAR1 SPI opcode `0x90`
   as a separate, already identified host-level helper until
   a bridge is demonstrated.
4. Investigate incomplete/incorrect first identification at
   the analog-classification vs EEPROM-read vs firmware-state
   boundary, using a focused reproduction only if the user
   encounters a persistent functional issue.

The user's existing real-hardware tests already establish that
the opened-jaw XStream warning, AP015 reidentification, ordinary
physical hotplug, and acquisition work under the corrected x64
driver. This page records the additional hardware context without
overstating what the Windows-driver trace can observe.
