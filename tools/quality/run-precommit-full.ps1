[CmdletBinding()]
param()

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error "PowerShell 7 or newer is required. Current version: $($PSVersionTable.PSVersion). Run this script with pwsh.exe."
    exit 1
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$excludedSegment = '[\\/](?:\.git|\.pio|\.Temp|node_modules|libdeps|dist)[\\/]'
$sourceRoots = @('src', 'include', 'test', 'examples') |
    ForEach-Object { Join-Path $repoRoot $_ } |
    Where-Object { Test-Path -LiteralPath $_ }
$sourceFiles = foreach ($sourceRoot in $sourceRoots) {
    Get-ChildItem -Path $sourceRoot -File -Recurse |
        Where-Object {
            $_.FullName -notmatch $excludedSegment -and
            $_.Extension -in @('.c', '.cc', '.cpp', '.h', '.hpp') -and
            $_.Name -ne 'html_content.h'
        }
}

git -C $repoRoot diff --check
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$preCommitConfig = Join-Path $repoRoot '.pre-commit-config.yaml'
if (Test-Path -LiteralPath $preCommitConfig) {
    if (-not (Get-Command pre-commit -ErrorAction SilentlyContinue)) {
        Write-Error 'pre-commit is configured but the executable is missing.'
        exit 1
    }
    & pre-commit run --all-files
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
} else {
    Write-Output 'No repository pre-commit configuration found; running whitespace and clang-format checks.'
}

$clangFormat = Get-Command clang-format, clang-format-18 -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $clangFormat) {
    Write-Error 'clang-format is required for the full repository gate. Install it for the current environment or use the CI workflow.'
    exit 1
}

if ($sourceFiles.Count -gt 0) {
    & $clangFormat.Source --dry-run --Werror $sourceFiles.FullName
    exit $LASTEXITCODE
}

Write-Output 'No ConfigManager C/C++ source files require formatting validation.'
