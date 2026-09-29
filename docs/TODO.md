# Development TODO

This file tracks unfinished work and deferred features. The current, verified capabilities are summarized in the [README](../README.md); detailed historical evidence belongs in the technical docs and [engineering handoff](next-chat-handoff.md). Entries here are not claims of implemented functionality.

## Active compatibility work

- [x] **Validate newly implemented read-only `0x00223044`:** owner-reported real-scope `lecdiag start-register` PASS on 2026-09-29. Legacy four-byte START/FVER returned `0x00000002`, and an independent generic register read of BAR0+0x000 also returned `0x00000002`. The earlier non-elevated signed-load attempt / `ERROR_INVALID_FUNCTION` result is superseded for this IOCTL. Detailed successful build/install logs were not supplied. The immediate post-change XStream/AP015 regression was subsequently owner-reported working (see next completed item).
- [x] **Immediate post-`0x00223044` XStream regression:** after successful physical register comparison, the owner replied `Ja klappt soweit alles` to the existing acquisition/control/AP015 checklist. No observed regression in the exercised baseline; not an instrumented full test matrix.
- [ ] **Keep a source-verified original/x64 coverage inventory:** 27 original top-level dispatch codes identified, 22 present in the x64 top-level switch (including one deliberately gated), five still absent after the passing `0x00223044` test. The missing cases are `0x0022303C`, `0x00223088`, `0xCFDC2130`, `0xCFDC2194` and `0xCFDC2400`. See the current section in [IOCTL map](ioctl-map.md); do not confuse identified code numbers with complete behavioral support.
- [ ] **Complete `0xCFDC2194` read-and-clear status:** Ghidra confirms an exact 29-byte reply (type DWORD 2 at +4; latched DWORD from original `this+0x116A` at +8; then clear latch). The initial direct field scan found the reader/clearer only. Source review identifies another candidate coordinate: `FUN_000115C4` constructs the hardware subobject at `main+0x1E0`, making the latch potentially `main+0x134A` if the dispatcher passes the same subobject. Overlap-aware exact-displacement targets and dispatch-wrapper ASM have been added to `ghidra_scripts/targets.txt`; the Ghidra rerun is still pending. Investigate indirect writers and concurrency before porting; see [focused latch investigation](cfdc2194-status-latch-investigation.md). Never fake a constant-zero success.

- [ ] Analyze remaining original hardware-writing/control handlers before porting: `0x0022303C` (`FUN_00012CAC` -> `FUN_0001259A` register write), `0xCFDC2130` (`FUN_00011CFF` serial-trigger FPGA programming), and `0xCFDC2400` (`FUN_00013A2E` -> `FUN_00012EDE` control callback). Establish input boundaries, call frequency, hardware effect and appropriate test isolation first.
- [ ] Differentiate complete original driver semantics from partial x64 coverage for nested `0xCFDC2110` subcommands and unobserved `0xCFDC2138` transfer shapes; compare redacted original x86 and native x64 runtime behavior where meaningful.
- [ ] Continue **broader** regression testing of untested XStream configurations and acquisition variants. The immediate post-`0x00223044` baseline check has already been confirmed working. Preserve the proven PCI, IRQ, DMA and AP015 paths.
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
