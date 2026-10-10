$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$decoder = Join-Path $repoRoot 'tools\fpga\virtexe_xc2s200e_decode.py'
$routingTests = Join-Path $repoRoot 'tests\dry\test_fpga_routing.py'
$pythonArgs = @('-B')

$python = $env:PYTHON
if (-not $python) {
    $cmd = Get-Command python -ErrorAction SilentlyContinue
    if ($cmd) {
        $python = $cmd.Source
    }
}
if (-not $python) {
    $cmd = Get-Command py -ErrorAction SilentlyContinue
    if ($cmd) {
        $python = $cmd.Source
        $pythonArgs = @('-3', '-B')
    }
}
if (-not $python) {
    throw 'Python was not found. Set PYTHON to a Python 3 executable.'
}

Write-Host 'Synthetic frame and routing tests (no private firmware required)'
& $python @pythonArgs $decoder --self-test
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& $python @pythonArgs $routingTests -v
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($env:WR6K_FPGA_SYMBOLIC -eq '1') {
    Write-Host 'Synthetic bounded SMT tests (requires requirements-symbolic.txt)'
    & $python @pythonArgs (Join-Path $repoRoot 'tests\dry\test_fpga_symbolic.py') -v
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host 'SKIP optional SMT tests: set WR6K_FPGA_SYMBOLIC=1 after installing requirements-symbolic.txt'
}

if ($env:WR6K_FPGA_BINARY_205) {
    Write-Host 'Private firmware frame calibration'
    & $python @pythonArgs $decoder $env:WR6K_FPGA_BINARY_205 --validate-knowns
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
    if ($env:WR6K_PRJCOMBINE -and $env:WR6K_ROUTING_ADAPTER) {
        Write-Host 'Private firmware native architecture calibration and F5/clock upstream routes'
        & $python @pythonArgs $decoder $env:WR6K_FPGA_BINARY_205 --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER --validate-architecture
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        Write-Host 'Private firmware PCI local state equations (not global reachability or DMA quiescence)'
        & $python @pythonArgs $decoder $env:WR6K_FPGA_BINARY_205 --project-combine $env:WR6K_PRJCOMBINE --adapter $env:WR6K_ROUTING_ADAPTER --validate-pci-state --max-logic 10000
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } else {
        Write-Host 'SKIP native architecture calibration: set WR6K_PRJCOMBINE and WR6K_ROUTING_ADAPTER'
    }
} else {
    Write-Host 'SKIP private firmware validation: set WR6K_FPGA_BINARY_205'
}
