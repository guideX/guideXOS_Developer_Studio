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
    [switch]$InteractiveWatch
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $FixtureRoot) { $FixtureRoot = Join-Path $RepoRoot "tests\fixtures\debugger-phase3b" }
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
$Executable = Join-Path $ServerRoot "guideXOSServer.experimental.exe"
$WatchExpression = if ($FixtureRoot -match 'debugger-phase9|debugger-phase10') { 'doubled == 42' } else { 'ctx != 0' }

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Debugger Phase 3B smoke failed: $Message" }
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

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) "rebuilt experimental hosted Server exists"
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot "guidexos.project") -PathType Leaf) "checked-in Phase 3B fixture exists"
Assert-True ((@($ContinueBreakpoint, $StepInto, $StepOver, $StepOut) | Where-Object { $_ }).Count -le 1) "debugger smoke mode is unambiguous"
Assert-True (-not ($RepeatedStepOut -and $ContinueAfterStepOut)) "Step Out follow-up mode is unambiguous"
Assert-True ((-not $RepeatedStepOut -and -not $ContinueAfterStepOut) -or $StepOut) "Step Out follow-up requires Step Out mode"

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
Add-Delay $parts 30
Add-ServerLine $parts 'gui.activate 1000'
Add-ShortDelay $parts

# Ctrl+F5 starts the real Developer Studio build -> hosted launch -> bind -> trap path.
Add-Key $parts 116 2 $true
Add-Delay $parts $DebugWaitSeconds

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
    Add-Delay $parts 20
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
    }
}
Add-ServerLine $parts 'nativeapp.processes'
Add-Delay $parts 2
Add-ServerLine $parts 'log'
Add-Delay $parts 2

if ($DiagnosticOnly) {
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 2
} elseif ($ContinueBreakpoint) {
    # After Continue the shared run controller is Running, so closing the
    # owned Studio window first asks to close the temporary project app; C
    # requests that close before Studio itself is closed. If the event is
    # serialized after the debugger modal appears, S handles that variant.
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 3
    Add-ServerLine $parts 'gui.activate 1000'
    Add-Key $parts 67 0 $true
    Add-Delay $parts 10
    Add-Key $parts 83 0 $true
    Add-Delay $parts 25
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 12
} else {
    # The original Phase 3B paused-target close path enters the debug-stop
    # confirmation; S confirms stop.
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 3
    Add-ServerLine $parts 'gui.activate 1000'
    Add-Key $parts 83 0 $true
    Add-Delay $parts 50
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 12
}
Add-ServerLine $parts 'exit'

$startInfo = New-Object Diagnostics.ProcessStartInfo
$startInfo.FileName = $Executable
$startInfo.WorkingDirectory = $ServerRoot
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
try {
    Assert-True $process.Start() "streamed hosted UI proof starts"
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    foreach ($part in $parts) {
        if ($process.HasExited) { break }
        $separator = $part.IndexOf('|')
        if ($separator -lt 0) { continue }
        $kind = $part.Substring(0, $separator)
        $value = $part.Substring($separator + 1)
        if ($kind -eq 'WAIT') {
            Start-Sleep -Seconds ([Math]::Max(1, [int]$value))
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
    $stdout = $stdoutTask.Result
    $stderr = $stderrTask.Result
    $text = $stdout + "`n" + $stderr

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
    } elseif ($StepInto -or $StepOver -or $StepOut) {
        $stepEvidence = $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=STEPPING') -and
                        $text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_STEP') -and
                        $text.Contains('EXCEPTION_SINGLE_STEP') -and
                        (($StepInto -and $text.Contains('[NativeAppDebugger] user source-step accepted') -and $text.Contains('Debug: step into')) -or
                         ($StepOver -and $text.Contains('Debug: step over')) -or
                         ($StepOut -and $text.Contains('Debug: step out') -and
                          $text.Contains('Debug: paused | Step | src/main.cpp:14') -and
                          $text.Contains('[NativeAppDebugger] internal single-step observed') -and
                          -not ($text -match 'stale_step_over|invalid_transition')))
        if (-not $stepEvidence) {
            Write-Host 'Captured hosted stepping diagnostics:'
            @($text -split "`r?`n" | Where-Object { $_ -match 'F11|step|Step|STEPPING|PAUSED_STEP|EXCEPTION_SINGLE_STEP|Debug:|debug_state|source_navigation|execution_marker' } | Select-Object -Last 260)
        }
        if ($StepInto) { Assert-True $stepEvidence "F11 performs a real hosted source-level Step Into" }
        elseif ($StepOver) { Assert-True $stepEvidence "F10 performs a real hosted source-level Step Over fallback" }
        else {
            Assert-True $stepEvidence "the real hosted Debug menu performs Step Out to the caller"
            if ($RepeatedStepOut) {
                Assert-True (([regex]::Matches($text, 'Debug: paused \| Step \| src/main.cpp:(14|19)')).Count -ge 2) "a second hosted Step Out reaches the next caller frame"
            }
        }
    }
    Assert-True ($text -match 'target-created.*processId=\d+.*nativeRuntimeId=\d+.*gate=closed') "hosted service publishes exact target identity before release"
    if (-not $DiagnosticOnly -and -not $ContinueBreakpoint) {
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
    if ($StepInto) { Write-Host 'Developer Studio Debugger Phase 5 end-to-end smoke PASS' }
    elseif ($StepOver) { Write-Host 'Developer Studio Debugger Phase 6 end-to-end smoke PASS' }
    elseif ($StepOut) { Write-Host 'Developer Studio Debugger Phase 8 end-to-end smoke PASS' }
    else { Write-Host 'Developer Studio Debugger Phase 3B end-to-end smoke PASS' }
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    if ($process) { $process.Dispose() }
}
