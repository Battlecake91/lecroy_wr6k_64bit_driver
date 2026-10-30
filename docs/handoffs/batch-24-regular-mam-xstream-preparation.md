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
