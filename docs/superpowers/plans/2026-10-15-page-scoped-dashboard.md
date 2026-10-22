# Page-scoped Dashboard Loading Plan

**Authorization:** User explicitly delegated implementation of page-scoped backend/Vue loading. Parent owns SBT page bindings, removal of builtin System runtime, firmware builds, USB uploads and hardware/browser acceptance. Preserve the frozen fragment baseline until parent releases its build freeze. No unrelated refactor.

## Current behavior

Runtime HTTP and WebSocket selections already choose sourceGroups before provider/control callbacks. Runtime metadata and live layout remain global. App.checkLiveContent loads full metadata and App mounts a configuration request while RuntimeDashboard also loads metadata/layout. Browser successful caches deduplicate identical URLs but do not reduce the first payload.

## Additive backend contract

- Add `/live_pages.json`: `{pageScoped,hasLiveContent,pages:[{key,name,title,order}],defaultPage}`. Build only the lightweight navigation descriptors; inspect registered metadata without invoking runtime getters or serializing full metadata. Sort consistently with the existing layout. Keep bounded JSON transport and CORS/cache policy.
- Add optional `page=<key>` to `/live_layout.json` and `/runtime_meta.json`. Existing no-query schemas and full semantics stay unchanged. A supplied key must match the index; reject empty or over-256-byte decoded selectors with 400, unknown keys with 404. Never convert malformed/unknown selection into full payload.
- Selected layout retains `{pages,placements,defaultPage}` but only includes the selected page and its placements. Selected metadata retains the existing array entries and sourceGroup values. Apply membership before size measurement and serialization; do not construct full JSON and filter it afterward.
- Resolve membership using actual registered `livePlacements` and normalized `livePages` keys, including existing first-key/last-placement semantics. Metadata without a matching rendered placement uses the existing `__unassigned__`/Other bucket; do not silently discard it. Preserve controls-only entries. With no explicit live layout, report `pageScoped:false` and use the existing legacy UI path.
- Keep unfiltered weak shared metadata caching and response active counters intact. Selected responses own only the selected result; introduce no persistent per-page firmware cache. Keep runtime, GUI, log and OTA event streams unchanged.

## Implementation entry points

- `ConfigManagerLayout.cpp` / `ConfigManager.h`: page index generation, page-key validation/membership and optional layout selection, preserving fragment assembly.
- `runtime/RuntimeManager.cpp` / `.h`: an internal metadata predicate applied before both size and serialization loops, plus a read-only metadata-presence query under the existing mutex. Keep public unfiltered behavior.
- `web/WebServer.cpp`: additive page index route and optional page selection; use existing bounded response ownership and error paths.
- `webui/src/runtimeDataClient.mjs`: shared index request and per-page successful cache with one in-flight promise per page. Sequential layout then metadata; rejected/aborted results are evicted. Cache only in the browser session; refresh/navigation reload resets it.
- `App.vue`: discover Live availability through the index instead of full metadata; defer config/settings loading until Settings is selected, retaining authentication and independent GUI/OTA event handling.
- `RuntimeDashboard.vue`: navigation tabs from the index, active-page layout/metadata request, page-key generation guard against stale results; runtimeReady becomes true only after both selected responses validate. Retain global legacy fallback only when index is unsupported (404) or explicitly pageScoped:false. An empty scoped page never triggers full runtime discovery.

## Loading lifecycle

Index is shared between App and Dashboard. Initial Live page loads layout then metadata. A page click clears active runtime demand immediately, cancels obsolete work, fetches or reuses that page, then subscribes to its sourceGroup union and requests a fresh selected runtime snapshot. Settings/hidden views suspend page work and selected runtime; GUI/log/OTA safety events remain active. On return, reuse successful page cache and fetch fresh runtime. Errors remain retryable and are not cached; aborts/stale responses cannot overwrite the active page. Preserve settings auth and OTA controls.

## Validation

Focused backend membership/selector tests cover exact known keys, malformed/unknown keys, unassigned metadata, duplicate placement semantics, empty pages and legacy unfiltered output. Actual serialization helper tests verify selected payloads never need a full document and retain failure checks. Frontend tests cover sequential requests, per-page cache/dedup, transient failure retry, stale/abort races, Settings/visibility pause, page tabs and active source subscriptions. Run existing focused runtime/JSON/gzip regressions and a mounted Vue harness with simulated endpoints. Parent alone regenerates embedded UI assets and performs the production build. Parent performs final clean USB build and hardware acceptance, including response sizes and no global metadata/layout request in scoped mode. No fabricated CPU/heap improvements.

## Status

Implemented after parent/reviewer approval and baseline build release. Automatic
placement identity now includes sourceGroup; explicit source-less overrides keep
legacy precedence. OTA safety serialization is independent of the optional
System telemetry provider.

Validation: 11 Node unit tests passed. The actual Vue application mounted against
mocked endpoints passed exact-key navigation, sequential scoped loading, cache
reuse, BMS/Victron duplicate field identity, empty-page tabs, stale cancellation,
Settings/visibility suspension, OTA safety and active body-timeout retry. The
reviewable harness is `webui/test/pageDashboard.browser.cjs`; run it against a
local Vite server, optionally setting `PLAYWRIGHT_MODULE`,
`PLAYWRIGHT_BROWSER_PATH` and `CM_WEBUI_TEST_URL`. Backend helper tests cover
selector bounds, exact source identity and explicit override precedence. The parent completed final clean firmware compilation and real four-page
HTTP/browser acceptance; measured evidence follows. Host tests alone do not claim
full target orchestration coverage.

Independent review found and verified the active-timeout retry correction. No
confirmed P1/P2 findings remain in the reviewed page-loading scope. Same-page
source collisions still require unique field IDs under the existing wire schema.


## Hardware acceptance evidence (2026-10-15)

Parent-reported results for the final clean USB build and uploaded artifact:

- ELF SHA-256: `05fa12f05a726d2cbf02413d2f4c209d6b5f49363ddc44434a25398bf2737692`.
- Clean USB build: 87.07 seconds; hash-verified upload: 26.13 seconds.
- Flash: 1,337,989 bytes, +5,104 bytes versus the bounded-response baseline
  (1,332,885 bytes). Static RAM: 78,296 bytes, unchanged.
- Embedded WebUI gzip: 60,407 bytes (+854 bytes). Device UI SHA-256:
  `cdb36b8c7788c1991fd216f7aeb01708558fa2dec4ddf930683d1e712d61db39`.
- Lightweight page index: 318 bytes.

| Page | Selected layout bytes | Selected metadata bytes |
|---|---:|---:|
| Bluetooth | 1,345 | 1,790 |
| MQTT | 838 | 1,247 |
| BMS | 1,685 | 2,701 |
| Victron | 1,408 | 2,127 |

All checked JSON responses were valid. OTA safety fields remained present without
System telemetry. Invalid page selection returned 400; unknown selection returned
404. The browser displayed all four tabs and Settings MQTT enable/password/base
Topic controls. BMS showed `adapter_missing`; Victron showed
`waiting_for_advertisement`. Rapid page changes produced no browser errors, and
MQTT remained online. The final serial capture lasted 300 seconds and contained
60 health samples, spanning device uptime 45,042 to 340,107 ms. No errors were
reported. Minimum reported ever-free heap was 27,784 bytes; the minimum sampled
free heap was 42,176 bytes and minimum sampled largest block was 14,324 bytes.
After closing the browser, final free heap was 49,268 bytes with a largest block
of 28,660 bytes. These are device observations, not general guarantees.

Parent retained the detailed evidence in the SBT working directory:
`.local/page-scoped-acceptance.json` and
`.local/serial-page-scoped-acceptance-hil-summary.json`. COM11 was closed after
observation.

An initial weak-AP/startup HTTP refusal episode required a USB reset before
functional acceptance. Similar startup behavior existed in the baseline. This
change does not establish a general Wi-Fi/startup fix, identify the episode's
root cause, or prove improved provider CPU time. Absolute AP priority and a
possible listener-start failure remain unconfirmed hypotheses.
