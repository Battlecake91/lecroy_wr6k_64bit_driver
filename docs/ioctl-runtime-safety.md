# Runtime IOCTL safety classification

This document classifies the IOCTLs dispatched by the current x64 driver for
controlled runtime qualification. It is a safety plan, not authorization to
open the device or issue any request. The classification is based on the
current `driver/Ioctl.c` implementation and the ABI declarations in
`driver/LecS65Drv.h`.

Classes are cumulative by risk:

- **R0** — software-only behavior or documented metadata with no hardware side
  effect;
- **R1** — bounded, established read-only hardware access;
- **R2** — software or hardware state change, interrupt-status consumption, or
  ambiguous hardware semantics;
- **R3** — DMA, acquisition, firmware/EEPROM, or potentially destructive
  behavior.

`Safe closed` means suitable for a separately authorized bounded test while
XStream and all acquisition clients are closed; it does not mean that the
request is authorized now. `Persistent` distinguishes known persistent or
programmable device state from volatile register and driver state.

| Common matrix property | Value |
|---|---|
| Device handle required | **Yes for every IOCTL row below**; no request can be issued by metadata enumeration alone |

## Public and legacy dispatch

| IOCTL | Request / response ABI | MMIO read | MMIO write | IRQ status | DMA | Dallas / FPGA / MCU | Persistent | Safe closed | Class and reason |
|---|---|---:|---:|---|---|---|---|---:|---|
| `0x00222400` | no input / no output | No | No | No | No | No | No | Yes | **R0** no-op compatibility success |
| `0x00222C00` | 4-byte delay in milliseconds / no output | No | No | No | No | No | No | Yes, zero delay only | **R0** bounded software delay; nonzero delays are unnecessary for the first batch |
| `0x00222C04` | 1-byte flag / no output | No | No | No | No | No | No | No | **R2** changes driver software state |
| `0x00223000` | exactly `0x108` bytes / no output | No | No | No | No | No | No | Yes | **R0** trace-control compatibility input is validated and logged; the current x64 path has no board side effect |
| `0x00223004` | no input / 4-byte size or `0x1A08`-byte trace payload | No | No | No | No | No | No | Yes, size query | **R0** software trace snapshot |
| `0x0022303C` | exactly `0x10A`-byte setter record / no output | No | Yes | Possible by selected register | Can arm acquisition/DMA-related registers | No | Not established; volatile device state | No | **R3** indexed write to the known 43-register table |
| `0x00223040` | no input / 4-byte size; or 4-byte index / `0x10A`-byte record; or full `43 * 0x10A` list | Yes for hardware-backed records | No | May observe status | No | No | No | Yes, size query and reviewed reads | **R0** for the size query; **R1** for established register reads |
| `0x00223044` | no input / 4-byte value | Yes, BAR0 START/FVER | No | No | No | No | No | Yes | **R1** established bounded START-register read |
| `0x00223080` | no input / 8-byte identifier | Yes | Yes, one-wire protocol commands | No | No | Dallas | No write to EEPROM, but board identity is sensitive | No | **R2** read transaction requires one-wire MMIO sequencing |
| `0x00223084` | no required input / 1..512 output bytes | Yes | Yes, one-wire protocol commands | No | No | Dallas | No write to EEPROM, but licensed contents are sensitive | No | **R2** read transaction has MMIO side effects and returns protected contents |
| `0x00223088` | legacy Dallas-write ABI; not dispatched by x64 | N/A | N/A | No | No | Dallas write | Yes | No | **R3** intentionally absent; unknown requests are rejected |
| `0x00223100` | three 32-bit event handles / no output | No | No | No | No | No | No | No | **R2** replaces referenced kernel-event state and lifetime |
| `0xCFDC2110` | packed command stream / command-dependent output | Yes | Yes | Can consume or acknowledge status | Can initiate transfer paths | JTAG, Dallas and board transport subcommands exist | Command-dependent | No | **R3** multiplexed legacy command family with hardware writes and DMA-capable operations |
| `0xCFDC2114` | user-mode observed; not dispatched by current x64 or captured x86 driver | No in current rejection path | No in current rejection path | No | No | Semantics unconfirmed | No in current rejection path | Expected-negative only | **R0** for the current deterministic unknown-IOCTL rejection; never infer or implement semantics from the number |
| `0xCFDC2124` | 12-byte legacy pointer/length/reserved record / 4-byte token | No direct MMIO | No direct MMIO | No | Pins/maps user pages and publishes transfer state | No | No | No | **R3** DMA registration and lifetime operation |
| `0xCFDC2128` | 4-byte token / no output | No direct MMIO | No direct MMIO | No | Retires registered transfer state | No | No | No | **R3** DMA lifetime operation; physical retirement is not proved |
| `0xCFDC212C` | no supported payload / rejection | No | No | No | No | No | No | Yes, expected-negative only | **R0** deterministic `STATUS_NOT_IMPLEMENTED` path |
| `0xCFDC2130` | legacy serial-trigger FPGA writer; not dispatched by x64 | N/A | N/A | No | No | FPGA/GPIO programming | Yes or potentially persistent | No | **R3** deliberately absent |
| `0xCFDC2138` | buffered acquisition ABI | Yes | Yes | Yes | Yes | No | No | No | **R3** acquisition and DMA path |
| `0xCFDC2180` | 4-byte event handle / no output | No | No | No | No | No | No | No | **R2** changes referenced kernel-event state |
| `0xCFDC2184` | no input / no output | No | No | No | No | No | No | Yes | **R0** no-op compatibility success |
| `0xCFDC218C` | 4-byte event handle / no output | No | No | No | No | No | No | No | **R2** changes referenced kernel-event state |
| `0xCFDC2190` | exactly 29 bytes / no output | Yes | Yes, ERRM and INTEN bit 1 | Changes interrupt enable state | No | No | No | No | **R2** interrupt/error-mode state change |
| `0xCFDC2194` | no input / exactly 29 bytes | No direct MMIO | No | Consumes the software error latch | No | No | No | No | **R2** read-and-clear software status semantics |
| `0xCFDC21C0` | 4-byte legacy offset or packed 5-byte BAR/offset / 4-byte value | Yes | No | May read status registers | No | Could reach mapped board registers | No | No, except the reviewed START helper | **R2** generic bounded register access has register-dependent semantics |
| `0xCFDC21C4` | 8-byte legacy offset/value or packed 9-byte BAR/offset/value / no output | No | Yes | Can alter INTEN/status | Can reach acquisition/DMA control registers | Could reach mapped board registers | Not established; volatile device state | No | **R3** generic register write |
| `0xCFDC21C8` | no input / 4-byte build number | No | No | No | No | No | No | Yes | **R0** returns legacy build `1002` |
| `0xCFDC21CC` | user-mode observed; not dispatched by current x64 or captured x86 driver | No in current rejection path | No in current rejection path | No | No | Semantics unconfirmed | No in current rejection path | Expected-negative only | **R0** for the current deterministic unknown-IOCTL rejection; never infer or implement semantics from the number |
| `0xCFDC2400` | 4-byte pending mask / no output | No direct new read | DPC-dependent | Injects/consumes software pending status and may acknowledge hardware through DPC processing | No new DMA launch, but enters interrupt completion paths | No | No | No | **R2** synthetic interrupt/DPC state manipulation |
| `0xCFDD219F` | METHOD_NEITHER acquisition ABI / rejection in the current build | No in the rejection path | No in the rejection path | No in the rejection path | Intended acquisition/DMA operation | No | No | No | **R3** hazardous intended role; current build fails closed with `STATUS_NOT_SUPPORTED` |

## Private diagnostic dispatch

| IOCTL | Request / response ABI | MMIO read | MMIO write | IRQ status | DMA | Dallas / FPGA / MCU | Persistent | Safe closed | Class and reason |
|---|---|---:|---:|---|---|---|---|---:|---|
| `0x80006000` | no input / `LECS65_DEBUG_STATS` | No | No | No | No | No | No | Yes | **R0** software counters and build metadata |
| `0x8000A004` | no input / no output | No | No | No | No | No | No | No | **R2** clears software counters |
| `0x80006008` | no input / `LECS65_DEBUG_BARS` | No | No | No | No | No | No | Yes | **R0** cached resource metadata |
| `0x8000600C` | no input / `LECS65_DEBUG_TRACE` | No | No | No | No | No | No | Yes | **R0** software trace snapshot |
| `0x8000A010` | no input / no output | No | No | No | No | No | No | No | **R2** clears software trace state |
| `0x80006014` | no input / `LECS65_DEBUG_PCI_CONFIG` | Read-only bus-interface access | No | No | No | No | No | Yes | **R1** bounded PCI configuration read |

## Proposed first R0/R1 runtime batch

This procedure is prepared but was **not executed** during Kernel Runtime
Qualification Batch 1. It requires explicit authorization to open the device
and issue the listed IOCTLs. XStream and all acquisition clients must remain
closed. Use the already built x64 `lecdiag.exe` whose hash is recorded before
the run. Each command gets a fresh non-existing log and a five-second timeout
through the repository's job-object native process runner; timeout terminates
the assigned process tree.

```powershell
Set-Location -LiteralPath '<reviewed-repository-root>'
. .\scripts\native-process.ps1

$lecdiag = (Resolve-Path -LiteralPath '.\tools\lecdiag\build\lecdiag.exe').Path
$evidence = '<new-non-existing-evidence-directory>'
New-Item -ItemType Directory -Path $evidence -ErrorAction Stop | Out-Null

$tests = @(
    @{ Name = 'build';       Args = @('build') },
    @{ Name = 'stats';       Args = @('stats') },
    @{ Name = 'bars';        Args = @('bars') },
    @{ Name = 'trace';       Args = @('trace') },
    @{ Name = 'noop-22400';  Args = @('raw-ioctl','0x00222400','','0') },
    @{ Name = 'noop-2184';   Args = @('raw-ioctl','0xCFDC2184','','0') },
    @{ Name = 'size-23004';  Args = @('raw-ioctl','0x00223004','','4') },
    @{ Name = 'size-23040';  Args = @('raw-ioctl','0x00223040','','4') },
    @{ Name = 'delay-zero';  Args = @('raw-ioctl','0x00222C00','00000000','0') }
)

foreach ($test in $tests) {
    Invoke-LecS65NativeProcess -Name $test.Name -FilePath $lecdiag `
        -Arguments $test.Args -WorkingDirectory (Get-Location).Path `
        -LogPath (Join-Path $evidence ($test.Name + '.log')) `
        -TimeoutSeconds 5
}
```

Expected R0 invariants are build `1002`; debug structure version `1`; BAR
lengths `0x200`, `0x40000`, `0x200`; successful no-op and zero-delay calls;
query sizes `0x1A08` and `0x2CAE`; and no device problem, interface-count
change, new kernel error, or acquisition activity. The deterministic
`0xCFDC212C` rejection is deferred until the harness records an expected
nonzero application exit without treating it as a retry.

After R0 passes and a separate R1 authorization is recorded, use the same
runner and fresh evidence directory for exactly these hardware-read commands:

```powershell
$r1Tests = @(
    @{ Name = 'pci';            Args = @('pci') },
    @{ Name = 'start-register'; Args = @('start-register') }
)

foreach ($test in $r1Tests) {
    Invoke-LecS65NativeProcess -Name $test.Name -FilePath $lecdiag `
        -Arguments $test.Args -WorkingDirectory (Get-Location).Path `
        -LogPath (Join-Path $evidence ($test.Name + '.log')) `
        -TimeoutSeconds 5
}
```

The PCI result must report vendor/device `1570:0005` at the previously recorded
bus/device/function. `start-register` performs the dedicated `0x00223044` read
and the generic BAR0 offset-zero read and requires them to be equal; the value
itself is recorded as observed rather than compared with historical firmware
data.

Stop immediately on timeout, access failure, unexpected output length/value,
device problem, service-state change, missing or duplicate interface, new
driver-related kernel event, capture of an unapproved IOCTL, or any sign of
acquisition, DMA, interrupt storm, or hardware-state change. Do not retry and
do not perform automatic rollback.

## Kernel Runtime Qualification Batch 2 result

Batch 2 used x64 `lecdiag.exe` SHA-256
`824F089EBED1D4C5EC0337CBE18F05489D2B062476AD5C1723333869B299A8DD`
and a five-second job-object timeout for each authorized command. Passive
preflight confirmed `oem101.inf`, version `0.2026.1018.1`, the approved loaded
SYS, device `OK`, problem code zero, a running service, five single enabled
interfaces, no acquisition client, and hash-identical rollback artifacts.

The first four R0 commands ran once and passed:

| Command | Request / response | Exit | Observed result |
|---|---|---:|---|
| `lecdiag build` | input 0; output capacity 4; returned 4 | 0 | build `1002` |
| `lecdiag stats` | input 0; output capacity/ABI result 48; the utility does not print `bytesReturned` | 0 | structure version `1`, build `1002`, unknown IOCTL count `0` |
| `lecdiag bars` | input 0; output capacity 72; returned 72 | 0 | lengths `0x200 / 0x40000 / 0x200`; bulk `0x40000` |
| `lecdiag trace` | input 0; output capacity 118,800; returned 118,800 | 0 | trace version `3`, one preexisting build-query record |

The fifth authorized command was not started. PowerShell rejected the explicit
empty raw-input argument while binding `Invoke-LecS65NativeProcess` because its
`string[] Arguments` parameter allowed an empty collection but not an empty
string element. No fifth IOCTL reached the driver. Tests 6–9 were not attempted,
and the conditional R1 gate did not become effective. No retry was made.

The bounded-process helper now declares both `AllowEmptyCollection` and
`AllowEmptyString`. A focused mock proves byte-for-byte positional preservation
of `before`, an empty argument and `after` under Windows PowerShell 5.1 and
PowerShell 7; both compatibility runs pass 26/26. This is an offline workflow
fix only. It does not renew the consumed runtime authorization.

The post-stop state is identical to preflight and the exact test interval has
no Kernel-PnP, Code Integrity or System event. Remaining R0 commands 5–9, and
then the two conditional R1 commands, require a new explicit authorization.
The private evidence-manifest SHA-256 is
`78D3C40B85F1885A0B9AC20EA1859A65BC5E5599675C775E5DB763E40259652A`;
the R0 result-record SHA-256 is
`B14326835689C647D57A698509A7EA9AC0654E6ED6EB9BAF409D57FC42A28F90`.
