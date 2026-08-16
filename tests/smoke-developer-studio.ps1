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

function Normalize-PackageRelativePath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    $normalized = $Path.Replace('\', '/')
    while ($normalized.StartsWith('./', [StringComparison]::Ordinal)) {
        $normalized = $normalized.Substring(2)
    }
    return $normalized
}

function Assert-ResolverEntryPath([string]$Output) {
    $expectedRelativePath = Normalize-PackageRelativePath 'bin/amd64/developerstudio.elf'
    $resolverMatch = [regex]::Match(
        $Output,
        '(?m)^\[LaunchResolver\]\s+App:\s+com\.guidexos\.developerstudio\s+Strategy:\s+NativeElf\s+Architecture:\s+amd64\s+Entry:\s+(?<entry>\S+)\s+Result:\s+success\b')
    Assert-True $resolverMatch.Success "resolver emits a successful canonical NativeElf decision"

    $actualRelativePath = Normalize-PackageRelativePath $resolverMatch.Groups['entry'].Value
    Assert-True ($actualRelativePath -eq $expectedRelativePath) "resolver selected the expected package-relative ELF identity"

    $packageRootFull = [IO.Path]::GetFullPath($PackageRoot).TrimEnd([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
    $resolvedEntry = [IO.Path]::GetFullPath([IO.Path]::Combine($packageRootFull, $actualRelativePath.Replace('/', [IO.Path]::DirectorySeparatorChar)))
    $packageBinaryFull = [IO.Path]::GetFullPath($PackageBinary)
    $contained = $resolvedEntry.StartsWith($packageRootFull + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)
    Assert-True $contained "resolver entry remains contained by the Developer Studio package"
    Assert-True ($resolvedEntry.Equals($packageBinaryFull, [StringComparison]::OrdinalIgnoreCase)) "resolver entry resolves to the packaged ELF file"
}

function Invoke-Server([string[]]$Commands, [string]$Executable) {
    Push-Location $ServerRoot
    try {
        $pipeline = '(' + (($Commands | ForEach-Object { "echo $_" }) -join ' & ') + ") | $Executable"
        $output = (& cmd.exe /d /s /c $pipeline 2>&1 | Out-String)
        return $output
    } finally {
        Pop-Location
    }
}

function Read-ServerLine([Diagnostics.Process]$Process, [System.Collections.Generic.List[string]]$Lines, [scriptblock]$Predicate, [int]$TimeoutSeconds = 45) {
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ([DateTime]::UtcNow -lt $deadline) {
        if ($script:DeveloperStudioLineTask.IsCompleted) {
            $line = $script:DeveloperStudioLineTask.Result
            if ($null -eq $line) { break }
            $line = [string]$line
            $Lines.Add($line)
            $script:DeveloperStudioLineTask = $Process.StandardOutput.ReadLineAsync()
            if (& $Predicate $line $Lines) { return $line }
            continue
        }
        if ($script:DeveloperStudioErrorLineTask.IsCompleted) {
            $line = $script:DeveloperStudioErrorLineTask.Result
            if ($null -eq $line) { break }
            $line = [string]$line
            $Lines.Add($line)
            $script:DeveloperStudioErrorLineTask = $Process.StandardError.ReadLineAsync()
            if (& $Predicate $line $Lines) { return $line }
            continue
        }
        if ($Process.HasExited) { break }
        Start-Sleep -Milliseconds 25
    }
    # A line can become visible on the other redirected stream at the same
    # boundary as the timeout check. Reconcile the bounded capture before
    # declaring a harness failure; this still requires the real observable
    # event to have been emitted by the hosted runtime.
    for ($index = 0; $index -lt $Lines.Count; ++$index) {
        $line = [string]$Lines[$index]
        if (& $Predicate $line $Lines) { return $line }
    }
    $lastLine = if ($Lines.Count -gt 0) { $Lines[$Lines.Count - 1] } else { "<none>" }
    Write-Host ("Captured hosted App Model output before timeout (lines={0}):" -f $Lines.Count)
    $start = [Math]::Max(0, $Lines.Count - 60)
    for ($index = $start; $index -lt $Lines.Count; ++$index) { Write-Host ("[{0}] {1}" -f $index, $Lines[$index]) }
    throw "Timed out waiting for hosted Server output. processExited=$($Process.HasExited) lastLine=$lastLine"
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
    $script:DeveloperStudioLineTask = $null
    $script:DeveloperStudioErrorLineTask = $null
    try {
        Assert-True $process.Start() "hosted runtime starts for interactive launch smoke"
        $script:DeveloperStudioLineTask = $process.StandardOutput.ReadLineAsync()
        $script:DeveloperStudioErrorLineTask = $process.StandardError.ReadLineAsync()
        $process.StandardInput.WriteLine("gui.start")
        $process.StandardInput.Flush()
        Start-Sleep -Seconds 5
        $process.StandardInput.WriteLine("desktop.launch com.guidexos.developerstudio")
        $process.StandardInput.Flush()
        Read-ServerLine $process $lines { param($line, $all) $line.Contains("Desktop launch successful: com.guidexos.developerstudio") } | Out-Null
        Read-ServerLine $process $lines { param($line, $all) $line.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS") } | Out-Null

        $process.StandardInput.WriteLine("desktop.windows.owners")
        $process.StandardInput.Flush()
        Read-ServerLine $process $lines { param($line, $all) $line -eq "DESKTOP_WINDOW_OWNERS_END" } | Out-Null
        $ownerLine = @($lines | Where-Object { $_ -match 'title=guideXOS Developer Studio' } | Select-Object -Last 1)
        $ownerMatch = [regex]::Match([string]$ownerLine, 'window id=(\d+)')
        Assert-True $ownerMatch.Success "hosted window ownership includes the Developer Studio title"
        $windowId = $ownerMatch.Groups[1].Value

        $process.StandardInput.WriteLine("gui.close $windowId")
        $process.StandardInput.Flush()
        # The clean-close host log is carried in the executor summary on this
        # runtime lane. Wait for the later completion boundary, then assert
        # that the real summary retained the clean-close marker.
        Read-ServerLine $process $lines { param($line, $all) $line.Contains("Native app execution completed: guideXOS Developer Studio") } | Out-Null
        $process.StandardInput.WriteLine("nativeapp.processes")
        $process.StandardInput.Flush()
        Read-ServerLine $process $lines { param($line, $all) $line -match 'appId=com.guidexos.developerstudio.*state=Exited' } | Out-Null
        $process.StandardInput.WriteLine("exit")
        $process.StandardInput.Flush()
        $process.StandardInput.Close()
        $process.WaitForExit()
        return ($lines -join [Environment]::NewLine)
    } finally {
        if ($process -and -not $process.HasExited) {
            $process.Kill()
            $process.WaitForExit()
        }
        if ($script:DeveloperStudioErrorLineTask -and $script:DeveloperStudioErrorLineTask.IsCompleted) { $null = $script:DeveloperStudioErrorLineTask.Result }
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
Assert-ResolverEntryPath $startupOutput
Assert-True ($startupOutput.Contains("windowCount=0")) "registration and resolution do not auto-launch Developer Studio"

$launchOutput = Invoke-InteractiveLaunch $experimentalServer
Assert-True ($launchOutput.Contains("Desktop launch successful: com.guidexos.developerstudio")) "normal DesktopService dispatch accepts the canonical package identity"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER application_construction=PASS")) "application construction marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER main_window_creation=PASS")) "main window creation marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER initial_render=PASS")) "initial render marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER filesystem_api=workspace_extensions")) "workspace filesystem ABI marker is emitted"
Assert-True ($launchOutput.Contains("GUIDEXOS_DEVELOPER_STUDIO_MARKER clean_close=PASS")) "clean close marker is emitted"
Assert-True ($launchOutput.Contains("appId=com.guidexos.developerstudio")) "native process diagnostics identify the canonical app"

Write-Host "Developer Studio hosted App Model smoke PASS"
