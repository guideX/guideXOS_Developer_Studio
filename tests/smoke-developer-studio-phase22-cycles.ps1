[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [int]$Cycles = 5,
    [int]$MaxRuntimeSeconds = 240,
    [string]$TraceDirectory = ''
)

$ErrorActionPreference = 'Stop'
if ($Cycles -lt 5) { throw 'Phase 22 ordinary workflow proof requires at least five cycles.' }
$workflow = Join-Path $PSScriptRoot 'smoke-developer-studio-phase22-workflow.ps1'
if (-not $TraceDirectory) { $TraceDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'logs\phase22-cycles' }
New-Item -ItemType Directory -Path $TraceDirectory -Force | Out-Null

for ($cycle = 1; $cycle -le $Cycles; ++$cycle) {
    Write-Host "Phase 22 ordinary IDE cycle $cycle/$Cycles"
    $arguments = New-Object 'System.Collections.Generic.List[string]'
    $arguments.Add('-NoProfile')
    $arguments.Add('-ExecutionPolicy')
    $arguments.Add('Bypass')
    $arguments.Add('-File')
    $arguments.Add($workflow)
    $arguments.Add('-ServerRoot')
    $arguments.Add($ServerRoot)
    if ($FixtureRoot) { $arguments.Add('-FixtureRoot'); $arguments.Add($FixtureRoot) }
    $arguments.Add('-Build')
    $arguments.Add('-MaxRuntimeSeconds'); $arguments.Add([string]$MaxRuntimeSeconds)
    $arguments.Add('-TraceDirectory'); $arguments.Add((Join-Path $TraceDirectory ("cycle-{0}" -f $cycle)))
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "Phase 22 ordinary IDE cycle $cycle failed." }
    Write-Host "ordinary_cycle_$cycle=PASS"
}

Write-Host "ordinary_cycles=$Cycles/$Cycles PASS"
