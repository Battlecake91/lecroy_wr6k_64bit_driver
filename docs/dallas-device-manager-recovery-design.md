# Dallas EEPROM recovery UI: Windows Device Manager integration

**Status:** Design proposal, 2026-09-29. No kernel write path or GUI has
yet been implemented. This proposal targets recovery/maintenance of the
owner's PCI-card DS2433. No license records, actual ROM serial numbers,
raw trace previews or owner-specific identifiers are published here.

## Can Device Manager display a custom Dallas page?

Yes. Windows supports device-specific **property-page extension DLLs**.
A native x64 DLL may export `ExtensionPropSheetPageProc`; the driver
package INF can associate it with the relevant device using the
`EnumPropPages32` AddReg value and install the DLL using CopyFiles.
This creates a separate tab on the device's Properties dialog, rather
than rewriting the normal Windows Driver tab.

Microsoft references:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/types-of-device-property-page-providers
- https://learn.microsoft.com/en-us/windows-hardware/drivers/install/specific-requirements-for-device-property-page-providers--property-pag

Do **not** add a legacy co-installer just to supply this page:
Microsoft marks co-installers/class installers deprecated, and since
January 2023 its hardware signing portal no longer signs packages
containing a co-installer:

- https://learn.microsoft.com/en-us/windows-hardware/drivers/develop/removing-coinstallers

**Packaging compatibility:** Current
`driver/LecS65AcqDrv.inf` installs a custom DataAcquisition
class, PCI matching instance and one `.sys` file; it does not
currently include a user-mode DLL or GUI. Check INF validation,
DLL installation path and catalogue/signing implications before
modifying the production package. In particular, avoid accidentally
turning a signing-compatible package into a co-installer package.
An optional/add-on GUI that does not require registering a new
property-page DLL remains useful regardless of INF-signing choices.

## Proposed user-mode architecture

```text
Device Manager / LeCroy Acquisition Device / Dallas EEPROM (optional)
       |
       +-- native x64 property-page extension DLL
       |   - chip presence, family, partial scope-ID/status
       |   - backup and open-recovery-manager actions
       |
LeCroy Dallas Manager.exe (standalone Windows GUI)
       |
       +-- shared user-mode Dallas I/O library
               |
               +-- device interface + explicit Dallas IOCTLs
                    |
                    +-- replacement kernel driver (one-wire controller)
                           |
                           +-- PCI Spartan U3 -> ID_DATA -> U11 DS2433
```

The user-mode DLL/GUI owns dialogs, file I/O, secure prompts, diff
view and progress feedback. The kernel driver implements validated
ROM/memory operations, never Windows dialogs or arbitrary file paths.
A standalone executable is preferable for full recovery and testing
even if a small Device Manager page is installed: it can run without
Device Manager and can be updated independently.

The property-page DLL is a **native x64** component for the x64
Device Manager host; the original 32-bit XStream application continues
to use its established WOW64 driver ABI. An optional Device Manager
page need not redirect or reinterpret the XStream driver channel.

## Scope identity: factory ROM versus writable memory

The DS2433 datasheet specifies:

| Field | Location and mutability |
|---|---|
| Family `0x23` | Byte 0 of the **factory-programmed 64-bit ROM** |
| Serial | ROM bytes 1..6, 48-bit factory identity |
| Dallas CRC8 | ROM byte 7 |
| 512-byte EEPROM | Separate 4096-bit user-writable memory, sixteen 32-byte pages |
| Scratchpad | Additional 32-byte write staging buffer, not an extra EEPROM page |

Source: https://www.analog.com/media/en/technical-documentation/data-sheets/DS2433.pdf

The user's displayed **primary six-hex-digit scope identifier**
matched ROM serial bytes **1..3 interpreted as a little-endian
24-bit value**, including independent matching observations
across seven available private runtime captures. The source
and algorithm for the separate **two-digit display suffix
remain unresolved**. Thus do not call the whole displayed
scope-ID an EEPROM field; the main confirmed component
comes from the immutable ROM. The whole 48-bit serial
and full display formatting must remain separate questions.
Avoid publishing the unique full ROM identity in logs or docs.

**Critical for replacements:** restoring the old 512-byte image
into a *different* DS2433 cannot rewrite its factory ROM
serial number. XStream license validation may use
this ROM as part of its device binding; that particular
binding algorithm has not yet been source-decoded. The
manager must distinguish EEPROM-repair-on-the-same-chip
from physical chip replacement and warn on ROM mismatch.
No fake ROM-ID injection into normal XStream operation
is implied by this recovery feature.

## Proposed Dallas tab and standalone manager features

| Function | Status and desired behavior |
|---|---|
| Probe/identify | Read device presence, ROM ID, check Dallas CRC8, show family 0x23 and derived **confirmed** main scope-ID; display suffix only when decoded. |
| Backup | Read complete 512-byte EEPROM **twice**, verify both match and ROM ID is unchanged; create a **new** private file, never silently overwrite an existing backup. Already implemented in `lecdiag dallas-backup` for raw binary output. |
| Backup container | New private versioned format containing original ROM ID, full 512-byte image, SHA-256, format version, timestamp and optional app/driver version. Also allow legacy **raw 512-byte `.bin`** import/export, but mark legacy raw import as **unbound** unless associated with separately captured ROM identity. |
| Compare / inspect | Side-by-side 16 x 32-byte page occupancy, changed-byte offsets, before/after diff; mask or avoid showing potential license content by default. Existing read-only `inspect-dallas-image.ps1` already supports redacted numeric statistics. |
| Edit image | Offline hex editor for a **copy** of a private backup. Save separate file; show differences and affected pages. Manual edit is *not* evidence a fabricated license will be accepted, or that an apparently all-zero page is unallocated. |
| Restore | Separate privileged workflow: validate file length/checksum; re-read actual ROM ID and check exact chip match; read current image; preview page differences; confirm; write only changed pages (if original hardware semantics permit), verify read-back **per page** and all 512 bytes after completion, save failure offset/status. No best-effort silent success. |
| Recovery | Offer in-place EEPROM recovery if ROM/presence and electrical write path remain functional. If chip/bus is physically dead or ROM ID cannot be read reliably, UI must report **external programmer/hardware repair required**. |
| Advanced controls | Explicit expert mode, administrative permission, typed confirmation and no simultaneous XStream/Dallas writers. No one-click mass erase on the only licensed PCI card. |
| Optional shadow mode | A **temporary read-only, same-ROM** diagnostic view may help compare how XStream handles a saved image, but must be clearly labeled *virtual/not programmed*. Avoid using it as a substitute for actual hardware recovery; disable by default and on restart. |

**Private backups are not public software artifacts.** Keep raw license
images, ROM IDs, serial fingerprints, trace payloads and edited
candidate images outside public GitHub; `.gitignore` already excludes
`license-backups/`, `*.ds2433.bin` and local trace capture folders.

## Write/recovery engine prerequisites

The existing x64 replacement currently implements Dallas
`0x00223080` GET ID and `0x00223084` READ, but **does not
implement `0x00223088` WRITE**. In real x64 XStream
trace `011858`, its genuine 512-byte WRITE was rejected
with `0xC0000010 STATUS_INVALID_DEVICE_REQUEST`, so a
GUI cannot meaningfully offer functional hardware Restore yet.

The original x86 Ghidra export
`ghidra_exports/selected/00011f54_FUN_00011f54.c`
shows 1..512-byte input, chunks at most 32 bytes through
`FUN_00016d90`, a full requested-length hardware
read-back through `FUN_00016f2c`, `RtlCompareMemory`,
and up to three attempts. The low-level helpers are still
required to match the original controller semantics.
Do not implement a fake success stub or bypass
hardware write verification.

The DS2433's own specification uses
`Write Scratchpad` -> verify scratchpad including
address/E/S status -> `Copy Scratchpad`; copying
requires the specified 1-Wire supply conditions.
A conventional scratchpad copy takes up to 5 ms,
during which 1-Wire voltage must stay at least
2.8 V. This is especially relevant to a reported
corruption on power loss or an unreliable pull-up.
See the manufacturer's datasheet above. Use
the original board/controller implementation
and independently verify timing and power on
a disposable spare chip, not the only licensed
PCI card.

Further recovery safety requirements:

1. Writes and XStream access must be serialized.
   Perform restore with XStream closed; driver
   rejects another writer while recovery is active.
2. Validate `0x23` family, ROM CRC8 and exact
   target match to the backup metadata. A readable
   EEPROM alone is not proof of a matching chip.
3. Read and privately save the **current** image
   before attempting any change, even if it appears
   partly corrupted.
4. After each written page verify both scratchpad
   operation and actual EEPROM read-back; on first
   mismatch stop, preserve diagnostic offsets and
   retain original recovery file. A full 512-byte
   compare and optional read after power cycle
   complete the process.
5. Initial write/restore validation MUST occur
   on a **spare disposable DS2433** and include
   interruption/error cases and post-power-cycle
   byte-for-byte comparison before enabling
   recovery on a working licensed PCI card.
6. If the physical chip is nonresponsive even
   to the factory ROM command, software on the
   same PCI card has no evidence of an operable
   path; external hardware programming or
   replacement may still be unavoidable.

## Build stages (recommended order)

**Stage 1:** implement a small read-only
`LeCroy Dallas Manager.exe` using existing
ID/read/backup code; add stable backup-container
format and masked compare UI.

**Stage 2:** export and analyze original low-level
helpers `FUN_00016d90` and `FUN_00016f2c`,
then port `WRITE_DALLAS_MEMORY` exactly with
scratchpad checks, stop-on-error and full readback.
Validate on disposable chip and preserve
all current PCI/IRQ/DMA safety constraints.

**Stage 3:** enable verified restore, pre-write
backup and offline editing in the standalone
manager with separate administrator confirmation.

**Stage 4:** optionally integrate the concise
status/backup/recovery-launch property tab via
a native x64 `EnumPropPages32` extension DLL,
after evaluating the current INF/catalog/signing
package. Avoid legacy co-installer dependencies.

The feature is maintenance/recovery of the
owner's existing PCI-card memory. It neither
creates genuine license entitlements nor
claims that replacing the physical DS2433
reproduces its factory ROM identity.

Related: `docs/dallas-license-memory-test-plan.md`,
`docs/pci-card-acquisition-board-topology.md`,
`docs/hardware-register-map.md`, `AGENTS.md`,
`docs/next-chat-handoff.md`.
