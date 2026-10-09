$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$decoder = Join-Path $repoRoot 'tools\fpga\virtexe_xc2s200e_decode.py'

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
        & $cmd.Source -3 $decoder --self-test
        exit $LASTEXITCODE
    }
}
if (-not $python) {
    throw 'Python was not found. Set PYTHON to a Python 3 executable.'
}

& $python $decoder --self-test
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($env:WR6K_FPGA_BINARY_205) {
    & $python $decoder $env:WR6K_FPGA_BINARY_205 --validate-knowns
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
