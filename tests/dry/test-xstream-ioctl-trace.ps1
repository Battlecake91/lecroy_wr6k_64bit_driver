$ErrorActionPreference = 'Continue'
Set-StrictMode -Version Latest

$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $repo 'scripts\native-process.ps1')
$buildScript = Join-Path $repo 'scripts\build-xstream-ioctl-trace.ps1'
$validator = Join-Path $repo 'scripts\validate-xstream-ioctl-capture.ps1'
$captureRunner = Join-Path $repo 'scripts\invoke-xstream-lossless-capture.ps1'
$waveformRunner = Join-Path $repo 'scripts\invoke-xstream-single-waveform.ps1'
$toolRoot = Join-Path $repo 'tools\xstream-ioctl-trace\build\x86-test'
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

& $buildScript -TestMode | Out-Host

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
    $captureResult = "$trace.capture-result.json"
    $ready = Join-Path $privateRoot 'capture.ready'
    $dispatch = Join-Path $privateRoot 'target.dispatch'
    $result = Join-Path $privateRoot 'validation.json'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $dispatch
    Remove-Item Env:LECS65_EXPECT_EMBEDDING -ErrorAction SilentlyContinue

    & $launcher $target $trace $ready
    $launchExit = $LASTEXITCODE
    $launcherMetadata = Get-Content -Raw -LiteralPath $captureResult | ConvertFrom-Json
    Check 'synthetic target completed through the production launcher' `
        ($launchExit -eq 0 -and (Test-Path $dispatch) -and (Test-Path $ready) -and
         $launcherMetadata.capture_classification -eq 'complete_normal' -and
         $launcherMetadata.footer_present -and $launcherMetadata.parent_flush_succeeded)
    Check 'natural target exit has separate create, resume, wait and exit timestamps' `
        (-not [string]::IsNullOrWhiteSpace($launcherMetadata.process_create_returned_utc) -and
         -not [string]::IsNullOrWhiteSpace($launcherMetadata.resume_thread_called_utc) -and
         -not [string]::IsNullOrWhiteSpace($launcherMetadata.resume_thread_returned_utc) -and
         -not [string]::IsNullOrWhiteSpace($launcherMetadata.target_wait_started_utc) -and
         -not [string]::IsNullOrWhiteSpace($launcherMetadata.target_wait_completed_utc) -and
         -not [string]::IsNullOrWhiteSpace($launcherMetadata.target_exit_observed_utc) -and
         $launcherMetadata.wait_result -eq 'WAIT_OBJECT_0' -and
         -not [bool]$launcherMetadata.target_was_alive_at_timeout -and
         [string]::IsNullOrWhiteSpace($launcherMetadata.termination_requested_utc))

    $classificationPath = Join-Path $privateRoot 'classification.json'
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $trace -CaptureResultPath $captureResult -ResultPath $classificationPath `
        -PrivateEvidenceRoot $privateRoot -RepositoryRoot $repo -ClassifyOnly
    $classificationExit = $LASTEXITCODE
    $classification = Get-Content -Raw -LiteralPath $classificationPath | ConvertFrom-Json
    Check 'complete trace receives the complete-normal classification' `
        ($classificationExit -eq 0 -and
         $classification.Classification -eq 'COMPLETE_NORMAL_CAPTURE')

    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $trace -ResultPath $result `
        -CaptureResultPath $captureResult -PrivateEvidenceRoot $privateRoot -RepositoryRoot $repo `
        -ExpectedRequestCount 7 -ExpectedRequestHash $expectedRequestHash
    $validationExit = $LASTEXITCODE
    $summary = Get-Content -Raw -LiteralPath $result | ConvertFrom-Json
    Check 'six Win32 and one native 130-byte request validate' `
        ($validationExit -eq 0 -and [int]$summary.RequestCount -eq 7)
    Check 'binary request including NUL bytes is preserved without truncation' `
        ($summary.AllRequestsIdentical -eq $true -and
         @($summary.UniqueRequestHashes).Count -eq 1 -and
         [string]$summary.UniqueRequestHashes[0] -eq $expectedRequestHash)
    Check 'concurrent request correlation retains multiple threads and handles' `
        (@($summary.Requests.ThreadId | Sort-Object -Unique).Count -ge 2 -and
         @($summary.Requests.Handle | Sort-Object -Unique).Count -ge 2)
    Check 'timestamps, sequence and Win32 error metadata are retained' `
        (@($summary.Requests | Where-Object {
            [string]::IsNullOrWhiteSpace($_.TimestampUtc) -or
            [int64]$_.Sequence -le 0 -or
            ($_.Api -eq 'ioctl' -and $null -eq $_.LastError)
        }).Count -eq 0)
    $nativeRequests = @($summary.Requests | Where-Object {$_.Api -eq 'nt_ioctl'})
    Check 'native NTSTATUS and IO_STATUS_BLOCK.Information are retained' `
        ($nativeRequests.Count -eq 1 -and
         [string]$nativeRequests[0].NtStatus -match '^0xC[0-9A-Fa-f]{7}$' -and
         [int64]$nativeRequests[0].ResponseBytes -eq 0)

    $jsonRecords = @(Get-Content -LiteralPath $trace | ForEach-Object {
        if (-not [string]::IsNullOrWhiteSpace($_)) { $_ | ConvertFrom-Json }
    })
    $targets = @($jsonRecords | Where-Object {
        ($_.type -eq 'ioctl' -or $_.type -eq 'nt_ioctl') -and
        $_.ioctl -eq '0xCFDC2110'
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

    $successTrace = Join-Path $privateRoot 'response14.jsonl'
    $successReady = Join-Path $privateRoot 'response14.ready'
    $successResult = "$successTrace.capture-result.json"
    $successDispatch = Join-Path $privateRoot 'response14.dispatch'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $successDispatch
    $env:LECS65_TEST_IOCTL_SUCCESS = '1'
    & $launcher $target $successTrace $successReady
    $responseLaunchExit = $LASTEXITCODE
    $responseValidation = Join-Path $privateRoot 'response14-validation.json'
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $successTrace -CaptureResultPath $successResult `
        -ResultPath $responseValidation -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ExpectedRequestCount 7 `
        -ExpectedRequestHash $expectedRequestHash
    $responseValidationExit = $LASTEXITCODE
    $responseSummary = Get-Content -Raw -LiteralPath $responseValidation | ConvertFrom-Json
    [byte[]]$expectedResponse = 0x31..0x3E
    $responseSha = [Security.Cryptography.SHA256]::Create()
    try {
        $expectedResponseHash = ([BitConverter]::ToString(
            $responseSha.ComputeHash($expectedResponse))).Replace('-','')
    }
    finally { $responseSha.Dispose() }
    Check 'synthetic response fixture records six 14-byte API replies and native failure information' `
        ($responseLaunchExit -eq 0 -and $responseValidationExit -eq 0 -and
         @($responseSummary.Requests | Where-Object {
            [int]$_.ResponseBytes -ne 14 -or
            [string]$_.ResponseSha256 -ne $expectedResponseHash
         }).Count -eq 1 -and
         @($responseSummary.Requests | Where-Object {
            $_.Api -eq 'nt_ioctl' -and [int]$_.ResponseBytes -ne 0
         }).Count -eq 0)
    Remove-Item Env:LECS65_TEST_IOCTL_SUCCESS -ErrorAction SilentlyContinue

    $hangTrace = Join-Path $privateRoot 'timeout.jsonl'
    $hangReady = Join-Path $privateRoot 'timeout.ready'
    $hangResult = "$hangTrace.capture-result.json"
    $hangClassification = Join-Path $privateRoot 'timeout-classification.json'
    $hangDispatch = Join-Path $privateRoot 'timeout.dispatch'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $hangDispatch
    $env:LECS65_SYNTHETIC_TARGET_MODE = 'hang-after-requests'
    # The production launcher emits an expected diagnostic to native stderr on
    # timeout; its exit code and parsed classification are the assertions.
    & $launcher --timeout-seconds 1 $target $hangTrace $hangReady
    $hangExit = $LASTEXITCODE
    $hangResultData = Get-Content -Raw -LiteralPath $hangResult | ConvertFrom-Json
    Remove-Item Env:LECS65_SYNTHETIC_TARGET_MODE -ErrorAction SilentlyContinue
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $hangTrace -CaptureResultPath $hangResult `
        -ResultPath $hangClassification -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ClassifyOnly
    $hangClass = Get-Content -Raw -LiteralPath $hangClassification | ConvertFrom-Json
    $hangReadyData = @{}
    foreach($line in Get-Content $hangReady){$parts=$line -split '=',2;if($parts.Count -eq 2){$hangReadyData[$parts[0]]=$parts[1]}}
    Check 'bounded timeout preserves a valid partial trace and known termination' `
        ($hangExit -ne 0 -and $hangClass.Classification -eq 'VALID_PARTIAL_LAUNCHER_TERMINATION' -and
         [int]$hangClass.TraceRecordCount -ge 6 -and (Test-Path $hangDispatch) -and
         @(Get-Process -Id ([int]$hangReadyData.Pid) -ErrorAction SilentlyContinue).Count -eq 0)
    $waitExpired = [DateTime]::Parse($hangResultData.target_wait_completed_utc).ToUniversalTime()
    $terminateRequested = [DateTime]::Parse($hangResultData.termination_requested_utc).ToUniversalTime()
    $terminateReturned = [DateTime]::Parse($hangResultData.terminate_process_returned_utc).ToUniversalTime()
    $exitObserved = [DateTime]::Parse($hangResultData.target_exit_observed_utc).ToUniversalTime()
    Check 'launcher timeout proves target alive at deadline and records exact termination order' `
        ($hangResultData.target_was_alive_at_timeout -and
         $hangResultData.wait_result -eq 'WAIT_OBJECT_0_AFTER_TERMINATION' -and
         $hangResultData.terminate_process_succeeded -and
         $waitExpired -le $terminateRequested -and
         $terminateRequested -le $terminateReturned -and
         $terminateReturned -le $exitObserved -and
         $hangResultData.target_deadline_utc)

    $crashTrace = Join-Path $privateRoot 'crash.jsonl'
    $crashReady = Join-Path $privateRoot 'crash.ready'
    $crashResult = "$crashTrace.capture-result.json"
    $crashClassification = Join-Path $privateRoot 'crash-classification.json'
    $env:LECS65_SYNTHETIC_TARGET_MODE = 'crash'
    & $launcher $target $crashTrace $crashReady
    $crashExit = $LASTEXITCODE
    Remove-Item Env:LECS65_SYNTHETIC_TARGET_MODE -ErrorAction SilentlyContinue
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $crashTrace -CaptureResultPath $crashResult `
        -ResultPath $crashClassification -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ClassifyOnly
    $crashClass = Get-Content -Raw -LiteralPath $crashClassification | ConvertFrom-Json
    Check 'target termination without footer remains unknown rather than a claimed crash' `
        ($crashExit -ne 0 -and $crashClass.Classification -eq 'UNKNOWN_TERMINATION')

    $crashZeroTrace = Join-Path $privateRoot 'crashzero.jsonl'
    $crashZeroReady = Join-Path $privateRoot 'crashzero.ready'
    $crashZeroResult = "$crashZeroTrace.capture-result.json"
    $crashZeroClassPath = Join-Path $privateRoot 'crashzero-classification.json'
    $env:LECS65_SYNTHETIC_TARGET_MODE = 'crashzero'
    & $launcher $target $crashZeroTrace $crashZeroReady
    $crashZeroExit = $LASTEXITCODE
    Remove-Item Env:LECS65_SYNTHETIC_TARGET_MODE -ErrorAction SilentlyContinue
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $crashZeroTrace -CaptureResultPath $crashZeroResult `
        -ResultPath $crashZeroClassPath -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ClassifyOnly
    $crashZeroClass = Get-Content -Raw -LiteralPath $crashZeroClassPath | ConvertFrom-Json
    Check 'zero exit code without footer does not falsely imply normal capture' `
        ($crashZeroExit -ne 0 -and $crashZeroClass.Classification -eq 'UNKNOWN_TERMINATION')

    $malformedTrace = Join-Path $privateRoot 'malformed.jsonl'
    $malformedResult = Join-Path $privateRoot 'malformed.capture-result.json'
    $malformedClassPath = Join-Path $privateRoot 'malformed-classification.json'
    $malformedHeader = '{"type":"legacy_xstream_user_trace","format_version":4,' +
        '"pid":1,"capture_limit":1048576,"qpc_frequency":1,' +
        '"header_qpc":1,"header_filetime_utc":1}'
    [IO.File]::WriteAllText($malformedTrace,$malformedHeader + "`r`n" + '{broken',
        (New-Object Text.UTF8Encoding($false)))
    [IO.File]::WriteAllText($malformedResult,
        '{"schema":1,"target_pid":1,"target_exit_code":0,"termination":"process_exited","footer_present":false,"footer_write_failed":false,"parent_flush_succeeded":true,"capture_classification":"unknown_termination"}',
        (New-Object Text.UTF8Encoding($false)))
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $malformedTrace -CaptureResultPath $malformedResult `
        -ResultPath $malformedClassPath -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ClassifyOnly
    $malformedClass = Get-Content -Raw -LiteralPath $malformedClassPath | ConvertFrom-Json
    Check 'malformed partial trace is distinguished from valid unknown termination' `
        ($malformedClass.Classification -eq 'CORRUPT_OR_TRUNCATED')

    $nonzeroTrace = Join-Path $privateRoot 'orderly-nonzero.jsonl'
    $nonzeroReady = Join-Path $privateRoot 'orderly-nonzero.ready'
    $nonzeroResult = "$nonzeroTrace.capture-result.json"
    $nonzeroClassPath = Join-Path $privateRoot 'orderly-nonzero-classification.json'
    $nonzeroDispatch = Join-Path $privateRoot 'orderly-nonzero.dispatch'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $nonzeroDispatch
    $env:LECS65_SYNTHETIC_TARGET_MODE = 'exit23'
    & $launcher $target $nonzeroTrace $nonzeroReady
    $nonzeroExit = $LASTEXITCODE
    Remove-Item Env:LECS65_SYNTHETIC_TARGET_MODE -ErrorAction SilentlyContinue
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $nonzeroTrace -CaptureResultPath $nonzeroResult `
        -ResultPath $nonzeroClassPath -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ClassifyOnly
    $nonzeroClass = Get-Content -Raw -LiteralPath $nonzeroClassPath | ConvertFrom-Json
    Check 'orderly nonzero target exit retains complete trace but remains a process failure' `
        ($nonzeroExit -eq 23 -and
         $nonzeroClass.Classification -eq 'COMPLETE_NORMAL_CAPTURE' -and
         [int64]$nonzeroClass.TargetExitCode -eq 23)

    $flushTrace = Join-Path $privateRoot 'flush-failure.jsonl'
    $flushReady = Join-Path $privateRoot 'flush-failure.ready'
    $flushResult = "$flushTrace.capture-result.json"
    $flushClassPath = Join-Path $privateRoot 'flush-failure-classification.json'
    $env:LECS65_TEST_HOOK_FLUSH_FAILURE = '1'
    & $launcher $target $flushTrace $flushReady
    $flushExit = $LASTEXITCODE
    Remove-Item Env:LECS65_TEST_HOOK_FLUSH_FAILURE -ErrorAction SilentlyContinue
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $flushTrace -CaptureResultPath $flushResult `
        -ResultPath $flushClassPath -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ClassifyOnly
    $flushClass = Get-Content -Raw -LiteralPath $flushClassPath | ConvertFrom-Json
    Check 'hook flush failure is retained as corrupt/incomplete evidence' `
        ($flushExit -ne 0 -and $flushClass.Classification -eq 'CORRUPT_OR_TRUNCATED')

    $parentFlushTrace = Join-Path $privateRoot 'parent-flush-failure.jsonl'
    $parentFlushReady = Join-Path $privateRoot 'parent-flush-failure.ready'
    $parentFlushResult = "$parentFlushTrace.capture-result.json"
    $parentFlushClassPath = Join-Path $privateRoot 'parent-flush-classification.json'
    $env:LECS65_TEST_PARENT_FLUSH_FAILURE = '1'
    & $launcher $target $parentFlushTrace $parentFlushReady
    $parentFlushExit = $LASTEXITCODE
    Remove-Item Env:LECS65_TEST_PARENT_FLUSH_FAILURE -ErrorAction SilentlyContinue
    & "$env:SystemRoot\System32\WindowsPowerShell\v1.0\powershell.exe" `
        -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $validator `
        -TracePath $parentFlushTrace -CaptureResultPath $parentFlushResult `
        -ResultPath $parentFlushClassPath -PrivateEvidenceRoot $privateRoot `
        -RepositoryRoot $repo -ClassifyOnly
    $parentFlushClass = Get-Content -Raw -LiteralPath $parentFlushClassPath | ConvertFrom-Json
    Check 'parent flush failure cannot be promoted to a complete capture' `
        ($parentFlushExit -ne 0 -and
         $parentFlushClass.Classification -eq 'UNKNOWN_TERMINATION')

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
         [int]$orchestration.Capture.RequestCount -eq 7 -and
         $orchestration.Waveform.Result -eq 'PASS' -and
         (Test-Path -LiteralPath $orchestrationDispatch))
    Check 'offline orchestration preserves Windows executable paths without escape parsing' `
        ($orchestration.Result -eq 'PASS' -and
         $orchestration.ObserverProcess.Executable -match '\\WindowsPowerShell\\v1\.0\\powershell\.exe$' -and
         $orchestration.ObserverProcess.ProcessId -gt 0)
    Check 'orchestration result exposes hashes and metadata but no opaque bytes' `
        ((Get-Content -Raw -LiteralPath `
            (Join-Path $orchestrationRoot 'result.json')) -notmatch 'input_hex|output_hex')

    $nonzeroOrchestrationRoot = Join-Path $privateRoot 'orchestration-nonzero'
    $nonzeroOrchestrationDispatch = Join-Path $privateRoot 'orchestration-nonzero.dispatch'
    $env:LECS65_SYNTHETIC_DISPATCH_MARKER = $nonzeroOrchestrationDispatch
    $env:LECS65_SYNTHETIC_TARGET_MODE = 'exit23'
    $powershell51 = Join-Path $env:SystemRoot `
        'System32\WindowsPowerShell\v1.0\powershell.exe'
    $nonzeroOrchestrationArgs = @('-NoProfile','-NonInteractive',
        '-ExecutionPolicy','Bypass','-File',$captureRunner,
        '-Mode','OfflineMock','-EvidenceRoot',$nonzeroOrchestrationRoot,
        '-TraceLauncherPath',$launcher,
        '-ExpectedTraceLauncherHash',(Get-FileHash -Algorithm SHA256 $launcher).Hash,
        '-TraceHookPath',$hook,
        '-ExpectedTraceHookHash',(Get-FileHash -Algorithm SHA256 $hook).Hash,
        '-TraceValidatorPath',$validator,
        '-ExpectedTraceValidatorHash',(Get-FileHash -Algorithm SHA256 $validator).Hash,
        '-WaveformRunnerPath',$waveformRunner,
        '-ExpectedWaveformRunnerHash',(Get-FileHash -Algorithm SHA256 $waveformRunner).Hash,
        '-XStreamExecutablePath',$target,
        '-ExpectedXStreamExecutableHash',(Get-FileHash -Algorithm SHA256 $target).Hash,
        '-ExpectedInf','oem-mock.inf','-ExpectedVersion','0.0.0.1',
        '-ExpectedSysHash',('6'*64),'-OwnerAuthorized',
        '-PhysicalDiskDisconnectedConfirmed','-MockStatePath',$mockState)
    $nonzeroChild = Invoke-LecS65NativeProcess `
        -Name 'Nonzero orchestration child test' -FilePath $powershell51 `
        -Arguments $nonzeroOrchestrationArgs -WorkingDirectory $repo `
        -LogPath (Join-Path $privateRoot 'nonzero-orchestration-child.log') `
        -TimeoutSeconds 120 -ReturnOnNonZeroExit
    $nonzeroOrchestrationExit = $nonzeroChild.ExitCode
    Remove-Item Env:LECS65_SYNTHETIC_TARGET_MODE -ErrorAction SilentlyContinue
    $nonzeroOrchestration = Get-Content -Raw -LiteralPath `
        (Join-Path $nonzeroOrchestrationRoot 'result.json') | ConvertFrom-Json
    Check 'nonzero child exit propagates through production orchestration with preserved capture metadata' `
        ($nonzeroOrchestrationExit -ne 0 -and
         $nonzeroOrchestration.Result -eq 'FAIL' -and
         [int]$nonzeroOrchestration.TraceLauncherExitCode -eq 23 -and
         $nonzeroOrchestration.CaptureClassification.Classification -eq 'COMPLETE_NORMAL_CAPTURE' -and
         $nonzeroOrchestration.TargetProcessRemaining -eq $false -and
         (Test-Path -LiteralPath $nonzeroOrchestrationDispatch))

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
