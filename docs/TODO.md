# Development TODO

This file tracks unfinished work and deferred features. The current, verified capabilities are summarized in the [README](../README.md); detailed historical evidence belongs in the technical docs and [engineering handoff](next-chat-handoff.md). Entries here are not claims of implemented functionality.

## Active compatibility work

- [ ] **Validate newly implemented read-only `0x00223044`:** the original `this+0x138` points to BAR0+0x000 (FVER/START). Native x64 case and `lecdiag start-register` equality diagnostic are committed. Build on Windows first; then use the diagnostic on the real test scope with XStream closed. No writes are necessary.
- [ ] **Complete `0xCFDC2194` read-and-clear status:** Ghidra confirms an exact 29-byte reply (type DWORD 2 at +4; latched DWORD from original `this+0x116A` at +8; then clear latch). The direct field scan found the reader/clearer only; its nonzero producer and concurrency semantics remain unresolved. Map those before porting; never pretend a hardcoded zero is full compatibility.

- [ ] Continue regression testing of normal XStream operation on the real x64 scope, preserving the working PCI, IRQ, DMA and AP015 baseline.
- [ ] Complete and validate the remaining legacy IOCTL and acquisition variants. In particular, keep `0xCFDD219F` (WOW64-sensitive `METHOD_NEITHER`) and unobserved transfer forms gated until their memory and DMA semantics are proven.
- [ ] Investigate any remaining differences in AP015 calibration/control replies where physical behavior has not yet been independently verified. Normal probe identification, hotplug and the open-jaw warning are already working.
- [ ] Port the native DS2433 `WRITE_DALLAS_MEMORY` (`0x00223088`) using the recovered x86 handler `FUN_00011f54` and helpers `FUN_00016d90` / `FUN_00016f2c`. Validate the real scratchpad/write/copy/readback sequence first on a disposable DS2433, including error handling and post-power-cycle verification. Do not return fabricated success or experiment with the sole licensed chip.
- [ ] Add a private backup container that binds the complete eight-byte Dallas ROM ID (including CRC8) to the 512-byte memory image and an integrity checksum; retain raw 512-byte `.bin` import/export for compatibility.

## Planned maintenance tooling

- [ ] Extend the existing read-only `lecdiag dallas-backup` workflow with offline image verification and a guarded, independently tested restore path once physical writing is proven.
- [ ] Develop a standalone Windows Dallas maintenance manager for status, private backup, compare and eventual verified recovery. An optional native x64 Device Manager `Dallas EEPROM` property page may launch it; evaluate INF/catalog/signing changes before packaging.
- [ ] Ensure exported diagnostics do not expose real license contents or unique hardware identifiers unnecessarily; old raw startup traces must remain private.

## Deferred: virtual Dallas recovery

> **Deliberately postponed as of 2026-09-29.** Do not treat this as the next implementation task or disconnect the working chip to start testing it.

- [ ] Evaluate a clearly identified virtual Dallas mode for an original-chip failure or replacement: a private, authenticated copy of the factory eight-byte ROM identity and the matching 512-byte memory image, with a separate coherent virtual write/shadow policy.
- [ ] Establish whether the PCI FPGA or any pre-IOCTL initialization depends directly on a physically responding DS2433. Software substitution at the host IOCTL boundary might not be sufficient.
- [ ] If implemented later, first validate virtual ID/read behavior with the original physical chip still attached. Only consider a reversible, powered-off and electrically verified `ID_DATA` isolation experiment afterward.
- [ ] Verify whether and how XStream binds its license records to the Dallas factory ROM identity. A new DS2433 has a different immutable ROM serial even if the writable memory is copied; universal license invalidation on replacement has **not** yet been proven.

Technical background: [Dallas recovery/UI design](dallas-device-manager-recovery-design.md), [Dallas test plan](dallas-license-memory-test-plan.md), [hardware map](pci-card-acquisition-board-topology.md).
