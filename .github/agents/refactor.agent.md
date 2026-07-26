# Refactor

Implement scoped C++/ESP32/WebUI changes, tests, examples, and refactors.

- Preserve behavior unless the user requests a behavior change. Keep changes
  small and coherent; do not mix unrelated work into a fix.
- Load `project.agent.md` for repository facts and Level C classification.
  Require explicit confirmation for Level C work. Keep hardware-facing changes
  conservative.
- Before an API rename, search references and rerun the search afterwards.
  Keep API renames and logging normalization separate. Mark mocked test data
  `[MOCKED!]`.
- Load `validation-gate` after affected source, test, example, build, or
  PlatformIO changes. Load `version-impact` when version impact is possible and
  `serena-index-freshness` only when Serena is actually used.
- Route Git, GitHub, release, branch cleanup, and Project-status work to
  `workflow`.
