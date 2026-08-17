[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO",
    [string]$FixtureRoot = "",
    [int]$BreakpointLine = 37,
    [string]$Condition = "counter == 2",
    [int]$DebugWaitSeconds = 90,
    [int]$MaxRuntimeSeconds = 300,
    [int]$UiWaitMilliseconds = 700,
    [switch]$ExpectConditionError,
    [switch]$RuntimeOnly,
    [switch]$UiOnly,
    [switch]$FrameOnly,
    [switch]$TreeOnly,
    [switch]$EditorOnly
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $FixtureRoot) { $FixtureRoot = Join-Path $RepoRoot "tests\fixtures\debugger-phase15" }
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
$Executable = Join-Path $ServerRoot "guideXOSServer.experimental.exe"

function Assert-True([bool]$ConditionValue, [string]$Message) {
    if (-not $ConditionValue) {
        if ($script:Phase15CapturedText) {
            Write-Host 'Phase 15 marker excerpt:'
            @($script:Phase15CapturedText -split "`r?`n" |
                Where-Object { $_ -match 'GUIDEXOS_DEVELOPER_STUDIO_MARKER' } |
                Select-Object -Last 120)
            Write-Host 'Phase 15 diagnostic excerpt:'
            @($script:Phase15CapturedText -split "`r?`n" |
                Where-Object { $_ -match 'Breakpoint condition|condition=|invalid condition|DEBUGGER FOUNDATION|Breakpoints|Call Stack|Locals|Arguments|Watch|Frame #|Selected frame|outerValue|rect|values|rectPtr|node|cycle|Key queued|Mouse queued|gui.mouse|debug_' } |
                Select-Object -Last 160)
            Write-Host 'Phase 15 value-tree excerpt:'
            @($script:Phase15CapturedText -split "`r?`n" |
                Where-Object { $_ -match 'draw_text windowId=1000 .*text="(0x|nullptr|<cycle>|\[-\] \*|\[\+\] \*|rectPtr)' } |
                Select-Object -Last 80)
        }
        throw "Debugger Phase 15 smoke failed: $Message"
    }
    Write-Host "PASS: $Message"
}

function Add-ServerLine([System.Collections.Generic.List[string]]$Parts, [string]$Line) {
    $Parts.Add("COMMAND|$Line")
}

function Add-ShortDelay([System.Collections.Generic.List[string]]$Parts) {
    # Keep the hosted proof responsive without making a long path such as a
    # full source-file open or condition entry take several minutes.  Longer
    # state transitions still use Add-Delay explicitly below.
    $Parts.Add("WAITMS|$([Math]::Max(100, $script:Phase15UiWaitMilliseconds))")
}

function Add-Delay([System.Collections.Generic.List[string]]$Parts, [int]$Seconds) {
    $Parts.Add("WAIT|$([Math]::Max(1, $Seconds))")
}

function Add-Key([System.Collections.Generic.List[string]]$Parts, [int]$KeyCode, [int]$Modifiers = 0, [bool]$WaitForUi = $false) {
    Add-ServerLine $Parts "gui.key $KeyCode down $Modifiers"
    if ($WaitForUi) { Add-ShortDelay $Parts }
}

function Add-Mouse([System.Collections.Generic.List[string]]$Parts, [int]$X, [int]$Y, [string]$Action, [bool]$WaitForUi = $false) {
    Add-ServerLine $Parts "gui.mouse 1000 $X $Y 1 $Action"
    if ($WaitForUi) { Add-ShortDelay $Parts }
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
    if ($Character -eq ':') { return @{ Key = 186; Modifiers = 1 } }
    if ($Character -eq '\') { return @{ Key = 220; Modifiers = 0 } }
    if ($Character -eq ' ') { return @{ Key = 32; Modifiers = 0 } }
    if ($Character -eq '=') { return @{ Key = 187; Modifiers = 0 } }
    if ($Character -eq '_') { return @{ Key = 189; Modifiers = 1 } }
    if ($Character -eq '-') { return @{ Key = 189; Modifiers = 0 } }
    throw "Unsupported Phase 15 smoke character: $Character"
}

function Add-Text([System.Collections.Generic.List[string]]$Parts, [string]$Value) {
    foreach ($character in $Value.ToCharArray()) {
        $key = Get-Key $character
        Add-Key $Parts $key.Key $key.Modifiers $false
        Add-ShortDelay $Parts
    }
}

function Add-Click([System.Collections.Generic.List[string]]$Parts, [int]$X, [int]$Y) {
    Add-Mouse $Parts $X $Y 'down' $false
    Add-Mouse $Parts $X $Y 'up' $true
}

function Add-DoubleClick([System.Collections.Generic.List[string]]$Parts, [int]$X, [int]$Y) {
    Add-Mouse $Parts $X $Y 'double' $true
}

function Add-ConditionEdit([System.Collections.Generic.List[string]]$Parts, [string]$Value, [int]$OldLength) {
    Add-Key $Parts 67 0 $true
    Add-ShortDelay $Parts
    Add-Key $Parts 67 0 $true
    Add-ShortDelay $Parts
    Add-Key $Parts 65 2 $true
    Add-Text $Parts $Value
    Add-ShortDelay $Parts
    Add-Key $Parts 13 0 $true
    Add-Delay $Parts 4
}

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) "rebuilt experimental hosted Server exists"
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot "guidexos.project") -PathType Leaf) "Phase 15 fixture project exists"
Assert-True ($Condition.Length -le 240) "condition input remains within the UI bound"
$script:Phase15UiWaitMilliseconds = $UiWaitMilliseconds

$parts = New-Object 'System.Collections.Generic.List[string]'
Add-ServerLine $parts 'gui.start'
Add-Delay $parts 8
Add-ServerLine $parts 'desktop.launch com.guidexos.developerstudio'
Add-Delay $parts 12
Add-ServerLine $parts 'gui.activate 1000'
Add-ShortDelay $parts
Add-Key $parts 79 3 $true
foreach ($character in $FixtureRoot.ToLowerInvariant().ToCharArray()) {
    $key = Get-Key $character
    Add-Key $parts $key.Key $key.Modifiers $false
}
Add-Key $parts 13 0 $true
Add-Delay $parts 10
Add-Key $parts 83 3 $true
Add-Delay $parts 2
for ($index = 1; $index -lt $BreakpointLine; ++$index) {
    Add-Key $parts 40 0 $false
    Add-ShortDelay $parts
}
Add-ServerLine $parts 'gui.activate 1000'
Add-ShortDelay $parts
Add-Key $parts 120 0 $true
$initialDebugWait = 30
if ($FrameOnly) { $initialDebugWait = 8 }
Add-Delay $parts $initialDebugWait
Add-ServerLine $parts 'gui.activate 1000'
Add-ShortDelay $parts

if (-not $FrameOnly) {
if (-not $UiOnly -and -not $TreeOnly) {
    # Open the real Breakpoints panel through the Debug menu and exercise the
    # condition editor, including invalid text, clear, and enable-state retention.
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Click $parts 610 30
    Add-Click $parts 620 230
    Add-Delay $parts 8
    Add-Click $parts 700 148
    Add-ConditionEdit $parts $Condition 0
    if (-not $ExpectConditionError -and -not $RuntimeOnly -and -not $UiOnly -and -not $FrameOnly) {
        Add-ConditionEdit $parts 'counter = 2' $Condition.Length
        Add-Key $parts 88 0 $true
        Add-Delay $parts 5
        # Re-select the breakpoint row after clearing so the subsequent edit
        # proves stable-ID rebinding through the real panel selection path.
        Add-Click $parts 700 148
        Add-ConditionEdit $parts $Condition 0
        Add-Delay $parts 10
        Add-Key $parts 32 0 $true
        Add-Key $parts 32 0 $true
        Add-Delay $parts 5
    }
    Add-Delay $parts 3
}

if (-not $EditorOnly) {
    Add-Key $parts 116 2 $true
    Add-Delay $parts $DebugWaitSeconds

    if ($UiOnly -and -not $TreeOnly) {
        Add-ServerLine $parts 'gui.activate 1000'
        Add-ShortDelay $parts
        Add-Click $parts 610 30
        Add-Click $parts 620 264
        Add-Delay $parts 3
        Add-ServerLine $parts 'gui.activate 1000'
        Add-ShortDelay $parts
        # Use the real Call Stack keyboard selection path in the full UI
        # proof; the separate frame-only smoke exercises mouse selection.
        Add-Key $parts 40 0 $true
    } elseif (-not $TreeOnly) {
        Add-Click $parts 470 106
        Add-Click $parts 200 180
    }

    if (-not $TreeOnly) {
        # Open Locals after selecting frame 1 so the caller values are
        # refreshed through the real hosted control path.
        Add-Click $parts 600 106
        Add-Delay $parts 2

        # Add a frame-sensitive Watch while frame 1 is selected, then return
        # to frame 0 and refresh it again through the panel tabs.
        Add-Click $parts 840 106
        Add-Key $parts 65 0 $true
        Add-Text $parts 'outerValue'
        Add-Key $parts 13 0 $true
        Add-Delay $parts 2
        Add-Click $parts 470 106
        Add-Click $parts 200 202
        Add-Click $parts 840 106
        Add-Delay $parts 2
    } else {
        Add-ServerLine $parts 'gui.activate 1000'
        Add-ShortDelay $parts
        Add-Click $parts 610 30
        Add-Click $parts 620 230
        Add-Delay $parts 2
        Add-Click $parts 600 106
        Add-Delay $parts 2
    }

    # Locals tree: rect -> origin -> x; values[2]; rectPtr -> *; and the
    # cyclic node. All clicks use disclosure-arrow geometry and real press /
    # release ordering.
    Add-Click $parts 600 106
    Add-Click $parts 104 198
    Add-Click $parts 122 220
    Add-Delay $parts 1
    Add-Click $parts 104 352
    Add-Delay $parts 1
    Add-Click $parts 104 330
    Add-Delay $parts 1
    Add-Click $parts 104 330
    Add-Click $parts 104 198
    Add-Click $parts 104 352
    Add-Click $parts 122 396
    Add-Delay $parts 2
    Add-Click $parts 104 198
    Add-Delay $parts 2
}

if ($EditorOnly) {
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 4
} else {
    Add-ServerLine $parts 'log'
    Add-Delay $parts 2
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 3
    if ($TreeOnly -or $FrameOnly) {
        Add-ServerLine $parts 'exit'
    } else {
        Add-ServerLine $parts 'gui.activate 1000'
        Add-Key $parts 83 0 $true
        Add-Delay $parts 30
        Add-ServerLine $parts 'gui.close 1000'
        Add-Delay $parts 12
    }
}
} else {
    Add-Key $parts 116 2 $true
    Add-Delay $parts $DebugWaitSeconds
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Click $parts 610 30
    Add-Click $parts 620 264
    Add-Delay $parts 8
    Add-ServerLine $parts 'gui.activate 1000'
    Add-ShortDelay $parts
    Add-Click $parts 200 180
    Add-Delay $parts 8
    Add-Click $parts 600 106
    Add-Delay $parts 4
    Add-ServerLine $parts 'log'
    Add-Delay $parts 1
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 3
    Add-ServerLine $parts 'gui.activate 1000'
    Add-Key $parts 83 0 $true
    Add-Delay $parts 8
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 3
}
Add-ServerLine $parts 'exit'

$startInfo = New-Object Diagnostics.ProcessStartInfo
$stdoutPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-phase15-$PID.out")
$stderrPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-phase15-$PID.err")
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
try {
    Assert-True $process.Start() "streamed Phase 15 hosted proof starts"
    foreach ($part in $parts) {
        if ($process.HasExited) { break }
        $separator = $part.IndexOf('|')
        if ($separator -lt 0) { continue }
        $kind = $part.Substring(0, $separator)
        $value = $part.Substring($separator + 1)
        if ($kind -eq 'WAIT') {
            Start-Sleep -Seconds ([Math]::Max(1, [int]$value))
        } elseif ($kind -eq 'WAITMS') {
            Start-Sleep -Milliseconds ([Math]::Max(100, [int]$value))
        } elseif ($kind -eq 'COMMAND') {
            $process.StandardInput.WriteLine($value)
            $process.StandardInput.Flush()
        }
    }
    $process.StandardInput.Close()
    $deadline = (Get-Date).AddSeconds($MaxRuntimeSeconds)
    while (-not $process.HasExited -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    $timedOut = -not $process.HasExited
    if ($timedOut) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        $process.WaitForExit()
    }
    $stdoutText = if (Test-Path -LiteralPath $stdoutPath) { Get-Content -Raw -LiteralPath $stdoutPath } else { '' }
    $stderrText = if (Test-Path -LiteralPath $stderrPath) { Get-Content -Raw -LiteralPath $stderrPath } else { '' }
    $text = $stdoutText + "`n" + $stderrText
    Remove-Item -LiteralPath $stdoutPath,$stderrPath -Force -ErrorAction SilentlyContinue
    $script:Phase15CapturedText = $text
    Assert-True ($text.Contains('Desktop launch successful: com.guidexos.developerstudio')) "Developer Studio launches through the hosted desktop"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS')) "hosted Developer Studio reaches its real initial render"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=PASS')) "Phase 15 fixture opens through Developer Studio"
    Assert-True ($text -match 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_breakpoint(_key|_toggle)?=(PASS|PENDING|MAPPED)') "F9 arms the Phase 15 breakpoint"
    if (-not $UiOnly -and -not $TreeOnly -and -not $FrameOnly) { Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_commit=PASS')) "valid condition entry is accepted by the UI" }
    if (-not $ExpectConditionError -and -not $RuntimeOnly -and -not $UiOnly -and -not $TreeOnly -and -not $FrameOnly) {
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_commit=INVALID')) "invalid condition remains visibly invalid"
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_clear=PASS')) "condition clear is reportable"
        $retainedCount = ([regex]::Matches($text, 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_retained=PASS')).Count
        Assert-True ($retainedCount -ge 2) "disable and re-enable retain the condition"
    }
    if ($FrameOnly) {
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) "frame-only hosted proof reaches a real breakpoint stop"
        Assert-True ($text.Contains('Frames: 4  Selected frame: #1')) "frame-only hosted proof renders and selects a nonzero Call Stack frame"
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_call_stack_mouse=SEEN')) "Call Stack summary receives the real hosted mouse selection event"
        Assert-True ($text.Contains('Frame #1 debugCaller') -and $text.Contains('outerValue')) "selected-frame Locals refresh after hosted frame selection"
    } elseif ($TreeOnly) {
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) "tree-only hosted proof reaches a real breakpoint stop"
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_panel_tab=PASS tab=3')) "tree-only hosted proof opens the real Locals panel"
        Assert-True ($text.Contains('debug_tree_expand=PASS name=rect') -and
                     $text.Contains('debug_tree_expand=PASS name=origin')) "hosted nested aggregate disclosure events expand rect.origin"
        Assert-True ($text -match 'draw_text windowId=1000 .*text="10"') "hosted rect.origin.x expansion returns 10"
        Assert-True ($text.Contains('debug_tree_expand=PASS name=values')) "hosted array disclosure event expands values"
        Assert-True ($text -match 'draw_text windowId=1000 .*text="3"') "hosted values[2] expansion returns 3"
        Assert-True ($text.Contains('debug_tree_expand=PASS name=rectPtr')) "hosted pointer disclosure event expands rectPtr"
        Assert-True ($text -match 'draw_text windowId=1000 .*text="0x') "hosted pointer-backed aggregate expansion is visible"
        Assert-True ($text.Contains('debug_tree_expand=PASS name=node') -and
                     $text.Contains('debug_tree_expand=PASS name=next')) "hosted cyclic-node disclosure events reach node.next"
        Assert-True ($text.Contains('<cycle>')) "hosted cyclic node expansion is bounded"
    } elseif ($UiOnly) {
        if (-not $EditorOnly) {
            Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) "unconditional hosted UI proof reaches a real breakpoint stop"
            Assert-True ($text.Contains('Frame #1 debugCaller') -and $text.Contains('outerValue')) "nonzero Call Stack frame selection refreshes Locals"
            Assert-True ($text.Contains('Selected frame: #1')) "hosted UI visibly selects a nonzero Call Stack frame"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="90"') "selected-frame Watch evaluates caller data"
            Assert-True ($text.Contains('Selected frame: #0') -and $text.Contains('UnknownIdentifier')) "returning to frame zero refreshes the frame-sensitive Watch"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="10"') "hosted rect.origin.x expansion returns 10"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="3"') "hosted values[2] expansion returns 3"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="0x') "hosted pointer-backed aggregate expansion is visible"
            Assert-True ($text.Contains('<cycle>')) "hosted cyclic node expansion is bounded"
        }
    } elseif (-not $ExpectConditionError) {
        if (-not $EditorOnly) {
            Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) "condition true hit surfaces a real hosted breakpoint stop"
            $falseHits = ([regex]::Matches($text, 'Debug: breakpoint condition false; continuing')).Count
            Assert-True ($falseHits -ge 2) "hosted condition filters at least two false hits before the true hit"
            Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_condition_true=PASS') -and
                         $text.Contains('condition=counter == 2')) "breakpoint remains condition-bound at the true hit"
            Assert-True ($text.Contains('Frame #1 debugCaller') -and $text.Contains('outerValue')) "nonzero Call Stack frame selection refreshes Locals"
            Assert-True ($text.Contains('Selected frame: #1')) "hosted UI visibly selects a nonzero Call Stack frame"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="90"') "selected-frame Watch evaluates caller data"
            Assert-True ($text.Contains('Selected frame: #0') -and $text.Contains('UnknownIdentifier')) "returning to frame zero refreshes the frame-sensitive Watch"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="10"') "hosted rect.origin.x expansion returns 10"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="3"') "hosted values[2] expansion returns 3"
            Assert-True ($text -match 'draw_text windowId=1000 .*text="0x') "hosted pointer-backed aggregate expansion is visible"
            Assert-True ($text.Contains('<cycle>')) "hosted cyclic node expansion is bounded"
        }
    } else {
        Assert-True ($text.Contains('Debug: breakpoint condition error') -and $text.Contains('ConditionError')) "hosted condition evaluation error is surfaced"
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=PAUSED_BREAKPOINT')) "condition error leaves the target stopped"
        Assert-True (-not $text.Contains('Debug: breakpoint condition false; continuing')) "condition error does not silently continue"
    }
    if ($timedOut) { throw "Phase 15 hosted smoke timed out after $MaxRuntimeSeconds seconds" }
    if (-not $EditorOnly) {
        Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=EXITED')) "hosted debugger exits through the product close path"
    }
    Assert-True ($process.ExitCode -eq 0) "hosted Server exits cleanly after the Phase 15 proof"
    if ($ExpectConditionError) { Write-Host "Developer Studio Phase 15 hosted condition-error smoke PASS" }
    else { Write-Host "Developer Studio Phase 15 hosted interaction smoke PASS" }
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    Remove-Item -LiteralPath $stdoutPath,$stderrPath -Force -ErrorAction SilentlyContinue
    if ($process) { $process.Dispose() }
}
