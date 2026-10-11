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

$script:StressRunToken = [Guid]::NewGuid().ToString('N')
$script:StressManifestPath = Join-Path $EvidenceRoot 'phase30a-stress-run.json'
$script:StressManifest = [ordered]@{
    schema = 'guideXOS.phase30a.stress-run.v1'
    runToken = $script:StressRunToken
    evidenceRoot = $EvidenceRoot
    fixtureSource = $FixtureSource
    serverRoot = $ServerRoot
    plannedCycles = $CycleCount
    remapCadence = '4 ManagerConditionEditor + 1 ManagerRemapRetention, repeated five times'
    state = 'running'
    currentCycle = 0
    completedCycles = 0
    failedCycle = $null
    startedAtUtc = [DateTime]::UtcNow.ToString('o')
    finishedAtUtc = $null
}
function Save-StressManifest() {
    if (-not (Test-Path -LiteralPath $script:EvidenceRoot -PathType Container)) {
        throw "Phase 30A test-infrastructure failure: evidence root is missing or moved; refusing to recreate it: $script:EvidenceRoot"
    }
    Set-Content -LiteralPath $script:StressManifestPath -Value ($script:StressManifest | ConvertTo-Json -Depth 5) -Encoding UTF8
}
function Assert-StressEvidenceRoot([string]$Stage) {
    if (-not (Test-Path -LiteralPath $script:EvidenceRoot -PathType Container)) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: evidence root is missing or moved; no fallback path will be used: $script:EvidenceRoot"
    }
    $resolvedRoot = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $script:EvidenceRoot).Path)
    if ($resolvedRoot -ine $script:EvidenceRoot) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: evidence root resolved to a different path: expected=$script:EvidenceRoot actual=$resolvedRoot"
    }
    if (-not (Test-Path -LiteralPath $script:StressManifestPath -PathType Leaf)) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: run manifest is missing from its recorded evidence root: $script:StressManifestPath"
    }
    $stored = Get-Content -LiteralPath $script:StressManifestPath -Raw | ConvertFrom-Json
    if ($stored.runToken -ne $script:StressRunToken -or $stored.evidenceRoot -ine $script:EvidenceRoot) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: evidence directory belongs to a different run"
    }
}
function Assert-RunChildPath([string]$Path, [string]$Label) {
    $fullPath = [IO.Path]::GetFullPath($Path)
    $rootPrefix = $script:EvidenceRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Phase 30A test-infrastructure failure: $Label is outside the recorded evidence root: $fullPath"
    }
}
function Assert-RunChildDirectory([string]$Path, [string]$Label) {
    Assert-RunChildPath $Path $Label
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Phase 30A test-infrastructure failure: $Label is missing or moved from its recorded path: $Path"
    }
}
Save-StressManifest
Write-Host "phase30a_stress_run_token=$script:StressRunToken"
Write-Host "phase30a_stress_evidence_root=$EvidenceRoot"
Write-Host "phase30a_stress_run_manifest=$script:StressManifestPath"

$passed = 0
for ($iteration = 1; $iteration -le $CycleCount; ++$iteration) {
    Assert-StressEvidenceRoot "cycle $iteration setup"
    $fixture = Join-Path $EvidenceRoot ("fixture-$iteration")
    Assert-RunChildPath $fixture 'fixture directory'
    New-Item -ItemType Directory -Path $fixture | Out-Null
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
    Assert-RunChildPath $traceDirectory 'trace directory'
    New-Item -ItemType Directory -Path $traceDirectory | Out-Null
    $script:StressManifest.currentCycle = $iteration
    $script:StressManifest.currentCase = $case
    $script:StressManifest.currentFixture = $fixture
    $script:StressManifest.currentTraceDirectory = $traceDirectory
    $script:StressManifest.completedCycles = $passed
    Save-StressManifest
    Assert-StressEvidenceRoot "cycle $iteration launch"
    Write-Host "Hosted projection/retention cycle $iteration/$CycleCount case=$case with fresh workspace state"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Phase20 `
        -Case $case `
        -ServerRoot $ServerRoot `
        -FixtureRoot $fixture `
        -TraceDirectory $traceDirectory `
        -TraceRunIndex $iteration `
        -TraceArtifactName ("phase30a-stress-$iteration.log")
    Assert-StressEvidenceRoot "cycle $iteration completion"
    Assert-RunChildDirectory $fixture "cycle $iteration fixture"
    Assert-RunChildDirectory $traceDirectory "cycle $iteration trace directory"
    if ($LASTEXITCODE -ne 0) {
        $script:StressManifest.state = 'failed'
        $script:StressManifest.failedCycle = $iteration
        $script:StressManifest.completedCycles = $passed
        $script:StressManifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
        Save-StressManifest
        throw "Phase 30A hosted stress stopped on cycle $iteration/$CycleCount case=$case; no retry was made. Evidence: $EvidenceRoot"
    }
    ++$passed
    $script:StressManifest.completedCycles = $passed
    Save-StressManifest
    Write-Host "projection_stress_cycle=$iteration PASS case=$case"
}

$script:StressManifest.state = 'complete'
$script:StressManifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
Save-StressManifest
Write-Host "phase30a_projection_retention_stress=$passed/$CycleCount PASS"
Write-Host "phase30a_projection_retention_cadence=4 ManagerConditionEditor + 1 ManagerRemapRetention, repeated five times"
Write-Host "phase30a_projection_retention_evidence=$EvidenceRoot"
