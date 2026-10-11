[CmdletBinding()]
param(
    [ValidateSet('ConditionEditor', 'ConditionErrorRecovery', 'ControllerPanelConditionEditor', 'ManagerConditionEditor', 'ManagerOpenCloseMarker', 'ManagerRemapRetention', 'Phase29NReadiness')]
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
$script:MarkerGateRunToken = ''
$script:CurrentMarkerGateCycle = 'setup'
$script:FreshMarkerObservations = New-Object 'System.Collections.Generic.List[object]'

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

function Add-MutateFixtureSource() {
    $script:Parts.Add('MUTATESOURCE|')
}

function Add-AssertFixtureCondition([string]$Condition) {
    $script:Parts.Add("ASSERTCONDITION|$Condition")
}

function Invoke-FixtureSourceRemapVariation() {
    $sourcePath = Join-Path $FixtureRoot 'src\main.cpp'
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Remap fixture source is missing: $sourcePath"
    }
    $source = [IO.File]::ReadAllText($sourcePath)
    $needle = '    int total = 0;'
    if (([regex]::Matches($source, [regex]::Escape($needle))).Count -ne 1) {
        throw 'Remap fixture no longer has exactly one expected pre-loop source statement'
    }
    $beforeHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    $updated = $source.Replace($needle, '    int total = 0; if (callerValue < 0) total += 0;')
    if ($updated -eq $source) { throw 'Remap fixture source variation made no change' }
    [IO.File]::WriteAllText($sourcePath, $updated, [Text.UTF8Encoding]::new($false))
    $afterHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    $lines = [IO.File]::ReadAllLines($sourcePath)
    if ($lines.Length -lt 37 -or $lines[36] -notmatch 'volatile int sample = rect\.origin\.x') {
        throw 'Remap fixture changed the logical breakpoint source identity at src/main.cpp:37'
    }
    if ($beforeHash -eq $afterHash) { throw 'Remap fixture source hash did not change' }
    Write-Host "phase30a_remap_source_mutation=PASS source_hash_before=$beforeHash source_hash_after=$afterHash breakpoint=src/main.cpp:37"
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
    $hasCapturedBaseline = $script:MarkerBaselines.ContainsKey($Marker)
    $baseline = if ($hasCapturedBaseline) {
        $script:MarkerBaselines[$Marker]
    } else {
        Get-MarkerCount $Marker
    }
    $script:MarkerBaselines.Remove($Marker)
    $baselineSource = if ($hasCapturedBaseline) { 'captured-before-action' } else { 'sampled-at-wait' }
    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $TimeoutSeconds))
    while ((Get-Date) -lt $deadline) {
        $actualCount = Get-MarkerCount $Marker
        if ($actualCount -gt $baseline) {
            if ($Case -eq 'ManagerOpenCloseMarker') {
                $script:FreshMarkerObservations.Add([pscustomobject]@{
                    Cycle = $script:CurrentMarkerGateCycle
                    Matcher = $Marker
                    Baseline = $baseline
                    Actual = $actualCount
                    BaselineSource = $baselineSource
                })
            }
            if ($Marker -match 'debug_condition_editor') {
                Write-Host "fresh_marker=PASS baseline_source=$baselineSource baseline=$baseline actual=$actualCount matcher=$Marker"
            }
            return
        }
        if ($script:Process.HasExited) {
            $content = Get-LiveText
            throw "Fresh marker was not observed before Server exit: $Marker`n$(Get-RecentDiagnostic $content)"
        }
        Start-Sleep -Milliseconds 500
    }
    $content = Get-LiveText
    throw "Timed out waiting for fresh marker: $Marker baseline=$baseline baselineSource=$baselineSource actualCount=$(Get-MarkerCount $Marker) windowId=$script:WindowId lastInput=$script:LastInput`n$(Get-RecentDiagnostic $content)"
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
        if ($TraceDirectory) {
            $directory = [IO.Path]::GetFullPath($TraceDirectory)
            if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
                throw "Phase 20 test-infrastructure failure: supplied trace directory is missing or moved: $directory"
            }
        } else {
            $directory = Join-Path $RepoRoot 'logs'
            New-Item -ItemType Directory -Path $directory -Force | Out-Null
        }
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
        if ($TraceDirectory) {
            throw "Phase 20 test-infrastructure failure: unable to write the trace under the supplied directory '$TraceDirectory': $($_.Exception.Message)"
        }
        Write-Host "WARNING: unable to write Phase 20 trace: $($_.Exception.Message)"
    }
}

function Add-InitialSetup([bool]$ReadinessOnly = $false, [bool]$EditorOwned = $false,
                         [bool]$StartTarget = $true) {
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
    if (-not $StartTarget) { return }
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

function Add-ManagerConditionEditorCase() {
    # Start with the persistent workspace row but no active target. The hosted
    # Manager provider must project it before any runtime binding exists.
    Add-InitialSetup $true $false $false
    Add-OpenDebugBreakpoints
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_manager_provider=PASS source=workspace rows=1'

    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'logical_count=1 runtime_id=0 runtime_session=0 binding_id=0'
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager'
    Add-CaptureMarker 'origin=manager editor_row='
    Add-CaptureMarker 'manager_snapshot=workspace'
    Add-Key 67
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'logical_count=1 runtime_id=0 runtime_session=0 binding_id=0'
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager'
    Add-WaitMarkerFresh 'manager_snapshot=workspace'
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'

    # Reopen through the source editor and write a different condition. Then
    # return to the Manager and require it to read that canonical value.
    Add-Key 27
    Add-OpenConditionEditor 'counter == 2' 'workspace'
    Add-ConditionText 'counter >= 2'
    Add-CommitCondition 'counter >= 2'
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'condition=counter >= 2'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'condition=counter >= 2'

    # Delete and recreate through the persistent Manager. Product policy is a
    # recreated breakpoint starts with empty condition configuration.
    Add-CaptureMarker 'debug_ui_breakpoint_remove=PASS'
    Add-CaptureMarker 'debug_manager_provider=PASS source=workspace rows=0'
    Add-Key 46
    Add-WaitMarkerFresh 'debug_ui_breakpoint_remove=PASS'
    Add-WaitMarkerFresh 'debug_manager_provider=PASS source=workspace rows=0'
    Add-CaptureMarker 'debug_manager_provider=PASS source=workspace rows=1'
    Add-Key 120
    Add-WaitMarkerFresh 'debug_manager_provider=PASS source=workspace rows=1'
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'condition=<none>'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'condition=<none>'
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager'
    Add-Key 67
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager'
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'

    # Launch from the supported hosted debug command. The rebuilt target must
    # bind the condition from workspace state, then filter false hits and stop
    # on the intended true value before shutdown tears the target down.
    Add-Key 27
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_binding=id='
    Add-CaptureMarker 'Debug: breakpoint condition false; continuing'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-Key 116 2
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_binding=id='
    Add-WaitMarkerFresh 'Debug: breakpoint condition false; continuing'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'runtime_id='
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id='
    Add-CaptureMarker 'origin=manager editor_row='
    Add-CaptureMarker 'condition=counter == 2'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'runtime_id='
    Add-CaptureMarker ' text=counter == 2'
    Add-Key 67
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id='
    Add-WaitMarkerFresh 'origin=manager editor_row='
    Add-WaitMarkerFresh ' text=counter == 2'
    Add-TargetedClose
}

function Add-ManagerOpenCloseMarkerCase() {
    $script:MarkerGateRunToken = [Guid]::NewGuid().ToString('N')
    $managerOpenMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager editor_row=0 editor_row_id=0 manager_id=0 manager_row_id=com.example.debuggerphase15/src/main.cpp:37 manager_workspace_generation='
    $managerEditorOpenMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=OPEN breakpoint_id=0 text=counter == 2 identity=workspace_configured'
    $managerEditorClosedMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=CLOSED reason=CANCEL breakpoint_id=0 text=counter == 2'
    $managerRowMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_manager_row_select=PASS row_id=com.example.debuggerphase15/src/main.cpp:37 source=src/main.cpp:37 workspace_generation='
    $managerRowConditionMarker = 'condition=counter == 2 logical_count=1 runtime_id=0 runtime_session=0 binding_id=0'

    Add-InitialSetup $true $false $false
    Add-OpenDebugBreakpoints
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_manager_provider=PASS source=workspace rows=1'

    # Establish one canonical condition before the measured open/close cycles.
    Add-CaptureMarker $managerRowMarker
    Add-Key 40
    Add-WaitMarkerFresh $managerRowMarker
    Add-CaptureMarker $managerOpenMarker
    Add-Key 67
    Add-WaitMarkerFresh $managerOpenMarker
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'
    Add-CaptureMarker $managerRowMarker
    Add-CaptureMarker $managerRowConditionMarker
    Add-Key 40
    Add-WaitMarkerFresh $managerRowMarker
    Add-WaitMarkerFresh $managerRowConditionMarker

    # Each baseline uses the exact matcher used by its waiter, before the
    # input that can emit that event is sent.
    for ($cycle = 1; $cycle -le 10; ++$cycle) {
        $script:Parts.Add("MARKERCYCLEBEGIN|$cycle")
        Add-CaptureMarker $managerOpenMarker
        Add-CaptureMarker $managerEditorOpenMarker
        Add-Key 67
        Add-WaitMarkerFresh $managerOpenMarker
        Add-WaitMarkerFresh $managerEditorOpenMarker

        Add-CaptureMarker $managerEditorClosedMarker
        Add-Key 27
        Add-WaitMarkerFresh $managerEditorClosedMarker

        # A fresh Manager selection after Escape proves input returned to the
        # Manager and the next cycle cannot be satisfied by a stale modal.
        Add-CaptureMarker $managerRowMarker
        Add-CaptureMarker $managerRowConditionMarker
        Add-Key 40
        Add-WaitMarkerFresh $managerRowMarker
        Add-WaitMarkerFresh $managerRowConditionMarker
        $script:Parts.Add("MARKERCYCLE|$cycle")
    }
    Add-CaptureMarker 'clean_close=PASS'
    Add-Command "gui.close $script:WindowId"
    Add-WaitMarkerFresh 'clean_close=PASS'
}

function Add-ManagerRemapRetentionCase() {
    # Create one canonical workspace breakpoint, configure it through the real
    # Manager, and verify the editor sees that value before the first build.
    Add-InitialSetup $true $false $false
    Add-OpenDebugBreakpoints
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_manager_provider=PASS source=workspace rows=1'
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'logical_count=1 runtime_id=0 runtime_session=0 binding_id=0'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'logical_count=1 runtime_id=0 runtime_session=0 binding_id=0'
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager'
    Add-CaptureMarker 'origin=manager editor_row='
    Add-Key 67
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager'
    Add-WaitMarkerFresh 'origin=manager editor_row='
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'
    Add-Key 27
    Add-OpenConditionEditor 'counter == 2' 'workspace'
    Add-Key 27
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'condition=counter == 2'
    Add-CaptureMarker 'runtime_id='
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'condition=counter == 2'

    # First supported hosted debug build. Captures are established before the
    # launch so a fast first stop cannot outrun the marker waiter.
    Add-Key 27
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_ARTIFACT'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_SYMBOL stage=DWARF_READY'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT stage=BOUND'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT_ADDRESS'
    Add-CaptureMarker 'Debug: breakpoint condition false; continuing'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-Key 116 2
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_ARTIFACT'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_SYMBOL stage=DWARF_READY'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT stage=BOUND'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT_ADDRESS'
    Add-WaitMarkerFresh 'Debug: breakpoint condition false; continuing'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'

    # Select the same source-key row after binding, then stop the target cleanly
    # before changing source so the next build has a distinct artifact.
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'runtime_id='
    Add-CaptureMarker 'logical_count=1'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'runtime_id='
    Add-WaitMarkerFresh 'logical_count=1'
    Add-CaptureMarker 'debug_stop=requested'
    Add-CaptureMarker 'debugger_teardown=PASS'
    Add-CaptureMarker 'debug_state=EXITED'
    Add-Key 116 1
    Add-WaitMarkerFresh 'debug_stop=requested'
    Add-WaitMarkerFresh 'debugger_teardown=PASS'
    Add-WaitMarkerFresh 'debug_state=EXITED'
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'runtime_id=0 runtime_session=0 binding_id=0'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'runtime_id=0 runtime_session=0 binding_id=0'

    # Change only a preceding statement. The source file, line 37 statement,
    # project ID, and workspace breakpoint key remain identical.
    Add-MutateFixtureSource
    Add-AssertFixtureCondition 'counter == 2'

    # Rebuild and launch from the same hosted Developer Studio process. This
    # gives the new target a later session/build generation while retaining the
    # canonical workspace condition and Manager provider row.
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_ARTIFACT'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_SYMBOL stage=DWARF_READY'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT stage=BOUND'
    Add-CaptureMarker 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT_ADDRESS'
    Add-CaptureMarker 'Debug: breakpoint condition false; continuing'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-Key 116 2
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_ARTIFACT'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_SYMBOL stage=DWARF_READY'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT stage=BOUND'
    Add-WaitMarkerFresh 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT_ADDRESS'
    Add-WaitMarkerFresh 'Debug: breakpoint condition false; continuing'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'

    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'condition=counter == 2'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'condition=counter == 2'
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id='
    Add-CaptureMarker 'origin=manager editor_row='
    Add-CaptureMarker ' text=counter == 2'
    Add-Key 67
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id='
    Add-WaitMarkerFresh 'origin=manager editor_row='
    Add-WaitMarkerFresh ' text=counter == 2'

    # Exercise the real post-remap stepping and inspection paths at the current
    # conditional stop before explicitly stopping the target.
    Add-Key 27
    Add-CaptureMarker 'debug_step_complete=StepInto'
    Add-CaptureMarker 'debug_inspection=refreshed'
    Add-Key 122
    Add-WaitMarkerFresh 'debug_step_complete=StepInto'
    Add-WaitMarkerFresh 'debug_inspection=refreshed'
    Add-CaptureMarker 'debug_step_complete=StepOver'
    Add-CaptureMarker 'debug_inspection=refreshed'
    Add-Key 121
    Add-WaitMarkerFresh 'debug_step_complete=StepOver'
    Add-WaitMarkerFresh 'debug_inspection=refreshed'
    Add-CaptureMarker 'debug_step_complete=StepOut'
    Add-CaptureMarker 'debug_inspection=refreshed'
    Add-Key 122 1
    Add-WaitMarkerFresh 'debug_step_complete=StepOut'
    Add-WaitMarkerFresh 'debug_inspection=refreshed'

    # The Editor is opened from the Manager one more time after the remap. Its
    # runtime-bound identity must still write through to the same workspace row.
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'condition=counter == 2'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'condition=counter == 2'
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id='
    Add-CaptureMarker 'origin=manager editor_row='
    Add-CaptureMarker ' text=counter == 2'
    Add-Key 67
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id='
    Add-WaitMarkerFresh 'origin=manager editor_row='
    Add-WaitMarkerFresh ' text=counter == 2'
    Add-Key 27

    Add-CaptureMarker 'debug_stop=requested'
    Add-CaptureMarker 'debugger_teardown=PASS'
    Add-CaptureMarker 'debug_state=EXITED'
    Add-Key 116 1
    Add-WaitMarkerFresh 'debug_stop=requested'
    Add-WaitMarkerFresh 'debugger_teardown=PASS'
    Add-WaitMarkerFresh 'debug_state=EXITED'

    # Removal clears every projection; F9 from the empty Manager recreates one
    # canonical row with the product's documented empty-condition semantics.
    Add-CaptureMarker 'debug_ui_breakpoint_remove=PASS'
    Add-CaptureMarker 'debug_manager_provider=PASS source=workspace rows=0'
    Add-Key 46
    Add-WaitMarkerFresh 'debug_ui_breakpoint_remove=PASS'
    Add-WaitMarkerFresh 'debug_manager_provider=PASS source=workspace rows=0'
    # Step Out leaves the caret at line 48. Close the Manager panel and move
    # back to line 37 before F9 recreates the empty-condition workspace row.
    Add-Key 27
    for ($i = 0; $i -lt 11; ++$i) { Add-Key 38 }
    Add-CaptureMarker 'debug_breakpoint_key=PASS'
    Add-Key 120
    Add-WaitMarkerFresh 'debug_breakpoint_key=PASS'
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-CaptureMarker 'debug_manager_row_select=PASS row_id='
    Add-CaptureMarker 'condition=<none>'
    Add-Key 40
    Add-WaitMarkerFresh 'debug_manager_row_select=PASS row_id='
    Add-WaitMarkerFresh 'condition=<none>'
    Add-CaptureMarker 'clean_close=PASS'
    Add-Command "gui.close $script:WindowId"
    Add-WaitMarkerFresh 'clean_close=PASS'
}

function Add-Phase29NReadinessCase() {
    Add-InitialSetup $true
    Add-TargetedClose
}

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) 'experimental hosted Server exists'
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot 'guidexos.project') -PathType Leaf) 'Phase 20 fixture project exists'
Assert-True ($BreakpointLine -gt 0) 'breakpoint line is positive'
if ($TraceDirectory) {
    $TraceDirectory = [IO.Path]::GetFullPath($TraceDirectory)
    if (-not (Test-Path -LiteralPath $TraceDirectory -PathType Container)) {
        throw "Phase 20 test-infrastructure failure: supplied trace directory is missing or moved before launch: $TraceDirectory"
    }
    Write-Host "phase20_trace_directory=$TraceDirectory"
}

if ($Case -eq 'ConditionEditor') { Add-ConditionEditorCase }
elseif ($Case -eq 'ConditionErrorRecovery') { Add-ConditionErrorRecoveryCase }
elseif ($Case -eq 'ControllerPanelConditionEditor') { Add-ControllerPanelConditionEditorCase }
elseif ($Case -eq 'ManagerConditionEditor') { Add-ManagerConditionEditorCase }
elseif ($Case -eq 'ManagerOpenCloseMarker') { Add-ManagerOpenCloseMarkerCase }
elseif ($Case -eq 'ManagerRemapRetention') { Add-ManagerRemapRetentionCase }
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
        elseif ($kind -eq 'MARKERCYCLEBEGIN') { $script:CurrentMarkerGateCycle = $value }
        elseif ($kind -eq 'MARKERCYCLE') { Write-Host "phase30a_marker_cycle=$value PASS run_token=$script:MarkerGateRunToken" }
        elseif ($kind -eq 'MUTATESOURCE') { Invoke-FixtureSourceRemapVariation }
        elseif ($kind -eq 'ASSERTCONDITION') {
            Assert-True (Test-PersistedCondition 'src/main.cpp' $BreakpointLine 1 $value) "canonical workspace condition is still '$value' before post-remap debug start"
            Write-Host "phase30a_remap_condition_prelaunch=PASS source=src/main.cpp:$BreakpointLine condition=$value"
        }
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
    if ($Case -eq 'ManagerRemapRetention') {
        Assert-True (Test-Marker 'clean_close=PASS') 'targeted app close completes after the debug targets are stopped'
        Assert-True ($script:CapturedText -match 'cleaned window count: 1; remaining owned window count: 0') 'Server cleanup releases the hosted app window'
    } elseif ($Case -eq 'ManagerOpenCloseMarker') {
        Assert-True (Test-Marker 'clean_close=PASS') 'the marker gate closes its hosted app after the repeated modal lifecycle'
    } else {
        Assert-True ((Test-Marker 'debug_shutdown_complete=PASS') -or (Test-Marker 'shutdownStage=complete')) 'targeted close reaches shutdown complete'
    }
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
    } elseif ($Case -eq 'ManagerConditionEditor') {
        Assert-True (Test-Marker 'debug_manager_provider=PASS source=workspace rows=1') 'hosted production provider publishes the persistent workspace row'
        Assert-True (Test-Marker 'debug_manager_row_select=PASS row_id=') 'production Manager selection resolves the source-key row'
        Assert-True (Test-Marker 'logical_count=1 runtime_id=0 runtime_session=0 binding_id=0') 'prelaunch Manager row has one canonical owner and no runtime binding'
        Assert-True (Test-Marker 'origin=manager editor_row=') 'Manager selection opens the real condition editor'
        Assert-True (Test-Marker 'manager_snapshot=workspace') 'condition editor reports the hosted workspace provider'
        Assert-True (Test-Marker 'text=counter >= 2 parse=VALID state=CONDITIONAL') 'editor-origin update persists after Manager-origin update'
        Assert-True (Test-Marker 'condition=counter >= 2') 'Manager reflects the editor-origin canonical condition'
        Assert-True (Test-Marker 'debug_ui_breakpoint_remove=PASS') 'Manager removal deletes the canonical workspace breakpoint'
        Assert-True (Test-PersistedCondition 'src/main.cpp' $BreakpointLine 1 'counter == 2') 'recreated breakpoint receives a fresh condition through Manager'
        Assert-True ((Get-PersistedBreakpointRecordCount 'src/main.cpp' $BreakpointLine 1) -eq 1) 'remove and recreate leave exactly one logical source breakpoint'
        Assert-True (Test-Marker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_binding=id=') 'Manager-configured breakpoint receives a live hosted binding'
        Assert-True (Test-Marker 'Debug: breakpoint condition false; continuing') 'false conditional hits continue without a visible stop'
        Assert-True (Test-Marker 'debug_condition_true=PASS') 'true conditional hit stops at the source breakpoint'
        Assert-True (Test-Marker 'debug_target_teardown=PASS') 'target teardown completes after the Manager route'
        Assert-True (Test-Marker 'debug_session_teardown=PASS') 'debugger session teardown completes after the Manager route'
        Assert-True ((Get-PersistedBreakpointRecordCount 'src/main.cpp' $BreakpointLine 1) -eq 1) 'persistent Manager row survives target teardown'
        Write-Host 'manager_workspace_provider=PASS'
        Write-Host 'manager_condition_round_trip=PASS'
        Write-Host 'manager_remove_recreate=PASS'
        Write-Host 'manager_hosted_lifecycle=PASS'
    } elseif ($Case -eq 'ManagerOpenCloseMarker') {
        $managerOpenMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=OPEN breakpoint_id=pending source=src/main.cpp:37 origin=manager editor_row=0 editor_row_id=0 manager_id=0 manager_row_id=com.example.debuggerphase15/src/main.cpp:37 manager_workspace_generation='
        $managerEditorOpenMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=OPEN breakpoint_id=0 text=counter == 2 identity=workspace_configured'
        $managerEditorClosedMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=CLOSED reason=CANCEL breakpoint_id=0 text=counter == 2'
        $managerCommitClosedMarker = 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_editor=CLOSED reason=COMMIT breakpoint_id=0 text=counter == 2'
        $managerRowConditionPattern = 'debug_manager_row_select=PASS row_id=com\.example\.debuggerphase15/src/main\.cpp:37 source=src/main\.cpp:37 workspace_generation=(\d+) condition=counter == 2 logical_count=1 runtime_id=0 runtime_session=0 binding_id=0'

        $openLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern $managerOpenMarker)
        $measuredOpenLines = @($openLines | Select-Object -Skip 1)
        $editorOpenLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern $managerEditorOpenMarker)
        $cancelLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern $managerEditorClosedMarker)
        $commitLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern $managerCommitClosedMarker)
        $managerRowLines = @(Select-String -LiteralPath $script:StdoutPath -Pattern $managerRowConditionPattern)
        $providerLines = @(Select-String -LiteralPath $script:StdoutPath -Pattern 'debug_manager_provider=PASS source=workspace rows=1 project_generation=(\d+) workspace_generation=(\d+)')

        Assert-True ($openLines.Count -eq 11) 'one setup open and ten distinct Manager condition-editor opens were appended'
        Assert-True ($editorOpenLines.Count -eq 10) 'each measured open rendered the configured canonical condition'
        Assert-True ($cancelLines.Count -eq 10) 'each measured modal closed through Escape'
        Assert-True ($commitLines.Count -eq 1) 'setup condition commit closed its editor once'
        Assert-True ($managerRowLines.Count -ge 11) 'Manager selection remained current after every close'
        Assert-True ($providerLines.Count -ge 1) 'Manager snapshot came from the workspace provider'
        Assert-True ((Get-PersistedBreakpointRecordCount 'src/main.cpp' $BreakpointLine 1) -eq 1) 'marker stress retains exactly one canonical breakpoint'
        Assert-True (Test-PersistedCondition 'src/main.cpp' $BreakpointLine 1 'counter == 2') 'marker stress leaves the canonical condition unchanged'

        $openGenerations = @($measuredOpenLines | ForEach-Object {
            [regex]::Match($_.Line, 'manager_workspace_generation=(\d+)').Groups[1].Value
        } | Sort-Object -Unique)
        $rowGenerations = @($managerRowLines | ForEach-Object {
            [regex]::Match($_.Line, 'workspace_generation=(\d+)').Groups[1].Value
        } | Sort-Object -Unique)
        $providerMatch = [regex]::Match($providerLines[-1].Line,
            'project_generation=(\d+) workspace_generation=(\d+)')
        Assert-True ($openGenerations.Count -eq 1 -and $openGenerations[0]) 'all ten measured opens resolved the same current workspace generation'
        Assert-True ($rowGenerations.Count -eq 1 -and $rowGenerations[0] -eq $openGenerations[0]) 'Manager row and condition editor share one workspace generation'
        Assert-True ($providerMatch.Success -and $providerMatch.Groups[2].Value -eq $openGenerations[0]) 'the current Manager provider snapshot matches the opened breakpoint generation'
        Assert-True (@($openLines | Where-Object { $_.Line -notmatch 'manager_snapshot=workspace' }).Count -eq 0) 'every opening reports the current hosted Manager snapshot'
        Assert-True (@($script:FreshMarkerObservations | Where-Object { $_.BaselineSource -ne 'captured-before-action' }).Count -eq 0) 'every marker gate wait used a baseline captured before its action'
        Assert-True (@($script:FreshMarkerObservations | Where-Object { $_.Cycle -ne 'setup' -and $_.Matcher -eq $managerOpenMarker }).Count -eq 10) 'all ten measured opens used a pre-action current-cycle marker baseline'

        Assert-True ($openLines[0].LineNumber -lt $commitLines[0].LineNumber -and
                     $commitLines[0].LineNumber -lt $openLines[1].LineNumber) 'setup open and commit precede measured cycles'
        for ($cycleIndex = 0; $cycleIndex -lt 10; ++$cycleIndex) {
            $openLine = $openLines[$cycleIndex + 1].LineNumber
            $editorOpenLine = $editorOpenLines[$cycleIndex].LineNumber
            $cancelLine = $cancelLines[$cycleIndex].LineNumber
            $previousCloseLine = if ($cycleIndex -eq 0) {
                $commitLines[0].LineNumber
            } else {
                $cancelLines[$cycleIndex - 1].LineNumber
            }
            $managerRowsAfterClose = @($managerRowLines | Where-Object {
                $_.LineNumber -gt $previousCloseLine -and $_.LineNumber -lt $openLine
            })
            Assert-True ($managerRowsAfterClose.Count -ge 1) "cycle $($cycleIndex + 1) has a fresh Manager selection after the prior close"
            $managerRow = $managerRowsAfterClose[0]
            $managerRowLine = $managerRow.LineNumber
            Assert-True ($previousCloseLine -lt $managerRowLine -and $managerRowLine -lt $openLine -and
                         $openLine -lt $editorOpenLine -and $editorOpenLine -lt $cancelLine) "cycle $($cycleIndex + 1) close/select/open/render/close order is coherent"
        }

        if ($TraceDirectory) {
            $traceRoot = [IO.Path]::GetFullPath($TraceDirectory)
            if (-not (Test-Path -LiteralPath $traceRoot -PathType Container)) {
                throw "Phase 30A test-infrastructure failure: marker evidence directory is missing or moved: $traceRoot"
            }
            $artifactName = if ($TraceArtifactName) { $TraceArtifactName } else { 'phase30a-marker-gate.log' }
            $artifactPath = Join-Path $traceRoot $artifactName
            $artifactLines = @(
                'guideXOS Phase 30A repeated Manager condition-editor marker gate',
                'result=10/10 PASS',
                "run_token=$script:MarkerGateRunToken",
                'producer=src/main.cpp drawModal / logBreakpointConditionUi',
                "open_matcher=$managerOpenMarker",
                "rendered_condition_matcher=$managerEditorOpenMarker",
                "closed_matcher=$managerEditorClosedMarker",
                'freshness=exact matcher count captured before input; post-action count had to increase',
                "measured_workspace_generation=$($openGenerations[0])",
                "manager_provider_generation=$($providerMatch.Groups[2].Value)",
                'canonical_breakpoint=com.example.debuggerphase15/src/main.cpp:37',
                'canonical_condition=counter == 2',
                'logical_breakpoint_count=1',
                '--- fresh marker observations ---'
            )
            foreach ($observation in $script:FreshMarkerObservations) {
                $artifactLines += "cycle=$($observation.Cycle) baseline_source=$($observation.BaselineSource) baseline=$($observation.Baseline) actual=$($observation.Actual) matcher=$($observation.Matcher)"
            }
            $artifactLines += '--- emitted Manager open and close records ---'
            foreach ($cycleIndex in 0..9) {
                $openLineNumber = $openLines[$cycleIndex + 1].LineNumber
                $previousCloseLine = if ($cycleIndex -eq 0) { $commitLines[0].LineNumber } else { $cancelLines[$cycleIndex - 1].LineNumber }
                $cycleManagerRow = @($managerRowLines | Where-Object {
                    $_.LineNumber -gt $previousCloseLine -and $_.LineNumber -lt $openLineNumber
                })[0]
                $artifactLines += "cycle=$($cycleIndex + 1) manager_row=$($cycleManagerRow.Line)"
                $artifactLines += "cycle=$($cycleIndex + 1) open=$($openLines[$cycleIndex + 1].Line)"
                $artifactLines += "cycle=$($cycleIndex + 1) rendered=$($editorOpenLines[$cycleIndex].Line)"
                $artifactLines += "cycle=$($cycleIndex + 1) closed=$($cancelLines[$cycleIndex].Line)"
            }
            Set-Content -LiteralPath $artifactPath -Value $artifactLines -Encoding UTF8
            Write-Host "phase30a_marker_gate_evidence=$artifactPath"
        }
        Write-Host 'phase30a_marker_gate=10/10 PASS'
    } elseif ($Case -eq 'ManagerRemapRetention') {
        Assert-True (Test-Marker 'debug_manager_provider=PASS source=workspace rows=1') 'persistent Manager row exists through the remap lifecycle'
        Assert-True (Test-Marker 'debug_manager_row_select=PASS row_id=com.example.debuggerphase15/src/main.cpp:37') 'Manager row keeps the same project/source/line identity'
        Assert-True (Test-Marker 'condition=counter == 2') 'Manager reflects the canonical condition after remap'
        Assert-True (Test-Marker 'origin=manager editor_row=') 'post-remap Manager selection opens the production condition editor'
        Assert-True ((Get-MarkerCount 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS') -ge 2) 'the retained condition reaches a true hit before and after remap'
        Assert-True ((Get-MarkerCount 'Debug: breakpoint condition false; continuing') -ge 2) 'false condition evaluations continue before and after remap'
        Assert-True (Test-Marker 'debug_step_complete=StepInto') 'post-remap Step Into completes'
        Assert-True (Test-Marker 'debug_step_complete=StepOver') 'post-remap Step Over completes'
        Assert-True (Test-Marker 'debug_step_complete=StepOut') 'post-remap Step Out completes'
        Assert-True (Test-Marker 'debug_inspection=refreshed') 'post-remap stack/locals inspection refreshes at stepped stops'
        Assert-True ((Get-MarkerCount 'debugger_teardown=PASS') -ge 2) 'pre-rebuild stop and post-remap target exit both complete debugger cleanup'
        Assert-True ((Get-MarkerCount 'debug_state=EXITED') -ge 2) 'pre-rebuild stop and post-remap target both reach the exited state'
        Assert-True (Test-Marker 'debug_ui_breakpoint_remove=PASS') 'post-remap removal clears the canonical breakpoint'
        Assert-True ((Get-MarkerCount 'debug_manager_provider=PASS source=workspace rows=0') -ge 1) 'Manager projection disappears on removal'
        Assert-True ((Get-MarkerCount 'debug_manager_provider=PASS source=workspace rows=1') -ge 2) 'Manager projection is recreated once after removal'
        Assert-True ((Get-PersistedBreakpointRecordCount 'src/main.cpp' $BreakpointLine 1) -eq 1) 'remove/recreate leaves one logical breakpoint'
        Assert-True (Test-PersistedCondition 'src/main.cpp' $BreakpointLine 1 '') 'recreated breakpoint does not inherit the removed condition'

        $artifactLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern 'DEVELOPER_STUDIO_PHASE29N_HOST_ARTIFACT')
        $artifactRecords = @()
        foreach ($line in $artifactLines) {
            $hash = [regex]::Match($line.Line, 'sha256=([0-9A-Fa-f]{64})').Groups[1].Value
            $operation = [regex]::Match($line.Line, 'build_operation=(\d+)').Groups[1].Value
            if ($hash -and $operation) {
                $artifactRecords += [PSCustomObject]@{ Operation = $operation; Hash = $hash }
            }
        }
        $uniqueArtifacts = @($artifactRecords | Sort-Object Operation,Hash -Unique)
        Assert-True ($uniqueArtifacts.Count -ge 2) 'both hosted builds report artifact hashes and build generations'
        Assert-True ($uniqueArtifacts[0].Hash -ne $uniqueArtifacts[-1].Hash) 'real source variation produces a different post-remap artifact hash'

        $addressLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT_ADDRESS')
        $addressOperations = @($addressLines | ForEach-Object {
            [regex]::Match($_.Line, ' op=(\d+) ').Groups[1].Value
        } | Where-Object { $_ } | Sort-Object { [int]$_ } -Unique)
        Assert-True ($addressOperations.Count -ge 2) 'both symbol generations report every mapped line address'
        $preOperation = [string]$addressOperations[0]
        $postOperation = [string]$addressOperations[-1]
        $preAddresses = @($addressLines | Where-Object { $_.Line -match (' op=' + [regex]::Escape($preOperation) + ' ') } | ForEach-Object {
            [regex]::Match($_.Line, 'address=(0x[0-9A-Fa-f]+)').Groups[1].Value
        } | Where-Object { $_ } | Sort-Object -Unique)
        $postAddresses = @($addressLines | Where-Object { $_.Line -match (' op=' + [regex]::Escape($postOperation) + ' ') } | ForEach-Object {
            [regex]::Match($_.Line, 'address=(0x[0-9A-Fa-f]+)').Groups[1].Value
        } | Where-Object { $_ } | Sort-Object -Unique)
        Assert-True ($preAddresses.Count -gt 0 -and $postAddresses.Count -gt 0) 'pre/post line mappings contain machine addresses'
        Assert-True (($preAddresses -join ',') -ne ($postAddresses -join ',')) 'the real rebuild materially changes the complete source-line mapping'

        $boundLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern 'DEVELOPER_STUDIO_PHASE29N_HOST_BREAKPOINT stage=BOUND')
        $preBound = @($boundLines | Where-Object { $_.Line -match (' op=' + [regex]::Escape($preOperation) + ' ') } | Select-Object -First 1)
        $postBound = @($boundLines | Where-Object { $_.Line -match (' op=' + [regex]::Escape($postOperation) + ' ') } | Select-Object -First 1)
        Assert-True ($preBound.Count -eq 1 -and $postBound.Count -eq 1) 'each build creates a verified runtime breakpoint binding'
        $prePrimary = [regex]::Match($preBound[0].Line, 'addr=(0x[0-9A-Fa-f]+)').Groups[1].Value
        $postPrimary = [regex]::Match($postBound[0].Line, 'addr=(0x[0-9A-Fa-f]+)').Groups[1].Value
        Assert-True ($prePrimary -and $postPrimary -and $prePrimary -ne $postPrimary) 'the primary runtime breakpoint address changes after remap'

        $managerLines = @(Select-String -LiteralPath $script:StdoutPath -Pattern 'debug_manager_row_select=PASS row_id=.*runtime_id=(\d+) runtime_session=(\d+) binding_id=(\d+)')
        $runtimeSessions = @($managerLines | ForEach-Object {
            $session = [regex]::Match($_.Line, 'runtime_session=(\d+)').Groups[1].Value
            if ($session -and $session -ne '0') { $session }
        } | Sort-Object -Unique)
        Assert-True ($runtimeSessions.Count -ge 2) 'pre/post remap rows carry fresh runtime session identities'
        $stopLine = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern 'debug_stop=requested' | Select-Object -First 1)
        $startLines = @(Select-String -LiteralPath $script:StdoutPath -SimpleMatch -Pattern 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS')
        $postStartLine = if ($startLines.Count -ge 2) { $startLines[1].LineNumber } else { [int]::MaxValue }
        $unboundAfterStop = @($managerLines | Where-Object {
            $_.LineNumber -gt $stopLine[0].LineNumber -and $_.LineNumber -lt $postStartLine -and
            $_.Line -match 'runtime_id=0 runtime_session=0 binding_id=0'
        })
        Assert-True ($stopLine.Count -eq 1 -and $unboundAfterStop.Count -ge 1) 'the pre-rebuild runtime binding disappears before the new debug session'

        Write-Host "remap_pre_artifact_sha256=$($uniqueArtifacts[0].Hash)"
        Write-Host "remap_post_artifact_sha256=$($uniqueArtifacts[-1].Hash)"
        Write-Host "remap_pre_operation=$preOperation addresses=$($preAddresses -join ',') primary=$prePrimary"
        Write-Host "remap_post_operation=$postOperation addresses=$($postAddresses -join ',') primary=$postPrimary"
        Write-Host "remap_runtime_sessions=$($runtimeSessions -join ',')"
        Write-Host 'manager_remap_retention=PASS'
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
