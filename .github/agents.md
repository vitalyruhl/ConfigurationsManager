
# AGENTS.md

Repository guidance for contributors and coding agents.

This repository is `ConfigurationsManager`, a C++/ESP32 project built with
PlatformIO and the Arduino framework. The repository also contains a web UI and
support tooling used by the embedded project.

## Communication

- Use informal German ("du") when talking to the user.
- Keep explanations short unless detail is explicitly requested.
- After completing work, summarize briefly.
- Internal chat with the user may remain informal German according to these
  communication rules.
- Only normal chat with the user may be German by default. Repository artifacts
  must remain in English unless the user explicitly requests a different
  language for a specific artifact.

## Repository Text Language

- Governance files under `.github/`, repository documentation, generated
  project text, issue text, pull request text, review comments created by
  agents, code comments, error messages, log messages, and commit messages
  created by agents must be written in English.
- Do not translate repository artifacts to German just because the user chat is
  German.
- If a specific artifact must be written in another language, require an
  explicit user request for that artifact.

## GitHub Text Language

- GitHub Issue titles must be written in English.
- GitHub Issue bodies must be written in English.
- GitHub Pull Request titles and descriptions must be written in English.
- GitHub comments created by agents should be written in English unless the user
  explicitly requests otherwise.
- Do not create German GitHub issue or PR text by default.
- These rules apply only to GitHub and repository artifacts, not to normal chat
  with the user.

## Repository Scope

- Primary project configuration: `platformio.ini`.
- Main firmware source code lives under `src/`.
- Web UI sources and build tooling live under `webui/`.
- Examples live under `examples/`.
- Supporting documentation lives under `docs/` and `README.md`.
- Tests live under `test/` when present.
- Support scripts and repository tooling live under `tools/`.
- Wokwi simulation files may live under example-specific `Wokwi/` folders.
- Do not assume Python application release flows or a standalone backend web
  service unless the repository explicitly introduces them.

## Agent Routing

- `.github/AGENTS.md` is the canonical source for repository-wide rules.
- `.github/agents/control-plane.agent.md` only routes work to the correct agent.
- Use `.github/agents/refactor.agent.md` for code changes, refactors, tests, and
  build validation.
- Use `.github/agents/workflow.agent.md` for branches, issues, PRs, releases,
  checkpoints, and explicit session-close workflows.
- Use `.github/agents/docs.agent.md` for documentation work.
- Agent files may add scope-specific rules, but they must not contradict this
  file. If they do, follow this file and report the drift.

## Tracking Policy

- GitHub Issues may be used for tracked work when useful.
- GitHub Pull Requests may be used for review and integration when useful.
- GitHub Project usage is optional and repository-specific.
- Current GitHub Project: `ConfigurationsManager` (#5)
- When tracked workflow or project coordination is relevant, agents may use the
  configured GitHub Project.
- Do not invent mandatory project-board updates for tasks that do not actually
  use tracking.
- If the configured GitHub Project changes later, update this file and keep
  workflow guidance consistent with the new value.

## Safety Principles

- User changes are sacred. Never revert or overwrite user edits without asking.
- Analyze before modifying files.
- Do not make functional project-code changes unless explicitly requested or
  strictly required to correct governance references.
- Do not invent project rules. Preserve existing project-specific rules unless
  they are clearly obsolete or consciously replaced by newer governance.
- Do not mark an issue as solved or fixed until the user confirms it works.
- Before rename/delete operations, search references first and update them.
- Before terminal commands that modify the workspace, ensure goal and scope are
  clear.
- If requirements are ambiguous or a change impacts multiple subsystems/files,
  ask 1-3 precise questions or propose 2-3 options before editing.
- Work incrementally in small steps: fix, verify, checkpoint or commit only when
  requested, then continue.
- Work on one side branch at a time.
- Before multi-file refactors or other risky changes, ensure the baseline is
  understood and either clean, committed, or intentionally dirty by user
  request.

## Autonomy Levels

- Level A, safe: read-only actions such as search, read, and analysis may be done
  immediately.
- Level B, normal: small, clearly scoped changes may be implemented immediately.
- Level C, risky: changes involving settings structure, storage/NVS, OTA,
  security, build pipelines, or large refactors require explicit confirmation.

## Repository Workflow Rules

- Route branch, release, PR, merge, checkpoint, and cleanup operations through
  `.github/agents/workflow.agent.md`.
- `main` is the published or released branch.
- `release/*` branches are runnable snapshot branches and must stay buildable
  and runnable.
- `release/*` branches are versioned by release, for example `release/v4.0.0`.
- `feature/*` branches are work-in-progress branches and may be unfinished or
  temporarily broken.
- Do not change `main` directly.
- Docs-only TODO updates under `docs/TODO.md` or `docs/todo_*.md` may be
  committed directly to `main` only when the user explicitly requests that
  workflow.
- If the active branch is `main` or `master` and file-changing work is
  requested, stop before editing and route through
  `.github/agents/workflow.agent.md` to create or select a proper side branch.
- The only direct-edit exception on `main` or `master` is an explicitly
  requested docs-only TODO update under `docs/TODO.md` or `docs/todo_*.md`.
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
- Stage, commit, and push only on explicit user request or when a named
  workflow explicitly requires it.
- If staging is requested, prefer `git add -A`.
- Do not prepend `Set-Location` to git commands. Use the configured working
  directory. For non-git commands that must change directory, use
  `Push-Location` / `Pop-Location`.
- Prefer gh for PRs, CI checks, issues, and project operations when available.

## Version Policy

- Require an appropriate project version bump for dependency updates,
  PlatformIO configuration changes, library metadata changes, firmware code
  changes, or example changes that affect build outputs unless the user
  explicitly says not to bump.
- Governance-only and documentation-only changes do not require a version bump.
  Report that the version bump was skipped by policy.
- Use patch version bumps for dependency updates, bug fixes, internal compatible
  changes, and build or configuration maintenance with no public API break.
- Use minor version bumps for new public features, new examples, new public
  APIs, and compatible behavior additions.
- Use major version bumps for breaking public API changes, incompatible
  storage/NVS layout changes, incompatible configuration schema changes, and
  behavior changes requiring user migration.
- Before changing versions, search for version declarations and report the
  candidate files found.
- If multiple version declarations exist, report them before changing versions.
- Bump library or project version declarations for dependency and build metadata
  changes. Do not automatically bump independent example firmware or app
  versions.
- Bump an example firmware or app version only when that example itself is
  intentionally changed or released.
- If the version source of truth is unclear, stop and report candidate files
  instead of guessing.
- If version ownership is unclear, stop and report candidate files instead of
  guessing.

## Session-Close Workflow

- Default session-close behavior:
  - commit the current work branch (`feature/*`) with `git add -A` and a
    meaningful English commit message
  - push the work branch to `origin`
  - run `pio run -e usb` from the repository root when `.cpp` or `.h` files
    changed
  - if that build succeeds, or was skipped because no `.cpp` or `.h` files
    changed, update the active `release/*` branch to match the work branch
  - prefer fast-forward updates for the release branch
  - if fast-forward is not possible, ask explicitly before using
    `--force-with-lease`
  - push the updated release branch

## Code Rules

- Code comments must be in English.
- Identifiers and function names must be in English.
- Error and log messages must be in English.
- Emojis are forbidden in code, comments, logs, and generated outputs.
- Favor small, coherent changes over broad speculative refactors.
- Keep hardware-facing and configuration-sensitive changes conservative.

## Documentation Exception

- Markdown prose is exempt from flash-oriented logging brevity rules.
- Markdown prose may use readable long-form tags such as `[WARNING]`, `[NOTE]`,
  and `[INFO]`.
- Code blocks inside Markdown are not exempt. Code examples and text log
  examples must still follow the global logging policy from this file.
- Documentation must still be written in English unless the user explicitly
  requests a different language for a specific document.

## Logging Policy

All source code, headers, examples, tests, and code blocks inside Markdown must
follow this policy.

- Log messages must use short ASCII-only severity tags:
  - `[E]` error
  - `[W]` warning
  - `[I]` info
  - `[D]` debug
  - `[T]` trace
- Long tags such as `[ERROR]`, `[WARNING]`, `[INFO]`, `[DEBUG]`, and `[SUCCESS]`
  are forbidden in code/log output.
- Verbose is not a severity level. Verbose controls whether logs are emitted;
  severity tags describe impact.
- Severity reassignment is allowed when technically justified.
- Log message updates are not public API renames.
- Keep log text semantically clear and as short as reasonably possible to reduce
  flash usage.
- Prefer common abbreviations such as `cfg`, `init`, `ok`, `fail`, `adc`, `pwm`,
  and `io` where clarity remains intact.
- Do not repeat information already encoded by severity tags, module prefixes,
  or surrounding context.
- IO and hardware-level logs are usually `[D]` or `[T]`. Use `[W]` for notable
  recoverable conditions and `[E]` for critical failures.

## Rename And Logging Interaction

- API renames and logging normalization are separate concerns.
- Do not mix logging normalization into API rename phases.
- Before any API rename, perform a full reference search with rg.
- After renaming, rerun rg to ensure old names do not remain in relevant
  project files such as `src/`, `include/`, `lib/`, `test/`, `docs/`, and
  examples when present.

## Tool Policy

- Prefer available VS Code or agent-workspace tools first when they fit the task.
- Prefer locally installed CLI tools next.
- Do not use unnecessarily heavy tools when simple search or inspection is
  enough.
- Known local tools on the user's Windows/PowerShell environment may include:
  - git
  - gh
  - rg
  - fd
  - jq
  - dasel
  - jc
  - 7z
  - pwsh
  - winget
  - choco
  - coreutils
  - node / npm
  - python
  - uv
  - platformio / pio, if installed for this repository
- Preferred tools:
  - rg for text search, audits, and reference checks.
  - fd for file discovery, audits, and reference checks.
  - git for version-control inspection and explicit version-control actions.
  - platformio / pio for PlatformIO build, upload, monitor, and test flows.
  - gh for GitHub PRs, CI checks, issues, and project operations when
    available.
  - jq for JSON.
  - dasel for YAML, TOML, JSON, or XML inspection when it is the safest fit.
- If rg or fd is missing, report that and give a simple install hint instead
  of silently using a weaker search path for audits or reference checks.
- When reporting search results, include the rg pattern used.
- Do not assume every listed tool is installed on every machine. If a required
  tool is missing, report the failed command clearly.
- Do not install or upgrade tools unless the user explicitly asks.
- On Windows, do not assume sed or awk are available. Prefer
  PowerShell-native commands unless availability was confirmed.
- Use dasel for YAML, TOML, JSON, or XML inspection and edits when it is the
  safest fit.
- Use jq for JSON.
- Use gh for GitHub operations when authenticated and available.
- Use 7z for archive inspection or extraction when needed.
- Use uv only when Python tooling is actually part of the task. Do not infer
  Python project workflows from the presence of uv.
- Do not mix jq and dasel syntax.
- Do not use deprecated dasel flags.
- Prefer structured tools over brittle text parsing when that reduces risk.
- Do not assume repository, branch, PR, or project state. Verify with git and
  gh.

## Validation Baseline

- Always run at least one PlatformIO build after `.cpp` or `.h` changes.
- Default build check:
  - `pio run -e usb`
- If only Markdown or governance files changed, skip PlatformIO build unless the
  user asks for it.
- If tests are affected, run `pio test` for at least one relevant environment.
- If a required validation cannot run, report that plainly.

## Mandatory Reporting

After file-changing work, report:

- branch used
- files changed
- concise summary of what changed
- validation run
- skipped validation with reason
- remaining risks or blockers

Inspect relevant diffs before reporting file-changing work. Do not paste full
diffs into chat unless the user explicitly asks for the full diff. Prefer
`git diff --stat`, changed file lists, and focused summaries. Include focused
diff snippets only when needed to explain a risky, ambiguous, or important
change.

## Final Rule

- Never mark an issue as solved or fixed until the user explicitly confirms it
  works.
