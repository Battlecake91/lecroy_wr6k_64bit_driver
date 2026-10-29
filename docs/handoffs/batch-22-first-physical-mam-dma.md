# Batch 22 first physical MAM DMA handoff

## Passive gate status

Branch `codex/phase2c-runtime-batch22` starts at Batch 21 commit
`6ed89647861201f92580fe02deb35c80d31740b8`. No driver, package or diagnostic
binary was rebuilt, signed, installed or restarted.

The repository-controlled passive launcher preflight passed with zero capture,
installation, PnP restart or no-launch invocations. A separate passive snapshot
confirmed:

- `oem106.inf`, version `0.2026.1029.1`;
- loaded SYS SHA-256
  `647A7B38597C19D96DF509557E05895C5074727C329EFBD182AD01AC144E3F71`;
- exact PCI instance, device `OK`, problem code zero and running service;
- D0 (`CM_POWER_DATA.MostRecentPowerState == PowerDeviceD0`);
- five published interfaces;
- `PNP_STATUS:`, `LECS65_WDM_V3_MAM_DMA_ENABLED` and
  `LECS65_FIRST_MAM_EXPERIMENT_ONLY` in the loaded SYS;
- test signing enabled, Secure Boot false, Verifier flags zero and no verified
  drivers;
- retained kernel-dump configuration (`CrashDumpEnabled=2`, automatic reboot
  disabled);
- no XStream, acquisition, `lecdiag`, `lecwatch` or DebugView process and no
  residual Dbgv driver;
- no Batch 22 launch-attempt marker.

The diagnostic executable is hash-pinned as
`0AF2D96B95D867EE6B0BCA2066BBE94D0F36E6DA743E81EA06AB078E594CDD34`.
DebugView 5.02 remains hash-pinned as
`7954A8BBEB1F650BB5D2B8C4C6B642FD4585319D2092433CA841C7D672A4634B`.
The qualified live-stdout readiness path remains available but was not started
during passive preflight.

## Evidence

Private local evidence is retained outside the public repository:

- `Batch22-Preflight-20261029-attempt1/result.json`, SHA-256
  `66BF99B83E3A30D1E37D78FEC98FCB91C0AA295188C16E490ABC9236CAEBF8BA`;
- `Batch22-PreGoPassive-20261029-attempt1/pre-go-passive.json`, SHA-256
  `E614BC40F327DDE584C478EFAAB6130ECFA737CE843EEF2FA128A8B4749486EC`;
- `Batch22-PreGoPassive-20261029-attempt1/runtime-baseline.json`, SHA-256
  `165C93D4FE83E011FC8F5AE859CB5776356F367CD2EC76ECD6A4DF7B693904C6`.

Software inventory shows only x64 NVMe model `HFM256GDHTNG-8510B`, serial
`ACE4_2E00_9590_A2AA.`, online and healthy. This is not proof that the
productive x86 disk is physically disconnected.

## Required owner gate

GO count is zero. Before any capture/command dispatch, obtain one explicit
owner statement that both:

1. the productive Windows x86 disk is physically disconnected; and
2. exactly one transfer using the installed `oem106.inf` candidate is
   authorized with:

```text
lecdiag mam-dma-once I-AUTHORIZE-ONE-MAM-GO
```

After that statement, start the hash-pinned live-stdout kernel capture, require
its readiness record, create the exclusive durable MAM attempt marker, and
dispatch the command once with a five-second timeout. Authorization is consumed
at dispatch. Never retry. A timeout or uncertain completion must preserve
`UnknownActive` ownership and stop all further active hardware testing.
