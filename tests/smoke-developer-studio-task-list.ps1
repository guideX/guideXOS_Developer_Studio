[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerRoot,
    [Parameter(Mandatory = $true)]
    [string]$AppPackageRoot,
    [string]$EvidenceRoot = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$AppPackageRoot = [IO.Path]::GetFullPath($AppPackageRoot)
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $RepoRoot "out\tools7-hosted-ui-evidence" }
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
$ServerExecutable = Join-Path $ServerRoot "guideXOSServer.experimental.exe"
$PackageElf = Join-Path $AppPackageRoot "bin\amd64\developerstudio.elf"
$EvidenceFile = Join-Path $EvidenceRoot "task-list-hosted-ui.log"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Task List hosted UI test failed: $Message" }
    Write-Host "PASS: $Message"
}

if (-not (Test-Path -LiteralPath $ServerExecutable -PathType Leaf)) {
    throw "The existing hosted Server executable is unavailable: $ServerExecutable"
}
if (-not (Test-Path -LiteralPath $PackageElf -PathType Leaf)) {
    throw "The isolated AMD64 Developer Studio package is unavailable: $PackageElf"
}

New-Item -ItemType Directory -Path $EvidenceRoot -Force | Out-Null
$RunId = [Guid]::NewGuid().ToString("N")
$RuntimeParentRoot = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) "guidexos-tools7-hosted-ui"))
$RuntimeRoot = Join-Path $RuntimeParentRoot ("r-" + $RunId)
$RuntimeRootFull = [IO.Path]::GetFullPath($RuntimeRoot)
$RuntimeParentPrefix = $RuntimeParentRoot.TrimEnd([IO.Path]::DirectorySeparatorChar,
    [IO.Path]::AltDirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not $RuntimeRootFull.StartsWith($RuntimeParentPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Disposable runtime path escaped its dedicated temp directory: $RuntimeRootFull"
}

$script:CapturedLines = New-Object 'System.Collections.Generic.List[string]'
$script:Actions = New-Object 'System.Collections.Generic.List[string]'
$script:Process = $null
$script:StdoutTask = $null
$script:StderrTask = $null
$script:StdoutDone = $false
$script:StderrDone = $false
$script:WindowId = ""
$OriginalTaskFile = @'
#include <cstddef>
int first = 0;
// TODO: first task
/* FIXME: block task
 * TODO: second task */
const char* text = "// TODO inside a string";
auto raw = R"tag(/* FIXME inside a raw string */)tag";
'@
$OtherTaskFile = @'
// FIXME: task from a second source file
int second = 1;
'@

function Receive-AvailableOutput {
    $madeProgress = $true
    while ($madeProgress) {
        $madeProgress = $false
        if (-not $script:StdoutDone -and $script:StdoutTask.IsCompleted) {
            $line = $script:StdoutTask.Result
            if ($null -eq $line) {
                $script:StdoutDone = $true
            } else {
                $script:CapturedLines.Add([string]$line)
                $script:StdoutTask = $script:Process.StandardOutput.ReadLineAsync()
            }
            $madeProgress = $true
        }
        if (-not $script:StderrDone -and $script:StderrTask.IsCompleted) {
            $line = $script:StderrTask.Result
            if ($null -eq $line) {
                $script:StderrDone = $true
            } else {
                $script:CapturedLines.Add([string]$line)
                $script:StderrTask = $script:Process.StandardError.ReadLineAsync()
            }
            $madeProgress = $true
        }
    }
}

function Send-Command([string]$Command) {
    if (-not $script:Process -or $script:Process.HasExited) {
        $exitCode = if ($script:Process) { $script:Process.ExitCode } else { "<not-started>" }
        throw "Cannot send a hosted command because the Server process has exited (exitCode=$exitCode): $Command"
    }
    Receive-AvailableOutput
    $script:Actions.Add("COMMAND $Command")
    $script:Process.StandardInput.WriteLine($Command)
    $script:Process.StandardInput.Flush()
    Start-Sleep -Milliseconds 2
    Receive-AvailableOutput
}

function Read-Until([scriptblock]$Predicate, [int]$AfterIndex = 0,
                    [int]$TimeoutSeconds = 30) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    $nextIndex = $AfterIndex
    while ([DateTime]::UtcNow -lt $deadline) {
        Receive-AvailableOutput
        while ($nextIndex -lt $script:CapturedLines.Count) {
            $line = [string]$script:CapturedLines[$nextIndex]
            ++$nextIndex
            if (& $Predicate $line $script:CapturedLines) { return $line }
        }
        if ($script:StdoutDone -and $script:StderrDone) { break }
        Start-Sleep -Milliseconds 20
    }
    $lastLine = if ($script:CapturedLines.Count -gt 0) {
        $script:CapturedLines[$script:CapturedLines.Count - 1]
    } else { "<none>" }
    Write-Host "Captured hosted Server output before timeout (lines=$($script:CapturedLines.Count)):"
    @($script:CapturedLines | Select-Object -Last 60) | ForEach-Object { Write-Host $_ }
    throw "Timed out waiting for hosted Server evidence. exited=$($script:Process.HasExited) lastLine=$lastLine"
}

function Wait-Marker([string]$Pattern, [int]$AfterIndex = 0,
                     [int]$TimeoutSeconds = 30) {
    Read-Until { param($line, $all) $line -match $Pattern } $AfterIndex $TimeoutSeconds | Out-Null
    Write-Host "PASS: observed hosted marker $Pattern"
}

function Send-Key([int]$KeyCode, [int]$Modifiers = 0) {
    Send-Command ("gui.keyto {0} {1} down {2}" -f $script:WindowId, $KeyCode, $Modifiers)
    Start-Sleep -Milliseconds 6
    Send-Command ("gui.keyto {0} {1} up {2}" -f $script:WindowId, $KeyCode, $Modifiers)
    Start-Sleep -Milliseconds 8
}

function Send-PathAsKeys([string]$Path) {
    foreach ($character in $Path.ToCharArray()) {
        $keyCode = 0
        $modifiers = 0
        if ($character -match "[A-Za-z]") {
            $keyCode = [int][char]([string]$character).ToUpperInvariant()
        } elseif ($character -match "[0-9]") {
            $keyCode = [int][char]$character
        } elseif ($character -eq [char]92) {
            $keyCode = 220
        } elseif ($character -eq ':') {
            $keyCode = 186
            $modifiers = 1
        } elseif ($character -eq '_') {
            $keyCode = 189
            $modifiers = 1
        } elseif ($character -eq '-') {
            $keyCode = 189
        } elseif ($character -eq '.') {
            $keyCode = 190
        } elseif ($character -eq ' ') {
            $keyCode = 32
        } else {
            throw "The hosted project path contains a character that this UI driver cannot type: '$character'"
        }
        Send-Key $keyCode $modifiers
    }
}

function Send-MouseClick([int]$X, [int]$Y) {
    Send-Command ("gui.mouse {0} {1} {2} 1 down" -f $script:WindowId, $X, $Y)
    Start-Sleep -Milliseconds 30
    Send-Command ("gui.mouse {0} {1} {2} 1 up" -f $script:WindowId, $X, $Y)
    Start-Sleep -Milliseconds 100
}

function Send-MouseClickFast([int]$X, [int]$Y) {
    Send-Command ("gui.mouse {0} {1} {2} 1 down" -f $script:WindowId, $X, $Y)
    Send-Command ("gui.mouse {0} {1} {2} 1 up" -f $script:WindowId, $X, $Y)
}

function Open-TaskListFromToolsMenu([switch]$ExpectVisible) {
    $start = $script:CapturedLines.Count
    Send-MouseClick 620 25
    Send-MouseClick 620 150
    Wait-Marker 'task_list_open=PASS source=tools_menu' $start
    if ($ExpectVisible) { Wait-Marker 'task_list_panel_visible=PASS' $start }
}

function Open-ProjectThroughUi([string]$ProjectRoot) {
    $start = $script:CapturedLines.Count
    Send-Key 79 3 # Ctrl+Shift+O opens the existing Open Project path prompt.
    Read-Until { param($line, $all) $line -match 'draw_text .*pos=210,220 text="Open Project"' } $start 10 | Out-Null
    Write-Host "PASS: the real Open Project path prompt is visible"
    Send-PathAsKeys $ProjectRoot
    Send-Key 13 0
    $result = Read-Until { param($line, $all) $line -match 'project_open=(PASS|FAIL)' } $start 45
    if ($result -notmatch 'project_open=PASS') {
        throw "The hosted app rejected the disposable project fixture: $result"
    }
    Write-Host "PASS: observed hosted marker $($Matches[0])"
}

function Get-FixtureHash([string]$Root) {
    $entries = New-Object 'System.Collections.Generic.List[string]'
    foreach ($file in (Get-ChildItem -LiteralPath $Root -File -Recurse | Sort-Object FullName)) {
        $relative = $file.FullName.Substring($Root.Length).TrimStart('\', '/')
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
        $entries.Add("$relative=$hash")
    }
    return [string]::Join("`n", $entries.ToArray())
}

function Write-FixtureText([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

function New-ProjectFixture([string]$Path, [string]$ProjectId,
                            [string]$DisplayName, [string]$MainSource) {
    New-Item -ItemType Directory -Path (Join-Path $Path "src") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $Path "app") -Force | Out-Null
    $project = [ordered]@{
        formatVersion = 1
        projectId = $ProjectId
        displayName = $DisplayName
        projectKind = "native-gui-application"
        sourceRoot = "src"
        applicationManifest = "app/app.json"
        defaultTargetProfile = "guidexos.amd64.hosted.native"
        entryPoint = "gx_main"
        abi = "guidexos-c-abi-v1"
        architecture = "amd64"
        outputName = "task-list-ui"
    } | ConvertTo-Json -Depth 4
    $manifest = [ordered]@{
        schemaVersion = 1
        id = $ProjectId
        displayName = $DisplayName
        version = "0.1.0"
        publisher = "Developer Studio hosted tests"
        description = "Disposable Task List UI qualification fixture."
        category = "Development"
        kind = "NativeElf"
        icon = ""
        minGuideXOSVersion = "0.5.0"
        supportedArchitectures = @("amd64")
        entries = @(@{
            architecture = "amd64"
            path = "bin/amd64/task-list-ui.elf"
            entryPoint = "gx_main"
            abi = "guidexos-c-abi-v1"
            runtime = "native-elf"
        })
        permissions = @("log", "window", "draw")
        fileAssociations = @()
        defaultWindow = @{ width = 640; height = 360; resizable = $true }
    } | ConvertTo-Json -Depth 8
    Write-FixtureText (Join-Path $Path "guidexos.project") $project
    Write-FixtureText (Join-Path $Path "app\app.json") $manifest
    Write-FixtureText (Join-Path $Path "CMakeLists.txt") "cmake_minimum_required(VERSION 3.20)`nproject($DisplayName LANGUAGES C CXX)`n"
    Write-FixtureText (Join-Path $Path "build.ps1") "param()`nWrite-Output 'Hosted UI fixture only'`n"
    Write-FixtureText (Join-Path $Path "README.md") "# $DisplayName`n`nDisposable hosted UI qualification fixture.`n"
    Write-FixtureText (Join-Path $Path "src\main.cpp") $MainSource
}

$success = $false
try {
    New-Item -ItemType Directory -Path $RuntimeParentRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $RuntimeRoot -Force | Out-Null
    $runtimeApps = Join-Path $RuntimeRoot "Apps"
    $runtimePackage = Join-Path $runtimeApps "DeveloperStudio"
    New-Item -ItemType Directory -Path $runtimePackage -Force | Out-Null
    foreach ($item in (Get-ChildItem -LiteralPath $AppPackageRoot -Force)) {
        Copy-Item -LiteralPath $item.FullName -Destination $runtimePackage -Recurse -Force
    }
    New-Item -ItemType Directory -Path (Join-Path $RuntimeRoot "sdk\samples") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $RuntimeRoot "examples\apps") -Force | Out-Null

    $fixtureRoot = Join-Path $RuntimeRoot "TaskListFixture"
    $emptyProject = Join-Path $RuntimeRoot "EmptyProject"
    New-ProjectFixture $fixtureRoot "com.example.tools7-task-list" "TOOLS7 Task List Fixture" "int main() { return 0; }`n"
    New-ProjectFixture $emptyProject "com.example.tools7-empty" "TOOLS7 Empty Fixture" "// ordinary comment`nint empty_project = 1;`n"
    Write-FixtureText (Join-Path $emptyProject "src\empty.cpp") ""
    Write-FixtureText (Join-Path $fixtureRoot "src\00_tasks.cpp") $OriginalTaskFile
    Write-FixtureText (Join-Path $fixtureRoot "src\01_other.c") $OtherTaskFile
    Write-FixtureText (Join-Path $fixtureRoot "src\02_malformed.cpp") "/* TODO: unterminated block comment"
    for ($index = 0; $index -lt 18; ++$index) {
        $name = "plain-{0:D3}.cpp" -f $index
        Write-FixtureText (Join-Path $fixtureRoot ("src\" + $name)) ("int plain_{0:D3} = {0};`n" -f $index)
    }

    $startInfo = New-Object Diagnostics.ProcessStartInfo
    $startInfo.FileName = $ServerExecutable
    $startInfo.WorkingDirectory = $RuntimeRoot
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardInput = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $script:Process = New-Object Diagnostics.Process
    $script:Process.StartInfo = $startInfo
    Assert-True $script:Process.Start() "the actual experimental hosted Server starts from an isolated runtime root"
    $script:StdoutTask = $script:Process.StandardOutput.ReadLineAsync()
    $script:StderrTask = $script:Process.StandardError.ReadLineAsync()

    Send-Command "gui.start"
    Start-Sleep -Seconds 5
    Send-Command "desktop.launch com.guidexos.developerstudio"
    Wait-Marker 'Desktop launch successful: com\.guidexos\.developerstudio' 0 60
    Wait-Marker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS' 0 60

    $ownerStart = $script:CapturedLines.Count
    Send-Command "desktop.windows.owners"
    Read-Until { param($line, $all) $line -eq "DESKTOP_WINDOW_OWNERS_END" } $ownerStart 30 | Out-Null
    $ownerLines = @($script:CapturedLines | Select-Object -Skip $ownerStart)
    $ownerLine = @($ownerLines | Where-Object { $_ -match 'title=guideXOS Developer Studio' } | Select-Object -Last 1)
    $ownerMatch = [regex]::Match([string]$ownerLine, 'window id=(\d+)')
    Assert-True $ownerMatch.Success "the hosted Server reports the real Developer Studio window owner"
    $script:WindowId = $ownerMatch.Groups[1].Value
    $script:Actions.Add("TARGET_WINDOW $($script:WindowId)")

    # A real Tools menu click opens Task List even when no project is active.
    Open-TaskListFromToolsMenu -ExpectVisible
    $noProjectScanStart = $script:CapturedLines.Count
    Send-MouseClick 796 97
    Wait-Marker 'task_list_scan=REJECTED reason=no_project' $noProjectScanStart
    $closeStart = $script:CapturedLines.Count
    Send-MouseClick 876 97
    Wait-Marker 'task_list_close=PASS source=close' $closeStart

    # Check the real keyboard shortcut routes and the Replace safety explanation.
    $shortcutCases = @(
        @{ Code = 70; Modifiers = 3; Marker = 'find_in_files_open=PASS source=keyboard_shortcut'; Name = 'Ctrl+Shift+F' },
        @{ Code = 80; Modifiers = 2; Marker = 'go_to_file_open=PASS source=keyboard_shortcut'; Name = 'Ctrl+P' },
        @{ Code = 71; Modifiers = 2; Marker = 'go_to_line_open=PASS source=keyboard_shortcut'; Name = 'Ctrl+G' },
        @{ Code = 72; Modifiers = 3; Marker = 'replace_in_files_open=PASS source=keyboard_shortcut'; Name = 'Ctrl+Shift+H' }
    )
    foreach ($case in $shortcutCases) {
        $start = $script:CapturedLines.Count
        Send-Key $case.Code $case.Modifiers
        Wait-Marker $case.Marker $start
        Write-Host "PASS: actual shortcut dispatch $($case.Name)"
    }
    $safetyStart = $script:CapturedLines.Count
    Wait-Marker 'replace_preview_apply=DISABLED reason=atomic_replace_and_durable_sync_unavailable' $safetyStart
    $replaceCloseStart = $script:CapturedLines.Count
    Send-Key 27 0
    Wait-Marker 'replace_in_files_close=PASS source=escape' $replaceCloseStart

    # An annotation-free project returns a completed, empty result set.
    Open-ProjectThroughUi $emptyProject
    Open-TaskListFromToolsMenu -ExpectVisible
    $emptyScanStart = $script:CapturedLines.Count
    Send-MouseClick 796 97
    Wait-Marker 'task_list_scan=STARTED' $emptyScanStart
    Wait-Marker 'task_list_scan=COMPLETED todo=0 fixme=0 tasks=0 files=2 truncated=false' $emptyScanStart 45
    $closeStart = $script:CapturedLines.Count
    Send-MouseClick 876 97
    Wait-Marker 'task_list_close=PASS source=close' $closeStart

    # A malformed trailing block comment is still bounded and searchable. The
    # result cap must be reported clearly instead of exposing an oversized list.
    $overflowContent = New-Object 'System.Text.StringBuilder'
    for ($index = 0; $index -le 128; ++$index) {
        [void]$overflowContent.AppendLine("// TODO: bounded result $index")
    }
    Write-FixtureText (Join-Path $emptyProject "src\main.cpp") $overflowContent.ToString()
    Open-TaskListFromToolsMenu -ExpectVisible
    $truncatedScanStart = $script:CapturedLines.Count
    Send-MouseClick 796 97
    Wait-Marker 'task_list_scan=STARTED' $truncatedScanStart
    Wait-Marker 'task_list_scan=COMPLETED todo=32 fixme=0 tasks=32 files=2 truncated=true' $truncatedScanStart 45
    Wait-Marker 'draw_text .*Scan truncated at a resource limit:' $truncatedScanStart 30
    $closeStart = $script:CapturedLines.Count
    Send-MouseClick 876 97
    Wait-Marker 'task_list_close=PASS source=close' $closeStart

    # Switching projects uses the real path prompt and resets the old Task List.
    Open-ProjectThroughUi $fixtureRoot
    Open-TaskListFromToolsMenu -ExpectVisible
    $fixtureHashBefore = Get-FixtureHash $fixtureRoot
    $fixtureScanStart = $script:CapturedLines.Count
    Send-MouseClick 796 97
    Wait-Marker 'task_list_scan=STARTED' $fixtureScanStart
    Wait-Marker 'task_list_scan=COMPLETED todo=3 fixme=2 tasks=5 files=22 truncated=false' $fixtureScanStart 45
    Assert-True ((Get-FixtureHash $fixtureRoot) -eq $fixtureHashBefore) "Task List scanning leaves all project fixture files byte-for-byte unchanged"

    # A file edited after scanning is opened at the recorded line, with no write by navigation.
    $modifiedTaskFile = $OriginalTaskFile.Replace("TODO: first task", "TODO: edited after scan")
    Write-FixtureText (Join-Path $fixtureRoot "src\00_tasks.cpp") $modifiedTaskFile
    Send-MouseClick 100 214
    $navigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    $expectedCaret = [IO.File]::ReadAllText((Join-Path $fixtureRoot "src\00_tasks.cpp")).IndexOf("// TODO: edited after scan", [StringComparison]::Ordinal)
    Wait-Marker ("task_list_navigation=PASS path=src/00_tasks\.cpp line=3 caret=" + $expectedCaret) $navigationStart
    Assert-True ([IO.File]::ReadAllText((Join-Path $fixtureRoot "src\00_tasks.cpp")) -eq $modifiedTaskFile) "opening a task from a changed source file does not rewrite it"
    Write-FixtureText (Join-Path $fixtureRoot "src\00_tasks.cpp") $OriginalTaskFile

    # Activating the same menu command while the panel is open must preserve its state.
    $repeatActivationStart = $script:CapturedLines.Count
    Open-TaskListFromToolsMenu
    $refreshStart = $script:CapturedLines.Count
    Send-MouseClick 796 97
    Wait-Marker 'task_list_scan=STARTED' $refreshStart
    Wait-Marker 'task_list_scan=COMPLETED todo=3 fixme=2 tasks=5 files=22 truncated=false' $refreshStart 45

    # A stale result for a deleted, previously unopened file fails safely.
    for ($step = 0; $step -lt 3; ++$step) { Send-Key 40 0 }
    $deletedPath = Join-Path $fixtureRoot "src\01_other.c"
    Remove-Item -LiteralPath $deletedPath -Force
    $staleNavigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    Wait-Marker 'task_list_navigation=FAILED path=src/01_other\.c line=1' $staleNavigationStart
    Write-FixtureText $deletedPath $OtherTaskFile

    # Cancellation is driven by the visible Cancel button during a bounded scan.
    for ($index = 0; $index -lt 8; ++$index) {
        $name = "cancel-{0:D3}.cpp" -f $index
        Write-FixtureText (Join-Path $fixtureRoot ("src\" + $name)) "int cancel_fixture = 1;`n"
    }
    $cancelStart = $script:CapturedLines.Count
    Send-MouseClickFast 796 97
    Send-MouseClickFast 876 97
    $cancelResult = Read-Until { param($line, $all) $line -match 'task_list_scan=(CANCELLED|COMPLETED|FAILED)' } $cancelStart 45
    if ($cancelResult -notmatch 'task_list_scan=CANCELLED') {
        throw "The Task List scan reached a non-cancelled terminal state after the immediate Cancel click: $cancelResult"
    }
    Write-Host "PASS: observed hosted marker task_list_scan=CANCELLED"
    $closeStart = $script:CapturedLines.Count
    Send-MouseClick 876 97
    Wait-Marker 'task_list_close=PASS source=close' $closeStart
    Get-ChildItem -LiteralPath (Join-Path $fixtureRoot "src") -Filter "cancel-*.cpp" -File |
        Remove-Item -Force

    # Closing the workspace after scanning clears result ownership; no stale task can navigate.
    $menuStart = $script:CapturedLines.Count
    Send-MouseClick 300 25
    Send-MouseClick 300 152
    Read-Until { param($line, $all) $line -match 'text="No workspace open"' } $menuStart 10 | Out-Null
    Write-Host "PASS: the hosted File menu closes the current workspace"
    Start-Sleep -Milliseconds 200
    Open-TaskListFromToolsMenu -ExpectVisible
    $staleSelectionStart = $script:CapturedLines.Count
    Send-Key 13 0
    Wait-Marker 'task_list_navigation=REJECTED reason=no_current_result' $staleSelectionStart
    $closeStart = $script:CapturedLines.Count
    Send-MouseClick 876 97
    Wait-Marker 'task_list_close=PASS source=close' $closeStart

    # Close Developer Studio through the real close path while a bounded scan is active.
    Open-ProjectThroughUi $fixtureRoot
    Open-TaskListFromToolsMenu -ExpectVisible
    for ($index = 0; $index -lt 8; ++$index) {
        $name = "shutdown-{0:D3}.cpp" -f $index
        Write-FixtureText (Join-Path $fixtureRoot ("src\" + $name)) "int shutdown_fixture = 1;`n"
    }
    $closeWhileScanningStart = $script:CapturedLines.Count
    Send-MouseClickFast 796 97
    $serverCloseStart = $script:CapturedLines.Count
    Send-Command ("gui.close {0}" -f $script:WindowId)
    Wait-Marker 'task_list_scan=STARTED' $closeWhileScanningStart
    Wait-Marker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS' $serverCloseStart 60
    Read-Until { param($line, $all) $line.Contains("Native app execution completed: guideXOS Developer Studio") } $serverCloseStart 60 | Out-Null

    Send-Command "exit"
    $script:Process.StandardInput.Close()
    if (-not $script:Process.WaitForExit(20000)) {
        throw "The hosted Server did not exit after the acceptance run."
    }
    Assert-True ($script:Process.ExitCode -eq 0) "the hosted Server exits cleanly after app shutdown"
    $success = $true
    Write-Host "Developer Studio Task List hosted UI integration PASS"
} finally {
    if ($script:Process -and -not $script:Process.HasExited) {
        try { $script:Process.Kill(); $script:Process.WaitForExit() } catch { }
    }
    if ($script:Process) { $script:Process.Dispose() }
    $evidence = New-Object 'System.Collections.Generic.List[string]'
    $evidence.Add("result=" + $(if ($success) { "PASS" } else { "FAIL" }))
    $evidence.Add("serverExecutable=$ServerExecutable")
    $evidence.Add("appPackage=$PackageElf")
    $evidence.Add("runtimeRoot=$RuntimeRootFull")
    $evidence.Add("--- queued hosted interactions ---")
    foreach ($action in $script:Actions) { $evidence.Add($action) }
    $evidence.Add("--- actual Server and app output ---")
    foreach ($line in $script:CapturedLines) { $evidence.Add($line) }
    [IO.File]::WriteAllLines($EvidenceFile, $evidence.ToArray(), [Text.UTF8Encoding]::new($false))
    Write-Host "Hosted UI evidence: $EvidenceFile"
    if (Test-Path -LiteralPath $RuntimeRootFull) {
        $resolvedRuntime = [IO.Path]::GetFullPath($RuntimeRootFull)
        if (-not $resolvedRuntime.StartsWith($RuntimeParentPrefix, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a runtime path outside its dedicated temp directory: $resolvedRuntime"
        }
        Remove-Item -LiteralPath $resolvedRuntime -Recurse -Force
    }
}
