# Original x86 driver: CPU-frequency-dependent hardware command failures

## Reported observation (not yet independently reproduced)

On an Intel Core i5-3450 host running Windows 10 x86 and the original LeCroy 32-bit driver, XStream regularly reports failed hardware actions, including an inability to set trigger threshold, at CPU frequency levels above the lowest available level. Holding a *fixed* frequency even approximately 100 MHz above the minimum does not resolve the failures. The lowest CPU frequency level is reportedly reliable.

On a Windows x64 system with the replacement driver, this behavior has not been observed so far. The operating systems, drivers, and possibly other configuration details differ; this is **not yet** a controlled driver-only A/B comparison.

## Interpretation (hypotheses, not findings)

1. CPU-speed-sensitive delay loop or uncalibrated busy wait in a legacy command path.
2. Inadequate MMIO ordering/readback, or an FPGA/peripheral readiness interval assumed implicitly by the old driver.
3. Scheduling/interrupt/command-completion race whose timing shifts with CPU speed.
4. Incorrect timer or TSC-based timeout accounting.
5. Platform PCI, firmware, chipset, power-management, or electrical effects that correlate with CPU P-state.

Do not infer that PCI bus frequency scales with CPU core frequency. Do not infer a proven original-driver defect or a proven replacement-driver fix from the current observations alone. The error wording may reflect a later verification failure rather than the failed primitive operation.

## Safe verification plan

- Capture exact error text, XStream action, driver version, clock readout and actual effective CPU clock, BIOS power settings, and hardware/PCI topology.
- At minimum clock and one fixed higher clock, repeat the *same* trigger-level operation, with identical boot state and XStream settings, recording attempts, failures and latency. Avoid invasive register writes and dangerous diagnostic IOCTLs.
- Differentiate command-write, status/readback, polling, event/IRQ, and timeout failures from already available traces.
- On the original x86 binary, inspect delay loops, KeStallExecutionProcessor, KeQueryPerformanceCounter, RDTSC, timer polling, IOCTL completion, and MMIO ordering around trigger-level paths.
- If available, compare x86 original and replacement on the same host or compare x64 frequency levels on the same host. Keep OS/driver/platform confounders explicit.

## Implementation implications

No production change is justified without a localized timing or ordering fault. Preserve original functional behavior and hardware timing requirements, not accidental speed sensitivity. Prefer documented Windows kernel timing and register-access primitives; do not introduce arbitrary delays as a blanket workaround.

## Evidence level

User-reported, not independently reproduced. Investigation pending.
