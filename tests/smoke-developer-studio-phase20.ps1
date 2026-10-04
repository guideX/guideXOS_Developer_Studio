[CmdletBinding()]
param(
    [ValidateSet('ConditionEditor', 'ConditionErrorRecovery', 'ControllerPanelConditionEditor', 'Phase29NReadiness')]
    [string]$Case = 'ConditionEditor',
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [int]$BreakpointLine = 37,
    [int]$DebugWaitSeconds = 90,
    [int]$MaxRuntimeSeconds = 300,
    [string]$TraceDirectory = '',
    [int]$TraceRunIndex = 0,
    [string]$TraceArtifactName = '',
    [switch]$KeepArtifacts
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $FixtureRoot) { $FixtureRoot = Join-Path $RepoRoot 'tests\fixtures\debugger-phase15' }
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
$Executable = Join-Path $ServerRoot 'guideXOSServer.experimental.exe'
$FixtureProject = Get-Content -LiteralPath (Join-Path $FixtureRoot 'guidexos.project') -Raw | ConvertFrom-Json
$script:FixtureProjectId = [string]$FixtureProject.projectId
if (-not $script:FixtureProjectId) { throw 'Phase 20 hosted smoke fixture has no project ID' }
$script:Parts = New-Object 'System.Collections.Generic.List[string]'
$script:LastInput = ''
$script:ExpectedMarker = ''
$script:Process = $null
$script:StdoutPath = ''
$script:StderrPath = ''
$script:CapturedText = ''
$script:WindowId = 1000
$script:MarkerBaselines = @{}
$script:PanelReady = $false

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Phase 20 hosted smoke failed: $Message" }
}
function Add-Command([string]$Command) {
    $script:Parts.Add("COMMAND|$Command")
    $script:LastInput = $Command
}

function Add-LogSnapshot() {
    # Keep the synchronization point bounded.  The hosted command stream is
    # already flushed by the targeted input path; issuing `log` here would
    # dump the entire compositor frame after every modal transition.
    Add-WaitMilliseconds 500
}

function Add-Wait([int]$Seconds = 1) {
    $script:Parts.Add("WAIT|$([Math]::Max(1, $Seconds))")
}

function Add-WaitMilliseconds([int]$Milliseconds = 150) {
    $script:Parts.Add("WAITMS|$([Math]::Max(50, $Milliseconds))")
}

function Add-WaitMarker([string]$Marker) {
    $script:Parts.Add("WAITMARK|$Marker")
}

function Add-WaitMarkerFresh([string]$Marker) {
    $script:Parts.Add("WAITMARKFRESH|$Marker")
}

function Add-CaptureMarker([string]$Marker) {
    $script:Parts.Add("CAPTURE|$Marker")
}

function Add-Key([int]$KeyCode, [int]$Modifiers = 0) {
    Add-Command "gui.keyto $script:WindowId $KeyCode down $Modifiers"
}

function Add-Click([int]$X, [int]$Y) {
    Add-Command "gui.mouse $script:WindowId $X $Y 1 down"
    Add-Command "gui.mouse $script:WindowId $X $Y 1 up"
}

function Get-Key([char]$Character) {
    if (($Character -cge 'a') -and ($Character -cle 'z')) {
        return @{ Key = [int][char](([string]$Character).ToUpperInvariant()); Modifiers = 0 }
    }
    if (($Character -cge 'A') -and ($Character -cle 'Z')) {
        return @{ Key = [int][char]$Character; Modifiers = 1 }
    }
    if (($Character -ge '0') -and ($Character -le '9')) {
        return @{ Key = [int][char]$Character; Modifiers = 0 }
    }
    switch ($Character) {
        ':' { return @{ Key = 186; Modifiers = 1 } }
        '\' { return @{ Key = 220; Modifiers = 0 } }
        ' ' { return @{ Key = 32; Modifiers = 0 } }
        '=' { return @{ Key = 187; Modifiers = 0 } }
        '>' { return @{ Key = 190; Modifiers = 1 } }
        '_' { return @{ Key = 189; Modifiers = 1 } }
        '-' { return @{ Key = 189; Modifiers = 0 } }
        default { throw "Unsupported Phase 20 smoke character: $Character" }
    }
}

function Add-Text([string]$Value) {
    foreach ($character in $Value.ToCharArray()) {
        $key = Get-Key $character
        Add-Key $key.Key $key.Modifiers
        # Keep the bounded priority lane below its configured cap while the
        # real compositor redraws the editor after each targeted key.
        Add-WaitMilliseconds 120
    }
}

function Add-ConditionText([string]$Value) {
    Add-CaptureMarker " text=$Value"
    Add-Key 65 2 # Ctrl+A targets the active condition editor buffer.
    Add-WaitMilliseconds 120
    Add-Text $Value
    # This marker is emitted by the real modal input handler after every
    # character. Waiting for the complete value proves ordered delivery and
    # removes the old per-character timing chain.
    Add-WaitMarkerFresh " text=$Value"
}

function Add-OpenDebugBreakpoints() {
    if ($script:PanelReady) { return }
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_panel_open=PASS tab=0 count='
    Add-Command "gui.activate $script:WindowId"
    Add-Wait 1
    Add-Click 610 30
    Add-Wait 1
    Add-Click 620 230
    # The condition-editor-open marker is the authoritative render/state
    # observable for the next action.  The menu geometry is kept only for the
    # supported real-UI navigation path; all text and modal actions are
    # targeted to the known Developer Studio window.
    Add-Wait 3
    Add-LogSnapshot
    Add-Wait 3
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_panel_open=PASS tab=0 count='
    # Focus the first visible row, then advance through the panel's semantic
    # selection key. Setup normalizes stale rows before this helper is used.
    Add-Click 200 148
    Add-WaitMilliseconds 200
    Add-Key 40
    Add-WaitMilliseconds 200
    $script:PanelReady = $true
}

function Add-OpenConditionEditor([string]$ExpectedText = '', [string]$ManagerSnapshot = 'absent') {
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id='
    Add-CaptureMarker 'origin=editor editor_row='
    Add-CaptureMarker "manager_snapshot=$ManagerSnapshot"
    if ($ExpectedText) { Add-CaptureMarker " text=$ExpectedText" }
    # Exercise the editor-owned selection route directly. This stays available
    # when the Manager snapshot has not been published for the active session.
    Add-Command "gui.activate $script:WindowId"
    Add-Click 610 30
    Add-Click 620 360
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id='
    Add-WaitMarkerFresh 'origin=editor editor_row='
    Add-WaitMarkerFresh "manager_snapshot=$ManagerSnapshot"
    if ($ExpectedText) { Add-WaitMarkerFresh " text=$ExpectedText" }
}

function Add-CommitCondition([string]$Value) {
    Add-CaptureMarker " text=$Value parse=VALID state=CONDITIONAL"
    Add-CaptureMarker 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id='
    Add-CaptureMarker "debug_condition_persist=PASS source=src/main.cpp:$BreakpointLine"
    Add-Key 13
    Add-LogSnapshot
    Add-WaitMarkerFresh " text=$Value parse=VALID state=CONDITIONAL"
    Add-WaitMarkerFresh 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id='
    Add-WaitMarkerFresh "debug_condition_persist=PASS source=src/main.cpp:$BreakpointLine"
}

function Add-InvalidCondition([string]$Value) {
    Add-CaptureMarker " text=$Value parse=INVALID state=UNCHANGED"
    Add-CaptureMarker 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id='
    Add-Key 13
    Add-LogSnapshot
    Add-WaitMarkerFresh " text=$Value parse=INVALID state=UNCHANGED"
    Add-WaitMarkerFresh 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id='
}

function Add-CancelCondition([string]$Value, [string]$CommittedValue) {
    Add-CaptureMarker " text=$Value"
    Add-Key 65 2
    Add-WaitMilliseconds 120
    Add-Text $Value
    Add-LogSnapshot
    Add-WaitMarkerFresh " text=$Value"
    Add-CaptureMarker 'debug_condition_editor=CLOSED reason=CANCEL breakpoint_id='
    Add-Key 27
    Add-LogSnapshot
    Add-WaitMarkerFresh 'debug_condition_editor=CLOSED reason=CANCEL breakpoint_id='
    Add-OpenConditionEditor $CommittedValue
}

function Add-ClearCondition() {
    Add-CaptureMarker 'parse=EMPTY state=UNCONDITIONAL'
    Add-CaptureMarker 'debug_condition_editor=CLOSED reason=CLEAR breakpoint_id='
    Add-Key 88
    Add-LogSnapshot
    Add-WaitMarkerFresh 'parse=EMPTY state=UNCONDITIONAL'
    Add-WaitMarkerFresh 'debug_condition_editor=CLOSED reason=CLEAR breakpoint_id='
}

function Add-TargetedClose() {
    Add-Command "gui.close $script:WindowId"
    $script:Parts.Add("WAITSHUTDOWN|$([Math]::Max(1, $DebugWaitSeconds))")
    Add-Command 'exit'
}

function Get-LiveText() {
    # Developer Studio's compositor trace is intentionally verbose.  Read a
    # bounded tail for polling and failure diagnostics so the harness itself
    # cannot turn a UI smoke into an unbounded memory/disk exercise.
    $stdout = if (Test-Path -LiteralPath $script:StdoutPath) {
        (Get-Content -LiteralPath $script:StdoutPath -Tail 6000 -ErrorAction SilentlyContinue) -join "`n"
    } else { '' }
    $stderr = if (Test-Path -LiteralPath $script:StderrPath) {
        (Get-Content -LiteralPath $script:StderrPath -Tail 1000 -ErrorAction SilentlyContinue) -join "`n"
    } else { '' }
    return $stdout + "`n" + $stderr
}

function Count-Occurrences([string]$Content, [string]$Needle) {
    if (-not $Content -or -not $Needle) { return 0 }
    $count = 0
    $offset = 0
    while ($offset -lt $Content.Length) {
        $index = $Content.IndexOf($Needle, $offset, [StringComparison]::Ordinal)
        if ($index -lt 0) { break }
        ++$count
        $offset = $index + $Needle.Length
    }
    return $count
}

function Get-MarkerCount([string]$Marker) {
    if (-not (Test-Path -LiteralPath $script:StdoutPath) -or -not $Marker) { return 0 }
    # Search the redirected stream without materializing compositor output.
    # Marker lines are sparse, so this remains bounded in memory even when the
    # native renderer emits a large amount of draw diagnostics.
    return [int](@(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern $Marker -ErrorAction SilentlyContinue).Count)
}

function Test-Marker([string]$Marker) {
    if (-not (Test-Path -LiteralPath $script:StdoutPath) -or -not $Marker) { return $false }
    return [bool](Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern $Marker -Quiet -ErrorAction SilentlyContinue)
}

function Test-PersistedBreakpoint([string]$SourcePath, [int]$Line, [int]$Column) {
    $workspacePath = Join-Path $FixtureRoot 'guidexos.debugger.json'
    if (-not (Test-Path -LiteralPath $workspacePath -PathType Leaf)) { return $false }
    try {
        $workspace = Get-Content -LiteralPath $workspacePath -Raw | ConvertFrom-Json
        foreach ($breakpoint in @($workspace.breakpoints)) {
            if ($breakpoint.sourcePath -eq $SourcePath -and
                [int]$breakpoint.line -eq $Line -and
                [int]$breakpoint.column -eq $Column -and
                [bool]$breakpoint.enabled -and
                $breakpoint.action -eq 'BREAK') { return $true }
        }
    } catch { return $false }
    return $false
}

function Get-PersistedBreakpointRecordCount([string]$SourcePath, [int]$Line, [int]$Column) {
    $workspacePath = Join-Path $FixtureRoot 'guidexos.debugger.json'
    if (-not (Test-Path -LiteralPath $workspacePath -PathType Leaf)) { return 0 }
    try {
        $workspace = Get-Content -LiteralPath $workspacePath -Raw | ConvertFrom-Json
        return @($workspace.breakpoints | Where-Object {
            $_.sourcePath -eq $SourcePath -and [int]$_.line -eq $Line -and
            [int]$_.column -eq $Column -and $_.action -eq 'BREAK'
        }).Count
    } catch { return 0 }
}

function Test-PersistedCondition([string]$SourcePath, [int]$Line, [int]$Column, [string]$Condition) {
    $workspacePath = Join-Path $FixtureRoot 'guidexos.debugger.json'
    if (-not (Test-Path -LiteralPath $workspacePath -PathType Leaf)) { return $false }
    try {
        $workspace = Get-Content -LiteralPath $workspacePath -Raw | ConvertFrom-Json
        $matches = @($workspace.breakpoints | Where-Object {
            $_.sourcePath -eq $SourcePath -and [int]$_.line -eq $Line -and
            [int]$_.column -eq $Column -and $_.action -eq 'BREAK'
        })
        return ($matches.Count -eq 1 -and [string]$matches[0].condition -eq $Condition)
    } catch { return $false }
}

function Get-PanelBreakpointCount() {
    $needle = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_panel_open=PASS tab=0 count='
    $match = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern $needle -ErrorAction SilentlyContinue |
        Select-Object -Last 1)
    if ($match.Count -eq 0) { return -1 }
    if ($match[0].Line -match 'count=(\d+)') { return [int]$Matches[1] }
    return -1
}

function Get-RecentDiagnostic([string]$Content) {
    return @($Content -split "`r?`n" |
        Where-Object { $_ -match 'debug_condition|debug_state|debug_shutdown|shutdownStage=|Debug: breakpoint|Key queued|Mouse queued|windowCount=|Native app processes:' } |
        Select-Object -Last 120) -join "`n"
}

function Wait-Marker([string]$Marker, [int]$TimeoutSeconds = $DebugWaitSeconds) {
    $script:ExpectedMarker = $Marker
    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $TimeoutSeconds))
    while ((Get-Date) -lt $deadline) {
        if (Test-Marker $Marker) { return }
        if ($script:Process.HasExited) {
            $content = Get-LiveText
            throw "Expected marker was not observed before Server exit: $Marker`n$(Get-RecentDiagnostic $content)"
        }
        Start-Sleep -Milliseconds 200
    }
    $content = Get-LiveText
    throw "Timed out waiting for marker: $Marker`n$(Get-RecentDiagnostic $content)"
}

function Wait-MarkerFresh([string]$Marker, [int]$TimeoutSeconds = $DebugWaitSeconds) {
    $script:ExpectedMarker = $Marker
    $baseline = if ($script:MarkerBaselines.ContainsKey($Marker)) {
        $script:MarkerBaselines[$Marker]
    } else {
        Get-MarkerCount $Marker
    }
    $script:MarkerBaselines.Remove($Marker)
    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $TimeoutSeconds))
    while ((Get-Date) -lt $deadline) {
        if ((Get-MarkerCount $Marker) -gt $baseline) { return }
        if ($script:Process.HasExited) {
            $content = Get-LiveText
            throw "Fresh marker was not observed before Server exit: $Marker`n$(Get-RecentDiagnostic $content)"
        }
        Start-Sleep -Milliseconds 500
    }
    $content = Get-LiveText
    throw "Timed out waiting for fresh marker: $Marker baseline=$baseline actualCount=$(Get-MarkerCount $Marker) windowId=$script:WindowId lastInput=$script:LastInput`n$(Get-RecentDiagnostic $content)"
}

function Get-LastShutdownStage([string]$Content) {
    $stages = @(
        @{ Name = 'complete'; Pattern = 'debug_shutdown_complete=PASS|shutdownStage=complete' },
        @{ Name = 'window_release'; Pattern = 'debug_window_release=(PASS|REQUESTED)|shutdownStage=window_release' },
        @{ Name = 'session_teardown'; Pattern = 'debug_session_teardown=PASS|shutdownStage=session_teardown' },
        @{ Name = 'target_teardown'; Pattern = 'debug_target_teardown=PASS|shutdownStage=target_teardown' },
        @{ Name = 'stop_requested'; Pattern = 'debug_stop=requested|shutdownStage=stop_requested' },
        @{ Name = 'request'; Pattern = 'debug_shutdown_request=|shutdownStage=request' }
    )
    foreach ($stage in $stages) { if ($Content -match $stage.Pattern) { return $stage.Name } }
    return 'none'
}

function Wait-Shutdown([int]$TimeoutSeconds) {
    $script:ExpectedMarker = 'shutdownStage=complete'
    $required = @(
        'debug_shutdown_request=targeted_close',
        'debug_stop=requested',
        'debug_target_teardown=PASS',
        'debug_session_teardown=PASS',
        'debug_window_release=PASS',
        'debug_shutdown_complete=PASS'
    )
    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $TimeoutSeconds))
    $nextStateQuery = Get-Date
    while ((Get-Date) -lt $deadline) {
        if (-not $script:Process.HasExited -and (Get-Date) -ge $nextStateQuery) {
            $script:LastInput = 'nativeapp.processes'
            $script:Process.StandardInput.WriteLine('nativeapp.processes')
            $script:Process.StandardInput.Flush()
            $nextStateQuery = (Get-Date).AddSeconds(2)
        }
        $complete = $true
        foreach ($marker in $required) {
            if (-not (Test-Marker $marker)) { $complete = $false; break }
        }
        if ($complete -and (Test-Marker 'shutdownStage=complete') -and (Test-Marker 'state=Exited')) { return }
        Start-Sleep -Milliseconds 200
    }
    $content = Get-LiveText
    throw "WAITSHUTDOWN timeout expected=complete actual=$(Get-LastShutdownStage $content) windowId=$script:WindowId lastInput=$script:LastInput`n$(Get-RecentDiagnostic $content)"
}

function Write-Phase20Trace([string]$Reason, [string]$Content) {
    try {
        $directory = if ($TraceDirectory) { [IO.Path]::GetFullPath($TraceDirectory) } else { Join-Path $RepoRoot 'logs' }
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $suffix = if ($TraceRunIndex -gt 0) { "-$TraceRunIndex" } else { '' }
        $artifactName = if ($TraceArtifactName) { $TraceArtifactName } else { "developer-studio-debugger-shutdown-trace$suffix.log" }
        $artifact = Join-Path $directory $artifactName
        $lines = @($Content -split "`r?`n")
        $lifecycle = @($lines | Where-Object { $_ -match 'debug_session|debug_state|debug_stop|debug_step|debug_binding|debug_transition|debug_condition|debug_shutdown|debug_target|debug_window|shutdownStage=|Native app processes:' } | Select-Object -Last 96)
        $recent = @($lines | Select-Object -Last 80)
        $header = @(
            'guideXOS Developer Studio Phase 20 hosted trace',
            "reason=$Reason",
            "case=$Case",
            "windowId=$script:WindowId",
            "expectedMarker=$script:ExpectedMarker",
            "lastInput=$script:LastInput",
            "lastShutdownStage=$(Get-LastShutdownStage $Content)",
            "serverExitCode=$(if ($script:Process -and $script:Process.HasExited) { $script:Process.ExitCode } else { 'unknown' })",
            '--- bounded lifecycle/UI markers ---'
        )
        $body = ($header + $lifecycle + @('--- bounded recent output ---') + $recent) -join "`r`n"
        if ($body.Length -gt 65536) { $body = $body.Substring($body.Length - 65536) }
        Set-Content -LiteralPath $artifact -Value $body -Encoding UTF8
        Write-Host "Phase 20 trace artifact: $artifact"
    } catch {
        Write-Host "WARNING: unable to write Phase 20 trace: $($_.Exception.Message)"
    }
}

function Add-InitialSetup([bool]$ReadinessOnly = $false, [bool]$EditorOwned = $false) {
    Add-Command 'gui.start'
    Add-Wait 8
    Add-Command 'desktop.launch com.guidexos.developerstudio'
    Add-WaitMarker 'Desktop launch successful: com.guidexos.developerstudio'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS'
    Add-Command "gui.activate $script:WindowId"
    Add-Click 300 180
    Add-Wait 1
    Add-Key 79 3
    Add-Text $FixtureRoot.ToLowerInvariant()
    Add-Key 13
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=PASS'
    Add-WaitMarker 'text="Outline indexing disabled in this build"'
    Add-Command "gui.activate $script:WindowId"
    # Stay in the editor text area; x=300 overlaps the breakpoint gutter.
    # The first editor row makes line navigation deterministic.
    Add-Click 340 86
    Add-Wait 1
    Add-Key 83 3
    Add-Wait 1
    for ($index = 1; $index -lt $BreakpointLine; ++$index) {
        Add-Key 40
        Add-WaitMilliseconds 150
    }
    $caretMarker = 'text="Document: main.cpp  Line ' + $BreakpointLine + ', Column 1"'
    Add-WaitMarker $caretMarker
    Add-Command "gui.activate $script:WindowId"
    Add-Wait 1
    Add-Key 120
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint_toggle=PASS'
    $breakpointMarker = 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT stage=WORKSPACE_PERSISTED project_id=' + $script:FixtureProjectId + ' pgen=1 op=0 symgen=0 id=none src=src/main.cpp line=' + $BreakpointLine + ' col=1 enabled=1 state=NotStarted'
    Add-WaitMarker $breakpointMarker
    # Before debug start F9 persists breakpoint intent in the workspace. The
    # runtime controller has no breakpoint ID or mapping state until the
    # debug build publishes symbols and beginDebugSession materializes it.
    # Start a real session before opening the runtime Breakpoints manager; the
    # editor condition UI operates on materialized controller rows, not the
    # persisted pre-start workspace intent.
    Add-Key 116 2
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    if ($ReadinessOnly) { return }
    if ($EditorOwned) { return }
    Add-OpenDebugBreakpoints
    $script:Parts.Add('NORMALIZE|breakpoints')
    # Keep the real panel open and select the intended row for all following
    # condition-editor actions; no menu reopen is needed after this point.
    Add-Key 40
    Add-WaitMilliseconds 300
}

function Add-SetCondition([string]$Value) {
    Add-OpenConditionEditor
    Add-ConditionText $Value
    Add-CommitCondition $Value
}

function Add-ConditionEditorCase() {
    Add-InitialSetup $false $true
    Add-SetCondition 'counter == 2'
    Add-OpenConditionEditor 'counter == 2'
    Add-ConditionText 'counter >= 2'
    Add-CommitCondition 'counter >= 2'
    Add-OpenConditionEditor 'counter >= 2'
    Add-CancelCondition 'counter >= 3' 'counter >= 2'
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'
    Add-OpenConditionEditor 'counter == 2'
    Add-ConditionText 'counter = 2'
    Add-InvalidCondition 'counter = 2'
    # Invalid syntax closes the modal and leaves the canonical condition
    # unchanged, so reopening must show the last valid expression.
    Add-OpenConditionEditor 'counter == 2'
    Add-ClearCondition
    Add-OpenConditionEditor
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'
    Add-Key 116
    Add-LogSnapshot
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-OpenConditionEditor 'counter == 2'
    # Leave the real editor modal active. The targeted close must bypass modal
    # focus and drive the authoritative Phase 19 shutdown sequence.
    Add-TargetedClose
}

function Add-ConditionErrorRecoveryCase() {
    Add-InitialSetup $false $true
    Add-SetCondition 'unknown_value == 2'
    Add-Key 116
    Add-LogSnapshot
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-WaitMarker 'Debug: breakpoint condition error'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-OpenConditionEditor 'unknown_value == 2'
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-Key 116
    Add-LogSnapshot
    # The replacement condition owns the current stopped trap. Depending on
    # the frame where the evaluation error was surfaced, the repaired
    # expression may match that trap immediately; wait for the resulting true
    # stop rather than assuming additional loop iterations.
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-OpenConditionEditor 'counter == 2'
    Add-ClearCondition
    Add-TargetedClose
}

function Add-ControllerPanelConditionEditorCase() {
    Add-InitialSetup $false $true
    # Start through the editor-owned route while the hosted Manager provider
    # is intentionally unavailable for NativeAppDebugger sessions.
    Add-SetCondition 'counter == 2'

    # The integrated debugger shows the real controller-owned session row as
    # a fallback while preserving the separate unavailable Manager status.
    Add-OpenDebugBreakpoints
    Add-WaitMarkerFresh 'condition=counter == 2'
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id='
    Add-CaptureMarker 'origin=controller editor_row='
    Add-CaptureMarker 'manager_snapshot=absent'
    Add-CaptureMarker ' text=counter == 2'
    Add-Key 67
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id='
    Add-WaitMarkerFresh 'origin=controller editor_row='
    Add-WaitMarkerFresh 'manager_snapshot=absent'
    Add-WaitMarkerFresh ' text=counter == 2'
    Add-ConditionText 'counter >= 2'
    Add-CommitCondition 'counter >= 2'
    Add-WaitMarkerFresh 'condition=counter >= 2'

    # Reopen from the editor after a controller-panel edit. Both routes must
    # resolve the same logical ID and read the canonical workspace condition.
    Add-Key 27
    Add-OpenConditionEditor 'counter >= 2' 'absent'
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'

    # Reopen the controller panel and require its rendered row to show the
    # editor-origin condition from the same workspace/controller record.
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-WaitMarkerFresh 'condition=counter == 2'
    Add-TargetedClose
}

function Add-Phase29NReadinessCase() {
    Add-InitialSetup $true
    Add-TargetedClose
}

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) 'experimental hosted Server exists'
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot 'guidexos.project') -PathType Leaf) 'Phase 20 fixture project exists'
Assert-True ($BreakpointLine -gt 0) 'breakpoint line is positive'

if ($Case -eq 'ConditionEditor') { Add-ConditionEditorCase }
elseif ($Case -eq 'ConditionErrorRecovery') { Add-ConditionErrorRecoveryCase }
elseif ($Case -eq 'ControllerPanelConditionEditor') { Add-ControllerPanelConditionEditorCase }
else { Add-Phase29NReadinessCase }

$startInfo = New-Object Diagnostics.ProcessStartInfo
$script:StdoutPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-phase20-$([Guid]::NewGuid().ToString('N')).out")
$script:StderrPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-phase20-$([Guid]::NewGuid().ToString('N')).err")
$startInfo.FileName = $env:ComSpec
$startInfo.Arguments = "/d /s /c `"`"$Executable`" 1>`"$script:StdoutPath`" 2>`"$script:StderrPath`"`""
$startInfo.WorkingDirectory = $ServerRoot
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $false
$startInfo.RedirectStandardError = $false
$script:Process = New-Object Diagnostics.Process
$script:Process.StartInfo = $startInfo

try {
    Assert-True $script:Process.Start() 'Phase 20 hosted Server starts'
    function Send-Direct([string]$Command) {
        $script:LastInput = $Command
        $script:Process.StandardInput.WriteLine($Command)
        $script:Process.StandardInput.Flush()
    }
    foreach ($part in $script:Parts) {
        if ($script:Process.HasExited) { throw "Server exited before command stream completed: $script:LastInput" }
        $separator = $part.IndexOf('|')
        $kind = $part.Substring(0, $separator)
        $value = $part.Substring($separator + 1)
        if ($kind -eq 'WAIT') { Start-Sleep -Seconds ([Math]::Max(1, [int]$value)) }
        elseif ($kind -eq 'WAITMS') { Start-Sleep -Milliseconds ([Math]::Max(50, [int]$value)) }
        elseif ($kind -eq 'WAITMARK') { Wait-Marker $value }
        elseif ($kind -eq 'WAITMARKFRESH') { Wait-MarkerFresh $value }
        elseif ($kind -eq 'CAPTURE') { $script:MarkerBaselines[$value] = Get-MarkerCount $value }
        elseif ($kind -eq 'WAITSHUTDOWN') { Wait-Shutdown ([int]$value) }
        elseif ($kind -eq 'COMMAND') {
            Send-Direct $value
        }
        elseif ($kind -eq 'NORMALIZE') {
            $count = Get-PanelBreakpointCount
            if ($count -gt 1) {
                # Add-OpenDebugBreakpoints leaves the second row selected.
                # Move to the leading stale row and disable it; the intended
                # line-37 row remains enabled and is selected again by the
                # next semantic panel-open helper.
                Send-Direct "gui.keyto $script:WindowId 38 down 0"
                Start-Sleep -Milliseconds 250
                Send-Direct "gui.keyto $script:WindowId 32 down 0"
                Start-Sleep -Milliseconds 400
            }
        }
    }
    $script:Process.StandardInput.Close()
    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $MaxRuntimeSeconds))
    while (-not $script:Process.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Assert-True $script:Process.HasExited "Phase 20 hosted case exits within $MaxRuntimeSeconds seconds"
    $script:CapturedText = Get-LiveText
    Assert-True ($script:Process.ExitCode -eq 0) "Phase 20 hosted Server exits with code 0 (actual=$($script:Process.ExitCode))"
    Assert-True ((Test-Marker 'debug_shutdown_complete=PASS') -or (Test-Marker 'shutdownStage=complete')) 'targeted close reaches shutdown complete'
    Assert-True (Test-PersistedBreakpoint 'src/main.cpp' $BreakpointLine 1) 'F9 persists the requested project-relative source breakpoint before debug start'
    if ($Case -eq 'ConditionEditor') {
        Assert-True (Test-Marker 'debug_condition_commit=INVALID') 'invalid syntax is reported by the real editor'
        Assert-True ((Test-Marker 'text=counter = 2 parse=INVALID state=UNCHANGED') -and
                     (Test-Marker 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id=')) 'invalid syntax does not become unconditional'
        Assert-True (Test-PersistedCondition 'src/main.cpp' $BreakpointLine 1 'counter == 2') 'editor condition persists on the single canonical workspace row'
        Assert-True ((Get-PersistedBreakpointRecordCount 'src/main.cpp' $BreakpointLine 1) -eq 1) 'editor condition edits do not create a duplicate breakpoint record'
        Assert-True (Test-Marker 'shutdown=TARGETED_CLOSE') 'modal-active targeted close is observed'
        Write-Host 'condition_editor_valid=PASS'
        Write-Host 'condition_editor_invalid_edit_clear_cancel=PASS'
        Write-Host 'condition_editor_modal_close=PASS'
    } elseif ($Case -eq 'ConditionErrorRecovery') {
        Assert-True ((Test-Marker 'ConditionError') -or (Test-Marker 'Debug: breakpoint condition error')) 'hosted ConditionError is surfaced'
        Assert-True (Test-Marker 'debug_condition_commit=PASS') 'ConditionError recovery commits a valid replacement through the editor'
        Assert-True (Test-Marker 'debug_condition_true=PASS') 'the replacement condition stops on a true evaluation'
        Assert-True (Test-Marker 'debug_condition_clear=PASS') 'ConditionError recovery clears the condition afterward'
        Write-Host 'condition_error_recovery=PASS'
    } elseif ($Case -eq 'ControllerPanelConditionEditor') {
        Assert-True (Test-Marker 'debug_condition_editor=OPEN breakpoint_id=') 'condition editor opened from both breakpoint views'
        Assert-True (Test-Marker 'origin=controller editor_row=') 'controller-panel selection resolves to a canonical condition-editor origin'
        Assert-True (Test-Marker 'origin=editor editor_row=') 'editor selection remains accepted after a controller-panel edit'
        Assert-True (Test-Marker 'manager_snapshot=absent') 'hosted Manager status remains unavailable without blocking controller/editor routes'
        Assert-True (Test-Marker 'text=counter >= 2 parse=VALID state=CONDITIONAL') 'controller-panel condition commit is valid'
        Assert-True (Test-Marker 'debug_condition_persist=PASS source=src/main.cpp:37') 'both routes persist through the canonical workspace record'
        Assert-True (Test-Marker 'condition=counter == 2') 'controller panel renders the editor-origin condition after reopen'
        Assert-True ((Get-PersistedBreakpointRecordCount 'src/main.cpp' $BreakpointLine 1) -eq 1) 'controller and editor views retain one logical workspace breakpoint'
        Assert-True (Test-PersistedCondition 'src/main.cpp' $BreakpointLine 1 'counter == 2') 'cross-view condition edits converge on one canonical value'
        # Hosted redraw logs can be large enough to roll early marker lines
        # out of the bounded diagnostic tail, so compare IDs from the retained
        # marker stream in the complete stdout artifact.
        $openIdLines = Select-String -LiteralPath $script:StdoutPath -Pattern 'debug_condition_editor=OPEN breakpoint_id=(\d+) source=src/main\.cpp:37'
        $openIds = @($openIdLines | ForEach-Object {
            [regex]::Match($_.Line, 'debug_condition_editor=OPEN breakpoint_id=(\d+)').Groups[1].Value
        } | Sort-Object -Unique)
        Assert-True ($openIds.Count -eq 1 -and $openIds[0] -ne '0') 'editor and controller-panel routes resolve the same logical breakpoint ID'
        Write-Host 'controller_editor_condition_convergence=PASS'
    } else {
        Assert-True (Test-Marker 'DEVELOPER_STUDIO_PHASE29N_HOST_SYMBOL stage=DWARF_READY') 'hosted DWARF load reaches Ready'
        Assert-True (Test-Marker 'DEVELOPER_STUDIO_PHASE29N_HOST_SOURCE_ASSOCIATION') 'hosted DWARF sources associate with the project root'
        Assert-True (Test-Marker 'DEVELOPER_STUDIO_PHASE29N_HOST_SOURCE_PATH kind=dwarf_compilation_entry') 'breakpoint source retains its raw DWARF compilation entry'
        Assert-True (Test-Marker 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT stage=BOUND') 'source breakpoint reaches Verified/BOUND'
        Assert-True (Test-Marker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT') 'source breakpoint is hit'
        Assert-True (Test-Marker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING') 'Continue resumes the hosted target'
        Write-Host 'phase29n_hosted_symbol_breakpoint_readiness=PASS'
    }
} catch {
    $script:CapturedText = Get-LiveText
    Write-Phase20Trace $_.Exception.Message $script:CapturedText
    throw
} finally {
    if ($script:Process -and -not $script:Process.HasExited) {
        & taskkill.exe /PID $script:Process.Id /T /F | Out-Null
        $script:Process.WaitForExit()
    }
    if (-not $KeepArtifacts) {
        Remove-Item -LiteralPath $script:StdoutPath,$script:StderrPath -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "Phase 20 artifacts retained: $script:StdoutPath / $script:StderrPath"
    }
    if ($script:Process) { $script:Process.Dispose() }
}
