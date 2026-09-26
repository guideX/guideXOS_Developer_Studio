[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [int]$BreakpointLine = 37,
    [int]$DebugWaitSeconds = 240,
    [int]$MaxRuntimeSeconds = 360,
    [string]$TraceDirectory = '',
    [switch]$ContinueAfterFailure,
    [int]$MaxFailures = 1,
    [switch]$Phase28OOnly,
    [switch]$Phase28QOnly
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

if ($Phase28QOnly) {
    $bootstrap = Join-Path $ServerRoot 'scripts\smoke-compiler-bootstrap.ps1'
    if (-not (Test-Path -LiteralPath $bootstrap -PathType Leaf)) {
        throw "Phase 28Q QEMU harness is missing: $bootstrap"
    }
    $phase28qMarkers = @(
        'DEVELOPER_STUDIO_PHASE28Q_BEGIN',
        'DEVELOPER_STUDIO_PHASE28Q_APP_LAUNCH_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_PROJECT_OPEN_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_DEBUG_START_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_RUNNING_PASS',
        'DEVELOPER_STUDIO_PHASE28S_PARKED_PAUSE_CAPTURE_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_PAUSE_UI_REQUEST_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_PAUSE_REQUEST_ACCEPT_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_USER_PAUSE_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_CONTEXT_CAPTURE_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_CALL_STACK_PASS',
        'DEVELOPER_STUDIO_PHASE29A_STOP_MAPPING_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_CONTINUE_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_RUNNING_AFTER_CONTINUE_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_SECOND_PAUSE_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_NEW_STOP_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_CONTINUE_SECOND_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_FINAL_RESULT_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_NO_SKIP_DUPLICATE_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_PHASE28P_REGRESSION_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_CLEANUP_PASS',
        'DEVELOPER_STUDIO_PHASE28Q_PASS'
    )
    $arguments = New-ValidationArgumentList -ScriptPath $bootstrap -Parameters @{
        Phase28QOnly = $true
        BootCount = 3
        TimeoutSeconds = $MaxRuntimeSeconds
    }
    $result = Invoke-ValidationPowerShell -Name 'Phase 28Q debugger pause/continue' `
        -ScriptPath $bootstrap -ScriptArguments $arguments -MaxOutputLines 1200
    $evidencePath = ''
    foreach ($line in $result.OutputTail) {
        if ($line -match '^Phase 28Q evidence preserved at:\s*(.+)\s*$') {
            $evidencePath = $Matches[1].Trim()
        }
    }
    $evidenceText = @($result.OutputTail) -join "`n"
    $serialFiles = @()
    if ($evidencePath -and (Test-Path -LiteralPath $evidencePath -PathType Container)) {
        $serialFiles = @(Get-ChildItem -LiteralPath $evidencePath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'serial|qemu|trace' })
        foreach ($serialFile in $serialFiles) {
            $evidenceText += "`n" + [IO.File]::ReadAllText($serialFile.FullName)
        }
    }
    $missing = @($phase28qMarkers | Where-Object { -not $evidenceText.Contains($_) })
    if ($result.ExitCode -ne 0 -or $missing.Count -ne 0) {
        $artifact = Join-Path $TraceDirectory "developer-studio-phase28q-$runId.log"
        Write-BoundedValidationTrace -Path $artifact -Header @(
            'guideXOS Developer Studio Phase 28Q debugger pause/continue trace',
            "runId=$runId",
            "childExitCode=$($result.ExitCode)",
            "evidencePath=$evidencePath",
            "missingMarkers=$($missing -join ',')",
            "serverRoot=$ServerRoot"
        ) -TraceFiles @($serialFiles | ForEach-Object { $_.FullName }) -OutputTail $result.OutputTail
        throw "Phase 28Q debugger pause/continue failed: childExit=$($result.ExitCode) missing=$($missing -join ',')"
    }
    $previous = -1
    foreach ($marker in $phase28qMarkers) {
        $position = $evidenceText.IndexOf($marker, [StringComparison]::Ordinal)
        if ($position -lt 0 -or $position -le $previous) {
            throw "Phase 28Q marker ordering failed at $marker"
        }
        $previous = $position
    }
    Write-Host "phase28q_evidence_path=$evidencePath"
    Write-Host 'developer_studio_phase28q=PASS'
    exit 0
}

if ($Phase28OOnly) {
    $bootstrap = Join-Path $ServerRoot 'scripts\smoke-compiler-bootstrap.ps1'
    if (-not (Test-Path -LiteralPath $bootstrap -PathType Leaf)) {
        throw "Phase 28O QEMU harness is missing: $bootstrap"
    }

    $phase28oMarkers = @(
        'DEVELOPER_STUDIO_PHASE28O_BEGIN',
        'DEVELOPER_STUDIO_PHASE28O_CORRUPT_PASS',
        'DEVELOPER_STUDIO_PHASE28O_PROJECT_OPEN_PASS',
        'DEVELOPER_STUDIO_PHASE28O_CONFIGURE_PASS',
        'DEVELOPER_STUDIO_PHASE28O_CAPACITY_PASS',
        'DEVELOPER_STUDIO_PHASE28O_ISOLATION_PASS',
        'DEVELOPER_STUDIO_PHASE28O_SAVE_PASS',
        'DEVELOPER_STUDIO_PHASE28O_CLOSE_PASS',
        'DEVELOPER_STUDIO_PHASE28O_RELAUNCH_PASS',
        'DEVELOPER_STUDIO_PHASE28O_RESTORE_PASS',
        'DEVELOPER_STUDIO_PHASE28O_NO_RUNTIME_ID_PASS',
        'DEVELOPER_STUDIO_PHASE28O_DIRTY_BUFFER_PASS',
        'DEVELOPER_STUDIO_PHASE28N_EDITOR_READY_PASS',
        'DEVELOPER_STUDIO_PHASE28O_DEBUG_START_PASS',
        'DEVELOPER_STUDIO_PHASE28O_MATERIALIZE_PASS',
        'DEVELOPER_STUDIO_PHASE28O_HIT_COUNT_ZERO_PASS',
        'DEVELOPER_STUDIO_PHASE28O_UNRESOLVED_PASS',
        'DEVELOPER_STUDIO_PHASE28O_FRESH_ID_PASS',
        'DEVELOPER_STUDIO_PHASE28O_BREAKPOINT_HIT_PASS',
        'DEVELOPER_STUDIO_PHASE28N_EXECUTION_FRAME0_PASS',
        'DEVELOPER_STUDIO_PHASE28O_WATCH_REEVALUATE_PASS',
        'DEVELOPER_STUDIO_PHASE28N_RUNNING_MARKER_CLEAR_PASS',
        'DEVELOPER_STUDIO_PHASE28O_LOG_OUTPUT_PASS',
        'DEVELOPER_STUDIO_PHASE28O_RESET_PASS',
        'DEVELOPER_STUDIO_PHASE28O_TERMINAL_PASS',
        'DEVELOPER_STUDIO_PHASE28O_SECOND_GENERATION_PASS',
        'DEVELOPER_STUDIO_PHASE28N_NEW_SESSION_MARKER_PASS',
        'DEVELOPER_STUDIO_PHASE28O_BOUNDS_PASS',
        'DEVELOPER_STUDIO_PHASE28O_CLEANUP_PASS',
        'DEVELOPER_STUDIO_PHASE28O_PASS'
    )
    $arguments = New-ValidationArgumentList -ScriptPath $bootstrap -Parameters @{
        Phase28OOnly = $true
        BootCount = 1
        TimeoutSeconds = $MaxRuntimeSeconds
    }
    $result = Invoke-ValidationPowerShell -Name 'Phase 28O debugger workspace persistence' `
        -ScriptPath $bootstrap -ScriptArguments $arguments -MaxOutputLines 800

    $evidencePath = ''
    foreach ($line in $result.OutputTail) {
        if ($line -match '^Phase 28O evidence preserved at:\s*(.+)\s*$') {
            $evidencePath = $Matches[1].Trim()
        }
    }
    $evidenceText = @($result.OutputTail) -join "`n"
    $serialFiles = @()
    if ($evidencePath -and (Test-Path -LiteralPath $evidencePath -PathType Container)) {
        $serialFiles = @(Get-ChildItem -LiteralPath $evidencePath -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match 'serial|qemu|trace' })
        foreach ($serialFile in $serialFiles) {
            $evidenceText += "`n" + [IO.File]::ReadAllText($serialFile.FullName)
        }
    }

    $missing = @($phase28oMarkers | Where-Object { -not $evidenceText.Contains($_) })
    if ($result.ExitCode -ne 0 -or $missing.Count -ne 0) {
        $artifact = Join-Path $TraceDirectory "developer-studio-phase28o-$runId.log"
        Write-BoundedValidationTrace -Path $artifact -Header @(
            'guideXOS Developer Studio Phase 28O debugger workspace persistence trace',
            "runId=$runId",
            "childExitCode=$($result.ExitCode)",
            "evidencePath=$evidencePath",
            "missingMarkers=$($missing -join ',')",
            "serverRoot=$ServerRoot"
        ) -TraceFiles @($serialFiles.FullName) -OutputTail $result.OutputTail
        throw "Phase 28O debugger workspace persistence failed: childExit=$($result.ExitCode) missing=$($missing -join ',')"
    }

    $previous = -1
    foreach ($marker in $phase28oMarkers) {
        $position = $evidenceText.IndexOf($marker, [StringComparison]::Ordinal)
        if ($position -lt 0 -or $position -le $previous) {
            throw "Phase 28O marker ordering failed at $marker"
        }
        $previous = $position
    }
    Write-Host "phase28o_evidence_path=$evidencePath"
    Write-Host 'developer_studio_phase28o=PASS'
    exit 0
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
    if (-not $IncludeBreakpointLine) {
        $parameters.Remove('BreakpointLine')
        # Phase 16 owns its fixture through Phase 15's default and does not
        # expose a FixtureRoot parameter. Do not forward the common project
        # argument to that child.
        $parameters.Remove('FixtureRoot')
    }
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
