# Root Agent Instructions

This file is the universal entry point. Repository policy is canonical in
`.github/AGENTS.md`.

## Required Reading

- Read `.github/AGENTS.md` and exactly the role file needed for the current
  action under `.github/agents/` before acting.
- Read `.github/agents/project.agent.md` only when project-specific facts are
  needed. Read a skill only when its trigger applies.
- Read known governance paths directly. Governance discovery with `rg` or `fd`
  must include hidden paths.

## Baseline

- Use informal German in chat. Keep every repository artifact in English.
- Never overwrite user changes or perform Git, upload, monitor, destructive,
  network, release, merge, or publication actions without explicit authority.
- Never mark an issue as solved or fixed until the user confirms it works.
- If this file conflicts with `.github/AGENTS.md`, follow `.github/AGENTS.md`.
