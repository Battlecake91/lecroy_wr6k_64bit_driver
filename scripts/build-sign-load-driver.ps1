param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [string]$CertificateName = "LecS65 x64 Test"
)

$ErrorActionPreference = "Stop"

function Assert-Administrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "Run this script from an elevated PowerShell window."
    }
}

function Find-SignTool {
    $cmd = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $kits = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (-not (Test-Path $kits)) {
        throw "Windows Kits bin directory not found: $kits"
    }

    $candidate = Get-ChildItem $kits -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName "x64\signtool.exe" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1

    if (-not $candidate) {
        throw "signtool.exe was not found in the Windows Kits installation."
    }

    return $candidate
}

function Find-Inf2Cat {
    $cmd = Get-Command Inf2Cat.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $kits = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
    if (-not (Test-Path $kits)) {
        throw "Windows Kits bin directory not found: $kits"
    }

    $candidate = Get-ChildItem $kits -Directory |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName "x86\Inf2Cat.exe" } |
        Where-Object { Test-Path $_ } |
        Select-Object -First 1

    if (-not $candidate) {
        throw "Inf2Cat.exe was not found in the Windows Kits installation."
    }

    return $candidate
}

Assert-Administrator

$repo = Split-Path -Parent $PSScriptRoot
$driver = Join-Path $repo ("x64\{0}\LecS65AcqDrv.sys" -f $Configuration)
$infSource = Join-Path $repo "driver\LecS65AcqDrv.inf"
$packageDir = Join-Path $repo ("x64\{0}\package" -f $Configuration)
$packageInf = Join-Path $packageDir "LecS65AcqDrv.inf"
$packageSys = Join-Path $packageDir "LecS65AcqDrv.sys"
$packageCat = Join-Path $packageDir "LecS65AcqDrv.cat"
$lecdiag = Join-Path $repo "tools\lecdiag\build\lecdiag.exe"

Write-Host "Building driver and lecdiag..."
& (Join-Path $PSScriptRoot "build-driver.ps1") -Configuration $Configuration -BuildLecdiag
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $driver)) {
    throw "Built driver not found: $driver"
}

$cert = Get-ChildItem Cert:\LocalMachine\My |
    Where-Object { $_.Subject -eq "CN=$CertificateName" } |
    Sort-Object NotAfter -Descending |
    Select-Object -First 1

if (-not $cert) {
    throw "Test certificate CN=$CertificateName was not found in Cert:\LocalMachine\My."
}

$signtool = Find-SignTool
Write-Host "Preparing signed driver package..."
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue $packageInf, $packageSys, $packageCat

Copy-Item -Force $driver $packageSys

$infText = Get-Content -Raw $infSource
$now = Get-Date
$version = "0.{0}.{1}.{2}" -f $now.Year, ([int]$now.ToString("MMdd")), ([int]$now.ToString("HHmm"))
$driverVer = "{0},{1}" -f $now.ToString("MM/dd/yyyy"), $version
$infText = [regex]::Replace(
    $infText,
    '(?m)^DriverVer=.*$',
    "DriverVer=$driverVer")
Set-Content -Path $packageInf -Value $infText -Encoding Ascii

$signtool = Find-SignTool
$inf2cat = Find-Inf2Cat

Write-Host "Signing SYS:"
Write-Host "  $packageSys"
& $signtool sign /fd SHA256 /sm /s My /n $CertificateName $packageSys
if ($LASTEXITCODE -ne 0) {
    throw "signtool SYS signing failed with exit code $LASTEXITCODE."
}

Write-Host "Generating catalog..."
& $inf2cat /driver:$packageDir /os:10_X64
if ($LASTEXITCODE -ne 0) {
    throw "Inf2Cat failed with exit code $LASTEXITCODE."
}

Write-Host "Signing catalog:"
Write-Host "  $packageCat"
& $signtool sign /fd SHA256 /sm /s My /n $CertificateName $packageCat
if ($LASTEXITCODE -ne 0) {
    throw "signtool CAT signing failed with exit code $LASTEXITCODE."
}

$device = Get-PnpDevice | Where-Object {
    $_.InstanceId -like 'PCI\VEN_1570&DEV_0005*'
} | Select-Object -First 1

if (-not $device) {
    throw "LeCroy PCI device PCI\VEN_1570&DEV_0005* was not found."
}

$instanceId = $device.InstanceId

Write-Host "Installing package through the Windows Driver Store..."
Write-Host "  $packageInf"
& pnputil.exe /add-driver "$packageInf" /install | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "pnputil /add-driver failed with exit code $LASTEXITCODE."
}

Write-Host "Restarting PnP device..."
& pnputil.exe /restart-device "$instanceId" | Out-Host
if ($LASTEXITCODE -ne 0) {
    Write-Host "pnputil /restart-device was not sufficient; trying disable/enable..."
    & pnputil.exe /disable-device "$instanceId" /force | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "pnputil /disable-device failed with exit code $LASTEXITCODE."
    }
    Start-Sleep -Seconds 1
    & pnputil.exe /enable-device "$instanceId" | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "pnputil /enable-device failed with exit code $LASTEXITCODE."
    }
}

Start-Sleep -Seconds 2

Write-Host ""
Write-Host "Verifying driver build..."
& $lecdiag build
if ($LASTEXITCODE -ne 0) {
    throw "lecdiag build verification failed with exit code $LASTEXITCODE."
}

Write-Host ""
Write-Host "Reading passive PCI configuration..."
& $lecdiag pci
if ($LASTEXITCODE -ne 0) {
    throw "lecdiag pci failed with exit code $LASTEXITCODE."
}
