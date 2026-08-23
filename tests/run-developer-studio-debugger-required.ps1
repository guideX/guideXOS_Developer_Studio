[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [int]$BreakpointLine = 37,
    [int]$DebugWaitSeconds = 240,
    [int]$MaxRuntimeSeconds = 360,
    [string]$TraceDirectory = '',
    [switch]$ContinueAfterFailure,
    [int]$MaxFailures = 1
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. (Join-Path $PSScriptRoot 'DeveloperStudioValidation.Common.ps1')

$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
if (-not $TraceDirectory) { $TraceDirectory = Join-Path $repoRoot 'logs' }
$TraceDirectory = [IO.Path]::GetFullPath($TraceDirectory)
if ($MaxFailures -lt 1 -or $MaxFailures -gt 8) { throw 'MaxFailures must be between 1 and 8.' }

$phase15 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase15.ps1'
$phase20 = Join-Path $PSScriptRoot 'smoke-developer-studio-phase20.ps1'
$phase16 = Join-Path $PSScriptRoot 'smoke-hosted-input-delivery-phase16.ps1'
$debugger = Join-Path $PSScriptRoot 'smoke-developer-studio-debugger.ps1'
$runId = Get-Date -Format 'yyyyMMdd-HHmmss'
$common = @{
    ServerRoot = $ServerRoot
    FixtureRoot = $FixtureRoot
    BreakpointLine = $BreakpointLine
    DebugWaitSeconds = $DebugWaitSeconds
    MaxRuntimeSeconds = $MaxRuntimeSeconds
    TraceDirectory = $TraceDirectory
}

function New-SuiteParameters {
    param(
        [hashtable]$Additional,
        [int]$TraceIndex,
        [string]$ChildTraceName,
        [bool]$IncludeBreakpointLine = $true
    )
    $parameters = @{}
    foreach ($key in $common.Keys) { $parameters[$key] = $common[$key] }
    if (-not $IncludeBreakpointLine) { $parameters.Remove('BreakpointLine') }
    foreach ($key in $Additional.Keys) { $parameters[$key] = $Additional[$key] }
    $parameters.TraceRunIndex = $TraceIndex
    $parameters.TraceArtifactName = $ChildTraceName
    return $parameters
}

$suites = @(
    [pscustomobject]@{ Name = 'condition-editor'; Label = 'condition editor'; Script = $phase20; Parameters = New-SuiteParameters @{ Case = 'ConditionEditor' } 2101 'developer-studio-debugger-required-condition-editor-child.log' },
    [pscustomobject]@{ Name = 'condition-error-recovery'; Label = 'ConditionError/recovery'; Script = $phase20; Parameters = New-SuiteParameters @{ Case = 'ConditionErrorRecovery' } 2102 'developer-studio-debugger-required-condition-error-recovery-child.log' },
    [pscustomobject]@{ Name = 'conditional-runtime'; Label = 'conditional false-false-true runtime'; Script = $phase15; Parameters = New-SuiteParameters @{ RuntimeOnly = $true; Condition = 'counter == 2' } 2103 'developer-studio-debugger-required-conditional-runtime-child.log' },
    [pscustomobject]@{ Name = 'selected-frame'; Label = 'selected-frame inspection'; Script = $phase15; Parameters = New-SuiteParameters @{ FrameOnly = $true; Condition = 'counter == 2' } 2104 'developer-studio-debugger-required-selected-frame-child.log' },
    [pscustomobject]@{ Name = 'structured-tree'; Label = 'structured Locals tree'; Script = $phase15; Parameters = New-SuiteParameters @{ TreeOnly = $true; Condition = 'counter == 2' } 2105 'developer-studio-debugger-required-structured-tree-child.log' },
    [pscustomobject]@{ Name = 'watch'; Label = 'Watch generation/inspection'; Script = $phase15; Parameters = New-SuiteParameters @{ WatchOnly = $true; Condition = 'counter == 2' } 2106 'developer-studio-debugger-required-watch-child.log' },
    [pscustomobject]@{ Name = 'input-delivery'; Label = 'input delivery'; Script = $phase16; Parameters = New-SuiteParameters @{} 2107 'developer-studio-debugger-required-input-delivery-child.log' $false },
    # Phase 8's checked-in fixture maps its Step Out breakpoint to line 9;
    # keep that fixture-specific source location explicit in the required tier.
    [pscustomobject]@{ Name = 'step-out-lifecycle'; Label = 'Step Out lifecycle and targeted shutdown'; Script = $debugger; Parameters = New-SuiteParameters @{ StepOut = $true; BreakpointLine = 9 } 2108 'developer-studio-debugger-required-step-out-child.log' }
)

$results = New-Object 'System.Collections.Generic.List[object]'
$failureCount = 0
$suiteNumber = 0
$started = Get-Date
Write-Host 'Developer Studio debugger required tier'
Write-Host "server_root=$ServerRoot"
Write-Host "trace_directory=$TraceDirectory"
Write-Host 'failure_policy=' + $(if ($ContinueAfterFailure) { "continue through $MaxFailures failures" } else { 'stop on first failure' })

foreach ($suite in $suites) {
    ++$suiteNumber
    $safeChild = Join-Path $TraceDirectory ([string]$suite.Parameters.TraceArtifactName)
    Remove-Item -LiteralPath $safeChild -Force -ErrorAction SilentlyContinue
    $arguments = New-ValidationArgumentList -ScriptPath $suite.Script -Parameters $suite.Parameters
    $result = Invoke-ValidationPowerShell -Name $suite.Label -ScriptPath $suite.Script -ScriptArguments $arguments
    $results.Add($result)
    if ($result.ExitCode -ne 0) {
        ++$failureCount
        $safeName = $suite.Name -replace '[^A-Za-z0-9_-]', '-'
        $artifact = Join-Path $TraceDirectory "developer-studio-debugger-required-$safeName-$runId.log"
        Write-BoundedValidationTrace -Path $artifact -Header @(
            'guideXOS Developer Studio required debugger-tier failure trace',
            "suite=$($suite.Name)",
            "suiteLabel=$($suite.Label)",
            "runId=$runId",
            "childExitCode=$($result.ExitCode)",
            "serverRoot=$ServerRoot"
        ) -TraceFiles @($safeChild) -OutputTail $result.OutputTail
        if (-not $ContinueAfterFailure -or $failureCount -ge $MaxFailures) { break }
    } else {
        Write-Host "$($suite.Name)=PASS"
    }
}

$elapsed = [Math]::Round(((Get-Date) - $started).TotalSeconds, 1)
Write-Host "required_suite_elapsed_seconds=$elapsed"
foreach ($result in $results) {
    $status = if ($result.ExitCode -eq 0) { 'PASS' } else { 'FAIL' }
    Write-Host ("required_suite_{0}={1} elapsed={2}s" -f $result.Name, $status, $result.ElapsedSeconds)
}
if ($failureCount -gt 0 -or $results.Count -ne $suites.Count) {
    throw "Developer Studio debugger required tier failed: failures=$failureCount completed=$($results.Count)/$($suites.Count)"
}
Write-Host 'developer_studio_debugger_required=PASS'
