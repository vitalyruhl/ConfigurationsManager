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
  - Stage/commit/push nur auf ausdrückliche Aufforderung; wenn gestaged wird, dann bevorzugt `git add -A`.
  - Pushes erfolgen nur, wenn der Nutzer es verlangt (kein Auto-Push nach jedem Commit).
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
  - C++ String Handling Preference (C++17):
    - Prefer `std::string_view` for read-only string parameters (logging, parsing, lookups, comparisons) to avoid heap allocations and copies, especially on ESP32.
    - Use `std::string` only when ownership or mutation is required (store the value, build/modify the string, return an owned result).
    - Do NOT store `std::string_view` in members/containers/queues unless the referenced storage is guaranteed to outlive the view (e.g., string literals or static storage). When in doubt: store `std::string`/`String`.
    - Do NOT assume `string_view::data()` is null-terminated. For Arduino/Print and binary-safe output, prefer `write(data, size)` over `print(data)`.
    - For C APIs requiring null-termination, convert explicitly to an owning string/buffer before calling the API.
    - Use `const std::string_view` or plain `std::string_view` for parameters; use `std::string_view` for tags/prefixes in logging; keep formatting functions (`printf`/`vsnprintf`) as `const char* fmt`.
    - If returning an exact forwarded type is required (refs/const), prefer `decltype(auto)`; otherwise use `auto` for local variables to reduce noise.
  - RAII and smart pointers preferred
  - Comprehensive error handling
  - Thread-safe implementations for concurrent operations
  - Detailed logging for debugging (English messages only)
  - IMMEDIATE EMOJI REPLACEMENT: If ANY emoji is detected in code, comments, or log messages, replace it immediately with plain text equivalent like [S], [E], [W], [I]
  - Callbacks & Behavior Injection Policy (C++17 / ESP32):
    - Prefer callbacks over preprocessor conditionals (`#ifdef`, `#ifndef`) when behavior needs to vary at runtime or between modules.
    - Use `std::function` for callbacks, hooks, filters, and extension points when:
      - behavior should be configurable or replaceable at runtime
      - multiple implementations are possible (e.g. logging filter, event handler, WebUI hooks)
      - testability and decoupling are more important than minimal call overhead
    - Typical valid use-cases for `std::function`:
      - logging filters and format hooks
      - configuration change callbacks
      - event dispatchers and observers
      - optional features enabled/disabled via callbacks instead of `#ifdef`
    - Avoid `std::function` in hot paths, tight loops, ISR contexts, or real-time critical code.
    - Prefer function pointers (`void (*fn)(...)`) when:
      - the callback has no captures
      - the implementation is fixed and performance-critical
    - Prefer templates or inline lambdas when:
      - the behavior is known at compile time
      - zero-overhead abstraction is required
    - Do NOT use macros to implement behavior switching or callbacks.
      - Macros are allowed only for compile-time feature flags, platform selection, or build configuration.
    - Replace chains of `#ifdef` behavior switches with:
      - callbacks (`std::function`)
      - strategy objects
      - or dependency injection via constructor/setter
    - Time Handling Policy (`<chrono>` on ESP32, C++17):
      - Use `<chrono>` exclusively for expressing durations, timeouts, intervals, and rate limits to ensure unit safety and readability.
      - Treat Arduino/ESP32 time sources (`millis()`, `micros()`) as raw clocks only.
        - Immediately convert their values to `std::chrono::duration` types.
      - Prefer `std::chrono::milliseconds` as the default internal time unit.
      - Use `std::chrono` literals (`100ms`, `1s`, `5min`) for configuration and comparisons.
      - Do NOT use `<chrono>` as a real-time clock:
        - Avoid `system_clock`, `high_resolution_clock`, or calendar/date logic without RTC/NTP.
      - Do NOT mix raw integer time values and `std::chrono` durations in APIs.
        - Public APIs must accept `std::chrono::duration` parameters instead of plain integers.
      - Assume `millis()` wrap-around behavior (~49 days) and write comparisons using duration arithmetic (`now - last >= interval`).
      - Avoid `<chrono>` in ISR contexts.
      - `<chrono>` usage must remain allocation-free and constexpr-friendly.
- Naming & API Consistency Policy:
  - The authoritative naming rules are defined in docs/NAMING.md.
  - Enforce the dominant token order for API names:
    - <verb><SignalType><Direction><Target><Context>
    - Example: addAnalogInput (NOT addInputAnalog)
  - Specificity rule:
    - If a method is specific to a signal type, it MUST include that token (Digital/Analog/Adc/Dac/Pwm).
    - Example: Digital-only methods must be named addDigitalInputToGUI (NOT addInputToGUI).
  - Unify concept vocabulary:
    - Use exactly ONE term per concept (e.g. State vs Status -> choose State).
    - Do not introduce synonyms or mixed acronyms (Wifi vs WiFi etc.). Follow the repo’s chosen form.
  - Since the library is NOT published yet:
    - Renames can be performed directly (no deprecation wrappers required).
  - Safety gate for renames:
    - Before renaming: search references.
    - After renaming: update ALL call sites (src, examples, docs snippets, docs tables, tests).
    - Ensure the project still builds (PlatformIO).

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

- Tooling & Search Policy:
  - The agent may freely use standard CLI tools available in its sandbox
    (e.g. rg/ripgrep, sed, awk) for analysis, refactoring safety, and audits.
  - The user environment provides ripgrep (rg) on Windows / PowerShell.
  - The agent MAY suggest rg commands to the user when helpful.
  - When suggesting search commands:
    - Prefer rg for code search (speed, accuracy, gitignore-aware).
    - Include file scope where reasonable (e.g. src/, include/, docs/, examples/).
  - Do NOT assume other Unix tools (sed/awk) are available locally unless explicitly required.
  - If a command is sandbox-only or non-portable, summarize the result instead of asking the user to run it.

- Rename Safety Rule:
  - Before any rename, always perform a full reference search using rg.
  - After renaming, re-run rg to ensure no old symbol names remain.
  - A rename is considered incomplete if any reference remains in:
    - src/
    - include/
    - examples/
    - docs/
    - tests/
- Global Logging Severity & Efficiency Policy (ESP32 / Flash-Optimized):

  - ALL log messages MUST use short, ASCII-only severity tags.
  - Allowed severity tags (mandatory, global):
    - [E] = Error (fatal or incorrect behavior)
    - [W] = Warning (unexpected but recoverable behavior)
    - [I] = Info (important state or decision)
    - [D] = Debug (developer-oriented diagnostics)
    - [T] = Trace (very low-level, noisy, step-by-step diagnostics)

  - Long tags are FORBIDDEN everywhere:
    - [ERROR], [WARNING], [INFO], [DEBUG], [SUCCESS], etc.

  - Verbose is NOT a severity level:
    - Verbose is a build/runtime flag controlling WHETHER logs are emitted.
    - Severity tags describe WHAT the log means.
    - Verbose / trace-enabled logs may still use [I] or [W] if semantically correct.
    - Use [T] only for very fine-grained, noisy diagnostics.

  - This policy applies globally with NO exceptions:
    - core code
    - libraries
    - IO / hardware logs
    - GUI-related logs
    - debug and trace output

- Log Message Efficiency Rule (Flash-Aware):

  - Log messages must be semantically clear but as short as reasonably possible.
  - Prefer compression over verbosity to reduce flash usage, even if logs are compiled out in release builds.
  - Avoid filler words and redundancy when meaning stays clear:
    - "already exists" -> "exists"
    - "both enabled" -> "both ON"
    - "using pull-up" -> "use pull-up"
  - Prefer standard abbreviations consistently:
    - cfg, init, ok, fail, pull-up/down, adc, pwm, io
  - Keep function names short when included in logs:
    - addAO instead of addAnalogOutput
  - Do NOT repeat information already encoded by:
    - severity tag ([E]/[W]/[I]/[D]/[T])
    - module prefix ([IO], [CM], etc.)
    - surrounding function or context

- IO / Low-Level Logging Guidance:

  - IO and hardware-level logs are typically debug-oriented.
  - Treat IO_LOG output as debug/trace by default and keep especially compact.
  - Prefer [W] or [T] for IO diagnostics unless the condition is truly critical ([E]).

- Emoji Policy (Hard Rule):

  - Emojis are forbidden everywhere:
    - code
    - comments
    - log messages
    - outputs
  - Use ASCII-only severity tags:
    - [E], [W], [I], [D], [T]

- Logging Examples (Guidance Only):

  - BAD:
    IO_LOG("[WARNING] Input '%s': pull-up and pull-down both enabled, using pull-up", id);

  - GOOD:
    IO_LOG("[W] Input '%s': pull-up/down both ON, use pull-up", id);

  - TRACE example:
    IO_LOG("[T] ADC raw=%u, avg=%u", raw, avg);
