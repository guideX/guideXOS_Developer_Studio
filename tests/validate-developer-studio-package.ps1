[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ServerRoot,
    [string]$PackageRoot = '',
    [switch]$AllowDebugSections
)

$ErrorActionPreference = 'Stop'
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
if (-not $PackageRoot) { $PackageRoot = Join-Path $ServerRoot 'Apps\DeveloperStudio' }
$PackageRoot = [IO.Path]::GetFullPath($PackageRoot)
$ManifestPath = Join-Path $PackageRoot 'app.json'
$ElfPath = Join-Path $PackageRoot 'bin\amd64\developerstudio.elf'

function Fail-Audit([string]$Message) {
    throw "Developer Studio package audit failed: $Message"
}

function Find-ReadElf {
    foreach ($name in @('llvm-readelf.exe', 'llvm-readelf', 'readelf.exe', 'readelf')) {
        $command = Get-Command $name -ErrorAction SilentlyContinue
        if ($command) { return $command.Source }
    }
    foreach ($root in @('C:\Program Files\LLVM\bin', 'C:\mingw64\bin')) {
        foreach ($name in @('llvm-readelf.exe', 'readelf.exe')) {
            $candidate = Join-Path $root $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
        }
    }
    return $null
}

if (-not (Test-Path -LiteralPath $PackageRoot -PathType Container)) { Fail-Audit "package directory is missing: $PackageRoot" }
if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) { Fail-Audit 'app.json is missing' }
if (-not (Test-Path -LiteralPath $ElfPath -PathType Leaf)) { Fail-Audit 'developerstudio.elf is missing' }

$expectedFiles = @('app.json', 'bin/amd64/developerstudio.elf')
$actualFiles = @(Get-ChildItem -LiteralPath $PackageRoot -Recurse -File -Force | ForEach-Object {
    $_.FullName.Substring($PackageRoot.Length + 1).Replace('\', '/')
})
$unexpectedFiles = @($actualFiles | Where-Object { $_ -notin $expectedFiles })
if ($unexpectedFiles.Count -ne 0) { Fail-Audit "unexpected package files: $($unexpectedFiles -join ', ')" }
if ($actualFiles.Count -ne $expectedFiles.Count) { Fail-Audit "expected exactly $($expectedFiles.Count) runtime files, found $($actualFiles.Count)" }

try { $manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json }
catch { Fail-Audit "manifest is not valid JSON: $($_.Exception.Message)" }
if ($manifest.schemaVersion -ne 1) { Fail-Audit 'manifest schemaVersion is not 1' }
if ($manifest.id -ne 'com.guidexos.developerstudio') { Fail-Audit 'manifest application ID is not canonical' }
if ($manifest.displayName -ne 'guideXOS Developer Studio') { Fail-Audit 'manifest displayName is not canonical' }
if ($manifest.kind -ne 'NativeElf') { Fail-Audit 'manifest kind is not NativeElf' }
if (@($manifest.supportedArchitectures) -join ',' -ne 'amd64') { Fail-Audit 'manifest architecture is not amd64-only' }
if (@($manifest.entries).Count -ne 1) { Fail-Audit 'manifest must contain exactly one executable entry' }
$entry = @($manifest.entries)[0]
if ($entry.architecture -ne 'amd64' -or $entry.path -ne 'bin/amd64/developerstudio.elf' -or
    $entry.entryPoint -ne 'gx_main' -or $entry.abi -ne 'guidexos-c-abi-v1' -or $entry.runtime -ne 'native-elf') {
    Fail-Audit 'manifest executable entry does not match the packaged Developer Studio ELF'
}

$readElf = Find-ReadElf
if (-not $readElf) { Fail-Audit 'llvm-readelf/readelf is required for ELF package validation' }
$header = (& $readElf -h $ElfPath 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { Fail-Audit 'readelf -h failed' }
if ($header -notmatch 'Class:\s+ELF64') { Fail-Audit 'ELF class is not ELF64' }
if ($header -notmatch 'Data:\s+2''s complement, little endian') { Fail-Audit 'ELF endianness is not little endian' }
if ($header -notmatch 'Type:\s+EXEC') { Fail-Audit 'ELF type is not ET_EXEC' }
if ($header -notmatch 'Machine:\s+Advanced Micro Devices X86-64') { Fail-Audit 'ELF architecture is not AMD64' }
$sections = (& $readElf -S $ElfPath 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { Fail-Audit 'readelf -S failed' }
$hasSymtab = $sections -match '(?m)\]\s+\.symtab\s'
$debugSectionMatches = @([regex]::Matches($sections, '(?m)\]\s+(\.debug_[^\s]+)') | ForEach-Object { $_.Groups[1].Value })
if (-not $hasSymtab) { Fail-Audit 'production ELF does not contain .symtab' }
if (-not $AllowDebugSections -and $debugSectionMatches.Count -ne 0) {
    Fail-Audit "production package contains debug sections: $($debugSectionMatches -join ', ')"
}

$file = Get-Item -LiteralPath $ElfPath
$hash = (Get-FileHash -LiteralPath $ElfPath -Algorithm SHA256).Hash
Write-Host "package_path=$ElfPath"
Write-Host "package_size=$($file.Length)"
Write-Host "package_sha256=$hash"
Write-Host "elf_class=ELF64"
Write-Host "elf_endian=little"
Write-Host "elf_architecture=amd64"
Write-Host "elf_type=ET_EXEC"
Write-Host "elf_entry_point=$(([regex]::Match($header, 'Entry point address:\s+(\S+)')).Groups[1].Value)"
Write-Host "elf_symtab=$([bool]$hasSymtab)"
Write-Host "elf_debug_sections=$([bool]($debugSectionMatches.Count -ne 0))"
Write-Host "package_files=$($actualFiles -join ',')"
Write-Host 'package_content_audit=PASS'
