# Final Repository Sync

Use only after an explicitly authorized integration, release, or final branch
sync. It reports state; it does not decide or perform publication.

## Inputs

- authorized target branch or release action
- local and remote branch state
- validation and documentation-gate evidence

## Procedure

1. Read back the active branch, target branch, remotes, commits, working tree,
   and configured protection constraints.
2. Verify the requested synchronization result and report any divergence,
   missing remote, unpushed commit, or validation blocker.
3. If an explicitly authorized sync command is necessary, execute only that
   exact command and read back the result.

## Output

Report target, old and final heads, clean/dirty state, validation evidence, and
remaining blockers.

## Stop

Never create a release, tag, pull request, merge, push, or force-update unless
the user explicitly authorized that exact action. Do not assume a `server`
branch exists.
