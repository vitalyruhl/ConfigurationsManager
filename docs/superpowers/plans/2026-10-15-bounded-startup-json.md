# Bounded Startup JSON Responses Follow-up Plan

> **For agentic workers:** Use superpowers:executing-plans. Preserve all existing changes; no commits, uploads or local firmware builds in this subtask.

**Goal:** Extend the verified accepted-versus-flushed response handling to the four startup JSON endpoints without copying whole payloads again.

**Architecture:** Extract the existing sender into `BoundedResponse.h` with borrowed static, moved String and shared immutable String ownership. Keep the gzip adapter and use an actual shared metadata specialization retaining its active-request counter until response destruction. The existing 512-byte acceptance/ACK budget and final-ACK close lifecycle remain unchanged.

**Spec:** Parent/user scope 2026-10-15: root HTML now arrives, but metadata/settings time out during severe heap pressure and subsequent network/BLE allocation crashes. Change only `/config.json`, `/runtime.json`, `/runtime_meta.json` and `/live_layout.json` transport; preserve schemas, authentication, cache/CORS and metadata sharing/counters. No WebUI, dependency, feature-flag, OTA or other-repository edits.

**Tech stack:** Existing C++17/ESP32 Arduino, pinned ESPAsyncWebServer and AsyncTCP, existing host platform shell.

## Steps

- [x] Add focused tests compiling the actual generalized sender and metadata specialization: moved payload outlives source scope, underlying long-buffer pointer is reused, shared payload remains alive after cache owner releases it, early disconnect releases it and decrements once, final ACK/disconnect destroys once, zero/short add and failed flush preserve exact long JSON bytes without gzip headers.
- [x] Extract the proven sender, keep `StaticGzipResponse` as the gzip-only adapter and replace the four successful JSON response paths. Remove the old unbounded metadata writer and startup JSON Basic/chunked paths. Preserve existing error/status behavior and CORS/cache/auth policy; add allocation checks where missing.
- [x] Run the new focused JSON tests plus the existing full embedded-gzip scenarios and `git diff --check`. Document the compatible correction within unpublished 4.6.0.
- [ ] Parent performs review and one clean SBT USB build; device recovery, transfer timing and BLE/MQTT coexistence remain hardware acceptance work.

## Provider generation pressure

This transport change does not bound temporary provider/serialization allocations. No arbitrary free-heap threshold or silent schema degradation is proposed. A separate possible gate is one active generated-JSON response, acquired before invoking a provider and released with response destruction; busy requests would receive 503/Retry-After. That behavior requires explicit parent approval plus frontend retry compatibility and is not included by default.

## Review status

Parent approved the plan before implementation. Independent read-only review completed with no confirmed P1/P2, including pinned Arduino String move semantics and metadata counter release paths. Source frozen for the parent-owned clean USB build. No provider-generation gate was introduced.

## Focused verification

- `tools/test/run-bounded-json-host-tests.ps1`: PASS, six actual-sender scenarios; 24,029 synthetic JSON bytes reassembled exactly, 512-byte admission/outstanding bound, owned/shared lifetime, null/empty payload behavior, early disconnect and final-ACK counter release, failed flush without duplicate admission.
- `tools/test/run-static-gzip-host-tests.ps1`: PASS, existing four scenarios with the entire 59,553-byte embedded gzip asset.
- `git diff --check`: PASS. These tests use an explicit mocked AsyncTCP/server shell and `std::string` as String; they do not reproduce the old library implementation or prove target heap recovery.
- Installed ESP32 Arduino `WString.cpp` move constructor calls `move`; non-SSO buffers transfer their pointer into a freshly initialized destination. The sender obtains `c_str()` only after moving, so both inline and heap-backed storage remain response-owned.
- The old metadata writer's one-time TCP stall log was removed with that implementation; allocation/unavailable diagnostics and the active-request counter remain. JSON schemas, provider callbacks, authentication, CORS and runtime cache headers are unchanged. Large configuration responses now use fixed Content-Length instead of chunked framing; config/layout connections close after the final ACK.
- Provider/serialization allocation peaks, radio coexistence and transfer timing remain target acceptance work. The 512-byte window can trade throughput for bounded TCP admission under delayed ACK.
