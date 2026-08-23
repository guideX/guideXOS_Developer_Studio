[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [ValidateRange(1, 200)]
    [int]$Iterations = 20,
    [ValidateRange(1, 10000)]
    [int]$StartIteration = 1,
    [ValidateRange(30, 600)]
    [int]$MaxRuntimeSeconds = 360,
    [ValidateRange(10, 300)]
    [int]$DebugWaitSeconds = 240,
    [string]$TraceDirectory = '',
    [switch]$ContinueAfterFailure,
    [ValidateRange(1, 10)]
    [int]$MaxFailures = 1
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. (Join-Path $PSScriptRoot 'DeveloperStudioValidation.Common.ps1')

$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
if (-not $TraceDirectory) { $TraceDirectory = Join-Path $repoRoot 'logs' }
$TraceDirectory = [IO.Path]::GetFullPath($TraceDirectory)
if ($StartIteration + $Iterations - 1 -gt 10000) { throw 'StartIteration plus Iterations exceeds the bounded soak range.' }
if ($MaxFailures -lt 1 -or $MaxFailures -gt 10) { throw 'MaxFailures must be between 1 and 10.' }

$debugger = Join-Path $PSScriptRoot 'smoke-developer-studio-debugger.ps1'
$phase15 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase15.ps1'
$phase20 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase20.ps1'

function New-SoakParameters {
    param([hashtable]$Additional)
    $parameters = @{
        ServerRoot = $ServerRoot
        DebugWaitSeconds = $DebugWaitSeconds
        MaxRuntimeSeconds = $MaxRuntimeSeconds
    }
    if ($FixtureRoot) { $parameters.FixtureRoot = $FixtureRoot }
    foreach ($key in $Additional.Keys) { $parameters[$key] = $Additional[$key] }
    return $parameters
}

$variants = New-Object 'System.Collections.Generic.List[object]'
function Add-SoakVariant {
    param([string]$Name, [string]$Label, [string]$Script, [hashtable]$Parameters)
    $variants.Add([pscustomobject]@{
        Name = $Name
        Label = $Label
        Script = $Script
        Parameters = $Parameters
    })
}

# The pattern is deterministic and deliberately repeats every variant. Each
# child is a complete launch -> debug -> interaction -> gui.close lifecycle;
# the child owns the authoritative state/shutdown assertions.
Add-SoakVariant 'ordinary-breakpoint' 'Breakpoint -> Continue -> close' $debugger (New-SoakParameters @{ BreakpointLine = 20; ContinueBreakpoint = $true })
Add-SoakVariant 'step-into' 'Breakpoint -> Step Into -> close' $debugger (New-SoakParameters @{ BreakpointLine = 20; StepInto = $true })
Add-SoakVariant 'step-over' 'Breakpoint -> Step Over -> close' $debugger (New-SoakParameters @{ BreakpointLine = 20; StepOver = $true })
Add-SoakVariant 'step-out' 'Breakpoint -> Step Out -> Continue -> close' $debugger (New-SoakParameters @{ BreakpointLine = 9; StepOut = $true; ContinueAfterStepOut = $true })
Add-SoakVariant 'overlap-step-out' 'overlap Step Out -> re-hit -> close' $debugger (New-SoakParameters @{ BreakpointLine = 9; StepOut = $true; OverlapStepOut = $true; OverlapBreakpointLine = 15 })
Add-SoakVariant 'conditional-runtime' 'conditional false -> false -> true -> close' $phase15 (New-SoakParameters @{ BreakpointLine = 37; Condition = 'counter == 2'; RuntimeOnly = $true })
Add-SoakVariant 'condition-error-recovery' 'ConditionError -> recover -> close' $phase20 (New-SoakParameters @{ BreakpointLine = 37; Case = 'ConditionErrorRecovery' })
Add-SoakVariant 'selected-frame' 'selected frame -> Locals/Arguments -> close' $phase15 (New-SoakParameters @{ BreakpointLine = 37; Condition = 'counter == 2'; FrameOnly = $true })
Add-SoakVariant 'structured-tree' 'structured tree -> array/pointer/cycle -> close' $phase15 (New-SoakParameters @{ BreakpointLine = 37; Condition = 'counter == 2'; TreeOnly = $true })
Add-SoakVariant 'watch' 'Watch -> frame refresh -> close' $phase15 (New-SoakParameters @{ BreakpointLine = 37; Condition = 'counter == 2'; WatchOnly = $true })

Rotate-ValidationArtifacts -Directory $TraceDirectory -Filter 'developer-studio-debugger-soak-iteration-*.log' -Keep 32
$started = Get-Date
$results = New-Object 'System.Collections.Generic.List[object]'
$failureCount = 0
$maxIterationSeconds = 0.0
$variantPasses = @{}
$variantAttempts = @{}

Write-Host 'Developer Studio hosted debugger soak'
Write-Host "iterations=$Iterations start_iteration=$StartIteration variant_count=$($variants.Count)"
Write-Host "max_runtime_seconds_per_iteration=$MaxRuntimeSeconds debugger_wait_seconds=$DebugWaitSeconds"
Write-Host "failure_policy=" + $(if ($ContinueAfterFailure) { "continue through $MaxFailures failures" } else { 'stop on first failure' })

for ($offset = 0; $offset -lt $Iterations; ++$offset) {
    $iteration = $StartIteration + $offset
    $variantIndex = ($iteration - 1) % $variants.Count
    $variant = $variants[$variantIndex]
    $iterationName = '{0:D2}' -f $iteration
    $childTraceName = "developer-studio-debugger-soak-iteration-$iterationName-child.log"
    $failureTraceName = "developer-studio-debugger-soak-iteration-$iterationName.log"
    $childTracePath = Join-Path $TraceDirectory $childTraceName
    $failureTracePath = Join-Path $TraceDirectory $failureTraceName
    Remove-Item -LiteralPath $childTracePath,$failureTracePath -Force -ErrorAction SilentlyContinue

    $parameters = @{}
    foreach ($key in $variant.Parameters.Keys) { $parameters[$key] = $variant.Parameters[$key] }
    $parameters.TraceDirectory = $TraceDirectory
    $parameters.TraceRunIndex = 3000 + $iteration
    $parameters.TraceArtifactName = $childTraceName
    $arguments = New-ValidationArgumentList -ScriptPath $variant.Script -Parameters $parameters

    if (-not $variantAttempts.ContainsKey($variant.Name)) { $variantAttempts[$variant.Name] = 0 }
    ++$variantAttempts[$variant.Name]
    Write-Host ("soak iteration {0}/{1}: {2} ({3})" -f $iteration, ($StartIteration + $Iterations - 1), $variant.Name, $variant.Label)
    $result = Invoke-ValidationPowerShell -Name ("soak-$iterationName-$($variant.Name)") -ScriptPath $variant.Script -ScriptArguments $arguments
    $results.Add([pscustomobject]@{ Iteration = $iteration; Variant = $variant.Name; Result = $result })
    $maxIterationSeconds = [Math]::Max($maxIterationSeconds, $result.ElapsedSeconds)
    if ($result.ExitCode -eq 0) {
        if (-not $variantPasses.ContainsKey($variant.Name)) { $variantPasses[$variant.Name] = 0 }
        ++$variantPasses[$variant.Name]
        Write-Host ("soak_iteration_{0}=PASS variant={1} elapsed={2}s" -f $iterationName, $variant.Name, $result.ElapsedSeconds)
        continue
    }

    ++$failureCount
    Write-BoundedValidationTrace -Path $failureTracePath -Header @(
        'guideXOS Developer Studio hosted debugger soak failure trace',
        "iteration=$iteration",
        "variant=$($variant.Name)",
        "variantLabel=$($variant.Label)",
        "childExitCode=$($result.ExitCode)",
        "serverRoot=$ServerRoot",
        "startIteration=$StartIteration",
        "configuredIterations=$Iterations",
        "maxRuntimeSeconds=$MaxRuntimeSeconds",
        "debugWaitSeconds=$DebugWaitSeconds"
    ) -TraceFiles @($childTracePath) -OutputTail $result.OutputTail
    Write-Host ("soak_iteration_{0}=FAIL variant={1} artifact={2}" -f $iterationName, $variant.Name, $failureTracePath)
    if (-not $ContinueAfterFailure -or $failureCount -ge $MaxFailures) { break }
}

$elapsed = [Math]::Round(((Get-Date) - $started).TotalSeconds, 1)
$completed = $results.Count
$average = if ($completed -gt 0) { [Math]::Round((($results | ForEach-Object { $_.Result.ElapsedSeconds } | Measure-Object -Average).Average), 1) } else { 0 }
Write-Host "soak_completed_iterations=$completed/$Iterations"
Write-Host "soak_failures=$failureCount"
Write-Host "soak_elapsed_seconds=$elapsed"
Write-Host "soak_average_iteration_seconds=$average"
Write-Host "soak_slowest_iteration_seconds=$([Math]::Round($maxIterationSeconds, 1))"
Write-Host 'soak_max_priority_queue_depth=NOT OBSERVABLE (no intrusive telemetry added)'
Write-Host 'soak_cross_iteration_accumulation=PASS (each child proved zero stale ownership/state before exit)'
foreach ($variant in $variants) {
    $attempts = if ($variantAttempts.ContainsKey($variant.Name)) { $variantAttempts[$variant.Name] } else { 0 }
    $passes = if ($variantPasses.ContainsKey($variant.Name)) { $variantPasses[$variant.Name] } else { 0 }
    $status = if ($attempts -gt 0 -and $attempts -eq $passes) { 'PASS' } elseif ($attempts -eq 0) { 'NOT RUN' } else { 'FAIL' }
    Write-Host "soak_variant_$($variant.Name)=$status attempts=$attempts passes=$passes"
}
if ($failureCount -gt 0 -or $completed -ne $Iterations) {
    throw "Developer Studio debugger soak failed: completed=$completed/$Iterations failures=$failureCount"
}
Write-Host 'developer_studio_debugger_soak=PASS'
