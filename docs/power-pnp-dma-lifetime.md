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
