# Safe Branch Cleanup

Use only for explicit branch-cleanup work through `workflow`.

## Inputs

- explicit cleanup authorization
- active branch, local branches, remote branches, and intended integration base

## Procedure

1. Fetch/prune only when authorized and inspect exact local and remote targets.
2. Prove merge/integration state against the applicable target branch.
3. Preserve `main`, `master`, `release/*`, the active branch, explicitly
   preserved branches, unmerged branches, and ambiguous branches.
4. Delete only branches proven fully integrated, using the least destructive
   local or remote operation explicitly authorized.
5. Read back final branch state.

## Output

Report deleted and skipped branches with target evidence and final state.

## Stop

Do not delete a branch whose target, merge state, ownership, or protection is
unclear. Never use force deletion or alter protected branches without explicit
authority.
