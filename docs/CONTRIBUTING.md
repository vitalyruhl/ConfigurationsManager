# Contributing Guidelines

These guidelines define how to work in this repository. The project targets ESP32 / PlatformIO and uses modern C++17.

## 1. Where things live

- **Documentation:** `docs/`
- **Review reports (generated):** `review/`
- **Tests:** `test/`
- **Tools / helper scripts:** `tools/`
- **GitHub files:** `.github/`
- Keep the repo root clean (only essential project files like `README.md`, `library.json`, etc.)

Recommended docs files:
- `docs/NAMING.md` – API naming rules (authoritative)
- `docs/CONTRIBUTING.md` – this document
- `docs/TODO.md` – project tracking

## 2. Branching model

- `main`: published / released branch (do **not** develop directly here)
- `release/v3.3.0`: runnable snapshot branch (must always build/run)
- `feature/*`: work-in-progress branches (may be unfinished/broken)
- Suggested naming:
  - `feature/<short-topic>`
  - `fix/<short-topic>`
  - `chore/<short-topic>`
  - `docs/<short-topic>`

## 3. Code style (hard rules)

- All code comments **English only**
- Clear, descriptive names (**English only**)
- All log/error messages **English only**
- **No emojis** in code/comments/log output. Use `[INFO]`, `[WARNING]`, `[ERROR]`, `[SUCCESS]`.
- Prefer C++17 features where appropriate
- Prefer RAII + smart pointers
- Avoid `std::function` in hot paths / ISR contexts

## 4. String handling (ESP32 / C++17)

- Prefer `std::string_view` for read-only parameters (logging, lookups, parsing)
- Use `std::string` when ownership/mutation is needed
- Never store `std::string_view` in members/containers unless lifetime is guaranteed
- Do not assume `string_view::data()` is null-terminated

## 5. Time handling

- Use `<chrono>` for durations/timeouts/intervals (unit safety)
- Treat `millis()` / `micros()` as raw clocks; convert to `std::chrono::duration` early
- Use wrap-safe comparisons (`now - last >= interval`)

## 6. Naming convention (API)

The authoritative naming rules live in `docs/NAMING.md`.

When changing public API names:
- If the library is **not yet published**, renames can be done directly (no deprecation required).
- Always update **all** call sites:
  - source code
  - examples
  - docs tables / snippets
  - tests
- Do not introduce synonyms (`State` vs `Status`, `Analog` vs `Adc`, etc.). Pick one concept name and use it everywhere.

## 7. Documentation requirements

- Every `docs/*.md` must contain a **single** `## Method overview` section with a table:
  | Method | Overloads / Variants | Description | Notes |
- Overloads and variants must be listed explicitly (one per line using `<br>`).
- Keep descriptions short, factual, and consistent.

## 8. Review artifacts (generated)

Automation may generate:
- `review/unused.deprecated.md`
- `review/unused.undocumented.md`
- `review/naming.inconsistencies.md`
- (optional) `review/rename.plan.md`

These files help decide what to keep, document, demonstrate in examples, or remove later.

## 9. Testing / build validation

- Default target for flashing/testing is typically `examples/Full-GUI-Demo` unless specified otherwise.
- Before merging into `release/*` or `main`:
  - Run at least one PlatformIO build: `pio run -e <env>`
  - If tests are affected: `pio test -e <env>`

## 10. Git workflow etiquette

- Prefer small, reviewable commits
- Do not stage/commit/push unless explicitly requested in the current task context
- Before rename/delete:
  - Search references and update them
  - Ensure builds still pass

## 11. CI / merge policy

- All configured CI checks must pass before merging to `main`
- No exceptions for failed CI
- Keep `README.md` and `docs/TODO.md` current when usage/behavior changes

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.begin` | `begin()` | Starts ConfigManager services and web routes. | Used in examples: yes. |
