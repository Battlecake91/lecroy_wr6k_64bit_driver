# Device interfaces

The WR6k PCI stack intentionally publishes **five device-interface classes**.
Four were recovered from consecutive DriverWorks interface registrations in the
original x86 kernel driver. The fifth, `958695A4-...`, was recovered from a
passive `NtCreateFile` trace of original XStream and is required for original
endpoint-selection compatibility.

## Expected interface set

| GUID | Recovered role | Evidence |
|---|---|---|
| `{958695A4-693A-435E-8297-66F805D8E46A}` | PCI discovery and main acquisition/control endpoint after the legacy DOS-name probe fails | Original-XStream `NtCreateFile`/IOCTL trace |
| `{7AC34BE9-F766-4F15-9E88-854BA5E2146E}` | Delay/flag helper (`0x00222C00`, `0x00222C04`) | Original DriverWorks registration and runtime trace |
| `{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}` | Dallas/board identification (`0x00222400`, `0x00223080`, `0x00223084`) | Original DriverWorks registration and runtime trace |
| `{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}` | Trace control (`0x00223000`, `0x00223004`) | Original DriverWorks registration and runtime trace |
| `{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}` | Three-event registration (`0x00223100`) | Original DriverWorks registration and runtime trace |

The four GUID objects visible together in the original x86 kernel image are at
`0x1CC98`, `0x1CCA8`, `0x1CCB8` and `0x1CCC8`. Their constructor call sites are
`0x10BE4`, `0x10BF1`, `0x10BFE` and `0x10C0B`. The corresponding DriverWorks
helper eventually calls `IoRegisterDeviceInterface`.

The fifth class is not evidence of an accidental duplicate. Original XStream
first probes `\\.\ALADDINAcqDriver0`; the reference system returns
`STATUS_OBJECT_NAME_NOT_FOUND`, after which XStream opens the
`958695A4-...` interface for the main acquisition/control stream. Publishing
the DOS alias on the replacement changed that selection path, so the
replacement deliberately omits the alias and retains all five PnP interfaces.

## Replacement-driver lifecycle

`Driver.c` declares exactly these five GUIDs and calls
`IoRegisterDeviceInterface` once for each class during `AddDevice`. Registration
allocates the symbolic-link strings but does not make the interfaces available.

After the lower stack has accepted `IRP_MN_START_DEVICE`, BAR setup has
succeeded and the interrupt is connected, `Device.c` enables each successfully
registered interface with `IoSetDeviceInterfaceState(..., TRUE)`. STOP,
SURPRISE_REMOVAL and REMOVE close IOCTL admission and disable every enabled
interface before resource teardown. REMOVE frees each registered interface
string after remove-lock rundown.

Enable failures currently use a continue-on-error policy: successful classes
remain published and START may still succeed. Offline fault models cover first,
middle, last and multiple enable failures and confirm that STOP/REMOVE disables
only the classes actually enabled. Original-driver evidence proves the four
kernel registrations and the fifth XStream endpoint requirement, but does not
prove legacy behavior when `IoSetDeviceInterfaceState` fails. Failing START or
rolling back successful classes is therefore a compatibility-affecting change,
not an established fix. The current behavior remains unchanged pending direct
fault injection and an explicit policy decision.

The INF binds the PCI hardware ID, class and service. It has no `AddInterface`
directive and does not create any of the five interface classes. Their
publication is exclusively the driver's runtime responsibility.

## Qualification contract

A healthy started device is expected to expose exactly one present interface
instance in each of the five classes above. The previous qualification wording
that expected one interface total was incorrect. Qualification must reject a
missing class, an unexpected sixth class, duplicate present instances for the
same PCI function, or an interface that remains present after STOP/REMOVE.

The hardware-independent source contract fixes the exact five-GUID set, count
and driver-owned registration loop. Runtime qualification is still required to
prove instance cardinality and publication/unpublication ordering under the
independently authorized Phase 2C runtime gates.
