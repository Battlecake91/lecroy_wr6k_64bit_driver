<# Focused exact-once and interruption tests for the staged runtime ledger. #>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ledger = Join-Path $repo 'scripts\invoke-runtime-ioctl-ledger.ps1'
$source = Join-Path $repo 'tools\lecdiag\lecdiag.c'
$mockSource = Join-Path $PSScriptRoot 'lecdiag-mock-device.c'
$mockHeader = Join-Path $PSScriptRoot 'lecdiag-mock-device.h'
$ps51 = "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe"
$root = Join-Path ([IO.Path]::GetTempPath()) ('lec-runtime-ledger-' + [guid]::NewGuid().ToString('N'))
$mockExe = Join-Path $root 'lecdiag-mock.exe'
$statePath = Join-Path $root 'state.json'
$passed = 0
$failed = 0
$sysHash = '73B235BD4D19354612752A87BA499C06D15336F63A6B73EDDC0C674C3086D908'
$interfaces = @(
    '{958695A4-693A-435E-8297-66F805D8E46A}',
    '{7AC34BE9-F766-4F15-9E88-854BA5E2146E}',
    '{8D1103B8-5BF4-4B5C-B21E-EEAACE97D418}',
    '{9007C2BC-EDFD-4F2F-A059-DF1131CB1AE5}',
    '{FC5DF040-D6CD-4BA0-B5E0-2561972963A2}'
)

function Write-Json([string]$Path, [object]$Value) {
    [IO.File]::WriteAllText($Path, (($Value | ConvertTo-Json -Depth 16) + "`r`n"),
        (New-Object Text.UTF8Encoding($false)))
}

function New-State([string]$Inf='oem103.inf', [string]$Version='0.2026.1024.1',
    [string]$Hash=$sysHash) {
    [pscustomobject]@{
        SchemaVersion=1; MockMode=$true
        Host=[pscustomobject]@{BuildNumber=19045;Is64BitOperatingSystem=$true}
        Security=[pscustomobject]@{
            TestSigning=$true;SecureBootEnabled=$false;ActiveCiPolicyFiles=@()
            DeviceGuard=[pscustomobject]@{
                VirtualizationBasedSecurityStatus=0;SecurityServicesConfigured=@(0)
                SecurityServicesRunning=@(0);CodeIntegrityPolicyEnforcementStatus=0
                UsermodeCodeIntegrityPolicyEnforcementStatus=0
            }
        }
        Devices=@([pscustomobject]@{
            InstanceId='PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00\4&39FD0A79&0&08F0'
            HardwareIds=@('PCI\VEN_1570&DEV_0005&SUBSYS_00000000&REV_00')
            Inf=$Inf;Version=$Version;ProblemCode=0;DeviceStatus='OK'
            ServiceName='LecS65AcqDrv';ServiceStatus='Running';SysHash=$Hash
            Location='PCI bus 5, device 1, function 0'
            Interfaces=@($interfaces | ForEach-Object {
                [pscustomobject]@{ClassGuid=$_;Count=1;InstanceMatches=1;Published=$true}
            })
        })
        BlockedClients=@()
        Rollback=[pscustomobject]@{
            CAT=[pscustomobject]@{Matches=$true};INF=[pscustomobject]@{Matches=$true}
            SYS=[pscustomobject]@{Matches=$true}
        }
    }
}

function Test-Case([string]$Name,[scriptblock]$Body) {
    try { & $Body; Write-Host "[PASS] $Name"; $script:passed++ }
    catch { Write-Host "[FAIL] $Name`: $($_.Exception.Message)"; $script:failed++ }
}

function Invoke-Ledger([string]$Evidence,[string]$Action,[switch]$Authorized,
    [string]$Scenario,[int]$ExpectedExit=0) {
    $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$ledger,'-Action',$Action,
        '-EvidenceRoot',$Evidence,'-TestMode','-MockStatePath',$statePath,
        '-MockExecutablePath',$mockExe,'-MockExecutableSha256',$script:mockHash,
        '-ExpectedInf','oem103.inf','-ExpectedVersion','0.2026.1024.1',
        '-ExpectedSysHash',$sysHash,'-ExpectedLecdiagHash',$script:mockHash)
    if ($Authorized) { $args += '-OwnerAuthorized' }
    if ($Scenario) { $args += @('-MockScenarioPath',$Scenario) }
    $old=$ErrorActionPreference
    try { $ErrorActionPreference='Continue'; $output=@(& $ps51 @args 2>&1); $code=$LASTEXITCODE }
    finally { $ErrorActionPreference=$old }
    if ($code -ne $ExpectedExit) { throw "exit=$code expected=$ExpectedExit output=$($output -join ' ')" }
    [pscustomobject]@{ExitCode=$code;Output=$output}
}

function Wait-Collect([string]$Evidence,[int]$ExpectedFinal=0) {
    $deadline=[datetime]::UtcNow.AddSeconds(20)
    do {
        $result=Invoke-Ledger $Evidence Collect -ExpectedExit 0
        if ($result.ExitCode -eq 0) { return $result }
        Start-Sleep -Milliseconds 100
    } while ([datetime]::UtcNow -lt $deadline)
    throw 'collect timeout'
}

function Complete-Next([string]$Evidence,[switch]$Authorized,[string]$Scenario) {
    [void](Invoke-Ledger $Evidence DispatchNext -Authorized:$Authorized -Scenario $Scenario)
    $deadline=[datetime]::UtcNow.AddSeconds(20)
    do {
        $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$ledger,'-Action','Collect',
            '-EvidenceRoot',$Evidence,'-TestMode')
        $old=$ErrorActionPreference
        try { $ErrorActionPreference='Continue'; $out=@(& $ps51 @args 2>&1); $code=$LASTEXITCODE }
        finally { $ErrorActionPreference=$old }
        if ($code -eq 0) { return $out }
        if ($code -ne 2) { throw "collect failed: $($out -join ' ')" }
        Start-Sleep -Milliseconds 100
    } while ([datetime]::UtcNow -lt $deadline)
    throw 'collect timeout'
}

New-Item -ItemType Directory -Path $root | Out-Null
try {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs=& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vcvars=Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    $build="`"$vcvars`" >nul && cd /d `"$root`" && cl /nologo /W4 /WX /O2 /D_CRT_SECURE_NO_WARNINGS /FI`"$mockHeader`" /Fe:`"$mockExe`" `"$source`" `"$mockSource`""
    & $env:ComSpec /d /s /c $build
    if ($LASTEXITCODE -ne 0) { throw 'mock build failed' }
    $script:mockHash=(Get-FileHash -Algorithm SHA256 $mockExe).Hash
    Write-Json $statePath (New-State)

    Test-Case 'outer stop before first dispatch records no hardware attempt' {
        $e=Join-Path $root 'before-first'; [void](Invoke-Ledger $e Create)
        $status=(Invoke-Ledger $e Status).Output -join "`n"
        if ($status -notmatch '"NextStage"\s*:\s*"preflight"') { throw 'preflight not pending' }
        if (@(Get-ChildItem (Join-Path $e 'dispatches')).Count -ne 0) { throw 'unexpected dispatch' }
    }

    Test-Case 'complete staged flow resumes and dispatches every command exactly once' {
        $e=Join-Path $root 'complete'; [void](Invoke-Ledger $e Create)
        [void](Complete-Next $e)
        for($i=0;$i -lt 11;$i++){ [void](Complete-Next $e -Authorized) }
        $status=(Invoke-Ledger $e Status).Output -join "`n"
        if ($status -notmatch '"Complete"\s*:\s*true') { throw 'ledger did not complete' }
        $attempts=@(Get-ChildItem $e -Recurse -Filter '*.attempted')
        if ($attempts.Count -ne 11) { throw "hardware attempts=$($attempts.Count)" }
    }

    Test-Case 'duplicate dispatch is rejected while child is unresolved' {
        $e=Join-Path $root 'duplicate'; [void](Invoke-Ledger $e Create)
        [void](Invoke-Ledger $e DispatchNext)
        [void](Invoke-Ledger $e DispatchNext -ExpectedExit 1)
        $deadline=[datetime]::UtcNow.AddSeconds(20)
        do {
            $old=$ErrorActionPreference
            try { $ErrorActionPreference='Continue'; & $ps51 -NoProfile -ExecutionPolicy Bypass -File $ledger -Action Collect -EvidenceRoot $e -TestMode *> $null; $code=$LASTEXITCODE }
            finally { $ErrorActionPreference=$old }
            if ($code -ne 2) { break }; Start-Sleep -Milliseconds 100
        } while([datetime]::UtcNow -lt $deadline)
        if ($code -ne 0) { throw "collect exit=$code" }
    }

    Test-Case 'changed executable is rejected before another dispatch' {
        $e=Join-Path $root 'changed-exe'; [void](Invoke-Ledger $e Create)
        [IO.File]::AppendAllText($mockExe,'x')
        try { [void](Invoke-Ledger $e DispatchNext -ExpectedExit 1) }
        finally {
            & $env:ComSpec /d /s /c $build | Out-Null
            $script:mockHash=(Get-FileHash -Algorithm SHA256 $mockExe).Hash
        }
    }

    Test-Case 'corrupted ledger fails closed' {
        $e=Join-Path $root 'corrupt'; [void](Invoke-Ledger $e Create)
        [IO.File]::WriteAllText((Join-Path $e 'plan.json'),'{bad')
        [void](Invoke-Ledger $e Status -ExpectedExit 1)
    }

    Test-Case 'changed driver identity fails preflight without hardware dispatch' {
        $e=Join-Path $root 'wrong-driver'; Write-Json $statePath (New-State -Inf 'oem999.inf')
        [void](Invoke-Ledger $e Create); [void](Invoke-Ledger $e DispatchNext)
        $deadline=[datetime]::UtcNow.AddSeconds(20)
        do {
            $old=$ErrorActionPreference
            try { $ErrorActionPreference='Continue'; & $ps51 -NoProfile -ExecutionPolicy Bypass -File $ledger -Action Collect -EvidenceRoot $e -TestMode *> $null; $code=$LASTEXITCODE }
            finally { $ErrorActionPreference=$old }
            if ($code -ne 2) { break }; Start-Sleep -Milliseconds 100
        } while([datetime]::UtcNow -lt $deadline)
        if ($code -ne 0) { throw "collect exit=$code" }
        $completion=Get-ChildItem $e -Recurse -Filter completion.json | ForEach-Object { Get-Content $_ -Raw|ConvertFrom-Json }
        if ($completion.Classification -ne 'NoHardwareDispatch') { throw 'preflight was not classified as no dispatch' }
        Write-Json $statePath (New-State)
    }

    Test-Case 'R0 failure is consumed and suppresses all later stages' {
        $scenario=Join-Path $root 'fail-r0.json'
        Write-Json $scenario ([pscustomobject]@{Behaviors=@([pscustomobject]@{Name='r0-1-build';Type='Fail';Ioctl='CFDC21C8'})})
        $e=Join-Path $root 'r0-fail'; [void](Invoke-Ledger $e Create -Scenario $scenario)
        [void](Complete-Next $e)
        [void](Invoke-Ledger $e DispatchNext -Authorized)
        $deadline=[datetime]::UtcNow.AddSeconds(20)
        do {
            $old=$ErrorActionPreference
            try { $ErrorActionPreference='Continue'; & $ps51 -NoProfile -ExecutionPolicy Bypass -File $ledger -Action Collect -EvidenceRoot $e -TestMode *> $null; $code=$LASTEXITCODE }
            finally { $ErrorActionPreference=$old }
            if ($code -ne 2) { break }; Start-Sleep -Milliseconds 100
        } while([datetime]::UtcNow -lt $deadline)
        if ($code -ne 1) { throw "failure collect exit=$code" }
        [void](Invoke-Ledger $e DispatchNext -Authorized -ExpectedExit 1)
        $later=@(Get-ChildItem $e -Recurse -Filter '*.attempted' | Where-Object Name -match 'r1')
        if ($later.Count -ne 0) { throw 'R1 was dispatched after R0 failure' }
    }

    Test-Case 'timeout is bounded, consumed and not retried' {
        $scenario=Join-Path $root 'timeout.json'
        Write-Json $scenario ([pscustomobject]@{Behaviors=@([pscustomobject]@{Name='r0-1-build';Type='Timeout';Ioctl='CFDC21C8'})})
        $e=Join-Path $root 'timeout'; [void](Invoke-Ledger $e Create -Scenario $scenario)
        [void](Complete-Next $e); [void](Invoke-Ledger $e DispatchNext -Authorized)
        Start-Sleep -Seconds 7
        [void](Invoke-Ledger $e Collect -ExpectedExit 1)
        [void](Invoke-Ledger $e DispatchNext -Authorized -ExpectedExit 1)
        if (@(Get-ChildItem $e -Recurse -Filter 'r0-1-build.attempted').Count -ne 1) { throw 'timeout was retried' }
    }

    Test-Case 'missing process evidence before hardware marker permits explicit no-dispatch recovery only' {
        $e=Join-Path $root 'after-dispatch-marker'; [void](Invoke-Ledger $e Create)
        $args=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$ledger,'-Action','DispatchNext',
            '-EvidenceRoot',$e,'-TestMode','-FailureInjection','AfterDispatchMarker')
        $old=$ErrorActionPreference
        try { $ErrorActionPreference='Continue'; & $ps51 @args *> $null; $code=$LASTEXITCODE }
        finally { $ErrorActionPreference=$old }
        if ($code -ne 97) { throw "injection exit=$code" }
        [void](Invoke-Ledger $e Collect)
        $completion=Get-ChildItem $e -Recurse -Filter completion.json | ForEach-Object {Get-Content $_ -Raw|ConvertFrom-Json}
        if ($completion.Classification -ne 'NoHardwareDispatch') { throw 'wrong classification' }
    }

    Test-Case 'source contains explicit downstream PnP and no-launch gates' {
        $capture=Get-Content (Join-Path $repo 'scripts\invoke-diagnostic-capture-install.ps1') -Raw
        $noLaunch=Get-Content (Join-Path $repo 'scripts\invoke-no-launch-preparation.ps1') -Raw
        if ($capture -notmatch 'AuthorizePnpRestart' -or $capture -notmatch 'RestartWithCapture') {
            throw 'PnP restart authorization gate missing'
        }
        if ($noLaunch -notmatch 'OwnerAuthorized' -or $noLaunch -notmatch 'ERROR_NOT_SUPPORTED') {
            throw 'no-launch authorization or closed-launch contract missing'
        }
    }
}
finally {
    if (Test-Path $root) { Remove-Item $root -Recurse -Force }
}

Write-Host "RUNTIME LEDGER: $passed/$($passed + $failed) passed; $failed failed."
if ($failed -ne 0) { exit 1 }
exit 0
