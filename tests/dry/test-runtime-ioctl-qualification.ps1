<# End-to-end mock qualification for the permanent runtime harness. #>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$harness = Join-Path $repo 'scripts\invoke-runtime-ioctl-qualification.ps1'
$source = Join-Path $repo 'tools\lecdiag\lecdiag.c'
$mockSource = Join-Path $PSScriptRoot 'lecdiag-mock-device.c'
$mockHeader = Join-Path $PSScriptRoot 'lecdiag-mock-device.h'
$ps51 = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
$root = Join-Path ([IO.Path]::GetTempPath()) `
    ('lec-runtime-harness-' + [guid]::NewGuid().ToString('N'))
$mockExe = Join-Path $root 'lecdiag-mock.exe'
$passed = 0
$failed = 0
$interfaceClasses = @(
    '{958695A4-693A-435E-8297-66F805D8E46A}',
    '{7AC34BE9-F766-4F15-9E88-854BA5E2146E}',
    '{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}',
    '{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}',
    '{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}'
)

function Write-JsonEncoding([string]$Path, [object]$Value, [bool]$Bom) {
    $encoding = New-Object System.Text.UTF8Encoding($Bom)
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), $encoding)
}

function New-MockState {
    param(
        [string]$Location = 'PCI bus 4, device 1, function 0',
        [string]$InstanceId = 'PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0',
        [AllowNull()][object]$Version = '0.2026.1018.1',
        [int]$ProblemCode = 0,
        [object]$DeviceGuard,
        [object[]]$Clients = @(),
        [int]$MissingInterface = -1,
        [int]$DuplicateInterface = -1,
        [string]$SysHash = '8E88F8A1CD60212FCCCB0CA9D894D76C6109A118E965041D02703BFAA8DB2D2A',
        [bool]$TestSigning = $true,
        [bool]$SecureBoot = $false
    )
    if ($null -eq $DeviceGuard) {
        $DeviceGuard = [pscustomobject]@{
            VirtualizationBasedSecurityStatus=0
            SecurityServicesConfigured=@(0)
            SecurityServicesRunning=@(0)
            CodeIntegrityPolicyEnforcementStatus=0
            UsermodeCodeIntegrityPolicyEnforcementStatus=0
        }
    }
    $interfaces = @()
    for ($i=0; $i -lt $interfaceClasses.Count; ++$i) {
        if ($i -eq $MissingInterface) { continue }
        $interfaces += [pscustomobject]@{
            ClassGuid=$interfaceClasses[$i]; Count=if ($i -eq $DuplicateInterface) { 2 } else { 1 }
            InstanceMatches=1; Published=$true
        }
    }
    $germanLocation = [string]::Concat('PCI-Bus 5, Ger',[char]0x00E4,'t 1, Funktion 0')
    if ($Location -eq 'GERMAN') { $Location = $germanLocation }
    [pscustomobject]@{
        SchemaVersion=1; MockMode=$true
        Host=[pscustomobject]@{ BuildNumber=19045; Is64BitOperatingSystem=$true }
        Security=[pscustomobject]@{ TestSigning=$TestSigning; SecureBootEnabled=$SecureBoot
            DeviceGuard=$DeviceGuard; ActiveCiPolicyFiles=@() }
        Devices=@([pscustomobject]@{
            InstanceId=$InstanceId
            HardwareIds=@('PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00')
            Inf='oem101.inf'; Version=$Version; ProblemCode=$ProblemCode
            DeviceStatus=if ($ProblemCode -eq 0) { 'OK' } else { 'Error' }
            ServiceName='LecS65AcqDrv'; ServiceStatus=if ($ProblemCode -eq 0) { 'Running' } else { 'Stopped' }
            SysHash=$SysHash; Location=$Location; Interfaces=@($interfaces)
        })
        BlockedClients=@($Clients)
        Rollback=[pscustomobject]@{
            CAT=[pscustomobject]@{ Matches=$true }
            INF=[pscustomobject]@{ Matches=$true }
            SYS=[pscustomobject]@{ Matches=$true }
        }
    }
}

function Test-Case([string]$Name, [scriptblock]$Body) {
    try { & $Body; Write-Host "[PASS] $Name"; $script:passed++ }
    catch { Write-Host "[FAIL] $Name`: $($_.Exception.Message)"; $script:failed++ }
}

function Invoke-HarnessCase {
    param(
        [string]$Name, [object]$State, [int]$ExpectedExit,
        [int]$ExpectedAttempts, [object]$Scenario = $null,
        [bool]$Bom = $false, [string]$HashOverride,
        [ValidateSet('All','Batch3Remaining')][string]$CommandSet = 'All'
    )
    $statePath = Join-Path $root ($Name + '-state.json')
    $scenarioPath = Join-Path $root ($Name + '-scenario.json')
    $evidence = Join-Path $root ($Name + '-evidence')
    Write-JsonEncoding $statePath $State $Bom
    $arguments = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$harness,
        '-Mode','OfflineMock','-EvidenceRoot',$evidence,'-MockStatePath',$statePath,
        '-MockExecutablePath',$mockExe,'-MockExecutableSha256',
        $(if ($HashOverride) { $HashOverride } else { $script:mockHash }),
        '-CommandSet',$CommandSet)
    if ($null -ne $Scenario) {
        Write-JsonEncoding $scenarioPath $Scenario $false
        $arguments += @('-MockScenarioPath',$scenarioPath)
    }
    $output = @(& $ps51 @arguments 2>&1)
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne $ExpectedExit) {
        throw "exit=$exitCode expected=$ExpectedExit output=$($output -join ' ')"
    }
    if (-not (Test-Path $evidence)) { throw 'evidence directory was not created' }
    $attempts = @(Get-ChildItem $evidence -Filter '*.attempted')
    if ($attempts.Count -ne $ExpectedAttempts) {
        throw "attempts=$($attempts.Count) expected=$ExpectedAttempts"
    }
    $outcome = Get-Content (Join-Path $evidence 'outcome.json') -Raw | ConvertFrom-Json
    if (($ExpectedExit -eq 0) -ne ($outcome.Outcome -eq 'PASS')) {
        throw "outcome mismatch: $($outcome.Outcome)"
    }
    [pscustomobject]@{ Evidence=$evidence; Outcome=$outcome; Output=$output }
}

New-Item -ItemType Directory -Path $root | Out-Null
try {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vs) { throw 'Visual Studio C++ tools not found.' }
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    $build = "`"$vcvars`" >nul && cd /d `"$root`" && cl /nologo /W4 /WX /O2 /D_CRT_SECURE_NO_WARNINGS /FI`"$mockHeader`" /Fe:`"$mockExe`" `"$source`" `"$mockSource`""
    & $env:ComSpec /d /s /c $build
    if ($LASTEXITCODE -ne 0) { throw 'Mock build failed.' }
    $script:mockHash = (Get-FileHash -Algorithm SHA256 $mockExe).Hash

    Test-Case 'complete production flow passes once with UTF-8 no BOM and German metadata' {
        $result = Invoke-HarnessCase 'pass-no-bom' (New-MockState -Location 'GERMAN') 0 7
        $context = Get-Content (Join-Path $result.Evidence 'context.json') -Raw | ConvertFrom-Json
        if (@($context.ApprovedCommands).Count -ne 7) { throw 'allowlist count is not seven' }
        $calls = @(Get-ChildItem $result.Evidence -Filter '*.stdout.txt' | ForEach-Object {
            Get-Content $_.FullName | Where-Object { $_ -like 'MOCK_DEVICE_IO_CONTROL *' }
        })
        if ($calls.Count -ne 8) { throw "mock IOCTL calls=$($calls.Count), expected=8" }
        if (@($calls | Where-Object { $_ -like '*ioctl=0x00222C00 input=4 output=0 input-hex=00000000*' }).Count -ne 1) {
            throw 'four-byte zero-delay request was not exact'
        }
    }
    Test-Case 'UTF-8 BOM and BDF 5:1.0 pass with unchanged identity' {
        [void](Invoke-HarnessCase 'pass-bom-bdf5' (New-MockState -Location 'GERMAN') 0 7 $null $true)
    }
    Test-Case 'BDF 4:1.0 passes with unchanged identity' {
        [void](Invoke-HarnessCase 'pass-bdf4' (New-MockState -Location 'PCI bus 4, device 1, function 0') 0 7)
    }
    Test-Case 'Batch 3 resume runs only R0-8, R0-9 and conditional R1' {
        $result = Invoke-HarnessCase 'batch3-resume' (New-MockState -Location 'GERMAN') `
            0 4 $null $false $null 'Batch3Remaining'
        $context = Get-Content (Join-Path $result.Evidence 'context.json') -Raw | ConvertFrom-Json
        $names = @($context.ApprovedCommands | ForEach-Object Name)
        if (($names -join ',') -ne 'r0-8-register-size,r0-9-delay-zero,r1-pci,r1-start-register') {
            throw "unexpected Batch 3 allowlist: $($names -join ',')"
        }
        $calls = @(Get-ChildItem $result.Evidence -Filter '*.stdout.txt' | ForEach-Object {
            Get-Content $_.FullName | Where-Object { $_ -like 'MOCK_DEVICE_IO_CONTROL *' }
        })
        if ($calls.Count -ne 5) { throw "Batch 3 mock IOCTL calls=$($calls.Count), expected=5" }
        if (@($calls | Where-Object { $_ -match '0x00222400|0xCFDC2184|0x00223004' }).Count -ne 0) {
            throw 'Batch 3 resume repeated a completed R0 command'
        }
    }
    Test-Case 'Batch 3 R0-8 failure blocks R0-9 and both R1 commands' {
        $scenario=[pscustomobject]@{Behaviors=@([pscustomobject]@{
            Name='r0-8-register-size';Type='Fail';Ioctl='00223040'})}
        [void](Invoke-HarnessCase 'batch3-r0-fail' (New-MockState) 1 1 $scenario `
            $false $null 'Batch3Remaining')
    }
    Test-Case 'changed PCI instance ID fails before commands' {
        [void](Invoke-HarnessCase 'bad-instance' (New-MockState -InstanceId 'PCI\WRONG') 1 0)
    }
    Test-Case 'missing PnP version fails before commands' {
        [void](Invoke-HarnessCase 'missing-property' (New-MockState -Version $null) 1 0)
    }
    Test-Case 'active Device Guard fails before commands' {
        $dg=[pscustomobject]@{VirtualizationBasedSecurityStatus=2;SecurityServicesConfigured=@(2);
            SecurityServicesRunning=@(2);CodeIntegrityPolicyEnforcementStatus=0;
            UsermodeCodeIntegrityPolicyEnforcementStatus=0}
        [void](Invoke-HarnessCase 'active-dg' (New-MockState -DeviceGuard $dg) 1 0)
    }
    Test-Case 'unknown Device Guard fails before commands' {
        $dg=[pscustomobject]@{VirtualizationBasedSecurityStatus=99;SecurityServicesConfigured=@(0);
            SecurityServicesRunning=@(0);CodeIntegrityPolicyEnforcementStatus=0;
            UsermodeCodeIntegrityPolicyEnforcementStatus=0}
        [void](Invoke-HarnessCase 'unknown-dg' (New-MockState -DeviceGuard $dg) 1 0)
    }
    Test-Case 'contradictory Device Guard fails before commands' {
        $dg=[pscustomobject]@{VirtualizationBasedSecurityStatus=0;SecurityServicesConfigured=@(0);
            SecurityServicesRunning=@(2);CodeIntegrityPolicyEnforcementStatus=0;
            UsermodeCodeIntegrityPolicyEnforcementStatus=0}
        [void](Invoke-HarnessCase 'contradictory-dg' (New-MockState -DeviceGuard $dg) 1 0)
    }
    Test-Case 'wrong SYS hash fails before commands' {
        [void](Invoke-HarnessCase 'bad-sys' (New-MockState -SysHash ('0' * 64)) 1 0)
    }
    Test-Case 'wrong diagnostic hash fails before commands' {
        [void](Invoke-HarnessCase 'bad-exe' (New-MockState) 1 0 $null $false ('0' * 64))
    }
    Test-Case 'problem code 52 fails before commands' {
        [void](Invoke-HarnessCase 'code52' (New-MockState -ProblemCode 52) 1 0)
    }
    Test-Case 'missing interface class fails before commands' {
        [void](Invoke-HarnessCase 'missing-interface' (New-MockState -MissingInterface 2) 1 0)
    }
    Test-Case 'duplicated interface class fails before commands' {
        [void](Invoke-HarnessCase 'duplicate-interface' (New-MockState -DuplicateInterface 3) 1 0)
    }
    Test-Case 'acquisition client fails before commands' {
        $client=[pscustomobject]@{Id=42;Name='XStream';Path='C:\Program Files\LeCroy\XStream.exe'}
        [void](Invoke-HarnessCase 'client-running' (New-MockState -Clients @($client)) 1 0)
    }
    Test-Case 'R0 process failure blocks remaining R0 and all R1' {
        $scenario=[pscustomobject]@{Behaviors=@([pscustomobject]@{
            Name='r0-7-trace-size';Type='Fail';Ioctl='00223004'})}
        [void](Invoke-HarnessCase 'r0-fail' (New-MockState) 1 3 $scenario)
    }
    Test-Case 'first R1 failure prevents second R1' {
        $scenario=[pscustomobject]@{Behaviors=@([pscustomobject]@{
            Name='r1-pci';Type='Fail';Ioctl='80006014'})}
        [void](Invoke-HarnessCase 'r1-fail' (New-MockState) 1 6 $scenario)
    }
    Test-Case 'timeout is bounded and never retried' {
        $scenario=[pscustomobject]@{Behaviors=@([pscustomobject]@{
            Name='r0-5-noop';Type='Timeout';Ioctl='00222400'})}
        [void](Invoke-HarnessCase 'timeout' (New-MockState) 1 1 $scenario)
    }
    Test-Case 'missing output stops at exact command' {
        $scenario=[pscustomobject]@{Behaviors=@([pscustomobject]@{
            Name='r0-7-trace-size';Type='MissingOutput';Ioctl='00223004'})}
        [void](Invoke-HarnessCase 'missing-output' (New-MockState) 1 3 $scenario)
    }
    Test-Case 'existing evidence directory is rejected without overwrite' {
        $statePath=Join-Path $root 'existing-state.json'; Write-JsonEncoding $statePath (New-MockState) $false
        $evidence=Join-Path $root 'existing-evidence'; New-Item -ItemType Directory $evidence | Out-Null
        $sentinel=Join-Path $evidence 'sentinel.txt'; [IO.File]::WriteAllText($sentinel,'keep')
        & $ps51 -NoProfile -ExecutionPolicy Bypass -File $harness -Mode OfflineMock `
            -EvidenceRoot $evidence -MockStatePath $statePath -MockExecutablePath $mockExe `
            -MockExecutableSha256 $script:mockHash *> $null
        if ($LASTEXITCODE -eq 0 -or (Get-Content $sentinel) -ne 'keep' -or
            @(Get-ChildItem $evidence).Count -ne 1) { throw 'existing evidence was not preserved' }
    }
}
finally {
    if (Test-Path $root) { Remove-Item $root -Recurse -Force }
}

Write-Host "RUNTIME QUALIFICATION HARNESS: $passed/$($passed + $failed) passed; $failed failed."
if ($failed -ne 0) { exit 1 }
exit 0
