# GUIDEXOS_NATIVE_BUILD_RECIPE_V1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SdkInclude,
    [Parameter(Mandatory=$true)][string]$ToolchainRoot,
    [ValidateSet("Debug", "DebugSymbols")][string]$Configuration = "Debug",
    [switch]$SkipReadElf
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildRoot = Join-Path $RepoRoot "build"
$PackageBin = Join-Path $BuildRoot "bin\amd64"
$ObjectRoot = Join-Path $BuildRoot "objects"
$clang = Join-Path $ToolchainRoot "clang++.exe"
$lld = Join-Path $ToolchainRoot "ld.lld.exe"
$readElf = Join-Path $ToolchainRoot "llvm-readelf.exe"
if (-not (Test-Path -LiteralPath $SdkInclude -PathType Container)) { throw "SDK headers not found: $SdkInclude" }
if (-not (Test-Path -LiteralPath $clang -PathType Leaf) -or -not (Test-Path -LiteralPath $lld -PathType Leaf)) { throw "LLVM clang++ and ld.lld are required" }
New-Item -ItemType Directory -Force -Path $PackageBin | Out-Null
New-Item -ItemType Directory -Force -Path $ObjectRoot | Out-Null
$flags = @('--target=x86_64-unknown-elf','-std=c++11','-ffreestanding','-fno-exceptions','-fno-rtti','-fno-stack-protector','-fno-omit-frame-pointer','-fno-unwind-tables','-fno-asynchronous-unwind-tables',"-I$SdkInclude")
if ($Configuration -eq "DebugSymbols") {
    $flags += @('-g','-O0',"-fdebug-compilation-dir=$RepoRoot","-fdebug-prefix-map=$RepoRoot=.")
}
function Invoke-Checked([string]$FilePath, [string[]]$Arguments) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed: $FilePath" }
}
try {
    $mainObject = Join-Path $ObjectRoot "main.o"
    $memoryObject = Join-Path $ObjectRoot "freestanding_memory.o"
    Invoke-Checked $clang ($flags + @('-c',(Join-Path $RepoRoot 'src\main.cpp'),'-o',$mainObject))
    Invoke-Checked $clang ($flags + @('-c',(Join-Path $RepoRoot 'src\freestanding_memory.cpp'),'-o',$memoryObject))
    $elfPath = Join-Path $PackageBin "debugger-phase7.elf"
    Invoke-Checked $lld @('-m','elf_x86_64','-static','-e','gx_main','--image-base=0x20000000',$mainObject,$memoryObject,'-o',$elfPath)
    if (-not (Test-Path -LiteralPath $elfPath -PathType Leaf)) { throw "Native ELF output was not produced" }
    if (-not $SkipReadElf -and (Test-Path -LiteralPath $readElf -PathType Leaf)) {
        $header = (& $readElf -h $elfPath 2>&1 | Out-String)
        if ($header -notmatch 'ELF64' -or $header -notmatch 'Advanced Micro Devices X86-64') { throw "ELF64 AMD64 validation failed" }
    }
} finally {
    if (Test-Path -LiteralPath $ObjectRoot) { Remove-Item -LiteralPath $ObjectRoot -Recurse -Force }
}
