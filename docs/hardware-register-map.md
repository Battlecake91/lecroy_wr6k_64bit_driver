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
