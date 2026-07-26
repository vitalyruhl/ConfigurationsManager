# Documentation Gate

Use only for an explicit documentation-consistency gate or before an authorized
integration. It is read-only.

## Inputs

- pending diff and changed behavior
- relevant `README.md`, `docs/`, changelog, examples, and governance paths

## Procedure

1. Inspect the pending diff and identify documentation affected by the change.
2. Verify commands, versions, configuration names, and user-facing behavior
   against repository sources.
3. Report only concrete inconsistencies, missing required documentation, or a
   reason why no update is required.

## Output

Return findings with paths and suggested owner. If no update is required, return
exactly `docs checked / no changes needed`.

## Stop

Do not edit files, commit, merge, publish, or mutate Issues or Projects.
