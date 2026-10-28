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
if (-not $EvidenceRoot) { $EvidenceRoot = Join-Path $RepoRoot "out\tools10-hosted-ui-evidence" }
$EvidenceRoot = [IO.Path]::GetFullPath($EvidenceRoot)
$ServerExecutable = Join-Path $ServerRoot "guideXOSServer.experimental.exe"
$PackageElf = Join-Path $AppPackageRoot "bin\amd64\developerstudio.elf"
$EvidenceFile = Join-Path $EvidenceRoot "project-symbols-hosted-ui.log"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Developer Studio hosted UI test failed: $Message" }
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
$RuntimeParentRoot = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetTempPath()) "guidexos-tools10-hosted-ui"))
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
$script:FixtureRoot = ""
$script:MainSource = @'
class Box {
public:
    int get() const { return 7; }
};
int original() { return 0; }
'@
$script:OtherSource = @'
struct Other { int value; };
int second() { return 2; }
'@
$script:StaleSource = @'
int obsolete() { return 3; }
'@

function Receive-AvailableOutput {
    $madeProgress = $true
    while ($madeProgress) {
        $madeProgress = $false
        if (-not $script:StdoutDone -and $script:StdoutTask.IsCompleted) {
            $line = $script:StdoutTask.Result
            if ($null -eq $line) { $script:StdoutDone = $true }
            else {
                $script:CapturedLines.Add([string]$line)
                $script:StdoutTask = $script:Process.StandardOutput.ReadLineAsync()
            }
            $madeProgress = $true
        }
        if (-not $script:StderrDone -and $script:StderrTask.IsCompleted) {
            $line = $script:StderrTask.Result
            if ($null -eq $line) { $script:StderrDone = $true }
            else {
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
        if ($character -match "[A-Za-z]") { $keyCode = [int][char]([string]$character).ToUpperInvariant() }
        elseif ($character -match "[0-9]") { $keyCode = [int][char]$character }
        elseif ($character -eq [char]92) { $keyCode = 220 }
        elseif ($character -eq ':') { $keyCode = 186; $modifiers = 1 }
        elseif ($character -eq '_') { $keyCode = 189; $modifiers = 1 }
        elseif ($character -eq '-') { $keyCode = 189 }
        elseif ($character -eq '.') { $keyCode = 190 }
        elseif ($character -eq ' ') { $keyCode = 32 }
        else { throw "The hosted project path contains an unsupported key: '$character'" }
        Send-Key $keyCode $modifiers
    }
}

function Send-TextAsKeys([string]$Text) {
    foreach ($character in $Text.ToCharArray()) {
        $keyCode = 0
        $modifiers = 0
        if ($character -match "[A-Za-z]") { $keyCode = [int][char]([string]$character).ToUpperInvariant() }
        elseif ($character -match "[0-9]") { $keyCode = [int][char]$character }
        elseif ($character -eq ' ') { $keyCode = 32 }
        elseif ($character -eq '_') { $keyCode = 189; $modifiers = 1 }
        elseif ($character -eq '(') { $keyCode = 57; $modifiers = 1 }
        elseif ($character -eq ')') { $keyCode = 48; $modifiers = 1 }
        elseif ($character -eq '{') { $keyCode = 219; $modifiers = 1 }
        elseif ($character -eq '}') { $keyCode = 221; $modifiers = 1 }
        elseif ($character -eq '.') { $keyCode = 190 }
        else { throw "The hosted source fixture contains an unsupported key: '$character'" }
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

function Write-FixtureText([string]$Path, [string]$Text) {
    [IO.File]::WriteAllText($Path, $Text, [Text.UTF8Encoding]::new($false))
}

function New-ProjectFixture([string]$Path, [string]$ProjectId, [string]$DisplayName) {
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
        outputName = "document-outline-ui"
    } | ConvertTo-Json -Depth 4
    $manifest = [ordered]@{
        schemaVersion = 1
        id = $ProjectId
        displayName = $DisplayName
        version = "0.1.0"
        publisher = "Developer Studio hosted tests"
        description = "Disposable Document Outline UI qualification fixture."
        category = "Development"
        kind = "NativeElf"
        icon = ""
        minGuideXOSVersion = "0.5.0"
        supportedArchitectures = @("amd64")
        entries = @(@{
            architecture = "amd64"
            path = "bin/amd64/document-outline-ui.elf"
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
    Write-FixtureText (Join-Path $Path "src\main.cpp") $script:MainSource
    Write-FixtureText (Join-Path $Path "src\other.cpp") $script:OtherSource
    Write-FixtureText (Join-Path $Path "src\stale.cpp") $script:StaleSource
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

function Open-ProjectThroughUi([string]$ProjectRoot) {
    $start = $script:CapturedLines.Count
    Send-Key 79 3 # Ctrl+Shift+O opens the existing Open Project path prompt.
    Read-Until { param($line, $all) $line -match 'draw_text .*text="Open Project"' } $start 10 | Out-Null
    Send-PathAsKeys $ProjectRoot
    Send-Key 13 0
    $result = Read-Until { param($line, $all) $line -match 'project_open=(PASS|FAIL)' } $start 45
    if ($result -notmatch 'project_open=PASS') { throw "The hosted app rejected the disposable fixture: $result" }
    Write-Host "PASS: the real Server UI opened the disposable project"
}

function Open-DocumentOutlineFromToolsMenu {
    $start = $script:CapturedLines.Count
    Send-MouseClick 620 25
    Send-MouseClick 620 174
    Wait-Marker 'document_outline_open=PASS source=tools_menu' $start
    Wait-Marker 'document_outline_panel_visible=PASS' $start
    return $start
}

function Open-ProjectSymbolsFromToolsMenu {
    $start = $script:CapturedLines.Count
    Send-MouseClick 620 25
    Send-MouseClick 620 198
    Wait-Marker 'project_symbols_open=PASS source=tools_menu' $start
    Wait-Marker 'project_symbols_panel_visible=PASS' $start
    return $start
}

function Set-ProjectSymbolsNameFilter([string]$Value) {
    Send-MouseClick 200 98
    Send-Key 65 2 # Ctrl+A selects the current bounded name filter.
    Send-Key 8 0
    if ($Value) { Send-TextAsKeys $Value }
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

    $script:FixtureRoot = Join-Path $RuntimeRoot "ProjectSymbolsFixture"
    New-ProjectFixture $script:FixtureRoot "com.example.tools10-project-symbols" "TOOLS10 Project Symbols Fixture"
    $mainPath = Join-Path $script:FixtureRoot "src\main.cpp"
    $otherPath = Join-Path $script:FixtureRoot "src\other.cpp"
    $stalePath = Join-Path $script:FixtureRoot "src\stale.cpp"
    $fixtureHashBefore = Get-FixtureHash $script:FixtureRoot

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

    # The command is discoverable in the actual Tools menu without a document.
    $noActiveStart = $script:CapturedLines.Count
    $null = Open-DocumentOutlineFromToolsMenu
    Wait-Marker 'document_outline_scan=REJECTED reason=no_active_document' $noActiveStart
    $closeStart = $script:CapturedLines.Count
    Send-Key 27 0
    Wait-Marker 'document_outline_close=PASS source=escape' $closeStart

    Open-ProjectThroughUi $script:FixtureRoot
    # Use the real Explorer path to open src/main.cpp so this acceptance check
    # does not depend on Go to File's separately built source index.
    Send-MouseClick 100 122
    Send-Key 13 0
    Read-Until { param($line, $all) $line -match 'text="\[TXT\] other\.cpp"' } 0 20 | Out-Null
    Send-MouseClick 100 104
    $mainOpenStart = $script:CapturedLines.Count
    Send-Key 13 0
    Wait-Marker 'document_open=PASS' $mainOpenStart
    Read-Until { param($line, $all) $line -match 'text="Document: main\.cpp' } $mainOpenStart 20 | Out-Null
    Write-Host "PASS: the real Explorer opened the fixture's main.cpp document"

    # Enter a line number through the real Go to Line panel and type a new
    # declaration into the editor buffer. The fixture file on disk is unchanged.
    $goToLineStart = $script:CapturedLines.Count
    Send-Key 71 2 # Ctrl+G opens Go to Line.
    Wait-Marker 'go_to_line_open=PASS source=keyboard_shortcut' $goToLineStart
    Send-Key 53 0 # The fixture has five physical lines.
    Send-Key 13 0
    Wait-Marker 'go_to_line_navigate=PASS' $goToLineStart 20
    Send-Key 35 0
    Send-Key 13 0
    Send-TextAsKeys "int unsavedOnly() {}"

    $outlineStart = Open-DocumentOutlineFromToolsMenu
    Wait-Marker 'document_outline_scan=COMPLETED total=4 shown=4 truncated=false source=activation' $outlineStart 30
    Read-Until { param($line, $all) $line -match 'draw_text .*text="unsavedOnly"' } $outlineStart 20 | Out-Null
    Write-Host "PASS: the visible outline contains a declaration added only to the unsaved editor buffer"

    # Select and navigate to a method, then to the unsaved function, using the
    # actual keyboard dispatch path owned by the visible outline panel.
    Send-Key 40 0
    $methodNavigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    Wait-Marker 'document_outline_navigation=PASS name=get line=3 caret=' $methodNavigationStart
    Send-Key 40 0
    Send-Key 40 0
    $unsavedNavigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    $unsavedNavigation = Read-Until { param($line, $all) $line -match 'document_outline_navigation=PASS name=unsavedOnly line=\d+ caret=\d+' } $unsavedNavigationStart
    $caretMatch = [regex]::Match($unsavedNavigation, 'caret=(\d+)')
    $diskLength = [IO.File]::ReadAllText($mainPath).Length
    Assert-True ($caretMatch.Success -and [int]$caretMatch.Groups[1].Value -gt $diskLength) "outline selection moves the editor caret into unsaved source beyond the on-disk file length"

    # Reactivating the active command rescans and republishes the current buffer.
    $repeatOutlineStart = Open-DocumentOutlineFromToolsMenu
    Wait-Marker 'document_outline_scan=COMPLETED total=4 shown=4 truncated=false source=activation' $repeatOutlineStart 30
    $closeOutlineStart = $script:CapturedLines.Count
    Send-Key 27 0
    Wait-Marker 'document_outline_close=PASS source=escape' $closeOutlineStart

    # Project Symbols scans the active project, sees the dirty main.cpp buffer,
    # and navigates from the active document into the saved other.cpp file.
    $projectSymbolsStart = Open-ProjectSymbolsFromToolsMenu
    Wait-Marker 'project_symbols_scan=COMPLETED files=3 symbols=7 retained=7 skipped=0 truncated=false' $projectSymbolsStart 30
    Set-ProjectSymbolsNameFilter "second"
    Send-Key 13 0 # Move focus from the name field to the result list.
    $secondNavigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    $secondNavigation = Read-Until { param($line, $all) $line -match 'project_symbols_navigation=PASS name=second file=src/other\.cpp line=2 caret=' } $secondNavigationStart
    Read-Until { param($line, $all) $line -match 'text="Document: other\.cpp' } $secondNavigationStart 20 | Out-Null
    Assert-True ($secondNavigation -match 'file=src/other\.cpp line=2') "Project Symbols switches documents and navigates to the known function in the second C++ file"
    Assert-True ((Get-FixtureHash $script:FixtureRoot) -eq $fixtureHashBefore) "project symbol scanning and cross-file navigation do not write fixture files"

    Set-ProjectSymbolsNameFilter ""
    Send-MouseClick 200 126
    Send-Key 65 2
    Send-Key 8 0
    Send-TextAsKeys "other.cpp"
    Read-Until { param($line, $all) $line -match 'draw_text .*text="second"' } $projectSymbolsStart 20 | Out-Null
    Write-Host "PASS: the filename filter narrows project symbols to other.cpp"
    Send-MouseClick 200 126
    Send-Key 65 2
    Send-Key 8 0

    # Invalidate a result for a closed file after scanning. Selection must be
    # rejected, preserve the current editor, and trigger a fresh project scan.
    Set-ProjectSymbolsNameFilter "obsolete"
    Write-FixtureText $stalePath "int replacement() { return 4; }`n"
    $staleNavigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    Send-Key 13 0
    Wait-Marker 'project_symbols_navigation=REJECTED reason=stale_symbol' $staleNavigationStart
    Wait-Marker 'project_symbols_scan=COMPLETED files=3 symbols=7 retained=7 skipped=0 truncated=false' $staleNavigationStart 30
    Read-Until { param($line, $all) $line -match 'text="Document: other\.cpp' } $staleNavigationStart 20 | Out-Null
    Set-ProjectSymbolsNameFilter "replacement"
    Send-Key 13 0
    $replacementNavigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    Wait-Marker 'project_symbols_navigation=PASS name=replacement file=src/stale\.cpp line=1 caret=' $replacementNavigationStart
    Read-Until { param($line, $all) $line -match 'text="Document: stale\.cpp' } $replacementNavigationStart 20 | Out-Null
    Write-Host "PASS: stale symbol selection preserves the prior editor, refreshes results, and finds the replacement declaration"

    # Keep each directory small and the project within the existing per-list
    # bound while making the scan long enough for real UI cancellation.
    $sourceRoot = Join-Path $script:FixtureRoot "src"
    for ($index = 0; $index -lt 16; ++$index) {
        $slowDirectory = Join-Path $sourceRoot ("slow_{0:D3}" -f $index)
        for ($nestedIndex = 0; $nestedIndex -lt 2; ++$nestedIndex) {
            $nestedDirectory = Join-Path $slowDirectory ("deep_{0:D3}" -f $nestedIndex)
            New-Item -ItemType Directory -Path $nestedDirectory -Force | Out-Null
            Write-FixtureText (Join-Path $nestedDirectory "empty.cpp") ""
        }
    }
    $fixtureHashBefore = Get-FixtureHash $script:FixtureRoot
    $cancelStart = $script:CapturedLines.Count
    Send-MouseClick 684 98 # Refresh.
    Wait-Marker 'project_symbols_scan=STARTED source=manual_refresh' $cancelStart 10
    Send-MouseClickFast 766 98 # Cancel while the bounded file walk is active.
    Wait-Marker 'project_symbols_cancel=REQUESTED' $cancelStart 10
    Wait-Marker 'project_symbols_scan=CANCELLED reason=user' $cancelStart 20
    Read-Until { param($line, $all) $line -match 'text="Scan cancelled; partial results retained\.' } $cancelStart 20 | Out-Null
    $cancelCloseStart = $script:CapturedLines.Count
    Send-MouseClick 846 98 # Close the completed scan through its visible control.
    Wait-Marker 'project_symbols_close=PASS source=close' $cancelCloseStart

    # Switch to a second document through the real Explorer and make sure the
    # next outline belongs to that active document only.
    $goToOtherStart = $script:CapturedLines.Count
    Send-MouseClick 100 122
    Send-Key 13 0
    Wait-Marker 'document_open=PASS' $goToOtherStart
    Read-Until { param($line, $all) $line -match 'text="Document: other\.cpp' } $goToOtherStart 20 | Out-Null
    Write-Host "PASS: the real Explorer switched the active editor to other.cpp"
    $otherOutlineStart = Open-DocumentOutlineFromToolsMenu
    Wait-Marker 'document_outline_scan=COMPLETED total=2 shown=2 truncated=false source=activation' $otherOutlineStart 30
    Read-Until { param($line, $all) $line -match 'draw_text .*text="Other"' } $otherOutlineStart 20 | Out-Null
    Write-Host "PASS: switching documents replaces the prior outline with the active file's symbols"
    Send-Key 40 0
    $otherNavigationStart = $script:CapturedLines.Count
    Send-Key 13 0
    Wait-Marker 'document_outline_navigation=PASS name=second' $otherNavigationStart
    $closeOtherOutlineStart = $script:CapturedLines.Count
    Send-Key 27 0
    Wait-Marker 'document_outline_close=PASS source=escape' $closeOtherOutlineStart

    Assert-True ((Get-FixtureHash $script:FixtureRoot) -eq $fixtureHashBefore) "outline scans and navigation leave every fixture file byte-for-byte unchanged"

    $serverCloseStart = $script:CapturedLines.Count
    Send-Command ("gui.close {0}" -f $script:WindowId)
    Read-Until { param($line, $all) $line.Contains('text="Unsaved changes"') } $serverCloseStart 20 | Out-Null
    Send-Command "gui.key 68 down 0"
    Start-Sleep -Milliseconds 20
    Send-Command "gui.key 68 up 0" # Discard the deliberately unsaved editor-buffer declaration.
    Wait-Marker 'unsaved_close=DISCARD' $serverCloseStart
    Wait-Marker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS' $serverCloseStart
    Read-Until { param($line, $all) $line.Contains("Native app execution completed: guideXOS Developer Studio") } $serverCloseStart 60 | Out-Null

    Send-Command "exit"
    $script:Process.StandardInput.Close()
    if (-not $script:Process.WaitForExit(20000)) { throw "The hosted Server did not exit after the acceptance run." }
    Assert-True ($script:Process.ExitCode -eq 0) "the hosted Server exits cleanly after the UI acceptance run"
    $success = $true
    Write-Host "Developer Studio Document Outline and Project Symbols hosted UI integration PASS"
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
