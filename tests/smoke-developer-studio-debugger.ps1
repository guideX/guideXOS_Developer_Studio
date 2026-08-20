[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO",
    [string]$FixtureRoot = "",
    [int]$BreakpointLine = 20,
    [int]$DebugWaitSeconds = 120,
    [int]$MaxRuntimeSeconds = 240,
    [switch]$DiagnosticOnly,
    [switch]$ContinueBreakpoint,
    [switch]$StepInto,
    [switch]$StepOver,
    [switch]$StepOut,
    [switch]$RepeatedStepOut,
    [switch]$ContinueAfterStepOut,
    [switch]$StepOutThenStepInto,
    [switch]$StepOutThenStepOver,
    [switch]$MixedLifecycle,
    [switch]$OverlapStepOut,
    [int]$OverlapBreakpointLine = 20,
    [switch]$InteractiveWatch,
    [string]$TraceDirectory = "",
    [int]$TraceRunIndex = 0
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$StepOutProof = $StepOut -or $RepeatedStepOut -or $ContinueAfterStepOut -or
                $StepOutThenStepInto -or $StepOutThenStepOver -or $OverlapStepOut
if (-not $FixtureRoot) {
    $FixtureRoot = Join-Path $RepoRoot $(if ($StepOutProof) { "tests\fixtures\debugger-phase8" } else { "tests\fixtures\debugger-phase3b" })
}
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
$Executable = Join-Path $ServerRoot "guideXOSServer.experimental.exe"
$WatchExpression = if ($FixtureRoot -match 'debugger-phase9|debugger-phase10') { 'doubled == 42' } else { 'ctx != 0' }

if ($StepOutProof -and $FixtureRoot -match 'debugger-phase8') {
    if ($BreakpointLine -eq 20) { $BreakpointLine = 9 }
    if ($OverlapStepOut -and $OverlapBreakpointLine -eq 20) { $OverlapBreakpointLine = 15 }
}

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) {
        $failure = "Debugger Phase 3B smoke failed: $Message"
        $failureContent = if ($text) { $text } else { "" }
        Write-ShutdownTrace $failure $failureContent
        throw $failure
    }
    Write-Host "PASS: $Message"
}

function Add-ServerLine([System.Collections.Generic.List[string]]$Parts, [string]$Line) {
    $Parts.Add("COMMAND|$Line")
}

function Add-ShortDelay([System.Collections.Generic.List[string]]$Parts) {
    # Keep the command stream bounded and let PowerShell pace the real Server
    # stdin.  The previous cmd.exe pipeline encoded every delay into one
    # command line and could exceed cmd.exe's command-line limit once an
    # interactive Watch expression was added.
    $Parts.Add("WAIT|1")
}

function Add-Delay([System.Collections.Generic.List[string]]$Parts, [int]$Seconds) {
    $Parts.Add("WAIT|$([Math]::Max(1, $Seconds))")
}

function Add-Key([System.Collections.Generic.List[string]]$Parts, [int]$KeyCode, [int]$Modifiers = 0, [bool]$WaitForUi = $false) {
    Add-ServerLine $Parts "gui.keyto 1000 $KeyCode down $Modifiers"
    if ($WaitForUi) { Add-ShortDelay $Parts }
}

function Add-Mouse([System.Collections.Generic.List[string]]$Parts, [int]$WindowId, [int]$X, [int]$Y, [int]$Button, [string]$Action, [bool]$WaitForUi = $false) {
    Add-ServerLine $Parts "gui.mouse $WindowId $X $Y $Button $Action"
    if ($WaitForUi) { Add-ShortDelay $Parts }
}

function Get-Key([char]$Character, [int]$Modifiers) {
    $Modifiers = 0
    if (($Character -cge 'a') -and ($Character -cle 'z')) {
        return @{ Key = [int][char](([string]$Character).ToUpperInvariant()); Modifiers = 0 }
    }
    if (($Character -cge 'A') -and ($Character -cle 'Z')) {
        return @{ Key = [int][char]$Character; Modifiers = 1 }
    }
    if (($Character -ge '0') -and ($Character -le '9')) {
        return @{ Key = [int][char]$Character; Modifiers = 0 }
    }
    if ($Character -eq '\') { return @{ Key = 220; Modifiers = 0 } }
    if ($Character -eq ':') { return @{ Key = 186; Modifiers = 1 } }
    if ($Character -eq '-') { return @{ Key = 189; Modifiers = 0 } }
    if ($Character -eq '_') { return @{ Key = 189; Modifiers = 1 } }
    if ($Character -eq '=') { return @{ Key = 187; Modifiers = 0 } }
    if ($Character -eq '!') { return @{ Key = 49; Modifiers = 1 } }
    if ($Character -eq '.') { return @{ Key = 190; Modifiers = 0 } }
    if ($Character -eq ' ') { return @{ Key = 32; Modifiers = 0 } }
    throw "Unsupported fixture path character: $Character"
}

function Get-LiveHostedText([string]$StdoutPath, [string]$StderrPath) {
    $stdout = if (Test-Path -LiteralPath $StdoutPath) { Get-Content -LiteralPath $StdoutPath -Raw -ErrorAction SilentlyContinue } else { "" }
    $stderr = if (Test-Path -LiteralPath $StderrPath) { Get-Content -LiteralPath $StderrPath -Raw -ErrorAction SilentlyContinue } else { "" }
    return ($stdout + "`n" + $stderr)
}

function Get-LastShutdownStage([string]$Content) {
    $stages = @(
        @{ Name = "complete"; Pattern = "debug_shutdown_complete=PASS|shutdownStage=complete" },
        @{ Name = "window_release"; Pattern = "debug_window_release=(PASS|REQUESTED)|shutdownStage=window_release" },
        @{ Name = "session_teardown"; Pattern = "debug_session_teardown=PASS|shutdownStage=session_teardown" },
        @{ Name = "target_teardown"; Pattern = "debug_target_teardown=PASS|shutdownStage=target_teardown" },
        @{ Name = "stop_requested"; Pattern = "debug_stop=requested|shutdownStage=stop_requested" },
        @{ Name = "request"; Pattern = "debug_shutdown_request=|shutdownStage=request" }
    )
    foreach ($stage in $stages) {
        if ($Content -match $stage.Pattern) { return $stage.Name }
    }
    return "none"
}

function Wait-ForShutdown([Diagnostics.Process]$HostedProcess, [string]$StdoutPath, [string]$StderrPath, [int]$TimeoutSeconds) {
    $deadline = (Get-Date).AddSeconds([Math]::Max(1, $TimeoutSeconds))
    $nextStateQuery = Get-Date
    $requiredMarkers = @(
        'debug_shutdown_request=targeted_close',
        'debug_stop=requested',
        'debug_target_teardown=PASS',
        'debug_session_teardown=PASS',
        'debug_window_release=PASS',
        'debug_shutdown_complete=PASS'
    )
    while ((Get-Date) -lt $deadline) {
        if (-not $HostedProcess.HasExited -and (Get-Date) -ge $nextStateQuery) {
            $HostedProcess.StandardInput.WriteLine('nativeapp.processes')
            $HostedProcess.StandardInput.Flush()
            $nextStateQuery = (Get-Date).AddSeconds(2)
        }
        $content = Get-LiveHostedText $StdoutPath $StderrPath
        $allMarkersPresent = $true
        foreach ($marker in $requiredMarkers) {
            if (-not $content.Contains($marker)) { $allMarkersPresent = $false; break }
        }
        if ($allMarkersPresent -and $content -match 'shutdownStage=complete' -and $content -match 'state=Exited') {
            return $content
        }
        Start-Sleep -Milliseconds 200
    }

    $content = Get-LiveHostedText $StdoutPath $StderrPath
    $stateLine = @($content -split "`r?`n" | Where-Object { $_ -match 'Native app processes:|runtimeId=.*shutdownStage=|windowCount=|owner' } | Select-Object -Last 8)
    $markerLine = @($content -split "`r?`n" | Where-Object { $_ -match 'debug_shutdown|debug_stop|debug_target|debug_session|debug_window|shutdownStage=' } | Select-Object -Last 24)
    $debuggerState = if ($content -match 'debug_state=([^ ]+)') { $Matches[1] } else { 'unknown' }
    $session = if ($content -match 'debug_session=(\d+)') { $Matches[1] } else { 'unknown' }
    $target = if ($content -match 'target-created.*processId=(\d+)') { $Matches[1] } else { 'unknown' }
    $details = @(
        "WAITSHUTDOWN timeout expected=complete actual=$((Get-LastShutdownStage $content))",
        "debuggerState=$debuggerState",
        "session=$session",
        "target=$target",
        "state/ownership:",
        ($stateLine -join "`n"),
        "recent lifecycle markers:",
        ($markerLine -join "`n")
    )
    throw ($details -join "`n")
}

function Write-ShutdownTrace([string]$Reason, [string]$Content) {
    try {
        $directory = if ($TraceDirectory) { [IO.Path]::GetFullPath($TraceDirectory) } else { Join-Path $RepoRoot "logs" }
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
        $suffix = if ($TraceRunIndex -gt 0) { "-$TraceRunIndex" } else { "" }
        $artifact = Join-Path $directory "developer-studio-debugger-shutdown-trace$suffix.log"
        $lines = @($Content -split "`r?`n")
        $lifecycle = @($lines | Where-Object { $_ -match 'debug_session|debug_state|debug_stop|debug_step|debug_binding|debug_transition|debug_shutdown|debug_target|debug_window|shutdownStage=|Native app processes:|Native app debug log:' } | Select-Object -Last 96)
        $recent = @($lines | Select-Object -Last 80)
        $serverExitCode = if ($process -and $process.HasExited) { $process.ExitCode } else { 'unknown' }
        $header = @(
            "guideXOS Developer Studio hosted shutdown trace",
            "reason=$Reason",
            "lastShutdownStage=$(Get-LastShutdownStage $Content)",
            "serverExitCode=$serverExitCode",
            "boundedLifecycleMarkerCount=$($lifecycle.Count)",
            "--- recent bounded lifecycle markers ---"
        )
        $body = ($header + $lifecycle + @("--- recent output ---") + $recent) -join "`r`n"
        if ($body.Length -gt 65536) { $body = $body.Substring($body.Length - 65536) }
        Set-Content -LiteralPath $artifact -Value $body -Encoding UTF8
        Write-Host "Shutdown trace artifact: $artifact"
    } catch {
        Write-Host "WARNING: unable to write shutdown trace: $($_.Exception.Message)"
    }
}

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) "rebuilt experimental hosted Server exists"
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot "guidexos.project") -PathType Leaf) "checked-in debugger fixture exists"
Assert-True ((@($ContinueBreakpoint, $StepInto, $StepOver, $StepOut) | Where-Object { $_ }).Count -le 1) "debugger smoke mode is unambiguous"
Assert-True (-not ($RepeatedStepOut -and $ContinueAfterStepOut)) "Step Out follow-up mode is unambiguous"
Assert-True (-not ($StepOutThenStepInto -and $StepOutThenStepOver)) "post-Step Out mode is unambiguous"
Assert-True (-not ($MixedLifecycle -and ($RepeatedStepOut -or $ContinueAfterStepOut -or $StepOutThenStepInto -or $StepOutThenStepOver))) "mixed lifecycle follow-up mode is unambiguous"
Assert-True ((-not $RepeatedStepOut -and -not $ContinueAfterStepOut -and -not $StepOutThenStepInto -and -not $StepOutThenStepOver -and -not $MixedLifecycle) -or $StepOut -or $MixedLifecycle) "Step Out follow-up requires Step Out mode"
Assert-True (-not $OverlapStepOut -or $StepOut) "overlap proof requires Step Out mode"
$parts = New-Object 'System.Collections.Generic.List[string]'
Add-ServerLine $parts 'gui.start'
Add-Delay $parts 8
Add-ServerLine $parts 'desktop.launch com.guidexos.developerstudio'
Add-Delay $parts 12
Add-ServerLine $parts 'desktop.windows.owners'
Add-Delay $parts 5
Add-ServerLine $parts 'gui.activate 1000'
Add-ShortDelay $parts

# Open the checked-in fixture through Developer Studio's real project dialog.
Add-Mouse $parts 1000 300 180 1 'down' $false
Add-Mouse $parts 1000 300 180 1 'up' $true
Add-Key $parts 79 3 $true
foreach ($character in $FixtureRoot.ToLowerInvariant().ToCharArray()) {
    $key = Get-Key $character 0
    Add-Key $parts $key.Key $key.Modifiers
    Add-ShortDelay $parts
}
Add-Key $parts 13 0 $true
Add-Delay $parts 10
# Normalize the imported workspace through the real Save All shortcut before
# any debugger input is sent. This is safe whether the workspace is already
# clean or has a pending imported-document change.
Add-Key $parts 83 3 $true
Add-Delay $parts 2

# Move the real editor caret to src/main.cpp:20 and arm F9.
for ($index = 1; $index -lt $BreakpointLine; ++$index) { Add-Key $parts 40 0 $false }
Add-Key $parts 120 0 $true
if ($OverlapStepOut) {
    Assert-True ($OverlapBreakpointLine -gt $BreakpointLine) "overlap breakpoint is after the initial callee breakpoint"
    for ($index = $BreakpointLine; $index -lt $OverlapBreakpointLine; ++$index) { Add-Key $parts 40 0 $false }
    Add-Key $parts 120 0 $true
}
Add-Delay $parts 30
Add-ServerLine $parts 'gui.activate 1000'
Add-ShortDelay $parts

# Ctrl+F5 starts the real Developer Studio build -> hosted launch -> bind -> trap path.
Add-Key $parts 116 2 $true
if ($ContinueBreakpoint) { Add-ShortDelay $parts } else { Add-Delay $parts $DebugWaitSeconds }

if ($InteractiveWatch) {
    # Open the product's Debug menu and Watch tab through compositor mouse
    # events, then add a deterministic comparison against the stopped target.
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Mouse $parts 1000 610 30 1 'down' $false
    Add-Mouse $parts 1000 610 30 1 'up' $true
    Add-Mouse $parts 1000 620 340 1 'down' $false
    Add-Mouse $parts 1000 620 340 1 'up' $true
    Add-Key $parts 65 0 $true
    $watchCharacterIndex = 0
    foreach ($character in $WatchExpression.ToCharArray()) {
        $key = Get-Key $character 0
        # Reassert focus at a bounded cadence while the target's own window
        # exists. This exercises the supported activation route without
        # creating a repaint storm for every character.
        if (($watchCharacterIndex % 4) -eq 0) {
            Add-ServerLine $parts 'gui.activate 1000'
            Add-ShortDelay $parts
        }
        Add-Key $parts $key.Key $key.Modifiers $true
        ++$watchCharacterIndex
    }
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Key $parts 13 0 $true
    Add-Delay $parts 4
}

if ($ContinueBreakpoint) {
    # F5 continues the real stopped target. The fixture remains alive after
    # the breakpointed instruction, so the smoke can observe Running without
    # relying on process exit to imply successful continuation.
    Add-Key $parts 116 0 $true
    Add-ShortDelay $parts
} elseif ($StepInto) {
    # F11 begins the real user source-step operation from the breakpoint stop.
    Add-Key $parts 122 0 $true
    Add-Delay $parts 20
} elseif ($StepOver) {
    # F10 begins the real call-aware/fallback Step Over operation from the
    # breakpoint stop. This checked-in fixture exercises the non-call fallback;
    # the native runtime harness proves the E8 call/return path.
    Add-Key $parts 121 0 $true
    Add-Delay $parts 20
} elseif ($MixedLifecycle) {
    # Exercise a bounded source-step mixture and Continue in one hosted
    # session. The dedicated Step Out follow-up modes cover the return path.
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Key $parts 122 0 $true
    Add-Delay $parts 20
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Key $parts 121 0 $true
    Add-Delay $parts 20
    Add-Key $parts 116 0 $true
    Add-Delay $parts 20
} elseif ($StepOut) {
    # Use the real Debug menu row for the return-address Step Out operation
    # from the deepest fixture frame. The controller must stop at the
    # immediate caller's raw return address.
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Mouse $parts 1000 610 30 1 'down' $false
    Add-Mouse $parts 1000 610 30 1 'up' $true
    Add-Mouse $parts 1000 620 141 1 'down' $false
    Add-Mouse $parts 1000 620 141 1 'up' $true
    Add-Delay $parts 20
    if ($RepeatedStepOut) {
        Add-ServerLine $parts 'gui.activate 1000'
        Add-ShortDelay $parts
        Add-Mouse $parts 1000 610 30 1 'down' $false
        Add-Mouse $parts 1000 610 30 1 'up' $true
        Add-Mouse $parts 1000 620 141 1 'down' $false
        Add-Mouse $parts 1000 620 141 1 'up' $true
        Add-Delay $parts 20
    } elseif ($ContinueAfterStepOut) {
        Add-ServerLine $parts 'gui.activate 1000'
        Add-ShortDelay $parts
        Add-Key $parts 116 0 $true
        Add-Delay $parts 20
    } elseif ($StepOutThenStepInto -or $StepOutThenStepOver) {
        Add-ServerLine $parts 'gui.activate 1000'
        Add-ShortDelay $parts
        Add-Key $parts ($(if ($StepOutThenStepInto) { 122 } else { 121 })) 0 $true
        Add-Delay $parts 20
    } elseif ($OverlapStepOut) {
        Add-ServerLine $parts 'gui.activate 1000'
        Add-ShortDelay $parts
        Add-Key $parts 116 0 $true
        Add-Delay $parts 20
    }
}
Add-ServerLine $parts 'nativeapp.processes'
Add-Delay $parts 2
Add-ServerLine $parts 'log'
Add-Delay $parts 2

if ($DiagnosticOnly) {
    Add-ServerLine $parts 'gui.close 1000'
    $parts.Add("WAITSHUTDOWN|$DebugWaitSeconds")
} elseif ($ContinueBreakpoint) {
    # The target is Running after Continue. The owned-window close still uses
    # the product's targeted shutdown path and requests debugger termination
    # directly rather than routing C/S through whichever window has focus.
    Add-ServerLine $parts 'gui.close 1000'
    $parts.Add("WAITSHUTDOWN|$DebugWaitSeconds")
    Add-ServerLine $parts 'nativeapp.processes'
    Add-ServerLine $parts 'desktop.windows.owners'
    Add-ServerLine $parts 'nativeapp.debuglog 64'
} else {
    # Targeted gui.close is the authoritative hosted shutdown request. The
    # app owns the stop/teardown sequence; no focused confirmation keystroke
    # is part of the successful path.
    Add-ServerLine $parts 'gui.close 1000'
    $parts.Add("WAITSHUTDOWN|$DebugWaitSeconds")
    Add-ServerLine $parts 'nativeapp.processes'
    Add-ServerLine $parts 'desktop.windows.owners'
    Add-ServerLine $parts 'nativeapp.debuglog 64'
}
Add-ServerLine $parts 'exit'

$startInfo = New-Object Diagnostics.ProcessStartInfo
$stdoutPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-debugger-$([Guid]::NewGuid().ToString('N')).out")
$stderrPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-debugger-$([Guid]::NewGuid().ToString('N')).err")
$startInfo.FileName = $env:ComSpec
$startInfo.Arguments = "/d /s /c `"`"$Executable`" 1>`"$stdoutPath`" 2>`"$stderrPath`"`""
$startInfo.WorkingDirectory = $ServerRoot
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $false
$startInfo.RedirectStandardError = $false
$process = New-Object Diagnostics.Process
$process.StartInfo = $startInfo
$text = ""
$smokeSucceeded = $false
try {
    Assert-True $process.Start() "streamed hosted UI proof starts"
    foreach ($part in $parts) {
        if ($process.HasExited) { break }
        $separator = $part.IndexOf('|')
        if ($separator -lt 0) { continue }
        $kind = $part.Substring(0, $separator)
        $value = $part.Substring($separator + 1)
        if ($kind -eq 'WAIT') {
            Start-Sleep -Seconds ([Math]::Max(1, [int]$value))
        } elseif ($kind -eq 'WAITSHUTDOWN') {
            Wait-ForShutdown $process $stdoutPath $stderrPath ([int]$value) | Out-Null
            Write-Host "PASS: WAITSHUTDOWN reached complete (bounded state poll)"
        } elseif ($kind -eq 'COMMAND') {
            $process.StandardInput.WriteLine($value)
            $process.StandardInput.Flush()
        }
    }
    $process.StandardInput.Close()
    $timedOut = $false
    $deadline = (Get-Date).AddSeconds($MaxRuntimeSeconds)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    if (-not $process.HasExited) {
        $timedOut = $true
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        $process.WaitForExit()
    }
    $text = Get-LiveHostedText $stdoutPath $stderrPath

    Assert-True ($text.Contains('Desktop launch successful: com.guidexos.developerstudio')) "Developer Studio launches through the hosted desktop"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS')) "hosted Developer Studio reaches its real initial render"
    Assert-True ($text -match 'window id=1000 ownerPid=\d+ ownerName=nativeelf:com.guidexos.developerstudio') "Developer Studio window ownership is published"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=PASS') -and
                 $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER project_metadata_parse=PASS')) "fixture project opens through Developer Studio"
    if ($text -notmatch 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint(_toggle)?=(PASS|PENDING|MAPPED)') {
        Write-Host 'Captured breakpoint diagnostics:'
        @($text -split "`r?`n" | Where-Object { $_ -match 'debug_|F9|breakpoint|editor|focus|Caret|project_open|project_metadata' } | Select-Object -Last 160)
    }
    Assert-True ($text -match 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint(_toggle)?=(PASS|PENDING|MAPPED)') "F9 arms the source breakpoint in the editor"
    if (-not $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS')) {
        Write-Host 'Captured debug diagnostics:'
        @($text -split "`r?`n" | Where-Object { $_ -notmatch 'draw_text' -and $_ -match '\[DevelopmentRun\]|\[NativeElf|\[NativeAppDebugger\]|Native app|Debug|debug_|GUIDEXOS_PHASE3B_FIXTURE|Build:|Run:|target-created|breakpoint|Project build|launch|Launch|execution|mapped|runtime' } | Select-Object -Last 240)
        Write-Host 'Captured UI command diagnostics:'
        @($text -split "`r?`n" | Where-Object { $_ -match 'Save All|Debug|Build|dirty|blocked|project|document|breakpoint' } | Select-Object -Last 160)
        Write-Host 'Development/native launch records:'
        @($text -split "`r?`n" | Where-Object { $_ -match '\[DevelopmentRun\]|\[NativeElf|\[NativeAppDebugger\]|Native app launch failed|Debug launch failed' })
    }
    if ($timedOut) {
        Write-Host 'Captured hosted output before smoke timeout:'
        @($text -split "`r?`n" | Select-Object -Last 80)
        Write-Host 'Captured hosted markers before smoke timeout:'
        @($text -split "`r?`n" | Where-Object { $_ -match 'GUIDEXOS_DEVELOPER_STUDIO_MARKER|Mouse queued|Key queued|Activate sent|Close requested|gui.mouse|Debug:|Watch|watch|error|Error|fail|Fail' } | Select-Object -Last 220)
        throw "Debugger Phase 3B smoke timed out after $MaxRuntimeSeconds seconds"
    }
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS')) "Ctrl+F5 starts the hosted debug session"
    if (-not $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) {
        Write-Host 'Captured hosted breakpoint-pause diagnostics:'
        @($text -split "`r?`n" | Where-Object { $_ -notmatch 'draw_text' -and $_ -match '\[DevelopmentRun\]|\[NativeElf|\[NativeAppDebugger\]|target-created|debug_|GUIDEXOS_PHASE3B_FIXTURE|Phase3B draw_rect caller|breakpoint|bound|Bound|Verified|trap|execution|runtime|Native app|Debug:' } | Select-Object -Last 300)
    }
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) "the real source breakpoint produces a breakpoint pause"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_source_navigation=PASS')) "the breakpoint stop navigates to the existing source document"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_execution_marker=PASS')) "the breakpoint stop publishes the editor execution marker"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_call_stack=PASS')) "the stopped hosted session builds its real call stack"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_variables=PASS')) "the stopped hosted session publishes real locals"
    if ($InteractiveWatch) {
        $watchRow = 'draw_text windowId=1000 pos=100,198 text="' + $WatchExpression + '"'
        if (-not ($text.Contains($watchRow) -and
                  $text.Contains('draw_text windowId=1000 pos=620,198 text="true"'))) {
            Write-Host 'Captured hosted Watch rows:'
            @($text -split "`r?`n" | Where-Object { $_ -match 'draw_text windowId=1000 pos=(100,198|620,198|220,285)|Add Watch|Edit Watch|Watch update failed|Watch' } | Select-Object -Last 160)
        }
        Assert-True ($text.Contains($watchRow) -and
                     $text.Contains('draw_text windowId=1000 pos=620,198 text="true"')) "the hosted Watch UI retains and evaluates a real comparison"
    }
    if ($ContinueBreakpoint) {
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING') -and
                     $text.Contains('[NativeAppDebugger] breakpoint continuation accepted') -and
                     $text.Contains('EXCEPTION_SINGLE_STEP') -and
                     $text.Contains('rebound=true')) "F5 continues through a real single-step and rebinds the breakpoint"
    } elseif ($StepInto -or $StepOver -or $StepOut -or $MixedLifecycle) {
        $overlapStop = if ($OverlapStepOut) { "Debug: paused | Breakpoint | src/main.cpp:$OverlapBreakpointLine" } else { "" }
        $stepEvidence = $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=STEPPING') -and
                        (($OverlapStepOut -and $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) -or
                         (-not $OverlapStepOut -and $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_STEP'))) -and
                        $text.Contains('EXCEPTION_SINGLE_STEP') -and
                         (($StepInto -and $text.Contains('[NativeAppDebugger] user source-step accepted') -and $text.Contains('Debug: step into')) -or
                          ($StepOver -and $text.Contains('Debug: step over')) -or
                          ($MixedLifecycle -and $text.Contains('Debug: step into') -and $text.Contains('Debug: step over')) -or
                           ($OverlapStepOut -and $text.Contains('Debug: step out') -and
                           $text.Contains($overlapStop) -and
                           $text.Contains('debug_step=StepOut') -and
                           $text.Contains('cleanup=1') -and
                           -not ($text -match 'stale_step_out|stale_step_over')) -or
                           ($StepOut -and $text.Contains('Debug: step out') -and
                            (($text -match 'Debug: paused \| Step \| src/main.cpp:(14|15|16|19|20|21)' -and
                              $text.Contains('[NativeAppDebugger] internal single-step observed')) -or
                             ($StepOutThenStepInto -and $text.Contains('Debug: step into')) -or
                             ($StepOutThenStepOver -and $text.Contains('Debug: step over'))) -and
                           -not ($text -match 'stale_step_over|invalid_transition')))
        if (-not $stepEvidence) {
            Write-Host 'Captured hosted stepping diagnostics:'
            @($text -split "`r?`n" | Where-Object { $_ -match 'F11|step|Step|STEPPING|PAUSED_STEP|EXCEPTION_SINGLE_STEP|Debug:|debug_state|source_navigation|execution_marker' } | Select-Object -Last 260)
        }
        if ($StepInto) { Assert-True $stepEvidence "F11 performs a real hosted source-level Step Into" }
        elseif ($StepOver) { Assert-True $stepEvidence "F10 performs a real hosted source-level Step Over fallback" }
        elseif ($MixedLifecycle) {
            Write-Host 'Captured mixed lifecycle diagnostics:'
            @($text -split "`r?`n" | Where-Object { $_ -match 'Debug: (step|paused|process)|debug_step=|debug_state=|debug_transition=|breakpoint continuation|stale|invalid_transition' } | Select-Object -Last 180)
            Assert-True ($text.Contains('Debug: step into') -and $text.Contains('Debug: step over') -and
                         ([regex]::Matches($text, 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=STEPPING')).Count -ge 2 -and
                         -not ($text -match 'stale_step_out|stale_step_over|invalid_transition')) "bounded mixed hosted lifecycle remains coherent"
        } else {
            Assert-True $stepEvidence "the real hosted Debug menu performs Step Out to the caller"
            if ($RepeatedStepOut) {
                Assert-True (([regex]::Matches($text, 'Debug: paused \| Step \| src/main.cpp:(14|15|16|19|20|21)')).Count -ge 2) "a second hosted Step Out reaches the next caller frame"
            }
            if ($StepOutThenStepInto) {
                Assert-True ($text.Contains('Debug: step out') -and $text.Contains('Debug: step into') -and
                             ([regex]::Matches($text, 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=STEPPING')).Count -ge 2 -and
                             ([regex]::Matches($text, 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_STEP')).Count -ge 2 -and
                             -not ($text -match 'stale_step_out|stale_step_over')) "Step Out -> Step Into remains valid in one hosted session"
            }
            if ($StepOutThenStepOver) {
                Assert-True ($text.Contains('Debug: step out') -and $text.Contains('Debug: step over') -and
                             ([regex]::Matches($text, 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=STEPPING')).Count -ge 2 -and
                             ([regex]::Matches($text, 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_STEP')).Count -ge 2 -and
                             -not ($text -match 'stale_step_out|stale_step_over')) "Step Out -> Step Over remains valid in one hosted session"
            }
            if ($OverlapStepOut) {
                $overlapStop = "Debug: paused | Breakpoint | src/main.cpp:$OverlapBreakpointLine"
                $overlapStopPattern = [regex]::Escape($overlapStop)
                Write-Host 'Captured overlap lifecycle markers:'
                @($text -split "`r?`n" | Where-Object { $_ -match 'debug_session|debug_step|debug_binding|debug_transition|Debug: paused|Debug: process' } | Select-Object -Last 120)
                Assert-True ($text -match 'debug_session=\d+ process=\d+ runtime=\d+ thread=\d+ state=' -and
                             $text -match 'debug_step=StepOut active=TRUE generation=\d+ session=\d+ completion=\d+ cleanup=\d+ temp_id=\d+ temp_binding=\d+ return=0x[0-9A-Fa-f]+ lookup=0x[0-9A-Fa-f]+ temp=TRUE' -and
                             $text -match 'debug_step=StepOut active=FALSE generation=\d+ session=\d+ completion=\d+ cleanup=1 temp_id=\d+ temp_binding=\d+ return=0x[0-9A-Fa-f]+ lookup=0x[0-9A-Fa-f]+ temp=FALSE') "lifecycle trace exposes session, Step Out generation, IDs, full addresses, and cleanup"
                Assert-True ($text -match 'debug_binding=id=\d+ address=0x[0-9A-Fa-f]+ owners=2 refcount=2 user=1 internal=1 shared=TRUE installed=TRUE' -and
                             $text -match 'debug_binding=id=\d+ address=0x[0-9A-Fa-f]+ owners=1 refcount=1 user=1 internal=0 shared=FALSE installed=TRUE' -and
                             $text -match 'debug_transition=.*->.* sequence=\d+ rejected=none') "lifecycle trace exposes binding refcounts and validated transitions"
                Assert-True ($text -match 'debug_binding=id=.*owners=2.*user=1.*internal=1.*shared=TRUE' -and
                             $text -match 'debug_binding=id=.*owners=1.*user=1.*internal=0.*shared=FALSE' -and
                             $text.Contains($overlapStop) -and
                             $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=RUNNING') -and
                             ([regex]::Matches($text, $overlapStopPattern)).Count -ge 2 -and
                             -not ($text -match 'stale_step_out|stale_step_over')) "overlapping Step Out preserves the user breakpoint and Continue path"
                Assert-True (([regex]::Matches($text, $overlapStopPattern)).Count -ge 2) "the preserved hosted user breakpoint re-hits after Continue"
            }
        }
    }
    Assert-True ($text -match 'target-created.*processId=\d+.*nativeRuntimeId=\d+.*gate=closed') "hosted service publishes exact target identity before release"
    if (-not $DiagnosticOnly) {
        $shutdownMarkers = @(
            'debug_shutdown_request=targeted_close',
            'debug_stop=requested',
            'debug_target_teardown=PASS',
            'debug_session_teardown=PASS',
            'debug_window_release=PASS',
            'debug_shutdown_complete=PASS'
        )
        $previousMarkerPosition = -1
        foreach ($marker in $shutdownMarkers) {
            $markerPosition = $text.IndexOf($marker)
            Assert-True ($markerPosition -ge 0 -and $markerPosition -gt $previousMarkerPosition) "shutdown marker ordering includes $marker"
            $previousMarkerPosition = $markerPosition
        }
        Assert-True ($text -match 'runtimeId=\d+ appId=com\.guidexos\.developerstudio .*state=Exited .*windows=\d+/\d+/0 .*shutdownStage=complete shutdownStageCode=6') "Server durable shutdown state records Exited, released windows, and complete"
        Assert-True ($text -match 'Native app debug log: \d+') "bounded Server lifecycle log query is available"
        $debugLogEntryCount = ([regex]::Matches($text, '(?m)^#\d+ runtimeId=')).Count
        Assert-True ($debugLogEntryCount -le 64) "failure-diagnostic lifecycle log remains bounded to 64 entries"
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_stop=requested')) "the hosted debugger stop is requested through the product close path"
        if (-not $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=EXITED')) {
            Write-Host 'Captured hosted debugger-stop diagnostics:'
            @($text -split "`r?`n" | Where-Object { $_ -match 'debug_stop|debug_state|stop|Stop|close|Close|target|Target|process|Process|NativeAppDebugger|DevelopmentRun|error|Error|fail|Fail' } | Select-Object -Last 220)
        }
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=EXITED')) "the hosted debug session exits cleanly"
        if (-not $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS')) {
            Write-Host 'Captured hosted close diagnostics:'
            @($text -split "`r?`n" | Where-Object { $_ -match 'close|Close|Debug|debug|run|Run|exit|Exit|process|Process|window|Window|dirty|prompt|confirm|error|Error' } | Select-Object -Last 220)
        }
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS')) "Developer Studio closes cleanly after teardown"
    } elseif (($ContinueBreakpoint -or $ContinueAfterStepOut) -and $DiagnosticOnly) {
        Assert-True ($text.Contains('Debug: process running')) "the hosted UI remains responsive after Continue"
    } elseif ($ContinueBreakpoint) {
        if (-not ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER run_close=REQUESTED') -or
                  $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_stop=requested'))) {
            Write-Host 'Captured teardown diagnostics:'
            @($text -split "`r?`n" | Where-Object { $_ -match 'close|Close|Debug|debug|run|Run|exit|Exit|process|Process' } | Select-Object -Last 180)
        }
        Write-Host "INFO: Continue teardown request was not asserted because compositor focus is not deterministic after the target creates its window."
    }
    if ($process.ExitCode -ne 0) {
        Write-Host "Hosted Server exit code: $($process.ExitCode)"
        @($text -split "`r?`n" | Where-Object { $_ -match 'close|Close|Debug|debug|run|Run|exit|Exit|process|Process|error|Error|fail|Fail' } | Select-Object -Last 180)
    }
    Assert-True ($process.ExitCode -eq 0) "hosted Server exits cleanly after the debugger proof (exit code $($process.ExitCode))"
    if ($StepOutThenStepInto) { Write-Host 'Developer Studio Debugger Phase 18 Step Out -> Step Into smoke PASS' }
    elseif ($StepOutThenStepOver) { Write-Host 'Developer Studio Debugger Phase 18 Step Out -> Step Over smoke PASS' }
    elseif ($OverlapStepOut) { Write-Host 'Developer Studio Debugger Phase 18 overlapping Step Out smoke PASS' }
    elseif ($StepInto) { Write-Host 'Developer Studio Debugger Phase 5 end-to-end smoke PASS' }
    elseif ($StepOver) { Write-Host 'Developer Studio Debugger Phase 6 end-to-end smoke PASS' }
    elseif ($StepOut) { Write-Host 'Developer Studio Debugger Phase 8 end-to-end smoke PASS' }
    else { Write-Host 'Developer Studio Debugger Phase 3B end-to-end smoke PASS' }
    $smokeSucceeded = $true
} finally {
    if (-not $smokeSucceeded) {
        $failureText = if ($text) { $text } else { Get-LiveHostedText $stdoutPath $stderrPath }
        Write-ShutdownTrace "smoke failure" $failureText
    }
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    if ($process) { $process.Dispose() }
    Remove-Item -LiteralPath $stdoutPath,$stderrPath -Force -ErrorAction SilentlyContinue
}
