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

Assert-Administrator

$repo = Split-Path -Parent $PSScriptRoot
$driver = Join-Path $repo ("x64\{0}\LecS65AcqDrv.sys" -f $Configuration)
$systemDriver = Join-Path $env:SystemRoot "System32\drivers\LecS65AcqDrv.sys"
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
Write-Host "Signing:"
Write-Host "  $driver"
& $signtool sign /fd SHA256 /sm /s My /n $CertificateName $driver
if ($LASTEXITCODE -ne 0) {
    throw "signtool failed with exit code $LASTEXITCODE."
}

Write-Host "Stopping LecS65AcqDrv..."
& sc.exe stop LecS65AcqDrv | Out-Host
Start-Sleep -Milliseconds 700

Write-Host "Installing freshly built SYS:"
Write-Host "  $systemDriver"
Copy-Item -Force $driver $systemDriver

Write-Host "Starting LecS65AcqDrv..."
& sc.exe start LecS65AcqDrv | Out-Host
Start-Sleep -Milliseconds 700

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
