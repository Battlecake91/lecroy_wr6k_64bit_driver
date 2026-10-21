<# Hardware-independent tests for the production Device Guard predicate. #>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) `
    'scripts\deviceguard-preflight.ps1')

$passed = 0
$failed = 0
function Test-Case {
    param([string]$Name, [scriptblock]$Body)
    try {
        & $Body
        Write-Host "[PASS] $Name"
        $script:passed++
    }
    catch {
        Write-Host "[FAIL] $Name`: $($_.Exception.Message)"
        $script:failed++
    }
}
function New-State {
    param(
        [AllowNull()][object]$Vbs = 0,
        [AllowNull()][AllowEmptyCollection()][object]$Configured = @(0),
        [AllowNull()][AllowEmptyCollection()][object]$Running = @(0),
        [AllowNull()][object]$KernelCi = 0,
        [AllowNull()][object]$UserCi = 0
    )
    [pscustomobject]@{
        VirtualizationBasedSecurityStatus = $Vbs
        SecurityServicesConfigured = $Configured
        SecurityServicesRunning = $Running
        CodeIntegrityPolicyEnforcementStatus = $KernelCi
        UsermodeCodeIntegrityPolicyEnforcementStatus = $UserCi
    }
}
function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

Test-Case 'zero sentinels are inactive and preserved' {
    $raw = New-State
    $result = Get-LecS65DeviceGuardEvaluation $raw @()
    Assert-True ($result.Allowed -and $result.State -eq 'Inactive') 'zero baseline was rejected'
    Assert-True ($result.Evaluated.RunningServices.Count -eq 1 -and
        $result.Evaluated.RunningServices[0] -eq 0) 'zero running sentinel was lost'
}
Test-Case 'empty service arrays are inactive' {
    $result = Get-LecS65DeviceGuardEvaluation (New-State -Configured @() -Running @()) @()
    Assert-True ($result.Allowed -and $result.State -eq 'Inactive') 'empty vectors were rejected'
}
Test-Case 'null policy values fail closed as unknown' {
    $result = Get-LecS65DeviceGuardEvaluation (New-State -Running $null) @()
    Assert-True (-not $result.Allowed -and $result.State -eq 'Unknown') 'null did not fail closed'
    Assert-True (@($result.UnknownReasons | Where-Object {
        $_ -match 'SecurityServicesRunning is null'
    }).Count -eq 1) 'null reason missing'
}
Test-Case 'absent Device Guard data fails closed' {
    $result = Get-LecS65DeviceGuardEvaluation $null @()
    Assert-True (-not $result.Allowed -and $result.State -eq 'Unknown') 'absent data did not fail closed'
}
Test-Case 'known active HVCI is rejected' {
    $result = Get-LecS65DeviceGuardEvaluation `
        (New-State -Vbs 2 -Configured @(2) -Running @(2)) @()
    Assert-True (-not $result.Allowed -and $result.State -eq 'Active') 'active HVCI was allowed'
}
Test-Case 'unknown numeric values fail closed' {
    $result = Get-LecS65DeviceGuardEvaluation (New-State -Running @(99)) @()
    Assert-True (-not $result.Allowed -and $result.State -eq 'Contradictory') 'unknown value was allowed'
    Assert-True (@($result.UnknownReasons | Where-Object {
        $_ -match 'unknown value 99'
    }).Count -eq 1) 'unknown value was not preserved'
}
Test-Case 'contradictory running state is rejected' {
    $result = Get-LecS65DeviceGuardEvaluation `
        (New-State -Vbs 0 -Configured @(0) -Running @(2)) @()
    Assert-True (-not $result.Allowed -and $result.State -eq 'Contradictory') 'contradiction was allowed'
}
Test-Case 'configured but inactive state is distinguished' {
    $result = Get-LecS65DeviceGuardEvaluation `
        (New-State -Vbs 1 -Configured @(2) -Running @(0) -KernelCi 1) @()
    Assert-True ($result.Allowed -and $result.State -eq 'ConfiguredNotBlocking') `
        'configured nonblocking state was not distinguished'
}
Test-Case 'enforced kernel CI policy is rejected' {
    $result = Get-LecS65DeviceGuardEvaluation (New-State -KernelCi 2) @()
    Assert-True (-not $result.Allowed -and $result.State -eq 'Active') 'enforced CI was allowed'
}
Test-Case 'active CI policy file is rejected and retained' {
    $result = Get-LecS65DeviceGuardEvaluation (New-State) @('C:\Policy\active.p7b')
    Assert-True (-not $result.Allowed -and $result.State -eq 'Active') 'active policy file was allowed'
    Assert-True ($result.Evaluated.ActiveCiPolicyFiles[0] -eq 'C:\Policy\active.p7b') `
        'active policy identity was lost'
}

Write-Host "DEVICEGUARD PREFLIGHT: $passed/$($passed + $failed) passed; $failed failed."
if ($failed -ne 0) { exit 1 }
exit 0
