[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [int]$MaxRuntimeSeconds = 240,
    [string]$TraceDirectory = ''
)

$ErrorActionPreference = 'Stop'
$workflow = Join-Path $PSScriptRoot 'smoke-developer-studio-phase22-workflow.ps1'
$arguments = @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', $workflow,
    '-ServerRoot', $ServerRoot,
    '-Documents',
    '-Build',
    '-MaxRuntimeSeconds', $MaxRuntimeSeconds
)
if ($FixtureRoot) { $arguments += @('-FixtureRoot', $FixtureRoot) }
if ($TraceDirectory) { $arguments += @('-TraceDirectory', $TraceDirectory) }

& powershell.exe @arguments
if ($LASTEXITCODE -ne 0) { throw "Phase 23 hosted document/completion coverage failed with exit code $LASTEXITCODE" }
Write-Host 'Developer Studio Phase 23 hosted document/completion coverage PASS'
