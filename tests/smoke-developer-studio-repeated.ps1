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
$script:RepeatedLaunchLineTask = $null

function Read-Until([scriptblock]$Predicate, [int]$TimeoutSeconds = 20) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($script:RepeatedLaunchLineTask.IsCompleted) {
            $line = $script:RepeatedLaunchLineTask.Result
            if ($null -eq $line) { break }
            $lines.Add($line)
            $script:RepeatedLaunchLineTask = $process.StandardOutput.ReadLineAsync()
            if (& $Predicate $line $lines) { return $line }
            continue
        }
        if ($process.HasExited) { break }
        Start-Sleep -Milliseconds 25
    }
    $lastLine = if ($lines.Count -gt 0) { $lines[$lines.Count - 1] } else { "<none>" }
    Write-Host 'Captured repeated-launch output before timeout:'
    @($lines | Select-Object -Last 40)
    throw "Timed out waiting for hosted Server output. processExited=$($process.HasExited) lastLine=$lastLine"
}

function Send-Command([string]$Command) {
    $process.StandardInput.WriteLine($Command)
    $process.StandardInput.Flush()
}

try {
    Assert-True $process.Start() "hosted Server starts"
    $script:RepeatedLaunchLineTask = $process.StandardOutput.ReadLineAsync()
    $errorTask = $process.StandardError.ReadToEndAsync()
    Send-Command "gui.start"
    Start-Sleep -Seconds 5

    for ($index = 1; $index -le $LaunchCount; $index++) {
        Send-Command "desktop.launch com.guidexos.developerstudio"
        Read-Until { param($line, $all) $line.Contains("Desktop launch successful: com.guidexos.developerstudio") } | Out-Null
        Read-Until { param($line, $all) $line.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS") } | Out-Null

        $ownerStart = $lines.Count
        Send-Command "desktop.windows.owners"
        Read-Until {
            param($line, $all)
            $currentOwnership = @($all | Select-Object -Skip $ownerStart)
            $hasOwner = @($currentOwnership | Where-Object { $_ -match 'window id=\d+.*title=guideXOS Developer Studio' }).Count -gt 0
            $hasEnd = @($currentOwnership | Where-Object { $_ -eq "DESKTOP_WINDOW_OWNERS_END" }).Count -gt 0
            return $hasOwner -and $hasEnd
        } | Out-Null
        $ownerLines = @($lines | Select-Object -Skip $ownerStart)
        $ownerLine = @($ownerLines | Where-Object { $_ -match 'title=guideXOS Developer Studio' } | Select-Object -Last 1)
        $ownerMatch = [regex]::Match([string]$ownerLine, 'window id=(\d+)')
        Assert-True $ownerMatch.Success "launch $index publishes the correct window owner"
        $windowId = $ownerMatch.Groups[1].Value
        $windowIds.Add($windowId)

        Send-Command "gui.close $windowId"
        Read-Until { param($line, $all) $line.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS") } | Out-Null

        $windowStart = $lines.Count
        Send-Command "desktop.windows.owners"
        Read-Until {
            param($line, $all)
            $currentOwnership = @($all | Select-Object -Skip $windowStart)
            $hasZero = @($currentOwnership | Where-Object { $_ -eq "windowCount=0" }).Count -gt 0
            $hasEnd = @($currentOwnership | Where-Object { $_ -eq "DESKTOP_WINDOW_OWNERS_END" }).Count -gt 0
            return $hasZero -and $hasEnd
        } | Out-Null
        $windowLines = @($lines | Select-Object -Skip $windowStart)
        Assert-True (@($windowLines | Where-Object { $_ -eq "windowCount=0" }).Count -gt 0) "launch $index leaves zero windows"

        Send-Command "nativeapp.processes"
        Read-Until { param($line, $all) $line -match 'appId=com.guidexos.developerstudio.*state=Exited' } | Out-Null
        Write-Host "PASS: launch $index native process exits and ownership is released"
    }

    Assert-True (($windowIds | Select-Object -Unique).Count -eq $LaunchCount) "window IDs are not reused while stale ownership exists"
    Send-Command "exit"
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
