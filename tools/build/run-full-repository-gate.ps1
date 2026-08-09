[CmdletBinding()]
param(
    [switch]$DiscoverOnly,
    [switch]$FullMatrix,
    [switch]$FailFast,
    [string]$LogRoot,
    [string]$MatrixRoot,
    [ValidateSet('Pass', 'Fail', 'Throw')]
    [string]$SelfTest
)

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error "PowerShell 7 or newer is required. Current version: $($PSVersionTable.PSVersion). Run this script with pwsh.exe."
    exit 1
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$platformIoRoot = if ($MatrixRoot) { (Resolve-Path -LiteralPath $MatrixRoot).Path } else { $repoRoot }
$temporaryRoot = if ($LogRoot) { [IO.Path]::GetFullPath($LogRoot) } else { Join-Path $repoRoot '.Temp\buildlogs' }
$env:PYTHONPYCACHEPREFIX = Join-Path $repoRoot '.Temp\pycache'
$logDirectory = Join-Path $temporaryRoot ("ConfigurationsManager-full-gate-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$results = [System.Collections.Generic.List[object]]::new()
$plannedChecks = [System.Collections.Generic.List[string]]::new()
$checkNumber = 0
$gateExitCode = 0
$locationPushed = $false

function Add-Result {
    param([string]$Name, [string]$Status, [string]$Detail, [double]$Seconds, [string]$LogPath)

    $results.Add([PSCustomObject]@{
            Name = $Name
            Status = $Status
            Detail = $Detail
            Seconds = [Math]::Round($Seconds, 2)
            LogPath = $LogPath
        })
}

function Get-SafeLogName {
    param([string]$Name)

    $invalidCharacters = [IO.Path]::GetInvalidFileNameChars() + [char[]]'[]:/'
    $safeName = $Name
    foreach ($character in $invalidCharacters) {
        $safeName = $safeName.Replace([string]$character, '-')
    }
    return ($safeName -replace '\s+', '-').Trim('-').ToLowerInvariant()
}

function Invoke-GateCheck {
    param([string]$Name, [scriptblock]$Action)

    $script:checkNumber++
    $logPath = Join-Path $logDirectory ('{0:D2}-{1}.log' -f $script:checkNumber, (Get-SafeLogName -Name $Name))
    $started = Get-Date
    $global:LASTEXITCODE = 0
    try {
        & $Action 2>&1 | Tee-Object -FilePath $logPath | Out-Host
        $nativeExitCode = $LASTEXITCODE
        if ($nativeExitCode -ne 0) {
            throw "Native command failed with exit code $nativeExitCode"
        }
        Add-Result -Name $Name -Status 'PASS' -Detail '' -Seconds ((Get-Date) - $started).TotalSeconds -LogPath $logPath
        return $true
    }
    catch {
        $message = $_.Exception.Message
        Add-Content -LiteralPath $logPath -Value "`nFAILED: $message"
        Add-Result -Name $Name -Status 'FAILED' -Detail $message -Seconds ((Get-Date) - $started).TotalSeconds -LogPath $logPath
        Write-Host "CHECK FAILED: $Name. Log: $logPath. Reason: $message"
        return $false
    }
}

function Add-NotRunResults {
    foreach ($name in $plannedChecks) {
        if (@($results | Where-Object { $_.Name -eq $name }).Count -eq 0) {
            Add-Result -Name $name -Status 'NOT_RUN' -Detail 'Skipped by fail-fast after a mandatory check failure.' -Seconds 0 -LogPath ''
        }
    }
}

function Write-Summary {
    param([int]$ExitCode)

    if ($ExitCode -ne 0) {
        Add-NotRunResults
    }
    $nonPassingResultCount = @($results | Where-Object { $_.Status -ne 'PASS' }).Count
    $overall = if ($ExitCode -eq 0 -and $nonPassingResultCount -eq 0 -and $results.Count -eq $plannedChecks.Count) { 'PASS' } else { 'FAILED' }
    Write-Output ''
    Write-Output "Full repository gate logs: $logDirectory"
    Write-Output "Full repository gate result: $overall"
    Write-Output "Exit code: $ExitCode"
    $failedResults = @($results | Where-Object { $_.Status -eq 'FAILED' })
    if ($failedResults.Count -gt 0) {
        Write-Output 'Failure details:'
        foreach ($result in $failedResults) {
            Write-Output "  [E] $($result.Name): $($result.Detail)"
            if ($result.LogPath) { Write-Output "      Log: $($result.LogPath)" }
        }
    }
    $results | Format-Table Name, Status, Seconds, LogPath, Detail -AutoSize
}

function Get-PlatformIoMatrix {
    $excludedSegment = '[\\/](?:\.git|\.pio|\.Temp|node_modules|dist|libdeps)[\\/]'
    $projects = Get-ChildItem -Path $platformIoRoot -Filter 'platformio.ini' -File -Recurse |
        Where-Object { $MatrixRoot -or $_.FullName -notmatch $excludedSegment } |
        Sort-Object FullName
    $matrix = foreach ($project in $projects) {
        $environments = @(Select-String -Path $project.FullName -Pattern '^\s*\[env:([^\]]+)\]' |
                ForEach-Object { $_.Matches[0].Groups[1].Value })
        if ($environments.Count -eq 0) { continue }
        $selectedEnvironments = if ($FullMatrix) {
            $environments | ForEach-Object { [PSCustomObject]@{ Environment = $_; SelectionReason = 'full matrix mode' } }
        }
        elseif ($environments -contains 'ota') {
            @([PSCustomObject]@{ Environment = ($environments | Where-Object { $_ -eq 'ota' } | Select-Object -First 1); SelectionReason = 'preferred' })
        }
        elseif ($environments -contains 'usb') {
            @([PSCustomObject]@{ Environment = ($environments | Where-Object { $_ -eq 'usb' } | Select-Object -First 1); SelectionReason = 'fallback' })
        }
        else {
            @([PSCustomObject]@{ Environment = $environments[0]; SelectionReason = 'first declared environment' })
        }
        foreach ($selection in $selectedEnvironments) {
            [PSCustomObject]@{
                ProjectDirectory = $project.Directory.FullName
                Environment = $selection.Environment
                SelectionReason = $selection.SelectionReason
                HasEmbeddedTests = Test-Path -LiteralPath (Join-Path $project.Directory.FullName 'test')
            }
        }
    }
    if (-not $matrix) { throw 'No supported PlatformIO environments were discovered.' }
    return @($matrix)
}

function Test-PowerShellSyntax {
    Get-ChildItem -Path (Join-Path $repoRoot 'tools') -Filter '*.ps1' -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            $tokens = $null
            $errors = $null
            [System.Management.Automation.Language.Parser]::ParseFile($_.FullName, [ref]$tokens, [ref]$errors) | Out-Null
            if ($errors.Count -gt 0) { throw "PowerShell syntax error in $($_.FullName): $(($errors | ForEach-Object Message) -join '; ')" }
        }
}

function Test-WorkflowStructure {
    $workflows = Get-ChildItem -Path (Join-Path $repoRoot '.github\workflows') -File |
        Where-Object { $_.Extension -in @('.yml', '.yaml') }
    if (-not $workflows) { throw 'No GitHub Actions workflows were found.' }
    foreach ($workflow in $workflows) {
        $content = Get-Content -LiteralPath $workflow.FullName -Raw
        foreach ($requiredKey in @('name:', 'on:', 'jobs:')) {
            if ($content -notmatch "(?m)^$([regex]::Escape($requiredKey))") { throw "Workflow $($workflow.Name) is missing top-level $requiredKey" }
        }
    }
}

function Test-IgnoredTemporaryContent {
    if (@(git -C $repoRoot ls-files -- .Temp).Count -gt 0) { throw '.Temp contains tracked files.' }
    git -C $repoRoot check-ignore -q .Temp
    if ($LASTEXITCODE -ne 0) { throw '.Temp is not ignored.' }
}

function Test-WorkingTreeStable {
    param([string[]]$Baseline)

    $difference = Compare-Object -ReferenceObject $Baseline -DifferenceObject @(git -C $repoRoot status --porcelain=v1)
    if ($difference) { throw "Validation changed the working tree or index: $(($difference | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }) -join '; ')" }
}

function Add-Plan {
    param([string[]]$Names)
    foreach ($name in $Names) { [void]$plannedChecks.Add($name) }
}

function Run-RequiredCheck {
    param([string]$Name, [scriptblock]$Action)
    if (-not (Invoke-GateCheck -Name $Name -Action $Action)) {
        $script:gateExitCode = 1
        return $false
    }
    return $true
}

try {
    if ($DiscoverOnly -and $SelfTest) { throw '-DiscoverOnly and -SelfTest cannot be combined.' }
    if ($MatrixRoot -and -not $DiscoverOnly) { throw '-MatrixRoot is only supported with -DiscoverOnly.' }
    New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
    New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
    Push-Location $repoRoot
    $locationPushed = $true

    if ($SelfTest) {
        Add-Plan @('Simulated first mandatory check', 'Simulated follow-up check')
        switch ($SelfTest) {
            'Pass' {
                if (Run-RequiredCheck -Name 'Simulated first mandatory check' -Action { Write-Output 'Simulated pass.' }) {
                    Run-RequiredCheck -Name 'Simulated follow-up check' -Action { Write-Output 'Simulated pass.' } | Out-Null
                }
            }
            'Fail' { Run-RequiredCheck -Name 'Simulated first mandatory check' -Action { throw 'Simulated mandatory check failure.' } | Out-Null }
            'Throw' { throw 'Simulated PowerShell exception.' }
        }
    } else {
        $matrix = Get-PlatformIoMatrix
        $embeddedTestTargets = @($matrix | Where-Object HasEmbeddedTests)
        $webUiTests = @(Get-ChildItem -Path (Join-Path $repoRoot 'webui\test') -Filter '*.test.mjs' -File -ErrorAction SilentlyContinue | Sort-Object FullName)
        Add-Plan @('Format and precommit checks', 'Cppcheck', 'Working tree after static checks', 'Temporary content policy', 'PowerShell syntax', 'Agent governance generator', 'Serena memory generator', 'Workflow structure')
        foreach ($target in $embeddedTestTargets) {
            $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
            Add-Plan "PlatformIO test compile $relativeDirectory [$($target.Environment)]"
        }
        foreach ($target in $matrix) {
            $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
            Add-Plan "PlatformIO build $relativeDirectory [$($target.Environment)]"
        }
        if (Test-Path -LiteralPath (Join-Path $repoRoot 'webui\package.json')) {
            Add-Plan 'WebUI production build'
            if ($webUiTests.Count -gt 0) { Add-Plan 'WebUI Node tests' }
        }

        Write-Output 'Discovered PlatformIO build matrix:'
        $matrix | ForEach-Object {
            $relativeDirectory = [IO.Path]::GetRelativePath($platformIoRoot, $_.ProjectDirectory).Replace('\', '/')
            if ($relativeDirectory -like 'examples/*') {
                $targetName = "Example $($relativeDirectory.Substring('examples/'.Length))"
            }
            else {
                $targetName = "Project $relativeDirectory"
            }
            Write-Output "${targetName}: selected environment $($_.Environment) ($($_.SelectionReason))"
        }
        Write-Output "Discovered embedded test targets: $($embeddedTestTargets.Count)"
        Write-Output "Discovered WebUI tests: $($webUiTests.Count)"
        Write-Output "Full repository gate logs: $logDirectory"
        if ($DiscoverOnly) { $gateExitCode = 0 } else {
            $workingTreeBaseline = @(git -C $repoRoot status --porcelain=v1)
            [IO.File]::WriteAllLines((Join-Path $logDirectory '00-working-tree-baseline.log'), $workingTreeBaseline, [Text.UTF8Encoding]::new($false))
            if (Run-RequiredCheck -Name 'Format and precommit checks' -Action { & (Join-Path $repoRoot 'tools\quality\run-precommit-full.ps1') }) {
                if (Run-RequiredCheck -Name 'Cppcheck' -Action { & (Join-Path $repoRoot 'tools\quality\run-cppcheck.ps1') }) {
                    Run-RequiredCheck -Name 'Working tree after static checks' -Action { Test-WorkingTreeStable -Baseline $workingTreeBaseline } | Out-Null
                }
            }
            if ($gateExitCode -eq 0 -and (Run-RequiredCheck -Name 'Temporary content policy' -Action { Test-IgnoredTemporaryContent })) {
                if (Run-RequiredCheck -Name 'PowerShell syntax' -Action { Test-PowerShellSyntax }) {
                    if (Run-RequiredCheck -Name 'Agent governance generator' -Action { & (Join-Path $repoRoot 'tools\governance\combine-agent-md.ps1') }) {
                        if (Run-RequiredCheck -Name 'Serena memory generator' -Action { & (Join-Path $repoRoot 'tools\governance\serena\combine-serena-shared.ps1') }) {
                            Run-RequiredCheck -Name 'Workflow structure' -Action { Test-WorkflowStructure } | Out-Null
                        }
                    }
                }
            }
            if ($gateExitCode -eq 0) {
                if (-not (Get-Command pio -ErrorAction SilentlyContinue)) { throw 'PlatformIO CLI `pio` is required for the full repository gate.' }
                foreach ($target in $embeddedTestTargets) {
                    $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
                    if (-not (Run-RequiredCheck -Name "PlatformIO test compile $relativeDirectory [$($target.Environment)]" -Action { pio test -d $target.ProjectDirectory -e $target.Environment --without-uploading --without-testing })) { break }
                }
            }
            if ($gateExitCode -eq 0) {
                foreach ($target in $matrix) {
                    $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
                    if (-not (Run-RequiredCheck -Name "PlatformIO build $relativeDirectory [$($target.Environment)]" -Action { pio run -d $target.ProjectDirectory -e $target.Environment })) { break }
                }
            }
            if ($gateExitCode -eq 0 -and (Test-Path -LiteralPath (Join-Path $repoRoot 'webui\package.json'))) {
                if (-not (Get-Command npm -ErrorAction SilentlyContinue) -or -not (Get-Command node -ErrorAction SilentlyContinue)) { throw 'Node.js and npm are required for WebUI validation.' }
                $webUiArchive = Join-Path $logDirectory 'webui-validation.zip'
                $webUiValidationRoot = Join-Path $logDirectory 'webui-validation'
                if (Run-RequiredCheck -Name 'WebUI production build' -Action {
                        git -C $repoRoot archive --format=zip --output=$webUiArchive HEAD webui
                        Expand-Archive -LiteralPath $webUiArchive -DestinationPath $webUiValidationRoot -Force
                        $archivedWebUi = Join-Path $webUiValidationRoot 'webui'
                        npm --prefix $archivedWebUi pkg set allowScripts.esbuild=true
                        npm --prefix $archivedWebUi ci
                        npm --prefix $archivedWebUi run build
                    }) {
                    if ($webUiTests.Count -gt 0) { Run-RequiredCheck -Name 'WebUI Node tests' -Action { node --test $webUiTests.FullName } | Out-Null }
                }
            }
        }
    }
}
catch {
    $gateExitCode = 1
    $exceptionLog = Join-Path $logDirectory '00-gate-exception.log'
    if (Test-Path -LiteralPath $logDirectory) { Set-Content -LiteralPath $exceptionLog -Value $_.Exception.ToString() -Encoding utf8 }
    Add-Result -Name 'Gate exception' -Status 'FAILED' -Detail $_.Exception.Message -Seconds 0 -LogPath $exceptionLog
    Write-Output "GATE EXCEPTION: $($_.Exception.Message)"
}
finally {
    if ($locationPushed) { Pop-Location }
    if (-not $DiscoverOnly) { Write-Summary -ExitCode $gateExitCode }
}

exit $gateExitCode
