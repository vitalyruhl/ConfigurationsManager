# Demand-driven Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking. No automatic commits are authorized.

**Goal:** Evaluate and transmit only runtime source groups needed by the active dashboard, while preserving legacy clients, GUI/log events and OTA safety.

**Architecture:** Add a bounded group selection shared by HTTP and WebSocket. Snapshot selected providers/controls before invoking callbacks; build one union snapshot per WebSocket tick and filter its already-evaluated JSON for each client. Browser metadata/layout are loaded once and reused; active-page visibility determines one subscription, and hidden views retain only a small safety envelope.

**Tech Stack:** C++17, ESP32 Arduino, ArduinoJson 7, ESPAsyncWebServer, Vue 3, Node test runner, PlatformIO.

**Spec:** The approved user scope captured below is the controlling specification. Parent-agent review is required before product edits; no further user approval is required for this already-authorized implementation.

## Global Constraints

- Worktree: `C:/Daten/_Codding/ESP32/ConfigurationsManager-demand-runtime`; branch: `feature/demand-driven-runtime`; base: `65762bd22edddd0307427d01f056fbbe0ddc0b51` from `feature/bluetooth-module`, preserving its full history.
- Original checkout and its pre-existing `.serena/project.yml` remain untouched. The same Serena diff is copied into this worktree as an explicitly excluded baseline. Never stage it.
- No commits, pushes, merges, branch deletion, GitHub changes, hardware access, uploads or live-device stress.
- Local context/index/build evidence stays ignored under `.local/` or `.projectatlas/`; no context databases enter product changes.
- No per-card endpoints/connections, generalized query framework, persistent runtime copies or invented application groups.
- HTTP without selection and legacy WebSocket clients retain complete runtime behavior. Controls-only groups and alarms remain supported.
- GUI/log/OTA streams remain responsive even while ordinary dashboard runtime is paused. Preserve `system.otaActive` and `system.otaHasPassword` in a small safety envelope without running unrelated system telemetry.
- The observed malformed startup responses have no proven transport cause. Filtering reduces load; it does not by itself prove that defect resolved.
- Version classification: compatible public selection feature, minor `4.5.0` -> `4.6.0`. Canonical `library.json`; synchronize current mirrors and add an explicitly unpublished changelog entry. No release implied.

## Review Focus

1. Two clients requesting different pages must not multiply provider invocations; legacy consumers may intentionally force a full union.
2. GUI-only connect must never briefly trigger full provider evaluation; selection mode must be known during the handshake, before push-on-connect.
3. A view/visibility change during an HTTP body read must not apply a stale page response, leak a timer or overlap another poll.
4. Controls without a matching provider, duplicate provider names and alarms must retain one getter evaluation and expected output.
5. Metadata/layout loading failures must remain retryable without repeated successful downloads or recursive Settings loads; independent events and OTA safety stay available.

## Task 1: Bounded selection and provider evaluation

**Files:** `src/runtime/RuntimeManager.{h,cpp}`, new `src/runtime/detail/RuntimeSelection.h`, new `test/host/runtime_selection_test.cpp`, new `tools/test/run-runtime-host-tests.ps1`; focused runtime tests in existing ESP32 test infrastructure where needed.

**Interfaces:** `RuntimeSelection` distinguishes legacy all from explicit empty, validates max 32 groups, max 64 UTF-8 bytes/group, max 2048 encoded selection bytes; membership compares exact source group names. `runtimeValuesToJSON()` stays available; a selected overload and JSON-object snapshot writer accept the selection. Invalid selections fail closed, unknown valid names yield only the envelope.

- [ ] Capture baseline before product changes: dependency/runtime versions; focused existing frontend tests; identical full WebUI embedding + Full-GUI-Demo USB build with unique build/libdeps directories inside this worktree. Root `src/main.cpp` is empty, so its binary alone is not a representative size baseline.
- [ ] Write and run failing host tests for all/empty, deduplication, bounds, malformed selection, exact group names and union behavior. Add callback-count coverage using real runtime snapshot logic with a focused ESP32 test build and/or host-compatible seam; mark mocked data.
- [ ] Filter provider/control snapshots under the existing mutex before callbacks. Invoke every selected provider once, every selected control getter once (remove current duplicate getter pass), and include alarms only when requested/all. Always retain uptime and applicable OTA safety envelope.
- [ ] Run focused tests; measure representative full vs selected serialized bytes and callback counts. Record runtime CPU timing as hardware-pending.

## Task 2: HTTP selection and bounded WebSocket subscriptions

**Files:** `src/web/WebServer.{h,cpp}`, `src/ConfigManager.h`, selection host tests.

**Interfaces:** `/runtime.json?groups=<JSON-array>` means explicit bounded selection; absent `groups` retains full output; malformed/oversize => HTTP 400 without callback evaluation. Existing JSON provider API stays supported through an additive selected provider callback. WebSocket `/ws?runtime=selected` starts with explicit empty selection; `/ws?runtime=none` is events-only; absent mode stays legacy full. Selection frames: `{"type":"runtimeSubscribe","groups":[...]}`. Ignore/reject fragmented, oversized and invalid frames; at most 8 selective subscriptions. Existing event traffic stays broadcast-compatible.

- [ ] Add failing host tests for subscription bounds, invalid replacement retaining last valid selection, empty/unsubscribe, disconnect cleanup and union of overlapping selections.
- [ ] Determine initial mode from the validated connection request before any push. Protect subscription state shared by network callbacks/main loop with a narrow mutex; never hold it while executing provider/user callbacks or sending frames.
- [ ] Build one selected union per periodic or explicit runtime push. Filter the resulting JSON per selected client, retaining the envelope; full clients get full union. Preserve custom payload builder compatibility and existing GUI/log send APIs. Skip runtime generation if only events-only clients remain.
- [ ] Verify callback counts do not scale with subscribed client count, no providers for events-only connections and no amplification from unknown groups. Verify HTTP legacy/custom callback fallback explicitly.

## Task 3: Active dashboard demand and resilient shared initial loading

**Files:** `webui/src/App.vue`, `webui/src/components/RuntimeDashboard.vue`, new `webui/src/runtimeDemand.mjs`, new `webui/src/runtimeDataClient.mjs`, tests under `webui/test/`.

**Interfaces:** Pure helpers normalize the union of visible active-page `runtimeSources`; `runtimeDataClient` owns abortable full-body reads and in-flight/cached metadata/layout promises. Runtime polling uses one bounded in-flight request plus a generation guard. Parent and dashboard reuse the same metadata/layout result. GUI socket connects events-only; dashboard socket connects selected and carries one union subscription, preserving existing log routing.

- [ ] Write failing Node tests for source union, duplicate names, empty/hidden selection, overflow, failed cache retry, shared concurrent requests, delayed-body timeout and stale response suppression.
- [ ] Compute groups from the active live page/cards after metadata/layout discovery; no full runtime fetch merely to discover already-described content. Initial metadata-less legacy fallback may perform one explicitly documented full discovery request, then derive groups.
- [ ] Send one selection on connect/change; visible page gets a fresh filtered HTTP snapshot and WS subscription. Settings, log-only view and hidden document stop ordinary polling and unsubscribe selected groups while retaining safety/events. Resume gets a fresh snapshot; unmount cancels timers/requests/listeners.
- [ ] Replace non-aborting Settings timeout with full-body abortable loading, guard overlapping loads and remove try-scoped `text` reference from catch. Keep successful metadata/layout cache across component remounts; failed loads can retry. Reuse initial metadata for has-live-content detection.
- [ ] Run focused existing/new Node tests and WebUI production build. Verify generated bundle and source binding for active page selection, Settings, document visibility, reconnect, timeout and OTA event continuity; browser/device acceptance explicitly pending when unavailable.

## Task 4: Version, documentation, final evidence and review

**Files:** `library.json`, `src/ConfigManager.h`, `webui/package.json`, `webui/package-lock.json`, `README.md`, existing runtime documentation/changelog and version mirrors found by bounded scan; this plan and ignored evidence ledger.

- [ ] Scan current/target version references before bump; report any existing mismatch rather than silently changing unrelated example app versions. Synchronize actual library mirrors and describe opt-in protocol/bounds, legacy behavior, safety envelope and metadata cache lifetime.
- [ ] Re-run identical Full-GUI-Demo USB/WebUI build; report baseline/final Flash bytes, static RAM bytes, deltas and generated gzip size. Also run required focused root `pio run -e usb` if demo validation exposes a separate need; no uploads.
- [ ] Run focused affected tests and `git diff --check`; inspect generated files and all changed paths. Keep pre-existing Serena diff separately identified.
- [ ] Parent performs independent review of the complete uncommitted diff and the five Review Focus items. Fix important findings with focused regression tests. Report remaining hardware/transport uncertainty without claiming live resolution.
- [ ] Verify original checkout HEAD, branch and Serena hash match recorded baseline. Leave all implementation uncommitted on the new branch; retain evidence and worktree for user review.

## Progress / evidence

- 2026-10-15: governance read from `origin/main`; worktree created from exact requested base; only copied pre-existing Serena diff initially present.
- Atlas navigation returned `verification_incomplete / publication_incomplete`; bounded exact source reads used, without mutating the original index.
- Parent approved the plan before product edits; implementation and focused validation completed below.


## Implementation evidence (2026-10-15)

- Branch remains `feature/demand-driven-runtime`, HEAD/base `65762bd22edddd0307427d01f056fbbe0ddc0b51`. All changes are uncommitted; inherited Bluetooth history is intact. No pushes, merges, hardware connections or uploads occurred.
- Original checkout verified: `feature/bluetooth-module`, same full HEAD, only its pre-existing `.serena/project.yml` diff. Copied Serena file hashes match; that diff is excluded from this work.
- `tools/test/run-runtime-host-tests.ps1`: PASS. Real shared selection/snapshot helpers execute synthetic provider/getter callbacks. Exact group matching, all/empty, invalid replacement, limits, controls-only groups, union projection, client removal and selective-client cap are exercised. Synthetic JSON sizes: full 527 bytes, selected 111 bytes, empty safety envelope 64 bytes. Each selected callback executes once; projection does not repeat callbacks.
- `node --test webui/test/*.test.mjs`: PASS, 8 tests. Coverage includes shared successful/failed loads, stalled-body abort, stale response suppression, serialized requests, selection bounds and metadata-driven initial page structure.
- Real compiled Vue UI tested in headless Edge at `http://cm.test/` with intercepted synthetic HTTP/WebSocket data (no device traffic). Desktop 1280x900 and mobile 390x844 screenshots retained locally. Start/page change/Settings/Live/document hidden/resume produce selections `[]`, `[A]`, `[B]`, `[]`, `[B]`, `[]`, `[B]`; metadata and layout download once each, with no full-runtime discovery for metadata-backed pages and no page exceptions. Injected OTA-active safety state still disables navigation while hidden, then clears correctly.
- The browser test exposed an initial `[A,B]` request: the legacy layout filtered out groups until their runtime values existed. A failed browser assertion and new Node regression pinned this; layout now uses known metadata before evaluating providers. Both checks pass after the correction.
- Full-GUI-Demo USB builds include the identical `npm ci` / Vite / generated WebUI-header pipeline and the same PlatformIO dependency versions, with task-local build/libdeps paths. Baseline: Flash 1,175,445 / 1,310,720 bytes, static RAM 55,268 / 327,680 bytes. Final: Flash 1,190,025 (+14,580; +1.24%), static RAM 55,284 (+16) bytes. Both exit 0. Final generated embedded WebUI gzip is 59,553 bytes; Vite JavaScript gzip changed from 52.05 kB to 52.64 kB (rounded tool output). An exact separately retained baseline embedded-header gzip was not captured.
- `git diff --check`: PASS. Canonical and library version mirrors are 4.6.0; minimal example references the macro. 4.5.0 unpublished Bluetooth history remains documented separately. No release is implied.
- Parent independent review reported no confirmed P1/P2 finding. Actual runtime-manager/WS task orchestration is build-checked and code-reviewed; host execution covers its shared helpers, not ESP network concurrency. Hardware cycles, dynamic heap peaks, physical GUI/log/OTA coexistence and the originally observed malformed startup transport remain unverified.

## Recorded implementation decisions

- Only newly introduced selective subscriptions are capped at eight. Legacy/event-only connections keep their existing connection behavior; no new global client cap was retained.
- Existing heartbeat intervals/timeouts remain unchanged. Client/subscription access is synchronized, and callbacks/network sends occur after releasing the lock.
- Custom WebSocket builders retain full opaque payload semantics; standalone legacy HTTP callbacks without the additive selected callback can still generate full output. The bundled ConfigManager route uses the selected callback.
- Empty dashboard demand retains the small OTA safety envelope, including via HTTP fallback. Successful metadata/layout cache lifetime is one browser module session; reload refreshes it.
- Parent review substitutes for a second implementation subagent. Existing focused tests, host seams and representative build satisfy scope; no full repository matrix or live-device test was requested.
- Evidence stays in ignored `.local/demand-runtime/` because commits and cleanup were not authorized. Browser plugin skill was not exposed; existing bundled Playwright and installed headless Edge were used without installing dependencies.
