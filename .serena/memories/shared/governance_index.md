# Governance Index

- Status: canonical summary; always reread the source governance files.
- Always reread `.github/AGENTS.md`; it is canonical and overrides summaries.
- Read the applicable agent file directly:
  - `.github/agents/refactor.agent.md` for code, tests, build, and structural changes.
  - `.github/agents/docs.agent.md` for documentation.
  - `.github/agents/workflow.agent.md` for branches, commits, PRs, merges, releases, and
    cleanup.
- Normal user chat is informal German. Repository artifacts, code comments, logs, commit
  messages, issues, PRs, and governance text are English.
- User changes are sacred. Analyze before editing and never revert unrelated changes.
- Do not edit `main` or `master`; use one side branch. Git writes require explicit user
  approval or an explicitly requested governed workflow.
- Upload, monitor, erase, OTA, and hardware/network-affecting commands require explicit
  user approval.
- Do not mark an issue solved or fixed until the user confirms it works.

Sources: `AGENTS.md`, `.github/AGENTS.md`, `.github/agents/refactor.agent.md`,
`.github/agents/docs.agent.md`, `.github/agents/workflow.agent.md`.
