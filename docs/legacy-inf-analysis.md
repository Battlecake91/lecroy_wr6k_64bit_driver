# Legacy INF analysis

Source: installed Windows INF exported/provided as `oem18.inf`.

The header comment identifies the original vendor filename as:

```text
LecS65AcqDrv.inf
```

## Version / device class

```ini
[Version]
Signature="$Chicago$"
Class = DataAcquisition
ClassGuid = {BA5FE95F-EE73-4113-8121-F38CC4FF0095}
Provider=%LCRY%
DriverVer=12/23/2008, 6.1.1.1002
```

Confirmed values:

| Property | Value |
|---|---|
| Original INF filename | `LecS65AcqDrv.inf` |
| Provider | `LeCroy` |
| Device class | `DataAcquisition` |
| Class GUID | `{BA5FE95F-EE73-4113-8121-F38CC4FF0095}` |
| Driver date | 2008-12-23 |
| Driver version | `6.1.1.1002` |
| Friendly device name | `LeCroy Acquisition Device (S65)` |
| Class friendly name | `Data Acquisition Devices` |

## Exact PCI match

The legacy INF binds the device using the fully-qualified hardware ID:

```ini
[LecAladdinAcqDeviceList]
%LecS65AcqDrv%=LecS65AcqDrv.Install, PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00
```

This confirms the Device Manager observation and gives us the exact original match expression.

For a compatibility-oriented x64 replacement we can initially use the same hardware ID. A less restrictive match such as `PCI\VEN_1570&DEV_0005` may be useful during development, but should only be widened deliberately.

## Kernel service

The legacy INF installs:

```ini
[LecS65AcqDrv.Install.NT.Services]
AddService =LecS65AcqDrv, 0x00000002,LecAladdinAcqDev.Service
```

Service configuration:

```ini
[LecAladdinAcqDev.Service]
DisplayName    = %LecS65AcqDrv.ServiceName%
ServiceType    = 1
StartType      = 3
ErrorControl   = 1
ServiceBinary  = %10%\System32\Drivers\LecS65AcqDrv.sys
LoadOrderGroup = Base
```

Resolved values:

| Property | Value |
|---|---|
| Service name | `LecS65AcqDrv` |
| Display name | `LecS65Driver` |
| Service type | kernel driver |
| Start type | demand start |
| Error control | normal |
| Binary | `%SystemRoot%\System32\Drivers\LecS65AcqDrv.sys` |
| Load order group | `Base` |
| Associated service flag | set |

This is important for drop-in compatibility: the future x64 package should preserve the service name `LecS65AcqDrv` unless testing proves it unnecessary.

## File destination

The package declares:

```ini
[DestinationDirs]
LecS65AcqDrv.Files.Driver.NT=10,System32\Drivers

[LecS65AcqDrv.Files.Driver.NT]
LecS65AcqDrv.sys
```

However, in the supplied INF the install section has the `CopyFiles` directive commented out:

```ini
[LecS65AcqDrv.Install.NT]
;CopyFiles=LecS65AcqDrv.Files.Driver.NT
AddReg=LecAladdinAcqDevice.AddReg;
```

That means this INF as installed does not itself request the SYS copy through that section. The original LeCroy installer may have placed the binary separately before PnP installation, or the INF may have been generated/modified as part of installation.

The x64 replacement INF should not copy this quirk blindly. It should use a normal modern Driver Store installation model.

## Legacy class installer

The INF defines a custom device class installer:

```ini
[ClassInstall32]
AddReg=DataAcquisition

[DataAcquisition]
HKR,,,,%DevClassName%
HKR,,Installer32,,"AcqDevClassInstaller.dll,ClassInstall"
HKR,,Icon,,101
```

So the old package expected:

```text
AcqDevClassInstaller.dll
```

with exported entry point:

```text
ClassInstall
```

There is also a commented-out property-page provider:

```ini
;HKR,,EnumPropPages32,,"AcqDevClassInstaller.dll,AcqDevPropPageProvider"
```

For the x64 reconstruction, this custom class installer should be treated separately from the acquisition kernel ABI. Modern Windows driver packages generally do not need to reproduce a 32-bit class-installer DLL just to bind the PCI function.

If the LeCroy application itself depends on class-registry state created by that installer, those registry effects will need to be identified and reproduced explicitly.

## Odd / legacy artifacts

The final strings section contains a malformed-looking line:

```ini
SERVICE_SYSTEM_START = 1ART = 1ART = 1
```

It appears unused by the active install sections and should not be copied into the replacement INF.

There is also an unused `[SuballocService]` section.

These are good examples of why the replacement package should preserve observable behaviour rather than mechanically clone every line of the 2008 installer.

## x64 replacement implications

The new INF can now preserve the known external identity:

```text
Device name:      LeCroy Acquisition Device (S65)
PCI hardware ID:  PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00
Class:            DataAcquisition
Class GUID:       {BA5FE95F-EE73-4113-8121-F38CC4FF0095}
Service name:     LecS65AcqDrv
Display name:     LecS65Driver
Binary name:      LecS65AcqDrv.sys
```

The four device-interface GUIDs registered at runtime are documented separately in [device-interfaces.md](device-interfaces.md).
