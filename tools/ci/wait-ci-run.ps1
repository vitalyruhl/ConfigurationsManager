<#
.SYNOPSIS
Waits for one exact GitHub Actions run and validates its expected commit.

.NOTES
Exit codes: 0 PASS; 1 CI failure; 2 timeout; 3 cancelled, skipped, or
incomplete result; 4 invalid run/repository/SHA target or stale PR head;
5 gh.exe, authentication, or GitHub query failure.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateRange(1, 9223372036854775807)]
    [Int64]$RunId,

    [Parameter(Mandatory)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string]$ExpectedHeadSha,

    [string]$Repository,

    [ValidateRange(1, 240)]
    [int]$TimeoutMinutes = 45,

    [ValidateRange(5, 300)]
    [int]$PollSeconds = 25,

    [ValidateRange(1, 200)]
    [int]$ErrorExcerptLines = 40,

    [ValidateRange(1, 2147483647)]
    [int]$PullRequest,

    [string]$ResultPath
)

# Exit codes: 0 PASS; 1 CI failure; 2 timeout; 3 cancelled, skipped, or
# incomplete result; 4 invalid run/repository/SHA target or stale PR head;
# 5 gh.exe, authentication, or GitHub query failure.

if ($PSVersionTable.PSVersion.Major -lt 7) {
    Write-Error "PowerShell 7 or newer is required. Current version: $($PSVersionTable.PSVersion). Run this script with pwsh.exe."
    exit 5
}

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$exitSuccess = 0
$exitCiFailure = 1
$exitTimeout = 2
$exitIncomplete = 3
$exitTargetMismatch = 4
$exitGhFailure = 5
$ansiPattern = [regex]::new("`e\[[0-?]*[ -/]*[@-~]")
$startedWaitingAt = Get-Date

function Write-ResultFile {
    param([hashtable]$Result)

    if (-not $script:ResultPath) {
        return
    }
    $directory = Split-Path -Parent $script:ResultPath
    if ($directory) {
        New-Item -ItemType Directory -Path $directory -Force | Out-Null
    }
    $Result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $script:ResultPath -Encoding utf8
}

function Exit-WithResult {
    param(
        [string]$Result,
        [int]$ExitCode,
        [hashtable]$Details = @{}
    )

    $payload = @{
        schemaVersion = 1
        result = $Result
        exitCode = $ExitCode
        repository = $script:Repository
        runId = $RunId
        runNumber = if ($script:Run) { $script:Run.number } else { $null }
        attempt = if ($script:Run) { $script:Run.attempt } else { $null }
        workflowName = if ($script:Run) { $script:Run.workflowName } else { $null }
        expectedHeadSha = $ExpectedHeadSha
        actualHeadSha = if ($script:Run) { $script:Run.headSha } else { $null }
        status = if ($script:Run) { $script:Run.status } else { $null }
        conclusion = if ($script:Run) { $script:Run.conclusion } else { $null }
        startedAt = if ($script:Run) { $script:Run.startedAt } else { $null }
        completedAt = if ($script:Run) { $script:Run.updatedAt } else { $null }
        durationSeconds = if ($script:Run -and $script:Run.status -eq 'completed') { Get-RunDuration -CurrentRun $script:Run } else { [Math]::Round(((Get-Date) - $startedWaitingAt).TotalSeconds, 2) }
        failedJobs = @($script:FailedJobs)
        failedSteps = @($script:FailedSteps)
        url = if ($script:Run) { $script:Run.url } else { $null }
        failedLogPath = $script:FailedLogPath
    }
    foreach ($entry in $Details.GetEnumerator()) {
        $payload[$entry.Key] = $entry.Value
    }
    Write-ResultFile -Result $payload
    exit $ExitCode
}

function Invoke-GhJson {
    param([string[]]$Arguments)

    $output = & gh @Arguments 2>&1
    if ($LASTEXITCODE -ne 0) {
        $message = (($output | ForEach-Object { $_.ToString() }) -join "`n").Trim()
        throw "GitHub CLI query failed: $message"
    }
    $json = (($output | ForEach-Object { $_.ToString() }) -join "`n")
    return $json | ConvertFrom-Json
}

function Get-RunDuration {
    param($CurrentRun)

    if ($CurrentRun.startedAt -and $CurrentRun.updatedAt) {
        return [Math]::Round(((Get-Date $CurrentRun.updatedAt) - (Get-Date $CurrentRun.startedAt)).TotalSeconds, 2)
    }
    return $null
}

function Get-FailedDetails {
    param($CurrentRun)

    $failedJobs = @()
    $failedSteps = @()
    foreach ($job in @($CurrentRun.jobs)) {
        if ($job.conclusion -in @('failure', 'cancelled', 'canceled', 'timed_out', 'skipped', 'action_required', 'stale')) {
            $failedJobs += $job.name
            foreach ($step in @($job.steps)) {
                if ($step.conclusion -in @('failure', 'cancelled', 'canceled', 'timed_out', 'skipped', 'action_required', 'stale')) {
                    $failedSteps += "$($job.name): $($step.name)"
                }
            }
        }
    }
    return @{ Jobs = $failedJobs; Steps = $failedSteps }
}

function Get-FailedLogExcerpt {
    $script:FailedLogPath = Join-Path (Split-Path -Parent $script:ResultPath) ("ci-run-$RunId-failed.log")
    $output = & gh run view $RunId -R $script:Repository --log-failed 2>&1
    $rawLog = (($output | ForEach-Object { $_.ToString() }) -join "`n")
    Set-Content -LiteralPath $script:FailedLogPath -Value $rawLog -Encoding utf8

    $lines = $rawLog -split "`r?`n" |
        ForEach-Object { $ansiPattern.Replace($_, '').TrimEnd() } |
        Where-Object { $_ -and $_ -notmatch '^\s*(Downloading|[0-9]+%|\.\.\.)' }
    return @($lines | Select-Object -First $ErrorExcerptLines)
}

function Test-RequiredPullRequestChecks {
    if (-not $PullRequest) {
        return
    }
    $pullRequest = Invoke-GhJson -Arguments @('pr', 'view', $PullRequest, '-R', $script:Repository, '--json', 'headRefOid,url')
    if ($pullRequest.headRefOid -ne $ExpectedHeadSha) {
        Write-Output 'CI RESULT: STALE_HEAD'
        Write-Output "Expected PR head: $ExpectedHeadSha"
        Write-Output "Actual PR head: $($pullRequest.headRefOid)"
        Exit-WithResult -Result 'STALE_HEAD' -ExitCode $exitTargetMismatch -Details @{ pullRequest = $PullRequest; pullRequestUrl = $pullRequest.url }
    }

    $checksOutput = & gh pr checks $PullRequest -R $script:Repository --required --json name,state,bucket,link 2>&1
    $checksExitCode = $LASTEXITCODE
    $checksText = (($checksOutput | ForEach-Object { $_.ToString() }) -join "`n")
    $checks = if ($checksText) { $checksText | ConvertFrom-Json } else { @() }
    if ($checks.Count -eq 0) {
        Write-Output 'CI RESULT: INCOMPLETE'
        Write-Output 'Required PR checks are not configured or not available for this PR head.'
        Exit-WithResult -Result 'PR_REQUIRED_CHECKS_UNAVAILABLE' -ExitCode $exitIncomplete -Details @{ pullRequest = $PullRequest; pullRequestUrl = $pullRequest.url }
    }
    $nonPassing = @($checks | Where-Object { $_.bucket -ne 'pass' })
    if ($checksExitCode -ne 0 -or $nonPassing.Count -gt 0) {
        Write-Output 'CI RESULT: INCOMPLETE'
        $nonPassing | ForEach-Object { Write-Output "Required check: $($_.name) [$($_.state)]" }
        Exit-WithResult -Result 'PR_REQUIRED_CHECKS_NOT_GREEN' -ExitCode $exitIncomplete -Details @{ pullRequest = $PullRequest; pullRequestUrl = $pullRequest.url; requiredChecks = $checks }
    }
}

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Error 'gh.exe is required but was not found.'
    exit $exitGhFailure
}

try {
    & gh auth status *> $null
    if ($LASTEXITCODE -ne 0) {
        throw 'GitHub CLI authentication is unavailable.'
    }
    if (-not $Repository) {
        $Repository = ((& gh repo view --json nameWithOwner --jq '.nameWithOwner').Trim())
        if ($LASTEXITCODE -ne 0 -or -not $Repository) {
            throw 'The current GitHub repository could not be determined.'
        }
    }
    $script:Repository = $Repository
    if (-not $ResultPath) {
        $ResultPath = Join-Path (Get-Location) '.Temp\ci-wait-result.json'
    }
    $script:ResultPath = [IO.Path]::GetFullPath($ResultPath)
    $script:Run = $null
    $script:FailedJobs = @()
    $script:FailedSteps = @()
    $script:FailedLogPath = $null
    try {
        $repositoryInfo = Invoke-GhJson -Arguments @('repo', 'view', $Repository, '--json', 'nameWithOwner')
    }
    catch {
        if ($_.Exception.Message -match '(?i)not found|404|could not resolve') {
            Write-Output 'CI RESULT: INVALID_TARGET'
            Write-Output "Repository was not found: $Repository"
            Exit-WithResult -Result 'REPOSITORY_NOT_FOUND' -ExitCode $exitTargetMismatch
        }
        throw
    }
    if ($repositoryInfo.nameWithOwner -ne $Repository) {
        Write-Output 'CI RESULT: INVALID_TARGET'
        Write-Output "Expected repository: $Repository"
        Write-Output "Actual repository: $($repositoryInfo.nameWithOwner)"
        Exit-WithResult -Result 'REPOSITORY_MISMATCH' -ExitCode $exitTargetMismatch
    }

    try {
        $script:Run = Invoke-GhJson -Arguments @('run', 'view', $RunId, '-R', $Repository, '--json', 'databaseId,attempt,conclusion,createdAt,headSha,jobs,name,number,startedAt,status,updatedAt,url,workflowName')
    }
    catch {
        if ($_.Exception.Message -match '(?i)not found|404|could not resolve') {
            Write-Output 'CI RESULT: INVALID_TARGET'
            Write-Output "Run ID was not found in ${Repository}: $RunId"
            Exit-WithResult -Result 'RUN_NOT_FOUND' -ExitCode $exitTargetMismatch
        }
        throw
    }

    if ($script:Run.databaseId -ne $RunId) {
        Write-Output 'CI RESULT: INVALID_TARGET'
        Write-Output "Requested run ID: $RunId"
        Write-Output "Returned run ID: $($script:Run.databaseId)"
        Exit-WithResult -Result 'RUN_ID_MISMATCH' -ExitCode $exitTargetMismatch
    }
    if ($script:Run.headSha -ne $ExpectedHeadSha) {
        Write-Output 'CI RESULT: STALE_HEAD'
        Write-Output "Expected commit: $ExpectedHeadSha"
        Write-Output "Run commit: $($script:Run.headSha)"
        Write-Output "Workflow: $($script:Run.workflowName)"
        Write-Output "URL: $($script:Run.url)"
        Exit-WithResult -Result 'HEAD_SHA_MISMATCH' -ExitCode $exitTargetMismatch
    }

    Write-Output "CI TARGET: Run ID $RunId | Workflow: $($script:Run.workflowName) | URL: $($script:Run.url)"
    Write-Output "CI TARGET: Commit: $($script:Run.headSha) | Status: $($script:Run.status)"
    $lastState = $null
    $deadline = $startedWaitingAt.AddMinutes($TimeoutMinutes)
    while ($true) {
        $state = "$($script:Run.status)/$($script:Run.conclusion)"
        if ($state -ne $lastState) {
            Write-Output "CI STATUS: $state"
            $lastState = $state
        }

        if ($script:Run.status -eq 'completed') {
            break
        }
        if ($script:Run.status -in @('action_required', 'stale')) {
            Write-Output "CI RESULT: INCOMPLETE ($($script:Run.status))"
            Exit-WithResult -Result $script:Run.status.ToUpperInvariant() -ExitCode $exitIncomplete
        }
        if ((Get-Date) -ge $deadline) {
            Write-Output 'CI RESULT: TIMEOUT'
            Write-Output "Run ID: $RunId"
            Write-Output "URL: $($script:Run.url)"
            Exit-WithResult -Result 'TIMEOUT' -ExitCode $exitTimeout
        }

        Start-Sleep -Seconds $PollSeconds
        $script:Run = Invoke-GhJson -Arguments @('run', 'view', $RunId, '-R', $Repository, '--json', 'databaseId,attempt,conclusion,createdAt,headSha,jobs,name,number,startedAt,status,updatedAt,url,workflowName')
        if ($script:Run.headSha -ne $ExpectedHeadSha) {
            Write-Output 'CI RESULT: STALE_HEAD'
            Write-Output "Expected commit: $ExpectedHeadSha"
            Write-Output "Run commit: $($script:Run.headSha)"
            Exit-WithResult -Result 'HEAD_SHA_MISMATCH' -ExitCode $exitTargetMismatch
        }
    }

    $details = Get-FailedDetails -CurrentRun $script:Run
    $script:FailedJobs = $details.Jobs
    $script:FailedSteps = $details.Steps
    if ($script:Run.conclusion -eq 'success') {
        Test-RequiredPullRequestChecks
        $passedJobs = @($script:Run.jobs | Where-Object { $_.conclusion -eq 'success' }).Count
        $totalJobs = @($script:Run.jobs).Count
        Write-Output 'CI RESULT: PASS'
        Write-Output "Run ID: $RunId"
        Write-Output "Workflow: $($script:Run.workflowName)"
        Write-Output "Commit: $($script:Run.headSha)"
        Write-Output "Jobs: $passedJobs/$totalJobs"
        Write-Output "Duration: $(Get-RunDuration -CurrentRun $script:Run)s"
        Write-Output "URL: $($script:Run.url)"
        Exit-WithResult -Result 'PASS' -ExitCode $exitSuccess
    }

    if ($script:Run.conclusion -in @('cancelled', 'canceled', 'skipped', 'timed_out', 'action_required', 'stale', $null)) {
        Write-Output "CI RESULT: INCOMPLETE ($($script:Run.conclusion))"
        Write-Output "Run ID: $RunId"
        Write-Output "URL: $($script:Run.url)"
        Exit-WithResult -Result 'INCOMPLETE' -ExitCode $exitIncomplete
    }

    $excerpt = @(Get-FailedLogExcerpt)
    Write-Output 'CI RESULT: FAIL'
    Write-Output "Run ID: $RunId"
    Write-Output "Workflow: $($script:Run.workflowName)"
    Write-Output "Commit: $($script:Run.headSha)"
    Write-Output "Failed job: $($script:FailedJobs -join ', ')"
    Write-Output "Failed step: $($script:FailedSteps -join ', ')"
    Write-Output 'Error:'
    $excerpt | ForEach-Object { Write-Output $_ }
    Write-Output "Full failed log: $($script:FailedLogPath)"
    Write-Output "URL: $($script:Run.url)"
    Exit-WithResult -Result 'FAIL' -ExitCode $exitCiFailure
}
catch {
    Write-Error "CI RESULT: GITHUB_ERROR. $($_.Exception.Message)"
    exit $exitGhFailure
}
