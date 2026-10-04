# Current project status

## Build and software regression

- Windows x64 Debug driver and `lecdiag` build successfully with the installed
  WDK 10.0.28000.0 toolchain.
- The hardware-independent Dry regression covers source/ABI contracts, DMA
  descriptor layout, mapping ownership, the quarantined asynchronous SG bridge
  and the inactive synchronous v3 no-launch stage. See
  [regression testing](regression-testing.md) for the current counts.
- Current result: source/ABI 25/25, layout 13/13, ownership 17/17,
  asynchronous SG 27/27 and synchronous v3 81/81; overall 163/163 PASS.

## DMA/PnP staging

- The active acquisition path is unchanged and still uses PFN-derived DMA
  addresses. Adapter-mapped staging is not reachable from acquisition, IOCTL
  or live PnP paths.
- The synchronous v3 stage uses `GetScatterGatherListEx` with
  `DMA_SYNCHRONOUS_CALLBACK` and no execution callback. It has no launch API.
- No-launch mappings now have centralized adapter/PDO ownership, unique transfer
  contexts, explicit `FreeAdapterObject(DeallocateObject)` cleanup, STOP/drain
  rundown, token serialization, MDL/map-register validation and WR6k descriptor
  capacity derived from the actual common-buffer length. The owner has an
  explicit construct/init/stop/destroy lifecycle, so failed repeated init does
  not overwrite live mappings or reinitialize an active lock.
- Ownership exclusion is confirmed only per adapter context. Separate contexts
  for one physical device are not globally serialized; future PnP integration
  must enforce a single context or supply a proven device-wide registry.
- Unknown-active hardware DMA remains quarantined. No software test establishes
  physical WR6k bus-idle, safe removal of an active mapping or complete real PnP
  teardown.

## Hardware status

- No hardware, driver installation/reload or XStream testing was performed for
  the current lifetime-hardening changes.
- The established real-PCI safe ABI baseline remains 9/9 PASS and the last
  established hardware regression milestone remains 11/11 PASS.
