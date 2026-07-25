[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO",
    [int]$LaunchCount = 2
)

$ErrorActionPreference = "Stop"
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$Executable = Join-Path $ServerRoot "guideXOSServer.experimental.exe"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Developer Studio repeated-launch smoke failed: $Message" }
    Write-Host "PASS: $Message"
}

if ($LaunchCount -lt 2) { throw "LaunchCount must be at least 2 for ownership validation." }
if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw "Experimental hosted Server is missing: $Executable"
}

$startInfo = New-Object Diagnostics.ProcessStartInfo
$startInfo.FileName = $Executable
$startInfo.WorkingDirectory = $ServerRoot
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardInput = $true
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true
$process = New-Object Diagnostics.Process
$process.StartInfo = $startInfo
$lines = New-Object 'System.Collections.Generic.List[string]'
$windowIds = New-Object 'System.Collections.Generic.List[string]'
$errorTask = $null

function Read-Until([scriptblock]$Predicate, [int]$TimeoutSeconds = 20) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $readTask = $process.StandardOutput.ReadLineAsync()
        while (-not $readTask.IsCompleted -and [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 25
        }
        if (-not $readTask.IsCompleted) { break }
        $line = $readTask.Result
        if ($null -eq $line) { break }
        $lines.Add($line)
        if (& $Predicate $line $lines) { return $line }
    }
    throw "Timed out waiting for hosted Server output."
}

try {
    Assert-True $process.Start() "hosted Server starts"
    $errorTask = $process.StandardError.ReadToEndAsync()

    for ($index = 1; $index -le $LaunchCount; $index++) {
        $process.StandardInput.WriteLine("desktop.launch guideXOS Developer Studio")
        Read-Until { param($line, $all) $line.Contains("Desktop launch successful: guideXOS Developer Studio") } | Out-Null
        Read-Until { param($line, $all) $line.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS") } | Out-Null

        $ownerStart = $lines.Count
        $process.StandardInput.WriteLine("desktop.windows.owners")
        Read-Until { param($line, $all) $line -eq "DESKTOP_WINDOW_OWNERS_END" } | Out-Null
        $ownerLines = @($lines | Select-Object -Skip $ownerStart)
        $ownerLine = @($ownerLines | Where-Object { $_ -match 'title=guideXOS Developer Studio' } | Select-Object -Last 1)
        $ownerMatch = [regex]::Match([string]$ownerLine, 'window id=(\d+)')
        Assert-True $ownerMatch.Success "launch $index publishes the correct window owner"
        $windowId = $ownerMatch.Groups[1].Value
        $windowIds.Add($windowId)

        $process.StandardInput.WriteLine("gui.close $windowId")
        Read-Until { param($line, $all) $line.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS") } | Out-Null

        $windowStart = $lines.Count
        $process.StandardInput.WriteLine("desktop.windows.owners")
        Read-Until { param($line, $all) $line -eq "DESKTOP_WINDOW_OWNERS_END" } | Out-Null
        $windowLines = @($lines | Select-Object -Skip $windowStart)
        Assert-True (@($windowLines | Where-Object { $_ -eq "windowCount=0" }).Count -gt 0) "launch $index leaves zero windows"

        $process.StandardInput.WriteLine("nativeapp.processes")
        Read-Until { param($line, $all) $line -match 'appId=com.guidexos.developerstudio.*state=Exited' } | Out-Null
        Write-Host "PASS: launch $index native process exits and ownership is released"
    }

    Assert-True (($windowIds | Select-Object -Unique).Count -eq $LaunchCount) "window IDs are not reused while stale ownership exists"
    $process.StandardInput.WriteLine("exit")
    $process.StandardInput.Close()
    $process.WaitForExit()
    Assert-True ($process.ExitCode -eq 0) "Server exits cleanly after repeated launches"
    Write-Host "Developer Studio repeated-launch and close-ownership smoke PASS"
}
finally {
    if ($process -and -not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
    }
    if ($errorTask) { $null = $errorTask.Result }
    $process.Dispose()
}
