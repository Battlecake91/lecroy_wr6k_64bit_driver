# IOCTL map

This table is reconstructed from the comparison tree inside `CLecS65AcqDrvDevice::DeviceControl()`.

All values below are **confirmed as dispatch values** in the analysed binary.

## Recovered dispatch table

| IOCTL | Device type | Function | Method | Handler RVA/VA | Recovered name / status |
|---:|---:|---:|---|---:|---|
| `0x00222C00` | `0x22` | `0xB00` | BUFFERED | `0x1286E` | unknown |
| `0x00222C04` | `0x22` | `0xB01` | BUFFERED | `0x128BC` | unknown |
| `0x00223000` | `0x22` | `0xC00` | BUFFERED | `0x12ADA` | unknown |
| `0x00223004` | `0x22` | `0xC01` | BUFFERED | `0x12A5E` | unknown |
| `0x0022303C` | `0x22` | `0xC0F` | BUFFERED | `0x12CAC` | unknown |
| `0x00223040` | `0x22` | `0xC10` | BUFFERED | `0x12C18` | unknown |
| `0x00223044` | `0x22` | `0xC11` | BUFFERED | `0x12D24` | unknown |
| `0x00223080` | `0x22` | `0xC20` | BUFFERED | `0x130EA` | **IOCTL_GET_DALLAS_ID** |
| `0x00223084` | `0x22` | `0xC21` | BUFFERED | `0x131B5` | **IOCTL_READ_DALLAS_MEMORY** |
| `0x00223088` | `0x22` | `0xC22` | BUFFERED | `0x11F54` | **IOCTL_WRITE_DALLAS_MEMORY** |
| `0x00223100` | `0x22` | `0xC40` | BUFFERED | `0x12988` | unknown |
| `0xCFDC2110` | `0xCFDC` | `0x844` | BUFFERED | `0x13AE2` | unknown |
| `0xCFDC2124` | `0xCFDC` | `0x849` | BUFFERED | `0x11BDC` | unknown |
| `0xCFDC2128` | `0xCFDC` | `0x84A` | BUFFERED | `0x11C36` | unknown |
| `0xCFDC212C` | `0xCFDC` | `0x84B` | BUFFERED | `0x11C5E` | unknown |
| `0xCFDC2130` | `0xCFDC` | `0x84C` | BUFFERED | `0x11CFF` | **IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA** |
| `0xCFDC2138` | `0xCFDC` | `0x84E` | BUFFERED | `0x141DC` | unknown |
| `0xCFDC2180` | `0xCFDC` | `0x860` | BUFFERED | `0x128F8` | unknown |
| `0xCFDC2184` | `0xCFDC` | `0x861` | BUFFERED | inline | completes success with zero information |
| `0xCFDC218C` | `0xCFDC` | `0x863` | BUFFERED | `0x12B34` | unknown |
| `0xCFDC2190` | `0xCFDC` | `0x864` | BUFFERED | `0x13A40` | unknown |
| `0xCFDC2194` | `0xCFDC` | `0x865` | BUFFERED | `0x12BAE` | unknown |
| `0xCFDC21C0` | `0xCFDC` | `0x870` | BUFFERED | `0x13954` | unknown |
| `0xCFDC21C4` | `0xCFDC` | `0x871` | BUFFERED | `0x1272A` | unknown |
| `0xCFDC21C8` | `0xCFDC` | `0x872` | BUFFERED | `0x12832` | unknown |
| `0xCFDC2400` | `0xCFDC` | `0x900` | BUFFERED | `0x13A2E` | unknown |
| `0xCFDD219F` | `0xCFDD` | `0x867` | NEITHER | `0x141F8` | unknown; high-priority x64 compatibility risk |

The access bits decode to `FILE_ANY_ACCESS` for all entries in this table.

## Known handler details

### 0x00223080: IOCTL_GET_DALLAS_ID

Confirmed behaviour:

- requires a non-null system buffer;
- checks output length for exactly 8 bytes;
- returns up to 8 bytes;
- uses the internal Dallas/1-Wire implementation.

### 0x00223084: IOCTL_READ_DALLAS_MEMORY

Confirmed behaviour:

- requires a non-null system buffer;
- uses the IOCTL output length;
- accepts lengths `1..0x200`;
- returns the requested byte count on success.

### 0x00223088: IOCTL_WRITE_DALLAS_MEMORY

Confirmed behaviour:

- requires a non-null system buffer;
- uses the IOCTL input length;
- accepts lengths `1..0x200`;
- writes in chunks of at most `0x20` bytes;
- allocates a read-back buffer;
- reads the data back and verifies it using `RtlCompareMemory`;
- contains a retry path.

### 0xCFDC2130: IOCTL_ALADDINDRV_PROG_SERTRIG_FPGA

Confirmed from an embedded original diagnostic string.

The handler validates a system buffer and buffer size, then performs direct register accesses. Exact request structure is still under analysis.

## Important caveat

The labels beginning with `IOCTL_ALADDINDRV_...` indicate that part of the interface may derive from an Aladdin/DriverWorks helper layer rather than being LeCroy-specific application ABI. The numeric values are nonetheless real dispatch values in this driver and must be preserved until callers are identified.

## Next analysis targets

1. Identify each unknown handler by behaviour and buffer contract.
2. Recover device-interface GUID(s) passed to `IoRegisterDeviceInterface`.
3. Recover actual device / DOS-link names.
4. Recover PCI hardware IDs from the corresponding INF or installed system if available.
5. Map BAR resource assignment in `IRP_MN_START_DEVICE`.
6. Recover register offsets for the embedded register-name table.
7. Identify all pointer-sized structures and the `METHOD_NEITHER` request before attempting an x64 ABI implementation.
