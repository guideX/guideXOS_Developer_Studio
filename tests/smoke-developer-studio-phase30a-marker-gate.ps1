[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureSource = '',
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
    $EvidenceRoot = Join-Path (Split-Path -Parent $ServerRoot) ("g30a-marker-" + $evidenceToken)
}
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)

if (Test-Path -LiteralPath $EvidenceRoot) {
    if ((Get-ChildItem -LiteralPath $EvidenceRoot -Force | Measure-Object).Count -ne 0) {
        throw "Phase 30A marker evidence root must be new or empty: $EvidenceRoot"
    }
} else {
    New-Item -ItemType Directory -Path $EvidenceRoot | Out-Null
}

$script:MarkerRunToken = [Guid]::NewGuid().ToString('N')
$script:ManifestPath = Join-Path $EvidenceRoot 'phase30a-marker-run.json'
$script:Manifest = [ordered]@{
    schema = 'guideXOS.phase30a.marker-run.v1'
    runToken = $script:MarkerRunToken
    evidenceRoot = $EvidenceRoot
    fixtureSource = $FixtureSource
    serverRoot = $ServerRoot
    expectedOpenCloseCycles = 10
    state = 'running'
    startedAtUtc = [DateTime]::UtcNow.ToString('o')
    finishedAtUtc = $null
}
function Save-MarkerManifest() {
    if (-not (Test-Path -LiteralPath $EvidenceRoot -PathType Container)) {
        throw "Phase 30A test-infrastructure failure: marker evidence root is missing or moved; refusing to recreate it: $EvidenceRoot"
    }
    Set-Content -LiteralPath $script:ManifestPath -Value ($script:Manifest | ConvertTo-Json -Depth 5) -Encoding UTF8
}
function Assert-MarkerEvidenceRoot([string]$Stage) {
    if (-not (Test-Path -LiteralPath $EvidenceRoot -PathType Container)) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: evidence root is missing or moved; no fallback path will be used: $EvidenceRoot"
    }
    $resolved = [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $EvidenceRoot).Path)
    if ($resolved -ine $EvidenceRoot) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: expected evidence root '$EvidenceRoot', resolved '$resolved'"
    }
    if (-not (Test-Path -LiteralPath $script:ManifestPath -PathType Leaf)) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: recorded run manifest is missing: $script:ManifestPath"
    }
    $saved = Get-Content -LiteralPath $script:ManifestPath -Raw | ConvertFrom-Json
    if ($saved.runToken -ne $script:MarkerRunToken -or $saved.evidenceRoot -ine $EvidenceRoot) {
        throw "Phase 30A test-infrastructure failure during ${Stage}: evidence directory belongs to a different run"
    }
}

if (-not (Test-Path -LiteralPath $Phase20 -PathType Leaf)) { throw "Phase 20 hosted runner is missing: $Phase20" }
if (-not (Test-Path -LiteralPath $FixtureSource -PathType Container)) { throw "Fixture source is missing: $FixtureSource" }
if (-not (Test-Path -LiteralPath (Join-Path $ServerRoot 'guideXOSServer.experimental.exe') -PathType Leaf)) {
    throw "Experimental hosted Server is missing: $ServerRoot"
}
Save-MarkerManifest
Assert-MarkerEvidenceRoot 'preparation'
Write-Host "phase30a_marker_run_token=$script:MarkerRunToken"
Write-Host "phase30a_marker_evidence_root=$EvidenceRoot"
Write-Host "phase30a_marker_run_manifest=$script:ManifestPath"

$fixture = Join-Path $EvidenceRoot 'fixture-1'
$traceDirectory = Join-Path $EvidenceRoot 'traces-1'
New-Item -ItemType Directory -Path $fixture | Out-Null
Copy-Item -Path (Join-Path $FixtureSource '*') -Destination $fixture -Recurse -Force
foreach ($name in @('guidexos.debugger.json', 'guidexos.debugger.json.bak')) {
    $stateFile = Join-Path $fixture $name
    if (Test-Path -LiteralPath $stateFile -PathType Leaf) { Remove-Item -LiteralPath $stateFile -Force }
}
New-Item -ItemType Directory -Path $traceDirectory | Out-Null
$script:Manifest.fixture = $fixture
$script:Manifest.traceDirectory = $traceDirectory
Save-MarkerManifest
Assert-MarkerEvidenceRoot 'launch'

Write-Host 'Starting focused Manager condition-editor freshness gate: 10 open/close cycles, no retry'
& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $Phase20 `
    -Case ManagerOpenCloseMarker `
    -ServerRoot $ServerRoot `
    -FixtureRoot $fixture `
    -TraceDirectory $traceDirectory `
    -TraceArtifactName 'phase30a-marker-gate-10.log'
$childExitCode = $LASTEXITCODE
Assert-MarkerEvidenceRoot 'completion'
if ($childExitCode -ne 0) {
    $script:Manifest.state = 'failed'
    $script:Manifest.childExitCode = $childExitCode
    $script:Manifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
    Save-MarkerManifest
    throw "Phase 30A marker gate stopped without retry (child exit $childExitCode). Evidence: $EvidenceRoot"
}
$artifact = Join-Path $traceDirectory 'phase30a-marker-gate-10.log'
if (-not (Test-Path -LiteralPath $artifact -PathType Leaf)) {
    $script:Manifest.state = 'failed'
    $script:Manifest.error = 'successful child produced no marker gate artifact'
    $script:Manifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
    Save-MarkerManifest
    throw "Phase 30A test-infrastructure failure: marker gate artifact is missing: $artifact"
}
$script:Manifest.state = 'complete'
$script:Manifest.result = '10/10 PASS'
$script:Manifest.markerArtifact = $artifact
$script:Manifest.finishedAtUtc = [DateTime]::UtcNow.ToString('o')
Save-MarkerManifest
Write-Host 'phase30a_marker_gate=10/10 PASS'
Write-Host "phase30a_marker_evidence=$artifact"
