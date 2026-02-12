Role:
you are my coding assistant. Follow the instructions in this file carefully when generating code.

========================================
COMMUNICATION STYLE
========================================
- Use informal tone with "you" (not "Sie" or formal language)
- Answer in German
- Give only a brief overview after completing tasks
- Provide detailed explanations only when explicitly asked

========================================
SEMI-AUTOMATIC WORKFLOW GUIDELINES
========================================
- User changes are sacred:
  - Never revert or overwrite user edits without asking first.

- Confirm-before-write:
  - If requirements are ambiguous or the change impacts multiple subsystems/files,
    ask 1–3 precise clarifying questions or propose 2–3 options before editing.

- One side-branch at a time:
  - Do not work on more than one side-branch simultaneously.

- Step-by-step workflow:
  - Implement changes incrementally in small steps:
    fix → verify → commit → continue.

- Do not revert user edits unless asked:
  - Even if unrelated to the current topic.

- Autonomy levels:
  - Level A (safe): Read-only actions (search, read, analyze) may be done immediately.
  - Level B (normal): Small, clearly scoped changes (1–2 files, obvious fix) may be implemented immediately.
  - Level C (risky): Changes involving settings structure/storage/NVS, OTA, security,
    build pipelines, or large refactors REQUIRE explicit confirmation.

- Safety gates:
  - Before rename/delete: always search references and update them.
  - Before running terminal commands that modify the workspace: ensure goal and scope are clear.

========================================
GIT WORKFLOW GUIDELINES
========================================
- Branch roles in this repository:
  - main:
    - published / released branch
  - release/*:
    - runnable snapshot branches
    - must always be buildable and runnable
    - versioned by release (e.g. release/v3.3.0, release/v3.4.0, ...)
  - feature/*:
    - work-in-progress branches
    - may be unfinished or temporarily broken

- "Feierabend" workflow trigger:
  - If the user says:
    "ich mach jetzt feierabend" or "ich will jetzt feierabend machen"
  - Treat this as an end-of-session cue.

  Default behavior:
  - Commit the current work branch (feature/*).
  - use git add -A and a meaningful commit message.
  - Push the work branch to origin.
  - Run a PlatformIO build in the repo root:
      pio run -e usb
    (must succeed without errors).
    - Exception: If no `.cpp` or `.h` files were changed, skip this build step.
  - Only if the build succeeds (or was skipped due to no `.cpp`/`.h` changes):
    - Update the active release/* branch to match the work branch exactly.
    - Prefer fast-forward updates.
    - If fast-forward is not possible, ask explicitly before force-pushing
      (push --force-with-lease).
  - Push the updated release/* branch.

- Never change main directly:
  - If the active branch is main/master:
    - Emit a [WARNUNG] warning
    - use git add -A and a meaningful commit message, if user has changed files.

- Exception:
  - Docs-only TODO updates (docs/TODO.md, docs/todo_*.md)
    may be committed directly to main.

- Git command rules:
  - Read-only git commands may be run without asking:
    git status, git diff, git log, git show, git branch, git remote -v
  - Commands that modify history or working tree require confirmation:
    git add, commit, switch/checkout, reset, merge, rebase, clean, stash, cherry-pick

- Staging / committing:
  - Stage/commit/push ONLY on explicit user request.
  - If staging is requested, prefer:
    git add -A

- Command execution style:
  - Do NOT prepend Set-Location for git commands.
  - Only change directories when required for non-git commands.
  - Use Push-Location / Pop-Location to restore the original directory.

- Large changes require a clean baseline:
  - Before multi-file refactors or risky changes, ensure work is committed or stashed.
  - Branch naming check:
    - Verify the active branch matches the topic.
    - If not, emit a [W] warning and propose 2–3 suitable branch names.


- GitHub CLI:
  - Prefer gh for PRs, CI checks, issues, when available.

========================================
CODE STYLE (STRICT, GLOBAL)
========================================
- All code comments in English only
- Clear, descriptive variable names (English only)
- All function names in English only
- All error and log messages in English
- Emojis are forbidden everywhere:
  - code
  - comments
  - log messages
  - outputs

----------------------------------------
DOCUMENTATION EXCEPTION (.md FILES)
----------------------------------------
- The strict logging severity and efficiency rules apply to:
  - source code
  - headers
  - examples
  - tests

- Markdown documentation files (*.md) are EXEMPT from strict logging rules:
  - Readability and clarity have priority over flash optimization.
  - Long-form tags like [WARNING], [NOTE], [INFO] are allowed in prose text.

- IMPORTANT:
  - Code blocks inside Markdown files (```cpp, ```c, ```text, etc.)
    MUST still follow the Global Logging Severity & Efficiency Policy.
  - Only narrative documentation text is exempt, not code examples.

========================================
GLOBAL LOGGING SEVERITY & EFFICIENCY POLICY
(ESP32 / Flash-Optimized, Mandatory)
========================================
- ALL log messages MUST use short, ASCII-only severity tags.
- Allowed severity tags:
  - [E] = Error
  - [W] = Warning
  - [I] = Info
  - [D] = Debug
  - [T] = Trace

- Long tags are FORBIDDEN everywhere:
  - [ERROR], [WARNING], [INFO], [DEBUG], [SUCCESS], etc.

- Verbose is NOT a severity level:
  - Verbose controls WHETHER logs are emitted.
  - Severity tags describe WHAT the log means.

- Severity reassignment is ALLOWED:
  - The agent MAY change severity levels
    (e.g. [W] → [D]) if technically justified.
  - Severity must reflect the actual relevance and impact of the message.

- Logging text changes are NOT API renames:
  - Log message updates do NOT count as public API changes.
  - Rename safety rules do NOT apply to log text.

----------------------------------------
LOG MESSAGE EFFICIENCY RULE
----------------------------------------
- Log messages must be semantically clear but as short as reasonably possible.
- Prefer compression over verbosity to reduce flash usage.
- Avoid filler words when meaning remains clear:
  - "already exists" → "exists"
  - "both enabled" → "both ON"
  - "using pull-up" → "use pull-up"
- Prefer standard abbreviations:
  - cfg, init, ok, fail, pull-up/down, adc, pwm, io
- Do NOT repeat information already encoded by:
  - severity tag
  - module prefix ([IO], [CM], etc.)
  - surrounding context

----------------------------------------
LOW-LEVEL LOGGING GUIDANCE
----------------------------------------
- IO and hardware-level logs are typically debug- or trace-oriented.
- Treat LOG output as [D] or [T] by default.
- Prefer [W] only for notable but recoverable conditions.
- Use [E] only for critical IO failures.

========================================
RENAME & LOGGING INTERACTION (PHASE RULE)
========================================
- API renames and logging normalization MUST be treated as separate concerns.
- Logging normalization MUST NOT be mixed with API renaming phases.
- Log text changes do NOT trigger rename safety rules.

========================================
TOOLING & SEARCH POLICY
========================================
- Preferred search tool:
  - ALWAYS use ripgrep (rg) for code searches, audits, and reference checks.
    - if is not installed, give instructions to install it first. (choco install ripgrep)
  - ALWAYS use sharkdp.fd (fd) for file searches, audits, and reference checks.
    - if is not installed, give instructions to install it first. (winget install sharkdp.fd)
- When reporting search results:
  - Include the rg pattern used.

- Windows compatibility:
  - Do NOT assume sed or awk are available.
  - Prefer PowerShell-native commands for replacements and filtering.
  - sed/awk may be used ONLY if explicitly available and confirmed.

========================================
RENAME SAFETY RULE
========================================
- Before any API rename:
  - Perform a full reference search using rg.
- After renaming:
  - Re-run rg to ensure no old names remain.
- A rename is incomplete if any reference remains in:
  - src/
  - include/
  - examples/
  - docs/
  - tests/

========================================
TESTING & BUILD VALIDATION
========================================
- Unit tests for core components
- Mock implementations for testing
  - mocked data must be clearly marked as [MOCKED!]
- Always run at least one PlatformIO build after changes.
  - Exception: If no `.cpp` or `.h` files were changed, do not run `pio run`.
- If tests are affected, run pio test for at least one environment.

========================================
FINAL RULE
========================================
- Never mark an issue as solved or fixed until the user explicitly confirms it works.
