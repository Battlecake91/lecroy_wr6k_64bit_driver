# Device interfaces

The legacy x86 driver registers **four device interfaces** during construction of the LeCroy device object.

This was reconstructed from four consecutive calls into the DriverWorks helper that ultimately invokes `IoRegisterDeviceInterface`.

## Registered GUIDs

| # | GUID | Constructor call site | Confidence |
|---:|---|---:|---|
| 1 | `{7AC34BE9-F766-4F15-9E88-854BA5E2146E}` | `0x10BE4` | Confirmed |
| 2 | `{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}` | `0x10BF1` | Confirmed |
| 3 | `{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}` | `0x10BFE` | Confirmed |
| 4 | `{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}` | `0x10C0B` | Confirmed |

The GUID objects reside consecutively in the original image at:

```text
0x1CC98
0x1CCA8
0x1CCB8
0x1CCC8
```

The DriverWorks interface-registration helper is at approximately `0x19EE6` in the loaded image and eventually calls the imported `IoRegisterDeviceInterface` routine.

## Implication for the x64 replacement

The replacement driver should initially register the same four interface classes unless caller analysis proves that only a subset is required.

These GUIDs are more important than the eventual `\\.\...` convenience name because modern PnP-aware user-mode code can discover devices through interface classes rather than a fixed DOS symbolic link.

## Still unknown

The following items are not yet proven from the driver binary alone:

- which of the four GUIDs is used by each LeCroy user-mode component;
- whether all four interfaces are enabled simultaneously;
- the final DOS symbolic-link/device name used by any legacy non-PnP caller;
- the PCI hardware IDs used by the INF.

A string search of the driver binary does not expose `PCI\\VEN_...` or `PCI\\DEV_...` identifiers, which is normal: those usually live in the INF rather than in the SYS binary.

The fastest way to close those gaps is to inspect the original INF and/or the installed device instance on the scope.
