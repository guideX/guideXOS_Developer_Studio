[CmdletBinding()]
param(
    [string]$ServerRoot = "",
    [switch]$SkipModelTest,
    [switch]$SkipProjectTest
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ServerRoot) { $ServerRoot = $env:GUIDEXOS_SERVER_ROOT }
if (-not $ServerRoot) { throw "Pass -ServerRoot or set GUIDEXOS_SERVER_ROOT." }
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$SdkInclude = Join-Path $ServerRoot "sdk\include"
$PackageRoot = Join-Path $ServerRoot "Apps\DeveloperStudio"
$PackageBin = Join-Path $PackageRoot "bin\amd64"
$Manifest = Join-Path $RepoRoot "app\app.json"
$ModelTest = Join-Path $ServerRoot "tmp\developer-studio-model-test.exe"
$ProjectTest = Join-Path $ServerRoot "tmp\developer-studio-project-test.exe"
$RunTest = Join-Path $ServerRoot "tmp\developer-studio-run-test.exe"
$FindTest = Join-Path $ServerRoot "tmp\developer-studio-find-test.exe"
$SearchTest = Join-Path $ServerRoot "tmp\developer-studio-project-search-test.exe"
$SymbolTest = Join-Path $ServerRoot "tmp\developer-studio-symbol-test.exe"
$NavigationTest = Join-Path $ServerRoot "tmp\developer-studio-navigation-definition-test.exe"
$ReferenceTest = Join-Path $ServerRoot "tmp\developer-studio-reference-test.exe"
$RenameTest = Join-Path $ServerRoot "tmp\developer-studio-rename-test.exe"
$CompletionTest = Join-Path $ServerRoot "tmp\developer-studio-completion-test.exe"
$SignatureTest = Join-Path $ServerRoot "tmp\developer-studio-signature-test.exe"
$ObjectRoot = Join-Path $ServerRoot "tmp\developer-studio-build"

function Find-Tool([string[]]$Names, [string[]]$KnownRoots) {
    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    foreach ($root in $KnownRoots) {
        foreach ($name in $Names) {
            $candidate = Join-Path $root $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
    }
    return $null
}

function Invoke-Checked([string]$FilePath, [string[]]$Arguments) {
    Write-Host ("Running: {0} {1}" -f $FilePath, ($Arguments -join " "))
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE`: $FilePath" }
}

if (-not (Test-Path -LiteralPath $SdkInclude -PathType Container)) {
    throw "guideXOS Server SDK headers were not found: $SdkInclude"
}
if (-not (Test-Path -LiteralPath $Manifest -PathType Leaf)) {
    throw "Developer Studio manifest was not found: $Manifest"
}

$clang = Find-Tool @("clang++.exe", "clang++") @("C:\Program Files\LLVM\bin", "C:\mingw64\bin")
$lld = Find-Tool @("ld.lld.exe", "ld.lld") @("C:\Program Files\LLVM\bin", "C:\mingw64\bin")
$readElf = Find-Tool @("llvm-readelf.exe", "llvm-readelf", "readelf.exe", "readelf") @("C:\Program Files\LLVM\bin", "C:\mingw64\bin")
if (-not $clang) { throw "clang++ was not found. Install LLVM or add clang++ to PATH." }
if (-not $lld) { throw "ld.lld was not found. Install LLVM or add ld.lld to PATH." }

New-Item -ItemType Directory -Force -Path $PackageBin | Out-Null
New-Item -ItemType Directory -Force -Path $ObjectRoot | Out-Null
Copy-Item -LiteralPath $Manifest -Destination (Join-Path $PackageRoot "app.json") -Force

try {
    if (-not $SkipModelTest) {
        Invoke-Checked "g++" @(
            "-std=c++11", "-Wall", "-Wextra", "-pedantic",
            "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "tests\model_test.cpp",
            "-o", $ModelTest
        )
        & $ModelTest
        if ($LASTEXITCODE -ne 0) { throw "Developer Studio model test failed with exit code $LASTEXITCODE" }
    }
    if (-not $SkipProjectTest) {
        Invoke-Checked "g++" @(
            "-std=c++17", "-Wall", "-Wextra", "-pedantic",
            "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_projects.cpp", "src\developer_studio_symbols.cpp", "src\developer_studio_workspace.cpp", "tests\project_test.cpp",
            "-o", $ProjectTest
        )
        & $ProjectTest
        if ($LASTEXITCODE -ne 0) { throw "Developer Studio project test failed with exit code $LASTEXITCODE" }
    }
    Invoke-Checked "g++" @(
        "-std=c++11", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_output.cpp", "src\developer_studio_run.cpp", "tests\run_test.cpp",
        "-o", $RunTest
    )
    & $RunTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio run controller test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "tests\find_test.cpp",
        "-o", $FindTest
    )
    & $FindTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio find test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_projects.cpp", "src\developer_studio_project_search.cpp", "tests\project_search_test.cpp",
        "-o", $SearchTest
    )
    & $SearchTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio project search test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_symbols.cpp", "src\developer_studio_navigation.cpp", "tests\symbol_test.cpp",
        "-o", $SymbolTest
    )
    & $SymbolTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio symbol test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_symbols.cpp", "src\developer_studio_navigation.cpp", "tests\navigation_definition_test.cpp",
        "-o", $NavigationTest
    )
    & $NavigationTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio definition navigation test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_projects.cpp", "src\developer_studio_project_search.cpp", "src\developer_studio_symbols.cpp", "src\developer_studio_navigation.cpp", "src\developer_studio_references.cpp", "tests\reference_test.cpp",
        "-o", $ReferenceTest
    )
    & $ReferenceTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio reference test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_projects.cpp", "src\developer_studio_project_search.cpp", "src\developer_studio_symbols.cpp", "src\developer_studio_navigation.cpp", "src\developer_studio_references.cpp", "src\developer_studio_rename.cpp", "tests\rename_test.cpp",
        "-o", $RenameTest
    )
    & $RenameTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio rename test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_symbols.cpp", "src\developer_studio_completion.cpp", "tests\completion_test.cpp",
        "-o", $CompletionTest
    )
    & $CompletionTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio completion test failed with exit code $LASTEXITCODE" }

    Invoke-Checked "g++" @(
        "-std=c++17", "-Wall", "-Wextra", "-pedantic",
        "-Isrc", "src\developer_studio_find.cpp", "src\developer_studio_syntax.cpp", "src\developer_studio_models.cpp", "src\developer_studio_symbols.cpp", "src\developer_studio_signature.cpp", "tests\signature_test.cpp",
        "-o", $SignatureTest
    )
    & $SignatureTest
    if ($LASTEXITCODE -ne 0) { throw "Developer Studio signature help model test failed with exit code $LASTEXITCODE" }

    $compileFlags = @(
        "--target=x86_64-unknown-elf", "-std=c++11", "-ffreestanding",
        "-fno-exceptions", "-fno-rtti", "-fno-stack-protector",
        "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
        "-I$SdkInclude", "-Isrc"
    )
    $modelObject = Join-Path $ObjectRoot "developer_studio_models.o"
    $findObject = Join-Path $ObjectRoot "developer_studio_find.o"
    $syntaxObject = Join-Path $ObjectRoot "developer_studio_syntax.o"
    $projectObject = Join-Path $ObjectRoot "developer_studio_projects.o"
    $workspaceObject = Join-Path $ObjectRoot "developer_studio_workspace.o"
    $buildObject = Join-Path $ObjectRoot "developer_studio_build.o"
    $outputObject = Join-Path $ObjectRoot "developer_studio_output.o"
    $runObject = Join-Path $ObjectRoot "developer_studio_run.o"
    $searchObject = Join-Path $ObjectRoot "developer_studio_project_search.o"
    $symbolObject = Join-Path $ObjectRoot "developer_studio_symbols.o"
    $navigationObject = Join-Path $ObjectRoot "developer_studio_navigation.o"
    $referencesObject = Join-Path $ObjectRoot "developer_studio_references.o"
    $renameObject = Join-Path $ObjectRoot "developer_studio_rename.o"
    $completionObject = Join-Path $ObjectRoot "developer_studio_completion.o"
    $signatureObject = Join-Path $ObjectRoot "developer_studio_signature.o"
    $memoryObject = Join-Path $ObjectRoot "freestanding_memory.o"
    $mainObject = Join-Path $ObjectRoot "main.o"
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_models.cpp"), "-o", $modelObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_find.cpp"), "-o", $findObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_syntax.cpp"), "-o", $syntaxObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_projects.cpp"), "-o", $projectObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_workspace.cpp"), "-o", $workspaceObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_build.cpp"), "-o", $buildObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_output.cpp"), "-o", $outputObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_run.cpp"), "-o", $runObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_project_search.cpp"), "-o", $searchObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_symbols.cpp"), "-o", $symbolObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_navigation.cpp"), "-o", $navigationObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_references.cpp"), "-o", $referencesObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_rename.cpp"), "-o", $renameObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_completion.cpp"), "-o", $completionObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_signature.cpp"), "-o", $signatureObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\freestanding_memory.cpp"), "-o", $memoryObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\main.cpp"), "-o", $mainObject))

    $elfPath = Join-Path $PackageBin "developerstudio.elf"
    Invoke-Checked $lld @("-m", "elf_x86_64", "-static", "-e", "gx_main", $findObject, $syntaxObject, $modelObject, $projectObject, $workspaceObject, $buildObject, $outputObject, $runObject, $searchObject, $symbolObject, $navigationObject, $referencesObject, $renameObject, $completionObject, $signatureObject, $memoryObject, $mainObject, "-o", $elfPath)
    if (-not (Test-Path -LiteralPath $elfPath -PathType Leaf) -or (Get-Item -LiteralPath $elfPath).Length -le 0) {
        throw "Native ELF output was not produced: $elfPath"
    }

    if ($readElf) {
        $header = (& $readElf -h $elfPath 2>&1 | Out-String)
        if ($LASTEXITCODE -ne 0 -or $header -notmatch "ELF64" -or $header -notmatch "Advanced Micro Devices X86-64") {
            throw "Developer Studio ELF header validation failed."
        }
    }

    Write-Host "Developer Studio Native ELF build PASS: $elfPath"
    $elfHash = (Get-FileHash -LiteralPath $elfPath -Algorithm SHA256).Hash
    Write-Host "Developer Studio packaged ELF SHA256: $elfHash"
    Write-Host "Staged App Model manifest: $(Join-Path $PackageRoot 'app.json')"
} finally {
    if (Test-Path -LiteralPath $ModelTest) { Remove-Item -LiteralPath $ModelTest -Force }
    if (Test-Path -LiteralPath $ProjectTest) { Remove-Item -LiteralPath $ProjectTest -Force }
    if (Test-Path -LiteralPath $RunTest) { Remove-Item -LiteralPath $RunTest -Force }
    if (Test-Path -LiteralPath $FindTest) { Remove-Item -LiteralPath $FindTest -Force }
    if (Test-Path -LiteralPath $SearchTest) { Remove-Item -LiteralPath $SearchTest -Force }
    if (Test-Path -LiteralPath $SymbolTest) { Remove-Item -LiteralPath $SymbolTest -Force }
    if (Test-Path -LiteralPath $NavigationTest) { Remove-Item -LiteralPath $NavigationTest -Force }
    if (Test-Path -LiteralPath $ReferenceTest) { Remove-Item -LiteralPath $ReferenceTest -Force }
    if (Test-Path -LiteralPath $RenameTest) { Remove-Item -LiteralPath $RenameTest -Force }
    if (Test-Path -LiteralPath $CompletionTest) { Remove-Item -LiteralPath $CompletionTest -Force }
    if (Test-Path -LiteralPath $SignatureTest) { Remove-Item -LiteralPath $SignatureTest -Force }
    if (Test-Path -LiteralPath $ObjectRoot) { Remove-Item -LiteralPath $ObjectRoot -Recurse -Force }
}
