[CmdletBinding()]
param(
    [string]$LogRoot
)

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error 'PowerShell 7 or newer is required.'
    exit 1
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$gateScript = Join-Path $PSScriptRoot 'run-full-repository-gate.ps1'
$gateStarter = Join-Path $PSScriptRoot 'run-full-repository-gate.cmd'
$testRoot = if ($LogRoot) { [IO.Path]::GetFullPath($LogRoot) } else { Join-Path (Join-Path $repoRoot '.Temp\buildlogs') ('regression-' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }

function Assert-Condition {
    param([bool]$Condition, [string]$Message)

    if (-not $Condition) { throw $Message }
}

function Invoke-GateSelfTest {
    param([ValidateSet('Pass', 'Fail', 'Throw')][string]$Mode, [int]$ExpectedExitCode)

    $modeLogRoot = Join-Path $testRoot $Mode
    $output = & pwsh.exe -NoProfile -File $gateScript -SelfTest $Mode -LogRoot $modeLogRoot 2>&1 | Out-String
    $actualExitCode = $LASTEXITCODE
    Assert-Condition -Condition ($actualExitCode -eq $ExpectedExitCode) -Message "Self-test $Mode returned $actualExitCode instead of $ExpectedExitCode. Output: $output"
    return $output
}

function Write-MatrixFixture {
    param([string]$Name, [string[]]$Environments)

    $fixtureDirectory = Join-Path (Join-Path $matrixRoot 'examples') $Name
    New-Item -ItemType Directory -Path $fixtureDirectory -Force | Out-Null
    $content = $Environments | ForEach-Object { "[env:$_]" }
    Set-Content -LiteralPath (Join-Path $fixtureDirectory 'platformio.ini') -Value $content -Encoding utf8
}

New-Item -ItemType Directory -Path $testRoot -Force | Out-Null

$matrixRoot = Join-Path $testRoot 'matrix-fixtures'
Write-MatrixFixture -Name 'ota-preferred' -Environments @('usb', 'ota', 'eth')
Write-MatrixFixture -Name 'usb-fallback' -Environments @('eth', 'usb')
Write-MatrixFixture -Name 'first-declared' -Environments @('eth', 'wifi')
$matrixOutput = & pwsh.exe -NoProfile -File $gateScript -DiscoverOnly -MatrixRoot $matrixRoot -LogRoot (Join-Path $testRoot 'matrix-discovery') 2>&1 | Out-String
Assert-Condition -Condition ($LASTEXITCODE -eq 0) -Message "Matrix discovery failed. Output: $matrixOutput"
Assert-Condition -Condition ($matrixOutput -match 'Example ota-preferred: selected environment ota \(preferred\)') -Message 'The gate did not prefer ota.'
Assert-Condition -Condition ($matrixOutput -match 'Example usb-fallback: selected environment usb \(fallback\)') -Message 'The gate did not select usb when ota was absent.'
Assert-Condition -Condition ($matrixOutput -match 'Example first-declared: selected environment eth \(first declared environment\)') -Message 'The gate did not select the first declared environment.'

$passOutput = Invoke-GateSelfTest -Mode Pass -ExpectedExitCode 0
Assert-Condition -Condition ($passOutput -match 'Full repository gate result: PASS') -Message 'Successful self-test did not report PASS.'
Assert-Condition -Condition ($passOutput -notmatch 'NOT_RUN') -Message 'Successful self-test unexpectedly skipped a check.'

$failOutput = Invoke-GateSelfTest -Mode Fail -ExpectedExitCode 1
Assert-Condition -Condition ($failOutput -match 'Full repository gate result: FAILED') -Message 'Failing self-test did not report FAILED.'
Assert-Condition -Condition ($failOutput -notmatch 'Full repository gate result: PASS') -Message 'Failing self-test reported PASS.'
Assert-Condition -Condition ($failOutput -match 'Simulated follow-up check\s+NOT_RUN') -Message 'Failing self-test did not mark the follow-up check NOT_RUN.'

$throwOutput = Invoke-GateSelfTest -Mode Throw -ExpectedExitCode 1
Assert-Condition -Condition ($throwOutput -match 'Full repository gate result: FAILED') -Message 'Exception self-test did not report FAILED.'
Assert-Condition -Condition ($throwOutput -match 'Gate exception\s+FAILED') -Message 'Exception self-test did not record the thrown PowerShell exception.'

$cmdLogRoot = Join-Path $testRoot 'cmd'
$cmdCommand = '(echo.| "{0}" -SelfTest Fail -LogRoot "{1}")' -f $gateStarter, $cmdLogRoot
$cmdOutput = & cmd.exe /d /c $cmdCommand 2>&1 | Out-String
$cmdExitCode = $LASTEXITCODE
Assert-Condition -Condition ($cmdExitCode -ne 0) -Message "CMD starter returned 0 for a failing gate. Output: $cmdOutput"
Assert-Condition -Condition ($cmdOutput -match '\[E\] Full repository gate failed with exit code') -Message 'CMD starter did not report a gate failure.'
Assert-Condition -Condition ($cmdOutput -notmatch '\[I\] Full repository gate completed successfully\.') -Message 'CMD starter reported success after a failing gate.'

Write-Output "Repository gate regression tests passed. Logs: $testRoot"
