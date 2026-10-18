# Compact Runtime Metadata Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking. No automatic task or final commits.

**Goal:** Add backward-compatible, purpose-specific CM metadata storage that
reduces retained RAM while preserving legacy application behavior.

**Architecture:** Keep legacy RuntimeFieldMeta records and add a lazy compact
store with separate typed vectors, shared routing text and sparse extensions.
Use one view-based serializer/layout path; explicitly promote a compact field
when a legacy mutable-pointer request requires a real RuntimeFieldMeta.

**Tech Stack:** C++17, ESP32 Arduino, ArduinoJson 7.4.3, PlatformIO, current
bounded JsonOutput serializer and focused host/embedded tests.

**Spec:** [Compact runtime metadata design](../specs/2026-10-18-compact-runtime-metadata-design.md).

**Status:** Owner-approved on 2026-10-18; implementation explicitly requested
after enriching restart context. Read the [handoff and execution record](2026-10-18-compact-runtime-metadata-handoff.md) first when resuming.

## Global Constraints

- Public API remains additive; no SIL/SBT migration here.
- No automatic legacy-to-compact migration or deprecation is introduced.
- Normal compact serialization performs no legacy materialization.
- No second permanent legacy mirror for a compact field.
- Preserve cache-mutex then data-mutex ordering and bounded count/write output.
- No NVS, settings schema, HTTP/WebSocket protocol, OTA, dependency or build
  pipeline redesign.
- No automatic task or final commits. Commit and branch operations below occur
  only after explicit owner approval of this review package.
- Push, main integration, branch deletion, release and device actions are not
  included.
- Proposed implementation version is 4.7.0; planning does not change 4.6.1.

## Review Focus

1. Legacy raw pointers into newly compact fields: promote once, never return a
   temporary or leave a full retained mirror (Task 3).
2. Mixed duplicate keys and cross-page reuse: preserve characterized matching,
   exact source/page selection and ordering (Tasks 1 and 3).
3. Long/dynamic text, UTF-8 and embedded NUL: owned strings/IDs remain valid
   after table/vector growth and retain existing JSON behavior (Tasks 2 and 3).
4. Concurrent registration/update, revision invalidation and response lifetime:
   one stable view for both passes, no stale cache or dangling view (Task 3).
5. Sparse options and unused integration: account for capacities and overhead;
   Bool/Int fields and legacy-only applications pay no hidden large payload
   or eager compact-store allocation (Tasks 2 and 4).

## Files and responsibilities

New production files, names proposed with this plan:

- src/runtime/RuntimeFieldTypes.h: public small types, IDs and extension types.
- src/runtime/detail/CompactRuntimeMetaStore.h: typed storage, text IDs,
  sparse extensions, handle mapping and promotion bookkeeping.
- src/runtime/detail/RuntimeMetaView.h: locked read adapters for both formats.

Affected existing files:

- src/runtime/RuntimeManager.h and .cpp: additive registration/update/access,
  legacy bridging, sorting, locks, cache revisions and development override.
- src/runtime/detail/RuntimeMetaJson.h: shared view-based wire serialization.
- src/ConfigManagerLayout.cpp: placement/page filtering through shared views.
- src/ConfigManager.h: expose types and integrate internal page predicates;
  preserve old public overloads.
- src/alarm/AlarmManager.cpp: change internal metadata access only if necessary
  to avoid incidental promotion; no alarm-policy change.
- test/host and test/basictest.cpp: focused compatibility/storage tests.
- README.md, docs/CHANGELOG.md, docs/heap-memory-inventory.md and canonical
  version mirrors: usage, measured costs, limitations and minor-version update.

New documentation: docs/runtime-metadata.md, the single user-facing reference
for legacy/compact APIs, optional extensions and promotion behavior.

## Phase 0: Preserve fixes and create the feature branch

This phase is workflow work, after owner acceptance. Do not implement before it.

- [ ] Reinspect branch/worktree, fetch origin/main and read current governance;
  load docs-gate before integration. Recheck actual validation/source identity.
- [ ] Review only the pending MQTT files and their documentation plus this
  review package. Existing checkpoint e578c91 already preserves earlier work.
- [ ] Run git diff --check and reuse unchanged successful focused MQTT/SBT
  results only when source state is demonstrably identical; otherwise rerun
  the affected check. Do not rebuild unchanged UI solely for this checkpoint.
- [ ] Commit named reviewed paths on feature/hybrid-json-output. No push.
- [ ] Verify feature/jbd-bms-transport is still an ancestor and its checkout is
  clean/available. Switch there and merge --ff-only feature/hybrid-json-output.
  If it has diverged, report the actual divergence and revise this phase;
  do not invent a conflict resolution or silently choose a merge strategy.
- [ ] From that updated branch create feature/compact-runtime-metadata, after
  checking the name is unused. Preserve both existing branches.
- [ ] Record full resulting SHAs and selected execution method. Implementation
  requires explicit Level C authorization; owner plan acceptance can provide
  that authorization when stated together with the execution method.

## Task 1: Pin compatibility and establish allocation baselines

**Files:** test/host/runtime_meta_compatibility_test.cpp (new),
tools/test/run-runtime-meta-compatibility-host-tests.ps1 (new), existing
runtime_meta_json_test.cpp, and test/basictest.cpp.

**Interfaces:** consumes current public RuntimeFieldMeta registration, lookup,
update, filtering, JSON and layout APIs. Produces frozen expected legacy
behavior and reproducible baseline fixtures; no new production API yet.

- [ ] Characterize legacy Float/Int/Bool/Text/Divider and every control kind,
  numeric/bool alarms, styles, optional labels and unusual combined flags.
- [ ] Record exact omission/order/defaults, duplicate group/key selection before
  and after serialization, sourceGroup aliases, page reuse and development
  override semantics. Preserve observed legacy behavior rather than imposing
  new uniqueness or ordering rules.
- [ ] Test callback mutation and pointer lifetime under the existing documented
  mutation boundary; verify cache revision and layout behavior.
- [ ] Establish allocation-accounted fixtures: 64 Float fields with distinct
  short keys/labels and shared routing/unit, 64 Bool fields, a mixed/control
  fixture and an equivalent legacy-only application. Record string allocations,
  capacities, registration peak, retained bytes and serialization peak.
- [ ] Run the new host runner and existing runtime-meta JSON runner, expect
  baseline success. Record target compiler sizeof evidence separately from
  host allocation measurements. New compact assertions are added in Task 2
  and must fail before implementation.

## Task 2: Add small types, owned text and sparse storage

**Files:** new RuntimeFieldTypes.h and CompactRuntimeMetaStore.h;
new test/host/compact_runtime_meta_store_test.cpp and
new tools/test/run-compact-runtime-meta-host-tests.ps1;
RuntimeManager.h/.cpp additive registration path.

**Interfaces:** consumes Task 1 baselines. Produces spec-defined small types,
RuntimeFieldId, typed addRuntimeMeta overloads, typed updateRuntimeMeta overloads
and the five extension setters named in the design. Store methods remain
private implementation details, not a second public registration API.

- [ ] Write failing tests for each type's allowed members/defaults, handle
  stability, owned temporary input, shared routing text and sparse extensions.
  Include invalid/wrong-kind IDs, empty styles and cross-kind alarms.
- [ ] Run the compact host runner; verify failures are missing compact behavior.
- [ ] Implement owned public definitions, private compact stored records and lazy
  separate-vector storage;
  keep the legacy overload/storage unchanged. Use 32-bit IDs and no borrowed
  text, largest-alternative record, per-field polymorphic object or inline
  large optional payload. Define checked text/field ID exhaustion handling.
- [ ] Implement typed mutation/extension validation and atomic visible changes.
  Evaluate actual container/allocation failure behavior before claiming OOM
  recovery. Report a required allocator-design change for review rather than
  expanding into a library-wide allocator refactor.
- [ ] Run storage/lifetime tests and record retained memory including all table,
  handle, capacity and extension overhead. No app conversion in this task.

## Task 3: Integrate shared output, layout and legacy bridges

**Files:** new RuntimeMetaView.h; RuntimeMetaJson.h, RuntimeManager.h/.cpp,
ConfigManagerLayout.cpp, ConfigManager.h; relevant internal alarm access;
compatibility/store tests and test/basictest.cpp.

**Interfaces:** consumes typed records/IDs from Task 2. Produces shared locked
read traversal and preserves all existing public signatures. New public API
remains exactly the typed overloads/setters defined by the design.

- [ ] Add failing mixed-format tests for full/page-selected JSON, group/order/
  label ordering, duplicates, layout placement and legacy predicates.
- [ ] Add failing tests for raw-pointer promotion, handle remapping after
  promotion, representable callback updates remaining compact, legacy-only
  callback combinations promoting once and wrong typed updates failing safely.
- [ ] Add long-text/UTF-8/NUL and vector-growth tests; verify no normal compact
  serialization constructs legacy records or document trees.
- [ ] Implement view-based shared serialization, deriving wire defaults for
  absent stored fields. Use small sorting references, not full metadata copies;
  account for their scratch storage and preserve both-pass consistency.
- [ ] Implement legacy update/predicate adapters and deliberate raw-pointer
  promotion. Keep one real legacy record after promotion, release old owned
  resources, preserve IDs and invalidate the cache using established locks.
- [ ] Route internal selected-page/layout/initialization reads through views;
  preserve control handlers/provider callbacks and development override.
- [ ] Run focused host suites and existing runtime-meta cache/concurrency tests
  where runnable. Compile-only embedded test results must not be called runtime
  test passes; record separately when hardware execution is unavailable.

## Task 4: Verify savings, document adoption and prepare review

**Files:** new docs/runtime-metadata.md; README.md, docs/CHANGELOG.md,
docs/heap-memory-inventory.md; library.json, src/ConfigManager.h,
webui/package.json, webui/package-lock.json and discovered version mirrors.
Optional focused example: examples/CompactRuntimeMetadata/src/main.cpp and
its minimal PlatformIO configuration, reusing existing pinned dependencies.

**Interfaces:** consumes complete mixed-format behavior from Task 3. Produces
CM-only adoption documentation, memory evidence and a compatible minor feature.

- [ ] Run the same Task 1 fixtures against compact registrations. Require at
  least 25% retained metadata reduction in the 64-Float reference fixture,
  including text/index/capacity overhead. Report Bool and mixed cases without
  claiming identical savings. Legacy-only compact store must remain absent;
  additional manager object storage must be at most 16 target bytes.
- [ ] Compare target ABI sizes and linked static RAM/flash. Report allocation
  calls and peaks separately; no inferred device fragmentation improvement.
- [ ] Document a Float, Int, Bool and control example, optional numeric/bool
  alarms, style attachment and migration via additive API. Explain pointer
  promotion, predicate-copy peaks, owned strings and old-project behavior.
- [ ] Apply version-impact: proposed 4.7.0, synchronize canonical source/mirrors
  and clearly unpublished changelog. Do not rewrite historical release entries.
- [ ] Run affected host runners, relevant runtime-meta compile-only ESP32 test
  build and a focused CM example build. Run WebUI tests only if affected source
  or a wire-contract concern justifies them; do not run a full matrix by default.
- [ ] Run git diff --check, inspect final changes and perform one independent
  architecture/compatibility review of the new feature delta. Resolve material
  findings with focused checks. No automatic final commit, push or merge.
- [ ] Present CM results for owner acceptance, with hardware status explicitly
  separate. Only after CM is accepted, prepare separate SIL and SBT migration
  plans based on their actual registration/legacy-access inventory.

## Approval and completion

Owner review should cover the type model, compatibility/promotion boundary,
measurable memory gate and branch sequence. Recommend parent-led sequential
implementation with one independent final reviewer because these tasks share
storage/view interfaces; delegate a narrow specialist check only if needed.
The owner can choose task-by-task subagents instead. No implementation starts
until the review package and execution method are approved.

Planning validation: source/API references and branch ancestry checked;
document links/checklists/contract consistency and whitespace checked. No
product tests, builds, uploads or Git mutations are part of this planning turn.

## Execution outcome

CM 4.7.0 is implemented; read the linked handoff for completed tasks, exact
checkpoints, memory accounting, test/build evidence and COM7 settings proof.
The selected example was flashed and showed no serial crash markers in270s.
An intermittent inherited HTTP delivery timeout/truncation remains an explicit
acceptance limitation. The implementation is not automatically committed or
published, and SIL/SBT migration plans remain deferred until owner acceptance.
