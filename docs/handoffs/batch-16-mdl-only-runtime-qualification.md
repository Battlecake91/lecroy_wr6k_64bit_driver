# Batch 16 MDL-Only Runtime Qualification

## Reviewed implementation

Commit `7eeae4c077d56de5741f8ae72d6d1bb65179edb5` fixes the safe
`CFDC2138` path to validate the registered MDL payload length without relying
on legacy PFN-table metadata. Production `CFDC2124` retains only its locked
MDL chain; `LECS65_ENABLE_UNSAFE_LEGACY_PFN_DMA` remains zero. Public IOCTL
numbers and structures are unchanged, WDM-v3 device addresses still come only
from the Windows DMA API, and hardware launch remains unconditionally denied.

## Offline and package evidence

- Debug and Release x64: zero warnings and errors.
- Functional suites: 792/792 PASS across 24 named suites.
- Stress: 66,000/66,000 PASS.
- MDL-only contracts: 15/15 PASS.
- Runtime qualification mocks: 22/22 PASS.
- Elevated signing launcher mocks: 36/36 PASS.
- Driver tree: `01ec8b7b58c7b2bf45ac9a8fa4fe8111e03169a8`.
- Include tree: `eb1420b7c97a5e9d827aee59dadd8b0d6dbf3add`.

The single signed package uses DriverVer `10/26/2026,0.2026.1026.1` and local
test certificate `4296FA2E355613DF0D5CE6F24D0898E00B706472`.

| Artifact | SHA-256 |
| --- | --- |
| INF | `78B6C2DCDA0321D9C5BDB91D8694F1D445F4742208D1CEAECE642A497428B93C` |
| CAT | `42D0BD8296C4110E00C97845CAECE7D607781A8A43C1E541310AA9DDFBD4AF50` |
| Signed SYS | `32878635FCA12750A1B4489A542E0F71A22D002A445E824A07B5AD160F4F3E4E` |
| Matching PDB | `1243C080B9749450B994DFB4AF68FD268C504E0E2E50FF5E72FC841C9CCDFBB9` |
| Manifest | `A8845CC06AB00E0A0388400CDCA80F8FAA8E443050F1BDE5ADF9FD0E99C55717` |

Authenticode, the local chain, catalog membership, INF consistency and
SYS/PDB identity passed. This is local test signing, not Microsoft production
signing.

## Runtime evidence

The captured installation ran once and published `oem104.inf`. The device is
`OK`, problem code zero; `LecS65AcqDrv` is running, five interfaces are
published and the loaded SYS matches the signed artifact. `oem102.inf` and
`oem103.inf` remain available for rollback.

- Installation capture SHA-256:
  `18C800C1E1AC06A8049B7C25990E58803E685918F608825737C180B93F4F0015`.
- Installation result SHA-256:
  `924B4F735C1EB75FA5D92A12518CFDD2792C9EDADAC0BC21B2BB3B8D21AC80AF`.

The exact-once ledger passed 9/9 R0 and 2/2 R1. Observed values were trace
size `0x110`, register-list size `0x2CAE`, PCI `1570:0005`, BDF `5:1.0`, PCI
command `0x0006`, BAR0/1/2 `F7CBFE00`/`F7CC0000`/`F7CBFC00`, IRQ line 19 and
matching START reads `0x00000002`. Matching all-ones reads are now a failing
trust gate.

- Runtime plan SHA-256:
  `493C502C8246092CB5BE9DC0DCDEE40FB8DC6C0A95E9892413EFFDF9825446D6`.
- Runtime wrapper log SHA-256:
  `F9641622B4718B7CC4FC09824C63DA2F7C60EBA6C7841BB92FD26A58B557D102`.

One captured PnP restart passed with exactly one restart invocation. The log
has 48 `PNP_STATUS` records, five interface disables/enables, one IRQ
disconnect/reconnect, three BAR unmaps/remaps, REMOVE, Unload and successful
START. Its SHA-256 is
`8D87D586910E01705810D5ABECD94F7B4493C94E6EEB50843EF5F9DCA236DE66`.
The restart result SHA-256 is
`779C4AB4D07C130E371745DFE6E980660795F039431C2E8933273EFDEEFC0920`.

## No-launch stop

The no-launch capture wrapper failed its readiness gate before invoking the
repository runner. It polled DebugView's buffered raw log; that log became
visible only after capture exit, although the final file contains attributable
StorNVMe records. No no-launch evidence directory, device-attempt marker or
`lecdiag` process exists. Therefore zero `CFDC2124`, `CFDC2138` and cleanup
requests were sent, and no WDM-v3 hardware mapping evidence was produced.
No retry was made. The failed capture's raw-log SHA-256 is
`780B64493791FE9DEFC377CE53B111B2DEBDF4B095D842DE5833235935E99F10`.

## Remaining gate

Physical DMA idle, post-GO retirement, real DMA and acquisition remain
unproved. The smallest next live step is one separately authorized bounded
no-launch attempt using the qualified DebugView live-stdout readiness path.
It must retain the current package identity, stop on any mapping/release or
health error, and still must not authorize GO or real DMA.
