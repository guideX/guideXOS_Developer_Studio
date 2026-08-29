[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerRoot,
    [string]$ToolchainRoot = ""
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$SdkInclude = Join-Path $ServerRoot "sdk\include"
$PackageRoot = Join-Path $ServerRoot "Apps\DS27I"
$PackageBin = Join-Path $PackageRoot "bin\amd64"
$ObjectRoot = Join-Path $ServerRoot "tmp\developer-studio-phase27i-build"
$ElfPath = Join-Path $PackageBin "p27i.elf"
$StagedElf = Join-Path $ObjectRoot "p27i.elf"

function Find-Tool([string[]]$Names, [string[]]$Roots) {
    foreach ($name in $Names) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    foreach ($root in $Roots) {
        foreach ($name in $Names) {
            $candidate = Join-Path $root $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
    }
    return $null
}

function Invoke-Checked([string]$FilePath, [string[]]$Arguments) {
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed with exit code $LASTEXITCODE`: $FilePath" }
}

function Set-U16([byte[]]$Bytes, [int]$Offset, [int]$Value) {
    $Bytes[$Offset] = [byte]($Value -band 0xff)
    $Bytes[$Offset + 1] = [byte](($Value -shr 8) -band 0xff)
}

function Get-U32([byte[]]$Bytes, [int]$Offset) {
    return [uint32]($Bytes[$Offset] -bor ($Bytes[$Offset + 1] -shl 8) -bor
        ($Bytes[$Offset + 2] -shl 16) -bor ($Bytes[$Offset + 3] -shl 24))
}

function Normalize-NativeElf([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x7f -or $bytes[1] -ne 0x45 -or $bytes[2] -ne 0x4c -or $bytes[3] -ne 0x46) { throw "Phase 27I ELF header is invalid." }
    $phoff = [BitConverter]::ToInt64($bytes, 32)
    $phentsize = [BitConverter]::ToUInt16($bytes, 54)
    $phnum = [BitConverter]::ToUInt16($bytes, 56)
    if ($phoff -ne 64 -or $phentsize -ne 56) { throw "Unexpected Phase 27I program-header layout." }
    $loads = New-Object System.Collections.Generic.List[byte[]]
    for ($i = 0; $i -lt $phnum; ++$i) {
        $offset = [int]($phoff + ($i * $phentsize))
        if ((Get-U32 $bytes $offset) -eq 1) {
            $header = New-Object byte[] $phentsize
            [Array]::Copy($bytes, $offset, $header, 0, $phentsize)
            $loads.Add($header)
        }
    }
    if ($loads.Count -eq 0 -or $loads.Count -gt 4) { throw "Phase 27I ELF has an unsupported PT_LOAD count." }
    for ($i = 0; $i -lt $loads.Count; ++$i) { [Array]::Copy($loads[$i], 0, $bytes, [int]($phoff + ($i * $phentsize)), $phentsize) }
    Set-U16 $bytes 56 $loads.Count
    for ($i = 40; $i -lt 48; ++$i) { $bytes[$i] = 0 }
    for ($i = 58; $i -lt 64; ++$i) { $bytes[$i] = 0 }
    [IO.File]::WriteAllBytes($Path, $bytes)
}

$toolRoots = @("C:\Program Files\LLVM\bin", "C:\mingw64\bin")
if (-not [string]::IsNullOrWhiteSpace($ToolchainRoot)) { $toolRoots = @($ToolchainRoot) + $toolRoots }
$clang = Find-Tool @("clang++.exe", "clang++") $toolRoots
$lld = Find-Tool @("ld.lld.exe", "ld.lld") $toolRoots
if (-not $clang -or -not $lld) { throw "clang++ and ld.lld are required for the Phase 27I app proof." }
if (-not (Test-Path -LiteralPath $SdkInclude -PathType Container)) { throw "SDK include directory not found: $SdkInclude" }

New-Item -ItemType Directory -Force -Path $PackageBin, $ObjectRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $RepoRoot "app\phase27i_app.json") -Destination (Join-Path $PackageRoot "app.json") -Force
if (Test-Path -LiteralPath $ElfPath -PathType Leaf) { Remove-Item -LiteralPath $ElfPath -Force }

$compileFlags = @(
    "--target=x86_64-unknown-elf", "-std=c++11", "-ffreestanding", "-O2",
    "-fno-exceptions", "-fno-rtti", "-fno-stack-protector",
    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
    "-ffunction-sections", "-fdata-sections", "-DGXOS_DEVELOPER_STUDIO_BARE_METAL",
    "-DGXOS_PHASE27I_APP", "-I$SdkInclude", "-Isrc"
)
$linkerScript = Join-Path $RepoRoot "scripts\phase27e-native.ld"
$sources = @(
    "developer_studio_find.cpp", "developer_studio_syntax.cpp", "developer_studio_models.cpp",
    "developer_studio_projects.cpp", "developer_studio_workspace.cpp", "developer_studio_output.cpp",
    "developer_studio_build.cpp", "developer_studio_run.cpp", "freestanding_memory.cpp",
    "phase27e_smoke_main.cpp"
)
$objects = @()
foreach ($source in $sources) {
    $object = Join-Path $ObjectRoot (($source -replace '\.cpp$', '') + ".o")
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot ("src\" + $source)), "-o", $object))
    $objects += $object
}
Invoke-Checked $lld (@("-m", "elf_x86_64", "-static", "-e", "gx_main", "--gc-sections", "-z", "max-page-size=0x1000", "-T", $linkerScript) + $objects + @("-o", $StagedElf))
Normalize-NativeElf $StagedElf
if (-not (Test-Path -LiteralPath $StagedElf -PathType Leaf) -or (Get-Item -LiteralPath $StagedElf).Length -le 0) { throw "Phase 27I NativeElf output was not produced." }
Move-Item -LiteralPath $StagedElf -Destination $ElfPath -Force
Write-Host "Developer Studio Phase 27I NativeElf proof app built: $ElfPath" -ForegroundColor Green
Write-Host ("Bytes: {0}" -f (Get-Item -LiteralPath $ElfPath).Length)
if (Test-Path -LiteralPath $ObjectRoot) { Remove-Item -LiteralPath $ObjectRoot -Recurse -Force }
