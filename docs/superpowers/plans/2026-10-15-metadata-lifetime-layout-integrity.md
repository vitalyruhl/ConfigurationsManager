# Metadata Lifetime and Layout Integrity Plan

**Scope:** Release cached metadata after its final response owner, and never serve a partial layout as HTTP 200. No UI, dependencies, flags, firmware build or device action in this subtask. Preserve existing changes.

## Evidence

Clean firmware delivered valid HTML/config/metadata, followed by repeated 62-byte incomplete layout JSON with Content-Length 62. The long-lived metadata cache retained approximately 10 KB after delivery. ArduinoJson 7.4.3 ArduinoStringWriter flushes in 31-byte increments; failed String concat can retain a prefix. Its constructor clears/invalidate-frees a pre-reserved String, so reserving before the standard String serializer alone is insufficient. DynamicJsonDocument(16384) is a compatibility API, not a fixed 16 KB arena in version 7.

## Implementation

- Change only the metadata cache ownership from shared_ptr to weak_ptr. Preserve cache/data mutex order, single-flight build flag, revision and invalidation behavior. Lock weak references before use. Reuse a clean live snapshot; rebuild expired snapshots. Concurrent requests during a build receive a live old snapshot or null, as before when no cache exists. A raced candidate remains a complete dirty fallback; successful current revisions become clean. Responses keep the payload alive independently; the last strong owner frees it.
- Add one checked JSON serialization helper used by both layout return paths. Reject overflowed documents, measure the output, reserve once and serialize through a checked concat writer that does not clear the reservation. Require expected, reported and stored lengths to agree; discard every partial result. Keep the layout schema and sorting unchanged.
- Return 503 layout_unavailable for empty/failed layout generation. Keep successful response headers/auth/CORS and bounded transport intact.

## Validation

- Compile the actual production serialization helper with the installed ArduinoJson library. Generate representative escaped layout documents and force actual document allocation failure plus mocked destination reserve/concat failure; assert complete valid output or empty failure, never a partial success.
- Extend existing actual RuntimeManager target tests for shared response reuse, expired weak snapshot release/rebuild and invalidation. Clearly distinguish tests added from tests executed if the hardware suite is not run.
- Re-run bounded JSON/gzip host regressions and diff-check. Parent owns review and final USB build/HIL.

## Status

Parent approved before product edits. Implementation complete; source frozen for independent review and parent-owned clean USB build/HIL.

## Verification evidence

- `tools/test/run-checked-json-host-tests.ps1`: PASS with ArduinoJson 7.4.3. The actual production helper serializes a real escaped layout document, rejects a real document allocator failure (`overflowed()`), rejects mocked destination reserve failure and a concat failure at the 62-byte boundary, and clears partial output. Initial RED was the missing helper, not a claimed reproduction of the old Arduino String writer.
- `tools/test/run-bounded-json-host-tests.ps1`: PASS, six ownership/lifetime/transport scenarios and 24,029 exact JSON bytes.
- `tools/test/run-static-gzip-host-tests.ps1`: PASS, four scenarios with the complete 59,553-byte embedded gzip.
- `git diff --check`: PASS.
- Existing actual RuntimeManager target tests now include shared-owner reuse, weak snapshot expiration after all owners release it, and a subsequent rebuild count. These additions were not separately compiled or executed in this subtask; existing update/failure fallback target cases remain. The host transport tests establish response ownership/destruction but do not execute RuntimeManager cache orchestration.
- No claim of device heap recovery, no generator-wide memory gate and no UI change. A weak snapshot is intentionally rebuilt on a later request once all prior owners are gone; the frontend retains successful metadata independently.


## Review correction

Independent review found that pinned ESP32 String::concat(ptr, size) copies size+1 source bytes. ArduinoJson numeric formatting supplies non-terminated slices, so direct pointer concat is unsafe even after reserving enough output. The writer now uses concat(char) for individual bytes and loops over that safe overload for bulk slices. The test destination deletes pointer concat explicitly (RED on the old implementation, GREEN on the correction), and includes an exact two-byte unterminated slice plus actual JSON numeric serialization. This is a compile-time API constraint/regression, not an ASAN execution claim.

Parent explicitly approved the same precise correction in the existing metadata StringAppendPrint bulk override, which had the identical source over-read. Existing actual metadata round-trip target tests remain available but were not run here. The parent stopped the in-progress firmware build before this correction; a fresh clean USB build must use the reviewed final source.
