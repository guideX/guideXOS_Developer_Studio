param(
    [string]$FixturePath = (Join-Path $PSScriptRoot 'fixtures\debugger-phase3b\build\bin\amd64\debugger-phase3b.elf')
)

$ErrorActionPreference = 'Stop'
$gxx = 'C:\mingw64\bin\g++.exe'
if (!(Test-Path $gxx)) {
    $command = Get-Command g++ -ErrorAction SilentlyContinue
    if ($command) { $gxx = $command.Source }
}
if (!(Test-Path $gxx)) { throw 'g++ was not found' }
if (!(Test-Path $FixturePath)) { throw "fixture was not found: $FixturePath" }

$repoRoot = Split-Path -Parent $PSScriptRoot
$outputPath = Join-Path ([System.IO.Path]::GetTempPath()) ('guidexos-debug-symbols-capacity-' + $PID + '.exe')
try {
    & $gxx -std=c++17 -Wall -O0 -DGXOS_DEVELOPER_STUDIO_BARE_METAL `
        ('-I' + (Join-Path $repoRoot 'src')) `
        (Join-Path $repoRoot 'tests\debug_symbols_capacity_test.cpp') `
        (Join-Path $repoRoot 'src\developer_studio_debug_symbols.cpp') `
        (Join-Path $repoRoot 'src\developer_studio_debug_variables.cpp') `
        -o $outputPath
    if ($LASTEXITCODE -ne 0) { throw "capacity test build failed: $LASTEXITCODE" }

    & $outputPath $FixturePath
    if ($LASTEXITCODE -ne 0) { throw "capacity test failed: $LASTEXITCODE" }
} finally {
    Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
}
