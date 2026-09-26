[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$PackageDir = "C:\Temp\LecS65_x64",
    [string]$CertificateSubject = "CN=LecS65 x64 Test",
    [string]$Inf2CatOs = "10_GE_X64",
    [switch]$SkipBuild,
    [switch]$SkipInstall,
    [switch]$RunBars
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Write-Step {
    param([string]$Text)
    Write-Host ""
    Write-Host "=== $Text ===" -ForegroundColor Cyan
}

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Dieses Skript muss in einer PowerShell als Administrator ausgefuehrt werden."
    }
}

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments
    )
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "'$FilePath' ist mit Exitcode $LASTEXITCODE fehlgeschlagen."
    }
}

function Find-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][string]$Filter,
        [Parameter(Mandatory = $true)][string]$PathRegex
    )
    $item = Get-ChildItem $Root -Recurse -File -Filter $Filter -ErrorAction SilentlyContinue |
        Where-Object FullName -match $PathRegex |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $item) {
        throw "$Filter wurde unter $Root nicht gefunden."
    }
    return $item.FullName
}

function Find-VcVars64 {
    $preferred = @(
        "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\2026\Insiders\VC\Auxiliary\Build\vcvars64.bat",
        "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
    )

    foreach ($path in $preferred) {
        if (Test-Path $path) {
            return $path
        }
    }

    $candidate = Get-ChildItem "C:\Program Files\Microsoft Visual Studio" -Recurse -File -Filter "vcvars64.bat" -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\18\\' -and $_.FullName -match '\\VC\\Auxiliary\\Build\\vcvars64\.bat$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if ($candidate) {
        return $candidate.FullName
    }

    throw "Visual Studio 2026 / v145 vcvars64.bat wurde nicht gefunden."
}

function Get-LecDevice {
    Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue |
        Where-Object InstanceId -like 'PCI\VEN_1570&DEV_0005*' |
        Select-Object -First 1
}

Assert-Administrator

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$solution = Join-Path $repoRoot "LecS65AcqDrv.sln"
$driverInf = Join-Path $repoRoot "driver\LecS65AcqDrv.inf"
$driverSys = Join-Path $repoRoot "x64\$Configuration\LecS65AcqDrv.sys"
$lecdiagDir = Join-Path $repoRoot "tools\lecdiag"
$lecdiagExe = Join-Path $lecdiagDir "build\lecdiag.exe"

$kitsRoot = "C:\Program Files (x86)\Windows Kits\10"
$vcvars64 = Find-VcVars64
$inf2cat = Find-Tool -Root $kitsRoot -Filter "Inf2Cat.exe" -PathRegex '\\bin\\[0-9.]+\\x86\\Inf2Cat\.exe$'
$signtool = Find-Tool -Root $kitsRoot -Filter "signtool.exe" -PathRegex '\\bin\\[0-9.]+\\x64\\signtool\.exe$'

Write-Host "Repo:      $repoRoot"
Write-Host "Config:    $Configuration"
Write-Host "Package:   $PackageDir"
Write-Host "vcvars64:  $vcvars64"
Write-Host "Inf2Cat:   $inf2cat"
Write-Host "SignTool:  $signtool"

Write-Step "Testsigning pruefen"
$bcd = bcdedit /enum "{current}" 2>&1 | Out-String
if ($bcd -notmatch '(?im)^\s*testsigning\s+Yes\s*$') {
    Write-Warning "BCDEdit meldet fuer {current} nicht 'testsigning Yes'."
}
else {
    Write-Host "Testsigning ist aktiviert."
}

if (-not $SkipBuild) {
    Write-Step "Treiber bauen"
    $msbuildCommand = 'call "' + $vcvars64 + '" >nul && msbuild "' + $solution + '" /m /t:Build /p:Configuration=' + $Configuration + ' /p:Platform=x64'
    Invoke-Native "cmd.exe" "/d" "/s" "/c" $msbuildCommand

    if (-not (Test-Path $driverSys)) {
        throw "Treiber-Build meldete Erfolg, aber $driverSys wurde nicht gefunden."
    }

    Write-Step "lecdiag bauen"
    $lecdiagCommand = 'call "' + $vcvars64 + '" >nul && cd /d "' + $lecdiagDir + '" && build.cmd'
    Invoke-Native "cmd.exe" "/d" "/s" "/c" $lecdiagCommand

    if (-not (Test-Path $lecdiagExe)) {
        throw "lecdiag-Build meldete Erfolg, aber $lecdiagExe wurde nicht gefunden."
    }
}
else {
    Write-Step "Build uebersprungen"
    if (-not (Test-Path $driverSys)) {
        throw "$driverSys existiert nicht."
    }
}

Write-Step "Testzertifikat vorbereiten"
$cert = Get-ChildItem Cert:\LocalMachine\My |
    Where-Object Subject -eq $CertificateSubject |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if (-not $cert) {
    Write-Host "Erzeuge neues Testzertifikat: $CertificateSubject"
    $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $CertificateSubject -CertStoreLocation "Cert:\LocalMachine\My" -HashAlgorithm SHA256
}
else {
    Write-Host "Vorhandenes Testzertifikat: $($cert.Thumbprint)"
}

$certTemp = Join-Path $env:TEMP "LecS65_Test.cer"
Export-Certificate -Cert $cert -FilePath $certTemp -Force | Out-Null

if (-not (Get-ChildItem Cert:\LocalMachine\Root | Where-Object Thumbprint -eq $cert.Thumbprint)) {
    Import-Certificate -FilePath $certTemp -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
}

if (-not (Get-ChildItem Cert:\LocalMachine\TrustedPublisher | Where-Object Thumbprint -eq $cert.Thumbprint)) {
    Import-Certificate -FilePath $certTemp -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
}

Write-Step "Treiberpaket vorbereiten"
New-Item -ItemType Directory -Force $PackageDir | Out-Null
Get-ChildItem $PackageDir -File -ErrorAction SilentlyContinue |
    Where-Object Extension -in ".sys", ".inf", ".cat" |
    Remove-Item -Force

Copy-Item $driverSys (Join-Path $PackageDir "LecS65AcqDrv.sys") -Force
Copy-Item $driverInf (Join-Path $PackageDir "LecS65AcqDrv.inf") -Force

$packageSys = Join-Path $PackageDir "LecS65AcqDrv.sys"
$packageInf = Join-Path $PackageDir "LecS65AcqDrv.inf"
$packageCat = Join-Path $PackageDir "LecS65AcqDrv.cat"

Write-Step "SYS signieren"
Invoke-Native $signtool "sign" "/v" "/fd" "SHA256" "/sm" "/s" "My" "/sha1" $cert.Thumbprint $packageSys

Write-Step "CAT erzeugen"
if (Test-Path $packageCat) {
    Remove-Item $packageCat -Force
}
Invoke-Native $inf2cat "/driver:$PackageDir" "/os:$Inf2CatOs" "/verbose"

if (-not (Test-Path $packageCat)) {
    throw "Inf2Cat war erfolgreich, aber $packageCat wurde nicht erzeugt."
}

Write-Step "CAT signieren"
Invoke-Native $signtool "sign" "/v" "/fd" "SHA256" "/sm" "/s" "My" "/sha1" $cert.Thumbprint $packageCat

Write-Step "Signaturen pruefen"
Invoke-Native $signtool "verify" "/v" "/pa" $packageSys
Invoke-Native $signtool "verify" "/v" "/pa" $packageCat

if (-not $SkipInstall) {
    Write-Step "Aktuell gebundenes Treiberpaket ermitteln"
    $device = Get-LecDevice

    if ($device) {
        Write-Host "Geraet: $($device.InstanceId)"
        $infProperty = Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName 'DEVPKEY_Device_DriverInfPath' -ErrorAction SilentlyContinue
        $infPath = $null
        if ($infProperty -and $infProperty.PSObject.Properties.Name -contains 'Data') {
            $infPath = $infProperty.Data
        }

        if ($infPath -and $infPath -match '^oem\d+\.inf$') {
            Write-Host "Entferne aktuell gebundenes Paket: $infPath"

            & "pnputil.exe" "/delete-driver" $infPath "/uninstall" "/force"
            $pnputilExit = $LASTEXITCODE

            if ($pnputilExit -eq 3010) {
                Write-Warning "PnP meldet ERROR_SUCCESS_REBOOT_REQUIRED (3010)."
                Write-Host ""
                Write-Host "Das alte Treiberpaket ist zur Entfernung vorgemerkt."
                Write-Host "Windows muss jetzt neu gestartet werden, bevor der neue Treiber sicher installiert werden kann."
                Write-Host "Nach dem Neustart dieses Skript einfach erneut ausfuehren."
                return
            }

            if ($pnputilExit -ne 0) {
                throw "'pnputil.exe' ist mit Exitcode $pnputilExit fehlgeschlagen."
            }
        }
        else {
            Write-Host "Kein gebundenes oem*.inf gefunden."
        }
    }
    else {
        Write-Host "PCI-Geraet ist aktuell nicht sichtbar. Installation wird trotzdem versucht."
    }

    Write-Step "Treiber installieren"
    Invoke-Native "pnputil.exe" "/add-driver" $packageInf "/install"
    Invoke-Native "pnputil.exe" "/scan-devices"
    Start-Sleep -Seconds 1

    Write-Step "PnP-Status"
    $device = Get-LecDevice
    if (-not $device) {
        throw "LeCroy PCI-Geraet wurde nach der Installation nicht gefunden."
    }

    $problemProperty = Get-PnpDeviceProperty -InstanceId $device.InstanceId -KeyName 'DEVPKEY_Device_ProblemCode' -ErrorAction SilentlyContinue
    $problem = $null
    if ($problemProperty -and $problemProperty.PSObject.Properties.Name -contains 'Data') {
        $problem = $problemProperty.Data
    }

    Write-Host "Status:   $($device.Status)"
    Write-Host "Instance: $($device.InstanceId)"
    Write-Host "Problem:  $problem"

    if ($device.Status -ne "OK" -or ($null -ne $problem -and [int]$problem -ne 0)) {
        throw "Treiber ist installiert, aber das PCI-Geraet ist nicht fehlerfrei gestartet."
    }

    Write-Step "Treiber-Service"
    sc.exe query LecS65AcqDrv
}

if ($RunBars) {
    Write-Step "Sicherer BAR-Ressourcendump"
    if (-not (Test-Path $lecdiagExe)) {
        throw "lecdiag.exe wurde nicht gefunden: $lecdiagExe"
    }
    Invoke-Native $lecdiagExe "bars"
}

Write-Host ""
Write-Host "Fertig." -ForegroundColor Green
