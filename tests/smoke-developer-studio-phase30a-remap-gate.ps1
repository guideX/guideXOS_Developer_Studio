[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureSource = '',
    [int]$CycleCount = 5,
    [string]$EvidenceRoot = ''
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Phase20 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase20.ps1'
if (-not $FixtureSource) { $FixtureSource = Join-Path $RepoRoot 'tests\fixtures\debugger-phase15' }
$FixtureSource = [IO.Path]::GetFullPath($FixtureSource)
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
if (-not $EvidenceRoot) {
    $evidenceToken = [Guid]::NewGuid().ToString('N').Substring(0, 8)
    $EvidenceRoot = Join-Path (Split-Path -Parent $ServerRoot) ("g30a-rmp-" + $evidenceToken)
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null

if ($CycleCount -ne 5) { throw 'Phase 30A remap gate requires exactly five consecutive cycles' }
if (-not (Test-Path -LiteralPath $Phase20 -PathType Leaf)) { throw "Phase 20 hosted runner is missing: $Phase20" }
if (-not (Test-Path -LiteralPath $FixtureSource -PathType Container)) { throw "Remap fixture source is missing: $FixtureSource" }

$passed = 0
for ($iteration = 1; $iteration -le $CycleCount; ++$iteration) {
    $fixture = Join-Path $EvidenceRoot ("fixture-$iteration")
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    Copy-Item -Path (Join-Path $FixtureSource '*') -Destination $fixture -Recurse -Force
    foreach ($name in @('guidexos.debugger.json', 'guidexos.debugger.json.bak')) {
        $stateFile = Join-Path $fixture $name
        if (Test-Path -LiteralPath $stateFile -PathType Leaf) { Remove-Item -LiteralPath $stateFile -Force }
    }
    $traceDirectory = Join-Path $EvidenceRoot ("traces-$iteration")
    New-Item -ItemType Directory -Path $traceDirectory -Force | Out-Null
    Write-Host "Hosted rebuild/remap cycle $iteration/$CycleCount starting with fresh workspace state"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Phase20 `
        -Case ManagerRemapRetention `
        -ServerRoot $ServerRoot `
        -FixtureRoot $fixture `
        -TraceDirectory $traceDirectory `
        -TraceRunIndex $iteration `
        -TraceArtifactName ("phase30a-remap-$iteration.log")
    if ($LASTEXITCODE -ne 0) {
        throw "Phase 30A remap gate stopped on cycle $iteration/$CycleCount; no retry was made. Evidence: $EvidenceRoot"
    }
    ++$passed
    Write-Host "remap_cycle=$iteration PASS"
}

Write-Host "phase30a_remap_gate=$passed/$CycleCount PASS"
Write-Host "phase30a_remap_evidence=$EvidenceRoot"
