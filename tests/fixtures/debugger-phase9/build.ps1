# GUIDEXOS_NATIVE_BUILD_RECIPE_V1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SdkInclude,
    [Parameter(Mandatory=$true)][string]$ToolchainRoot,
    [ValidateSet("Debug", "DebugSymbols")][string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildRoot = Join-Path $RepoRoot "build"
$PackageBin = Join-Path $BuildRoot "bin\amd64"
$ObjectRoot = Join-Path $BuildRoot "objects"
$clang = Join-Path $ToolchainRoot "clang++.exe"
$lld = Join-Path $ToolchainRoot "ld.lld.exe"
$flags = @('--target=x86_64-unknown-elf','-std=c++11','-ffreestanding','-fno-exceptions','-fno-rtti','-fno-stack-protector','-fno-omit-frame-pointer','-fno-unwind-tables','-fno-asynchronous-unwind-tables',"-I$SdkInclude")
if ($Configuration -eq "DebugSymbols") { $flags += @('-g','-O0',"-fdebug-compilation-dir=$RepoRoot","-fdebug-prefix-map=$RepoRoot=.") }
New-Item -ItemType Directory -Force -Path $PackageBin,$ObjectRoot | Out-Null
function Invoke-Checked([string]$FilePath, [string[]]$Arguments) { & $FilePath @Arguments; if ($LASTEXITCODE -ne 0) { throw "Command failed: $FilePath" } }
try {
    $mainObject = Join-Path $ObjectRoot "main.o"
    $memoryObject = Join-Path $ObjectRoot "freestanding_memory.o"
    Invoke-Checked $clang ($flags + @('-c',(Join-Path $RepoRoot 'src\main.cpp'),'-o',$mainObject))
    Invoke-Checked $clang ($flags + @('-c',(Join-Path $RepoRoot 'src\freestanding_memory.cpp'),'-o',$memoryObject))
    $elfPath = Join-Path $PackageBin "debugger-phase9.elf"
    Invoke-Checked $lld @('-m','elf_x86_64','-static','-e','gx_main','--image-base=0x20000000',$mainObject,$memoryObject,'-o',$elfPath)
} finally { if (Test-Path -LiteralPath $ObjectRoot) { Remove-Item -LiteralPath $ObjectRoot -Recurse -Force } }
