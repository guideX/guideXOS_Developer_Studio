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
    @{ Name = 'condition editor retention'; Args = @('-EditorOnly'); Condition = $Condition },
    @{ Name = 'conditional false-false-true runtime'; Args = @('-RuntimeOnly'); Condition = $Condition },
    @{ Name = 'ConditionError'; Args = @('-ExpectConditionError'); Condition = 'unknownIdentifier' },
    @{ Name = 'Call Stack and selected frame'; Args = @('-FrameOnly'); Condition = $Condition },
    @{ Name = 'structured Locals tree'; Args = @('-TreeOnly'); Condition = $Condition },
    @{ Name = 'frame-sensitive Watch'; Args = @('-WatchOnly'); Condition = $Condition }
)

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
