# Hardware register map

This map is reconstructed from the register-wrapper initialization code in the legacy x86 driver.

The BAR labels and offsets below are **confirmed** from the binary. The driver stores both the textual register name and the offset in each wrapper object and computes the runtime address from the assigned BAR base.

## BAR0

| Register | Offset |
|---|---:|
| `FVER` | `0x000` |
| `ERRS` | `0x004` |
| `ERRM` | `0x008` |
| `START` | `0x00C` |
| `SGTA` | `0x040` |
| `IIMTC` | `0x044` |
| `IIMCL` | `0x048` |
| `IIMST` | `0x04C` |
| `INTST` | `0x080` |
| `INTEN` | `0x084` |

## BAR1

| Register | Offset |
|---|---:|
| `ITMODE` | `0x000` |
| `CLRERR` | `0x004` |
| `CLRIRQ` | `0x008` |
| `ACQFVER` | `0x00C` |
| `JTAGNUM` | `0x020` |
| `JTAGDAT` | `0x024` |
| `JTAGDIN` | `0x028` |
| `MAMDAT` | `0x040` |
| `MAMPGO` | `0x044` |
| `MAMSEQ` | `0x060` |
| `MAMRGO` | `0x064` |
| `MTTCTL` | `0x080` |
| `MTTRGO` | `0x084` |
| `MTTNUM` | `0x090` |
| `SPICTL` | `0x0A0` |
| `SPIDAT` | `0x0A4` |
| `SPIDIN` | `0x0A8` |
| `GPIODIR` | `0x0C0` |
| `GPIODAT` | `0x0C4` |
| `LEDCTL` | `0x0E0` |
| `RMIDIV` | `0x0E4` |
| `RMICUM` | `0x0E8` |
| `ACQDIV` | `0x0EC` |
| `ACQCUM` | `0x0F0` |
| `PFREG` | `0x0F4` |
| `SetIRQ` | `0x100` |
| `TxControl` | `0x400` |
| `RxControl` | `0x404` |
| `TxCount` | `0x408` |
| `RxCount` | `0x40C` |
| `HWInt` | `0x410` |

## BAR2

| Register | Offset |
|---|---:|
| `BUZZER` | `0x000` |
| `ONEWIRE` | `0x040` |

## Access width / wrapper metadata

The legacy code creates register wrapper objects containing:

- runtime register address;
- pointer to the register name string;
- pointer to the BAR label string;
- an access-width field;
- an internal type field;
- the register offset.

Most recovered wrappers use a four-byte access width and are eventually accessed through `READ_REGISTER_ULONG` / `WRITE_REGISTER_ULONG`.

The internal wrapper type values (for example 4, 5, 6, 7) are DriverWorks/application abstractions and should not be copied blindly into the x64 implementation.

## Special handling observed

`BAR0 + 0x84` (`INTEN`) is special-cased in the generic register-write IOCTL. The driver keeps a cached copy of the value before writing the register.

That is evidence that interrupt-enable state is referenced elsewhere in the ISR/DPC path and should be preserved in the replacement driver.

## Proven raw-register IOCTLs

The driver exposes generic raw register access:

- `0xCFDC21C0`: register read;
- `0xCFDC21C4`: register write.

Both support legacy BAR0-only requests and extended packed requests carrying a BAR selector. See [ioctl-map.md](ioctl-map.md).

This is useful for bring-up because an x64 replacement can be validated register-by-register before higher-level acquisition paths are implemented.


## Superseded early BAR-size hypothesis

An early bring-up build tried to classify the translated resources by size and
treated the 0x40000-byte window as a separate bulk region. That hypothesis is
superseded by the original driver's resource-constructor disassembly and by the
working Dallas/1-Wire implementation.

The legacy mapping is by memory-resource order: the 0x40000-byte second memory
resource is BAR1, while the first and third 0x200-byte resources are BAR0 and
BAR2 respectively. The corrected mapping is described below.


## Recovered Dallas / 1-Wire hardware path

The 2008 reference driver establishes the three memory resources in resource-index
order and passes them to its register-map constructor as BAR0, BAR1 and BAR2.

On the current reference system the translated resources are therefore:

- BAR0: first memory resource, 0x200 bytes
- BAR1: second memory resource, 0x40000 bytes
- BAR2: third memory resource, 0x200 bytes

This corrects the earlier conservative size-based prototype classification.

The Dallas/1-Wire controller register is confirmed at:

```text
BAR2 + 0x40  ONEWIRE
```

The original low-level controller protocol is:

- write `0`: 1-Wire reset
- write `1`: transmit a zero bit
- write `2`: transmit a one bit
- write `3`: read a bit
- read bit 0: controller busy
- read bit 1: sampled 1-Wire data/presence state

The original driver polls bit 0 until clear. After a reset, bit 1 clear indicates
a detected presence pulse.

Recovered Dallas transactions:

```text
GET_DALLAS_ID:
  reset
  write 0x33 (READ ROM), LSB first
  read 8 bytes, LSB first
  validate Dallas/Maxim CRC-8
  retry up to 10 times on CRC failure

READ_DALLAS_MEMORY:
  reset
  write 0xCC (SKIP ROM)
  write 0xF0 (READ MEMORY)
  write 0x00
  write 0x00
  read requested 1..0x200 bytes
```

Reference hardware results captured through the original x86 driver:

```text
Dallas ROM ID:
23 F0 47 37 00 00 00 AC

Full memory read:
512 bytes
```

The ROM CRC byte `0xAC` matches the Dallas/Maxim CRC-8 of the preceding seven
bytes.


## BAR1 message-transport windows

Further disassembly of the legacy `0xCFDC2110` transfer path identifies a
message transport implemented entirely in BAR1.

Confirmed control registers:

```text
BAR1 + 0x100  SetIRQ
BAR1 + 0x400  TxControl
BAR1 + 0x404  RxControl
BAR1 + 0x408  TxCount
BAR1 + 0x40C  RxCount
BAR1 + 0x410  HWInt
```

The data windows used by the original driver are:

```text
BAR1 + 0x420 + 4*n   transmit word slots
BAR1 + 0x600 + 4*n   receive word slots
```

Each logical protocol word is 16 bits, but the hardware slots are spaced on
32-bit boundaries. The legacy driver writes and reads the slots through
`WRITE_REGISTER_BUFFER_ULONG` and `READ_REGISTER_BUFFER_ULONG`, then consumes
the low 16 bits of each slot.

The original transmit helper:

1. waits until `TxControl` reports idle;
2. splits the outgoing byte stream into 16-bit words;
3. writes up to `0x78` words into the transmit slots;
4. writes the remaining/total word count through `TxCount`;
5. starts the transfer by writing `TxControl` with bit 15 set and the chunk
   word count in the low byte;
6. repeats for continuation chunks when required.

The receive helper:

1. reads `RxControl`;
2. treats bit 15 as data-ready;
3. takes the low byte as the number of available 16-bit words;
4. reads those words from the receive slots;
5. clears the ready/count fields in `RxControl`;
6. repeats when the continuation state is set.

This transport is the hardware endpoint behind the A5FB/85FB protocol used by
`0xCFDC2110`.


## Buzzer behavior and proposed x64-driver modes

The legacy driver controls the on-board piezo through:

```text
BAR2 + 0x000  BUZZER
```

The recovered helper only writes logical on/off values:

```text
WRITE_REGISTER_ULONG(BUZZER, 1)
delay
WRITE_REGISTER_ULONG(BUZZER, 0)
```

The original initialization path uses this helper for the audible startup
beeps. There is currently no static evidence in the legacy driver for a
dedicated buzzer-frequency or divider register.

This means three replacement-driver startup modes are feasible without
inventing unknown hardware semantics:

- `Standard`: reproduce the original startup beep pattern.
- `Off`: suppress startup buzzer writes.
- `Melody`: use a distinctive custom-driver pattern.

For `Melody`, two implementation levels must be distinguished:

1. **Rhythm-only melody (confirmed feasible):** use the known BUZZER gate with
   different on/off durations. Pitch remains whatever the hardware generates.
2. **Variable-pitch melody (not yet proven):** only possible if the piezo is
   passive and the BUZZER register can be toggled fast enough in software, or
   if another FPGA/divider register controls its oscillator. The legacy driver
   provides no evidence for such a frequency control.

Therefore the safe first implementation should treat `Melody` as a unique
rhythmic signature using the existing on/off gate. Frequency control should be
added only after the electrical/FPGA behavior has been verified.
