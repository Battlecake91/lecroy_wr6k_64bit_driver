# Runtime Qualification Batch 4 handoff

This handoff records the still-open captured PnP and Driver Verifier gates.
Raw logs remain private outside the public repository.

## Runtime result

- Baseline: PASS for exact instance, `oem101.inf` 0.2026.1018.1, running
  service, five interfaces and loaded SYS SHA-256
  `8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A`.
- Authorized operation: consumed without retry.
- Launcher result: FAIL before PnPUtil because the raw DebugView log remained
  zero bytes during a 15-second readiness gate.
- Invocation counts: capture 1, install 0, restart 0.
- Post-state: unchanged and problem-free; no target event, SetupAPI change or
  Event 411; no remaining DebugView/DBGV component.
- Root cause: valid kernel records were buffered and became visible only after
  the gate. The raw log later contained 5,861 StorNVMe kernel records.
- Consequence: STOP/START, IRQ/interface lifetime and Event 411 recurrence are
  still not runtime-qualified. A new explicit restart authorization is needed.

## DMA review result

PR #27 head `1120687c181e85ade70f91d656d127ef0533d7a6` passed independent
source review, Debug/Release builds, the complete Dry suite and 66,000 expanded
DMA/INTEN stress iterations. No defect or ABI change was found. It remains an
offline-only, uninstalled change stacked on PR #26.

## Verifier blocker

The proposed non-DMA mask `0x0002093B` is retained. Verifier is currently off,
but WinRE is disabled and the 1,280-MiB page file does not demonstrate useful
dump backing for about 8 GiB RAM. Before activation, separately authorize and
verify a recovery path, dump/page-file policy, Safe Mode `verifier /reset`
rehearsal, one-boot configuration and reboot. Do not combine the first
Verifier boot with PnP, hardware IOCTLs, acquisition or DMA.

## Integration order

GitHub and local merge-tree checks currently report both PRs mergeable. PR #26
targets `codex/phase2c-instrumented-package`; PR #27 targets PR #26's branch.
Merge PR #26 first, then re-evaluate PR #27 against the advanced base. Do not
rebase, force-push or merge automatically.
