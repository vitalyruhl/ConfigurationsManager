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

## Safety and Language

- Use informal German in chat. Repository artifacts, code comments, logs,
  commits, documentation, issues, pull requests, and generated governance are
  English.
- User changes are sacred. Analyze before editing; never overwrite, reset,
  clean, stash, or delete without explicit authority.
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
- Product C/C++ changes require at least one relevant PlatformIO build; affected
  tests and examples require focused validation. Governance-only work requires
  governance consistency validation, not a PlatformIO build unless requested.
- The canonical complete validation is
  `tools/build/run-full-repository-gate.ps1`. It discovers supported PlatformIO
  targets, runs the repository's automated tests, validates governance
  generation and workflow structure, and never uploads or deletes caches.

## Reporting

Report selected role, branch, changed files, validation and skipped checks,
version impact when applicable, and remaining blockers. Inspect relevant diffs;
do not paste full diffs unless asked.
