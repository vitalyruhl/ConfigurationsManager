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

$clangFormat = Get-Command clang-format-18 -ErrorAction SilentlyContinue | Select-Object -First 1
$localClangFormat = Join-Path $repoRoot '.Temp/ci-clang-format/clang-format.exe'
$clangFormatPath = if ($clangFormat) { $clangFormat.Source }
elseif (Test-Path -LiteralPath $localClangFormat -PathType Leaf) { $localClangFormat }
else {
    $clangFormat = Get-Command clang-format -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($clangFormat) { $clangFormat.Source } else { $null }
}
if (-not $clangFormatPath) {
    Write-Error 'clang-format 18.1.3 is required for the full repository gate.'
    exit 1
}
$clangFormatVersion = & $clangFormatPath --version
if ($LASTEXITCODE -ne 0 -or $clangFormatVersion -notmatch '\bversion 18\.1\.3\b') {
    Write-Error "clang-format 18.1.3 is required to match CI. Found: $clangFormatVersion"
    exit 1
}
Write-Output "Using $clangFormatVersion"

if ($sourceFiles.Count -gt 0) {
    & $clangFormatPath --style=file --fallback-style=none --dry-run --Werror $sourceFiles.FullName
    exit $LASTEXITCODE
}

Write-Output 'No ConfigManager C/C++ source files require formatting validation.'
