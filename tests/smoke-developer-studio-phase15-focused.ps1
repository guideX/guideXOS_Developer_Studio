[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO",
    [string]$FixtureRoot = "",
    [int]$BreakpointLine = 37,
    [string]$Condition = "counter == 2",
    [int]$DebugWaitSeconds = 90,
    [int]$MaxRuntimeSeconds = 300,
    [int]$UiWaitMilliseconds = 700,
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$phase15 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase15.ps1'
$phase20 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase20.ps1'

function Invoke-Phase20Case([string]$CaseName, [int]$TraceRunIndex) {
    Write-Host "Phase 20 focused smoke: $CaseName"
    $arguments = New-Object 'System.Collections.Generic.List[string]'
    $arguments.Add('-NoProfile')
    $arguments.Add('-ExecutionPolicy')
    $arguments.Add('Bypass')
    $arguments.Add('-File')
    $arguments.Add($phase20)
    $arguments.Add('-Case')
    $arguments.Add($CaseName)
    $arguments.Add('-ServerRoot')
    $arguments.Add($ServerRoot)
    if ($FixtureRoot) { $arguments.Add('-FixtureRoot'); $arguments.Add($FixtureRoot) }
    $arguments.Add('-BreakpointLine'); $arguments.Add([string]$BreakpointLine)
    $arguments.Add('-DebugWaitSeconds'); $arguments.Add([string]$DebugWaitSeconds)
    $arguments.Add('-MaxRuntimeSeconds'); $arguments.Add([string]$MaxRuntimeSeconds)
    $arguments.Add('-TraceRunIndex'); $arguments.Add([string]$TraceRunIndex)
    if ($KeepArtifacts) { $arguments.Add('-KeepArtifacts') }
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "Phase 20 focused smoke failed: $CaseName" }
}
$common = New-Object 'System.Collections.Generic.List[string]'
$common.Add('-NoProfile')
$common.Add('-ExecutionPolicy')
$common.Add('Bypass')
$common.Add('-File')
$common.Add($phase15)
$common.Add('-ServerRoot')
$common.Add($ServerRoot)
if ($FixtureRoot) {
    $common.Add('-FixtureRoot')
    $common.Add($FixtureRoot)
}
$common.Add('-BreakpointLine')
$common.Add([string]$BreakpointLine)
$common.Add('-DebugWaitSeconds')
$common.Add([string]$DebugWaitSeconds)
$common.Add('-MaxRuntimeSeconds')
$common.Add([string]$MaxRuntimeSeconds)
$common.Add('-UiWaitMilliseconds')
$common.Add([string]$UiWaitMilliseconds)
if ($KeepArtifacts) { $common.Add('-KeepArtifacts') }

$cases = @(
    @{ Name = 'conditional false-false-true runtime'; Args = @('-RuntimeOnly'); Condition = $Condition },
    @{ Name = 'Call Stack and selected frame'; Args = @('-FrameOnly'); Condition = $Condition },
    @{ Name = 'structured Locals tree'; Args = @('-TreeOnly'); Condition = $Condition },
    @{ Name = 'frame-sensitive Watch'; Args = @('-WatchOnly'); Condition = $Condition }
)

# The legacy all-in-one editor and ConditionError paths are retired from the
# authoritative suite. Phase 20 owns those proofs with targeted input,
# observable panel/modal state, recovery, and bounded failure traces.
Invoke-Phase20Case 'ConditionEditor' 201
Invoke-Phase20Case 'ConditionErrorRecovery' 202

foreach ($case in $cases) {
    Write-Host "Phase 15 focused smoke: $($case.Name)"
    $arguments = New-Object 'System.Collections.Generic.List[string]'
    foreach ($argument in $common) { $arguments.Add($argument) }
    $arguments.Add('-Condition')
    $arguments.Add($case.Condition)
    foreach ($argument in $case.Args) { $arguments.Add($argument) }
    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Phase 15 focused smoke failed: $($case.Name)"
    }
}

Write-Host 'Developer Studio Phase 15 focused smoke suite PASS'
