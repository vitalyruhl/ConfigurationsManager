# TODO / Roadmap / Refactor Notes (Planning)

## Terminology

- Top buttons: Live / Settings / Flash / Theme (always visible)
- Settings:
  - SettingsPage = tab in Settings (e.g. "MQTT-Topics")
  - SettingsCard = big card container with title one page can have multiple cards
  - SettingsGroup = group box inside a card (contains fields; shown stacked vertically)
- Live/Runtime:
  - LivePage = top-level page selector under "Live" (similar to Settings tabs)
  - LiveCard = card container inside a live page
  - LiveGroup = if we want a second nesting level like Settings

## High Priority (Prio 1) - Proposed API vNext (Draft)

- Review-driven hardening execution list (source: `review/review.md`)
- Scope note: SSL/TLS transport warnings are intentionally excluded for now (core limitation accepted).

1. [DONE] Add strict HTTP body size limits + early `413` reject on JSON/body endpoints
   - Files: `src/web/WebServer.cpp` (`/config_raw` and all handlers using `request->_tempObject`)
   - Goal: prevent heap exhaustion/fragmentation from unbounded `String.reserve(total)` + `concat(...)`
   - Baseline measurement (`curl`, SolarInverterLimiter): `GET /config.json` currently ~`14.2 KB` response size on target device
   - Decision: start with `8 KB` request-body hard limit for JSON/body POST endpoints
   - Acceptance:
     - hard max size check before allocation
     - clear `413` response for oversized payloads
     - no behavior regressions for normal payload sizes

2. [DONE] Remove extra memory peak in `config.json` chunked response path
   - Files: `src/web/WebServer.cpp` (`beginChunkedResponse` currently captures `String json` by value)
   - Goal: avoid duplicate large `String` copy in lambda capture
   - Acceptance:
     - chunked response uses shared/move-backed state without unnecessary extra copy
     - large config payload remains stable under low-heap conditions

3. [DONE] Replace manual request temp object lifecycle with safer centralized cleanup
   - Files: `src/web/WebServer.cpp` (all `new String()` / `delete body` request body handlers)
   - Goal: avoid leaks on abort/disconnect/error paths
   - Acceptance:
     - one consistent allocation/cleanup pattern
     - no dangling `_tempObject` on early exit paths

4. [DONE] Check and act on Preferences write return values
   - Files: `src/ConfigManager.h` (`prefs.putString/putBool/putInt/putFloat` in load/save paths)
   - Goal: avoid silent persistence failure (NVS full/corrupt)
   - Acceptance:
     - return values checked
     - clear short error logs on write failure
     - behavior defined when write fails (retry/skip/fallback)

5. [DONE] Define runtime threading contract explicitly
   - Files: `src/runtime/RuntimeManager.cpp`, `src/runtime/RuntimeManager.h`
   - Goal: eliminate ambiguity around mutation (`addRuntimeProvider/addRuntimeMeta`) vs serialization (`runtimeValuesToJSON`)
   - Decision: use `runtime-safe` registration with lightweight locking
   - Acceptance:
     - chosen model documented in headers
     - implementation aligned and race-prone usage removed

6. [DONE] MQTT callback API safety cleanup
   - Files: `src/mqtt/MQTTManager.h`
   - Goal:
     - remove ineffective weak-hook checks (`if (&onXYZ != nullptr)`)
     - reduce misuse risk of non-null-terminated payload passed as `const char*`
   - Decision: breaking API change is allowed (library not published yet)
   - Acceptance:
     - weak-hook dispatch simplified and explicit
     - payload callback moved to safe byte-view style contract (no implicit C-string expectation)

7. [DONE] Reduce blocking `delay(...)` in WiFi roaming/reset flows
   - Files: `src/wifi/WiFiManager.cpp` (`roaming`, `performStackReset`)
   - Goal: reduce main-loop stalls/WDT risk
   - Acceptance:
     - convert delay-heavy flow to non-blocking state/timestamp steps where feasible
     - reconnect/reset behavior remains stable in practice

8. [IN PROGRESS] Performance and consistency follow-up from review
   - Files:
     - `src/logging/LoggingManager.cpp` (queue front-erase strategy)
     - `src/web/WebServer.cpp` (debug-heavy request path logging)
     - runtime/web/wifi JSON+String hot paths
   - Goal: reduce CPU jitter, heap churn, and log footprint
   - Status:
     - [DONE] `LoggingManager` queue front-erase strategy optimized (`std::deque` + `pop_front`)
     - [NEXT] trim debug-heavy request-path logs in `WebServer`
     - [NEXT] follow-up on runtime/web/wifi JSON+String hot paths

Open questions (implementation detail follow-up):
- Should the `8 KB` body limit be endpoint-specific (e.g. `config_raw` lower, larger JSON endpoints higher), or globally uniform for all POST body handlers?

## Medium Priority (Prio 5)

- verify/implement compile-time warnings for invalid IO pin bindings (e.g., hold button test pin)
- add addCSSClass helper for all Live controls (buttons, sliders, inputs) so user CSS selectors can override styles (Live only, not Settings)



## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)



## Done / Resolved

- Prio 1: documented `ConfigManager.performStackReset()` in `docs/WIFI.md`
- Prio 1: added minimal diagnostic `performStackReset()` snippet in `examples/Full-Logging-Demo`
- Prio 1: converted `docs/MQTT.md` Method overview to required table format
- Prio 1: replaced generic Method overview placeholders with topic-specific entries (or `_none_` in non-API docs)
- Prio 1: aligned severity-tag guidance to short tags (`[I]`, `[W]`, `[E]`, `[D]`, `[T]`) in contributor guidance

## Status Notes

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| _none_ | - | Planning document; no callable API methods in this document. | Track tasks and status only. |

