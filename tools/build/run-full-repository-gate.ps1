[CmdletBinding()]
param(
    [switch]$DiscoverOnly,
    [switch]$FailFast
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$isWindowsHost = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
$temporaryRoot = if ($isWindowsHost) { 'C:\Temp' } else { [IO.Path]::GetTempPath() }
$logDirectory = Join-Path $temporaryRoot ("ConfigurationsManager-full-gate-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$results = [System.Collections.Generic.List[object]]::new()

function Add-Result {
    param(
        [string]$Name,
        [string]$Status,
        [string]$Detail,
        [double]$Seconds
    )

    $results.Add([PSCustomObject]@{
            Name = $Name
            Status = $Status
            Detail = $Detail
            Seconds = $Seconds
        })
}

function Invoke-GateCheck {
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    $started = Get-Date
    try {
        & $Action
        if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {
            throw "Native command failed with exit code $LASTEXITCODE"
        }
        Add-Result -Name $Name -Status 'pass' -Detail '' -Seconds ((Get-Date) - $started).TotalSeconds
    }
    catch {
        Add-Result -Name $Name -Status 'fail' -Detail $_.Exception.Message -Seconds ((Get-Date) - $started).TotalSeconds
        Write-Error "$Name failed: $($_.Exception.Message)"
        if ($FailFast) {
            throw
        }
    }
    finally {
        $global:LASTEXITCODE = 0
    }
}

function Get-PlatformIoMatrix {
    $excludedSegment = '[\\/](?:\.git|\.pio|\.Temp|node_modules|dist)[\\/]'
    $projects = Get-ChildItem -Path $repoRoot -Filter 'platformio.ini' -File -Recurse |
        Where-Object { $_.FullName -notmatch $excludedSegment } |
        Sort-Object FullName

    $matrix = foreach ($project in $projects) {
        $environments = Select-String -Path $project.FullName -Pattern '^\[env:([^\]]+)\]' |
            ForEach-Object { $_.Matches[0].Groups[1].Value }
        foreach ($environment in $environments) {
            [PSCustomObject]@{
                ProjectDirectory = $project.Directory.FullName
                ProjectFile = $project.FullName
                Environment = $environment
                HasEmbeddedTests = Test-Path -LiteralPath (Join-Path $project.Directory.FullName 'test')
            }
        }
    }

    if (-not $matrix) {
        throw 'No supported PlatformIO environments were discovered.'
    }
    return @($matrix)
}

function Test-PowerShellSyntax {
    $scripts = Get-ChildItem -Path (Join-Path $repoRoot 'tools') -Filter '*.ps1' -File -Recurse |
        Sort-Object FullName
    foreach ($script in $scripts) {
        $tokens = $null
        $errors = $null
        [System.Management.Automation.Language.Parser]::ParseFile(
            $script.FullName,
            [ref]$tokens,
            [ref]$errors
        ) | Out-Null
        if ($errors.Count -gt 0) {
            $message = ($errors | ForEach-Object { $_.Message }) -join '; '
            throw "PowerShell syntax error in $($script.FullName): $message"
        }
    }
}

function Test-WorkflowStructure {
    $workflowDirectory = Join-Path $repoRoot '.github\workflows'
    $workflows = Get-ChildItem -Path $workflowDirectory -File |
        Where-Object { $_.Extension -in @('.yml', '.yaml') }
    if (-not $workflows) {
        throw "No GitHub Actions workflows found in $workflowDirectory"
    }
    foreach ($workflow in $workflows) {
        $content = Get-Content -LiteralPath $workflow.FullName -Raw
        foreach ($requiredKey in @('name:', 'on:', 'jobs:')) {
            if ($content -notmatch "(?m)^$([regex]::Escape($requiredKey))") {
                throw "Workflow $($workflow.Name) is missing top-level $requiredKey"
            }
        }
    }
}

function Test-IgnoredTemporaryContent {
    $tracked = @(git -C $repoRoot ls-files -- .Temp)
    if ($tracked.Count -gt 0) {
        throw '.Temp contains tracked files.'
    }
    git -C $repoRoot check-ignore -q .Temp
    if ($LASTEXITCODE -ne 0) {
        throw '.Temp is not ignored.'
    }
}

New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
Push-Location $repoRoot
try {
    $matrix = Get-PlatformIoMatrix
    Write-Output 'Discovered PlatformIO build matrix:'
    $matrix | ForEach-Object {
        $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $_.ProjectDirectory).Replace('\', '/')
        Write-Output "  $relativeDirectory [$($_.Environment)]"
    }

    $embeddedTestTargets = @($matrix | Where-Object HasEmbeddedTests)
    $webUiTests = @(Get-ChildItem -Path (Join-Path $repoRoot 'webui\test') -Filter '*.test.mjs' -File -ErrorAction SilentlyContinue |
            Sort-Object FullName)
    Write-Output "Discovered embedded test targets: $($embeddedTestTargets.Count)"
    Write-Output "Discovered WebUI tests: $($webUiTests.Count)"
    Write-Output "Logs: $logDirectory"

    if ($DiscoverOnly) {
        exit 0
    }

    Invoke-GateCheck -Name 'Git diff check' -Action { git diff --check }
    Invoke-GateCheck -Name 'Temporary content policy' -Action { Test-IgnoredTemporaryContent }
    Invoke-GateCheck -Name 'PowerShell syntax' -Action { Test-PowerShellSyntax }
    Invoke-GateCheck -Name 'Agent governance generator' -Action {
        & (Join-Path $repoRoot 'tools\governance\combine-agent-md.ps1')
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot '.Temp\all-agents-combined.md'))) {
            throw 'Agent governance generator did not produce its output.'
        }
    }
    Invoke-GateCheck -Name 'Serena memory generator' -Action {
        & (Join-Path $repoRoot 'tools\governance\serena\combine-serena-shared.ps1')
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot '.Temp\serena-memory.md'))) {
            throw 'Serena memory generator did not produce its output.'
        }
    }
    Invoke-GateCheck -Name 'Workflow structure' -Action { Test-WorkflowStructure }

    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
        throw 'PlatformIO CLI `pio` is required for the full repository gate.'
    }
    foreach ($target in $matrix) {
        $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
        Invoke-GateCheck -Name "PlatformIO build $relativeDirectory [$($target.Environment)]" -Action {
            pio run -d $target.ProjectDirectory -e $target.Environment
        }
    }
    foreach ($target in $embeddedTestTargets) {
        $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
        Invoke-GateCheck -Name "PlatformIO test compile $relativeDirectory [$($target.Environment)]" -Action {
            pio test -d $target.ProjectDirectory -e $target.Environment --without-uploading --without-testing
        }
    }

    $webUiDirectory = Join-Path $repoRoot 'webui'
    if (Test-Path -LiteralPath (Join-Path $webUiDirectory 'package.json')) {
        if (-not (Get-Command npm -ErrorAction SilentlyContinue) -or -not (Get-Command node -ErrorAction SilentlyContinue)) {
            throw 'Node.js and npm are required for WebUI validation.'
        }
        $webUiArchive = Join-Path $logDirectory 'webui-validation.zip'
        $webUiValidationRoot = Join-Path $logDirectory 'webui-validation'
        Invoke-GateCheck -Name 'WebUI production build' -Action {
            git -C $repoRoot archive --format=zip --output=$webUiArchive HEAD webui
            Expand-Archive -LiteralPath $webUiArchive -DestinationPath $webUiValidationRoot -Force
            $archivedWebUi = Join-Path $webUiValidationRoot 'webui'
            npm --prefix $archivedWebUi pkg set allowScripts.esbuild=true
            npm --prefix $archivedWebUi ci
            npm --prefix $archivedWebUi run build
        }
        if ($webUiTests.Count -gt 0) {
            Invoke-GateCheck -Name 'WebUI Node tests' -Action { node --test $webUiTests.FullName }
        }
    }

    Write-Output ''
    Write-Output 'Full repository gate summary:'
    $results | Format-Table -AutoSize
    if (($results | Where-Object Status -eq 'fail').Count -gt 0) {
        exit 1
    }
}
finally {
    Pop-Location
}
