[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureSource = '',
    [int]$CycleCount = 25,
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
    $EvidenceRoot = Join-Path (Split-Path -Parent $ServerRoot) ("g30a-stress-" + $evidenceToken)
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)

if ($CycleCount -ne 25) { throw 'Phase 30A combined stress requires exactly 25 cycles' }
if (-not (Test-Path -LiteralPath $Phase20 -PathType Leaf)) { throw "Phase 20 hosted runner is missing: $Phase20" }
if (-not (Test-Path -LiteralPath $FixtureSource -PathType Container)) { throw "Fixture source is missing: $FixtureSource" }
if (-not (Test-Path -LiteralPath (Join-Path $ServerRoot 'guideXOSServer.experimental.exe') -PathType Leaf)) {
    throw "Experimental hosted Server is missing: $ServerRoot"
}
if (Test-Path -LiteralPath $EvidenceRoot) {
    if ((Get-ChildItem -LiteralPath $EvidenceRoot -Force | Measure-Object).Count -ne 0) {
        throw "Stress evidence root must be new or empty: $EvidenceRoot"
    }
} else {
    New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
}

$passed = 0
for ($iteration = 1; $iteration -le $CycleCount; ++$iteration) {
    $fixture = Join-Path $EvidenceRoot ("fixture-$iteration")
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    Copy-Item -Path (Join-Path $FixtureSource '*') -Destination $fixture -Recurse -Force
    foreach ($name in @('guidexos.debugger.json', 'guidexos.debugger.json.bak')) {
        $stateFile = Join-Path $fixture $name
        if (Test-Path -LiteralPath $stateFile -PathType Leaf) {
            Remove-Item -LiteralPath $stateFile -Force
        }
    }

    # Four Manager/editor lifecycles followed by one full rebuild/remap run.
    # This yields five independent real remaps across the 25-cycle stress.
    $case = if (($iteration % 5) -eq 0) { 'ManagerRemapRetention' } else { 'ManagerConditionEditor' }
    $traceDirectory = Join-Path $EvidenceRoot ("traces-$iteration")
    New-Item -ItemType Directory -Path $traceDirectory -Force | Out-Null
    Write-Host "Hosted projection/retention cycle $iteration/$CycleCount case=$case with fresh workspace state"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Phase20 `
        -Case $case `
        -ServerRoot $ServerRoot `
        -FixtureRoot $fixture `
        -TraceDirectory $traceDirectory `
        -TraceRunIndex $iteration `
        -TraceArtifactName ("phase30a-stress-$iteration.log")
    if ($LASTEXITCODE -ne 0) {
        throw "Phase 30A hosted stress stopped on cycle $iteration/$CycleCount case=$case; no retry was made. Evidence: $EvidenceRoot"
    }
    ++$passed
    Write-Host "projection_stress_cycle=$iteration PASS case=$case"
}

Write-Host "phase30a_projection_retention_stress=$passed/$CycleCount PASS"
Write-Host "phase30a_projection_retention_cadence=4 ManagerConditionEditor + 1 ManagerRemapRetention, repeated five times"
Write-Host "phase30a_projection_retention_evidence=$EvidenceRoot"
