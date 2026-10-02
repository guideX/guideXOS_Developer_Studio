# GUIDEXOS_NATIVE_BUILD_RECIPE_V1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SdkInclude,
    [Parameter(Mandatory=$true)][string]$ToolchainRoot,
    [ValidateSet("Debug", "DebugSymbols")][string]$Configuration = "Debug",
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO"
)

$ErrorActionPreference = "Stop"
if ($env:GUIDEXOS_SERVER_ROOT) { $ServerRoot = $env:GUIDEXOS_SERVER_ROOT }
$fixtureRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$output = Join-Path $fixtureRoot "build\bin\amd64\debugger-phase29q-positive.elf"
$producer = Join-Path ([IO.Path]::GetFullPath($ServerRoot)) "scripts\build-gxsm-debugger-fixture.ps1"
if (-not (Test-Path -LiteralPath $producer -PathType Leaf)) { throw "bootstrap compiler producer script not found: $producer" }

# SdkInclude and ToolchainRoot are part of the hosted recipe contract. The
# positive artifact intentionally uses the production guideXOS compiler
# backend and final GXSM writer instead of Clang/LLD.
& $producer `
    -SourcePath (Join-Path $fixtureRoot "src\main.cpp") `
    -SourceIdentity "src/main.cpp" `
    -OutputPath $output
if (-not (Test-Path -LiteralPath $output -PathType Leaf)) { throw "guideXOS bootstrap compiler failed to build the GXSM fixture" }
