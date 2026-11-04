$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$buildScript = Join-Path $repo 'scripts\build-xstream-ioctl-trace.ps1'
$validator = Join-Path $repo 'scripts\validate-xstream-ioctl-capture.ps1'
$captureRunner = Join-Path $repo 'scripts\invoke-xstream-lossless-capture.ps1'
$waveformRunner = Join-Path $repo 'scripts\invoke-xstream-single-waveform.ps1'
$toolRoot = Join-Path $repo 'tools\xstream-ioctl-trace\build\x86'
$launcher = Join-Path $toolRoot 'xstream_trace_launcher.exe'
$hook = Join-Path $toolRoot 'xstream_io_hook.dll'
$targetSource = Join-Path $PSScriptRoot 'xstream-trace-synthetic-target.c'
$target = Join-Path $toolRoot 'xstream_trace_synthetic_target.exe'
$passed = 0
$failed = 0

[byte[]]$expectedRequest = New-Object byte[] 130
$expectedRequest[0]=6; $expectedRequest[2]=112; $expectedRequest[4]=3
$expectedRequest[6]=0xFB; $expectedRequest[7]=0xA5
$expectedRequest[8]=0x40; $expectedRequest[9]=2; $expectedRequest[10]=3
for ($i=11; $i -lt 120; $i++) {
    $expectedRequest[$i] = if ($i % 9 -eq 0) { 0 } else { $i -bxor 0xA5 }
}
$expectedRequest[120]=8; $expectedRequest[122]=2; $expectedRequest[124]=3
$expectedRequest[126]=0xFB; $expectedRequest[127]=0x85
$expectedRequest[128]=0x40; $expectedRequest[129]=0
$expectedSha = [Security.Cryptography.SHA256]::Create()
try {
    $expectedRequestHash = ([BitConverter]::ToString(
        $expectedSha.ComputeHash([byte[]]$expectedRequest))).Replace('-','')
}
finally { $expectedSha.Dispose() }

function Check([string]$Name,[bool]$Condition) {
    if ($Condition) { $script:passed++; Write-Host "PASS: $Name" }
    else { $script:failed++; Write-Host "FAIL: $Name" }
}

function Invoke-ExpectedFailure([scriptblock]$Action) {
    try { & $Action; return $false }
    catch { return $true }
}

& $buildScript | Out-Host

$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars32.bat'
$compile = '"' + $vcvars + '" >nul && cl /nologo /W4 /WX /O2 /MT ' +
    '/D_CRT_SECURE_NO_WARNINGS /Fe:"' + $target + '" "' + $targetSource + '"'
& $env:ComSpec /d /s /c $compile
if ($LASTEXITCODE -ne 0) { throw 'Synthetic trace target build failed.' }

$privateRoot = Join-Path ([IO.Path]::GetTempPath()) `
    ('lec-xstream-trace-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($privateRoot) | Out-Null
try {
    $trace = Join-Path $privateRoot 'capture.jsonl'
    $ready = Join-Path $privateRoot 'capture.ready'
    $dispatch = Join-Path $privateRoot 'target.dispatch'
    $result = Join-Path $privateRoot 'validation.json'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $dispatch
    Remove-Item Env:LECS65_EXPECT_EMBEDDING -ErrorAction SilentlyContinue

    & $launcher $target $trace $ready
    $launchExit = $LASTEXITCODE
    Check 'synthetic target completed through the production launcher' `
        ($launchExit -eq 0 -and (Test-Path $dispatch) -and (Test-Path $ready))

    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $trace -ResultPath $result `
        -PrivateEvidenceRoot $privateRoot -RepositoryRoot $repo `
        -ExpectedRequestCount 6 -ExpectedRequestHash $expectedRequestHash
    $validationExit = $LASTEXITCODE
    $summary = Get-Content -Raw -LiteralPath $result | ConvertFrom-Json
    Check 'six complete 130-byte requests validate' `
        ($validationExit -eq 0 -and [int]$summary.RequestCount -eq 6)
    Check 'binary request including NUL bytes is preserved without truncation' `
        ($summary.AllRequestsIdentical -eq $true -and
         @($summary.UniqueRequestHashes).Count -eq 1 -and
         [string]$summary.UniqueRequestHashes[0] -eq $expectedRequestHash)
    Check 'concurrent request correlation retains multiple threads and handles' `
        (@($summary.Requests.ThreadId | Sort-Object -Unique).Count -ge 2 -and
         @($summary.Requests.Handle | Sort-Object -Unique).Count -ge 2)
    Check 'timestamps, sequence and error response metadata are retained' `
        (@($summary.Requests | Where-Object {
            [string]::IsNullOrWhiteSpace($_.TimestampUtc) -or
            [int64]$_.Sequence -le 0 -or $null -eq $_.LastError
        }).Count -eq 0)

    $jsonRecords = @(Get-Content -LiteralPath $trace | ForEach-Object {
        if (-not [string]::IsNullOrWhiteSpace($_)) { $_ | ConvertFrom-Json }
    })
    $targets = @($jsonRecords | Where-Object {
        $_.type -eq 'ioctl' -and $_.ioctl -eq '0xCFDC2110'
    })
    Check 'production records carry exact lengths, family and opcode' `
        (@($targets | Where-Object {
            [int]$_.input_captured -ne 130 -or [bool]$_.input_truncated -or
            [int]$_.output_length -ne 14 -or [int]$_.family -ne 2 -or
            [int]$_.opcode -ne 3
        }).Count -eq 0)
    $successOutput = @($jsonRecords | Where-Object {
        $_.type -eq 'ioctl' -and $_.ioctl -eq '0x0009003C' -and
        [bool]$_.success -and [int]$_.output_captured -eq 2 -and
        -not [bool]$_.output_truncated
    })
    Check 'successful output capture remains complete' ($successOutput.Count -eq 1)
    Check 'trace footer proves normal completion without write failure' `
        (@($jsonRecords | Where-Object {
            $_.type -eq 'trace_footer' -and -not [bool]$_.write_failed
        }).Count -eq 1)

    $secondDispatch = Join-Path $privateRoot 'second.dispatch'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $secondDispatch
    $savedPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    & $launcher $target $trace $ready 2>$null
    $existingExit = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    Check 'existing trace and marker fail before target dispatch' `
        ($existingExit -ne 0 -and -not (Test-Path $secondDispatch))

    $missingRoot = Join-Path $privateRoot 'missing\child'
    $missingTrace = Join-Path $missingRoot 'capture.jsonl'
    $missingReady = Join-Path $missingRoot 'capture.ready'
    $thirdDispatch = Join-Path $privateRoot 'third.dispatch'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $thirdDispatch
    $ErrorActionPreference = 'Continue'
    & $launcher $target $missingTrace $missingReady 2>$null
    $missingExit = $LASTEXITCODE
    $ErrorActionPreference = $savedPreference
    Check 'capture-open failure stops before target dispatch' `
        ($missingExit -ne 0 -and -not (Test-Path $thirdDispatch))

    $embeddingTrace = Join-Path $privateRoot 'embedding.jsonl'
    $embeddingReady = Join-Path $privateRoot 'embedding.ready'
    $embeddingDispatch = Join-Path $privateRoot 'embedding.dispatch'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $embeddingDispatch
    $env:LECS65_EXPECT_EMBEDDING = '1'
    & $launcher --embedding $target $embeddingTrace $embeddingReady
    Check 'fixed embedding mode is forwarded by the production launcher' `
        ($LASTEXITCODE -eq 0 -and (Test-Path $embeddingDispatch) -and
         (Select-String -LiteralPath $embeddingReady -Pattern 'Embedding=true' -Quiet))

    $orchestrationRoot = Join-Path $privateRoot 'orchestration'
    $orchestrationDispatch = Join-Path $privateRoot 'orchestration.dispatch'
    $mockState = Join-Path $privateRoot 'waveform-state.json'
    [ordered]@{
        Driver=[ordered]@{Inf='oem-mock.inf';Version='0.0.0.1';ProblemCode=0
            DeviceStatus='OK';ServiceStatus='Running';SysHash=('6'*64)}
        Disks=@([ordered]@{Model='HFM256GDHTNG-8510B'
            Serial='ACE4_2E00_9590_A2AA.';Status='OK'})
        AcquisitionClients=@();MockWaveformObserved=$true
        MockReportedSamples=16;MockReturnedValues=16
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $mockState -Encoding UTF8
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $orchestrationDispatch
    $env:LECS65_EXPECT_EMBEDDING = '1'
    & $captureRunner -Mode OfflineMock -EvidenceRoot $orchestrationRoot `
        -TraceLauncherPath $launcher `
        -ExpectedTraceLauncherHash (Get-FileHash -Algorithm SHA256 $launcher).Hash `
        -TraceHookPath $hook `
        -ExpectedTraceHookHash (Get-FileHash -Algorithm SHA256 $hook).Hash `
        -TraceValidatorPath $validator `
        -ExpectedTraceValidatorHash (Get-FileHash -Algorithm SHA256 $validator).Hash `
        -WaveformRunnerPath $waveformRunner `
        -ExpectedWaveformRunnerHash (Get-FileHash -Algorithm SHA256 $waveformRunner).Hash `
        -XStreamExecutablePath $target `
        -ExpectedXStreamExecutableHash (Get-FileHash -Algorithm SHA256 $target).Hash `
        -ExpectedInf oem-mock.inf -ExpectedVersion 0.0.0.1 `
        -ExpectedSysHash ('6'*64) -OwnerAuthorized `
        -PhysicalDiskDisconnectedConfirmed -MockStatePath $mockState
    $orchestrationExit = $LASTEXITCODE
    $orchestration = Get-Content -Raw -LiteralPath `
        (Join-Path $orchestrationRoot 'result.json') | ConvertFrom-Json
    Check 'complete offline orchestration preserves the production launch path' `
        ($orchestrationExit -eq 0 -and $orchestration.Result -eq 'PASS' -and
         [int]$orchestration.Capture.RequestCount -eq 6 -and
         $orchestration.Waveform.Result -eq 'PASS' -and
         (Test-Path -LiteralPath $orchestrationDispatch))
    Check 'orchestration result exposes hashes and metadata but no opaque bytes' `
        ((Get-Content -Raw -LiteralPath `
            (Join-Path $orchestrationRoot 'result.json')) -notmatch 'input_hex|output_hex')
    $publicEvidence = Join-Path $repo 'private-xstream-evidence-must-not-exist'
    Check 'orchestrator rejects a repository evidence root before target dispatch' `
        (Invoke-ExpectedFailure {
            & $captureRunner -Mode OfflineMock -EvidenceRoot $publicEvidence `
                -TraceLauncherPath $launcher -ExpectedTraceLauncherHash ('0'*64) `
                -TraceHookPath $hook -ExpectedTraceHookHash ('0'*64) `
                -TraceValidatorPath $validator -ExpectedTraceValidatorHash ('0'*64) `
                -WaveformRunnerPath $waveformRunner -ExpectedWaveformRunnerHash ('0'*64) `
                -XStreamExecutablePath $target -ExpectedXStreamExecutableHash ('0'*64) `
                -ExpectedInf mock -ExpectedVersion mock -ExpectedSysHash ('0'*64) `
                -OwnerAuthorized -PhysicalDiskDisconnectedConfirmed `
                -MockStatePath $mockState
        } -and -not (Test-Path -LiteralPath $publicEvidence))

    $badTrace = Join-Path $privateRoot 'truncated.jsonl'
    $badResult = Join-Path $privateRoot 'truncated-result.json'
    $header = '{"type":"legacy_xstream_user_trace","format_version":4,' +
        '"pid":1,"capture_limit":1048576,"qpc_frequency":1,' +
        '"header_qpc":1,"header_filetime_utc":1}'
    $bad = '{"type":"ioctl","seq":1,"tid":1,"qpc":1,' +
        '"timestamp_filetime_utc":1,"duration_us":1,"handle":"0x1",' +
        '"ioctl":"0xCFDC2110","input_length":130,"output_length":14,' +
        '"success":false,"last_error":1,"bytes_returned":0,' +
        '"pending":false,"input_captured":64,"input_truncated":true,' +
        '"input_hex":"00","family":2,"opcode":3,' +
        '"output_captured":0,"output_truncated":false,"output_hex":""}'
    $footer = '{"type":"trace_footer","records":1,"write_failed":false}'
    [IO.File]::WriteAllLines($badTrace,@($header,$bad,$footer),`
        (New-Object Text.UTF8Encoding($false)))
    Check 'validator rejects a 64-byte truncated request' `
        (Invoke-ExpectedFailure {
            & $validator -TracePath $badTrace -ResultPath $badResult `
                -PrivateEvidenceRoot $privateRoot -RepositoryRoot $repo `
                -ExpectedRequestCount 1
        })

    $writeFailureTrace = Join-Path $privateRoot 'write-failure.jsonl'
    $writeFailureResult = Join-Path $privateRoot 'write-failure-result.json'
    [IO.File]::WriteAllLines($writeFailureTrace,@($header,
        '{"type":"trace_footer","records":1,"write_failed":true}'),
        (New-Object Text.UTF8Encoding($false)))
    Check 'validator rejects a capture write failure' `
        (Invoke-ExpectedFailure {
            & $validator -TracePath $writeFailureTrace `
                -ResultPath $writeFailureResult -PrivateEvidenceRoot $privateRoot `
                -RepositoryRoot $repo
        })

    $insideRepo = Join-Path $repo 'private-result-must-not-exist.json'
    Check 'validator rejects public-repository evidence paths' `
        (Invoke-ExpectedFailure {
            & $validator -TracePath $trace -ResultPath $insideRepo `
                -PrivateEvidenceRoot $repo -RepositoryRoot $repo
        })
    Check 'no private result was written into the repository' `
        (-not (Test-Path -LiteralPath $insideRepo))
}
finally {
    Remove-Item Env:LECS65_SYNTHETIC_DISPATCH_MARKER -ErrorAction SilentlyContinue
    Remove-Item Env:LECS65_EXPECT_EMBEDDING -ErrorAction SilentlyContinue
    if ($privateRoot.StartsWith([IO.Path]::GetTempPath(),
        [StringComparison]::OrdinalIgnoreCase) -and
        (Test-Path -LiteralPath $privateRoot)) {
        Remove-Item -LiteralPath $privateRoot -Recurse -Force
    }
}

Write-Host "XSTREAM IOCTL TRACE: $passed/$($passed+$failed) passed; $failed failed."
if ($failed -ne 0) { exit 1 }
