# Shared bounded helpers for the repository-native validation tiers.
# This file is dot-sourced by validation runners; it is not production code.

Set-StrictMode -Version Latest

function New-ValidationArgumentList {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [Parameter(Mandatory = $true)]
        [hashtable]$Parameters
    )

    $arguments = New-Object 'System.Collections.Generic.List[string]'
    $arguments.Add('-NoProfile')
    $arguments.Add('-ExecutionPolicy')
    $arguments.Add('Bypass')
    $arguments.Add('-File')
    $arguments.Add($ScriptPath)
    foreach ($key in $Parameters.Keys) {
        $value = $Parameters[$key]
        if ($null -eq $value -or $value -eq '') { continue }
        if ($value -is [bool]) {
            if ($value) { $arguments.Add("-$key") }
            continue
        }
        $arguments.Add("-$key")
        $arguments.Add([string]$value)
    }
    return $arguments.ToArray()
}

function Invoke-ValidationPowerShell {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,
        [string[]]$ScriptArguments = @(),
        [int]$MaxOutputLines = 400
    )

    $tail = New-Object 'System.Collections.Generic.List[string]'
    $started = Get-Date
    Write-Host "[$Name] START"
    # New-ValidationArgumentList already includes PowerShell's launcher
    # switches and the child -File path; do not prepend a second -File pair.
    # A nonzero child is expected input to this helper: callers must be able
    # to turn it into a bounded failure artifact instead of being interrupted
    # by the caller's Stop preference while the native child is still emitting
    # its diagnostic stream.
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & powershell.exe @ScriptArguments 2>&1 |
            ForEach-Object {
                $line = [string]$_
                Write-Host $line
                if ($tail.Count -ge [Math]::Max(40, $MaxOutputLines)) {
                    $tail.RemoveAt(0)
                }
                $tail.Add($line)
            }
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $elapsed = ((Get-Date) - $started).TotalSeconds
    $result = [pscustomobject]@{
        Name = $Name
        ExitCode = $exitCode
        ElapsedSeconds = [Math]::Round($elapsed, 1)
        OutputTail = @($tail)
    }
    if ($exitCode -eq 0) {
        Write-Host ("[{0}] PASS ({1}s)" -f $Name, $result.ElapsedSeconds)
    } else {
        Write-Host ("[{0}] FAIL exit={1} ({2}s)" -f $Name, $exitCode, $result.ElapsedSeconds)
    }
    return $result
}

function Get-ValidationLifecycleLines {
    param(
        [string[]]$Lines,
        [int]$Maximum = 96
    )

    $patterns = 'debug_session|debug_state|debug_stop|debug_step|debug_binding|debug_transition|debug_shutdown|debug_target|debug_window|shutdownStage=|Native app processes:|Native app debug log:|priority|capture|modal|owner'
    return @($Lines | Where-Object { $_ -match $patterns } | Select-Object -Last $Maximum)
}

function Write-BoundedValidationTrace {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,
        [Parameter(Mandatory = $true)]
        [string[]]$Header,
        [string[]]$TraceFiles = @(),
        [string[]]$OutputTail = @(),
        [int]$MaximumBytes = 65536
    )

    $sourceLines = New-Object 'System.Collections.Generic.List[string]'
    foreach ($traceFile in $TraceFiles) {
        if (Test-Path -LiteralPath $traceFile -PathType Leaf) {
            foreach ($line in @(Get-Content -LiteralPath $traceFile -ErrorAction SilentlyContinue)) {
                if ($sourceLines.Count -ge 256) { $sourceLines.RemoveAt(0) }
                $sourceLines.Add([string]$line)
            }
        }
    }
    $lifecycle = @(Get-ValidationLifecycleLines -Lines @($sourceLines) -Maximum 96)
    $recent = @($OutputTail | Select-Object -Last 80)
    $body = @(
        $Header
        'boundedLifecycleMarkerCount=' + $lifecycle.Count
        'boundedRecentOutputCount=' + $recent.Count
        '--- bounded lifecycle/state trace ---'
        $lifecycle
        '--- bounded recent runner output ---'
        $recent
    ) -join "`r`n"
    if ($body.Length -gt $MaximumBytes) {
        $body = $body.Substring($body.Length - $MaximumBytes)
    }
    $directory = Split-Path -Parent $Path
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Set-Content -LiteralPath $Path -Value $body -Encoding UTF8
    Write-Host "bounded validation trace: $Path"
}

function Rotate-ValidationArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Directory,
        [Parameter(Mandatory = $true)]
        [string]$Filter,
        [int]$Keep = 16
    )

    if (-not (Test-Path -LiteralPath $Directory -PathType Container)) { return }
    $files = @(Get-ChildItem -LiteralPath $Directory -File -Filter $Filter -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending)
    foreach ($file in @($files | Select-Object -Skip ([Math]::Max(0, $Keep)))) {
        Remove-Item -LiteralPath $file.FullName -Force -ErrorAction SilentlyContinue
    }
}
