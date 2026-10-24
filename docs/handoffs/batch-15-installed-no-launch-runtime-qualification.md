# Batch 15 installed no-launch runtime qualification

## Scope and immutable runtime identity

Batch 15 qualified the already installed Batch 14 package without signing or
installation. The runtime identity remained:

- PCI instance:
  `PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0`
- installed INF: `oem103.inf`
- version: `0.2026.1024.1`
- loaded SYS SHA-256:
  `73B235BD4D19354612752A87BA499C06D15336F63A6B73EDDC0C674C3086D908`
- service: `LecS65AcqDrv`, running
- device: `OK`, problem code zero
- interfaces: all five legacy-compatible classes published

The previous `oem102.inf` rollback package remained available. No signing,
installation, reboot, Verifier, acquisition, GO, FPGA, EEPROM or firmware
operation occurred.

## Runtime execution reliability

The Batch 14 command did not hit a child IOCTL timeout. An outer automation
process terminated the long-lived elevated wrapper after passive collection
and before the first command-attempt marker. The correction is a persistent,
repository-controlled stage ledger:

- passive preflight and every command are separate short child processes;
- an immutable plan pins HEAD, worktree, executable and installed identity;
- an exclusive dispatch record precedes each child launch;
- completion is persisted before the next stage is admitted;
- a dispatched stage without completion becomes `UNCERTAIN` and cannot retry;
- no stage automatically retries and duplicate supervisors are rejected.

Focused results were 21/21 for the runtime harness and 10/10 for ledger
failure injection and interruption recovery.

## R0 and R1

The ledger completed exactly one dispatch for every authorized command:

- R0: 9/9 PASS; trace size `0x110`, register-list size `0x2CAE`, zero-length
  and zero-delay contracts matched.
- R1 PCI: PASS; vendor/device `1570:0005`, observed BDF `5:1.0`.
- R1 START: PASS; the legacy and independent bounded reads both returned
  `0xFFFFFFFF`.
- device health remained `OK`, problem code zero, after every stage.

Evidence root:
`C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-Batch15-RuntimeLedger-20261024-attempt1`

- `plan.json`:
  `70F877EA95AD0EBF86C3FEC84D56B7C072144EB489529CC915511069CD5BE949`
- `supervisor.log`:
  `082132F2745416141F55D93B04266DD26721F1E3D2EEA40E9DECF6083203AD34`
- `supervisor-result.json`:
  `1615F7D1EC6B64E36DD8DFC6F04BE2B488E83F7B137D3A15988DCCB2A2C8D884`

## Captured PnP restart

The first capture invocation failed before PnP dispatch because the supplied
PnPUtil string contained a literal `$env:SystemRoot` and the launcher correctly
rejected it as non-absolute. Its result SHA-256 is
`535C3C6BE71797E37C1B8D06FFF65B4E7BC95EFE3AD8A313CB1181D1015B01F5`;
`PnpRestartInvocations=0`. Stopping the remaining DebugView process was cleanup,
not a PnP operation.

The subsequent invocation used `C:\Windows\System32\pnputil.exe` and performed
the first and only PnP dispatch. It passed with PnPUtil exit zero and 48
`PNP_STATUS` records. The trace shows REMOVE/re-enumeration, IRQ reconnection,
DMA-parent START, five interface enables and final START. The installed INF,
version and SYS were unchanged. `DEVPKEY_Device_PowerData` reported D0.

Evidence root:
`C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-Batch15-PnpRestart-20261024-attempt2`

- `result.json`:
  `FA17B152129587C19AACAE11E387EB695CB6714774FAFEFC5483C5D951EAC913`
- `kernel-capture.log`:
  `3028C922047976077CC23A45BFC1FDDAC5F1C167FE1E586DD9D46BA22BB89866`
- PnP child log:
  `AF24F6CF75E88213AE79D4E09875D20F67AF3D9EB8AC3AF4E851407CDD0795BD`
- capture-ready record:
  `A234BA3E544948F80A752D8C7C56C1F99190A5DEC47E7B45023857FBC2091B44`

The post-restart passive preflight passed. Its `outcome.json` SHA-256 is
`57450E7C10991B2E8C4624FC71621FA66F19BEEE4E21A4559B6D33BC54776619`.

## No-launch result and source defect

The one authorized runner dispatched `lecdiag no-launch-prepare` exactly once.
It opened the intended interface and `CFDC2124` returned Win32
`ERROR_NOT_SUPPORTED` (50). No token was returned, so the harness correctly
stopped before `CFDC2138` and `CFDC2128`. There was no timeout and the process
tree closed. The device remained healthy; no hardware launch was authorized or
observed.

The installed source called `LecBuildDescriptorTable` during `CFDC2124` even
with `LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA=0`. That builder has two matching
`STATUS_NOT_SUPPORTED` paths: descriptor storage above 4 GiB and a source page
above 4 GiB. The retained trace cannot distinguish them. Both occur before the
WDM-v3 mapping call, so the live run provides no SG, DMA-logical descriptor or
rollback evidence.

Evidence root:
`C:\Users\LeCroyUser\.codex\driver-backups\Phase2C-Batch15-NoLaunch-20261024-attempt1`

- `result.json`:
  `11825D6416169362C19E4FB8B97D69FB24D39A8BB8452E53D6E2849ADC21AC9A`
- `no-launch-child.log`:
  `3000513C62C6501091222E8957B588D8C0E695F0CFF6504FDA7C048957AFB76E`
- `child-result.json`:
  `78E5C5016F78AED23FD192FA9AC4BA941167688F5733BC0A7BCD14ECFA5E7A61`

## Offline correction and qualification

Production `LecRegisterTransfer` now builds only the locked MDL chain and
publishes the token. `LecBuildDescriptorTable` is compiled only under
`LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA`; the default remains zero. No ABI, MMIO,
GO, interrupt or WDM-v3 transaction behavior changed.

Offline results:

- Debug x64: 0 warnings, 0 errors; SYS SHA-256
  `A0BFFFEF762466FF9854344F18C7F97B58992BDBC156A7D0E8A60D8607B02429`.
- Release x64: 0 warnings, 0 errors; SYS SHA-256
  `D553966140FC7B8BB56EB0B09013D16330083A6EFDA12B4FC0BE4F07893234BD`.
- pre-GO side-effect contracts: 10/10 PASS.
- source contracts: 69/69 PASS.
- no-launch mock and launcher: 3/3 PASS.
- previously completed focused SG/PnP: 157/157 PASS.
- previously completed explicit WDM-v3 transfer: 74/74 PASS.
- complete Dry suite: PASS, including default DMA 5,000/5,000 and INTEN
  1,000/1,000 stress iterations.

## Remaining boundary

The correction is neither signed nor installed. The next bounded sequence is:

1. review and authorize one signed package containing the correction;
2. separately authorize one installation and captured START;
3. repeat the passive R0/R1 and PnP prerequisites as required for that new
   binary identity;
4. separately authorize one new no-launch preparation attempt.

Even a successful future no-launch attempt would prove only mapping,
descriptor preparation, launch rejection and never-launched cleanup. Physical
DMA bus-idle, posted-write retirement and safe post-GO release remain unproved;
no external PCI-analyzer evidence has been collected.
