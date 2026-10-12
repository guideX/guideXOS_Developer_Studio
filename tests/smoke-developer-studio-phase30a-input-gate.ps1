[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureSource = '',
    [int]$CycleCount = 20,
    [string]$EvidenceRoot = ''
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Phase20 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase20.ps1'
if (-not $FixtureSource) { $FixtureSource = Join-Path $RepoRoot 'tests\fixtures\debugger-phase15' }
$FixtureSource = [IO.Path]::GetFullPath($FixtureSource)
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)

if ($CycleCount -ne 20) { throw 'Phase 30A focused input gate requires exactly 20 cycles' }
if (-not (Test-Path -LiteralPath $Phase20 -PathType Leaf)) { throw "Phase 20 hosted runner is missing: $Phase20" }
if (-not (Test-Path -LiteralPath $FixtureSource -PathType Container)) { throw "Fixture source is missing: $FixtureSource" }
if (-not (Test-Path -LiteralPath (Join-Path $ServerRoot 'guideXOSServer.experimental.exe') -PathType Leaf)) {
    throw "Experimental hosted Server is missing: $ServerRoot"
}

if (-not $EvidenceRoot) {
    $evidenceToken = [Guid]::NewGuid().ToString('N')
    $EvidenceRoot = Join-Path (Split-Path -Parent $ServerRoot) ("g30a-input-gate-" + $evidenceToken)
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
if (Test-Path -LiteralPath $EvidenceRoot) {
    if ((Get-ChildItem -LiteralPath $EvidenceRoot -Force | Measure-Object).Count -ne 0) {
        throw "Focused input evidence root must be new or empty: $EvidenceRoot"
    }
} else {
    New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
}

$script:InputRunToken = [Guid]::NewGuid().ToString('N')
$script:ManifestPath = Join-Path $EvidenceRoot 'phase30a-input-gate-run.json'
$script:Manifest = [ordered]@{
    schema = 'guideXOS.phase30a.input-gate.v1'
    runToken = $script:InputRunToken
    evidenceRoot = $EvidenceRoot
    fixtureSource = $FixtureSource
    serverRoot = $ServerRoot
    plannedCycles = 20
    case = 'ManagerConditionInputOnly'
    input = 'counter == 2'
    perCycleEvents = 13
    state = 'running'
    currentCycle = 0
    completedCycles = 0
    failedCycle = $null
    failure = $null
    startedAtUtc = [DateTime]::UtcNow.ToString('o')
    finishedAtUtc = $null
    cycles = @()
}

function Save-InputManifest() {
    if (-not (Test-Path -LiteralPath $script:EvidenceRoot -PathType Container)) {
        throw "Phase 30A input gate test-infrastructure failure: evidence root is missing or moved: $script:EvidenceRoot"
    }
    Set-Content -LiteralPath $script:ManifestPath -Value ($script:Manifest | ConvertTo-Json -Depth 6) -Encoding UTF8
}

function Assert-InputEvidenceRoot([string]$Stage) {
    if (-not (Test-Path -LiteralPath $script:EvidenceRoot -PathType Container)) {
        throw "Phase 30A input gate test-infrastructure failure during ${Stage}: evidence root is missing or moved: $script:EvidenceRoot"
    }
    $resolvedRoot = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $script:EvidenceRoot).Path)
    if ($resolvedRoot -ine $script:EvidenceRoot) {
        throw "Phase 30A input gate test-infrastructure failure during ${Stage}: resolved evidence root differs: expected=$script:EvidenceRoot actual=$resolvedRoot"
    }
    if (-not (Test-Path -LiteralPath $script:ManifestPath -PathType Leaf)) {
        throw "Phase 30A input gate test-infrastructure failure during ${Stage}: run manifest is missing: $script:ManifestPath"
    }
    $stored = Get-Content -LiteralPath $script:ManifestPath -Raw | ConvertFrom-Json
    if ($stored.runToken -ne $script:InputRunToken -or $stored.evidenceRoot -ine $script:EvidenceRoot) {
        throw "Phase 30A input gate test-infrastructure failure during ${Stage}: evidence directory belongs to a different run"
    }
}

function Assert-RunChildPath([string]$Path, [string]$Label) {
    $fullPath = [IO.Path]::GetFullPath($Path)
    $rootPrefix = $script:EvidenceRoot.TrimEnd('\') + '\'
    if (-not $fullPath.StartsWith($rootPrefix, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Phase 30A input gate test-infrastructure failure: $Label is outside the recorded evidence root: $fullPath"
    }
}

function Assert-RunChildDirectory([string]$Path, [string]$Label) {
    Assert-RunChildPath $Path $Label
    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        throw "Phase 30A input gate test-infrastructure failure: $Label is missing or moved: $Path"
    }
}

Save-InputManifest
Write-Host "phase30a_input_gate_run_token=$script:InputRunToken"
Write-Host "phase30a_input_gate_evidence_root=$EvidenceRoot"
Write-Host "phase30a_input_gate_run_manifest=$script:ManifestPath"

$passed = 0
try {
for ($iteration = 1; $iteration -le 20; ++$iteration) {
    Assert-InputEvidenceRoot "cycle $iteration setup"
    $fixture = Join-Path $EvidenceRoot ("fixture-$iteration")
    Assert-RunChildPath $fixture 'fixture directory'
    New-Item -ItemType Directory -Path $fixture | Out-Null
    foreach ($item in Get-ChildItem -LiteralPath $FixtureSource -Force) {
        if ($item.Name -notin @('guidexos.debugger.json', 'guidexos.debugger.json.bak')) {
            Copy-Item -LiteralPath $item.FullName -Destination $fixture -Recurse -Force
        }
    }

    $traceDirectory = Join-Path $EvidenceRoot ("traces-$iteration")
    Assert-RunChildPath $traceDirectory 'trace directory'
    New-Item -ItemType Directory -Path $traceDirectory | Out-Null
    $inputEvidencePath = Join-Path $traceDirectory ("phase20-condition-input-$iteration.log")
    $script:Manifest.currentCycle = $iteration
    $script:Manifest.completedCycles = $passed
    Save-InputManifest
    Assert-InputEvidenceRoot "cycle $iteration launch"
    Write-Host "Focused condition-input cycle $iteration/20 with fresh workspace state"

    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Phase20 `
        -Case ManagerConditionInputOnly `
        -ServerRoot $ServerRoot `
        -FixtureRoot $fixture `
        -TraceDirectory $traceDirectory `
        -TraceRunIndex $iteration
    $cycleExit = $LASTEXITCODE
    Assert-InputEvidenceRoot "cycle $iteration completion"
    Assert-RunChildDirectory $fixture "cycle $iteration fixture"
    Assert-RunChildDirectory $traceDirectory "cycle $iteration trace directory"
    if ($cycleExit -ne 0) {
        $script:Manifest.state = 'failed'
        $script:Manifest.failedCycle = $iteration
        $script:Manifest.completedCycles = $passed
        $script:Manifest.failure = "Phase 20 input-only case returned exit code $cycleExit; no retry was made."
        $script:Manifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
        Save-InputManifest
        throw "Phase 30A focused input gate stopped on cycle $iteration/20; no retry was made. Evidence: $EvidenceRoot"
    }
    if (-not (Test-Path -LiteralPath $inputEvidencePath -PathType Leaf)) {
        throw "Phase 30A input gate test-infrastructure failure: cycle $iteration input evidence is missing: $inputEvidencePath"
    }
    $inputEvidence = Get-Content -LiteralPath $inputEvidencePath -Raw
    foreach ($sequence in 1..13) {
        foreach ($stage in @('stage=harness_generated', 'stage=harness_sent', 'stage=server_enqueue result=accepted', 'stage=compositor_received', 'stage=app_mailbox_enqueue result=accepted', 'stage=runtime_dequeued', 'stage=application_event_return', 'stage=marker_ack')) {
            if (-not $inputEvidence.Contains("input_seq=$sequence $stage")) {
                throw "Phase 30A input gate evidence incomplete: cycle=$iteration input_seq=$sequence missing=$stage file=$inputEvidencePath"
            }
        }
    }
    if (-not $inputEvidence.Contains('text=counter == 2')) {
        throw "Phase 30A input gate evidence lacks the final rendered condition for cycle ${iteration}: $inputEvidencePath"
    }

    ++$passed
    $script:Manifest.completedCycles = $passed
    $script:Manifest.cycles += [pscustomobject]@{
        cycle = $iteration
        case = 'ManagerConditionInputOnly'
        result = 'PASS'
        inputEvents = 13
        deliveryStages = 'harness_sent,server_enqueue_accepted,compositor_received,app_mailbox_accepted,runtime_dequeued,application_event_return,marker_ack'
        inputEvidence = $inputEvidencePath
    }
    Save-InputManifest
    Write-Host "phase30a_input_gate_cycle=$iteration PASS events=13 evidence=$inputEvidencePath"
}
} catch {
    if ($script:Manifest.state -eq 'running') {
        $script:Manifest.state = 'failed'
        if ($script:Manifest.currentCycle -gt 0) { $script:Manifest.failedCycle = $script:Manifest.currentCycle }
        $script:Manifest.failure = $_.Exception.Message
        $script:Manifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
        Save-InputManifest
    }
    throw
}

$script:Manifest.state = 'passed'
$script:Manifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
Save-InputManifest
Write-Host 'phase30a_input_gate=20/20 PASS'
Write-Host "phase30a_input_gate_evidence_root=$EvidenceRoot"
Write-Host "phase30a_input_gate_run_manifest=$script:ManifestPath"
