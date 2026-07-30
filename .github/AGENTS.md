# ConfigurationsManager Governance

This repository is a C++ ESP32/Arduino library with PlatformIO projects, a Web
UI, examples, tests, and support tools. This file is the canonical policy.

## Load and Routing

- Before an action, read this file and the selected role under
  `.github/agents/`. Read `project.agent.md` only for repository facts and load
  only triggered skills from `.agents/skills/`.
- Read known governance paths directly. Governance searches must use hidden
  paths, for example `rg --hidden` or `fd --hidden`.
- `control-plane` routes only. `docs` owns documentation and governance;
  `refactor` owns product changes and bounded validation; `workflow` owns Git,
  GitHub, Project, release, and final workflow decisions. `plan`, `audit`, and
  `architecture-audit` are read-only.
- If the role or scope is ambiguous, stop and report the candidates. Do not
  substitute a role or duplicate its procedure.
- If governance rules conflict, apply the stricter rule and report the conflict
  before continuing.

## Safety and Language

- Use informal German in chat. Repository artifacts, code comments, logs,
  commits, documentation, issues, pull requests, and generated governance are
  English.
- User changes are sacred. Analyze before editing; never overwrite, reset,
  clean, stash, or delete without explicit authority.
- Before any Git mutation, inspect the active branch and working tree. Never
  stage foreign, pre-existing, or unclear changes, and never use uncontrolled
  `git add -A`.
- Do not commit after a failed validation or a validation that changed files.
  Reuse validation only when commit/source state, working tree, command, and
  relevant scope are demonstrably unchanged.
- Work on one side branch. Do not edit `main` or `master`, except an explicitly
  requested docs-only TODO update under `docs/TODO.md` or `docs/todo_*.md`.
- Upload, monitor, hardware, network, release, merge, tag, and publication
  actions require explicit authority. Never mark an issue fixed until the user
  confirms it works.
- Level C changes (storage/NVS, OTA, security, PlatformIO/build pipelines, or
  large refactors) require explicit confirmation.

## Project Facts

- Canonical library version: `library.json`.
- Project configuration: `platformio.ini`; source: `src/`; Web UI: `webui/`;
  examples: `examples/`; tests: `test/`; documentation: `README.md`, `docs/`;
  tools: `tools/`.
- `src/ConfigManager.h`, `webui/package.json`, `webui/package-lock.json`,
  README version text, and minimal-example version references are mirrors of
  `library.json`. Other example app versions change only when explicitly in
  scope.
- Code and Markdown code blocks use concise ASCII severity tags `[E]`, `[W]`,
  `[I]`, `[D]`, or `[T]`; Markdown prose may use long-form tags.

## Workflow and Tracking

- Integration to `main` is pull-request based by default. `release/*` branches
  are optional runnable snapshots; do not assume they exist. Direct pushes and
  bypasses require explicit authority.
- Read-only Git inspection is allowed. Staging, commit, switch, rebase, merge,
  cleanup, push, and GitHub mutations require explicit authority or an
  explicitly invoked workflow shortcut.
- GitHub Project `ConfigurationsManager` (#5) is used only for tracked work or
  when project coordination is explicit. GitHub text is English.
- Before a bug-fix pull request, search existing issues; reuse a matching issue
  or create one with the existing `bug` label, add it to Project #5 as `In
  Progress`, and use `Fixes #<issue>` in the pull-request body. Keep it open and
  `In Progress` until merge and every required validation gate—including HIL
  when applicable—has passed. Report any tracking blocker; never silently skip
  it.
- Before integration, run the documentation impact gate. Governance-only and
  documentation-only changes need neither changelog nor version bump unless the
  change itself affects governance documentation.

## Version and Validation

- Use a patch bump for compatible fixes, dependencies, and build maintenance;
  minor for compatible public features; major for breaking APIs, storage,
  configuration schema, or migration-required behavior. Classify with
  `version-impact` before version-affecting work.
- Before a version change, scan the canonical source and mirrors. Synchronize
  all relevant mirrors from `library.json`, update the changelog for published
  changes, and report mismatches or missing mirrors.
- A required but missing version bump blocks checkpoints and integration.
- Product C/C++ changes require at least one relevant PlatformIO build; affected
  tests and examples require focused validation. Governance-only work requires
  governance consistency validation, not a PlatformIO build unless requested.
- The canonical complete validation is
  `tools/build/run-full-repository-gate.ps1`. It discovers supported PlatformIO
  targets, runs the repository's automated tests, validates governance
  generation and workflow structure, and never uploads or deletes caches.
- For GitHub CI, start one exact Run-ID-bound wait through
  `tools/ci/wait-ci-run.ps1` with the expected full head SHA. Do not poll CI
  through repeated agent turns, reuse an older commit result, or load successful
  full logs. On failure, inspect only the compact excerpt first and fetch a
  specific failed job only when that excerpt is insufficient.
- A PR gate must additionally re-check the current PR head and its required
  checks after the exact run passes. A changed PR head is `STALE_HEAD`; require
  an explicit new Run ID and expected SHA rather than switching automatically.

## Changelog and Release Documentation

- During development, separate user-relevant changes may be recorded as clearly
  unpublished development entries. Governance, agent, workflow, and internal
  Git changes normally do not belong in the public changelog.
- Do not rewrite published release history. Before a release, `docs-gate` must
  identify all unpublished entries since the last release, verify the related
  product changes and tracked work, and consolidate technical intermediate
  entries into one user-oriented entry for the actual release version and date.
- The release gate must remove redundant intermediate headings only from the
  unpublished range, include issue references when useful, avoid claims based
  only on commit subjects, and report the changelog diff explicitly before
  release work continues.

## Reporting

Report selected role, branch, changed files, validation and skipped checks,
version impact when applicable, and remaining blockers. Inspect relevant diffs;
do not paste full diffs unless asked.
