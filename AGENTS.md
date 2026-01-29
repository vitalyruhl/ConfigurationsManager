Role:
you are my coding assistant. Follow the instructions in this file carefully when generating code.

- Communication Style:
  - Use informal tone with "you" (not "Sie" or formal language)
  - Answer in German
  - Give only brief overview after completing tasks
  - Provide detailed explanations only when explicitly asked

- Semi-automatic Workflow Guidelines:
  - User changes are sacred: Never revert/overwrite user edits without asking first.
  - Confirm-before-write: If requirements are ambiguous or the change impacts multiple subsystems/files, ask 1-3 precise clarifying questions (or propose 2-3 options) before editing files.
  - One side-branch at a time: Do not work on more than one side-branch simultaneously.
  - Step-by-step workflow: Implement changes incrementally in small steps: fix -> verify -> commit -> continue.
  - Do not revert user edits unless asked: If the user made changes (even unrelated to the current branch/topic), do not roll them back unless the user explicitly requests it.
  - Autonomy levels:
    - Level A (safe): Read-only actions (search, read, analyze) can be done immediately.
    - Level B (normal): Small, clearly scoped changes (1-2 files, obvious fix) can be implemented immediately.
    - Level C (risky): Changes involving settings structure/storage/NVS, OTA, security, build pipelines, or large refactors require explicit confirmation before implementation.
  - Safety gates: Before rename/delete, always search for references and update them. Before running terminal commands that modify the workspace, ensure the goal/scope is clear.

- Git Workflow Guidelines:
  - Branch roles in this repo (important):
    - main: published/released branch
    - release/v3.3.0: "runnable snapshot" branch (must always be buildable/runnable)
    - feature/* (e.g. feature/io-manager): work-in-progress branch (may be unfinished/broken)
  - "Feierabend" workflow trigger:
    - If the user says "ich mach jetzt feierabend" or "ich will jetzt feierabend machen", treat this as an end-of-session cue.
    - Default rule:
      - Always: commit + push the current work branch (feature/*) to origin.
      - Then: run a PlatformIO build in the repo root: `pio run -e usb` (must succeed with no errors).
      - Only if the root build succeeds: update `release/v3.3.0` to be a full copy of the work branch (all files identical).
        - No cherry-pick. Prefer a fast-forward update when possible.
        - If fast-forward is not possible, a hard reset + `push --force-with-lease` is allowed, but ask explicitly before rewriting the remote history.
      - Finally: push `release/v3.3.0` to origin.
  - Never change main directly: For future work, do not implement changes while the active branch is main/master. If the active branch is main/master, emit a [WARNING] and propose 2-3 suitable branch names before making any edits.
  - Exception (docs-only TODO updates): If the only changes are documentation TODO files (e.g. docs/TODO.md, docs/todo_*.md), direct commits to main are allowed.
  - Git read-only commands are allowed without asking: Examples: `git status`, `git diff`, `git log`, `git show`, `git branch`, `git remote -v`.
  - Git commands that can modify the working tree/index/history still require confirmation: Examples: `git add`, `git commit`, `git switch/checkout`, `git reset`, `git merge`, `git rebase`, `git clean`, `git stash`, `git cherry-pick`.
  - After each commit on a work branch (feature/*), always push to origin.
  - Command execution style:
    - For Git commands, do NOT prepend `Set-Location ...;` (just run e.g. `git status -sb`).
    - Only change directories when required for non-git commands (e.g. WebUI/PlatformIO in examples) and use `Push-Location`/`Pop-Location` to avoid leaving the terminal in a different folder.
  - Large changes require a clean baseline: Before starting larger changes (multi-file refactor, settings/storage/OTA/security, or anything that could take >30 minutes), ensure the current work is saved in git (commit or stash) so changes stay reviewable and reversible.
  - Branch naming check: Verify the active branch name matches the change topic. If it does not, emit a [WARNING] and propose 2-3 suitable branch names.
  - GitHub CLI preferred: If GitHub-related actions are needed (create/view PRs, check CI status, view issues), prefer using GitHub CLI (gh) when available.
  - Suggested branch structure:
    - feature/<short-topic> (new feature)
    - fix/<short-topic> (bug fix)
    - chore/<short-topic> (maintenance/refactor/tooling)
    - docs/<short-topic> (documentation only)
  - exceptions:
    - index.html.gz will be generated always on build, can be ignored for clan baseline purposes

- Code Style: (Important: follow these rules strictly)
  - All code comments in English only
  - Clear, descriptive variable names (English only)
  - All function names in English only
  - All error messages in English
  - NEVER use emojis in code, comments, log messages, or outputs
  - Use plain text symbols like [SUCCESS], [ERROR], [WARNING] instead of emojis

- Code Style Preferences:
  - Use modern C++17 features
  - RAII and smart pointers preferred
  - Comprehensive error handling
  - Thread-safe implementations for concurrent operations
  - Detailed logging for debugging (English messages only)
  - IMMEDIATE EMOJI REPLACEMENT: If ANY emoji is detected in code, comments, or log messages, replace it immediately with plain text equivalent like [SUCCESS], [ERROR], [WARNING], [INFO]

- Testing Approach:
  - Unit tests for core components
  - Mock implementations for testing

- ESP32 / PlatformIO Project Guidelines:
  - Build target rule: For testing/flashing, use the example `examples/Full-GUI-Demo` by default unless the user explicitly requests another target. For publishing/release verification, build the minimal/root project first.
  - Build validation: Always run the relevant PlatformIO build for at least one ESP32 environment from platformio.ini (e.g. "pio run -e <env>"). If tests are affected, also run "pio test -e <env>".
  - Memory/flash safety: After changes to WebUI/HTML content or large strings, verify binary size and memory behavior (heap/PSRAM) to avoid runtime instability on ESP32.
  - Settings migration: Any change to the settings structure must be backwards-compatible (defensive defaults) and should include a migration/versioning strategy to prevent OTA updates from breaking existing devices.

- Versioning Guidelines:
  - Single source of truth: The project version is defined in library.json. Do not introduce additional independent version numbers.
  - Keep consumers in sync: When the version changes, update any displayed/served versions (e.g. the firmware /version endpoint and the WebUI package version) to match library.json.
  - SemVer bump rules (MAJOR.MINOR.PATCH):
    - PATCH: Bugfix only, no breaking changes, no new settings required.
    - MINOR: New backwards-compatible features, new settings with safe defaults, additive API.
    - MAJOR: Breaking changes (API, settings schema without seamless migration, behavior changes that require user action), or removed/deprecated features.

- File Organization Guidelines:
  - PowerShell scripts for automation preferred, Python only if necessary
  - ALL documentation and development notes go in /docs/ directory
  - ALL tests (unit, integration, scripts) go in /test/ directory (PlatformIO default)
  - ALL tools go in /tools/ directory
  - Keep root directory clean - only essential project files (README.md, library.json, etc.)
  - GitHub-specific files (.github/FUNDING.yml, SECURITY.md, etc.) stay in their conventional locations
  - Move obsolete or temporary files to appropriate directories or remove them

- Research & Idea Management:
  - ALWAYS check the /dev-info/ directory for existing solutions and research before starting new tasks
  - Review all contents including links to avoid redundant work if solutions already exist
  - Reference existing ideas and implementations to build upon previous work

- Documentation & External Information Handling:
  - When fetching information from the internet (APIs, documentation, etc.):
    1. Create a subdirectory in /dev-info/ for the specific source
    2. Save important information in organized .md files with timestamps
    3. Include source URLs and fetch dates for reference
    4. Before fetching new information, ALWAYS check existing /dev-info/ subdirectories first
    5. Only fetch from internet if information is missing or outdated
  - Example structure: /dev-info/coinbase-api/endpoints.md, /dev-info/coinbase-api/authentication.md
  - Include version info and last-updated dates in saved documentation
  - This prevents unnecessary re-fetching and preserves valuable research

- TODO Management & Project Tracking:
  - ALWAYS keep /docs/TODO.md updated with current project status (create it if missing)
  - Update TODO.md immediately after completing any major feature or milestone
  - Mark tasks as [COMPLETED] when done, [CURRENT] when working on them
  - Add new priorities and features as they emerge during development
  - Reference TODO.md to understand current priorities and completed work
  - Mock/Demo Data Policy: All simulated data MUST be clearly labeled with [MOCKED DATA] prefix [MOCKED DATA] labels must be placed at the BEGINNING of log messages, not at the end
  - Never present fake data as real without explicit warning to user
  - Rule: Always prefix simulated/demo content with "[MOCKED DATA]" at the start of the message
  - CRITICAL: NO MOCK DATA WITHOUT EXPLICIT USER REQUEST - only implement mock/test data when user explicitly asks for it
  - Production code should always fetch real data from APIs - mock data only for testing when specifically requested

- Executable Path Policy:
  - Scope note: The full-path rule applies to PowerShell commands that invoke executables (especially when paths may contain spaces). It does NOT apply to git commands (e.g. `git status`, `git diff`, `git commit`) which can be run normally.
  - ALWAYS use full absolute paths for executables to prevent PowerShell path confusion
  - CRITICAL: Use PowerShell call operator (&) when using quoted full paths, OR change directory first and use relative paths
  - NEVER use relative paths like .\Release\executable.exe without changing directory first
  - When running executables in PowerShell, always use the complete full path from C:\ OR change to the directory first
  - PlatformIO builds (PowerShell): ALWAYS use `Push-Location` into the target example/project folder, resolve `pio` via `Get-Command`, run the build, then `Pop-Location`.
    - Example: `Push-Location "C:\\...\\examples\\Full-IO-Demo"; $pio=(Get-Command pio).Source; & $pio run -e usb; Pop-Location`

- Problem Resolution Policy:
  - Never mark problems as "solved" or "fixed" until user explicitly confirms the fix works
  - Always test end-to-end functionality before claiming success
  - If tests fail or user reports continued issues, acknowledge the problem persists
  - Only declare success when: (1) All tests pass in Docker CI, OR (2) User explicitly confirms the fix works

- FINAL CI/CD POLICY - MASTER MERGE REQUIREMENTS:
  - MANDATORY: ALL configured CI checks must pass before any merge to master/main
  - ZERO TOLERANCE: No exceptions for failed CI checks - automatic PR rejection
  - VALIDATION (PlatformIO): Run the PlatformIO build/tests relevant to the change (e.g. pio run, pio test) before merging
  - VALIDATION (Docker): If Docker CI is configured (e.g. Dockerfile.ci exists), validate using the documented Docker command before merging
  - ARCHITECTURE SUPPORT: Both Windows development and Linux production must work (where applicable)
  - DOCUMENTATION: README.md and /docs/TODO.md must be current when changes impact usage/behavior
