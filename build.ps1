[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO",
    [switch]$SkipModelTest
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$SdkInclude = Join-Path $ServerRoot "sdk\include"
$PackageRoot = Join-Path $ServerRoot "Apps\DeveloperStudio"
$PackageBin = Join-Path $PackageRoot "bin\amd64"
$Manifest = Join-Path $RepoRoot "app\app.json"
$ModelTest = Join-Path $ServerRoot "tmp\developer-studio-model-test.exe"
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
            "-Isrc", "src\developer_studio_models.cpp", "tests\model_test.cpp",
            "-o", $ModelTest
        )
        & $ModelTest
        if ($LASTEXITCODE -ne 0) { throw "Developer Studio model test failed with exit code $LASTEXITCODE" }
    }

    $compileFlags = @(
        "--target=x86_64-unknown-elf", "-std=c++11", "-ffreestanding",
        "-fno-exceptions", "-fno-rtti", "-fno-stack-protector",
        "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
        "-I$SdkInclude", "-Isrc"
    )
    $modelObject = Join-Path $ObjectRoot "developer_studio_models.o"
    $workspaceObject = Join-Path $ObjectRoot "developer_studio_workspace.o"
    $memoryObject = Join-Path $ObjectRoot "freestanding_memory.o"
    $mainObject = Join-Path $ObjectRoot "main.o"
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_models.cpp"), "-o", $modelObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\developer_studio_workspace.cpp"), "-o", $workspaceObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\freestanding_memory.cpp"), "-o", $memoryObject))
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot "src\main.cpp"), "-o", $mainObject))

    $elfPath = Join-Path $PackageBin "developerstudio.elf"
    Invoke-Checked $lld @("-m", "elf_x86_64", "-static", "-e", "gx_main", $modelObject, $workspaceObject, $memoryObject, $mainObject, "-o", $elfPath)
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
    Write-Host "Staged App Model manifest: $(Join-Path $PackageRoot 'app.json')"
} finally {
    if (Test-Path -LiteralPath $ModelTest) { Remove-Item -LiteralPath $ModelTest -Force }
    if (Test-Path -LiteralPath $ObjectRoot) { Remove-Item -LiteralPath $ObjectRoot -Recurse -Force }
}
