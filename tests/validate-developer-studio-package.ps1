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
$Architectures = @(
    [PSCustomObject]@{ Name = 'amd64'; Machine = 'Advanced Micro Devices X86-64' },
    [PSCustomObject]@{ Name = 'arm64'; Machine = 'AArch64' }
)

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
$ElfPaths = @{}
foreach ($architecture in $Architectures) {
    $elfPath = Join-Path $PackageRoot ("bin\{0}\developerstudio.elf" -f $architecture.Name)
    if (-not (Test-Path -LiteralPath $elfPath -PathType Leaf)) { Fail-Audit "developerstudio.elf is missing for $($architecture.Name)" }
    $ElfPaths[$architecture.Name] = $elfPath
}

$expectedFiles = @('app.json') + @($Architectures | ForEach-Object { "bin/$($_.Name)/developerstudio.elf" })
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
if (@($manifest.supportedArchitectures) -join ',' -ne 'amd64,arm64') { Fail-Audit 'manifest must advertise amd64 and arm64' }
if (@($manifest.entries).Count -ne $Architectures.Count) { Fail-Audit 'manifest must contain one executable entry per architecture' }
foreach ($architecture in $Architectures) {
    $entry = @($manifest.entries | Where-Object { $_.architecture -eq $architecture.Name })
    if ($entry.Count -ne 1 -or $entry[0].path -ne "bin/$($architecture.Name)/developerstudio.elf" -or
        $entry[0].entryPoint -ne 'gx_main' -or $entry[0].abi -ne 'guidexos-c-abi-v1' -or $entry[0].runtime -ne 'native-elf') {
        Fail-Audit "manifest executable entry does not match the packaged $($architecture.Name) ELF"
    }
}

$readElf = Find-ReadElf
if (-not $readElf) { Fail-Audit 'llvm-readelf/readelf is required for ELF package validation' }
foreach ($architecture in $Architectures) {
    $elfPath = $ElfPaths[$architecture.Name]
    $header = (& $readElf -h $elfPath 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { Fail-Audit "readelf -h failed for $($architecture.Name)" }
    if ($header -notmatch 'Class:\s+ELF64') { Fail-Audit "$($architecture.Name) ELF class is not ELF64" }
    if ($header -notmatch 'Data:\s+2''s complement, little endian') { Fail-Audit "$($architecture.Name) ELF endianness is not little endian" }
    if ($header -notmatch 'Type:\s+EXEC') { Fail-Audit "$($architecture.Name) ELF type is not ET_EXEC" }
    if ($header -notmatch "Machine:\s+$([regex]::Escape($architecture.Machine))") { Fail-Audit "$($architecture.Name) ELF architecture is not $($architecture.Machine)" }
    $sections = (& $readElf -S $elfPath 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { Fail-Audit "readelf -S failed for $($architecture.Name)" }
    $hasSymtab = $sections -match '(?m)\]\s+\.symtab\s'
    $debugSectionMatches = @([regex]::Matches($sections, '(?m)\]\s+(\.debug_[^\s]+)') | ForEach-Object { $_.Groups[1].Value })
    if (-not $hasSymtab) { Fail-Audit "$($architecture.Name) production ELF does not contain .symtab" }
    if (-not $AllowDebugSections -and $debugSectionMatches.Count -ne 0) {
        Fail-Audit "$($architecture.Name) production ELF contains debug sections: $($debugSectionMatches -join ', ')"
    }

    $file = Get-Item -LiteralPath $elfPath
    $hash = (Get-FileHash -LiteralPath $elfPath -Algorithm SHA256).Hash
    Write-Host "package_path=$elfPath"
    Write-Host "package_size=$($file.Length)"
    Write-Host "package_sha256=$hash"
    Write-Host "elf_class=ELF64"
    Write-Host "elf_endian=little"
    Write-Host "elf_architecture=$($architecture.Name)"
    Write-Host "elf_type=ET_EXEC"
    Write-Host "elf_entry_point=$(([regex]::Match($header, 'Entry point address:\s+(\S+)')).Groups[1].Value)"
    Write-Host "elf_symtab=$([bool]$hasSymtab)"
    Write-Host "elf_debug_sections=$([bool]($debugSectionMatches.Count -ne 0))"
}
Write-Host "package_files=$($actualFiles -join ',')"
Write-Host 'package_content_audit=PASS'
