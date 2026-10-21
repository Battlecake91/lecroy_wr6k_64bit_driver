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

## Kernel Runtime Qualification Batch 2B result

Batch 2B authorized commands 5–9 and conditionally the two R1 reads. Preflight
again proved `oem101.inf`, version `0.2026.1018.1`, the approved loaded SYS,
problem code zero, a running service, five single enabled interfaces, closed
acquisition clients and the hash-identical `oem100.inf` rollback export. The
corrected runner passed its Windows PowerShell 5.1 compatibility suite 26/26.

Command 5 ran once with the reviewed argument vector. `lecdiag` opened the
approved interface and then exited 2 with `invalid input hex`; source inspection
proved that `parse_hex_bytes` rejected `digits == 0`. It did not call
`DeviceIoControl`, so no command-5 IOCTL reached the driver. The stop rule
therefore prohibited commands 6–9 and both R1 reads. No retry occurred.

The exact UTC interval from `2026-10-20T16:52:32.1040864Z` through
`2026-10-20T16:53:01.3960437Z` contains no System, Kernel-PnP or Code Integrity
event. INF, version, problem code, service, loaded SYS and all five interface
counts are identical before and after the stopped attempt.

The offline utility correction initializes an empty result and accepts an
explicit zero-length string without allocation. The rebuilt x64 utility has
SHA-256
`ADD4A869C05DD85A55DD9308927D2AE946907F26C0D0054E46A54738441FB7BA`;
source contracts pass 59/59. This hash has never been used for a live request.
The next runtime authorization must name it explicitly. Private evidence is
retained outside the public repository; final evidence-manifest SHA-256 is
`3472CE4AC5854DA4861A9E5139DCC61F802C1D4CEBCEA10BF69004C613EC5DC8`.

## Kernel Runtime Qualification Batch 2C stopped preflight

The exact production `lecdiag.c` was compiled behind a fake Win32 device
boundary and invoked through the production Windows PowerShell 5.1 process
runner. All nine R0 and both R1 command shapes reached the expected mocked
IOCTL with exact input/output lengths. The matrix also proved preservation of
an explicit empty argument, four zero input bytes for `00000000`, the two
four-byte outputs, R1 PCI structure size 84, both START-register calls and
failure-before-`DeviceIoControl` for five malformed cases. Result: 16/16 PASS.

The following live preflight did not meet the approved baseline. At
`2026-10-20T18:02:18.1588646Z` the selected package was still `oem101.inf`,
version `0.2026.1018.1`, and the installed SYS file retained SHA-256
`8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`.
The device nevertheless reported problem code 52, the service was stopped,
the five interface instances were disabled, and enumeration reported BDF
5:1.0 rather than the authorized 4:1.0 baseline. System evidence records a
successful intervening boot, Code Integrity event 3004 for
`LecS65AcqDrv.sys`, and Kernel-PnP event 219 for the failed service load.

The gate therefore stopped before any device path was opened. R0-5 through
R0-9 and both conditional R1 commands are NOT RUN. The event evidence proves
the Code Integrity rejection. A later elevated read-only audit proved that the
active EFI/BCD `{current}` entry lacks `testsigning`; `0xC0000428` is therefore
attributed to the absent Test Mode boot element, not package drift. No repair,
retry, PnP action or IOCTL is part of this result. Future batches must use the
exact stable hardware/instance identity gate in `scripts/test-runtime-baseline.ps1`;
BDF changes require review but are never a device-selection key.

## Kernel Runtime Qualification Batch 2D stopped preflight

The exact `lecdiag.exe` SHA-256 remained
`ADD4A869C05DD85A55DD9308927D2AE946907F26C0D0054E46A54738441FB7BA` and
the bounded runner SHA-256 remained
`58AE76C5F90EE34F7E8634E5CA769A36F134A13CD6E3262FE38D1C16E07700D7`.
A fresh mock command-chain run passed all eleven positive R0/R1 forms and five
malformed forms, 16/16. The previously established Windows PowerShell 5.1
serialization evidence therefore remained applicable; the fresh run added an
unchanged-source check rather than changing the command contract.

The only elevated live invocation stopped before its runtime-baseline snapshot
or any `lecdiag` call. `Win32_DeviceGuard` reported
`VirtualizationBasedSecurityStatus=0`, both code-integrity enforcement values
as `0`, and `SecurityServicesRunning={0}`. The one-shot wrapper correctly
rejected what it interpreted as a preflight mismatch, but its predicate was
wrong: it tested whether the array was nonempty instead of treating the
documented zero value as no running service. There are zero `*.attempted`
markers in the exclusive evidence directory. Consequently no device interface
was opened, no `DeviceIoControl` occurred, and R0-5 through R0-9 and both R1
commands remain NOT RUN. The stop/no-retry rule was honored. The private
failure manifest has SHA-256
`A41492D7CD65C20DE4CE0C72C3D40D643BC22EE8D30E9CD2BF8C7DAB9D1DF9FB`.
A subsequent passive baseline passed with unchanged package, SYS, device,
service, BDF and five-interface state. The target event window was empty.
Snapshot SHA-256 is
`08C05F481BBED834B8CEF857661267EA2F96FB3C343F39A1E748D155243422DA` and
supplemental evidence-manifest SHA-256 is
`D459BA05A31B90EFF665D320EFA7A4C336593CC09A994F9AF15607DD2CB7BC90`.

A future authorization must use a reviewed predicate that accepts only a
service list containing no nonzero values. It must create a new exclusive
evidence root and must not infer renewed hardware authorization from this
offline correction.

## R0/R1 completion attempt stopped before device access

The reusable `scripts/deviceguard-preflight.ps1` predicate now performs that
reviewed evaluation. It preserves raw CIM data, treats numeric zero as no
running service, distinguishes inactive, configured, active, unknown and
contradictory states, and rejects active CI policy files. Ten exact-predicate
tests pass under PowerShell 7 and Windows PowerShell 5.1. The current host's
`{0}` service vectors, VBS/CI status zero and empty active-policy set evaluate
as `Inactive` and allowed.

The one authorized elevated command-chain invocation nevertheless stopped in
the subsequent stable-identity baseline. The temporary external wrapper was
UTF-8 without a BOM; Windows PowerShell 5.1 interpreted its `Gerät` literal
through the legacy ANSI path as `GerÃ¤t`. The captured device location itself
was still `PCI-Bus 5, Gerät 1, Funktion 0`, and every substantive identity and
health check passed. The baseline correctly failed closed because the supplied
previous-location string differed and no acknowledgement switch was present.

No `*.attempted` marker exists, so no interface was opened and no
`DeviceIoControl` was issued. R0-5 through R0-9 and both conditional R1 reads
remain NOT RUN. No retry was made. The initial private manifest SHA-256 is
`8995F1FFE864ADCE58244AD5A8710CD71852B4FA1EE6E79AA3E6116671E8AA99`.
A later passive baseline passed with identical device and service state; its
SHA-256 is `55967F8633D61E66D8B89C91A883A6B831C0ED1D8C25C214FE03A5DF2B4D6780`,
the exact target-event window is empty, and supplemental manifest SHA-256 is
`AB9D0181B675FADEDB1B546498CAD4904AC1A0A3AE17044B98FD70E94000A48C`.

A future launcher must avoid locale-sensitive source literals under Windows
PowerShell 5.1, for example by constructing the character from its Unicode code
point or by consuming a retained UTF-8 JSON value. Location acknowledgement is
permitted only after the exact stable instance and hardware IDs have passed;
it must never change device selection.
