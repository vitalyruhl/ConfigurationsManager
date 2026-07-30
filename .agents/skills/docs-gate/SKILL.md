# Documentation Gate

Use only for an explicit documentation-consistency gate or before an authorized
integration or release. It is read-only.

## Inputs

- pending diff and changed behavior
- relevant `README.md`, `docs/`, changelog, examples, and governance paths

## Procedure

1. Inspect the pending diff and identify documentation affected by the change.
2. Verify commands, versions, configuration names, and user-facing behavior
   against repository sources.
3. Report only concrete inconsistencies, missing required documentation, or a
   reason why no update is required.
4. Before a release, identify unpublished entries since the last release;
   verify their product and tracked-work evidence; then review a proposed
   user-oriented consolidated entry for the actual release version and date.
   Remove redundant headings only from that unpublished range, retain published
   history, and do not infer claims from commit subjects alone.

## Output

Return findings with paths and suggested owner. If no update is required, return
exactly `docs checked / no changes needed`.

For a release, explicitly report the reviewed changelog diff, the unpublished
range, supporting product/Issue evidence, and any remaining ambiguity.

## Stop

Do not edit files, commit, merge, publish, or mutate Issues or Projects.
