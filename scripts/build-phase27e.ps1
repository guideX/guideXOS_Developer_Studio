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
$PackageRoot = Join-Path $ServerRoot "Apps\DS27E"
$PackageBin = Join-Path $PackageRoot "bin\amd64"
$ObjectRoot = Join-Path $ServerRoot "tmp\developer-studio-phase27e-build"
$ElfPath = Join-Path $PackageBin "p27e.elf"
$StagedElf = Join-Path $ObjectRoot "p27e.elf"

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
    Write-Host ("Running: {0} {1}" -f $FilePath, ($Arguments -join " "))
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
    # ld.lld emits useful normal ELF metadata (PT_PHDR/PT_GNU_STACK and
    # section tables). NativeElf intentionally accepts only the bounded
    # PT_LOAD subset, so retain the load headers and clear section metadata in
    # the staged copy. No code or load bytes are changed by this normalization.
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 64 -or $bytes[0] -ne 0x7f -or $bytes[1] -ne 0x45 -or
        $bytes[2] -ne 0x4c -or $bytes[3] -ne 0x46) { throw "Phase 27E ELF header is invalid." }
    $programHeaderOffset = [BitConverter]::ToInt64($bytes, 32)
    $programHeaderCount = [BitConverter]::ToUInt16($bytes, 56)
    $programHeaderSize = [BitConverter]::ToUInt16($bytes, 54)
    if ($programHeaderOffset -ne 64 -or $programHeaderSize -ne 56) { throw "Unexpected Phase 27E program-header layout." }
    $loads = New-Object System.Collections.Generic.List[byte[]]
    for ($i = 0; $i -lt $programHeaderCount; ++$i) {
        $offset = [int]($programHeaderOffset + ($i * $programHeaderSize))
        if ((Get-U32 $bytes $offset) -eq 1) {
            $header = New-Object byte[] $programHeaderSize
            [Array]::Copy($bytes, $offset, $header, 0, $programHeaderSize)
            $loads.Add($header)
        }
    }
    if ($loads.Count -eq 0 -or $loads.Count -gt 4) { throw "Phase 27E ELF has an unsupported PT_LOAD count." }
    for ($i = 0; $i -lt $loads.Count; ++$i) {
        [Array]::Copy($loads[$i], 0, $bytes, [int]($programHeaderOffset + ($i * $programHeaderSize)), $programHeaderSize)
    }
    Set-U16 $bytes 56 $loads.Count
    # e_shoff, e_shentsize, e_shnum, and e_shstrndx are unsupported by the
    # NativeElf validator and are deliberately zero in the ABI subset.
    for ($i = 40; $i -lt 48; ++$i) { $bytes[$i] = 0 }
    for ($i = 58; $i -lt 64; ++$i) { $bytes[$i] = 0 }
    [IO.File]::WriteAllBytes($Path, $bytes)
}

$toolRoots = @("C:\Program Files\LLVM\bin", "C:\mingw64\bin")
if (-not [string]::IsNullOrWhiteSpace($ToolchainRoot)) { $toolRoots = @($ToolchainRoot) + $toolRoots }
$clang = Find-Tool @("clang++.exe", "clang++") $toolRoots
$lld = Find-Tool @("ld.lld.exe", "ld.lld") $toolRoots
if (-not $clang -or -not $lld) { throw "clang++ and ld.lld are required for the Phase 27E app proof." }
if (-not (Test-Path -LiteralPath $SdkInclude -PathType Container)) { throw "SDK include directory not found: $SdkInclude" }

New-Item -ItemType Directory -Force -Path $PackageBin | Out-Null
New-Item -ItemType Directory -Force -Path $ObjectRoot | Out-Null
Copy-Item -LiteralPath (Join-Path $RepoRoot "app\phase27e_app.json") -Destination (Join-Path $PackageRoot "app.json") -Force
if (Test-Path -LiteralPath $ElfPath -PathType Leaf) { Remove-Item -LiteralPath $ElfPath -Force }

$compileFlags = @(
    "--target=x86_64-unknown-elf", "-std=c++11", "-ffreestanding", "-O2",
    "-fno-exceptions", "-fno-rtti", "-fno-stack-protector",
    "-fno-unwind-tables", "-fno-asynchronous-unwind-tables",
    "-ffunction-sections", "-fdata-sections",
    "-DGXOS_DEVELOPER_STUDIO_BARE_METAL", "-I$SdkInclude", "-Isrc"
)
$linkerScript = Join-Path $RepoRoot "scripts\phase27e-native.ld"
$sources = @(
    "developer_studio_find.cpp", "developer_studio_syntax.cpp", "developer_studio_models.cpp",
    "developer_studio_projects.cpp", "developer_studio_workspace.cpp", "developer_studio_output.cpp",
    "developer_studio_build.cpp", "freestanding_memory.cpp", "phase27e_smoke_main.cpp"
)
$objects = @()
foreach ($source in $sources) {
    $object = Join-Path $ObjectRoot (($source -replace '\.cpp$', '') + ".o")
    Invoke-Checked $clang ($compileFlags + @("-c", (Join-Path $RepoRoot ("src\" + $source)), "-o", $object))
    $objects += $object
}
Invoke-Checked $lld (@("-m", "elf_x86_64", "-static", "-e", "gx_main", "--gc-sections", "-z", "max-page-size=0x1000", "-T", $linkerScript) + $objects + @("-o", $StagedElf))
Normalize-NativeElf $StagedElf
if (-not (Test-Path -LiteralPath $StagedElf -PathType Leaf) -or (Get-Item -LiteralPath $StagedElf).Length -le 0) { throw "Phase 27E NativeElf output was not produced." }
Move-Item -LiteralPath $StagedElf -Destination $ElfPath -Force
Write-Host "Developer Studio Phase 27E NativeElf proof app built: $ElfPath" -ForegroundColor Green
Write-Host ("Bytes: {0}" -f (Get-Item -LiteralPath $ElfPath).Length)

if (Test-Path -LiteralPath $ObjectRoot) { Remove-Item -LiteralPath $ObjectRoot -Recurse -Force }
