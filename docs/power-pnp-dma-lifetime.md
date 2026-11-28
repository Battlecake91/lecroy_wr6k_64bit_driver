# POWER and PnP DMA lifetime

This document is the canonical software-side admission contract for the
Batch 12 WDM-v3 preparation path. It does not claim physical DMA idle.

START creates exactly one per-PDO adapter context, common descriptor buffer
and synchronous SG owner before setting `Started`, opening IOCTL admission or
publishing interfaces. Failed partial initialization rolls back without a
mapping or launch.

QUERY_STOP and QUERY_REMOVE close global IOCTL admission, drain admitted
IOCTLs, close the DMA-parent admission gate and only then forward the query.
A lower-stack rejection or a successful CANCEL reopens admission only when the
parent remains Started+D0. STOP, REMOVE and surprise removal close admission,
drain IOCTLs, disconnect the interrupt, drain DPC/timer work and release only
resources whose no-launch provenance is established.

Device SET_POWER uses a nonpaged completion context and retains both the IRP
RemoveLock and a publication reference until lower completion. D0-to-Dx closes
DMA preparation before forwarding and records Dx only on successful lower
completion; failure restores D0 admission. Dx-to-D0 remains closed until lower
completion succeeds. The path adds no MMIO and no undocumented power DDI.

The selected synchronous mapping contract has no delayed callback. A caller
holds `TransferMutex` while its locked MDL is mapped and released, preventing
unregister, last-CLOSE or process cleanup from freeing it. A future
asynchronous mapping path would require a separately referenced request/file
context and is not implicitly authorized by this design.

Software quiescence is not hardware retirement. If any future build can issue
GO, IRQ/DPC rundown, STOP, REMOVE and Dx cannot release its mapping until an
independent physical-idle predicate has been established; otherwise the full
owner must enter permanent `UnknownActive` quarantine.

## PCI configuration and board context on resume

The PnP manager's assigned resources describe the expected mappings.
`DEVPKEY_Device_PowerData` describes the most recent reported power state.
Neither establishes that PCI memory decoding is active. In the
[Batch 33 post-resume snapshot](handoffs/batch-33-xstream-com-timeout-causality.md#post-resume-pci-decode-snapshot),
the PCI function still answered configuration reads, but the Command
register was zero and all three assigned BAR bases read zero. The existing
MMIO mappings had no verified decoding target. Check memory-space enable
and matching BAR bases before interpreting any diagnostic MMIO value.

### Recovered x86 behavior

- In PnP dispatch `0x1A6EA`, START takes the lower-stack success path
  when the framework's lower-first policy is enabled, then invokes
  `+0x88 -> 0x115C4` to consume the translated resources and construct
  three MMIO mappings. STOP and REMOVE take distinct teardown paths. See
  [the DMA lifetime audit](legacy-dma-abort-bus-idle-audit.md).
- Under its framework policy flags, power dispatch `0x1B096` calls
  `+0x120 -> 0x10D6E -> 0x138D4` before a supported system down-transition.
  The quiesce callback saves the software INTEN mask, masks errors via
  BAR0 ERRM and requests a synchronized INTEN clear. The synchronized
  writer is also gated by the framework's interrupt state. This sequence
  performs no verified DMA drain.
- On the supported system power-up branch, `0x1B096` calls
  `+0x11C -> 0x10D62 -> 0x138B2 -> 0x1381E` before its
  `PoCallDriver`. The lower system-IRP completion adapter
  `0x1A410 -> 0x1A342` can request the mapped device power state with
  `PoRequestPowerIrp` after lower-stack success. The device power-up
  completion `0x19C54 -> 0x196B4` takes a different callback, `+0x114`,
  which is a no-op thunk on this board. The recovered register restore
  belongs to the system-transition callback and supplies no verified
  post-D0 PCI decode check.
- The restore helper invokes the board startup probe `0x12FDE` on existing
  mappings. It writes BAR0 START (`+0x00C`, distinct from FVER at
  `+0x000`) and tests its bit 0. The success branch drives BAR1 ITMODE
  through 7 then 3 and uses the BAR2 buzzer. On successful startup,
  `0x1381E` clears interrupt sources, resets BAR1 MTTCTL, replays cached
  MAMDAT and MAMSEQ entries through `0x11A30`, restores the saved
  INTEN mask under synchronization, and can signal an event for a
  pending condition. `0x14847` binds those register wrappers to their
  respective BARs. The recovered chain shows no PCI Command/BAR
  programming, PCI FPGA image reload or BAR2 ONEWIRE command. The
  observed BAR2 access is the buzzer at `+0x000`, so this path provides
  no direct evidence of a 1-Wire-specific restore.

### Native x64 behavior and diagnostic implication

`LecS65Power` forwards system power IRPs and reacts to device SET_POWER.
The current source has no `PoRequestPowerIrp` call to translate system
power into a device transition at this layer. `LecDmaPowerCompletion`
calls `LecDmaPnpCompleteResumeD0` on lower completion. Successful lower
completion updates the software power state and can reopen DMA admission.
It performs no PCI decode validation, vendor register restore, IRQ-mask
replay or cache replay. `LecRunLegacyStartupProbe` is used in PnP START.
Its START bit test alone would also accept an all-ones read, so it does
not qualify a resume mapping.

Windows PCI handles PCI power/configuration and the device power policy
owner requests the device transition. Vendor context restoration is
separate. The source difference and disabled decode after wake justify
checking the system-to-device IRP sequence before designing a register
restore. The snapshot does not show which IRPs ran at wake or when the
configuration was lost. Replaying the x86 MMIO sequence with disabled
decoding would use mappings that have not been validated.

See Microsoft [PCI power management and device drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/pci/pci-power-management-and-device-drivers),
[system set-power IRP in a device power policy owner](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/handling-a-system-set-power-irp-in-a-device-power-policy-owner),
and [CM_POWER_DATA](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/wdm/ns-wdm-cm_power_data_s).
