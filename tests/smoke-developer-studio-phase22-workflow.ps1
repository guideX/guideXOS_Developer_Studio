[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [string]$FixtureRoot = '',
    [int]$MaxRuntimeSeconds = 180,
    [string]$TraceDirectory = '',
    [switch]$Build,
    [switch]$BuildRecovery,
    [switch]$LongSession,
    [switch]$Intelligence
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if (-not $FixtureRoot) { $FixtureRoot = Join-Path $RepoRoot 'tests\fixtures\debugger-phase15' }
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
$Executable = Join-Path $ServerRoot 'guideXOSServer.experimental.exe'
$TemporaryFixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-phase22-{0}" -f $PID)
$StdoutPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-phase22-{0}.out" -f $PID)
$StderrPath = Join-Path ([IO.Path]::GetTempPath()) ("guidexos-phase22-{0}.err" -f $PID)
$script:Parts = New-Object 'System.Collections.Generic.List[string]'
$script:Process = $null
$script:LastInput = ''
$script:MarkerHits = @{}
$script:TextHits = @{}

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Phase 22 workflow smoke failed: $Message" }
    Write-Host "PASS: $Message"
}

function Add-Command([string]$Command) {
    $script:Parts.Add("COMMAND|$Command")
    $script:LastInput = $Command
}

function Add-Wait([int]$Seconds = 1) {
    $script:Parts.Add("WAIT|$([Math]::Max(1, $Seconds))")
}

function Add-WaitMarker([string]$Marker) {
    $script:Parts.Add("WAITMARK|$Marker")
}

function Add-WaitMarkerFresh([string]$Marker) {
    $script:Parts.Add("WAITMARKFRESH|$Marker")
}

function Add-WaitText([string]$TextNeedle) {
    $script:Parts.Add("WAITTEXT|$TextNeedle")
}

function Add-Key([int]$KeyCode, [int]$Modifiers = 0) {
    Add-Command "gui.keyto 1000 $KeyCode down $Modifiers"
}

function Add-Click([int]$X, [int]$Y) {
    Add-Command "gui.mouse 1000 $X $Y 1 down"
    Add-Command "gui.mouse 1000 $X $Y 1 up"
}

function Get-Key([char]$Character) {
    if (($Character -cge 'a') -and ($Character -cle 'z')) {
        return @{ Key = [int][char](([string]$Character).ToUpperInvariant()); Modifiers = 0 }
    }
    if (($Character -ge '0') -and ($Character -le '9')) {
        return @{ Key = [int][char]$Character; Modifiers = 0 }
    }
    switch ($Character) {
        ':' { return @{ Key = 186; Modifiers = 1 } }
        '\' { return @{ Key = 220; Modifiers = 0 } }
        '_' { return @{ Key = 189; Modifiers = 1 } }
        '-' { return @{ Key = 189; Modifiers = 0 } }
        default { throw "Unsupported Phase 22 smoke character: $Character" }
    }
}

function Add-Text([string]$Value) {
    foreach ($character in $Value.ToLowerInvariant().ToCharArray()) {
        $key = Get-Key $character
        Add-Key $key.Key $key.Modifiers
    }
}

function Get-LiveText([switch]$Full) {
    $stdout = if (Test-Path -LiteralPath $StdoutPath) {
        if ($Full) { Get-Content -Raw -LiteralPath $StdoutPath -ErrorAction SilentlyContinue }
        else { (Get-Content -LiteralPath $StdoutPath -Tail 6000 -ErrorAction SilentlyContinue) -join "`n" }
    } else { '' }
    $stderr = if (Test-Path -LiteralPath $StderrPath) {
        if ($Full) { Get-Content -Raw -LiteralPath $StderrPath -ErrorAction SilentlyContinue }
        else { (Get-Content -LiteralPath $StderrPath -Tail 1000 -ErrorAction SilentlyContinue) -join "`n" }
    } else { '' }
    return $stdout + "`n" + $stderr
}

function Count-Marker([string]$Text, [string]$Marker) {
    if (-not $Text -or -not $Marker) { return 0 }
    return ([regex]::Matches($Text, [regex]::Escape($Marker))).Count
}

function Wait-ForMarker([string]$Marker, [bool]$Fresh = $false, [int]$TimeoutSeconds = 60) {
    $before = if ($Fresh) { Count-Marker (Get-LiveText -Full) $Marker } else { 0 }
    $lastFullScan = [DateTime]::MinValue
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $text = Get-LiveText
        $count = Count-Marker $text $Marker
        if ((($Fresh -and $count -le $before) -or (-not $Fresh -and $count -eq 0)) -and
            ([DateTime]::UtcNow - $lastFullScan).TotalSeconds -ge 1) {
            $text = Get-LiveText -Full
            $count = Count-Marker $text $Marker
            $lastFullScan = [DateTime]::UtcNow
        }
        if (($Fresh -and $count -gt $before) -or (-not $Fresh -and $count -gt 0)) {
            if (-not $script:MarkerHits.ContainsKey($Marker)) { $script:MarkerHits[$Marker] = 0 }
            $script:MarkerHits[$Marker]++
            Write-Host "PASS: marker reached $Marker"
            return
        }
        if ($script:Process -and $script:Process.HasExited) { break }
        Start-Sleep -Milliseconds 100
    }
    throw "Timed out waiting for marker '$Marker'. lastInput=$script:LastInput`n$(Get-LiveText | Select-Object -Last 120)"
}

function Wait-ForText([string]$TextNeedle, [int]$TimeoutSeconds = 60) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ((Get-LiveText -Full).Contains($TextNeedle)) {
            if (-not $script:TextHits.ContainsKey($TextNeedle)) { $script:TextHits[$TextNeedle] = 0 }
            $script:TextHits[$TextNeedle]++
            Write-Host "PASS: text reached $TextNeedle"
            return
        }
        if ($script:Process -and $script:Process.HasExited) { break }
        Start-Sleep -Milliseconds 250
    }
    throw "Timed out waiting for text '$TextNeedle'. lastInput=$script:LastInput`n$(Get-LiveText | Select-Object -Last 120)"
}

function Write-Trace([string]$Reason, [string]$Text) {
    if (-not $TraceDirectory) { return }
    $directory = [IO.Path]::GetFullPath($TraceDirectory)
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $body = @(
        'guideXOS Developer Studio Phase 22 workflow trace'
        "reason=$Reason"
        "lastInput=$script:LastInput"
        '--- bounded output ---'
        @($Text -split "`r?`n" | Select-Object -Last 120)
    ) -join "`r`n"
    if ($body.Length -gt 65536) { $body = $body.Substring($body.Length - 65536) }
    Set-Content -LiteralPath (Join-Path $directory 'developer-studio-phase22-workflow-trace.log') -Value $body -Encoding UTF8
}

Assert-True (Test-Path -LiteralPath $Executable -PathType Leaf) 'experimental hosted Server exists'
Assert-True (Test-Path -LiteralPath (Join-Path $FixtureRoot 'guidexos.project') -PathType Leaf) 'workflow fixture project exists'
Assert-True (-not (($Build -and $BuildRecovery) -or
                   ($LongSession -and ($Build -or $BuildRecovery)) -or
                   ($Intelligence -and ($BuildRecovery -or $LongSession)))) 'workflow mode is unambiguous'
Copy-Item -LiteralPath $FixtureRoot -Destination $TemporaryFixtureRoot -Recurse -Force
$TemporaryFixtureRoot = [IO.Path]::GetFullPath($TemporaryFixtureRoot)

# Open a project, prove save and focus handoff after the output panel owns the
# keyboard, then exercise the visible Save and File menu controls. Every
# state assertion is synchronized by the product's existing semantic marker.
Add-Command 'gui.start'
Add-Wait 8
Add-Command 'desktop.launch com.guidexos.developerstudio'
Add-WaitMarker 'Desktop launch successful: com.guidexos.developerstudio'
Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS'
Add-Command 'gui.activate 1000'
Add-Click 300 180
Add-Key 79 3
Add-Text $TemporaryFixtureRoot
Add-Key 13
Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER project_open=PASS'
Add-Command 'gui.activate 1000'
Add-Click 300 180
Add-Key 83 3
Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS all=TRUE'

if ($Intelligence) {
    # Use the checked-in Phase 15 C++ fixture as a real project-source
    # intelligence target: Rectangle is a named type, and rect.origin is a
    # type-aware member-access context.
    Add-Click 400 214
    Add-Key 84 6
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info=PASS'
    Add-Key 27
    Add-Click 392 486
    Add-Key 32 2
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_begin=PASS'
    Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_context=MEMBER_LEXICAL'
}

if ($Build -or $BuildRecovery) {
    if ($BuildRecovery) {
        # Inject and remove the same character through the active editor caret;
        # this avoids relying on document-end navigation semantics.
        Add-Key 50 1
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_dirty=TRUE'
        Add-Key 83 2
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS'
        Add-Key 66 3
        Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER build_start=PASS'
        Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER build_complete=FAILED'
        Add-WaitText '[Build] Error: Build Failed'
        # The inserted character is at the start of the newly opened
        # document. Refocus the text area and delete at that exact location,
        # independent of output-panel focus after a failed build.
        Add-Click 320 86
        Add-Key 46
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_dirty=TRUE'
        Add-Key 83 2
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS'
    }
    Add-Key 66 3
    Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER build_start=PASS'
    Add-WaitText '[Build] Success: Build Succeeded'
}

if ($LongSession) {
    # Keep one Developer Studio process alive while repeating the existing
    # build -> debug -> stop -> edit -> save sequence. The Debug menu Stop
    # row is used so this proves normal session teardown rather than closing
    # and relaunching the application between iterations.
    for ($iteration = 1; $iteration -le 3; ++$iteration) {
        Add-Key 66 3
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER build_start=PASS'
        Add-WaitText '[Build] Success: Build Succeeded'
        Add-Click 300 180
        Add-Wait 3
        Add-Key 116 2
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'
        Add-Command 'gui.activate 1000'
        Add-Click 610 30
        Add-Wait 1
        # The menu row hitbox is above the Stop label baseline at y=196.
        Add-Click 620 185
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_stop=requested'
        Add-Wait 8
        Add-Click 300 180
        Add-Key 32
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_dirty=TRUE'
        Add-Key 83 2
        Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS'
    }
}

# Output click -> editor click -> text entry is the generalized focus
# regression. The dirty marker proves the character reached the document,
# rather than being swallowed by the output panel.
Add-Click 200 600
Add-Click 300 180
Add-Key 32
Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_dirty=TRUE'
Add-Key 83 2
Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS'

# These coordinates are the labels rendered by drawShell. The File menu's
# Close Document item proves both the visible File hitbox and its aligned menu
# panel; the Save click proves the visible toolbar hitbox.
Add-Click 360 30
Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS'
Add-Click 300 30
Add-Click 320 130
Add-WaitMarkerFresh 'GUIDEXOS_DEVELOPER_STUDIO_MARKER document_close=PASS'
Add-Command 'gui.close 1000'
Add-WaitMarker 'GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS'
Add-Command 'exit'

$startInfo = New-Object Diagnostics.ProcessStartInfo
$startInfo.FileName = $env:ComSpec
$startInfo.Arguments = "/d /s /c `"`"$Executable`" 1>`"$StdoutPath`" 2>`"$StderrPath`"`""
$startInfo.WorkingDirectory = $ServerRoot
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $true
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $false
$startInfo.RedirectStandardError = $false
$script:Process = New-Object Diagnostics.Process
$script:Process.StartInfo = $startInfo
$succeeded = $false

try {
    Assert-True $script:Process.Start() 'hosted Server starts'
    foreach ($part in $script:Parts) {
        if ($script:Process.HasExited) { throw "Server exited before command stream completed: $script:LastInput" }
        $separator = $part.IndexOf('|')
        $kind = $part.Substring(0, $separator)
        $value = $part.Substring($separator + 1)
        if ($kind -eq 'WAIT') { Start-Sleep -Seconds ([Math]::Max(1, [int]$value)) }
        elseif ($kind -eq 'WAITMARK') { Wait-ForMarker $value $false }
        elseif ($kind -eq 'WAITMARKFRESH') { Wait-ForMarker $value $true }
        elseif ($kind -eq 'WAITTEXT') { Wait-ForText $value }
        elseif ($kind -eq 'COMMAND') {
            $script:Process.StandardInput.WriteLine($value)
            $script:Process.StandardInput.Flush()
        }
    }
    $deadline = [DateTime]::UtcNow.AddSeconds($MaxRuntimeSeconds)
    while (-not $script:Process.HasExited -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (-not $script:Process.HasExited) { throw "hosted Server did not exit within $MaxRuntimeSeconds seconds" }
    $script:Process.WaitForExit()
    $text = Get-LiveText
    Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER document_dirty=TRUE'] -ge 1) 'editor receives text after output-to-editor focus handoff'
    Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS all=TRUE'] -ge 1 -and
                 $script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER document_save=PASS'] -ge 2) 'Ctrl+Shift+S, Ctrl+S, and visible Save all save the active document'
    Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER document_close=PASS'] -ge 1) 'visible File menu closes the active document'
    if ($Build -or $BuildRecovery) {
        Assert-True ($script:TextHits['[Build] Success: Build Succeeded'] -ge 1) 'build completes successfully from the IDE'
    }
    if ($LongSession) {
        Assert-True ($script:TextHits['[Build] Success: Build Succeeded'] -ge 3) 'long session completes three IDE builds'
        Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_start=PASS'] -ge 3) 'long session starts three debugger sessions'
        Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_stop=requested'] -ge 3) 'long session stops three debugger sessions'
    }
    if ($BuildRecovery) {
        Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER build_complete=FAILED'] -ge 1) 'intentional compile failure is reported'
        Assert-True ($script:TextHits['[Build] Error: Build Failed'] -ge 1) 'build failure text is visible in output'
    }
    if ($Intelligence) {
        Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER type_info=PASS'] -ge 1) 'Quick Type Info resolves a real project-source type'
        Assert-True ($script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_begin=PASS'] -ge 1 -and
                     $script:MarkerHits['GUIDEXOS_DEVELOPER_STUDIO_MARKER completion_context=MEMBER_LEXICAL'] -ge 1) 'member completion opens in a real project-source member context'
    }
    Assert-True ($script:Process.ExitCode -eq 0) 'hosted Server exits cleanly after workflow smoke'
    Write-Host 'Developer Studio Phase 22 focused workflow smoke PASS'
    $succeeded = $true
} finally {
    if (-not $succeeded) { Write-Trace 'Phase 22 workflow smoke failure' (Get-LiveText) }
    if ($script:Process -and -not $script:Process.HasExited) { $script:Process.Kill(); $script:Process.WaitForExit() }
    if ($script:Process) { $script:Process.Dispose() }
    Remove-Item -LiteralPath $StdoutPath,$StderrPath -Force -ErrorAction SilentlyContinue
    if (Test-Path -LiteralPath $TemporaryFixtureRoot) { Remove-Item -LiteralPath $TemporaryFixtureRoot -Recurse -Force -ErrorAction SilentlyContinue }
}
