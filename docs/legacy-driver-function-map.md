# Legacy driver function map

This is the semantic coverage map for every function currently recognized by Ghidra in the original 32-bit `LecS65AcqDrv.sys`.

- Complete pseudocode snapshot: `4e622dc2528127cce13ae35146554a9b2ada2982`
- Architecture overview: [legacy-driver-architecture.md](legacy-driver-architecture.md)
- `High` means the role is directly established by API calls, register effects, strings, known ABI routing or reconstructed call flow.
- `Pending` means pseudocode is available but an exact semantic name has not yet been justified. These entries are not being discarded.
- `ASM audit` means the semantic role is substantially known but Ghidra emitted a control-flow warning that must be checked against raw x86 instructions. The 2026-10-04 ASM pass resolved both such warnings.
- The 420 entries reflect the functions recognized by Ghidra in the inventory, **not** all executable bytes or every short virtual thunk in the PE32 driver. Audit: [vtables and raw assembly](legacy-driver-vtables-and-asm-audit.md).

| Address | Ghidra name | Subsystem | Semantic role | Status |
|---|---|---|---|---|
| `0x10380` | `FUN_00010380` | device/IRP low-level | pool allocation wrapper (NonPagedPool-style) | High |
| `0x1039A` | `FUN_0001039a` | device/IRP low-level | tagged pool allocation wrapper | High |
| `0x103B6` | `FUN_000103b6` | device/IRP low-level | null-safe pool free wrapper | High |
| `0x103C8` | `FUN_000103c8` | device/IRP low-level | stored kernel-handle close/reset | High |
| `0x103DC` | `FUN_000103dc` | device/IRP low-level | global DriverWorks driver/support destructor; releases copied registry/global buffers | High |
| `0x104A4` | `FUN_000104a4` | device/IRP low-level | PoSetPowerState wrapper and cached device-power update | High |
| `0x104F4` | `FUN_000104f4` | device/IRP low-level | CLecS65AcqDrvDevice factory / AddDevice path | High |
| `0x10582` | `FUN_00010582` | device/IRP low-level | owned object destruction + pool free | High |
| `0x10598` | `FUN_00010598` | device/IRP low-level | owned pointer/buffer release helper | High |
| `0x105B4` | `FUN_000105b4` | device/IRP low-level | UNICODE_STRING initializer wrapper | High |
| `0x105CC` | `FUN_000105cc` | device/IRP low-level | owned Unicode/pool buffer release | High |
| `0x105F2` | `FUN_000105f2` | device/IRP low-level | referenced-object cleanup; ObDereferenceObject and clear stored pointer | High |
| `0x1060C` | `FUN_0001060c` | device/IRP low-level | interrupt setup orchestrator; prepare resource state then IoConnectInterrupt | High |
| `0x10636` | `FUN_00010636` | device/IRP low-level | KeSynchronizeExecution wrapper | High |
| `0x10656` | `FUN_00010656` | device/IRP low-level | mapped-resource range/base metadata getter | High |
| `0x1067A` | `FUN_0001067a` | device/IRP low-level | IoStartPacket queue-start wrapper | High |
| `0x106A0` | `FUN_000106a0` | device/IRP low-level | mapped-I/O resource unmap helper | High |
| `0x106CC` | `FUN_000106cc` | device/IRP low-level | MMIO resource mapping wrapper around framework MmMapIoSpace helper | High |
| `0x106E6` | `FUN_000106e6` | device/IRP low-level | translated resource select/map helper | High |
| `0x10734` | `FUN_00010734` | device/IRP low-level | generic pool-owned deleting destructor | High |
| `0x10750` | `FUN_00010750` | device/IRP low-level | trace string/value formatter wrapper | High |
| `0x1076E` | `FUN_0001076e` | device/IRP low-level | trace numeric formatter wrapper | High |
| `0x10798` | `FUN_00010798` | device/IRP low-level | complete IRP with status | High |
| `0x107C2` | `FUN_000107c2` | device/IRP low-level | PoCallDriver forwarding helper | High |
| `0x107E0` | `FUN_000107e0` | device/IRP low-level | forward IRP to lower driver after outstanding-I/O release | High |
| `0x107FE` | `FUN_000107fe` | device/IRP low-level | MMIO write + descriptor shadow update | High |
| `0x10816` | `FUN_00010816` | device/IRP low-level | signal enabled event object | High |
| `0x1082E` | `FUN_0001082e` | device/IRP low-level | interrupt/resource cleanup helper | High |
| `0x108D6` | `FUN_000108d6` | device/IRP low-level | LeCroy ISR | High |
| `0x10A88` | `FUN_00010a88` | device/IRP low-level | global synchronization/callback object accessor | High |
| `0x10A8E` | `FUN_00010a8e` | device/IRP low-level | per-process linked reference record add/ref | High |
| `0x10B10` | `FUN_00010b10` | device/IRP low-level | replace owned helper object | High |
| `0x10B3C` | `FUN_00010b3c` | device/IRP low-level | CLecS65AcqDrvDevice constructor | High |
| `0x10C98` | `FUN_00010c98` | device/IRP low-level | CLecS65AcqDrvDevice destructor | High |
| `0x10D46` | `FUN_00010d46` | device/IRP low-level | hardware stop/interrupt cleanup | High |
| `0x10D9A` | `FUN_00010d9a` | device/IRP low-level | success/no-op helper | High |
| `0x10DA0` | `FUN_00010da0` | device/IRP low-level | IRP cancel routine / device-queue removal | High |
| `0x10E3A` | `FUN_00010e3a` | device/IRP low-level | create/open dispatch bookkeeping | High |
| `0x10F30` | `FUN_00010f30` | device/IRP low-level | close dispatch / per-process transfer cleanup | High |
| `0x11390` | `FUN_00011390` | IRQ/events/legacy IOCTL | LeCroy DPC dispatcher | High |
| `0x11532` | `FUN_00011532` | IRQ/events/legacy IOCTL | CLecS65AcqDrvDevice deleting destructor | High |
| `0x115C4` | `FUN_000115c4` | IRQ/events/legacy IOCTL | StartDevice resource mapping + board initialization + IRQ connect | High |
| `0x11894` | `FUN_00011894` | IRQ/events/legacy IOCTL | ASM-verified guarded IRP/cancel dispatch: major 0x0E sets STATUS_INVALID_DEVICE_REQUEST then tail-jumps to hardware virtual +0x20; other majors CALL same virtual slot | High |
| `0x11914` | `FUN_00011914` | IRQ/events/legacy IOCTL | owned buffer/object release | High |
| `0x11946` | `FUN_00011946` | IRQ/events/legacy IOCTL | small owned-buffer object constructor | High |
| `0x11962` | `FUN_00011962` | IRQ/events/legacy IOCTL | register descriptor constructor | High |
| `0x119A0` | `FUN_000119a0` | IRQ/events/legacy IOCTL | register descriptor cleanup | High |
| `0x119BC` | `FUN_000119bc` | IRQ/events/legacy IOCTL | register descriptor metadata copy | High |
| `0x11A00` | `FUN_00011a00` | IRQ/events/legacy IOCTL | 256-slot indexed-register shadow/cache object constructor | High |
| `0x11A30` | `FUN_00011a30` | IRQ/events/legacy IOCTL | flush dirty 256-slot indexed-register shadow entries to bound MMIO register | High |
| `0x11A88` | `FUN_00011a88` | IRQ/events/legacy IOCTL | bind shared trace/control owner pointer into hardware helper fields | High |
| `0x11AA6` | `FUN_00011aa6` | IRQ/events/legacy IOCTL | bind CFDC2110 divider/counter register pointers | High |
| `0x11AD2` | `FUN_00011ad2` | IRQ/events/legacy IOCTL | bind CFDC2110 parent/trace state | High |
| `0x11AF2` | `FUN_00011af2` | IRQ/events/legacy IOCTL | release all captured event-object references and reset holder | High |
| `0x11B18` | `FUN_00011b18` | IRQ/events/legacy IOCTL | capture/reference user event handle | High |
| `0x11B48` | `FUN_00011b48` | IRQ/events/legacy IOCTL | release captured event references | High |
| `0x11B70` | `FUN_00011b70` | IRQ/events/legacy IOCTL | event wait/signaling helper | High |
| `0x11BDC` | `FUN_00011bdc` | IRQ/events/legacy IOCTL | CFDC2124 transfer-registration IOCTL | High |
| `0x11C36` | `FUN_00011c36` | IRQ/events/legacy IOCTL | CFDC2128 transfer-unregister IOCTL | High |
| `0x11C5E` | `FUN_00011c5e` | IRQ/events/legacy IOCTL | CFDC212C STATUS_NOT_IMPLEMENTED handler | High |
| `0x11CFF` | `FUN_00011cff` | IRQ/events/legacy IOCTL | CFDC2130 serial FPGA/GPIODAT programming IOCTL | High |
| `0x11DC2` | `FUN_00011dc2` | IRQ/events/legacy IOCTL | consume software pending bit 0 | High |
| `0x11DD8` | `FUN_00011dd8` | IRQ/events/legacy IOCTL | consume software pending bit 1 | High |
| `0x11DEE` | `FUN_00011dee` | IRQ/events/legacy IOCTL | consume software pending bit 2 | High |
| `0x11E04` | `FUN_00011e04` | IRQ/events/legacy IOCTL | consume software pending bit 4 | High |
| `0x11E1A` | `FUN_00011e1a` | IRQ/events/legacy IOCTL | consume/clear software pending bitmap bit 0x20 | High |
| `0x11E30` | `FUN_00011e30` | IRQ/events/legacy IOCTL | consume/clear software pending bitmap bit 0x08 | High |
| `0x11E46` | `FUN_00011e46` | IRQ/events/legacy IOCTL | publish software interrupt-enable mask to INTEN | High |
| `0x11E72` | ``vector_constructor_iterator'` | IRQ/events/legacy IOCTL | compiler vector-constructor iterator helper | High |
| `0x11F54` | `FUN_00011f54` | IRQ/events/legacy IOCTL | Dallas memory-write IOCTL with full readback verification | High |
| `0x120DC` | `FUN_000120dc` | IOCTL/register/DMA/acquisition | clear BAR1 GPIODAT bit 16 | High |
| `0x120FA` | `FUN_000120fa` | IOCTL/register/DMA/acquisition | allocate dynamic array of 0x10A-byte records | High |
| `0x12166` | `FUN_00012166` | IOCTL/register/DMA/acquisition | dynamic-array free/reset | High |
| `0x12184` | `FUN_00012184` | IOCTL/register/DMA/acquisition | grow DWORD/pointer dynamic array | High |
| `0x121FA` | `FUN_000121fa` | IOCTL/register/DMA/acquisition | grow 0x10A-byte record array | High |
| `0x12290` | `FUN_00012290` | IOCTL/register/DMA/acquisition | set trace level in trace metadata/list | High |
| `0x122E2` | `FUN_000122e2` | IOCTL/register/DMA/acquisition | per-process reference record deref/remove | High |
| `0x1232A` | `FUN_0001232a` | IOCTL/register/DMA/acquisition | OR flags into per-process reference record | High |
| `0x1234C` | `FUN_0001234c` | IOCTL/register/DMA/acquisition | free/reset owned trace/register metadata buffer | High |
| `0x1236E` | `FUN_0001236e` | IOCTL/register/DMA/acquisition | commit cached register value when descriptor enabled | High |
| `0x12386` | `FUN_00012386` | IOCTL/register/DMA/acquisition | compute required byte size of register list: (maxIndex+1)*0x10A | High |
| `0x123B4` | `FUN_000123b4` | IOCTL/register/DMA/acquisition | refresh and serialize complete register list | High |
| `0x124C2` | `FUN_000124c2` | IOCTL/register/DMA/acquisition | refresh and serialize one register-list entry | High |
| `0x1259A` | `FUN_0001259a` | IOCTL/register/DMA/acquisition | SetOneRegister indexed physical write | High |
| `0x1260E` | `FUN_0001260e` | IOCTL/register/DMA/acquisition | board interrupt/error service helper | High |
| `0x126EE` | `FUN_000126ee` | IOCTL/register/DMA/acquisition | board interrupt/error reset/ack helper | High |
| `0x1272A` | `FUN_0001272a` | IOCTL/register/DMA/acquisition | CFDC21C4 generic register-write IOCTL | High |
| `0x12832` | `FUN_00012832` | IOCTL/register/DMA/acquisition | CFDC21C8 driver-build query (1002) | High |
| `0x1286E` | `FUN_0001286e` | IOCTL/register/DMA/acquisition | legacy millisecond delay/control IOCTL | High |
| `0x128BC` | `FUN_000128bc` | IOCTL/register/DMA/acquisition | legacy flag-byte/control IOCTL | High |
| `0x128F8` | `FUN_000128f8` | IOCTL/register/DMA/acquisition | CFDC2180 event-registration/control IOCTL | High |
| `0x12988` | `FUN_00012988` | IOCTL/register/DMA/acquisition | 0x00223100 three-event registration IOCTL | High |
| `0x12A5E` | `FUN_00012a5e` | IOCTL/register/DMA/acquisition | variable saved-buffer query IOCTL | High |
| `0x12ADA` | `FUN_00012ada` | IOCTL/register/DMA/acquisition | trace-control record update IOCTL | High |
| `0x12B34` | `FUN_00012b34` | IOCTL/register/DMA/acquisition | CFDC218C event-registration/control IOCTL | High |
| `0x12BAE` | `FUN_00012bae` | IOCTL/register/DMA/acquisition | CFDC2194 29-byte status/error-latch query | High |
| `0x12C18` | `FUN_00012c18` | IOCTL/register/DMA/acquisition | register-list query IOCTL | High |
| `0x12CAC` | `FUN_00012cac` | IOCTL/register/DMA/acquisition | 0x0022303C SetOneRegister outer IOCTL | High |
| `0x12D24` | `FUN_00012d24` | IOCTL/register/DMA/acquisition | START/FVER direct register-read IOCTL | High |
| `0x12D6A` | `FUN_00012d6a` | IOCTL/register/DMA/acquisition | common acquisition orchestrator | High |
| `0x12E18` | `FUN_00012e18` | IOCTL/register/DMA/acquisition | remove/free all registered transfer objects owned by a process | High |
| `0x12E72` | `FUN_00012e72` | IOCTL/register/DMA/acquisition | release captured event references associated with a process | High |
| `0x12EAE` | `FUN_00012eae` | IOCTL/register/DMA/acquisition | interrupt-synchronized board callback | High |
| `0x12EDE` | `FUN_00012ede` | IOCTL/register/DMA/acquisition | CFDC2400 software-pending injection/dispatch IOCTL | High |
| `0x12F30` | `FUN_00012f30` | IOCTL/register/DMA/acquisition | MAM configuration-record programmer | High |
| `0x12FDE` | `FUN_00012fde` | IOCTL/register/DMA/acquisition | board startup ITMODE/initialization sequence | High |
| `0x130EA` | `FUN_000130ea` | IOCTL/register/DMA/acquisition | Dallas ROM-ID IOCTL | High |
| `0x131B5` | `FUN_000131b5` | IOCTL/register/DMA/acquisition | Dallas memory-read IOCTL | High |
| `0x13230` | `FUN_00013230` | IOCTL/register/DMA/acquisition | append/store one 0x10A-byte metadata record in dynamic record array | High |
| `0x1326E` | `FUN_0001326e` | IOCTL/register/DMA/acquisition | append/store one pointer/DWORD in dynamic array | High |
| `0x1329E` | `FUN_0001329e` | IOCTL/register/DMA/acquisition | CKeTraceControl constructor | High |
| `0x133C4` | `FUN_000133c4` | IOCTL/register/DMA/acquisition | CKeTraceControl destructor | High |
| `0x1340C` | `FUN_0001340c` | IOCTL/register/DMA/acquisition | SPI helper register binding | High |
| `0x13434` | `FUN_00013434` | IOCTL/register/DMA/acquisition | register-list/metadata object construction | High |
| `0x134AE` | `FUN_000134ae` | IOCTL/register/DMA/acquisition | resource/register metadata cleanup | High |
| `0x134F6` | `FUN_000134f6` | IOCTL/register/DMA/acquisition | LeCroy hardware-subobject destructor | High |
| `0x13768` | `FUN_00013768` | IOCTL/register/DMA/acquisition | per-process transfer cleanup on close | High |
| `0x137C4` | `FUN_000137c4` | IOCTL/register/DMA/acquisition | global transfer/resource cleanup on stop/remove | High |
| `0x1381E` | `FUN_0001381e` | IOCTL/register/DMA/acquisition | hardware resume/start sequence; reinitialize board controls and restore interrupt state | High |
| `0x138B2` | `FUN_000138b2` | IOCTL/register/DMA/acquisition | resume/start wrapper that optionally signals event then runs hardware start sequence | High |
| `0x138D4` | `FUN_000138d4` | IOCTL/register/DMA/acquisition | hardware quiesce/stop sequence; save and disable interrupt state | High |
| `0x13954` | `FUN_00013954` | IOCTL/register/DMA/acquisition | CFDC21C0 generic register-read IOCTL | High |
| `0x13A2E` | `FUN_00013a2e` | IOCTL/register/DMA/acquisition | CFDC2400 outer handler | High |
| `0x13A40` | `FUN_00013a40` | IOCTL/register/DMA/acquisition | CFDC2190 29-byte status/control IOCTL | High |
| `0x13AE2` | `FUN_00013ae2` | IOCTL/register/DMA/acquisition | CFDC2110 packed batch dispatcher | High |
| `0x13C84` | `FUN_00013c84` | IOCTL/register/DMA/acquisition | CFDC2138 METHOD_BUFFERED acquisition parser/frontend | High |
| `0x13DC6` | `FUN_00013dc6` | IOCTL/register/DMA/acquisition | CFDD219F METHOD_NEITHER acquisition parser/frontend | High |
| `0x13F3C` | `FUN_00013f3c` | IOCTL/register/DMA/acquisition | append item to dynamic pointer/index list | High |
| `0x13F70` | `FUN_00013f70` | IOCTL/register/DMA/acquisition | append 0x10A-byte metadata record | High |
| `0x13FA6` | `FUN_00013fa6` | IOCTL/register/DMA/acquisition | insert named register descriptor into public register list | High |
| `0x14122` | `FUN_00014122` | IOCTL/register/DMA/acquisition | LeCroy hardware-subobject deleting destructor | High |
| `0x14142` | `FUN_00014142` | IOCTL/register/DMA/acquisition | legacy event/pending-state initialization | High |
| `0x141DC` | `FUN_000141dc` | IOCTL/register/DMA/acquisition | CFDC2138 IOCTL wrapper | High |
| `0x141F8` | `FUN_000141f8` | IOCTL/register/DMA/acquisition | CFDD219F IOCTL wrapper | High |
| `0x14212` | `FUN_00014212` | board construction/register binding | LeCroy hardware-subobject constructor | High |
| `0x14847` | `FUN_00014847` | board construction/register binding | bind PCI resources and construct named register model | High |
| `0x1530E` | `FUN_0001530e` | CFDC2110/JTAG/SPI/control | initialize CFDC packed-record/parser state object | High |
| `0x15348` | `FUN_00015348` | CFDC2110/JTAG/SPI/control | bind owner/trace pointer into packed-record parser state | High |
| `0x15356` | `FUN_00015356` | CFDC2110/JTAG/SPI/control | initialize/reset CFDC response-cursor state | High |
| `0x15364` | `FUN_00015364` | CFDC2110/JTAG/SPI/control | advance to next packed request record by header+payload length | High |
| `0x15380` | `FUN_00015380` | CFDC2110/JTAG/SPI/control | return current packed-record/output cursor pointer | High |
| `0x15384` | `FUN_00015384` | CFDC2110/JTAG/SPI/control | return accumulated packed payload/output byte count | High |
| `0x15388` | `FUN_00015388` | CFDC2110/JTAG/SPI/control | return packed-record count | High |
| `0x1538C` | `FUN_0001538c` | CFDC2110/JTAG/SPI/control | reset packed request parser state | High |
| `0x153A2` | `FUN_000153a2` | CFDC2110/JTAG/SPI/control | parse packed CFDC2110 batch-record stream | High |
| `0x15402` | `FUN_00015402` | CFDC2110/JTAG/SPI/control | free/reset dynamic response/output buffer | High |
| `0x15422` | `FUN_00015422` | CFDC2110/JTAG/SPI/control | response/output staging-buffer resize | High |
| `0x15484` | `FUN_00015484` | CFDC2110/JTAG/SPI/control | advance response/output cursor by byte count | High |
| `0x1549E` | `FUN_0001549e` | CFDC2110/JTAG/SPI/control | rewind response/output cursor to buffer start | High |
| `0x154A6` | `FUN_000154a6` | CFDC2110/JTAG/SPI/control | test whether response/output cursor is valid/nonzero | High |
| `0x154B0` | `FUN_000154b0` | CFDC2110/JTAG/SPI/control | construct packed request-parser state from input buffer | High |
| `0x154DC` | `FUN_000154dc` | CFDC2110/JTAG/SPI/control | construct response/output-buffer state | High |
| `0x15500` | `FUN_00015500` | CFDC2110/JTAG/SPI/control | gated register-pulse/delay helper constructor | High |
| `0x1550E` | `FUN_0001550e` | CFDC2110/JTAG/SPI/control | gated register-pulse/delay helper reset/destructor shell | High |
| `0x1551A` | `FUN_0001551a` | CFDC2110/JTAG/SPI/control | bind and enable register descriptor for gated pulse/delay helper | High |
| `0x1552C` | `FUN_0001552c` | CFDC2110/JTAG/SPI/control | set gated pulse/delay helper enable flag | High |
| `0x15536` | `FUN_00015536` | CFDC2110/JTAG/SPI/control | write boolean 0/1 to bound register descriptor and shadow | High |
| `0x1555A` | `FUN_0001555a` | CFDC2110/JTAG/SPI/control | gated register-pulse/delay helper deleting destructor | High |
| `0x1557C` | `FUN_0001557c` | CFDC2110/JTAG/SPI/control | gated millisecond delay / control pulse helper | High |
| `0x155D0` | `FUN_000155d0` | CFDC2110/JTAG/SPI/control | encode selected control bit/field into cached register-control word | High |
| `0x156D2` | `FUN_000156d2` | CFDC2110/JTAG/SPI/control | destroy six CFDC transport register descriptors | High |
| `0x15710` | `FUN_00015710` | CFDC2110/JTAG/SPI/control | classify packed batch record as CFDC type 3 | High |
| `0x15720` | `FUN_00015720` | CFDC2110/JTAG/SPI/control | classify packed batch record as MAM/config type 1 or 2 | High |
| `0x1573E` | `FUN_0001573e` | CFDC2110/JTAG/SPI/control | toggle software interrupt-enable state bit 0x10 and synchronize hardware mask | High |
| `0x15772` | `FUN_00015772` | CFDC2110/JTAG/SPI/control | toggle software interrupt-enable state bit 0x20 and synchronize hardware mask | High |
| `0x157A6` | `FUN_000157a6` | CFDC2110/JTAG/SPI/control | OR enabled asynchronous command bits into sticky CFDC pending mask | High |
| `0x157B8` | `FUN_000157b8` | CFDC2110/JTAG/SPI/control | return enabled-and-pending CFDC asynchronous command mask | High |
| `0x157C4` | `FUN_000157c4` | CFDC2110/JTAG/SPI/control | kernel timer/event helper constructor | High |
| `0x157EC` | `FUN_000157ec` | CFDC2110/JTAG/SPI/control | kernel timer/event helper destructor; cancel timer and release event state | High |
| `0x15802` | `FUN_00015802` | CFDC2110/JTAG/SPI/control | JTAGNUM programming helper | High |
| `0x15848` | `FUN_00015848` | CFDC2110/JTAG/SPI/control | JTAGDAT programming helper | High |
| `0x1586E` | `FUN_0001586e` | CFDC2110/JTAG/SPI/control | JTAGDIN read helper | High |
| `0x1588E` | `FUN_0001588e` | CFDC2110/JTAG/SPI/control | reverse 32-bit bit order and write result to bound register | High |
| `0x158C0` | `FUN_000158c0` | CFDC2110/JTAG/SPI/control | pulse one encoded control selector low/high through cached register helper | High |
| `0x158EE` | `FUN_000158ee` | CFDC2110/JTAG/SPI/control | CFDC2110 object constructor | High |
| `0x159BC` | `FUN_000159bc` | CFDC2110/JTAG/SPI/control | bind CFDC parent/context pointers and initialize packet-parser state | High |
| `0x159E2` | `FUN_000159e2` | CFDC2110/JTAG/SPI/control | bind CFDC2110 register regions + transport | High |
| `0x15A08` | `FUN_00015a08` | CFDC2110/JTAG/SPI/control | build common CFDC2110 status/result header | High |
| `0x15A26` | `FUN_00015a26` | CFDC2110/JTAG/SPI/control | CFDC2110 response-buffer allocation/copy | High |
| `0x15A88` | `FUN_00015a88` | CFDC2110/JTAG/SPI/control | stage common 8-byte CFDC status response | High |
| `0x15ACC` | `FUN_00015acc` | CFDC2110/JTAG/SPI/control | family-0 JTAG command | High |
| `0x15B64` | `FUN_00015b64` | CFDC2110/JTAG/SPI/control | family-1 divider/counter read | High |
| `0x15BCE` | `FUN_00015bce` | CFDC2110/JTAG/SPI/control | read ACQFVER and stage 12-byte CFDC response | High |
| `0x15C26` | `FUN_00015c26` | CFDC2110/JTAG/SPI/control | read START/base register and stage 12-byte CFDC response | High |
| `0x15C7E` | `FUN_00015c7e` | CFDC2110/JTAG/SPI/control | family-1 JTAG command with readback | High |
| `0x15DB8` | `FUN_00015db8` | CFDC2110/JTAG/SPI/control | family-1 common transport-send command | High |
| `0x15DEA` | `FUN_00015dea` | CFDC2110/JTAG/SPI/control | family-1 generic register read | High |
| `0x15E80` | `FUN_00015e80` | CFDC2110/JTAG/SPI/control | CFDC2110 family command helper | High |
| `0x15F7C` | `FUN_00015f7c` | CFDC2110/JTAG/SPI/control | divider register write (RMIDIV/ACQDIV) | High |
| `0x15FD8` | `FUN_00015fd8` | CFDC2110/JTAG/SPI/control | CFDC2110 family command helper | High |
| `0x1600E` | `FUN_0001600e` | CFDC2110/JTAG/SPI/control | family-0 generic register write | High |
| `0x16066` | `FUN_00016066` | CFDC2110/JTAG/SPI/control | CFDC2110 status/not-supported helper | High |
| `0x16074` | `FUN_00016074` | CFDC2110/JTAG/SPI/control | toggle software interrupt-state bit 2 and synchronize | High |
| `0x160A8` | `FUN_000160a8` | CFDC2110/JTAG/SPI/control | toggle software interrupt-state bit 3 and synchronize | High |
| `0x160DC` | `FUN_000160dc` | CFDC2110/JTAG/SPI/control | CFDC2110 family command helper | High |
| `0x16168` | `FUN_00016168` | CFDC2110/JTAG/SPI/control | family-0 common TX transport command | High |
| `0x1619A` | `FUN_0001619a` | CFDC2110/JTAG/SPI/control | CFDC2110 request/response transport transaction | High |
| `0x1621A` | `FUN_0001621a` | CFDC2110/JTAG/SPI/control | family-2 timer arm/re-arm | High |
| `0x1634E` | `FUN_0001634e` | CFDC2110/JTAG/SPI/control | family-2 timer wait/start | High |
| `0x163B2` | `FUN_000163b2` | CFDC2110/JTAG/SPI/control | family-2 INTST write | High |
| `0x16414` | `FUN_00016414` | CFDC2110/JTAG/SPI/control | family-2 INTST pulse operation | High |
| `0x16490` | `FUN_00016490` | CFDC2110/JTAG/SPI/control | family-2 board reset/error service | High |
| `0x164B8` | `FUN_000164b8` | CFDC2110/JTAG/SPI/control | family-2 LEDCTL two-bit writer with status response | High |
| `0x16500` | `FUN_00016500` | CFDC2110/JTAG/SPI/control | timer/event helper destructor | High |
| `0x16524` | `FUN_00016524` | CFDC2110/JTAG/SPI/control | timer/event helper deleting destructor | High |
| `0x16544` | `FUN_00016544` | CFDC2110/JTAG/SPI/control | CFDC2110 object cleanup/destructor | High |
| `0x16594` | `FUN_00016594` | CFDC2110/JTAG/SPI/control | C5FB handler | High |
| `0x165A6` | `FUN_000165a6` | CFDC2110/JTAG/SPI/control | A5FB family-1 dispatcher | High |
| `0x166A8` | `FUN_000166a8` | CFDC2110/JTAG/SPI/control | A5FB family-2 dispatcher | High |
| `0x167F4` | `FUN_000167f4` | CFDC2110/JTAG/SPI/control | 85FB response-buffer fetch/transaction helper | High |
| `0x16962` | `FUN_00016962` | CFDC2110/JTAG/SPI/control | CFDC2110 command helper | High |
| `0x169B4` | `FUN_000169b4` | CFDC2110/JTAG/SPI/control | 85FB response handler | High |
| `0x16A66` | `FUN_00016a66` | CFDC2110/JTAG/SPI/control | A5FB family-0 dispatcher | High |
| `0x16BBA` | `FUN_00016bba` | CFDC2110/JTAG/SPI/control | A5FB top-level family dispatcher | High |
| `0x16C14` | `FUN_00016c14` | CFDC2110/JTAG/SPI/control | CFDC2110 packet signature dispatcher | High |
| `0x16C62` | `FUN_00016c62` | 1-Wire/Dallas | 1-Wire helper base/destructor vtable reset | High |
| `0x16C6A` | `FUN_00016c6a` | 1-Wire/Dallas | bind ONEWIRE controller register | High |
| `0x16C92` | `FUN_00016c92` | 1-Wire/Dallas | issue low-level ONEWIRE command | High |
| `0x16CA4` | `FUN_00016ca4` | 1-Wire/Dallas | read ONEWIRE controller status | High |
| `0x16CB0` | `FUN_00016cb0` | 1-Wire/Dallas | 1-Wire reset/presence | High |
| `0x16CF0` | `FUN_00016cf0` | 1-Wire/Dallas | 1-Wire write byte | High |
| `0x16D22` | `FUN_00016d22` | 1-Wire/Dallas | 1-Wire read byte | High |
| `0x16D60` | `FUN_00016d60` | 1-Wire/Dallas | 1-Wire/Dallas helper object constructor | High |
| `0x16D82` | `FUN_00016d82` | 1-Wire/Dallas | 1-Wire write-byte status wrapper | High |
| `0x16D90` | `FUN_00016d90` | 1-Wire/Dallas | DS2433 scratchpad write/copy/verify | High |
| `0x16F2C` | `FUN_00016f2c` | 1-Wire/Dallas | Dallas READ MEMORY | High |
| `0x16FAC` | `FUN_00016fac` | 1-Wire/Dallas | Dallas READ ROM + CRC retry | High |
| `0x170BA` | `FUN_000170ba` | 1-Wire/Dallas | small acquisition helper object constructor | High |
| `0x170C8` | `FUN_000170c8` | 1-Wire/Dallas | small acquisition helper object destructor/vtable reset | High |
| `0x170FA` | `FUN_000170fa` | acquisition/DMA/MAM/transport | acquisition/transfer hardware object constructor | High |
| `0x17184` | `FUN_00017184` | acquisition/DMA/MAM/transport | acquisition/transfer hardware object destructor | High |
| `0x171DE` | `FUN_000171de` | acquisition/DMA/MAM/transport | synchronous DMA/acquisition launch and wait | High |
| `0x172A2` | `FUN_000172a2` | acquisition/DMA/MAM/transport | unlink/unregister transfer object | High |
| `0x1731C` | `FUN_0001731c` | acquisition/DMA/MAM/transport | register user transfer and build DMA descriptors | High |
| `0x17478` | `FUN_00017478` | acquisition/DMA/MAM/transport | launch wrapper translating timeout status | High |
| `0x174A8` | `FUN_000174a8` | acquisition/DMA/MAM/transport | acquisition object deleting destructor | High |
| `0x174C8` | `FUN_000174c8` | acquisition/DMA/MAM/transport | read/clear status bit 5 in acquisition register descriptor | High |
| `0x174F2` | `FUN_000174f2` | acquisition/DMA/MAM/transport | initialize acquisition register-descriptor set | High |
| `0x17560` | `FUN_00017560` | acquisition/DMA/MAM/transport | wait for CFDC TX idle | High |
| `0x17578` | `FUN_00017578` | acquisition/DMA/MAM/transport | CFDC RX block receive | High |
| `0x176A2` | `FUN_000176a2` | acquisition/DMA/MAM/transport | read/clear HWInt command word | High |
| `0x176D0` | `FUN_000176d0` | acquisition/DMA/MAM/transport | CFDC RX-ready/state helper | High |
| `0x176E6` | `FUN_000176e6` | acquisition/DMA/MAM/transport | CFDC TX block sender | High |
| `0x1785B` | `FUN_0001785b` | acquisition/DMA/MAM/transport | CFDC transport register/event constructor | High |
| `0x179E2` | `FUN_000179e2` | acquisition/DMA/MAM/transport | indexed 16-bit hardware write with per-index shadow | High |
| `0x17A0A` | `FUN_00017a0a` | acquisition/DMA/MAM/transport | store acquisition global config | High |
| `0x17A16` | `FUN_00017a16` | acquisition/DMA/MAM/transport | dynamic list reset/init helper | High |
| `0x17A36` | `FUN_00017a36` | acquisition/DMA/MAM/transport | dynamic list allocation helper | High |
| `0x17A98` | `FUN_00017a98` | acquisition/DMA/MAM/transport | dynamic list/record initialization helper | High |
| `0x17B00` | `FUN_00017b00` | acquisition/DMA/MAM/transport | grow byte-oriented dynamic buffer | High |
| `0x17B72` | `FUN_00017b72` | acquisition/DMA/MAM/transport | validate multi-list acquisition object state | High |
| `0x17BA0` | `FUN_00017ba0` | acquisition/DMA/MAM/transport | MAM helper constructor/init | High |
| `0x17BA6` | `FUN_00017ba6` | acquisition/DMA/MAM/transport | bind MAMDAT/MAMPGO/MAMSEQ/MAMRGO | High |
| `0x17BC8` | `FUN_00017bc8` | acquisition/DMA/MAM/transport | indexed MAM data programmer | High |
| `0x17C16` | `FUN_00017c16` | acquisition/DMA/MAM/transport | program five MAMDAT slots for one channel | High |
| `0x17CDC` | `FUN_00017cdc` | acquisition/DMA/MAM/transport | emit cached MAMSEQ list | High |
| `0x17D20` | `FUN_00017d20` | acquisition/DMA/MAM/transport | program all acquisition channels then MAMSEQ | High |
| `0x17DAC` | `FUN_00017dac` | acquisition/DMA/MAM/transport | grow/store byte-list element | High |
| `0x17DDC` | `FUN_00017ddc` | acquisition/DMA/MAM/transport | temporary acquisition channel-list constructor | High |
| `0x17E58` | `FUN_00017e58` | acquisition/DMA/MAM/transport | temporary acquisition channel-list destructor | High |
| `0x17EAC` | `FUN_00017eac` | acquisition/DMA/MAM/transport | append channel and return sequence index | High |
| `0x17EE0` | `FUN_00017ee0` | acquisition/DMA/MAM/transport | encode/append one MAMSEQ channel entry | High |
| `0x17F4E` | `FUN_00017f4e` | acquisition/DMA/MAM/transport | DMA/acquisition descriptor helper initializer | High |
| `0x17F60` | `FUN_00017f60` | acquisition/DMA/MAM/transport | unlock and free MDL chain | High |
| `0x17F8C` | `FUN_00017f8c` | acquisition/DMA/MAM/transport | allocate fixed 0x33000-byte DMA descriptor buffer and MDL | High |
| `0x17FD6` | `FUN_00017fd6` | acquisition/DMA/MAM/transport | free one transfer object and all MDL/descriptor resources | High |
| `0x1807A` | `FUN_0001807a` | acquisition/DMA/MAM/transport | build/probe/lock user-buffer MDL chain in chunks up to 32 MiB | High |
| `0x18168` | `FUN_00018168` | acquisition/DMA/MAM/transport | lookup transfer object by token/id | High |
| `0x18194` | `FUN_00018194` | acquisition/DMA/MAM/transport | build legacy 32-bit DMA descriptor table | High |
| `0x1829A` | `FUN_0001829a` | acquisition/DMA/MAM/transport | free every transfer object in a linked transfer list | High |
| `0x182D0` | `__alldiv` | DriverWorks/WDM support | compiler runtime __alldiv | High |
| `0x18380` | `__allshr` | DriverWorks/WDM support | compiler runtime __allshr | High |
| `0x183A2` | `FUN_000183a2` | DriverWorks/WDM support | DriverWorks base-device helper constructor/vtable initialization | High |
| `0x183B8` | `FUN_000183b8` | DriverWorks/WDM support | IoDeleteDevice wrapper/base-device teardown | High |
| `0x183CE` | `FUN_000183ce` | DriverWorks/WDM support | DriverWorks base-device deleting destructor | High |
| `0x184C0` | `FUN_000184c0` | DriverWorks/WDM support | append Unicode string helper | High |
| `0x184FA` | `FUN_000184fa` | DriverWorks/WDM support | registered global/static destructor node invocation and unlink/free | High |
| `0x1851C` | `FUN_0001851c` | DriverWorks/WDM support | run compiler/DriverWorks static initializer table | High |
| `0x18542` | `FUN_00018542` | DriverWorks/WDM support | run registered global/static destructors | High |
| `0x1859C` | `FUN_0001859c` | DriverWorks/WDM support | register global/static destructor callback; immediate fallback on allocation failure | High |
| `0x185DE` | `FUN_000185de` | DriverWorks/WDM support | runtime initialization entry; run initializer tables and return global init status | High |
| `0x1860E` | `FUN_0001860e` | DriverWorks/WDM support | IoConnectInterrupt wrapper | High |
| `0x18648` | `FUN_00018648` | DriverWorks/WDM support | IoDisconnectInterrupt wrapper | High |
| `0x18660` | `FUN_00018660` | DriverWorks/WDM support | interrupt resource/connect helper | High |
| `0x186E4` | `FUN_000186e4` | DriverWorks/WDM support | MMIO resource mapping via MmMapIoSpace | High |
| `0x1875C` | `FUN_0001875c` | DriverWorks/WDM support | allocate/build/forward framework IRP | High |
| `0x187F2` | `FUN_000187f2` | DriverWorks/WDM support | framework resource-list element query wrapper | High |
| `0x18812` | `FUN_00018812` | DriverWorks/WDM support | count matching resource-list entries relative to selected index/type | High |
| `0x18862` | `FUN_00018862` | DriverWorks/WDM support | invoke optional cleanup callback for resource/request helper | High |
| `0x18870` | `FUN_00018870` | DriverWorks/WDM support | destroy synchronous request/event helper state | High |
| `0x1888E` | `FUN_0001888e` | DriverWorks/WDM support | synchronous lower-device resource/property request | High |
| `0x1896E` | `FUN_0001896e` | DriverWorks/WDM support | connect to optional \\Device\\DebugMessageDevice and fetch debug callbacks | High |
| `0x18A1C` | `FUN_00018a1c` | DriverWorks/WDM support | formatted trace/logging backend | High |
| `0x18B9E` | `FUN_00018b9e` | DriverWorks/WDM support | trace/debug object destructor; invoke debug callback and free owned name/buffer | High |
| `0x18FBF` | `FUN_00018fbf` | DriverWorks/WDM support | system-power-state name helper | High |
| `0x190AD` | `FUN_000190ad` | DriverWorks/WDM support | device-power-state name helper | High |
| `0x1919A` | `FUN_0001919a` | DriverWorks/WDM support | IRP trace formatter | High |
| `0x192AC` | `FUN_000192ac` | DriverWorks/WDM support | trace-object string/config construction | High |
| `0x19362` | `FUN_00019362` | DriverWorks/WDM support | trace object constructor | High |
| `0x193B8` | `FUN_000193b8` | DriverWorks/WDM support | IoAcquireCancelSpinLock wrapper | High |
| `0x193D0` | `FUN_000193d0` | DriverWorks/WDM support | IRP completion-routine setup helper | High |
| `0x1941A` | `FUN_0001941a` | DriverWorks/WDM support | cancel-spinlock release/guard helper | High |
| `0x1944C` | `FUN_0001944c` | DriverWorks/WDM support | initialize DriverWorks device state after required synchronization objects exist | High |
| `0x194BA` | `FUN_000194ba` | DriverWorks/WDM support | initialize default PnP policy/capability bitfields | High |
| `0x19506` | `FUN_00019506` | DriverWorks/WDM support | outstanding-I/O reference increment / event clear | High |
| `0x1955A` | `FUN_0001955a` | DriverWorks/WDM support | outstanding-I/O reference decrement / event signaling | High |
| `0x195C2` | `FUN_000195c2` | DriverWorks/WDM support | wait helper for framework outstanding-I/O state | High |
| `0x19614` | `FUN_00019614` | DriverWorks/WDM support | wait helper for framework outstanding-I/O state | High |
| `0x19662` | `FUN_00019662` | DriverWorks/WDM support | wait helper for framework outstanding-I/O state | High |
| `0x196B4` | `FUN_000196b4` | DriverWorks/WDM support | power completion/state callback | High |
| `0x19734` | `FUN_00019734` | DriverWorks/WDM support | initialize default power-policy bitfields and cached power state | High |
| `0x197EE` | `FUN_000197ee` | DriverWorks/WDM support | power completion callback; clear pending flag, start next power IRP, release outstanding-I/O ref | High |
| `0x19812` | `FUN_00019812` | DriverWorks/WDM support | issue synchronous IRP_MN_QUERY_CAPABILITIES and cache DEVICE_CAPABILITIES | High |
| `0x198D0` | `FUN_000198d0` | DriverWorks/WDM support | synchronous power-state query/forward helper | High |
| `0x19966` | `FUN_00019966` | DriverWorks/WDM support | power request completion callback | High |
| `0x199AA` | `FUN_000199aa` | DriverWorks/WDM support | power-request completion callback; continue device-power transition and release context | High |
| `0x19A00` | `FUN_00019a00` | DriverWorks/WDM support | cancel pending power IRP | High |
| `0x19B82` | `FUN_00019b82` | DriverWorks/WDM support | framework spinlock/list object constructor | High |
| `0x19BA6` | `FUN_00019ba6` | DriverWorks/WDM support | spinlock acquire wrapper | High |
| `0x19BB8` | `FUN_00019bb8` | DriverWorks/WDM support | ASM-verified tail-jump to KfReleaseSpinLock with lock at this+0x0C and saved KIRQL from this+0x10 | High |
| `0x19BC6` | `FUN_00019bc6` | DriverWorks/WDM support | locked container operation wrapper around intrusive-list front/pop helper | High |
| `0x19BE4` | `FUN_00019be4` | DriverWorks/WDM support | locked intrusive-list lookup wrapper | High |
| `0x19C08` | `FUN_00019c08` | DriverWorks/WDM support | locked intrusive-list insertion wrapper | High |
| `0x19C26` | `FUN_00019c26` | DriverWorks/WDM support | locked intrusive-list pop-front wrapper | High |
| `0x19C9C` | `FUN_00019c9c` | DriverWorks/WDM support | complete stored/forwarded IRP and release originating device outstanding-I/O ref | High |
| `0x19CCC` | `FUN_00019ccc` | DriverWorks/WDM support | framework device support-state cleanup | High |
| `0x19D44` | `FUN_00019d44` | DriverWorks/WDM support | power completion continuation; free context, update flags, complete original IRP | High |
| `0x19D84` | `FUN_00019d84` | DriverWorks/WDM support | power completion continuation; on success forward original IRP, on failure complete it | High |
| `0x19DDC` | `FUN_00019ddc` | DriverWorks/WDM support | synchronous power request helper | High |
| `0x19E56` | `FUN_00019e56` | DriverWorks/WDM support | asynchronous PoRequestPowerIrp helper | High |
| `0x19EE6` | `FUN_00019ee6` | DriverWorks/WDM support | register device interface | High |
| `0x19F54` | `FUN_00019f54` | DriverWorks/WDM support | enable/disable registered device interface | High |
| `0x19F9A` | `FUN_00019f9a` | DriverWorks/WDM support | enable/disable registered device interface | High |
| `0x19FE0` | `FUN_00019fe0` | DriverWorks/WDM support | spinlock-protected intrusive-list insertion | High |
| `0x19FFE` | `FUN_00019ffe` | DriverWorks/WDM support | spinlock-protected intrusive-list lookup/remove target | High |
| `0x1A022` | `FUN_0001a022` | DriverWorks/WDM support | spinlock-protected intrusive-list pop-front | High |
| `0x1A040` | `FUN_0001a040` | DriverWorks/WDM support | spinlock-protected intrusive-list empty test | High |
| `0x1A05E` | `FUN_0001a05e` | DriverWorks/WDM support | DriverWorks intrusive queue/list wrapper constructor | High |
| `0x1A07C` | `FUN_0001a07c` | DriverWorks/WDM support | no-op framework hook/default callback | High |
| `0x1A0B6` | `FUN_0001a0b6` | DriverWorks/WDM support | DriverWorks base device constructor | High |
| `0x1A17A` | `FUN_0001a17a` | DriverWorks/WDM support | DriverWorks base device destructor | High |
| `0x1A196` | `FUN_0001a196` | DriverWorks/WDM support | cancel routine for IRP stored in DriverWorks queue | High |
| `0x1A1D4` | `FUN_0001a1d4` | DriverWorks/WDM support | drain queued IRPs, cancel or dispatch each according to mode | High |
| `0x1A276` | `FUN_0001a276` | DriverWorks/WDM support | partition queued IRPs by criterion; cancel matching entries and restore others | High |
| `0x1A342` | `FUN_0001a342` | DriverWorks/WDM support | power continuation that may request a device-power transition before completing | High |
| `0x1A420` | `FUN_0001a420` | DriverWorks/WDM support | generic DriverWorks IRP dispatcher | High |
| `0x1A6EA` | `FUN_0001a6ea` | DriverWorks/WDM support | full PnP IRP state machine | High |
| `0x1B096` | `FUN_0001b096` | DriverWorks/WDM support | full power IRP state machine | High |
| `0x1B98E` | `FUN_0001b98e` | DriverWorks/WDM support | attach device to lower stack | High |
| `0x1B9C6` | `FUN_0001b9c6` | DriverWorks/WDM support | lower-device wrapper constructor/bind helper with status return | High |
| `0x1B9FA` | `FUN_0001b9fa` | DriverWorks/WDM support | lower-device wrapper constructor/reset | High |
| `0x1BA0A` | `FUN_0001ba0a` | DriverWorks/WDM support | synchronous power IRP forward-and-wait | High |
| `0x1BA92` | `FUN_0001ba92` | DriverWorks/WDM support | spinlock acquire respecting current IRQL | High |
| `0x1BAAE` | `FUN_0001baae` | DriverWorks/WDM support | matching spinlock release | High |
| `0x1BACA` | `FUN_0001baca` | DriverWorks/WDM support | spinlock acquire wrapper preserving caller IRQL mode | High |
| `0x1BAEC` | `FUN_0001baec` | DriverWorks/WDM support | matching spinlock release wrapper | High |
| `0x1BB0E` | `FUN_0001bb0e` | DriverWorks/WDM support | convert KDEVICE_QUEUE entry pointer to containing queued-IRP object | High |
| `0x1BB2A` | `FUN_0001bb2a` | DriverWorks/WDM support | advance to next KDEVICE_QUEUE entry and recover containing queued-IRP object | High |
| `0x1BB4E` | `FUN_0001bb4e` | DriverWorks/WDM support | cancel/remove matching queued IRPs | High |
| `0x1BC0E` | `FUN_0001bc0e` | DriverWorks/WDM support | cancel-spinlock ownership/match check for current IRP | High |
| `0x1BC5E` | `FUN_0001bc5e` | DriverWorks/WDM support | rebind owned referenced-object holder and optionally clear status | High |
| `0x1BCBA` | `FUN_0001bcba` | DriverWorks/WDM support | wide-string length helper | High |
| `0x1BCCE` | `FUN_0001bcce` | DriverWorks/WDM support | wide-string byte-length helper | High |
| `0x1BCDC` | `FUN_0001bcdc` | DriverWorks/WDM support | recover DriverWorks object/header base from embedded variable-length string pointer | High |
| `0x1BCFA` | `FUN_0001bcfa` | DriverWorks/WDM support | owned Unicode buffer constructor | High |
| `0x1BD3C` | `FUN_0001bd3c` | DriverWorks/WDM support | copy UNICODE_STRING into owned buffer | High |
| `0x1BD6A` | `FUN_0001bd6a` | DriverWorks/WDM support | copy wide string into owned buffer | High |
| `0x1BDA0` | `FUN_0001bda0` | DriverWorks/WDM support | grow owned Unicode buffer | High |
| `0x1BE12` | `FUN_0001be12` | DriverWorks/WDM support | initialize simple lower-device/file-object holder | High |
| `0x1BE20` | `FUN_0001be20` | DriverWorks/WDM support | initialize lower-device pointer/status wrapper | High |
| `0x1BE34` | `FUN_0001be34` | DriverWorks/WDM support | IoGetDeviceObjectPointer wrapper for named device | High |
| `0x1BED2` | `FUN_0001bed2` | DriverWorks/WDM support | synchronous generic IRP forward-and-wait | High |
| `0x1BF5A` | `FUN_0001bf5a` | DriverWorks/WDM support | synchronous lower-device DeviceIoControl helper | High |
| `0x1BFDA` | `FUN_0001bfda` | DriverWorks/WDM support | initialize intrusive doubly-linked-list head with embedded-link offset | High |
| `0x1BFEC` | `FUN_0001bfec` | DriverWorks/WDM support | return first intrusive-list object or null | High |
| `0x1BFFA` | `FUN_0001bffa` | DriverWorks/WDM support | insert object into intrusive doubly-linked list | High |
| `0x1C024` | `FUN_0001c024` | DriverWorks/WDM support | lookup intrusive-list object by supplied link pointer/default front | High |
| `0x1C04A` | `FUN_0001c04a` | DriverWorks/WDM support | remove specified intrusive-list object | High |
| `0x1C064` | `FUN_0001c064` | DriverWorks/WDM support | test intrusive list empty | High |
| `0x1C06A` | `FUN_0001c06a` | DriverWorks/WDM support | pop first intrusive-list object | High |
| `0x1C084` | `FUN_0001c084` | DriverWorks/WDM support | close/release named-device/file-object holder | High |
| `0x1C0A8` | `FUN_0001c0a8` | DriverWorks/WDM support | compiler SEH prolog/register-save helper | High |
| `0x1C0E1` | `__SEH_epilog` | DriverWorks/WDM support | compiler SEH epilog | High |
| `0x1C0F2` | `memmove` | DriverWorks/WDM support | memmove import thunk | High |
| `0x1C0F8` | `DbgBreakPoint` | DriverWorks/WDM support | import thunk for DbgBreakPoint | High |
| `0x1C0FE` | `strchr` | DriverWorks/WDM support | import thunk for strchr | High |
| `0x1C104` | `_vsnprintf` | compiler/SEH/runtime | import thunk for _vsnprintf | High |
| `0x1C10A` | `DbgPrint` | compiler/SEH/runtime | import thunk for DbgPrint | High |
| `0x1C1C6` | `FUN_0001c1c6` | compiler/SEH/runtime | C++ exception cleanup/unwind dispatcher wrapper | High |
| `0x1C1E4` | `__global_unwind2` | compiler/SEH/runtime | compiler global unwind helper | High |
| `0x1C226` | `FUN_0001c226` | compiler/SEH/runtime | C++ exception cleanup-table unwind loop | High |
| `0x1C2A4` | `RtlUnwind` | compiler/SEH/runtime | RtlUnwind import thunk | High |
| `0x1C2AA` | `FUN_0001c2aa` | compiler/SEH/runtime | register static destructor callback | High |
| `0x1C2B6` | `FUN_0001c2b6` | compiler/SEH/runtime | initialize global INTEN register descriptor and register its static destructor | High |
| `0x1D3A4` | `FUN_0001d3a4` | DriverWorks registry/string/device support | recursive DriverWorks object-tree cleanup/destructor walk | High |
| `0x1D3DC` | `FUN_0001d3dc` | DriverWorks registry/string/device support | free DriverWorks object only when allocation magic matches | High |
| `0x1D3F2` | `FUN_0001d3f2` | DriverWorks registry/string/device support | no-op framework hook | High |
| `0x1D3F4` | `FUN_0001d3f4` | DriverWorks registry/string/device support | DriverWorks device-support object constructor/vtable initialization | High |
| `0x1D418` | `FUN_0001d418` | DriverWorks registry/string/device support | concatenate/grow Unicode strings | High |
| `0x1D476` | `FUN_0001d476` | DriverWorks registry/string/device support | framework device/symbolic-link destructor | High |
| `0x1D4D6` | `FUN_0001d4d6` | DriverWorks registry/string/device support | patch DriverWorks dispatch defaults according to driver capability flags | High |
| `0x1D508` | `FUN_0001d508` | DriverWorks registry/string/device support | DriverWorks device-support deleting destructor | High |
| `0x1D572` | `FUN_0001d572` | DriverWorks registry/string/device support | IoCreateDevice + symbolic-link helper | High |
| `0x1D756` | `FUN_0001d756` | DriverWorks registry/string/device support | allocating wrapper around DriverWorks IoCreateDevice/symbolic-link helper | High |
| `0x1D78A` | `FUN_0001d78a` | DriverWorks registry/string/device support | free owned generated Unicode-name buffer | High |
| `0x1D7A6` | `FUN_0001d7a6` | DriverWorks registry/string/device support | increment numeric suffix and regenerate Unicode device name | High |
| `0x1D7FA` | `FUN_0001d7fa` | DriverWorks registry/string/device support | construct generated numbered Unicode device name | High |
| `0x1D868` | `FUN_0001d868` | DriverWorks registry/string/device support | numbered Unicode device-name helper constructor | High |
| `0x1D8A2` | `FUN_0001d8a2` | DriverWorks registry/string/device support | registry key open/create helper | High |
| `0x1DBCE` | `FUN_0001dbce` | DriverWorks registry/string/device support | map DriverWorks registry-root selector to fixed NT registry path | High |
| `0x1DC24` | `FUN_0001dc24` | DriverWorks registry/string/device support | registry-key wrapper constructor | High |
| `0x1DC76` | `FUN_0001dc76` | DriverWorks registry/string/device support | scan resource-list structure for Nth descriptor of requested type | High |
| `0x1DCD4` | `FUN_0001dcd4` | DriverWorks registry/string/device support | kernel mutex wrapper constructor | High |
| `0x1DCFA` | `FUN_0001dcfa` | DriverWorks registry/string/device support | bind/reinitialize mutex wrapper | High |
| `0x1DD1E` | `FUN_0001dd1e` | DriverWorks registry/string/device support | event/synchronous-I/O helper constructor | High |
| `0x1DD80` | `FUN_0001dd80` | DriverWorks registry/string/device support | install global DriverWorks driver-object singleton/vtable | High |
| `0x1DD98` | `FUN_0001dd98` | DriverWorks registry/string/device support | global DriverWorks support initialization | High |
| `0x1DDC6` | `FUN_0001ddc6` | DriverWorks registry/string/device support | install DriverWorks default dispatch/callback slots on DRIVER_OBJECT support state | High |
| `0x1DE46` | `FUN_0001de46` | DriverWorks registry/string/device support | global driver-object initialization | High |
| `0x1DF42` | `entry` | entry | PE/DriverEntry entry point | High |
