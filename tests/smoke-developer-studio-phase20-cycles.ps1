[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [int]$BreakpointLine = 20,
    [int]$DebugWaitSeconds = 120,
    [int]$MaxRuntimeSeconds = 360,
    [string]$TraceDirectory = ''
)

$ErrorActionPreference = 'Stop'
$debuggerSmoke = Join-Path $PSScriptRoot 'smoke-developer-studio-debugger.ps1'
if (-not $TraceDirectory) { $TraceDirectory = Join-Path (Split-Path -Parent $PSScriptRoot) 'logs' }

$cycles = @(
    @{ Number = 1; Name = 'continue'; Args = @('-ContinueBreakpoint') },
    @{ Number = 2; Name = 'step-out'; Args = @('-StepOut') },
    @{ Number = 3; Name = 'step-out-continue'; Args = @('-StepOut', '-ContinueAfterStepOut') },
    @{ Number = 4; Name = 'step-out-into'; Args = @('-StepOut', '-StepOutThenStepInto') },
    # Keep structured inspection as its own lifecycle variant. The legacy
    # aggregate asserts a post-Watch F5/rebind sequence, while this cycle's
    # contract is inspection followed by targeted close from the stopped
    # session; the dedicated Continue cycle already proves F5/rebind.
    @{ Number = 5; Name = 'watch-inspection'; Args = @('-InteractiveWatch') }
)

foreach ($cycle in $cycles) {
    Write-Host "Phase 20 lifecycle cycle $($cycle.Number): $($cycle.Name)"
    $arguments = New-Object 'System.Collections.Generic.List[string]'
    $arguments.Add('-NoProfile')
    $arguments.Add('-ExecutionPolicy')
    $arguments.Add('Bypass')
    $arguments.Add('-File')
    $arguments.Add($debuggerSmoke)
    $arguments.Add('-ServerRoot')
    $arguments.Add($ServerRoot)
    if ($FixtureRoot) { $arguments.Add('-FixtureRoot'); $arguments.Add($FixtureRoot) }
    $arguments.Add('-BreakpointLine'); $arguments.Add([string]$BreakpointLine)
    $arguments.Add('-DebugWaitSeconds'); $arguments.Add([string]$DebugWaitSeconds)
    $arguments.Add('-MaxRuntimeSeconds'); $arguments.Add([string]$MaxRuntimeSeconds)
    $arguments.Add('-TraceDirectory'); $arguments.Add($TraceDirectory)
    $arguments.Add('-TraceRunIndex'); $arguments.Add([string]$cycle.Number)
    foreach ($argument in $cycle.Args) { $arguments.Add($argument) }

    & powershell.exe @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Phase 20 lifecycle cycle $($cycle.Number) failed: $($cycle.Name)"
    }
    Write-Host "debug_cycle_$($cycle.Number)=PASS"
}

Write-Host 'debug_cycles=5/5 PASS'
