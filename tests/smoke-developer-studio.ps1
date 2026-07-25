[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO",
    [switch]$SkipBuild,
    [switch]$SkipServerBuild
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$ManifestPath = Join-Path $RepoRoot "app\app.json"
$PackageRoot = Join-Path $ServerRoot "Apps\DeveloperStudio"
$PackageManifest = Join-Path $PackageRoot "app.json"
$PackageBinary = Join-Path $PackageRoot "bin\amd64\developerstudio.elf"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Developer Studio smoke failed: $Message" }
    Write-Host "PASS: $Message"
}

function Invoke-Server([string[]]$Commands, [string]$Executable) {
    Push-Location $ServerRoot
    try {
        $output = ($Commands | & $Executable 2>&1 | Out-String)
        return $output
    } finally {
        Pop-Location
    }
}

function Read-ServerLine([Diagnostics.Process]$Process, [System.Collections.Generic.List[string]]$Lines, [scriptblock]$Predicate, [int]$TimeoutSeconds = 15) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        $readTask = $Process.StandardOutput.ReadLineAsync()
        while (-not $readTask.IsCompleted -and [DateTime]::UtcNow -lt $deadline) {
            Start-Sleep -Milliseconds 25
        }
        if (-not $readTask.IsCompleted) { break }
        $line = $readTask.Result
        if ($null -eq $line) { break }
        $Lines.Add($line)
        if (& $Predicate $line $Lines) { return $line }
    }
    throw "Timed out waiting for hosted Server output."
}

function Invoke-InteractiveLaunch([string]$Executable) {
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
    $errorTask = $null
    try {
        Assert-True $process.Start() "hosted runtime starts for interactive launch smoke"
        $errorTask = $process.StandardError.ReadToEndAsync()
        $process.StandardInput.WriteLine("desktop.launch guideXOS Developer Studio")
        Read-ServerLine $process $lines { param($line, $all) $line.Contains("Desktop launch successful: guideXOS Developer Studio") } | Out-Null
        Read-ServerLine $process $lines { param($line, $all) $line.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS") } | Out-Null

        $process.StandardInput.WriteLine("desktop.windows.owners")
        Read-ServerLine $process $lines { param($line, $all) $line -eq "DESKTOP_WINDOW_OWNERS_END" } | Out-Null
        $ownerLine = @($lines | Where-Object { $_ -match 'title=guideXOS Developer Studio' } | Select-Object -Last 1)
        $ownerMatch = [regex]::Match([string]$ownerLine, 'window id=(\d+)')
        Assert-True $ownerMatch.Success "hosted window ownership includes the Developer Studio title"
        $windowId = $ownerMatch.Groups[1].Value

        $process.StandardInput.WriteLine("gui.close $windowId")
        Read-ServerLine $process $lines { param($line, $all) $line.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS") } | Out-Null
        $process.StandardInput.WriteLine("nativeapp.processes")
        Read-ServerLine $process $lines { param($line, $all) $line -match 'appId=com.guidexos.developerstudio.*state=Exited' } | Out-Null
        $process.StandardInput.WriteLine("exit")
        $process.StandardInput.Close()
        $process.WaitForExit()
        return ($lines -join [Environment]::NewLine)
    } finally {
        if ($process -and -not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        if ($errorTask) { $null = $errorTask.Result }
        $process.Dispose()
    }
}

$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
Assert-True ($manifest.id -eq "com.guidexos.developerstudio") "canonical manifest ID is stable"
Assert-True ($manifest.displayName -eq "guideXOS Developer Studio") "manifest display name is correct"
Assert-True ($manifest.kind -eq "NativeElf") "manifest uses the existing NativeElf App Model kind"
Assert-True (@($manifest.supportedArchitectures) -contains "amd64") "manifest advertises only the proven amd64 target"
Assert-True ($manifest.entries[0].entryPoint -eq "gx_main") "manifest entry point uses the guideXOS C ABI"
Assert-True (@($manifest.permissions) -contains "filesystem.read") "manifest grants the existing filesystem read capability"
Assert-True (@($manifest.permissions) -contains "filesystem.write") "manifest grants the existing filesystem write capability"

if (-not $SkipBuild) {
    & (Join-Path $RepoRoot "build.ps1") -ServerRoot $ServerRoot
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio build failed with exit code $LASTEXITCODE" }
}

Assert-True (Test-Path -LiteralPath $PackageManifest -PathType Leaf) "staged manifest exists in the Server app package"
Assert-True (Test-Path -LiteralPath $PackageBinary -PathType Leaf) "staged Native ELF exists in the Server app package"
$packageHash = (Get-FileHash -LiteralPath $PackageBinary -Algorithm SHA256).Hash
Assert-True ($packageHash -match '^[0-9A-F]{64}$') "staged Native ELF has a reportable SHA-256 identity"
Write-Host "Developer Studio packaged ELF SHA256: $packageHash"

if (-not $SkipServerBuild) {
    Push-Location $ServerRoot
    try {
        & cmd.exe /c build-native-experimental.bat
        if ($LASTEXITCODE -ne 0) { throw "Experimental hosted Server build failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
}

$experimentalServer = Join-Path $ServerRoot "guideXOSServer.experimental.exe"
Assert-True (Test-Path -LiteralPath $experimentalServer -PathType Leaf) "experimental hosted runtime exists"

$startupOutput = Invoke-Server @(
    "desktop.apps",
    "desktop.launch.resolve com.guidexos.developerstudio",
    "nativeapp.inspect com.guidexos.developerstudio",
    "desktop.windows.owners",
    "exit"
) $experimentalServer
Assert-True ($startupOutput.Contains("guideXOS Developer Studio [NativeElf] id=com.guidexos.developerstudio")) "Developer Studio is registered in the App Model list"
Assert-True ($startupOutput.Contains("AppModel registry candidate:  displayName=guideXOS Developer Studio id=com.guidexos.developerstudio")) "registration diagnostic identifies the Developer Studio manifest"
Assert-True ($startupOutput.Contains("appId: com.guidexos.developerstudio")) "canonical ID resolves through launch resolution"
Assert-True ($startupOutput.Contains("selectedStrategy: NativeElf") -and $startupOutput.Contains("selectedEntryPath: bin/amd64/developerstudio.elf")) "manifest entry resolves to the amd64 Native ELF path"
Assert-True ($startupOutput.Contains("windowCount=0")) "registration and resolution do not auto-launch Developer Studio"

$launchOutput = Invoke-InteractiveLaunch $experimentalServer
Assert-True ($launchOutput.Contains("Desktop launch successful: guideXOS Developer Studio")) "normal DesktopService dispatch accepts the display name"
Assert-True ($launchOutput.Contains("[LaunchDispatch] source=HostedDesktopService target=guideXOS Developer Studio resolvedType=NativeElfApp appId=com.guidexos.developerstudio")) "normal dispatch emits the canonical NativeElf decision marker"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER application_construction=PASS")) "application construction marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER main_window_creation=PASS")) "main window creation marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS")) "initial render marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER filesystem_api=workspace_extensions")) "workspace filesystem ABI marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS")) "clean close marker is emitted"
Assert-True ($launchOutput.Contains("appId=com.guidexos.developerstudio")) "native process diagnostics identify the canonical app"

Write-Host "Developer Studio hosted App Model smoke PASS"
