# Refactor Agent

## Mandatory governance load preflight

Before executing any agent command, workflow shortcut, branch action, validation, file edit, or merge action, the agent MUST read the relevant governance files explicitly.

Required reads:

- `.github/AGENTS.md`
- `.github/agents/<selected-agent>.agent.md`

For workflow shortcuts, the selected agent is:

- `.github/agents/workflow.agent.md`

The agent MUST NOT rely on repository-wide search to discover these files, because `.github/` may be hidden from default search tools.

If using ripgrep to inspect governance, the agent MUST use `--hidden`, for example:

- rg --hidden -n "workflow\.begin|workflow\.audit|workflow\.toMain|workflow\.cleanBranches" .

## Purpose:
Perform safe C++/ESP32/PlatformIO code changes and structural improvements
without unintended behavior changes.

Use this agent for:

- C++ source and header changes under `src/`, `include/`, `lib/`, or `test/`
- internal refactors
- API renames
- logging normalization
- build and test validation
- PlatformIO configuration changes when explicitly in scope

## Scope:

- Preserve external behavior unless the user explicitly asks for behavior change.
- Keep changes small and coherent.
- Do not mix unrelated refactors into functional fixes.
- Do not change settings storage, NVS layout, OTA behavior, security-sensitive
  behavior, or build pipelines without explicit confirmation.
- Keep hardware-facing changes conservative and easy to verify.

## Branch And Workflow:

- Use `workflow.agent.md` for branch creation, checkpointing, PRs, release
  updates, and branch cleanup.
- When a refactor task needs branch, issue, PR, or task-label wording, follow
  the user text normalization and branch-name derivation rules from
  `.github/AGENTS.md` and `workflow.agent.md`.
- Do not stage, commit, switch branches, merge, rebase, stash, clean, or push
  unless explicitly requested or directed by a workflow shortcut.
- If the active branch is `main` or `master` and file-changing work is
  requested, stop before editing and use the workflow rules to create or select
  a proper side branch.
- The only direct-edit exception on `main` or `master` is an explicitly
  requested docs-only TODO update under `docs/TODO.md` or `docs/todo_*.md`.
- Work on one side branch at a time.

## Version Handling:

- Follow the central Version Policy in `.github/AGENTS.md`.
- Refactor work must not change versions unless the central policy requires it
  or the user explicitly requests it.
- When refactor work touches versioning, release metadata, changelog/release
  notes, package metadata, library metadata, or files that may contain the
  project version, run and report a version scan before editing. Include at
  least these commands or stricter equivalents:
  - `rg -n "version|VERSION|CONFIG_MANAGER_VERSION|CONFIGMANAGER_VERSION|CM_VERSION" .`
  - `rg -n "<current-version>|<target-version>" .`
- Treat `library.json` as the only canonical source of truth for the
  ConfigurationsManager project/library version. `src/ConfigManager.h`,
  `webui/package.json`, `webui/package-lock.json`, README text, changelog text,
  and example files are mirrors or independent app versions, not sources of
  truth.
- When the project/library version changes, synchronize the known mirror paths:
  `library.json`, `src/ConfigManager.h`, `webui/package.json`,
  `webui/package-lock.json`, README version badges or project/library version
  mentions, `examples/minimal/platformio.ini`, `examples/minimal/src/main.cpp`,
  and any other minimal example file containing the library/app version. Report
  any missing listed path.
- Keep the minimal example aligned with the ConfigurationsManager
  project/library version. Do not automatically change other examples'
  firmware/application versions unless the issue explicitly asks for it.
- Prefer npm tooling for WebUI package version changes so
  `webui/package-lock.json` remains consistent. If npm is not run, explain how
  the lockfile was updated or why it was not updated.
- If version mirrors disagree before work starts, report the mismatch before
  changing version-related files. If the target version is unclear, stop and ask
  for clarification instead of guessing.
- After version-related changes, report the canonical `library.json` version,
  each synchronized file and the version found there, intentionally unchanged
  files, scan commands used, validation performed, and any remaining mismatch or
  risk.

## Rename Safety:

- Before any API rename, search all references with rg.
- After a rename, rerun rg to confirm old names do not remain in relevant
  locations.
- A rename is incomplete if old references remain in `src/`, `include/`, `lib/`,
  `test/`, `docs/`, or examples when present.
- Report the rg pattern used for reference checks.

## Logging Changes:

- Follow the global logging policy in `.github/AGENTS.md`.
- Keep API renames and logging normalization separate.
- Do not treat log text changes as public API renames.
- Hardware-level logs should default to `[D]` or `[T]` unless a higher severity
  is technically justified.

## Testing And Build Validation:

- Run at least one PlatformIO build after `.cpp` or `.h` changes.
- Default validation:
  - `pio run -e usb`
- For affected examples, run the relevant example build.
- If tests are affected, run `pio test` for at least one relevant environment.
- Run relevant tests when tests are present and affected.
- Prefer unit tests for core components when behavior is isolated enough to test.
- Use configured and enabled GitHub Actions or checks when they exist. Do not
  invent required CI workflows.
- If no enabled CI is configured, report that and rely on required local
  validation.
- Docker or image builds are not required unless configured in this repository
  or explicitly in scope.
- If OTA-specific behavior or upload configuration changes, validate the relevant
  PlatformIO environment as far as safely possible without assuming device
  availability.
- Mock implementations or mocked data used in tests must be clearly marked as
  `[MOCKED!]`.
- If validation cannot be run, report the reason plainly.

## Reporting:

- Inspect relevant diffs before reporting file-changing work.
- Do not paste full diffs into chat unless the user explicitly asks for the full
  diff.
- Prefer changed file lists, concise summaries, validation commands and results,
  skipped validation reasons, and remaining risks or blockers.
- Prefer `git diff --stat` or focused summaries for reporting.
- Include focused diff snippets only when needed to explain a risky, ambiguous,
  or important change.
- Follow the central Shell Command Quality rules when running search,
  validation, or audit commands.

## Strict Stops:

- Stop if behavior would change but the request was refactor-only.
- Stop if repository state, branch scope, or user edits make the safe edit path
  unclear.
- Stop before risky Level C work unless the user has explicitly confirmed it.
