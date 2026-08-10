[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO",
    [string]$FixtureRoot = "",
    [int]$BreakpointLine = 20,
    [int]$DebugWaitSeconds = 120,
    [int]$MaxRuntimeSeconds = 240,
    [switch]$DiagnosticOnly
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $FixtureRoot) { $FixtureRoot = Join-Path $RepoRoot "tests\fixtures\debugger-phase3b" }
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
$Executable = Join-Path $ServerRoot "guideXOSServer.experimental.exe"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Debugger Phase 3B smoke failed: $Message" }
    Write-Host "PASS: $Message"
}

function Add-ServerLine([System.Collections.Generic.List[string]]$Parts, [string]$Line) {
    $Parts.Add("echo $Line")
}

function Add-ShortDelay([System.Collections.Generic.List[string]]$Parts) {
    # cmd.exe has no portable millisecond sleep; two loopback pings provide a
    # deterministic one-second turn for the hosted UI to consume each key.
    $Parts.Add("ping -n 2 -w 100 127.0.0.1 >nul")
}

function Add-Delay([System.Collections.Generic.List[string]]$Parts, [int]$Seconds) {
    $count = [Math]::Max(1, $Seconds + 1)
    $Parts.Add("ping -n $count 127.0.0.1 >nul")
}

function Add-Key([System.Collections.Generic.List[string]]$Parts, [int]$KeyCode, [int]$Modifiers = 0, [bool]$WaitForUi = $false) {
    Add-ServerLine $Parts "gui.key $KeyCode down $Modifiers"
    if ($WaitForUi) { Add-ShortDelay $Parts }
}

function Get-Key([char]$Character, [int]$Modifiers) {
    $Modifiers = 0
    if (($Character -ge 'a') -and ($Character -le 'z')) {
        return @{ Key = [int][char](([string]$Character).ToUpperInvariant()); Modifiers = 0 }
    }
    if (($Character -ge '0') -and ($Character -le '9')) {
        return @{ Key = [int][char]$Character; Modifiers = 0 }
    }
    if ($Character -eq '\') { return @{ Key = 220; Modifiers = 0 } }
    if ($Character -eq ':') { return @{ Key = 186; Modifiers = 1 } }
    if ($Character -eq '-') { return @{ Key = 189; Modifiers = 0 } }
    if ($Character -eq '_') { return @{ Key = 189; Modifiers = 1 } }
    if ($Character -eq '.') { return @{ Key = 190; Modifiers = 0 } }
    if ($Character -eq ' ') { return @{ Key = 32; Modifiers = 0 } }
    throw "Unsupported fixture path character: $Character"
}

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) "rebuilt experimental hosted Server exists"
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot "guidexos.project") -PathType Leaf) "checked-in Phase 3B fixture exists"

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
Add-Key $parts 79 3 $true
foreach ($character in $FixtureRoot.ToLowerInvariant().ToCharArray()) {
    $key = Get-Key $character 0
    Add-Key $parts $key.Key $key.Modifiers
}
Add-Key $parts 13 0 $true
Add-Delay $parts 10

# Move the real editor caret to src/main.cpp:20 and arm F9.
for ($index = 1; $index -lt $BreakpointLine; ++$index) { Add-Key $parts 40 0 $false }
Add-Key $parts 120 0 $true
Add-Delay $parts 30
Add-ServerLine $parts 'gui.activate 1000'
Add-ShortDelay $parts

# Ctrl+F5 starts the real Developer Studio build -> hosted launch -> bind -> trap path.
    Add-Key $parts 116 2 $true
    Add-ShortDelay $parts
    Add-Delay $parts $DebugWaitSeconds
Add-ServerLine $parts 'nativeapp.processes'
Add-Delay $parts 2
Add-ServerLine $parts 'log'
Add-Delay $parts 2

if ($DiagnosticOnly) {
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 2
} else {
    # Closing the owned window enters the product's existing debug-stop confirmation; S confirms stop.
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 3
    Add-ServerLine $parts 'gui.activate 1000'
    Add-Key $parts 83 0 $true
    Add-Delay $parts 25
    Add-ServerLine $parts 'gui.close 1000'
    Add-Delay $parts 12
}
Add-ServerLine $parts 'exit'

$pipeline = '(' + ($parts -join ' & ') + ") | $Executable"
$startInfo = New-Object Diagnostics.ProcessStartInfo
$startInfo.FileName = 'cmd.exe'
$startInfo.Arguments = '/d /s /c "' + $pipeline + '"'
$startInfo.WorkingDirectory = $ServerRoot
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$process = New-Object Diagnostics.Process
    $process.StartInfo = $startInfo
try {
    Assert-True $process.Start() "delayed hosted UI proof starts"
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
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
        Write-Host 'Development/native launch records:'
        @($text -split "`r?`n" | Where-Object { $_ -match '\[DevelopmentRun\]|\[NativeElf|\[NativeAppDebugger\]|Native app launch failed|Debug launch failed' })
    }
    if ($timedOut) {
        Write-Host 'Captured hosted output before smoke timeout:'
        @($text -split "`r?`n" | Select-Object -Last 80)
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
    Assert-True ($text -match 'target-created.*processId=\d+.*nativeRuntimeId=\d+.*gate=closed') "hosted service publishes exact target identity before release"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_stop=requested')) "the hosted debugger stop is requested through the product close path"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_state=EXITED')) "the hosted debug session exits cleanly"
    Assert-True ($text.Contains('GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS')) "Developer Studio closes cleanly after teardown"
    Assert-True ($process.ExitCode -eq 0) "hosted Server exits cleanly after the debugger proof"
    Write-Host 'Developer Studio Debugger Phase 3B end-to-end smoke PASS'
} finally {
    if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
    if ($process) { $process.Dispose() }
}
