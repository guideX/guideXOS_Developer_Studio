[CmdletBinding()]
param(
    [ValidateSet('ConditionEditor', 'ConditionErrorRecovery')]
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

function Add-OpenConditionEditor([string]$ExpectedText = '') {
    Add-CaptureMarker 'debug_condition_editor=OPEN breakpoint_id='
    if ($ExpectedText) { Add-CaptureMarker " text=$ExpectedText" }
    Add-Key 67 # C opens the selected breakpoint's real condition editor.
    Add-Wait 2
    Add-LogSnapshot
    Add-WaitMarkerFresh 'debug_condition_editor=OPEN breakpoint_id='
    if ($ExpectedText) { Add-WaitMarkerFresh " text=$ExpectedText" }
}

function Add-CommitCondition([string]$Value) {
    Add-CaptureMarker " text=$Value parse=VALID state=CONDITIONAL"
    Add-CaptureMarker 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id='
    Add-Key 13
    Add-LogSnapshot
    Add-WaitMarkerFresh " text=$Value parse=VALID state=CONDITIONAL"
    Add-WaitMarkerFresh 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id='
}

function Add-InvalidCondition([string]$Value) {
    Add-CaptureMarker " text=$Value parse=INVALID state=CONDITION_REPLACED"
    Add-CaptureMarker 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id='
    Add-Key 13
    Add-LogSnapshot
    Add-WaitMarkerFresh " text=$Value parse=INVALID state=CONDITION_REPLACED"
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

function Add-InitialSetup() {
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
    Add-Command "gui.activate $script:WindowId"
    Add-Click 300 180
    Add-Wait 1
    Add-Key 83 3
    Add-Wait 1
    for ($index = 1; $index -lt $BreakpointLine; ++$index) {
        Add-Key 40
        Add-WaitMilliseconds 150
    }
    Add-Command "gui.activate $script:WindowId"
    Add-Wait 1
    Add-Key 120
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint_toggle=PASS'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint=PENDING'
    # Normalize any stale rows left by an interrupted legacy smoke through
    # the real Breakpoints panel. The runtime part inspects the bounded panel
    # count marker and disables only the known stale leading row when present.
    Add-OpenDebugBreakpoints
    $script:Parts.Add('NORMALIZE|breakpoints')
    # Keep the real panel open and select the intended row for all following
    # condition-editor actions; no menu reopen is needed after this point.
    Add-Key 40
    Add-WaitMilliseconds 300
}

function Add-SetCondition([string]$Value) {
    if (-not $script:PanelReady) { Add-OpenDebugBreakpoints }
    Add-OpenConditionEditor
    Add-ConditionText $Value
    Add-CommitCondition $Value
}

function Add-ConditionEditorCase() {
    Add-InitialSetup
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
    Add-OpenConditionEditor 'counter = 2'
    Add-ClearCondition
    Add-OpenConditionEditor
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'
    Add-CaptureMarker 'text=counter == 2 enabled=FALSE'
    Add-Key 32
    Add-LogSnapshot
    Add-WaitMarkerFresh 'text=counter == 2 enabled=FALSE'
    Add-CaptureMarker 'text=counter == 2 enabled=TRUE'
    Add-Key 32
    Add-LogSnapshot
    Add-WaitMarkerFresh 'text=counter == 2 enabled=TRUE'
    Add-Key 116 2
    Add-LogSnapshot
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-OpenConditionEditor 'counter == 2'
    # Leave the real modal active. The targeted close must bypass modal focus
    # and drive the authoritative Phase 19 shutdown sequence.
    Add-TargetedClose
}

function Add-ConditionErrorRecoveryCase() {
    Add-InitialSetup
    Add-SetCondition 'unknown_value == 2'
    Add-Key 116 2
    Add-LogSnapshot
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
    Add-WaitMarker 'Debug: breakpoint condition error'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-OpenConditionEditor 'unknown_value == 2'
    Add-ConditionText 'counter == 2'
    Add-CommitCondition 'counter == 2'
    # The panel owns unmodified function-key input while visible. Close it
    # through the real panel Escape path before the recovery Continue/F5.
    Add-Key 27
    Add-WaitMilliseconds 300
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING'
    Add-CaptureMarker 'Debug: breakpoint condition false; continuing'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-CaptureMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    Add-Key 116
    Add-LogSnapshot
    # The replacement condition owns the current stopped trap. One Continue
    # must therefore drive the normal false -> false -> true conditional path;
    # a prior aggregate used a stale PAUSED marker and sent a second F5 before
    # the first recovery transition was observable.
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING'
    Add-WaitMarkerFresh 'Debug: breakpoint condition false; continuing'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS'
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT'
    $script:PanelReady = $false
    Add-OpenDebugBreakpoints
    Add-OpenConditionEditor 'counter == 2'
    Add-ClearCondition
    Add-TargetedClose
}

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) 'experimental hosted Server exists'
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot 'guidexos.project') -PathType Leaf) 'Phase 20 fixture project exists'
Assert-True ($BreakpointLine -gt 0) 'breakpoint line is positive'

if ($Case -eq 'ConditionEditor') { Add-ConditionEditorCase }
else { Add-ConditionErrorRecoveryCase }

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
    if ($Case -eq 'ConditionEditor') {
        Assert-True (Test-Marker 'debug_condition_commit=INVALID') 'invalid syntax is reported by the real editor'
        Assert-True ((Test-Marker 'text=counter = 2 parse=INVALID state=CONDITION_REPLACED') -and
                     (Test-Marker 'debug_condition_editor=CLOSED reason=COMMIT breakpoint_id=')) 'invalid syntax does not become unconditional'
        Assert-True ((Test-Marker 'enabled=FALSE') -and (Test-Marker 'enabled=TRUE')) 'disable and re-enable retain the same condition'
        Assert-True (Test-Marker 'shutdown=TARGETED_CLOSE') 'modal-active targeted close is observed'
        Write-Host 'condition_editor_valid=PASS'
        Write-Host 'condition_editor_invalid_edit_clear_cancel=PASS'
        Write-Host 'condition_editor_modal_close=PASS'
    } else {
        $falseHits = Get-MarkerCount 'Debug: breakpoint condition false; continuing'
        Assert-True $falseHits -ge 2 'ConditionError recovery reaches false, false, true behavior'
        Assert-True ((Test-Marker 'ConditionError') -or (Test-Marker 'Debug: breakpoint condition error')) 'hosted ConditionError is surfaced'
        Assert-True (Test-Marker 'debug_condition_commit=PASS') 'ConditionError recovery commits a valid replacement through the editor'
        Assert-True (Test-Marker 'debug_condition_clear=PASS') 'ConditionError recovery clears the condition afterward'
        Write-Host 'condition_error_recovery=PASS'
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
