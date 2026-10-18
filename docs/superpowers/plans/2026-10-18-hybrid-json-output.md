# Hybrid JSON Output Implementation Plan

> **For agentic workers:** Use focused delegated implementation and independent review; preserve the user-approved scope and no automatic final commit.

**Goal:** Remove temporary JSON trees from CM-owned live schema output while keeping parser/provider compatibility.
**Architecture:** A reusable bounded count/write sink emits scalars and objects directly. Page index, layout and runtime metadata use it; response lifetime and transport are unchanged.
**Tech Stack:** C++17, ESP32 Arduino, ArduinoJson 7.4.3, PlatformIO, clang++ host tests.
**Spec:** ../specs/2026-10-18-hybrid-json-output.md

## Global Constraints
- Preserve external JSON contracts and ArduinoJson provider APIs.
- Avoid temporary JSON documents, dynamic escaping buffers and unbounded output writes.
- Retain input ownership across both passes; preserve metadata mutex coverage.
- Scope tests to changed paths; no hardware interaction or automatic final commits.

## Review Focus
- Control bytes, embedded NUL, UTF-8 and escaped property names remain valid JSON.
- Numeric boundaries, nonfinite values and overlapping field flags preserve previous semantics.
- Duplicate style targets/properties and layout IDs retain last-write behavior.
- Reserve failure, short write, count/write mismatch and size overflow reject partial output.
- Page filtering, empty layouts and weak metadata cache ownership remain unchanged.

## Tasks
- [x] Checkpoint and push prior docs; create feature/hybrid-json-output; keep Serena project configuration local.
- [x] Add sink tests first, prove missing behavior, implement src/web/JsonOutput.h with focused host runner. API: raw, string(pointer,length), string(string-like), value(scalar/string), member(bool& first,key,value), good, size; cm::buildJsonOutput(builder,output).
- [x] Convert page index/layout in src/ConfigManagerLayout.cpp. Preserve sorting and selection; test the production helper directly with frozen ArduinoJson host references.
- [x] Extract direct metadata serializer into src/runtime/detail/RuntimeMetaJson.h and use it in RuntimeManager.cpp. Add host golden/ownership/failure coverage against prior ArduinoJson behavior.
- [x] Classify compatible optimization as patch 4.6.1; synchronize version mirrors and add an explicitly unpublished changelog entry.
- [x] Run focused host suites, a relevant ESP32 build and compile-only firmware tests. Record commands/results and limitations.
- [x] Independently review final diff, repair substantive findings and report uncommitted implementation state.

## Execution Notes
The user explicitly requested implementation after agreeing the hybrid approach. Implementation proceeds under that approval; this plan records the agreed design. Roles: workflow for checkpoint/branch, docs for records, refactor for source/tests. Work stays in the existing checkout on the dedicated sidebranch; the separate consumer checkout remains untouched.

## Validation Evidence (2026-10-18)

- `tools/test/run-json-output-host-tests.ps1`: passed. Scalar boundaries, all control bytes, UTF-8, numeric compatibility, count/write bounds and failure cleanup. The float-only ArduinoJson configuration also passed.
- `tools/test/run-live-layout-json-host-tests.ps1`: passed. Production helper output matches the prior ArduinoJson reference for ordering, selection, special pages and duplicate placements.
- `tools/test/run-runtime-meta-json-host-tests.ps1`: passed, including all 16 range-flag combinations, duplicate properties/targets and embedded-NUL keys with ESP32 String comparison semantics.
- `tools/test/run-runtime-host-tests.ps1 -ArduinoJsonInclude .pio/libdeps/usb/ArduinoJson/src`: passed; existing selection/provider behavior remains covered.
- `pio test -e runtime-meta-test --without-uploading --without-testing -v`: final ESP32 compile/link passed (exit 0). PlatformIO reports runtime tests SKIPPED: zero test cases executed on hardware. This is compile-only evidence, not device acceptance.
- `git diff --check`: passed. Independent review found a length-insensitive String comparison regression in metadata duplicate handling; the new regression test failed before the length-plus-memcmp repair and passed afterward.
- Runtime metadata retains failure-stage and required-size diagnostics. HTTP response creation/transport, parser/provider APIs and consumer bindings are unchanged.

### Resource Evidence and Limits

The frozen 64-field/two-pass host metadata fixture made 1,024 allocator calls requesting 542,080 bytes cumulatively. This is neither peak memory nor an ESP32 measurement. Direct metadata serialization creates no JsonDocument trees. A pre-reserved core-writer test counted zero C++ new allocations; that counter does not intercept arbitrary CRT malloc. The response buffer, layout sorting and normalization still allocate.

Same-environment firmware comparison against the saved pre-change runtime-meta-test build:

| Metric | Before | After | Difference |
| --- | ---: | ---: | ---: |
| firmware.bin bytes | 572224 | 572272 | +48 |
| ELF text | 404045 | 404345 | +300 |
| ELF data | 168076 | 167816 | -260 |
| ELF bss | 13577 | 13577 | 0 |

No flash saving is demonstrated by this test firmware. The production SBT/SIL images and on-device allocation peaks have not been measured. Repeated browser-load, HTTP-delivery and panic acceptance remain open. No uploads, serial monitoring, device controls or consumer edits were performed.

### Delivery State

Checkpoint `dea1bf931d7dcacb4d774127a612574599033b1f` is pushed on `feature/jbd-bms-transport`. Serena cleanup `cf68f0477d90882c555c26d39ae62b3fbca51a1d` is pushed on `feature/hybrid-json-output`; `.serena/project.yml` remains present locally and ignored, while shared Serena memories remain tracked. Hybrid implementation and its records remain uncommitted for owner review, as requested by the no-automatic-final-commit rule.


### Subsequent owner-authorized app-name recovery

After device panic reports, the owner authorized the WebUI recovery repair documented in `docs/todo_http_delivery_panic.md`. New helper `webui/src/appInfoClient.mjs` and App.vue integration preserve identity, support legacy fallback and bounded retries/cancellation. Node tests: 24/24 pass. `webui/test/appInfo.browser.cjs` verifies the actual embedded gzip page with mocked transport, using PLAYWRIGHT_MODULE and PLAYWRIGHT_BROWSER_PATH to select existing runtimes. The WebUI build, generated `src/html_content.h`, and compile-only ESP32 test build pass. This follow-up shares unreleased patch 4.6.1. Earlier resource-size measurements above refer to the hybrid-only checkpoint; they do not include this newly rebuilt UI. The runtime heap panic is not resolved by this UI change.


### App-info allocation peak and request-time diagnostics

The later 62d613765e85ce17 panic prompted targeted app-info document removal and verbose-only heap/integrity probes, recorded in `docs/todo_http_delivery_panic.md`. This is a bounded extension of the authorized crash investigation, not a transport redesign. `run-app-info-json-host-tests.ps1`, `run-http-heap-host-tests.ps1` (verbose 0 and 1), `run-bounded-json-host-tests.ps1` and `run-static-gzip-host-tests.ps1` pass. CM compile-only and actual SBT USB builds pass. Hardware acceptance remains pending; no upload or final commit was performed. The candidate remains unreleased patch 4.6.1.


### Default-capability health and CSS response follow-up

New owner logs from ELF160fa396 show successful app-info output with only about 5-6 KB DEFAULT-capable memory, followed by request-header or theme-response-header allocation failure. The earlier Health totals used INTERNAL and must not be compared directly with DEFAULT.

CM 4.6.1 now avoids the default 1,460-byte stream buffer for built-in CSS and preserves custom CSS through an exact owned snapshot. Focused CSS, bounded-response and HTTP diagnostic tests pass; the verbose-disabled CM compile-only build passes. SBT's owning app adds the first-16 allocator failure recorder and aligns Health fields with DEFAULT, retaining the broader INTERNAL value separately. Its host guard/callback tests pass for verbose 0 and 1. SBT patch version is 1.0.1 per its governance; the existing feature/move-sil-planning branch and prior main.cpp edits are preserved. No dependency bindings, settings or controls are changed.

The first SBT compile failed because `.pio/ble-smoke/usb/src` disappeared during compilation (`opening dependency file .pio/ble-smoke/usb/src/main.cpp.d: No such file or directory`). No product-code compilation error was reported. An isolated project-local `.pio/http-diagnostics-check` build is used for final validation without touching the owner's regular build directory. Runtime panic acceptance remains pending; no upload, monitor, commit or push is performed.


Final isolated SBT validation: `PLATFORMIO_BUILD_DIR=.pio/http-diagnostics-check pio run -e usb` passes (exit 0); the ELF contains both diagnostic markers. This is compile/link evidence only. The regular build path was not replaced. All focused tests and both affected firmware builds are now passing; the heap-exhaustion cause still requires the next owner-captured allocation-failure event.


### Serialized startup and late failure visibility

The subsequent owner logs established real DEFAULT-heap shortage and fragmentation; documentation details are in `docs/todo_http_delivery_panic.md`. The candidate staggers initial body reads/schema/status/socket work, corrects page-refresh request suspension, preserves [] safety recovery after repeated schema failure, and allows late small allocation failures through the SBT sampling filter. This shares the existing unreleased CM4.6.1/SBT1.0.1 patch batch.

31 WebUI unit tests, the mounted order/retry/OTA regression, the actual embedded-page recovery browser test, and both affected firmware compiles pass. Embedded gzip content was verified inside the final isolated SBT ELF. Hardware heap/panic acceptance remains pending; no deployment or Git publication occurred. The separately requested heap inventory and redesign estimates are documentation only.

Requested inventory: [Heap memory inventory and refactoring options](../../heap-memory-inventory.md). It distinguishes retained storage, transient peaks, effective feature switches and conditional redesign estimates; no redesign is authorized or implemented by that documentation request.

### Owner-authorized MQTT System-Info lifetime follow-up

On 2026-10-18 the owner requested a local CM intermediate commit followed by
the MQTT lifetime refactor. Checkpoint `e578c9184b9d7146df005a5e953e63b550262bd1`
preserves the reviewed hybrid/output/startup candidate on
`feature/hybrid-json-output`; no push was requested for this checkpoint.

The bounded follow-up creates and publishes the ESP and WiFi System-Info
payloads sequentially, releasing each ArduinoJson document before network
publication where practical. It preserves JSON schemas/order/formatting,
topics, optional fields, retained cleanup, QoS, both publication attempts,
combined success and existing grow-only MQTT buffer behavior. It adds no
feature switch, direct-writer redesign or buffer-size reduction. The metadata
storage redesign remains explanatory only. Compatible unpublished patch 4.6.1
is retained.

Validation: a focused regression must exercise production serialization and
lifetime/allocation behavior with real ArduinoJson, including output parity
and failed-first-publication behavior, followed by a relevant ESP32 build.
Independent review checks the narrow delta against this contract. Host savings
must be labeled separately from device measurements. No final commit, push,
upload, monitor or device acceptance is authorized by this follow-up.

MQTT follow-up result: implementation and independent scoped review pass.
The focused host runner passes five fixtures; baseline extraction failed 20
lifetime checks before the scoped fix. Exact output and continuation after the
first publication fails are covered. ArduinoJson-only host64 allocation peaks
fall from 8401 to 4227 bytes (long-text fixture 8994 to 4820), with allocation
count unchanged at 10. Both publish callbacks see zero live document allocation
bytes. These host results exclude payload storage and TCP and are not target
heap measurements.

The actual SBT consumer build passes (exit 0, 31.77 seconds) with the real
Arduino String instantiation: static RAM 85,248 bytes, flash 1,386,369 bytes.
No full matrix or unchanged UI test suites were rerun. The metadata compile-only
probe confirms String=16 bytes, RuntimeFieldMeta=244, style-vector handle=12;
the ESP32 default ArduinoJson variant pool is 1,024 bytes rather than the host64
4,096. The heap inventory records this correction and keeps metadata redesign
as a proposal. Hardware heap/panic acceptance and broker delivery are pending.
Final SBT ELF SHA256: `D62E6CF34ADA803C1CE10710C163C0EB15E8B29989F8B99E7311C1356799E0CF`. No final commit or push performed.
