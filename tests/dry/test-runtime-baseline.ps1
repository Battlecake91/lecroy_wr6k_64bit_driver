[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script = Join-Path $repo 'scripts\test-runtime-baseline.ps1'
$host51 = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
. (Join-Path $repo 'scripts\native-process.ps1')
$root = Join-Path ([System.IO.Path]::GetTempPath()) `
    ('lec-runtime-baseline-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$script:passed = 0
$script:failed = 0

function Invoke-Case {
    param([string]$Name, [object[]]$Devices, [bool]$Acknowledge, [int]$ExpectedExit)
    $stem = ($Name -replace '[^A-Za-z0-9]', '-')
    $state = Join-Path $root "$stem-state.json"
    $result = Join-Path $root "$stem-result.json"
    @{ Devices = $Devices } | ConvertTo-Json -Depth 8 |
        Set-Content -LiteralPath $state -Encoding UTF8
    $args = @('-NoLogo','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass',
        '-File',$script,'-ExpectedInf','oem101.inf',
        '-ExpectedVersion','0.2026.1018.1',
        '-ExpectedSysHash',('A' * 64),'-PreviousLocation','PCI bus 4, device 1, function 0',
        '-MockStatePath',$state,'-OutputPath',$result)
    if ($Acknowledge) { $args += '-AcknowledgeLocationChange' }
    # Expected fail-closed cases intentionally write a terminating exception to
    # stderr. Keep that native stream as data so an enclosing workflow with
    # ErrorActionPreference=Stop cannot convert the expected child failure into
    # a parent PowerShell exception.
    $process = Invoke-LecS65NativeProcess -Name "runtime baseline: $Name" `
        -FilePath $host51 -Arguments $args -WorkingDirectory $repo `
        -TimeoutSeconds 30 -ReturnOnNonZeroExit
    $exitCode = $process.ExitCode
    $ok = $exitCode -eq $ExpectedExit -and (Test-Path -LiteralPath $result)
    $record = Get-Content -Raw -LiteralPath $result | ConvertFrom-Json
    if ($ExpectedExit -eq 0) { $ok = $ok -and $record.Outcome -eq 'PASS' }
    else { $ok = $ok -and $record.Outcome -eq 'FAIL' }
    Write-Host ("[{0}] {1}" -f $(if ($ok) {'PASS'} else {'FAIL'}), $Name)
    if ($ok) { ++$script:passed } else { ++$script:failed }
}

function New-Device([string]$Instance, [string]$Location, [int]$Problem = 0) {
    [pscustomobject]@{
        InstanceId = $Instance
        HardwareIds = @('PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00')
        Inf = 'oem101.inf'; Version = '0.2026.1018.1'
        ProblemCode = $Problem; DeviceStatus = $(if ($Problem) {'Error'} else {'OK'})
        ServiceName = 'LecS65AcqDrv'; ServiceStatus = $(if ($Problem) {'Stopped'} else {'Running'})
        SysHash = ('A' * 64); Location = $Location
        Interfaces = @(1..5 | ForEach-Object { [pscustomobject]@{ ClassGuid = "guid-$_"; Published = $true } })
    }
}

$instance = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0'
$same = New-Device $instance 'PCI bus 4, device 1, function 0'
$moved = New-Device $instance 'PCI bus 5, device 1, function 0'
Invoke-Case 'stable identity and location pass' @($same) $false 0
Invoke-Case 'BDF change fails pending acknowledgement' @($moved) $false 1
Invoke-Case 'acknowledged BDF change retains stable identity' @($moved) $true 0
Invoke-Case 'wrong instance fails closed' @((New-Device ($instance + '-other') $same.Location)) $false 1
Invoke-Case 'duplicate hardware matches fail closed' @($same,$moved) $false 1
Invoke-Case 'unhealthy device fails closed' @((New-Device $instance $same.Location 52)) $false 1

Remove-Item -LiteralPath $root -Recurse -Force
Write-Host ("RUNTIME BASELINE TESTS: {0}/{1} passed; {2} failed." -f
    $script:passed, ($script:passed + $script:failed), $script:failed)
if ($script:failed) { throw 'Runtime baseline tests failed.' }
