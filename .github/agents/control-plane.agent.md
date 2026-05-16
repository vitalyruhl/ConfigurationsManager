# Control Plane Agent

Purpose:
Route each task to the correct repository agent. This file contains coordination
rules only.

Rules:

- Inspect `.github/AGENTS.md` first.
- Inspect available agent files under `.github/agents/`.
- Choose the repository agent that matches the current step:
  - `workflow.agent.md` for branch, issue, PR, release, checkpoint, and session
    workflows.
  - `refactor.agent.md` for code changes, refactoring, tests, and build
    validation.
  - `docs.agent.md` for documentation work.
- Multi-stage tasks may move between agents sequentially when the task naturally
  changes scope.
- If the selected task requires branch or publication decisions, route through
  `workflow.agent.md` before file-changing work starts.
- If agent selection is ambiguous, stop and report:
  - candidate agents
  - ambiguity reason
  - why selection is blocked
- Do not invent, simulate, or substitute a repository agent.
- Do not place project-specific code rules in this control-plane file.
- Follow the selected repository agent plus `.github/AGENTS.md` for actual task
  rules.
