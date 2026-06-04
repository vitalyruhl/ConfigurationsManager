# Agent Workflow Notes

- Status: canonical summary; always reread `.github/AGENTS.md` and the applicable agent
  file before acting.
- `workflow.begin`: create/select the appropriate side branch, then stop before edits.
- `workflow.checkpoint`: commit and push the current coherent state.
- `workflow.docs`: perform narrow documentation synchronization.
- `workflow.audit`: strictly read-only; no file, branch, commit, or merge changes.
- `workflow.ship`: build and verify artifacts without implicit merge.
- `workflow.ready`: prepare for review/integration and run or report validation; do not
  merge or update release branches.
- `workflow.toMain`: use the agreed integration workflow, normally a PR; validation and
  documentation impact checks are required before merge.
- `workflow.cleanBranches`: delete only branches verified as integrated.
- `workflow.end`: report repository, validation, and blocker state without implicit Git
  writes.
- Shortcuts run sequentially. Follow-up work after `workflow.audit` requires explicit user
  request and no remaining blockers.

Sources: `.github/AGENTS.md`, `.github/agents/workflow.agent.md`.
