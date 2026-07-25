[CmdletBinding()]
param(
    [string]$ServerRoot = "D:\dev\guideXOSServerV0.5_DEVELOPER_STUDIO"
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$ServerRoot = [IO.Path]::GetFullPath($ServerRoot)
$TempRoot = Join-Path $ServerRoot "tmp"
$Fixture = Join-Path $TempRoot ("developer-studio-workflow-" + [Guid]::NewGuid().ToString("N"))
$TestBinary = Join-Path $TempRoot "developer-studio-workflow-test.exe"

function Assert-True([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw "Developer Studio workflow smoke failed: $Message" }
    Write-Host "PASS: $Message"
}

New-Item -ItemType Directory -Force -Path $TempRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Fixture "subdirectory") | Out-Null
try {
    [IO.File]::WriteAllText((Join-Path $Fixture "sample.cpp"), "int main() { return 0; }`n")
    [IO.File]::WriteAllText((Join-Path $Fixture "notes.txt"), "notes`n")
    [IO.File]::WriteAllText((Join-Path $Fixture "subdirectory\config.json"), "{`"ok`":true}`n")
    [IO.File]::WriteAllBytes((Join-Path $Fixture "binary.dat"), [byte[]](0x41, 0x00, 0x42, 0xFF))
    [IO.File]::WriteAllText((Join-Path $Fixture "oversized.txt"), ("x" * (256 * 1024 + 1)))

    & g++ -std=c++17 -Wall -Wextra -pedantic -Isrc src\developer_studio_models.cpp src\developer_studio_workspace.cpp tests\workflow_test.cpp -o $TestBinary
    Assert-True ($LASTEXITCODE -eq 0) "workflow test compiles"
    $workflowOutput = (& $TestBinary $Fixture 2>&1 | Out-String)
    Write-Host $workflowOutput
    $workflowExit = $LASTEXITCODE
    Write-Host ("workflow exit=" + $workflowExit)
    Assert-True ($workflowExit -eq 0) "workspace workflow smoke passes"
} finally {
    if ([IO.Path]::GetFullPath($Fixture).StartsWith([IO.Path]::GetFullPath($TempRoot), [StringComparison]::OrdinalIgnoreCase)) {
        if (Test-Path -LiteralPath $Fixture) { Remove-Item -LiteralPath $Fixture -Recurse -Force }
    }
    if (Test-Path -LiteralPath $TestBinary) { Remove-Item -LiteralPath $TestBinary -Force }
}
