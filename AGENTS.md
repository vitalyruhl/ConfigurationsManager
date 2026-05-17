# Root Agent Instructions

This root file exists for tools that load repository-root `AGENTS.md` before
repository-specific governance.

## Canonical Governance

- `.github/AGENTS.md` is the canonical repository governance file.
- Before repository work, read `.github/AGENTS.md` and the applicable
  `.github/agents/*.agent.md` file directly.
- Do not rely on repository-wide search to discover governance files; read known
  governance paths directly.
- If this file conflicts with `.github/AGENTS.md`, follow `.github/AGENTS.md`.
- Defer to `.github/AGENTS.md` for repository workflow, branch, pull request,
  merge, release, validation, version, tool, and agent-routing rules.

## Communication

- Use informal German ("du") in normal chat with the user.
- Keep user-facing summaries brief unless detailed explanation is requested.
- Repository artifacts, code comments, commit messages, issue text, pull request
  text, and generated governance text follow the language rules in
  `.github/AGENTS.md`.

## Safety Baseline

- User changes are sacred. Never revert or overwrite user edits without explicit
  request.
- Do not stage, commit, push, merge, rebase, reset, clean, stash, switch
  branches, or update release branches unless the user explicitly requests it or
  a named workflow in `.github/AGENTS.md` requires it.
- Do not work directly on `main` or `master` except for the narrow exceptions
  defined in `.github/AGENTS.md`.
- Do not run upload, monitor, destructive, or hardware/network-affecting
  commands unless explicitly requested.

## Final Rule

- Never mark an issue as solved or fixed until the user explicitly confirms it
  works.
