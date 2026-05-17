# Workflow Agent

Purpose:
Provide repository workflow rules for branches, issues, PRs, checkpoints,
release branches, and explicit session-close handling.

This agent must apply `.github/AGENTS.md`.

## Mandatory governance load

Before executing any agent command, workflow shortcut, branch action, validation, file edit, or merge action, the agent MUST read the relevant governance files explicitly.

Required reads:

- `.github/AGENTS.md`
- `.github/agents/<selected-agent>.agent.md`

For workflow shortcuts, the selected agent is:

- `.github/agents/workflow.agent.md`

The agent MUST NOT rely on repository-wide search to discover these files, because `.github/` may be hidden from default search tools.

If using ripgrep to inspect governance, the agent MUST use `--hidden`, for example:

- rg --hidden -n "workflow\.begin|workflow\.audit|workflow\.toMain|workflow\.cleanBranches" .

## Branch Model

- `main` is the published/released branch.
- `release/*` branches are runnable snapshot branches and must stay buildable and
  runnable.
- `release/*` branches are versioned by release, for example `release/v4.0.0`
  or `release/v4.1.0`.
- Do not assume `release/*` branches exist. If no suitable release branch
  exists, report that and skip release-branch updates unless the user explicitly
  asks to create or update one.
- Missing release branches do not block normal pull-request based `main`
  integration unless release sync is explicitly in scope.
- `feature/*` branches are work-in-progress branches and may be unfinished or
  temporarily broken.
- Work on one side branch at a time.
- Do not change `main` directly.
- Integrate into `main` through pull requests by default.
- Docs-only TODO updates under `docs/TODO.md` or `docs/todo_*.md` may be
  committed directly to `main` when the user explicitly requests that workflow.
- If file-changing work is requested while the active branch is `main` or
  `master`, stop before editing and create or select a proper side branch.
- The only direct-edit exception on `main` or `master` is an explicitly
  requested docs-only TODO update under `docs/TODO.md` or `docs/todo_*.md`.
- Direct pushes to `main` are forbidden unless the user explicitly requests an
  exception.
- Fast-forward integration to `main` is allowed only when the user explicitly
  requests fast-forward or `ff`.

## Git Command Rules

- Read-only git commands may be run without asking:
  - `git status`
  - `git diff`
  - `git log`
  - `git show`
  - `git branch`
  - `git remote -v`
- Commands that modify history or working tree require confirmation:
  - `git add`
  - `git commit`
  - `git switch` / `git checkout`
  - `git reset`
  - `git merge`
  - `git rebase`
  - `git clean`
  - `git stash`
  - `git cherry-pick`
- Stage, commit, and push only on explicit user request or when a named workflow
  explicitly requires it.
- If staging is requested, prefer `git add -A`.
- Do not prepend `Set-Location` to git commands. Use the configured working
  directory. For non-git commands that must change directory, use
  `Push-Location` / `Pop-Location`.

## Branch Safety

- Before multi-file refactors or risky changes, ensure the current baseline is
  understood and either clean, committed, or intentionally dirty by user request.
- Work incrementally: fix, verify, checkpoint or commit only when requested, then
  continue.
- Verify the active branch matches the task.
- If the active branch is `main` or `master`, warn and stop before
  file-changing work unless the direct-edit docs-only TODO exception applies.
- If branch naming does not match the task, warn and propose suitable branch
  names instead of silently switching.
- Never revert user edits unless explicitly asked.

## Branch Name And Title Derivation

- Derive branch names, issue titles, pull request titles, and task labels from
  the task intent, not by blindly copying informal free text.
- Correct obvious spelling mistakes in user-provided free text when the intended
  meaning is clear.
- Prefer short, clean English, lowercase, hyphen-separated branch names under
  the appropriate prefix, usually `feature/`.
- Do not silently correct exact file paths, commands, symbols, identifiers,
  branch names, issue numbers, pull request numbers, tags, versions, or quoted
  literals.
- If the user explicitly provides an exact branch name and says to use it
  exactly, preserve it as written.
- If the requested wording is ambiguous, or a correction would materially change
  the task scope, stop and ask before creating or switching branches.
- Check quoted paths against repository state before treating different casing
  as a typo. For example, `.github/agents.md` and `.github/AGENTS.md` may be a
  casing issue rather than interchangeable paths on every platform.
- Examples:
  - `use workflow.begin [gevernance-sharpenes]` may derive
    `feature/governance-sharpening`.
  - `use workflow.begin [update boilersaver example]` may derive
    `feature/update-boilersaver-example`.

## GitHub Workflow

- Prefer gh for PRs, CI checks, and issues when available.
- Keep PRs scoped to one coherent change.
- Do not claim merge readiness without running or reporting the relevant
  validation.
- Maintainer-owned PRs do not require external review by repository governance.
- Maintainer-owned PRs may be merged when required validation passed and GitHub
  branch protection allows the merge.
- External contributor PRs require review before merge. Do not merge unreviewed
  external contributions.
- Branch protection or ruleset bypass may only be used when the user explicitly
  requests owner/admin bypass for the current action.
- Do not use owner/admin bypass automatically.
- If GitHub branch protection blocks a merge because review is required, report
  the blocker and do not bypass branch protection.
- If a maintainer-owned PR is blocked only by a review requirement and required
  validation passed, report that owner/admin bypass may be possible when
  configured.
- If review is not intended for maintainer-owned PRs, recommend adjusting branch
  protection instead of bypassing it.
- Do not bypass external contributor PR review unless the user explicitly
  confirms a repository-owner exception after reviewing the PR.
- Required status checks must not be bypassed unless the user explicitly
  confirms an owner/admin exception and the reason is reported.
- GitHub Issue titles and bodies created by agents must be written in English.
- GitHub Pull Request titles and descriptions created by agents must be written
  in English.
- GitHub comments created by agents should be written in English unless the user
  explicitly requests otherwise.
- Internal chat with the user may remain informal German; do not use that as a
  reason to create German GitHub issue or PR text by default.
- Use GitHub Issues or PRs as task tracking when the user asks for tracked
  workflow, but do not invent mandatory project-board rules for this repository.
- GitHub Project usage is optional and controlled by the current configured
  project in `.github/AGENTS.md` `Tracking Policy`.
- Project-board actions remain optional unless the user asks for tracked
  workflow or the task explicitly uses project coordination.
- Issues and PRs remain allowed regardless of GitHub Project configuration.

## PlatformIO Workflow

- Use configured and enabled GitHub Actions or checks when they exist.
- Do not invent required CI workflows.
- If no enabled CI is configured, report that and rely on required local
  validation.
- Default build validation:
  - `pio run -e usb`
- For affected examples, run the relevant example build.
- OTA environment validation, when explicitly relevant:
  - `pio run -e ota`
- Upload commands require explicit user request because they interact with
  hardware or network devices.
- Serial monitor commands require explicit user request because they attach to
  local devices.
- If no `.cpp` or `.h` files changed, skip PlatformIO build unless requested.
- Run relevant tests when tests are present and affected.
- Docker or image builds are not required unless configured in this repository
  or explicitly in scope.

## Documentation Impact Workflow

- Before `workflow.toMain` prepares or merges changes into `main`, perform a
  documentation impact check.
- Check whether the changed files or behavior require updates to `README.md`,
  `docs/`, release notes, or governance documentation.
- If documentation is affected, route through `docs.agent.md` before merging.
- If `docs/CHANGELOG.md` exists and the change is user-visible,
  release-relevant, dependency-related, build-related, or version-related,
  update it or explicitly justify why no changelog update is needed.
- If documentation is not affected, report that documentation sync is not
  required and why.
- Do not invent documentation updates for purely internal changes.
- Governance-only changes do not require changelog entries unless this
  repository intentionally tracks governance changes in the changelog.
- Documentation-only changes do not require a version bump.

## Version Bump Workflow

- Dependency updates, PlatformIO configuration changes, library metadata
  changes, firmware code changes, and example changes that affect build outputs
  require an appropriate project version bump unless the user explicitly says
  not to bump.
- Governance-only and documentation-only changes do not require a version bump.
  Report that the version bump was skipped by policy.
- Use patch bumps for dependency updates, bug fixes, internal compatible
  changes, and build or configuration maintenance with no public API break.
- Use minor bumps for new public features, new examples, new public APIs, and
  compatible behavior additions.
- Use major bumps for breaking public API changes, incompatible storage/NVS
  layout changes, incompatible configuration schema changes, and behavior
  changes requiring user migration.
- Before changing versions, search for version declarations and report the
  candidate files found.
- If multiple version declarations exist, report them before changing versions.
- Treat the ConfigurationsManager package/library version as the main project
  version.
- Treat the WebUI package under `webui/` as part of the repository build
  artifact, not as an independent example firmware or app.
- When WebUI npm dependencies, WebUI build metadata, or WebUI package metadata
  change, align the WebUI package version with the ConfigurationsManager
  package/library version.
- Bump an example firmware or app version only when that example itself is
  intentionally changed or released.
- If the version source of truth is unclear, stop and report candidate files
  instead of guessing.

## Release Branch Workflow

- `release/*` branches should represent runnable snapshots.
- Do not assume a release branch exists.
- If no suitable release branch exists, report that and skip release-branch
  update unless the user explicitly asks to create or update one.
- Missing release branches do not block PR-based `main` integration unless
  release sync was explicitly in scope.
- Prefer fast-forward updates when moving a release branch to a verified feature
  state.
- If fast-forward is not possible, ask explicitly before force-pushing. Prefer
  `--force-with-lease` if force-push is approved.
- Do not invent Python or `pyproject.toml` release steps for this repository
  unless the repository later adds such files and policy.
- Project version bumps are governed by `Version Bump Workflow`.

## Workflow Shortcuts

These names describe expected intent if the user invokes them:

- `workflow.begin`:
  - create the appropriate branch according to branch policy
  - derive a suitable clean English branch name from the task intent
  - avoid propagating obvious spelling mistakes from informal free text
  - preserve an exact branch name only when the user explicitly says to use that
    exact branch name
  - report the derived branch name before or immediately after creating it
  - mention when obvious typos were normalized, when relevant
  - do not perform code edits
  - do not perform build changes
  - do not run implementation work unless the user explicitly asks after the branch exists
- `workflow.checkpoint`: create a commit and push the current coherent state.
- `workflow.docs`: perform a narrow documentation-only synchronization.
- `workflow.audit`: read-only workflow or repository-state audit.
  - read-only only
  - no file changes
  - no branch changes
  - no commits
  - no merges
  - if followed by `workflow.toMain` or `workflow.cleanBranches`, finish the
    audit first and report blockers before any follow-up workflow runs
- `workflow.ship`: build and verify artifacts without implicit merge.
- `workflow.ready`: prepare work for review or integration, run or report
  relevant validation, do not merge to `main`, do not update `release/*`, and do
  not push unless explicitly requested or covered by a named workflow.
- `workflow.toMain`: get validated work onto `main` through the agreed pull
  request workflow unless the user explicitly requested fast-forward or `ff`;
  perform the documentation impact check before merging.
  - commit, push, PR creation, PR merge, and branch cleanup are allowed only as
    part of this explicitly requested workflow
  - run or report relevant validation before merge
  - perform the documentation impact check before merge
  - report GitHub blockers before merge, including required reviews, failing
    checks, conflicts, and branch protection
  - use owner/admin bypass only when the user explicitly requested it for the
    current action
  - do not bypass required status checks unless the user explicitly confirmed
    that exception and the reason is reported
- `workflow.cleanBranches`: delete only branches verified as integrated.
  - do not delete active, unmerged, or ambiguous branches
  - report skipped branches with the reason
- `workflow.end`: inspect repository state and report current branch, changed
  files, validation state, and blockers without claiming merge or fix success.
  Do not commit, push, merge, or update release branches unless explicitly
  requested.

Shortcut behavior must remain conservative:

- Inspect repository state first.
- Avoid destructive operations.
- Report blockers plainly.
- Do not create parallel branch lines for the same work.
- Chained shortcuts run sequentially. If audit blockers remain, stop before
  merge, cleanup, release updates, or destructive actions.
- Follow-up workflows after `workflow.audit` may run only when the user
  explicitly requested them and no blockers remain.
- Follow the central Shell Command Quality rules for search, validation, and
  audit commands.

## Mandatory Reporting

Report:

- current branch
- files changed when relevant
- concise summary of what changed
- git actions performed
- validation run
- validation skipped with reason
- release branch updates if any
- remaining blockers or risks

Inspect relevant diffs before reporting file-changing work. Do not paste full
diffs into chat unless the user explicitly asks for the full diff. Prefer
`git diff --stat`, changed file lists, and focused summaries. Include focused
diff snippets only when needed to explain a risky, ambiguous, or important
change.
