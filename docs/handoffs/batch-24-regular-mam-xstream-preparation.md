# Batch 24 regular MAM and first XStream preparation

Historical traces prove automatic repeated MAM acquisition. The regular WDM-v3
path already preserves the original one-channel ABI/register sequence using
Windows DMA-logical addresses. Batch 24 adds `LecMamLaunchLimit`, an atomic
development admission limit after descriptor validation and before MMIO. The
initial candidate uses limit 1, keeps `FirstMamExperimentOnly=false`, does not
enable MTT and leaves the default build no-launch.

Offline evidence: focused gate 21/21 PASS, source contracts 72/72 PASS, complete
Windows PowerShell 5.1 Dry suite PASS, and zero-warning Debug/default plus
Release/bounded builds. Signed-package, installation and R0/R1 evidence follow
after their one-shot operations. XStream launch remains unauthorized.

## Signed and installed result

Commit `24fe8d90ca64c32d734eb131138caffd6486b815` produced signed Release x64
version `0.2026.1030.1` with certificate
`4296FA2E355613DF0D5CE6F24D0898E00B706472`.

- INF: `03019646F18DBF808571377A875171B62B302FE48BD6112C007F025A3B522AC8`
- CAT: `F7481516D53295CCAC085AAF4ACA9AD11AAA6C03C797277817BDB6B143C327F9`
- signed SYS: `184471C04BCE4688D3EE6EAC01049606F7878D9A0568419F0A08516282F35CAE`
- PDB: `00EB3783DE6B23C787D602626B6D78E5478DACCE5D83A86F63FCD362229CDBFC`
- manifest: `F3D16A27BDB1F137E47942C38F4A959A503AD1F12771F57542BE29081C31BC82`

The one captured installation succeeded without retry and published
`oem107.inf`. The capture contains `LECS65_WDM_V3_MAM_DMA_ENABLED` and
`LECS65_MAM_LAUNCH_LIMIT=1`. Device status is `OK`, problem code zero, service
running and five interfaces published. Installed qualification passed R0 9/9
and R1 2/2; PCI is `1570:0005`, observed BDF `5:1.0`, and both START reads are
`0x00000002`.

Private evidence hashes: install capture
`AAB53661C6E82ECDAFB2FA3A37B3CAFF47F491E0F985C809E8D31806DFDCBE91`,
install result `FA34656103B2BDBEF1BF1BBA33BF17AA39723684ABEF9892999CA168F3EAFF24`,
R0/R1 manifest `C7840E7C970ABAC9E0562EED467B521667832DD3D3313F14F9C652B70936F0E5`.

Software inventory exposes only x64 NVMe `HFM256GDHTNG-8510B`, serial
`ACE4_2E00_9590_A2AA.`; this is not physical-isolation proof. Before XStream,
obtain fresh owner confirmation that the productive x86 disk remains physically
disconnected and separate authorization to launch XStream. The maximum kernel
admission is one. Any admitted pre-GO failure consumes it; post-GO uncertainty
quarantines DMA; no retry is permitted.
