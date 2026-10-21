<#
.SYNOPSIS
  Evaluate Device Guard and Code Integrity state for a test-signed driver gate.
.DESCRIPTION
  This file is side-effect free. The caller supplies previously collected
  Win32_DeviceGuard and active-policy-file values. The returned object retains
  those values and explains the fail-closed decision.
#>

Set-StrictMode -Version Latest

function Get-LecS65PolicyProperty {
    param(
        [AllowNull()][object]$InputObject,
        [Parameter(Mandatory=$true)][string]$Name,
        [switch]$Vector
    )

    $property = if ($null -ne $InputObject) {
        $InputObject.PSObject.Properties[$Name]
    } else { $null }
    $present = $null -ne $property
    # Inspect PSPropertyInfo.Value directly. Assigning an empty array to a
    # normal PowerShell variable enumerates it and collapses it to $null.
    $isNull = -not $present -or $null -eq $property.Value
    $values = if ($isNull) { @() } else { @($property.Value) }
    [pscustomobject][ordered]@{
        Name = $Name
        Present = $present
        IsNull = $isNull
        IsVector = [bool]$Vector
        Values = @($values)
    }
}

function ConvertTo-LecS65PolicyIntegers {
    param(
        [Parameter(Mandatory=$true)][object]$Property,
        [Parameter(Mandatory=$true)][int[]]$KnownValues,
        [Parameter(Mandatory=$true)][AllowEmptyCollection()]
        [System.Collections.Generic.List[string]]$Reasons
    )

    if (-not $Property.Present) {
        $Reasons.Add("$($Property.Name) is absent.")
        return @()
    }
    if ($Property.IsNull) {
        $Reasons.Add("$($Property.Name) is null.")
        return @()
    }
    if ($Property.Values.Count -eq 0) {
        if (-not $Property.IsVector) {
            $Reasons.Add("$($Property.Name) is empty.")
        }
        return @()
    }

    $result = @()
    foreach ($value in $Property.Values) {
        $parsed = 0
        if (-not [int]::TryParse([string]$value, [ref]$parsed)) {
            $Reasons.Add("$($Property.Name) contains nonnumeric value '$value'.")
            continue
        }
        $result += $parsed
        if ($KnownValues -notcontains $parsed) {
            $Reasons.Add("$($Property.Name) contains unknown value $parsed.")
        }
    }
    return @($result)
}

function Get-LecS65DeviceGuardEvaluation {
    [CmdletBinding()]
    param(
        [AllowNull()][object]$DeviceGuard,
        [AllowNull()][object[]]$ActiveCiPolicyFiles
    )

    $reasons = New-Object 'System.Collections.Generic.List[string]'
    $blocking = New-Object 'System.Collections.Generic.List[string]'
    $contradictions = New-Object 'System.Collections.Generic.List[string]'

    $vbsProperty = Get-LecS65PolicyProperty $DeviceGuard `
        'VirtualizationBasedSecurityStatus'
    $configuredProperty = Get-LecS65PolicyProperty $DeviceGuard `
        'SecurityServicesConfigured' -Vector
    $runningProperty = Get-LecS65PolicyProperty $DeviceGuard `
        'SecurityServicesRunning' -Vector
    $kernelCiProperty = Get-LecS65PolicyProperty $DeviceGuard `
        'CodeIntegrityPolicyEnforcementStatus'
    $userCiProperty = Get-LecS65PolicyProperty $DeviceGuard `
        'UsermodeCodeIntegrityPolicyEnforcementStatus'

    $vbs = @(ConvertTo-LecS65PolicyIntegers $vbsProperty @(0,1,2) $reasons)
    $configured = @(ConvertTo-LecS65PolicyIntegers $configuredProperty @(0,1,2,3,4) $reasons)
    $running = @(ConvertTo-LecS65PolicyIntegers $runningProperty @(0,1,2,3,4) $reasons)
    $kernelCi = @(ConvertTo-LecS65PolicyIntegers $kernelCiProperty @(0,1,2) $reasons)
    $userCi = @(ConvertTo-LecS65PolicyIntegers $userCiProperty @(0,1,2) $reasons)

    $configuredNonzero = @($configured | Where-Object { $_ -ne 0 } | Select-Object -Unique)
    $runningNonzero = @($running | Where-Object { $_ -ne 0 } | Select-Object -Unique)
    $activePolicies = @($ActiveCiPolicyFiles | Where-Object {
        $null -ne $_ -and -not [string]::IsNullOrWhiteSpace([string]$_)
    })

    if ($vbs.Count -eq 1 -and $vbs[0] -eq 2) {
        $blocking.Add('Virtualization-based security is running.')
    }
    if ($runningNonzero.Count -ne 0) {
        $blocking.Add("Device Guard security services are running: $($runningNonzero -join ',').")
    }
    if ($kernelCi.Count -eq 1 -and $kernelCi[0] -eq 2) {
        $blocking.Add('Kernel Code Integrity policy enforcement is active.')
    }
    if ($userCi.Count -eq 1 -and $userCi[0] -eq 2) {
        $blocking.Add('User-mode Code Integrity policy enforcement is active.')
    }
    if ($activePolicies.Count -ne 0) {
        $blocking.Add("Active Code Integrity policy files were found: $($activePolicies.Count).")
    }

    if ($runningNonzero.Count -ne 0 -and ($vbs.Count -ne 1 -or $vbs[0] -ne 2)) {
        $contradictions.Add('Running security services contradict the VBS status.')
    }
    foreach ($service in $runningNonzero) {
        if ($configuredNonzero -notcontains $service) {
            $contradictions.Add("Running service $service is not reported as configured.")
        }
    }
    if ($vbs.Count -eq 1 -and $vbs[0] -eq 0 -and $configuredNonzero.Count -ne 0) {
        $contradictions.Add('Configured security services contradict disabled VBS status.')
    }

    $unknown = $reasons.Count -ne 0
    $state = if ($contradictions.Count -ne 0) {
        'Contradictory'
    } elseif ($unknown) {
        'Unknown'
    } elseif ($blocking.Count -ne 0) {
        'Active'
    } elseif (($vbs.Count -eq 1 -and $vbs[0] -eq 1) -or
        $configuredNonzero.Count -ne 0 -or
        ($kernelCi.Count -eq 1 -and $kernelCi[0] -eq 1) -or
        ($userCi.Count -eq 1 -and $userCi[0] -eq 1)) {
        'ConfiguredNotBlocking'
    } else {
        'Inactive'
    }

    [pscustomobject][ordered]@{
        SchemaVersion = 1
        Allowed = $state -in @('Inactive','ConfiguredNotBlocking')
        State = $state
        BlockingReasons = @($blocking)
        Contradictions = @($contradictions)
        UnknownReasons = @($reasons)
        Evaluated = [ordered]@{
            VbsStatus = @($vbs)
            ConfiguredServices = @($configured)
            RunningServices = @($running)
            KernelCiStatus = @($kernelCi)
            UserCiStatus = @($userCi)
            ActiveCiPolicyFiles = @($activePolicies)
        }
        Raw = [ordered]@{
            DeviceGuard = $DeviceGuard
            ActiveCiPolicyFiles = @($ActiveCiPolicyFiles)
            Properties = @(
                $vbsProperty, $configuredProperty, $runningProperty,
                $kernelCiProperty, $userCiProperty
            )
        }
    }
}
