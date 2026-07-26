[CmdletBinding()]
param(
    [switch]$DiscoverOnly,
    [switch]$FailFast,
    [string]$LogRoot
)

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error "PowerShell 7 or newer is required. Current version: $($PSVersionTable.PSVersion). Run this script with pwsh.exe."
    exit 1
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$isWindowsHost = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
$temporaryRoot = if ($LogRoot) {
    [IO.Path]::GetFullPath($LogRoot)
} elseif ($isWindowsHost) {
    'C:\Temp'
} else {
    [IO.Path]::GetTempPath()
}
$logDirectory = Join-Path $temporaryRoot ("ConfigurationsManager-full-gate-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$results = [System.Collections.Generic.List[object]]::new()
$checkNumber = 0

function Add-Result {
    param(
        [string]$Name,
        [string]$Status,
        [string]$Detail,
        [double]$Seconds,
        [string]$LogPath
    )

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
    param(
        [string]$Name,
        [scriptblock]$Action
    )

    $script:checkNumber++
    $logPath = Join-Path $logDirectory ('{0:D2}-{1}.log' -f $script:checkNumber, (Get-SafeLogName -Name $Name))
    $started = Get-Date
    $LASTEXITCODE = 0
    try {
        & $Action 2>&1 | Tee-Object -FilePath $logPath
        if ($LASTEXITCODE -ne 0) {
            throw "Native command failed with exit code $LASTEXITCODE"
        }
        Add-Result -Name $Name -Status 'PASS' -Detail '' -Seconds ((Get-Date) - $started).TotalSeconds -LogPath $logPath
        return $true
    }
    catch {
        $message = $_.Exception.Message
        Add-Content -LiteralPath $logPath -Value "`nFAILED: $message"
        Add-Result -Name $Name -Status 'FAILED' -Detail $message -Seconds ((Get-Date) - $started).TotalSeconds -LogPath $logPath
        Write-Error "$Name failed. Log: $logPath. $message"
        if ($FailFast) {
            throw
        }
        return $false
    }
}

function Write-Summary {
    Write-Output ''
    Write-Output "Full repository gate logs: $logDirectory"
    Write-Output 'Full repository gate summary:'
    $results | Format-Table Name, Status, Seconds, LogPath, Detail -AutoSize
}

function Get-PlatformIoMatrix {
    $excludedSegment = '[\\/](?:\.git|\.pio|\.Temp|node_modules|dist|libdeps)[\\/]'
    $projects = Get-ChildItem -Path $repoRoot -Filter 'platformio.ini' -File -Recurse |
        Where-Object { $_.FullName -notmatch $excludedSegment } |
        Sort-Object FullName

    $matrix = foreach ($project in $projects) {
        $environments = Select-String -Path $project.FullName -Pattern '^\[env:([^\]]+)\]' |
            ForEach-Object { $_.Matches[0].Groups[1].Value }
        foreach ($environment in $environments) {
            [PSCustomObject]@{
                ProjectDirectory = $project.Directory.FullName
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
        [System.Management.Automation.Language.Parser]::ParseFile($script.FullName, [ref]$tokens, [ref]$errors) | Out-Null
        if ($errors.Count -gt 0) {
            throw "PowerShell syntax error in $($script.FullName): $(($errors | ForEach-Object Message) -join '; ')"
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

function Test-WorkingTreeStable {
    param([string[]]$Baseline)

    $current = @(git -C $repoRoot status --porcelain=v1)
    $difference = Compare-Object -ReferenceObject $Baseline -DifferenceObject $current
    if ($difference) {
        $details = ($difference | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }) -join '; '
        throw "Validation changed the working tree or index: $details"
    }
}

New-Item -ItemType Directory -Path $temporaryRoot -Force | Out-Null
New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
Push-Location $repoRoot
try {
    $matrix = Get-PlatformIoMatrix
    $embeddedTestTargets = @($matrix | Where-Object HasEmbeddedTests)
    $webUiTests = @(Get-ChildItem -Path (Join-Path $repoRoot 'webui\test') -Filter '*.test.mjs' -File -ErrorAction SilentlyContinue |
            Sort-Object FullName)

    Write-Output 'Discovered PlatformIO build matrix:'
    $matrix | ForEach-Object {
        $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $_.ProjectDirectory).Replace('\', '/')
        Write-Output "  $relativeDirectory [$($_.Environment)]"
    }
    Write-Output "Discovered embedded test targets: $($embeddedTestTargets.Count)"
    Write-Output "Discovered WebUI tests: $($webUiTests.Count)"
    Write-Output "Full repository gate logs: $logDirectory"

    if ($DiscoverOnly) {
        exit 0
    }

    $workingTreeBaseline = @(git -C $repoRoot status --porcelain=v1)
    $baselinePath = Join-Path $logDirectory '00-working-tree-baseline.log'
    [IO.File]::WriteAllLines($baselinePath, $workingTreeBaseline, [Text.UTF8Encoding]::new($false))

    $staticChecksPassed = $true
    $staticChecksPassed = (Invoke-GateCheck -Name 'Format and precommit checks' -Action {
            & (Join-Path $repoRoot 'tools\quality\run-precommit-full.ps1')
        }) -and $staticChecksPassed
    $staticChecksPassed = (Invoke-GateCheck -Name 'Cppcheck' -Action {
            & (Join-Path $repoRoot 'tools\quality\run-cppcheck.ps1')
        }) -and $staticChecksPassed
    $staticChecksPassed = (Invoke-GateCheck -Name 'Working tree after static checks' -Action {
            Test-WorkingTreeStable -Baseline $workingTreeBaseline
        }) -and $staticChecksPassed
    if (-not $staticChecksPassed) {
        Write-Summary
        exit 1
    }

    Invoke-GateCheck -Name 'Temporary content policy' -Action { Test-IgnoredTemporaryContent } | Out-Null
    Invoke-GateCheck -Name 'PowerShell syntax' -Action { Test-PowerShellSyntax } | Out-Null
    Invoke-GateCheck -Name 'Agent governance generator' -Action {
        & (Join-Path $repoRoot 'tools\governance\combine-agent-md.ps1')
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot '.Temp\all-agents-combined.md'))) {
            throw 'Agent governance generator did not produce its output.'
        }
    } | Out-Null
    Invoke-GateCheck -Name 'Serena memory generator' -Action {
        & (Join-Path $repoRoot 'tools\governance\serena\combine-serena-shared.ps1')
        if (-not (Test-Path -LiteralPath (Join-Path $repoRoot '.Temp\serena-memory.md'))) {
            throw 'Serena memory generator did not produce its output.'
        }
    } | Out-Null
    Invoke-GateCheck -Name 'Workflow structure' -Action { Test-WorkflowStructure } | Out-Null
    if (($results | Where-Object Status -eq 'FAILED').Count -gt 0) {
        Write-Summary
        exit 1
    }

    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
        throw 'PlatformIO CLI `pio` is required for the full repository gate.'
    }
    foreach ($target in $matrix) {
        $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
        Invoke-GateCheck -Name "PlatformIO build $relativeDirectory [$($target.Environment)]" -Action {
            pio run -d $target.ProjectDirectory -e $target.Environment
        } | Out-Null
    }
    foreach ($target in $embeddedTestTargets) {
        $relativeDirectory = [IO.Path]::GetRelativePath($repoRoot, $target.ProjectDirectory).Replace('\', '/')
        Invoke-GateCheck -Name "PlatformIO test compile $relativeDirectory [$($target.Environment)]" -Action {
            # Compile real Unity test sources without upload or hardware execution.
            pio test -d $target.ProjectDirectory -e $target.Environment --without-uploading --without-testing
        } | Out-Null
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
        } | Out-Null
        if ($webUiTests.Count -gt 0) {
            Invoke-GateCheck -Name 'WebUI Node tests' -Action { node --test $webUiTests.FullName } | Out-Null
        }
    }

    Write-Summary
    if (($results | Where-Object Status -eq 'FAILED').Count -gt 0) {
        exit 1
    }
}
finally {
    Pop-Location
}
