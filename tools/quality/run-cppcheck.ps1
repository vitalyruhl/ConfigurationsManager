[CmdletBinding()]
param()

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error "PowerShell 7 or newer is required. Current version: $($PSVersionTable.PSVersion). Run this script with pwsh.exe."
    exit 1
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$sourceRoots = @('src', 'include', 'test') |
    ForEach-Object { Join-Path $repoRoot $_ } |
    Where-Object { Test-Path -LiteralPath $_ }

if (-not (Get-Command cppcheck -ErrorAction SilentlyContinue)) {
    Write-Error 'cppcheck is required for the full repository gate. Install it for the current environment or use the CI workflow.'
    exit 1
}

$arguments = @(
    '--enable=warning,style,performance,portability',
    '--error-exitcode=1',
    '--inline-suppr',
    '--suppress=missingIncludeSystem',
    '--suppress=unusedFunction',
    '--std=c++17',
    '--language=c++',
    '-DPROGMEM=',
    '-DARDUINO_ARCH_ESP32',
    # Match the pinned ESP32Async/ESPAsyncWebServer dependency for analysis.
    '-DASYNCWEBSERVER_FORK_ESP32Async=1',
    '-DASYNCWEBSERVER_VERSION_NUM=0x030C01',
    '-DASYNCWEBSERVER_USE_MUTEX=1'
) + $sourceRoots

& cppcheck @arguments
exit $LASTEXITCODE
