# Validation Gate

Use after implementation when `refactor` selects bounded validation, or when a
full repository gate is explicitly requested.

## Inputs

- changed paths and requested validation scope
- project profile and current repository state

## Procedure

1. Select the smallest relevant tests and builds. Reuse successful unchanged
   results only when their final-state relevance is clear.
2. For governance-only changes, run governance consistency checks and skip
   PlatformIO builds unless explicitly requested.
3. For the complete repository gate, invoke
   `tools/build/run-full-repository-gate.ps1`; do not reconstruct its dynamic
   PlatformIO matrix manually.
4. Run `git diff --check` for file-changing work and report exact commands,
   exit status, and skipped checks.

## Output

Return command, exit status, changed files caused by validation, and `pass`,
`fail`, or `blocker` for the final state.

## Stop

Do not decide product scope, version impact, merge readiness, or bypass policy.
Never upload, monitor, delete PlatformIO caches, or mask a failing check.
