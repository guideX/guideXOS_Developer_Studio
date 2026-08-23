[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$BuildDirectory = '',
    [string]$TraceDirectory = '',
    [switch]$SkipPackage,
    [switch]$SkipHosted
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
if (-not $BuildDirectory) { $BuildDirectory = Join-Path $repoRoot 'build-native' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
if (-not $TraceDirectory) { $TraceDirectory = Join-Path $repoRoot 'logs' }
$TraceDirectory = [IO.Path]::GetFullPath($TraceDirectory)

function Invoke-FastChecked {
    param([string]$Name, [string]$FilePath, [string[]]$Arguments)
    $started = Get-Date
    Write-Host "[$Name] START"
    & $FilePath @Arguments
    $exitCode = $LASTEXITCODE
    $elapsed = [Math]::Round(((Get-Date) - $started).TotalSeconds, 1)
    if ($exitCode -ne 0) { throw "[$Name] failed with exit code $exitCode" }
    Write-Host "[$Name] PASS ($elapsed s)"
}

function Test-DeveloperStudioManifest {
    $manifestPath = Join-Path $repoRoot 'app\app.json'
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) { throw "Manifest is missing: $manifestPath" }
    $manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
    if ($manifest.id -ne 'com.guidexos.developerstudio') { throw 'Manifest id is not canonical.' }
    if ($manifest.kind -ne 'NativeElf') { throw 'Manifest kind is not NativeElf.' }
    if ($manifest.entries.Count -ne 1 -or $manifest.entries[0].path -notmatch '^bin/amd64/developerstudio\.elf$') { throw 'Manifest executable resolver path is invalid.' }
    Write-Host 'manifest_resolver=PASS'
}

$started = Get-Date
Write-Host 'Developer Studio fast validation tier'
Write-Host "build_directory=$BuildDirectory"
Write-Host "server_root=$ServerRoot"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'cmake is required for the fast validation tier.' }
$cmakeConfigureArguments = @('-S', $repoRoot, '-B', $BuildDirectory, '-DGUIDEXOS_BUILD_NATIVE_ELF=OFF', '-DCMAKE_BUILD_TYPE=Debug')
$mingwGxx = 'C:\mingw64\bin\g++.exe'
if (Test-Path -LiteralPath $mingwGxx -PathType Leaf) {
    $cmakeConfigureArguments += @('-DCMAKE_CXX_COMPILER=C:\mingw64\bin\g++.exe')
}
Invoke-FastChecked 'cmake configure' 'cmake' $cmakeConfigureArguments
Invoke-FastChecked 'native CMake build' 'cmake' @('--build', $BuildDirectory, '--parallel')
Invoke-FastChecked 'CTest' 'ctest' @('--test-dir', $BuildDirectory, '--output-on-failure')
Test-DeveloperStudioManifest

if ($SkipPackage) {
    Write-Host 'package_build=NOT RUN'
} else {
    $packageScript = Join-Path $repoRoot 'build.ps1'
    Invoke-FastChecked 'Developer Studio package build' 'powershell.exe' @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $packageScript, '-ServerRoot', $ServerRoot, '-Configuration', 'Debug')
    Write-Host 'package_build=PASS'
}

if ($SkipHosted) {
    Write-Host 'representative_hosted_debugger=NOT RUN'
} else {
    $hostedScript = Join-Path $repoRoot 'tests\smoke-developer-studio-debugger.ps1'
    $hostedArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $hostedScript, '-ServerRoot', $ServerRoot, '-ContinueBreakpoint', '-TraceDirectory', $TraceDirectory, '-TraceRunIndex', '101')
    Invoke-FastChecked 'representative hosted debugger lifecycle' 'powershell.exe' $hostedArgs
    Write-Host 'representative_hosted_debugger=PASS'
}

$elapsed = [Math]::Round(((Get-Date) - $started).TotalSeconds, 1)
Write-Host "fast_tier_elapsed_seconds=$elapsed"
Write-Host 'developer_studio_validation_fast=PASS'
