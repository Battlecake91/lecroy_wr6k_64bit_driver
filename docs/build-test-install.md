# Automatischer Build/Test-Install

`scripts\build-test-install.ps1` automatisiert den aktuellen x64-Bring-up-Zyklus:

1. `LecS65AcqDrv.sys` bauen
2. `lecdiag.exe` bauen
3. Testzertifikat erzeugen oder wiederverwenden
4. SYS signieren
5. CAT nach dem SYS-Signing neu erzeugen
6. CAT signieren und beide Signaturen prüfen
7. das aktuell gebundene `oem*.inf` fuer `PCI\VEN_1570&DEV_0005` entfernen
8. das neue Paket installieren
9. PnP-Status prüfen
10. optional den sicheren `lecdiag bars`-Dump ausführen

Aus einer PowerShell als Administrator:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build-test-install.ps1 -RunBars
```

Nützliche Optionen:

```powershell
# Nur bauen/signieren, installierten Treiber nicht anfassen
.\scripts\build-test-install.ps1 -SkipInstall

# Vorhandenen Build neu paketieren/installieren
.\scripts\build-test-install.ps1 -SkipBuild -RunBars

# Release-Build
.\scripts\build-test-install.ps1 -Configuration Release
```

Das Skript führt bewusst keine rohen BAR-Reads oder BAR-Writes aus.