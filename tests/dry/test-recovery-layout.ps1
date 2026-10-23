[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$scriptPath = Join-Path $repo 'scripts\validate-recovery-layout.ps1'
$root = Join-Path ([System.IO.Path]::GetTempPath()) `
    ('lec-recovery-layout-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$script:passed = 0
$script:failed = 0

function Test-Condition([string]$Name, [bool]$Condition) {
    Write-Host ('[{0}] {1}' -f $(if($Condition){'PASS'}else{'FAIL'}),$Name)
    if($Condition){++$script:passed}else{++$script:failed}
}
function Expect-Rejection([string]$Name, [scriptblock]$Body) {
    $rejected=$false; try { & $Body | Out-Null } catch { $rejected=$true }
    Test-Condition $Name $rejected
}
function New-Marker([string]$Path) {
    New-Item -ItemType Directory -Path (Split-Path -Parent $Path) -Force | Out-Null
    [System.IO.File]::WriteAllText($Path, 'fixture')
}

try {
    $windows=Join-Path $root 'WindowsVolume'
    $efi=Join-Path $root 'EfiVolume'
    $media=Join-Path $root 'RecoveryMedia'
    $rollback=Join-Path $root 'Rollback'
    New-Marker (Join-Path $windows 'Windows\System32\Config\SYSTEM')
    New-Marker (Join-Path $windows 'Windows\System32\ntoskrnl.exe')
    New-Marker (Join-Path $efi 'EFI\Microsoft\Boot\BCD')
    New-Marker (Join-Path $media 'EFI\Boot\bootx64.efi')
    New-Marker (Join-Path $media 'sources\boot.wim')
    New-Marker (Join-Path $rollback 'recovery-manifest.json')

    $before=@($windows,$efi,$media,$rollback | ForEach-Object {
        Get-ChildItem -LiteralPath $_ -Recurse -File | Get-FileHash -Algorithm SHA256
    } | ForEach-Object Hash)
    $resultPath=Join-Path $root 'result.json'
    $result=& $scriptPath -WindowsRoot $windows -EfiRoot $efi `
        -RecoveryMediaRoot $media -RollbackRoot $rollback -OutputPath $resultPath
    $after=@($windows,$efi,$media,$rollback | ForEach-Object {
        Get-ChildItem -LiteralPath $_ -Recurse -File | Get-FileHash -Algorithm SHA256
    } | ForEach-Object Hash)
    Test-Condition 'valid layout passes with all required markers' (
        $result.Outcome -eq 'PASS' -and (Test-Path $resultPath))
    Test-Condition 'validation leaves all input marker hashes unchanged' (
        ($before -join ',') -eq ($after -join ','))
    Test-Condition 'result records no BCD hive Driver Store or reboot mutation' (
        -not $result.Safety.BcdModified -and
        -not $result.Safety.RegistryHiveLoaded -and
        -not $result.Safety.DriverStoreModified -and
        -not $result.Safety.RebootInitiated)
    Expect-Rejection 'existing result cannot be overwritten' {
        & $scriptPath -WindowsRoot $windows -EfiRoot $efi `
            -RecoveryMediaRoot $media -OutputPath $resultPath
    }
    Remove-Item -LiteralPath (Join-Path $media 'EFI\Boot\bootx64.efi')
    Expect-Rejection 'missing x64 UEFI boot marker is rejected' {
        & $scriptPath -WindowsRoot $windows -EfiRoot $efi -RecoveryMediaRoot $media
    }
    Expect-Rejection 'roots must remain distinct' {
        & $scriptPath -WindowsRoot $windows -EfiRoot $windows `
            -RecoveryMediaRoot $rollback
    }
}
finally {
    if(Test-Path $root){Remove-Item -LiteralPath $root -Recurse -Force}
}

Write-Host ("RECOVERY LAYOUT: {0}/{1} passed; {2} failed." -f
    $script:passed,($script:passed+$script:failed),$script:failed)
if($script:failed){throw 'Recovery-layout tests failed.'}
