# Batch 12 acquisition integration handoff

## Current state

Branch `codex/phase2c-runtime-batch12` starts at
`ece626ab73eed7e135a644d0251b5595ab5e2e1b`. The bounded one-channel
`CFDC2124`/`CFDC2138` MAM request is connected to the production WDM-v3
preparation owner. The public ABI is unchanged and the launch gate remains
unconditionally closed. No hardware operation was performed.

The live source path owns PID/FileObject identity, the persistent locked MDL,
DeviceControl RemoveLock, active-IOCTL admission, `TransferMutex` and a per-PDO
publication reference. It maps with synchronous `GetScatterGatherListEx`,
validates/copies DMA-logical SG entries, encodes the common-buffer descriptor
chain, publishes it, receives `LecDmaTxLaunchDisabled`, cancels and releases
the never-launched mapping exactly once. No PFN address, GO or acquisition
MMIO is reachable from this path.

START, QUERY/CANCEL, STOP/REMOVE/surprise and device D0/Dx completion now drive
the same adapter parent. Physical idle remains unproved. Original-x86 timeout,
process cleanup and teardown paths contain no retirement fence; available FPGA
and schematic evidence has no RTL/bridge contract. Preserve irreversible
`UnknownActive` after every possible future GO.

## Verification

- Debug and Release x64: zero warnings and zero errors with SDK/WDK
  10.0.28000.0 and MSVC 14.41.34120.
- Complete Dry runner: PASS.
- Source contracts 69/69; synchronous SG/PnP integration 157/157;
  publication 38/38; continuation 17/17.
- Expanded DMA 50,000/50,000 and INTEN 10,000/10,000; additional DMA
  250,000/250,000.
- Installed `oem102.inf` version `0.2026.1022.1` and loaded SYS hash
  `D4CFA707E4458D98B1CA91F4C6ADC1603E073752D3E18B77F457B1F2E294FC4A`
  remained unchanged and healthy.

## Next gate

Do not install this build or invoke `CFDC2138`. The next active milestone
requires separate authorization for a signed no-launch candidate and a
software-only runtime preparation test. GO must remain closed. Any later GO
experiment additionally requires external PCI observation and a reviewed
physical-idle/retirement criterion.
