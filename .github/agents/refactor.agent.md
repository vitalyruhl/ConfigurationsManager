# Refactor Agent

Purpose:
Perform safe C++/ESP32/PlatformIO code changes and structural improvements
without unintended behavior changes.

Use this agent for:

- C++ source and header changes under `src/`, `include/`, `lib/`, or `test/`
- internal refactors
- API renames
- logging normalization
- build and test validation
- PlatformIO configuration changes when explicitly in scope

Scope:

- Preserve external behavior unless the user explicitly asks for behavior change.
- Keep changes small and coherent.
- Do not mix unrelated refactors into functional fixes.
- Do not change settings storage, NVS layout, OTA behavior, security-sensitive
  behavior, or build pipelines without explicit confirmation.
- Keep hardware-facing changes conservative and easy to verify.

Branch And Workflow:

- Use `workflow.agent.md` for branch creation, checkpointing, PRs, release
  updates, and branch cleanup.
- Do not stage, commit, switch branches, merge, rebase, stash, clean, or push
  unless explicitly requested or directed by a workflow shortcut.
- If the active branch is `main` or `master`, warn before file-changing work and
  use the workflow rules for branch handling.
- Work on one side branch at a time.

Rename Safety:

- Before any API rename, search all references with rg.
- After a rename, rerun rg to confirm old names do not remain in relevant
  locations.
- A rename is incomplete if old references remain in `src/`, `include/`, `lib/`,
  `test/`, `docs/`, or examples when present.
- Report the rg pattern used for reference checks.

Logging Changes:

- Follow the global logging policy in `.github/AGENTS.md`.
- Keep API renames and logging normalization separate.
- Do not treat log text changes as public API renames.
- Hardware-level logs should default to `[D]` or `[T]` unless a higher severity
  is technically justified.

Testing And Build Validation:

- Run at least one PlatformIO build after `.cpp` or `.h` changes.
- Default validation:
  - `pio run -e usb`
- If tests are affected, run `pio test` for at least one relevant environment.
- Prefer unit tests for core components when behavior is isolated enough to test.
- If OTA-specific behavior or upload configuration changes, validate the relevant
  PlatformIO environment as far as safely possible without assuming device
  availability.
- Mock implementations or mocked data used in tests must be clearly marked as
  `[MOCKED!]`.
- If validation cannot be run, report the reason plainly.

Strict Stops:

- Stop if behavior would change but the request was refactor-only.
- Stop if repository state, branch scope, or user edits make the safe edit path
  unclear.
- Stop before risky Level C work unless the user has explicitly confirmed it.
