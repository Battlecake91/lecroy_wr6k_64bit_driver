# Captured reference-system state

This document records the state of the working 32-bit LeCroy installation captured before the x64 migration.

## Operating system

```text
Computer name:  LECROY-W10
OS:             Windows 10 Pro
Version:        10.0.19045
Architecture:   32-bit
System type:    x86-based PC
CPU family:     x64-capable Intel Family 6 Model 58
LeCroy model:   WR6200A
```

The operating-system description reports:

```text
LeCroy DSO WR6200A
```

This confirms that the working reference environment is a 32-bit Windows 10 installation on x64-capable hardware.

## Acquisition PCI function

```text
Name:        LeCroy Acquisition Device (S65)
PCI ID:      PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00
Class:       DataAcquisition
Class GUID:  {BA5FE95F-EE73-4113-8121-F38CC4FF0095}
Service:     LecS65AcqDrv
Location:    PCI bus 4, device 1, function 0
PDO:         \Device\NTPNP_PCI0016
Status:      OK
```

The reported installed driver is:

```text
Provider:    LeCroy
Version:     6.1.1.1002
Date:        2008-12-23
INF:         oem18.inf
Signed:      no
```

## Kernel service state

The working system reports:

```text
Name:         LecS65AcqDrv
Display name: LecS65Driver
State:        Running
Start mode:   Manual
Type:         Kernel Driver
Path:         C:\Windows\system32\Drivers\LecS65AcqDrv.sys
Tag ID:       20
ErrorControl: Normal
```

Registry enumeration binds exactly one PCI instance to the service.

## DataAcquisition class registry

The registered class contains:

```text
Class:       DataAcquisition
ClassDesc:   Data Acquisition Devices
Installer32: AcqDevClassInstaller.dll,ClassInstall
Icon:        101
```

The class installer is present at:

```text
C:\Windows\System32\AcqDevClassInstaller.dll
```

Captured file metadata:

```text
Size:       57,344 bytes
Timestamp:  2007-05-03 18:01:58
```

Two class-instance keys (`0000` and `0001`) reference the S65 legacy package, both matching the same S65 hardware ID.

## Migration relevance

This capture gives us a known-good baseline for the x64 replacement:

1. the PCI function and driver service are unambiguous;
2. the legacy driver is demand-start and currently running;
3. the custom class installer is a 32-bit-era component and should not be assumed necessary for the x64 package;
4. the original application already runs on Windows 10, so the first x64 experiment should keep the existing 32-bit LeCroy user-mode stack and replace only the kernel driver.
