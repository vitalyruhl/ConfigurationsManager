# Code Review Report
## Summary
- The repository has a solid modular structure (WiFi/Web/OTA/Runtime/MQTT), but several request/transport paths still trust unbounded input sizes and plaintext channels.
- Most critical risks are around memory pressure from HTTP body handling and unsecured credential transport for settings/OTA.
- Runtime and MQTT subsystems are feature-rich, but callback/container usage relies on assumptions about single-threaded access that are not enforced.
- JSON and String-heavy code paths are functional, yet likely to fragment heap under sustained web/MQTT load.
- Logging and WebSocket queues are bounded, but queue operations are algorithmically expensive and can become a latency hotspot.

**Findings count:**
- **PRIO 1:** 3
- **PRIO 2:** 6
- **PRIO 3:** 5

## PRIO 1 – Dangerous (must fix)

### 1) Unbounded HTTP body buffering can trigger heap exhaustion/reset
- **Location:** `src/web/WebServer.cpp:246-255` (`/config_raw` body handler)
- **Risk:** Large `total` body sizes are reserved and concatenated directly into heap `String`, enabling memory exhaustion/fragmentation and watchdog resets.
- **Evidence:** `request->_tempObject = new String();` then `reserve(total);` then repeated `concat(...)` with no upper limit or early rejection.
- **Impact:** Device instability under malformed/oversized requests; intermittent crashes in production due to low-heap fragmentation.
- **Recommendation:** Enforce strict maximum body size before allocation, abort early (413), and parse incrementally/streamed where possible.
- **Confidence:** High.

### 2) Sensitive settings and OTA credentials are processed over plaintext HTTP headers/body
- **Location:** `src/web/WebServer.cpp:303`, `src/ota/OTAManager.cpp:228-240`
- **Risk:** Passwords/tokens can be intercepted on LAN/WLAN (MITM/sniffing), enabling unauthorized config updates and firmware upload.
- **Evidence:** Explicit comment `Passwords are transmitted in plaintext over HTTP.` and OTA password check via `X-OTA-PASSWORD` header without transport protection.
- **Impact:** Security compromise of device behavior and firmware integrity.
- **Recommendation:** Require TLS-terminating reverse proxy or signed challenge/response; at minimum disable these endpoints unless explicitly opted-in for trusted networks.
- **Confidence:** High.

### 3) Potential concurrent access race on runtime provider/meta containers (Needs confirmation)
- **Location:** `src/runtime/RuntimeManager.cpp` (`addRuntimeProvider`, `addRuntimeMeta`, `runtimeValuesToJSON`)
- **Risk:** If callbacks/tasks mutate runtime vectors while JSON serialization iterates, iterator/pointer invalidation can cause undefined behavior.
- **Evidence:** `push_back` in add-functions and pointer snapshot/iteration in `runtimeValuesToJSON` without synchronization.
- **Impact:** Rare hard-to-reproduce crashes/data corruption under concurrent registration and web/ws reads.
- **Recommendation:** Guard mutations/reads with a lightweight mutex or freeze registrations after init and document that contract explicitly.
- **Confidence:** Medium (depends on actual call threading model).

## PRIO 2 – Should be reworked

### 1) Large `config.json` path duplicates memory via lambda capture
- **Location:** `src/web/WebServer.cpp:326-349`
- **Risk:** Capturing `String json` by value in chunked response can keep additional large heap allocations alive.
- **Evidence:** `String json = configJsonProvider();` then `beginChunkedResponse(... [json](...) { ... })`.
- **Impact:** Elevated peak RAM usage and fragmentation during large config responses.
- **Recommendation:** Use response stream/chunk generator backed by shared buffer or move-only/state object without extra copy.
- **Confidence:** High.

### 2) Manual `new`/`delete` request temp objects rely on happy-path completion
- **Location:** `src/web/WebServer.cpp` multiple handlers (`request->_tempObject = new String(); ... delete body;`)
- **Risk:** Abrupt disconnect/error paths can leak heap objects depending on framework cleanup behavior.
- **Evidence:** Allocation and manual delete in many lambdas; cleanup tied to `index + len == total` branch.
- **Impact:** Slow memory loss under unstable networks/high request churn.
- **Recommendation:** Centralize cleanup on disconnect/failure hooks or use RAII wrapper attached to request lifecycle.
- **Confidence:** Medium.

### 3) Weak hook null checks are ineffective and may hide design intent
- **Location:** `src/mqtt/MQTTManager.h:36-61`
- **Risk:** `if (&onXYZ != nullptr)` is always true for functions; gives false safety impression.
- **Evidence:** Checks before calling weak hooks are function-address comparisons against `nullptr`.
- **Impact:** Maintainability issue and potential misuse expectations by consumers.
- **Recommendation:** Remove redundant checks or replace with explicit compile-time/default-hook strategy only.
- **Confidence:** High.

### 4) MQTT message callback exposes non-null-terminated payload pointer as `const char*`
- **Location:** `src/mqtt/MQTTManager.h:1985` + hook signature `:27`
- **Risk:** Consumers may treat payload as C-string and read past bounds (length is provided but easy to ignore).
- **Evidence:** `reinterpret_cast<const char*>(payload)` passed to hook API typed as `const char*`.
- **Impact:** Downstream UB/crashes in user code, difficult to diagnose.
- **Recommendation:** Prefer `span`-like byte view API or enforce null-terminated temporary buffer before string callback.
- **Confidence:** High.

### 5) Repeated blocking delays in WiFi reset/roaming paths
- **Location:** `src/wifi/WiFiManager.cpp:690, 893-907`
- **Risk:** Multi-hundred-ms delays can stall the main loop and increase watchdog risk when invoked in sensitive timing windows.
- **Evidence:** Multiple `delay(500/200/100)` in reset/reconnect procedures.
- **Impact:** Responsiveness degradation, increased chance of WDT under load.
- **Recommendation:** Convert to non-blocking state machine with timestamp checks.
- **Confidence:** Medium.

### 6) Error handling for Preferences writes ignores return values
- **Location:** `src/ConfigManager.h` (`prefs.putString/putBool/putInt/putFloat` in `load/save`)
- **Risk:** Silent persistence failure (NVS full/corrupt) while app assumes success.
- **Evidence:** Write APIs are called without checking result bytes/status.
- **Impact:** Lost configuration updates, inconsistent behavior after reboot.
- **Recommendation:** Validate return values and emit actionable error logs/fallback behavior.
- **Confidence:** High.

## PRIO 3 – Worth considering

### 1) O(n) vector front erases in log queues
- **Location:** `src/logging/LoggingManager.cpp:133-135, 145-158`
- **Risk:** CPU overhead grows with queue size.
- **Evidence:** `pending_.erase(pending_.begin())` and similar buffer eviction strategy.
- **Impact:** Minor loop jitter under high log throughput.
- **Recommendation:** Use ring buffer/deque indices.
- **Confidence:** High.

### 2) Verbose debug logging on request paths can inflate flash and runtime overhead
- **Location:** `src/web/WebServer.cpp:258-301`
- **Risk:** Extra string literals and formatting in hot API path.
- **Evidence:** Parameter/body dump logs for each `/config_raw` request.
- **Impact:** Larger firmware and avoidable CPU cycles.
- **Recommendation:** Gate behind dedicated debug macro and shorten messages.
- **Confidence:** High.

### 3) Broad use of dynamic `String` in hot loops
- **Location:** `src/wifi/WiFiManager.cpp`, `src/runtime/RuntimeManager.cpp`, `src/web/WebServer.cpp`
- **Risk:** Fragmentation over long uptime.
- **Evidence:** Frequent concatenation and transient `String` creation.
- **Impact:** Gradual heap quality degradation.
- **Recommendation:** Reuse buffers and reserve capacities where practical.
- **Confidence:** Medium.

### 4) JSON document capacities are static guesses
- **Location:** `src/runtime/RuntimeManager.cpp:112`, `src/ConfigManager.cpp:651`, `src/mqtt/MQTTManager.h:2195`
- **Risk:** Oversizing wastes RAM; undersizing leads to truncation/failures.
- **Evidence:** Fixed capacities `4096`, `16384`, `2048` without adaptive handling.
- **Impact:** Avoidable RAM pressure.
- **Recommendation:** Profile typical payload sizes and tune/reuse docs.
- **Confidence:** Medium.

### 5) Logging severity tag consistency differs from stated policy
- **Location:** Multiple modules use `[WARNING]` etc.
- **Risk:** Inconsistent operational diagnostics and larger string footprint.
- **Evidence:** e.g., `CM_CORE_LOG("[WARNING] ...")`, `WIFI_LOG("[WARNING] ...")`.
- **Impact:** Minor but widespread consistency debt.
- **Recommendation:** Normalize to compact severity tokens.
- **Confidence:** High.

## Flash/RAM Savings Opportunities (no compiler flag changes)

### High gain
- **Bound request body and avoid whole-body buffering** (`src/web/WebServer.cpp`): prevents large transient heap use and fragmentation spikes. **Behavior risk:** Low if clear 413 response is returned.
- **Avoid `String` copy capture in chunked config response** (`src/web/WebServer.cpp:342-349`): removes large duplicate allocations. **Behavior risk:** Low.
- **Reduce debug dump logging in hot endpoints** (`src/web/WebServer.cpp:258-301`): cuts format strings and runtime formatting cost. **Behavior risk:** Low.

### Medium gain
- **Replace vector front-erases with ring buffer/deque** (`src/logging/LoggingManager.cpp`): lowers CPU churn and temporary allocations. **Behavior risk:** Low.
- **Pre-reserve/reuse JSON documents for periodic runtime payloads** (`src/runtime/RuntimeManager.cpp`): less heap churn in WS/HTTP loops. **Behavior risk:** Low-Medium.
- **Minimize transient String concatenations in WiFi scan logs** (`src/wifi/WiFiManager.cpp`): lowers fragmentation over long runs. **Behavior risk:** Low.

### Low gain
- **Compact repeated literal messages/severity tags** (cross-module): slight flash savings from string dedup opportunities. **Behavior risk:** None.
- **Prefer short constant labels in runtime/meta JSON keys where possible** (cross-module): small payload+RAM reductions. **Behavior risk:** Medium (API/UI contract impact).

## Consolidation / Consistency Opportunities
- Unify repeated HTTP body parsing patterns (`/config_raw` and runtime control endpoints) into one helper with centralized size limits, parse error handling, and cleanup.
- Consolidate auth handling logic (settings token + OTA password checks) into a shared security utility to avoid divergent policy.
- Introduce a common request lifecycle guard for `request->_tempObject` allocation/cleanup to remove repeated manual patterns.
- Standardize severity tags and module log style globally to simplify filtering and reduce string footprint.
- Define explicit threading contract for Runtime/MQTT registration APIs (init-only vs runtime-safe) in headers.

## Quick Checklist (for follow-up execution)
1. Add hard request size limits and early rejection (413) for all JSON/body endpoints.
2. Secure settings/OTA channels (TLS path or challenge-based auth), and document threat model.
3. Decide and enforce runtime container threading model (lock or init-only).
4. Refactor large config response generation to avoid duplicate `String` copies.
5. Add robust cleanup strategy for request temp allocations on error/disconnect.
6. Check and act on Preferences write return values.
7. Reduce blocking delays in WiFi reset/reconnect to non-blocking state transitions.
8. Optimize logging queue structure and trim debug-heavy endpoint logs.
