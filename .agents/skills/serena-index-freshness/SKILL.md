# Serena Index Freshness

Use only when a plan, refactor, or audit actually uses Serena. It does not apply
to ordinary documentation or workflow work.

## Inputs

- intended Serena query or memory use
- `.serena/project.yml`, shared memories, and current repository state

## Procedure

1. Confirm that Serena is needed for the task.
2. Inspect index metadata and relevant shared memories for staleness against the
   working tree.
3. Refresh only the minimal required index data using the repository's existing
   Serena tooling, if available and authorized.

## Output

Report whether Serena was used, freshness evidence, refresh action, and limits.

## Stop

Do not treat Serena memories as source of truth or edit repository code solely
to satisfy index freshness. If the needed tool is unavailable, continue with
normal repository inspection and report the limitation.
