# Batch 18 MAM DMA restoration handoff

## Result

The original MAM sequence is integrated with the persistent WDM-v3 mapping
owner behind the default-off `LecWdmV3MamDma` build property. Normal matching
IRQ/DPC completion retires the per-call mapping and permits repeated use of the
legacy registration. Uncertain post-GO outcomes quarantine ownership.

The unsafe PFN path remains disabled. Ordinary builds remain no-launch. No
hardware operation, signing, installation, PnP transition or XStream run was
performed.

Offline verification completed with Debug and Release x64 driver/lecdiag
builds at zero warnings/errors, the complete Windows PowerShell 5.1 Dry runner,
transaction 176/176, default gate 16/16, synchronous WDM-v3/PnP 164/164,
lecdiag chain 19/19, and expanded DMA/INTEN stress 50,000/50,000 plus
10,000/10,000.

## First active experiment gate

Build and sign a separately identifiable Release x64 package with
`/p:LecWdmV3MamDma=true`, preserving the exact reviewed source identity. The
proposed package identity is `DriverVer=10/28/2026,0.2026.1028.1`; it is not
yet applied or approved. After separate signing and installation authorization,
the proposed exact command is:

```powershell
lecdiag.exe mam-dma-once I-AUTHORIZE-ONE-MAM-GO
```

It performs one `CFDC2124` registration for 16 data bytes, one `CFDC2138` with
channel `0x30` and config `0x00000C00`, no retry, and `CFDC2128` only after an
exact successful four-byte completion. On any failure it retains the buffer and
registration for driver quarantine/recovery review.

Before that run, physically identify and disconnect the productive x86 disk,
verify the x64 test disk identity, retain the known-good signed package and
crash-dump setup, close XStream, and capture kernel/driver evidence. Stop on a
missing IRQ, timeout, unexpected descriptor count, signature/PnP error or
device-health change.
