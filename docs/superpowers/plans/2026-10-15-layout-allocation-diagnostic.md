# Layout Allocation Failure Diagnostic Plan

## Verified state

Parent HIL candidate `7dcaec2a1b0d0f01` returns valid config (5,257 bytes) and metadata (9,964 bytes). Heap returns to approximately 30,008 free / 14,836 largest after metadata; weak ownership therefore removes the previously persistent payload allocation on this target. Layout still returns 503 after 15 seconds at stable baseline heap. The failure guard works; the exact failing generation stage is not yet known.

## One diagnostic build

1. Extend the checked serializer with an optional fixed-size result structure: reason (document overflow, size bound, reserve failure, append failure, length mismatch or success), expected bytes, written bytes and stored bytes before clearing a failed result. This must not change existing success/failure behavior.
2. In the layout method, capture free heap and largest 8-bit block before document construction and directly before serialization. On failure, emit one short serial line with the reason, document overflow flag, byte counts, numeric page/placement counts and before/built/failed heap samples. Never output content, labels, names, settings or secrets.
3. Because CM_ENABLE_LOGGING=0 removes CM_LOG, use a clearly marked temporary diagnostic block: stack char buffer, bounded snprintf and Serial.write. Do not enable all logging or alter firmware build flags. The parent controls initialized serial hardware and performs one clean USB build/flash and isolated layout request. This diagnostic is removed or disabled after the evidence is captured.
4. Extend real-ArduinoJson host checks to assert each result reason and preserve all existing output-integrity assertions. No schema, traversal, error-status or allocation-policy changes in this diagnostic step.

Heap deltas are observations at boundaries, not exact layout attribution: concurrent application tasks can allocate between samples. ArduinoJson 7 memoryUsage() is not used as a document allocation measurement.

## Decision after evidence

- Document overflow: investigate the full DOM allocation peak. Measured output from an overflowed document must never be treated as complete layout size.
- Reserve failure with complete document: final output plus full DOM coexistence exceeds the available contiguous block.
- Append or length failure after successful full reservation: inspect writer semantics/concurrent mutation before proposing a memory architecture change.

## Conditional implementation design (not yet approved)

If the full-document/output peak is confirmed, replace the complete DOM with a two-pass checked layout writer. Keep the existing page/card/group/placement traversal, ordering, normalization, defaults and JSON schema. The first pass counts exact encoded bytes; reserve the response String once; the second pass writes those bytes with the safe character append writer and verifies the exact final length. Use ArduinoJson for escaped scalar values or bounded small fragments, never hand-written escaping. Both passes use the same traversal and must fail closed on any allocation/write failure. Avoid per-card payload files, a second complete String or persistent caches. Tests compare complete parsed layouts and ordering against representative existing fixtures, cover escaped names/empty layout/placement mappings, and inject failures. Concurrent layout mutation consistency must be assessed before choosing the final traversal implementation.

## Status

Diagnostic proposal submitted to parent. No source changes or firmware build performed for this phase.


## Target result and approved narrow fix

Captured diagnostic:

```text
[CM layout diag] reason=reserve overflow=0 expected=6422 written=0 stored=0 pages=3 placements=62 free=28708/18500/18500 largest=14836/5108/5108
```

The document was complete. The 6,422-byte output reservation failed after document generation reduced the largest available block to 5,108 bytes. Parent approved option A: call the identical builder twice, destroy the first document and all builder-local vectors after measurement, reserve the output before constructing the second document, then verify the second size and checked serialization. This changes allocation order, not schema or traversal. The bounded-fragment alternative remains deferred.

Implementation uses `buildJsonWithReservedOutput` in CheckedJson.h. Overflow in either pass, failed reservation, a changed size between passes or failed output serialization returns an empty result and retains the route's 503 behavior. The temporary serial diagnostic was removed completely. Provider concurrency and total heap availability can still cause a clean 503; success on the target is not yet claimed.

## Focused verification

- `run-reserved-json-host-tests.ps1` PASS: real ArduinoJson allocation tracking confirms all first-document blocks are gone when output reservation starts and the second builder begins only after that reservation. The actual production two-pass helper matches the single-document golden JSON (4,013 bytes; three pages, 62 placements, escaped names, arrays and orders). Both-pass document OOM, reserve failure, changed second-pass length, output write failure and an empty layout are covered.
- The host fixture is not the full ConfigManagerClass. Separately, a token comparison of the actual production populateLayout body against HEAD passed after stripping only indentation and prior serialization/return plumbing: schema assignment, traversal, sorting and escaping inputs are unchanged.
- Checked JSON, bounded JSON (six scenarios) and complete gzip (59,553 bytes) host regressions PASS; diff-check PASS.
- Target clean USB build/HIL remains parent-owned. Two document-generation passes add low-frequency CPU work; device timing remains measured rather than inferred.


## Final fragment mitigation

Option A was insufficient on target: second_overflow, expected6422, free28552/18372/28552/22104/13804, largest14836/5108/14836/8180/500. Parent approved replacing the full document with one page fragment and one placement fragment at a time. Two passes count/reserve/write one response String; dynamic keys/values are escaped by ArduinoJson. Duplicate placement keys preserve first position/last value. Unused full-DOM two-pass helper and its superseded tests were removed; all temporary diagnostics removed.

Focused fragment golden test PASS (4200 bytes), including escaped keys/names, duplicates, empty output, fragment lifetime, both-pass OOM, reserve/change/write failure. Checked JSON, bounded JSON and complete gzip regressions PASS. Independent review completed with no new findings. Final fragment source frozen for parent baseline build; subsequent page-scoped enhancement is separately planned.
