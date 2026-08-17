[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerRoot,
    [int]$MaxRuntimeSeconds = 240,
    [int]$DebugWaitSeconds = 35,
    [int]$UiWaitMilliseconds = 700
)

$ErrorActionPreference = 'Stop'
$smoke = Join-Path $PSScriptRoot 'smoke-developer-studio-phase15.ps1'
$args = @(
    '-NoProfile',
    '-ExecutionPolicy', 'Bypass',
    '-File', $smoke,
    '-ServerRoot', $ServerRoot,
    '-WatchOnly',
    '-KeepArtifacts',
    '-MaxRuntimeSeconds', $MaxRuntimeSeconds,
    '-DebugWaitSeconds', $DebugWaitSeconds,
    '-UiWaitMilliseconds', $UiWaitMilliseconds
)

$output = & powershell.exe @args 2>&1
$exitCode = $LASTEXITCODE
$text = $output -join "`n"
if ($exitCode -ne 0) {
    throw "Hosted input delivery regression failed; child exit code $exitCode"
}
$artifactMatch = [regex]::Match($text, 'Hosted smoke artifacts retained: (?<stdout>[^\r\n ]+) /')
$trace = if ($artifactMatch.Success -and (Test-Path -LiteralPath $artifactMatch.Groups['stdout'].Value)) {
    Get-Content -Raw -LiteralPath $artifactMatch.Groups['stdout'].Value
} else { '' }
$text = $text + "`n" + $trace

function Assert-Delivery([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Hosted input delivery regression failed: $Message" }
    Write-Host "PASS: $Message"
}

Assert-Delivery ($text.Contains('debug_panel_mouse_down=SEEN')) 'post-panel mouse input reaches Developer Studio'
Assert-Delivery ($text.Contains('debug_panel_tab=PASS tab=5')) 'post-panel tab click changes the active panel'
Assert-Delivery ($text.Contains('debug_key_a_received tab=5')) 'post-panel keyboard input is observed in the active panel'
Assert-Delivery ($text.Contains('debug_watch_prompt=PASS') -and $text.Contains('debug_watch_add=PASS')) 'post-panel keyboard sequence reaches the Watch handler'
Assert-Delivery ($text.Contains('debug_selected_frame=PASS index=1') -and $text.Contains('debug_selected_frame=PASS index=0')) 'later Call Stack mouse events continue to select and restore frames'
Write-Host 'Hosted Phase 16 input delivery regression PASS'
