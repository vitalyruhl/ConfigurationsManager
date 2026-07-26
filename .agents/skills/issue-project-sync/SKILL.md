# Issue and Project Sync

Use only for workflow-authorized GitHub Issue or Project work. This skill does
not infer tracking intent from ordinary implementation work.

## Inputs

- explicit user or workflow authorization
- verified issue number, branch, commit, pull request, or URL
- configured Project `ConfigurationsManager` (#5)

## Procedure

1. Read the current Issue and Project state before mutation.
2. For a bug-fix pull request, search for a matching issue, reuse it or create
   it with the existing `bug` label, and set its Project status to `In Progress`.
3. Apply only authorized changes, then read back labels, status, links, and
   references.

## Output

Report old state, intended mutation, final state, and any permission or API
blocker.

## Stop

Never invent issue references, close an issue, mark it fixed, change Project
status to Done, or create duplicate labels. Keep bug issues open and `In
Progress` until merged validation—including required HIL—has passed.
