[CmdletBinding()]
param(
    [string]$ServerRoot = 'D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO',
    [Parameter(Mandatory = $true)]
    [string]$FixtureRoot,
    [ValidateRange(1, 25)]
    [int]$Iterations = 5,
    [ValidateRange(1, 10000)]
    [int]$StartIteration = 1,
    [ValidateRange(0, 255)]
    [int]$ExpectedTargetExitCode = 7,
    [ValidateRange(30, 600)]
    [int]$MaxRuntimeSeconds = 240,
    [ValidateRange(10, 300)]
    [int]$DebugWaitSeconds = 120,
    [string]$TraceDirectory = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
. (Join-Path $PSScriptRoot 'DeveloperStudioValidation.Common.ps1')

$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$FixtureSourceRoot = [IO.Path]::GetFullPath($FixtureRoot)
if (-not $TraceDirectory) {
    $TraceDirectory = Join-Path ([IO.Path]::GetTempPath()) ('guidexos-phase29t-hosted-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [Guid]::NewGuid().ToString('N').Substring(0, 8))
}
$TraceDirectory = [IO.Path]::GetFullPath($TraceDirectory)
if ($StartIteration + $Iterations - 1 -gt 10000) { throw 'StartIteration plus Iterations exceeds the bounded gate range.' }
if (-not [IO.Directory]::Exists($FixtureSourceRoot)) { throw "Tracked hosted fixture source does not exist: $FixtureSourceRoot" }

$trackedFixtureFiles = @(
    'CMakeLists.txt',
    'README.md',
    'app/app.json',
    'build.ps1',
    'guidexos.project',
    'src/main.cpp'
)
$projectMetadataPath = Join-Path $FixtureSourceRoot 'guidexos.project'
$applicationManifestPath = Join-Path $FixtureSourceRoot 'app\app.json'
$sourceProjectMetadata = Get-Content -LiteralPath $projectMetadataPath -Raw | ConvertFrom-Json
$sourceApplicationManifest = Get-Content -LiteralPath $applicationManifestPath -Raw | ConvertFrom-Json
if ($sourceProjectMetadata.projectId -ne 'com.example.debuggerphase29qpositive' -or
    $sourceProjectMetadata.projectId -ne $sourceApplicationManifest.id -or
    $sourceProjectMetadata.displayName -ne $sourceApplicationManifest.displayName -or
    $sourceProjectMetadata.sourceRoot -ne 'src' -or
    $sourceProjectMetadata.applicationManifest -ne 'app/app.json') {
    throw "The tracked Phase 29Q fixture identity or source paths are inconsistent: $FixtureSourceRoot"
}
foreach ($relativePath in $trackedFixtureFiles) {
    $sourcePath = Join-Path $FixtureSourceRoot $relativePath.Replace('/', [IO.Path]::DirectorySeparatorChar)
    if (-not [IO.File]::Exists($sourcePath)) { throw "Tracked Phase 29Q fixture file is missing: $sourcePath" }
}

$smoke = Join-Path $PSScriptRoot 'smoke-developer-studio-debugger.ps1'
$results = New-Object 'System.Collections.Generic.List[object]'
$fixtureRoots = New-Object 'System.Collections.Generic.List[string]'
$started = Get-Date
$runToken = [Guid]::NewGuid().ToString('N').Substring(0, 10)
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$serverExecutable = [IO.Path]::GetFullPath((Join-Path $ServerRoot 'guideXOSServer.experimental.exe'))
$developerStudioPackageRoot = Join-Path $ServerRoot 'Apps\DeveloperStudio'
$developerStudioManifestPath = Join-Path $developerStudioPackageRoot 'app.json'
if (-not [IO.File]::Exists($serverExecutable)) { throw "Hosted Server executable is missing: $serverExecutable" }
if (-not [IO.File]::Exists($developerStudioManifestPath)) { throw "Developer Studio package manifest is missing: $developerStudioManifestPath" }
$developerStudioManifest = Get-Content -LiteralPath $developerStudioManifestPath -Raw | ConvertFrom-Json
$amd64Entries = @($developerStudioManifest.entries | Where-Object { $_.architecture -eq 'amd64' })
if ($developerStudioManifest.id -ne 'com.guidexos.developerstudio' -or $amd64Entries.Count -ne 1 -or
    -not $amd64Entries[0].path) {
    throw "Developer Studio package manifest has no unique AMD64 entry: $developerStudioManifestPath"
}
$developerStudioElfPath = [IO.Path]::GetFullPath((Join-Path $developerStudioPackageRoot $amd64Entries[0].path.Replace('/', [IO.Path]::DirectorySeparatorChar)))
if (-not [IO.File]::Exists($developerStudioElfPath)) { throw "Developer Studio AMD64 package is missing: $developerStudioElfPath" }
$developerStudioElfBytes = [IO.File]::ReadAllBytes($developerStudioElfPath)
$developerStudioElfText = [Text.Encoding]::ASCII.GetString($developerStudioElfBytes)
$requiredLifecycleMarkers = @(
    'TARGET_EXIT_NORMAL code=',
    'debugger_teardown=PASS',
    'debug_inspection=invalidated',
    'debug_watch_runtime=invalidated'
)
$missingLifecycleMarkers = @($requiredLifecycleMarkers | Where-Object { -not $developerStudioElfText.Contains($_) })
$developerStudioElfHash = (Get-FileHash -LiteralPath $developerStudioElfPath -Algorithm SHA256).Hash
if ($missingLifecycleMarkers.Count -gt 0) {
    throw ("Developer Studio package is stale for the hosted terminal gate: path={0} sha256={1} missing_markers={2}. " -f `
        $developerStudioElfPath, $developerStudioElfHash, ($missingLifecycleMarkers -join ',')) +
        'Pass -ServerRoot for an isolated Server stage containing the current Phase 29R-compatible Developer Studio package.'
}

Write-Host 'Developer Studio hosted fresh-fixture target-exit gate'
Write-Host "server_root=$ServerRoot"
Write-Host ("APP_PACKAGE_AUDIT path={0} bytes={1} sha256={2} required_lifecycle_markers={3} result=PASS" -f `
    $developerStudioElfPath, $developerStudioElfBytes.Length, $developerStudioElfHash, $requiredLifecycleMarkers.Count)
Write-Host "fixture_source_root=$FixtureSourceRoot"
Write-Host "fixture_source_project_id=$($sourceProjectMetadata.projectId) application_id=$($sourceApplicationManifest.id) display_name='$($sourceProjectMetadata.displayName)'"
Write-Host "tracked_fixture_file_count=$($trackedFixtureFiles.Count) trace_directory=$TraceDirectory"
Write-Host "iterations=$Iterations start_iteration=$StartIteration expected_target_exit_code=$ExpectedTargetExitCode"
Write-Host 'failure_policy=stop on first failed iteration; no retries'
Write-Host 'fixture_policy=one new temp root per iteration; copy only the six tracked source files; no identity rewrite'

for ($offset = 0; $offset -lt $Iterations; ++$offset) {
    $iteration = $StartIteration + $offset
    $iterationName = '{0:D3}' -f $iteration
    $fixtureLeaf = "debugger-phase29q-positive-29t-$runToken-$iterationName"
    $fixtureRootForRun = [IO.Path]::GetFullPath((Join-Path $tempRoot $fixtureLeaf))
    $childTraceName = "hosted-target-exit-$iterationName-child.log"
    $failureTraceName = "hosted-target-exit-$iterationName-failure.log"
    $runnerLogName = "hosted-target-exit-$iterationName-runner.log"
    if ($fixtureRootForRun.Length -ge 160 -or @($fixtureRootForRun -split '[\\/]+' | Where-Object { $_.Length -ge 64 }).Count -gt 0) {
        throw "Generated fresh fixture root exceeds product path limits: $fixtureRootForRun"
    }
    if ([IO.Directory]::Exists($fixtureRootForRun) -or [IO.File]::Exists($fixtureRootForRun)) {
        throw "Fresh fixture root already exists; refusing to reuse it: $fixtureRootForRun"
    }
    $fixtureRoots.Add($fixtureRootForRun)
    [IO.Directory]::CreateDirectory($fixtureRootForRun) | Out-Null

    $copied = New-Object 'System.Collections.Generic.List[object]'
    foreach ($relativePath in $trackedFixtureFiles) {
        $sourcePath = Join-Path $FixtureSourceRoot $relativePath.Replace('/', [IO.Path]::DirectorySeparatorChar)
        $destinationPath = Join-Path $fixtureRootForRun $relativePath.Replace('/', [IO.Path]::DirectorySeparatorChar)
        [IO.Directory]::CreateDirectory((Split-Path -Parent $destinationPath)) | Out-Null
        $sourceBytes = [IO.File]::ReadAllBytes($sourcePath)
        [IO.File]::WriteAllBytes($destinationPath, $sourceBytes)
        $sourceHash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
        $destinationHash = (Get-FileHash -LiteralPath $destinationPath -Algorithm SHA256).Hash
        if ($sourceHash -ne $destinationHash) { throw "Fresh fixture copy hash mismatch: $relativePath" }
        $copied.Add([pscustomobject]@{ RelativePath = $relativePath; SourceHash = $sourceHash; DestinationHash = $destinationHash; Bytes = $sourceBytes.Length })
    }

    $freshFiles = @(Get-ChildItem -LiteralPath $fixtureRootForRun -Force -Recurse -File -ErrorAction Stop)
    $generatedBefore = @($freshFiles | Where-Object {
        $_.FullName -match '[\\/](build|bin)[\\/]' -or
        $_.Name -match '^guidexos\.debugger\.json(\.bak)?$' -or
        $_.Extension -in @('.elf', '.o', '.obj')
    })
    if ($freshFiles.Count -ne $trackedFixtureFiles.Count -or $generatedBefore.Count -ne 0) {
        throw "Fresh fixture file audit failed: root=$fixtureRootForRun files=$($freshFiles.Count) generated=$($generatedBefore.Count)"
    }
    $copiedProject = Get-Content -LiteralPath (Join-Path $fixtureRootForRun 'guidexos.project') -Raw | ConvertFrom-Json
    $copiedManifest = Get-Content -LiteralPath (Join-Path $fixtureRootForRun 'app\app.json') -Raw | ConvertFrom-Json
    if ($copiedProject.projectId -ne $copiedManifest.id -or
        $copiedProject.displayName -ne $copiedManifest.displayName -or
        -not [IO.Directory]::Exists((Join-Path $fixtureRootForRun $copiedProject.sourceRoot)) -or
        -not [IO.File]::Exists((Join-Path $fixtureRootForRun $copiedProject.applicationManifest))) {
        throw "Fresh fixture identity/path validation failed before Server launch: $fixtureRootForRun"
    }
    Write-Host "FIXTURE_STAGE iteration=$iterationName root=$fixtureRootForRun source_origin=$FixtureSourceRoot project_id=$($copiedProject.projectId) application_id=$($copiedManifest.id) display_name='$($copiedProject.displayName)' generated_before=$($generatedBefore.Count) files_closed=true file_count=$($freshFiles.Count)"
    foreach ($file in $copied) {
        Write-Host "FIXTURE_COPY iteration=$iterationName relative_path=$($file.RelativePath) bytes=$($file.Bytes) source_sha256=$($file.SourceHash) destination_sha256=$($file.DestinationHash)"
    }

    $parameters = @{
        ServerRoot = $ServerRoot
        FixtureRoot = $fixtureRootForRun
        FixtureSourceOrigin = $FixtureSourceRoot
        BreakpointLine = 4
        PositiveGxsmLifecycle = $true
        ExpectedTargetExitCode = $ExpectedTargetExitCode
        DebugWaitSeconds = $DebugWaitSeconds
        MaxRuntimeSeconds = $MaxRuntimeSeconds
        TraceDirectory = $TraceDirectory
        TraceRunIndex = 29000 + $iteration
        TraceArtifactName = $childTraceName
    }
    $arguments = New-ValidationArgumentList -ScriptPath $smoke -Parameters $parameters
    Write-Host ("hosted_target_exit_iteration_{0}/{1}=START fixture_root={2}" -f $iterationName, ('{0:D3}' -f ($StartIteration + $Iterations - 1)), $fixtureRootForRun)
    $result = Invoke-ValidationPowerShell -Name ("hosted-target-exit-$iterationName") `
        -ScriptPath $smoke -ScriptArguments $arguments -MaxOutputLines 1800
    $results.Add($result)
    $runnerLogPath = Join-Path $TraceDirectory $runnerLogName
    Set-Content -LiteralPath $runnerLogPath -Value $result.OutputTail -Encoding UTF8
    $childTracePath = Join-Path $TraceDirectory $childTraceName
    $runnerText = $result.OutputTail -join "`n"
    $traceText = if ([IO.File]::Exists($childTracePath)) { Get-Content -LiteralPath $childTracePath -Raw } else { '' }

    $failureReasons = New-Object 'System.Collections.Generic.List[string]'
    if ($result.ExitCode -ne 0) { $failureReasons.Add("child_exit_code=$($result.ExitCode)") }
    $dispatchMatches = [regex]::Matches($runnerText, 'PROJECT_OPEN_DISPATCH request_id=[0-9a-f]{32} generation=not_exposed route=Ctrl\+Shift\+O\+path\+Enter flow_count=1')
    if ($dispatchMatches.Count -ne 1) { $failureReasons.Add("project_open_dispatch_count=$($dispatchMatches.Count)") }
    if ($runnerText -notmatch 'PROJECT_OPEN_DISPATCH_RESULT request_id=[0-9a-f]{32} result=sent flow_count=1') { $failureReasons.Add('project_open_command_not_sent') }
    if ($runnerText -notmatch 'PROJECT_OPEN_ACCEPTED request_id=[0-9a-f]{32} generation=not_exposed .*result=project_open_PASS') { $failureReasons.Add('project_open_acceptance_not_observed') }
    if (-not $runnerText.Contains('DEVELOPER_STUDIO_WINDOW_READY window_id=')) { $failureReasons.Add('developer_studio_window_not_owned') }
    if (-not $runnerText.Contains('hosted Ctrl+Shift+O reaches the owned Developer Studio window')) { $failureReasons.Add('project_open_key_not_admitted_to_owned_window') }
    if (-not $runnerText.Contains('displays its real project-path prompt before fixture input')) { $failureReasons.Add('project_path_prompt_not_observed') }
    # The wrapper retains the explicit project_open=PASS wait and accepted
    # dispatch record above. The later end-of-smoke summary assertions are not
    # emitted when a terminal debugger marker stops the child first, so use
    # the observed symbol-backed debug_variables marker as the independent
    # project/build readiness proof here.
    if ($runnerText -notmatch 'PASS: hosted target publishes GUIDEXOS_DEVELOPER_STUDIO_MARKER debug_variables=PASS') {
        $failureReasons.Add('project_build_symbol_readiness_not_observed')
    }
    if (-not $runnerText.Contains("hosted Server exits cleanly after the debugger proof (exit code 0)")) { $failureReasons.Add('server_exit_zero_not_asserted') }
    if (-not $runnerText.Contains('process_reaped=True')) { $failureReasons.Add('server_process_not_reaped') }
    $targetExitCount = ([regex]::Matches($traceText, "TARGET_EXIT_NORMAL code=$ExpectedTargetExitCode")).Count
    $debuggerTeardownCount = ([regex]::Matches($traceText, 'debugger_teardown=PASS')).Count
    if ($targetExitCount -lt 2) { $failureReasons.Add("target_exit_markers=$targetExitCount") }
    if ($debuggerTeardownCount -lt 2) { $failureReasons.Add("debugger_teardown_markers=$debuggerTeardownCount") }

    $remainingServerProcesses = @(Get-CimInstance -ClassName Win32_Process -Filter "Name='guideXOSServer.experimental.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.ExecutablePath -and $_.ExecutablePath -ieq $serverExecutable })
    if ($remainingServerProcesses.Count -gt 0) {
        $failureReasons.Add('server_process_still_running=' + (($remainingServerProcesses | ForEach-Object { [string]$_.ProcessId }) -join ','))
    }
    $fixtureLocks = New-Object 'System.Collections.Generic.List[string]'
    foreach ($fixtureFile in $freshFiles) {
        try {
            $exclusive = [IO.FileStream]::new($fixtureFile.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::None)
            $exclusive.Dispose()
        } catch {
            $fixtureLocks.Add($fixtureFile.FullName)
        }
    }
    if ($fixtureLocks.Count -gt 0) { $failureReasons.Add('fixture_files_still_open=' + ($fixtureLocks -join ',')) }
    $unexpectedDebuggerFiles = @(Get-ChildItem -LiteralPath $fixtureRootForRun -Force -Recurse -File -ErrorAction Stop | Where-Object {
        $_.Name -match '^guidexos\.debugger\.json(\.bak)?$'
    })
    if ($unexpectedDebuggerFiles.Count -gt 0) { $failureReasons.Add('debugger_configuration_not_restored') }

    if ($failureReasons.Count -gt 0) {
        Write-BoundedValidationTrace -Path (Join-Path $TraceDirectory $failureTraceName) -Header @(
            'guideXOS Developer Studio Phase 29T hosted fresh-fixture target-exit gate failure',
            "iteration=$iteration",
            "failure=$($failureReasons -join '; ')",
            "childExitCode=$($result.ExitCode)",
            "expectedTargetExitCode=$ExpectedTargetExitCode",
            "serverRoot=$ServerRoot",
            "fixtureSourceRoot=$FixtureSourceRoot",
            "fixtureRoot=$fixtureRootForRun",
            "runnerLog=$runnerLogPath",
            "childTrace=$childTracePath"
        ) -TraceFiles @($childTracePath) -OutputTail $result.OutputTail
        throw "Hosted fresh-fixture target-exit gate stopped at iteration $iteration; $($failureReasons -join '; '). No retry was attempted."
    }
    Write-Host ("hosted_target_exit_iteration_{0}=PASS target_sessions=2 exit_code={1} fixture_root={2} server_reaped=true" -f $iterationName, $ExpectedTargetExitCode, $fixtureRootForRun)
}

$elapsed = [Math]::Round(((Get-Date) - $started).TotalSeconds, 1)
Write-Host "fixture_roots_distinct=$(@($fixtureRoots | Select-Object -Unique).Count)/$Iterations"
Write-Host "hosted_target_exit_iterations=$($results.Count)/$Iterations"
Write-Host "hosted_target_exit_sessions=$($results.Count * 2)"
Write-Host "hosted_target_exit_elapsed_seconds=$elapsed"
Write-Host "hosted_target_exit_gate=PASS exit_code=$ExpectedTargetExitCode"
