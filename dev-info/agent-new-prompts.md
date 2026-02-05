You are an autonomous repository agent operating inside this GitHub repository.

Primary principle:
- API naming must be stabilized FIRST.
- Documentation must be generated ONLY AFTER naming is finalized.

The library is NOT published yet:
- Direct renames are allowed.
- No deprecation wrappers are required.
- All references MUST be updated.

Global rules:
- Work strictly inside this repository.
- Do not delete code automatically.
- Do not invent APIs.
- Always update ALL references when renaming:
  source code, headers, examples, docs tables, docs code blocks, tests.
- Overloads and overrides count as separate API variants.
- Always include file paths and full signatures in reports.

=================================
PHASE 0 — Repository mapping
=================================
1) Detect:
   - Source roots (include/, src/, lib/, etc.)
   - Examples roots (examples/, samples/, demo/, etc.)
   - Documentation directory (docs/)
2) Define "public API":
   - All headers in include/
   - All classes/functions intended for consumer use
3) Define "examples-shown API":
   - Any symbol referenced in:
     - examples/** code
     - README*.md code blocks
     - docs/**/*.md usage snippets

=================================
PHASE 1 — Naming audit (READ-ONLY)
=================================
Goal: identify all naming convention violations without changing code.

Authoritative naming rules:
A) Method shape:
   <verb><SignalType?><Direction?><Target?><Context?>

   Verbs: add, remove, configure, set, get, is, has, begin, update
   SignalType tokens: Digital, Analog, Adc, Dac, Pwm (use repo-dominant form)
   Direction tokens: Input, Output
   Target tokens: State, Value, Settings, Runtime, Alarm, Events, Gui
   Context tokens: ToGUI, WithAlarm, Raw, Volt, Slider, etc.

B) Required token order:
   Verb + SignalType + Direction + Target + Context
   Example: addAnalogInput (NOT addInputAnalog)

C) Specificity rule:
   If a method is signal-type-specific, it MUST include that token.
   Example:
   - Digital-only GUI helpers must use addDigitalInputToGUI
     (NOT addInputToGUI).

D) Concept unification:
   - Use exactly one term per concept.
   - No synonyms (State vs Status, Analog vs Adc, etc.).
   - Enforce consistent acronym casing (GUI, DAC, IO, Wifi).
   
E) Subsystem context rule:
- Method names MUST NOT repeat the owning subsystem or class name.
- If a symbol is accessed via an instance or namespace (e.g. mqtt.*),
  the subsystem identifier (e.g. Mqtt, MQTT) is forbidden in the method name.
- Subsystem identity belongs to the type/namespace, not the method.
Examples:
  mqtt.addTopicToSettingsGroup()   ✔
  mqtt.addMqttTopicToSettingsGroup() ✘ (redundant context)

Tasks:
4) Parse all public API method names.
5) CamelCase-split names into tokens.
6) Infer dominant naming patterns per verb group.
7) Flag violations:
   - Token order inversion
   - Missing required qualifiers
   - Synonym drift
   - Acronym casing inconsistencies
8) Produce /review/naming.inconsistencies.md with table:
   | Symbol | Signature | Location | Current name | Proposed name | Rule violated | Notes |

9) Produce /review/rename.plan.md containing:
   - Ordered rename list (old → new)
   - Rationale per rename (rule reference)
   - All impacted files (paths)

Do NOT modify code in this phase.

=================================
PHASE 2 — Naming refactor (WRITE)
=================================
Goal: apply the approved naming rules mechanically and completely.

10) Apply all renames from rename.plan.md.
11) Update ALL references:
    - Headers and source files
    - Examples
    - Docs code blocks and references
    - Tests
12) Ensure no mixed old/new names remain.
13) Ensure the project still builds (PlatformIO at least one env).

Output:
- Updated codebase with consistent naming.
- rename.plan.md updated with "APPLIED" status per rename.

=================================
PHASE 2.5 — Type & value-semantics audit (READ-ONLY)
=================================
Goal: improve correctness and performance by auditing:
- auto vs decltype(auto)
- std::string vs std::string_view
- accidental copies, lost references, and unnecessary allocations (ESP32).

Rules (authoritative):
A) auto vs decltype(auto)
- Use `auto` for values when copying is intended.
- Use `decltype(auto)` when forwarding return types or preserving references is required.
- If binding to an expression that may be an lvalue reference (e.g., function returning T&, container element access, operator[]),
  prefer:
  - `auto&` / `const auto&` when a reference is intended and lifetime is stable
  - `decltype(auto)` for perfect preservation when the expression type matters (ref/const).
- Do NOT use decltype(auto) if it obscures ownership or lifetime.
- Flag suspicious cases:
  - `auto x = someRefReturningExpr();` (reference lost → copy)
  - `auto x = container[i];` when container returns reference and copy is unintended
  - `auto x = getStringView();` assigned to std::string_view where source lifetime is temporary/unsafe

B) std::string vs std::string_view
- Prefer `std::string_view` for read-only parameters (logging, parsing, lookups) to avoid allocations.
- Use `std::string` when ownership or mutation is required, or when data must outlive the caller scope.
- Never store `std::string_view` in members/containers unless the referenced storage lifetime is guaranteed.
- Do not assume `string_view::data()` is null-terminated; use size-aware APIs.
- For C APIs requiring null-termination, convert to owning string/buffer explicitly.

Scope:
- Apply primarily to public API, hot paths, and frequently-called functions.
- Be conservative with changes that affect lifetimes.

Output:
- Create/overwrite /review/type.semantics.md with tables:
  1) auto/decltype(auto) findings:
     | Location | Code | Issue | Safer alternative | Notes |
  2) string/string_view findings:
     | Symbol/Location | Current signature/usage | Issue | Proposed change | Lifetime notes |

Do NOT change code in this phase.

=================================
PHASE 2.6 — Type & value-semantics refactor (WRITE)
=================================
Goal: apply safe, mechanical improvements based on Phase 2.5.

Rules for applying changes:
- Only apply changes that are clearly safe regarding lifetime and semantics.
- Prefer changing function parameters from `const std::string&` to `std::string_view` for read-only usage,
  unless the function stores the value.
- Prefer `auto&` / `const auto&` over `decltype(auto)` when readability is better and the intent is “bind by reference”.
- If a proposed change could alter behavior or lifetime safety, record it in the review file but do not apply it.

Update:
- All call sites, including examples/docs/tests.
- Ensure the project still builds (PlatformIO at least one env).

Then continue with documentation normalization using the final signatures.

=================================
PHASE 3 — Documentation normalization
=================================
Goal: regenerate docs based on FINAL API names.

14) For every docs/*.md:
    - Ensure exactly one section "## Method overview".
    - Place it after any introductory text.
15) Under that section, insert a table with header:
    | Method | Overloads / Variants | Description | Notes |
16) Populate tables using the FINAL API:
    - Fully qualified names if ambiguous
    - List all overloads explicitly (use <br>)
    - Short, factual descriptions
    - Notes: Used in examples (yes/no), Advanced, Runtime-only, etc.
17) Preserve existing doc structure; only insert/augment.

=================================
PHASE 4 — Undocumented & unused API scan
=================================
18) Identify public API symbols missing documentation:
    - Not in any docs "Method overview" table
    - Not documented via structured comments
19) For each undocumented symbol:
    - Add entry to /review/unused.undocumented.md
    - Immediately add it to the appropriate docs table
      (or create a new docs page if needed).

20) Identify public API symbols not shown in examples:
    - Not referenced in examples/**
    - Not referenced in README/docs usage snippets
21) Classify:
    - Advanced but useful
    - Likely obsolete / redundant
    - Internal accidentally public
22) Record findings in review files with recommendations
    (keep, document only, add example, consider removal later).

=================================
PHASE 5 — Final consistency check
=================================
23) Verify:
    - /review exists
    - naming.inconsistencies.md exists
    - rename.plan.md exists and is applied
    - unused.undocumented.md exists
    - unused.deprecated.md exists
    - Every docs/*.md has "## Method overview"
24) Check:
    - No broken markdown tables
    - No outdated method names in docs
    - All overloads listed consistently

=================================
PHASE 6 — Final report
=================================
25) Provide a summary:
    - Total renames applied
    - Docs files modified
    - Undocumented APIs fixed
    - APIs not shown in examples (top candidates to add)
    - Any remaining naming edge cases

Execute all phases strictly in order.
