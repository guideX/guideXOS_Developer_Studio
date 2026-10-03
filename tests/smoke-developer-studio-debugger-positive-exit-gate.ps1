[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [Parameter(Mandatory = $true)]
    [string]$FixtureRoot,
    [ValidateRange(1, 25)]
    [int]$Iterations = 5,
    [ValidateRange(1, 10000)]
    [int]$StartIteration = 1,
    [ValidateRange(0, 255)]
    [int]$ExpectedTargetExitCode = 7,
    [ValidateRange(30, 600)]
    [int]$MaxRuntimeSeconds = 240,
    [ValidateRange(10, 300)]
    [int]$DebugWaitSeconds = 120,
    [string]$TraceDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. (Join-Path $PSScriptRoot 'DeveloperStudioValidation.Common.ps1')

$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
if (-not $TraceDirectory) {
    $TraceDirectory = Join-Path ([IO.Path]::GetTempPath()) ('guidexos-phase29r-hosted-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$TraceDirectory = [IO.Path]::GetFullPath($TraceDirectory)
if ($StartIteration + $Iterations - 1 -gt 10000) { throw 'StartIteration plus Iterations exceeds the bounded gate range.' }

$smoke = Join-Path $PSScriptRoot 'smoke-developer-studio-debugger.ps1'
$results = New-Object 'System.Collections.Generic.List[object]'
$started = Get-Date
Write-Host 'Developer Studio hosted target-exit gate'
Write-Host "server_root=$ServerRoot"
Write-Host "fixture_root=$FixtureRoot"
Write-Host "iterations=$Iterations start_iteration=$StartIteration expected_target_exit_code=$ExpectedTargetExitCode"
Write-Host "trace_directory=$TraceDirectory"
Write-Host 'failure_policy=stop on first failed iteration; no retries'

for ($offset = 0; $offset -lt $Iterations; ++$offset) {
    $iteration = $StartIteration + $offset
    $iterationName = '{0:D2}' -f $iteration
    $childTraceName = "hosted-target-exit-$iterationName-child.log"
    $failureTraceName = "hosted-target-exit-$iterationName-failure.log"
    $parameters = @{
        ServerRoot = $ServerRoot
        FixtureRoot = $FixtureRoot
        BreakpointLine = 4
        PositiveGxsmLifecycle = $true
        ExpectedTargetExitCode = $ExpectedTargetExitCode
        DebugWaitSeconds = $DebugWaitSeconds
        MaxRuntimeSeconds = $MaxRuntimeSeconds
        TraceDirectory = $TraceDirectory
        TraceRunIndex = 29000 + $iteration
        TraceArtifactName = $childTraceName
    }
    $arguments = New-ValidationArgumentList -ScriptPath $smoke -Parameters $parameters
    Write-Host ("hosted_target_exit_iteration_{0}/{1}=START" -f $iterationName, ($StartIteration + $Iterations - 1))
    $result = Invoke-ValidationPowerShell -Name ("hosted-target-exit-$iterationName") `
        -ScriptPath $smoke -ScriptArguments $arguments -MaxOutputLines 1200
    $results.Add($result)
    if ($result.ExitCode -ne 0) {
        Write-BoundedValidationTrace -Path (Join-Path $TraceDirectory $failureTraceName) -Header @(
            'guideXOS Developer Studio Phase 29R hosted nonzero-exit gate failure',
            "iteration=$iteration",
            "childExitCode=$($result.ExitCode)",
            "expectedTargetExitCode=$ExpectedTargetExitCode",
            "serverRoot=$ServerRoot",
            "fixtureRoot=$FixtureRoot"
        ) -TraceFiles @(Join-Path $TraceDirectory $childTraceName) -OutputTail $result.OutputTail
        throw "Hosted target-exit gate stopped at iteration $iteration; child exit code $($result.ExitCode)."
    }
    Write-Host ("hosted_target_exit_iteration_{0}=PASS target_sessions=2 exit_code={1}" -f $iterationName, $ExpectedTargetExitCode)
}

$elapsed = [Math]::Round(((Get-Date) - $started).TotalSeconds, 1)
Write-Host "hosted_target_exit_iterations=$($results.Count)/$Iterations"
Write-Host "hosted_target_exit_sessions=$($results.Count * 2)"
Write-Host "hosted_target_exit_elapsed_seconds=$elapsed"
Write-Host "hosted_target_exit_gate=PASS exit_code=$ExpectedTargetExitCode"
