# Deferred HTTP delivery and panic investigation

Status: completed for the current iteration; operator-accepted on 2026-10-22.
Owner: ConfigurationsManager (CM), with separate SolarBatteryTelemetry (SBT) and
SolarInverterLimiter (SIL) consumer observations.

## Current disposition - 2026-10-22

The operator reports that normal SIL and SBT web access is good, substantially
faster than before and does not cause observed crashes. On that basis the
operator explicitly accepted the HTTP/WebSocket work and requested that this
iteration be marked done. This is operator-provided runtime evidence, not a new
agent-run load test or an assertion about every historical firmware cohort.

CM sources are consolidated on `feature/ble-compact-runtime` at
`4c2daf2238ca3fbb6ea9a5ecb8a802b09a8f1072`, version 4.8.0 Unreleased. The current
instruction is development-branch consolidation; no main integration, release or
new hardware action follows from this acceptance. The former blanket instruction
to keep the feature off main until this investigation is solved is superseded by
this disposition; ordinary integration/release governance still applies.

All entries below are dated historical evidence. Preserve their failures, source
identities, firmware hashes and limitations. Their then-current instructions and
pending gates describe those checkpoints, not the present work queue. The failed
normal/overload cohorts have not been retroactively passed, and the historical
allocation-pressure/missing-ACK causes have not been established as fully fixed.
Further overload/resource-lifetime research is optional and deferred; it does
not block this operator acceptance. Reopen only for a new reproducible problem
or an explicit request for that research. GitHub issue #71 concerns WT32-ETH01
Ethernet rapid reload and is not closed by SIL/SBT WiFi feedback.

The original evidence checkpoint is 2026-10-16, with follow-ups through
2026-10-21. This document retains that investigation history and does not claim
a proven shared root cause.

## Resume context

- CM 4.6.0, branch `feature/jbd-bms-transport`, baseline `554869da7efc98e0c1874e74bf0c78d4bcd1664a`. SBT's diagnostic report used the separate `ConfigurationsManager-demand-runtime` checkout at this same commit. Reconfirm each consumer's actual dependency binding before rebuilding.
- CM dependency declarations: `platformio/espressif32@7.0.1`, `bblanchon/ArduinoJson@~7.4.3`, `esphome/ESPAsyncWebServer-esphome@~3.4.0`, `esphome/AsyncTCP-esphome@2.1.4`. The SBT audit inspected installed ArduinoJson 7.4.3; capture resolved versions again on resumption.
- SBT production: `192.168.2.21`, app 0.3.0, diagnostic source baseline `861068793d3abf064e8d2c1df0888aa1a686d5a2` plus then-uncommitted feature changes. Local artifact SHA-256 `278d9541b769c999ab3a76986b3953b9f32a05c88d071845fbde714db64086c4`, 1,360,848 bytes; its local ELF/BIN timestamps were 2026-10-16 13:07:19/20. The audit did not read back running firmware flash. Preserve the exact matching ELF before replacing build outputs.
- SIL production `.122` was 4.3.2; the receiver/UI work ran on isolated test device `.126` (COM7), 4.4.0. Its final documented artifact is `847f207aebc011cc0797bdc2a086df1c1a218c2fbc661e03af1debab7126f10e`. Do not confuse test acceptance with production deployment.
- All versions, addresses, source states and results here are historical checkpoints. Check current state before reproducing. SBT MOS switching is unrelated to HTTP diagnosis and must not be used as a performance test.

## Confirmed failures and limits

SBT's serial GET sequence used a 6-second socket timeout. It received fast headers and then an early end of stream with fewer bytes than the advertised Content-Length:

| Request | Result | Elapsed |
| --- | --- | ---: |
| `/live_layout.json?page=bms` | `IncompleteRead(739 bytes read, 2427 more expected)`; expected 3,166 bytes | 5,656 ms |
| `/runtime_meta.json?page=bms` | `IncompleteRead(512 bytes read, 4807 more expected)`; expected 5,319 bytes | 5,500 ms |
| Repeated combined-group `/runtime.json` | `IncompleteRead(1024 bytes read, 2203 more expected)` | 5,656 ms |
| `/runtime_meta.json` | HTTP 503 | 141 ms |

A root browser navigation reported `ERR_CONTENT_LENGTH_MISMATCH`. Browser card acceptance did not succeed in that audit. Successful comparison requests included `/live_pages.json` (318 bytes), full runtime (3,228 bytes), BMS-selected runtime (1,857 bytes), Bluetooth layout/metadata (1,345/1,790 bytes), and Victron/MQTT metadata (2,127/1,094 bytes). Page keys are lowercase `bluetooth`, `mqtt`, `bms`, `victron`; the initial `page=BMS` 404 responses were harness mistakes.

The approximately 100-second combined observation included one browser root navigation, so it was not fully isolated from browser traffic. A later six-request page sequence had no additional browser navigation. Runtime uptime increased from 400,707 to 490,466 ms; fresh BMS/Victron MQTT health and sequences continued, including a Victron message during the failed HTTP transfer. No new reset was observed. A separate normal-location SBT acceptance had 17/18 successful HTTP requests over 180 seconds and one four-second timeout, with increasing uptime; the HTTP stability issue remained open.

### Earlier panic is a separate unresolved finding

`smoke.resetReason=4` maps to `ESP_RST_PANIC` in the inspected ESP-IDF. This reports a panic before the observation window. No corresponding panic backtrace, decoded frame or complete panic UART log was available. Heap exhaustion, stack overflow, watchdog, RF loss and a particular component are not established causes.

Fresh SBT System-Info samples reported free heap 33,000 and 29,640 bytes, minimum-ever free heap 17,336, largest allocatable block 16,372, RSSI -57 dBm. These two snapshots neither prove a leak nor measure the JSON-generation allocation peak. BLE worker stack high-water free bytes were 5,824 of 8,192; main-loop, AsyncTCP and NimBLE high-water marks were missing. Do not shrink stacks based on these incomplete observations.

Earlier startup recovery separately recorded WiFi receive-buffer allocation failure, AsyncTCP task-creation failure and a later WiFi-task watchdog in failed test runs. Reducing metadata registration from 65 to 64 avoided vector growth but did not alone fix startup; SBT's AP-priority initialization correction later passed bounded startup tests. Do not relabel those startup findings as the cause of the later truncated responses or unexplained panic.

### SIL comparison does not establish a general CM fix

SIL recorded intermittent HTTP gaps, including a roughly 50-second gap in a 14/19 run, later WLAN disconnects, and another timeout without UART panic/reboot. A proven SIL snapshot-lifetime defect was corrected: ArduinoJson borrowed receiver member strings beyond mutex-protected snapshot creation. Regression tests and subsequent selected/full runtime checks passed, including 19/19 HTTP over 180 seconds and 9/9 MQTT checks in each mode. The final UI artifact passed 7/7 HTTP over 60 seconds with real source sequences. These bounded passes do not explain every prior gap or clear CM.

A separate suspected CM race remains to inspect: `src/mqtt/MQTTManager.h` diagnostics read `lastTopic_`/`lastPayload_` while the main-loop callback replaces the Arduino Strings without a common lock. This is a review lead from SIL, not a proven crash or HTTP-failure cause. Isolate selected `sbt`/`system` runtime from full diagnostics during comparison.

## Investigation order

- [ ] Re-establish the baseline: exact consumer commit plus uncommitted diff, CM resolved pin, dependency versions, firmware/ELF hash, app version, boot reason, uptime, target and WiFi state. Retain the historical evidence rather than overwriting it.
- [ ] Instrument response delivery with bounded aggregate counters: generated length, accepted/written/acked bytes, add/send failures, close reason, elapsed time and active/outstanding memory. Record completion versus early close separately. Do not log credentials, full settings or full MQTT payloads.
- [ ] Inspect `src/web/BoundedResponse.h` and its `StaticGzipResponse`/`WebServer.cpp` call sites first. `maxInFlight=512` bounds unacknowledged bytes; headers/body are admitted separately and final ACK closes the connection. The audited gzip asset was 60,407 bytes (at least 118 body windows). Installed AsyncTCP used `ASYNC_MAX_ACK_TIME=5000`; its timeout callback closes the client. The approximately 5.5-second failures are consistent with that path but do not prove it. Distinguish ACK timeout, radio/scheduling delay, sender bookkeeping and other close paths using measurements.
- [ ] Only after measurement, compare the existing window with a bounded MSS-sized/two-segment candidate, one change at a time and with a global outstanding-memory budget. Increasing timeouts alone is not an acceptance result and can retain buffers longer.
- [ ] Instrument the 503 separately: `src/runtime/RuntimeManager.cpp` metadata size/emit passes, weak cached payload ownership, and `src/ConfigManagerLayout.cpp` placement serialization. Record allocation failure stage, per-request minimum heap/largest block, generation duration and lock hold time. ArduinoJson 7 `StaticJsonDocument<1536>` uses dynamic allocation, so repeated per-field construction is a heap-churn candidate, not a fixed stack pool. Preserve escaping, schema and ownership; do not replace the weak cache with a permanent large payload merely to hide regeneration.
- [ ] Review and, if reproduced, fix the CM MQTT diagnostic string synchronization independently. Preserve consumer snapshot-lifetime regression coverage and avoid assuming the already-fixed SIL defect explains remaining CM behavior.
- [ ] Capture a future panic's complete boot/backtrace log and exact ELF/build identity; decode the first failing frame. Add low-cost persistent reset/panic context only if supported and separately approved. Do not deliberately panic or power-cycle production to obtain evidence.
- [ ] Coordinate separate SBT optimizations only after establishing this transport baseline: invariant schema repeated in values responses, unnecessary snapshot/status work and oversized buffers. Keep legacy JSON contracts, availability/freshness and control deadlines. Those opportunities are not established panic fixes.

## Bounded reproduction and acceptance

Start with one client and no parallel browser polling: GET `/`, `/live_pages.json`, then each page's layout and metadata, then selected and full runtime. URL-encode the `groups` JSON array. Record status, Content-Length, actual bytes, complete JSON parse, header time, full-body time, uptime and source progress. Use a cold browser session for the real root/BMS page check after the serial baseline; CM serializes layout then metadata, uses a 5-second body deadline there, a 4-second runtime deadline and a 5-second failed-page retry. A partial layout can prevent card construction even while MQTT is healthy.

For a candidate fix, agree the hardware window before execution and cover normal, slow and cancelled clients, reconnect, repeated cold page loads, MQTT refresh/reconnect and BLE coexistence. Keep source availability changes distinguishable from HTTP delivery failure. Require complete Content-Length delivery and valid JSON for successful responses, explicit recoverable errors for rejected requests, no silent partial HTTP 200, recovery after cancellation, no new reset/panic, and no sustained heap/stack regression. Record every failure and the sample count; short successful windows cannot clear intermittent or historical faults.

Rerun affected sender/lifetime host tests (`tools/test/run-bounded-json-host-tests.ps1`, `tools/test/run-static-gzip-host-tests.ps1`) and relevant JSON/UI tests, plus the required affected firmware build. Existing mocked sender passes do not establish target TCP timing, allocation safety or radio coexistence. Preserve settings, OTA capability and existing controls; HTTP investigation requires no physical MOS command.

## Evidence to reopen

- SBT: `docs/diagnostics/2026-10-16-sbt-runtime-performance.md` is the detailed measurements/source-pointer report; `docs/acceptance/2026-10-16-required-bms-telemetry-controls.md` contains recovery and normal-location context.
- SBT private evidence: ignored `.local/performance-baseline-20261016.json`, `.local/performance-pages-20261016.json`, corresponding helpers, and `.local/production-sbt-real-source-stability.json`. Keep these local because they contain private runtime data.
- SIL: `docs/sbt-receiver-validation-2026-10-16.md` separates failed runs, the proven SIL lifetime correction and final bounded passes; `docs/cm-runtime-validation-2026-10-16.md` records the earlier production CM upgrade acceptance. Local raw logs remain ignored under `.pio/`, especially `sbt-snapshot-*`, `sil-ui-gate-*`, `sil-ui-disconnect-followup-*` and `sil-ui-encoding-final-gate-*`.
- CM: [bounded startup JSON plan](superpowers/plans/2026-10-15-bounded-startup-json.md), [page-scoped dashboard plan](superpowers/plans/2026-10-15-page-scoped-dashboard.md), [metadata lifetime/layout integrity plan](superpowers/plans/2026-10-15-metadata-lifetime-layout-integrity.md). Earlier implementation/test passes are historical context, not proof that this follow-up is solved.

The original 2026-10-16 handoff changed documentation only. The separately approved 2026-10-18 implementation checkpoint is recorded below. Completion requires evidence for the actual fixes and owner confirmation; the unexplained historical panic must remain explicitly open if no causal evidence is recovered.


## 2026-10-18 panic and hybrid-output checkpoint

The new SBT serial panic reports ELF prefix `ddc24091e425a463`. The matching local artifact at `SolarBatteryTelemetry/.pio/ble-smoke/usb/firmware.elf` has SHA-256 `DDC24091E425A463B0AD77B71F8A43B6F72C0809C66BFFA66395C549A635CF9F`. Its decoded failure path is `abort -> std::terminate -> __cxa_allocate_exception -> operator new -> AsyncWebServerResponse::addHeader -> beginResponse -> send -> ConfigManagerWeb::setupAPIRoutes` (route lambda).

Disassembly identifies the `/live_pages.json` HTTP 503 `{"error":"pages_unavailable"}` fallback after failed/empty page-index generation. A 32-byte response-header allocation fails in this path. This identifies the immediate allocation failure, but does not establish why usable heap was exhausted or whether prior corruption/fragmentation contributed. SIL reportedly shows the same browser-triggered symptom without a serial trace; a shared cause remains unproven. Earlier truncated-response observations remain separate.

On `feature/hybrid-json-output`, the approved 4.6.1 candidate replaces temporary document trees in live page index, layout and metadata with bounded direct output. ArduinoJson parsing/provider callbacks and numeric formatting remain. Focused host tests and an ESP32 compile-only build pass; the final test firmware is 48 bytes larger than its saved baseline. The independent review's embedded-NUL key regression is repaired and covered. See [implementation and validation record](superpowers/plans/2026-10-18-hybrid-json-output.md).

The candidate remains uncommitted and has not been installed in SBT or SIL. Reduced document allocation churn is established by the implementation; lower target heap peaks, eliminated fragmentation, HTTP delivery stability and panic resolution are not yet established. Keep this incident open until bounded device evidence and owner confirmation are available.


### Follow-up: intermittent panic with the hybrid candidate

The owner subsequently rebuilt SBT against `ConfigurationsManager` (local CM 4.6.1), supplied a new panic, and confirmed that it occurs intermittently on opening/reloading the website; some page loads succeed. The new boot log reports application version 1.0.0. This is owner-provided runtime evidence, not an agent-run hardware test.

Exact matching ELF SHA-256: `92E1E0661B01D89AF8A5E86E1983932511B1B97502ACA6BA8A85C2C630B43332`. A hash-verified copy is retained locally at `.local/panic-20261018-92e1e0661b01d89a/firmware.elf` before any further build. The source artifact was `SolarBatteryTelemetry/.pio/ble-smoke/usb/firmware.elf`.

Independent disassembly verification identifies `/live_pages.json` again, but now its successful generation path: the new `cm::JsonOutput<String>` emits a nonempty page index, a `cm::BoundedResponse` is constructed, and the regular `request->send(response)` call at `0x400e384d` reaches `_assembleHead`. The two `pages_unavailable` fallback paths were not taken. Adding `Accept-Ranges: none` fails while allocating an 8-byte `LinkedListNode<AsyncWebHeader*>` (`movi a10, 8` at `0x400efc5f`, allocator call at `0x400efc65`). The call chain is `LinkedList::add -> AsyncWebServerResponse::addHeader -> _assembleHead -> cm::BoundedResponse::_respond -> AsyncWebServerRequest::send`, followed by the C++ allocation-failure/terminate/abort path.

This confirms that the hybrid output is present and did not eliminate the panic. It does not prove which preceding activity consumes or damages the heap. The outer `new (std::nothrow)` for the response does not protect later throwing allocations in `addHeader` or `_assembleHead`. The existing SBT `printHealth` call is commented out and its Debug-level output would also be filtered by the current Info-level logger, so the supplied log lacks the needed request-time heap evidence.

Next diagnostic scope: record bounded, allocation-free heap samples (free bytes, largest block, minimum and integrity) immediately before/after page-index generation and before response/header construction; distinguish concurrent requests and startup/BLE activity. Keep diagnostic output independent of the allocating log/JSON path. Compare a successful and a failing load before selecting a corrective change. No new product code, rebuild, upload, device action or final commit was performed for this analysis.


### Health-sampled follow-up: appinfo CORS allocation

Owner-supplied one-second samples preceding ELF `C89291931956DE8EC43FD8FA9175168D90D99B141F27B78E636AA2C026B55278` show free heap 34,940 bytes at 5,198 ms, 25,948 bytes at 10,203 ms around MQTT connection/system-info publication, and 28,168 bytes unchanged from 12,203 through 16,206 ms. The largest free block recovers from 7,924 to 12,788 bytes, while minimum-ever free heap remains 15,608 bytes. These samples do not show continuous depletion in that interval and do not measure the short allocation peak at the failing request. MQTT temporal overlap alone is not causal proof.

The hash-matched ELF is preserved at `.local/panic-20261018-c89291931956de8e/firmware.elf`. Its new crash is in `/appinfo`, at `enableCORS(response)` (call `0x400e550d`), while adding `Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS` (call `0x400e056f`). ELF literals independently confirm the CORS header and appinfo fields. Again the failed request is exactly 8 bytes for the header list node (`0x400efc5f` size argument; call `0x400efc65`). This is a different route and header than the preceding live-pages panic, strengthening the need to inspect shared allocator state and concurrent response lifetimes instead of assuming a page-index-specific serializer defect.

The log identifies the firmware as SolarBatteryTelemetry 1.0.0 on .126, with an SIL-test MQTT client ID/topic. Do not count this as independent SIL-firmware evidence. No build, upload, or device interaction was performed in this analysis. The next useful evidence remains request-boundary allocator/integrity samples and a bounded allocation-failure record using fixed storage rather than the normal logging path.


### App-name recovery candidate (2026-10-18)

The owner requested a fix after the page remained visible with the generic ConfigManager heading. The embedded WebUI was verified to match the failing ELF: its network-error catch skipped the legacy `/version` fallback and erased identity, and the initial app-info request had no retry.

The 4.6.1 candidate now uses `/version` after transport, HTTP, schema or JSON failure; retains an already loaded identity on temporary outage; and retries structured app info at most three times with 2/5/10-second delays. Each fetch plus body read has a four-second deadline. A later GUI WebSocket reconnect may start a fresh bounded cycle; active/scheduled cycles do not overlap. Unmount cancels requests and timers. GUI logging is enabled only after a structured response explicitly confirms it. After exhausted retries without a later reconnect, no permanent background retry is performed.

Validation: 24/24 focused WebUI Node tests passed (13 new cases). The generated gzip page passed a headless-browser test with all device endpoints/sockets mocked: connection-reset fallback, delayed structured recovery, and retained heading/title after a later reconnect failure. `npm --prefix webui run build` and `node tools/webui_to_header.js` succeeded; compile/link via `pio test -e runtime-meta-test --without-uploading --without-testing -v` passed, with zero on-device test cases executed. Independent review found no substantive UI issue. The initial browser fixture selected the wrong mocked socket; selecting the GUI socket corrected the fixture and the final run passed.

This repairs the identified UI recovery behavior only. Read-only review found no proven response-retention defect in the inspected BoundedResponse/AsyncTCP lifetime. Concurrent startup requests and the app-info document lifetime remain allocation-peak investigation leads, not proven panic causes. HTTP transport and backend serialization were not changed in this follow-up. The ESP panic remains open. No upload, device interaction, commit or push was performed.


### Later appinfo header-object failure and diagnostic candidate

The owner supplied a further intermittent panic with exact ELF SHA-256 `62D613765E85CE17F1C13688AD08AC47C2E2FAE82FAC001F3CBA271120EAA719`, preserved locally at `.local/panic-20261018-62d613765e85ce17/firmware.elf`. The log shows 28,064 free bytes at 69,271 ms, with only 6,388 bytes in the largest free block and a minimum-ever free heap of 13,356. Earlier largest-block samples in the same excerpt were 4,084 and 4,596 bytes. These are evidence of limited contiguous capacity at the sampled times, not proof of what happened at the later allocation failure or of heap corruption.

The new crash again reaches `/appinfo -> enableCORS -> addHeader` for `Access-Control-Allow-Methods`, but now fails allocating the 32-byte AsyncWebHeader itself (`movi a10, 32` at `0x400f3d73`, allocator call `0x400f3d79`), before the list-node allocation seen in the preceding report. The failure is therefore not limited to the list-node type.

The candidate now emits `/appinfo` through `AppInfoJson.h`/JsonOutput with one output reservation and moves the payload into BoundedResponse. It no longer retains the ArduinoJson 7 document pool (the host reference requests at least 4,096 bytes) or a separate BasicResponse payload copy during CORS construction. Output/object allocation failures close the connection without allocating another error response. Internal framework header allocations can still fail; this peak reduction is not a proven panic fix.

`HttpHeapDiagnostics.h` adds request-stage samples for `/appinfo` and `/live_pages.json`, including each CORS-header boundary. Lines start with `[HTTP-HEAP]` and contain route, stage, MALLOC_CAP_DEFAULT free/largest/minimum bytes, free-block count, and heap integrity (`ok` or `CORRUPT`). The probe uses a fixed local struct and ROM output, bypassing allocating logging tags/JSON/Arduino Strings. No global allocator callback is installed. Probes and stage strings are excluded by preprocessor when `CM_ENABLE_VERBOSE_LOGGING=0`. Heap walks and UART output can change timing; samples are not atomic with allocations by other tasks. Do not treat a clean integrity result or successful diagnostic run as proof that the intermittent fault is gone.

Validation: app-info host goldens/schema/control-byte/failure-cleanup tests pass. Diagnostics host tests pass with verbose 0 and 1; the disabled binary contains no `[HTTP-HEAP]` marker. Existing bounded JSON ownership/transport tests and static-gzip tests pass. CM compile-only `runtime-meta-test` passes with verbose disabled (zero on-device test cases run). The actual SBT consumer `pio run -e usb` passes with its existing verbose-enabled configuration: static RAM 85,216 bytes; flash 1,388,717 bytes. Independent review found no substantive issue in the focused changes. No upload, monitor, device setting, commit or push was performed.

Next owner test: rebuild/upload the linked SBT checkout as usual, open/reload the page, and capture the `[HTTP-HEAP]` sequence through the first panic (or a comparable successful load). A `CORRUPT` stage points toward earlier heap damage; a sharp free/largest drop localizes the pressure. Preserve the new build's ELF for any subsequent panic.


### 160fa396 reports: allocator capability mismatch and CSS stream

The owner supplied two more panics from exact SBT ELF `160FA3965292F3A370283692981BBDE215B7970498C64B3EF314D23171CA9239`, preserved at `.local/panic-20261018-160fa3965292f3a3/firmware.elf`. One failed an 8-byte header-list node in `AsyncWebServerRequest::_parseReqHeader`, before any route handler. The other failed a 32-byte response header in `_assembleHead` while sending `/user_theme.css`; the route and allocation were independently matched by source/disassembly. The app-info probe sequence preceding the second panic completed through `send`, with no reported integrity error.

A key measurement correction: the pinned Arduino core implements ESP.getFreeHeap()/getMinFreeHeap() using MALLOC_CAP_INTERNAL. The HTTP probes use MALLOC_CAP_DEFAULT. These pools overlap but are not interchangeable; internal totals can include memory not eligible for normal byte-oriented malloc. Thus the earlier 28 KB Health readings did not establish that 28 KB was available to `operator new`. In the new app-info trace, default-capable free memory is only 5,764 bytes at entry and 6,176 bytes before send, despite a nearby internal total of 28,036. The largest default block ends at 4,084 bytes and minimum is 4,488. All shown integrity checks are `ok`; that does not prove integrity at the later failure. Concurrent frees make stage-to-stage byte differences unsuitable as isolated allocation counts.

The CSS route previously created an AsyncResponseStream with its pinned default 1,460-byte cbuf, even though built-in CSS is currently two bytes. The candidate now uses the existing bounded sender with static built-in bytes. Custom CSS retains an owned exact-length snapshot, preserving the old immediate-copy lifetime rather than borrowing mutable app storage. Empty content, embedded NULs, cache headers and failure cleanup are covered by focused host tests. Constructor and header allocations can still fail.

Because the request-parser panic occurs before the CM probes, an app-owned allocator-failure hook is added in SBT 1.0.1 under CM_ENABLE_VERBOSE_LOGGING. It reports the first 16 failure events with `[HEAP-FAIL]`: requested size/capabilities/API and eligible/default free/largest memory. Fixed local structs, atomics and ROM output avoid the normal allocating logger; no integrity walk is performed inside the callback. It replaces the single process-wide hook only in the owning SBT application (no existing callback was found), rather than installing one from CM. General ISR/cache-off safety has not been qualified. Health's heap/min/largest now all use DEFAULT; the old broader total is explicitly labeled internal. All diagnostic code is excluded with verbose=0.

Focused CSS snapshot/ownership/error tests, existing bounded-response tests, HTTP probe guard tests and SBT failure-hook tests (verbose 0 and 1) pass. Independent review found no confirmed regression. Firmware compile results are recorded in the implementation plan. Hardware validation and the actual allocation-pressure source remain open; no upload, monitor, commit or push is authorized by this record.


### 961dc7e1 reports: exhausted callback budget and proven low-memory window

The owner supplied both successful loads and a panic from exact ELF `961DC7E19F95732A83D3561CFE2EA07AE9C994D76C3AA8ECE737A308DBCFCBCE`, preserved at `.local/panic-20261018-961dc7e19f95732a/firmware.elf`. The missing `[HEAP-FAIL]` at the final panic is explained by the original first-16-event output limit, already exhausted by earlier recoverable failures. The recorder now retains all small failures up to 64 bytes after that limit and samples every 64th larger event; host tests verify the late critical case and periodic sampling.

The critical page-index sequence is explicit: at entry DEFAULT free=976/largest=500; after JSON free=640/largest=172; integrity remains `ok`. Disassembly reaches BoundedResponse construction and its `Connection: close` header's 32-byte object allocation before response-ready. This establishes an immediate memory-shortage window; it does not identify one leaking component. Earlier events also demonstrate fragmentation: a 1,536-byte request fails with several KB free but largest eligible block=1,524. Separate callback snapshots for eligible and DEFAULT capabilities can differ because other tasks allocate/free between the two queries.

The 1,536-byte size has a concrete matching producer in the pinned TCP stack: effective MSS=1,436, rounded PBUF_TRANSPORT header reservation=76 and pbuf structure=24 bytes. The linked tcp_write path therefore requests 1,536 even for a smaller bounded application write. `LWIP_NETIF_TX_SINGLE_PBUF=1` makes this allocation use the maximum segment size; setting NODELAY does not remove it in this build. Source and ELF support TCP as a strong source of the repeated events; without each event's caller stack, HTTP versus MQTT traffic is not distinguished. Sizes 1,460 and 2,308 have not been assigned a definitive producer. MQTT's configured 1,536-byte application buffer alone does not prove repeated allocation from that component.

The WiFiClient errors are nonblocking socket writes returning errno=11/EAGAIN, retried by the pinned client. The displayed `No more processes` text must not be interpreted as a measured process/task limit. These failures fit network buffer pressure and can stall the main loop while retries run.

A separate concrete peak source is confirmed in the mounted WebUI: Theme, app identity, live discovery and GUI WebSocket start in parallel, while the already-mounted child starts runtime/socket work before schema loading. A bounded sequential bootstrap is being tested to reduce this overlap. It cannot guarantee a global heap budget or protection from independent clients, and browser body completion is not proof that the device has already processed the final TCP ACK. No global TCP/NODELAY/configuration change is made.


### Serialized startup candidate: software validation complete

The WebUI now sequences theme, first app identity attempt, shared page discovery, selected layout/metadata, a bounded OTA probe and first runtime snapshot before its first runtime socket attempt; the GUI socket follows that attempt. The parent gates the child's initial mount, and app-info retry timers pause during this bootstrap. Each initial HTTP phase consumes its body with a finite deadline. A first bounded error still releases bootstrap so recovery/safety/OTA do not wait indefinitely for successful schema loading; Settings and Flash stay guarded during startup.

The mounted regression test first demonstrated the old burst and early socket/runtime work. During implementation, a pre-existing use of permanent runtimeRequest.cancel() for page refresh was replaced by suspend(), keeping permanent cancel for unmount. Review then found that repeated metadata failures with unchanged [] groups could leave HTTP safety polling suspended; the final bounded refresh cleanup explicitly restores [] demand on every relevant current-generation exit. The test covers an initial stalled body, a second 503 and eventual success while core safety reads continue.

Final software validation: 31 WebUI Node tests pass; mounted-browser order/body-delay/navigation/visibility/repeated-schema-error/initial-OTA tests pass; regenerated embedded page's recovery browser test passes; independent review has no open substantive finding. `npm --prefix webui run build` and header generation pass. CM verbose-off compile-only and isolated SBT verbose-on builds pass. The SBT ELF SHA-256 is `6E402C02D42CF2323901A3C39B098B59B6A8C53BAD85A43F057E97D7874DE8F4`; its actual embedded gzip (61,595 bytes) matches the generated header. These are compiler/browser proofs, not target heap measurements. CM remains unreleased 4.6.1 and SBT remains the uncommitted 1.0.1 candidate.

This reduces a demonstrated startup overlap, but does not enforce a process-wide heap budget, eliminate fragmentation, or protect independent tabs/clients. A consumed browser body is not proof that the ESP has processed the final ACK. Device confirmation is still required. No upload, monitor, commit or push has been performed.

### 2026-10-18: compact metadata BME acceptance smoke

CM4.7.0 adds opt-in compact fields; only BME280-Temp-Sensor was migrated.
On COM7 at192.168.2.126, the example initialized the sensor and rendered its
four compact Float fields plus legacy warning. Two serial observation windows
(total270s) contained no abort/backtrace/reset evidence after initial setup.
Full flash was backed up and only activeapp0 at0x10000 was uploaded; partition
table,NVS andOTA selection were byte-identical before/after upload and runtime.

Delivery is NOT fully accepted: an8s repeated HTTP smoke aborted on read timeout.
A separate diagnostic returned complete appinfo107B,meta1302B,runtime539B,
pageindex201B,layout779B bodies, but meta took10.58s. A static root response
advertised61595B and closed with3072B received after~13s. After an intentional
settings-verification reboot, a full root request delivered61595/61595B in9.74s.
The short client timeout explains some failures, not the independent early EOF.
Browser automated reload count was lost in a tool timeout and must not be
reported as10successfulreloads. Private detailed JSON/serial evidence is under
.local/compact-runtime-metadata; no secrets belong in this report.

The feature does not change BoundedResponse/StaticGzipResponse/WebServer.cpp.
Pinned ESPAsyncWebServer already sets server NODELAY and disables request RX
idle timeout during responses. AsyncTCP ACK-timeout closure after a stalled
512-byte bounded window is a plausible next diagnostic target, not a decoded
cause. A useful follow-up is ACK/admission/send-error/close-reason tracing;
another setNoDelay call would not address the existing setting. Keep the
original SBT/SIL crash and HTTP-delivery acceptance open pending owner evidence.

### SBT compact-metadata test follow-up (2026-10-18)

After successful bounded BME/System-metadata smoke testing, owner-authorized SBT
1.0.2 with all 64 application fields compact was uploaded app-only to COM7.
The first browser load produced a Core 0 LoadProhibited panic, not the earlier
Core 1 abort. Exact deployed ELF SHA-256:
`7ded81181f7c496f6cffebb6c1a3303d00dced284b920247190fd6026e3c8e01`.
Backtrace `0x40142326:0x3ffdf390 0x401328be:0x3ffdf3b0` resolves to lwIP
`sys_sem_signal` (sys_arch.c:152) and `tcpip_thread_handle_msg` / `tcpip_thread`
(tcpip.c:173/154). This locates the observed failure, not its root cause.

After reboot SBT identity, all 64 metadata fields and its WebSocket UI were
available. Do not mark the panic fixed or declare this a proven metadata
regression. No transport/dependency workaround was applied. Settings preservation
was verified separately (all app values unchanged; only SDK WiFi cache changed).
Full separate SBT plan/evidence pointers:
`C:/Daten/_Codding/ESP32/SolarBatteryTelemetry/docs/plans/2026-10-18-compact-runtime-metadata.md`.
Owner performs the next behavior test. Keep this issue open.

### Bounded HTTP transport follow-up (2026-10-20, CM 4.7.1 Unreleased)

A private SBT 1.2.0 probe reproduced an active `/runtime.json` response closed by
AsyncTCP's ACK timeout after 5025 ms. Of 2946 declared body bytes, 1536 were admitted;
1024 body bytes were acknowledged and the next 512-byte block remained pending.
TCP still had 5248 bytes of send space; no output/flush failure was logged. Sampled
free heap remained 26124-26840 bytes with an 18420-byte largest block and WiFi
status 3. This establishes the observed timeout/close path, not why the ACK was
missing. It does not directly assign the earlier EOF events or explain prior
ESP panics. Five separate RX timeouts had no request/response and were idle
connections. The same probe delivered a 61598-byte UI file in 122 admissions,
requiring 8.25-22.40 seconds; the repeated 512-byte ACK waits amplify transport
latency.

The initial CM 4.7.1 candidate configured only BoundedResponse HTTP clients with
a finite 15000 ms ACK budget and one public negotiated MSS as the admission cap,
limited to 1460 bytes and falling back to 512 when MSS is zero. For the measured
1436-byte MSS this allowed up to 924 additional TCP payload bytes per active
response versus the earlier 512-byte sender, adding one size_t to the object. It changes no MQTT, source
freshness, MOS, OTA flow, dependency or WebUI retry policy. Existing successful
admission accounting and final-ACK ownership remain intact. No NODELAY change is
needed; the pinned server already enables it.

This is not a universal 15-second cleanup guarantee. Pinned AsyncTCP evaluates
its ACK timeout only when the last TX timestamp is strictly later than the last
ACK timestamp. An ACK and the following send within the same millisecond do not
satisfy that condition. The host fixture explicitly models one millisecond
between ACK processing and the next send; its expiry assertions cover that
eligible path. This pre-existing upstream condition is unchanged by CM 4.7.1.

The focused production-header host regression now models transmitted bytes
separately from accepted bytes and gates polls by send space. Its expiry case
models the upstream ACK-timeout eligibility condition. It covers 1436-byte MSS
delivery, 0/256/9000-byte MSS fallback and bounds, a 9-second delayed ACK, partial ACKs, no-ACK polling/15000 ms expiry,
failed flush duplicate safety and the existing six ownership scenarios. The
61598-byte fixture needs 44 successful output operations with full-segment ACKs,
versus 122 at 512 bytes. RED was observed against the former sender; all focused
scenarios pass with the correction.

All four SBT/SIL consumer builds passed for that initial one-segment candidate.
The follow-up below supersedes its window size; device acceptance remains open.
Do not mark the original crash or HTTP-delivery finding resolved before owner
confirmation. Packet loss, client behavior and TCP/event scheduling remain
unassigned hypotheses; increasing this budget does not establish their cause.


### Two-segment candidate follow-up (2026-10-21, CM 4.7.1 Unreleased)

On the stable-power COM8 SIL probe, two runtime responses still closed on actual
ACK timeouts after 15007/15109 ms. The client received exactly the admitted body
prefixes: 2505 of 2803 bytes and 1436 of 2802 bytes. The server retained 4324 bytes
of TCP send space, with no output failure and eligible TX/ACK timestamps, but its
one-segment cap prevented the remaining body from being admitted. The receiver's
ACK absence is unassigned; packet loss, client behavior and event scheduling are
not proved causes. Increasing the ACK budget further is not this candidate.

The refined unpublished CM 4.7.1 candidate clamps nonzero MSS to 1460 bytes, then
allows twice that amount outstanding (maximum 2920 bytes). Zero MSS keeps the
legacy 512-byte window. The 15000 ms ACK budget, timestamp eligibility condition,
COPY admission, flush retry/accounting and final-ACK ownership are unchanged.
No third-party, Nagle, power-saving, zero-copy or source/control timer change is
included. At the measured MSS 1436, each response can queue up to 1436 additional
payload bytes versus the one-segment candidate (at most 1460 on other links),
plus TCP/pbuf overhead. There is no additional response-object field. Concurrent
responses multiply this cost; SBT's roughly 10 KB observed minimum heap requires
fresh device validation rather than an assumed safe margin.

Two new regressions first failed with the one-segment sender: a delayed first
ACK prevented second-segment peer delivery, and a 2803-byte body remained
incomplete at the peer despite its header already being acknowledged. The new
fixture separates peer byte delivery from ACK feedback. With two segments, the
peer receives the complete 2803-byte body before its body ACK, while the sender
retains ownership until that ACK arrives. Partial ACKs replenish only the freed
window; repeated polls without ACK progress remain bounded. The six previous
ownership cases and five earlier MSS/timeout/flush cases remain covered with the
updated limits. All 13 focused scenarios pass; the new pipelining/peer-completion
cases and low/high MSS bounds were observed RED before the product change.

This mitigates a measured application-level ACK dependency. It does not fix or
explain the missing ACK, guarantee arbitrary response completion, or establish
crash/overnight stability. Parent-independent host review, all four rebuilt
consumer artifacts and low-heap SBT/SIL device acceptance are separate gates.


### Matched panic and heartbeat-close candidate (2026-10-21)

The SBT two-MSS test failed under stable COM8 power with allocation failures and
HTTP timeouts. A subsequent observation began after a new boot; it is not valid
cooldown/heap-recovery evidence. The retained core has matching CRC32 8EE39DE8
and application ELF digest prefix 958042be00309985 (full ELF SHA256
`958042BE00309985341C0881B428B39D51FB6BDE3DD458A728018CC0CCE1A7D3`).
Installed Xtensa GDB decoded 15 captured tasks. Core 0 task `esp_timer` panicked
at the conditional abort call 0x40084ec5 in newlib `lock_init_generic`, after
`xQueueCreateMutex` returned zero while WiFi timer error logging needed a UART
recursive mutex. Concurrent `loopTask` was inside failed 8-byte `operator new`
from `AsyncWebSocketClient::_queueControl`, reached through WebSocket close and
`ConfigManagerClass::handleWebsocketPush`. The private matching decode report is
retained with the core; application/library LTO line labels were not trusted
without checking symbols and actual source. The WebSocket object is outside
captured memory, so its queue length and the entire retained-heap composition
cannot be measured from this dump.

Source inspection matches a specific CM defect: heartbeat maintenance requested
close on every application-loop call after a client's 120000 ms deadline. Pinned
ESPAsyncWebServer close appends an uncapped control message while leaving its
status CONNECTED; a stalled peer can therefore accumulate repeated close
controls. This path appears in the simultaneous failing-allocation stack. It is
not proof that all memory loss came from those controls or that TCP/ACK behavior
was otherwise correct.

The unpublished 4.7.1 heartbeat candidate now rechecks the current client's
lastSeen under `wsClientsMutex` and atomically removes an expired subscription
before requesting external close. It samples the current 32-bit clock inside the
same lock, so a Pong received after the detached snapshot is not misclassified.
Close runs after unlocking, permitting a reentrant disconnect callback. Removed
IDs cannot be revived by seen/subscribe; disconnect remains idempotent and an
explicit new connection can register again. Strict `elapsed > 120000` expiry and
uint32 rollover behavior are retained. There is no new closing state, queue
budget, third-party edit, abort policy or changed heartbeat timeout.

The existing runtime host runner extracts the actual production maintenance
method into a synthetic network shell and uses the real subscription helper.
Behavioral RED showed repeated close against a stalled CONNECTED peer and a late
Pong ignored by the snapshot-based check. GREEN covers exactly one close across
10000 maintenance passes, removed-ID seen/subscribe rejection, healthy/deadline
and late-Pong behavior, disconnect/reconnect with a stale snapshot, a reentrant
disconnect callback and rollover boundaries, alongside the existing runtime
selection/provider tests. The separate 13 HTTP sender cases were unchanged and
were not repeated for this heartbeat-only change.

Parent-independent review, rebuilt consumer images and fresh low-heap device
acceptance follow. Keep the HTTP/ACK cause, overall heap-retention attribution
and historical overnight-reset acceptance open; this bounded source correction
is not a complete panic-resolution claim.


### Runtime return-copy allocation failure (2026-10-21)

The subsequent SBT heartbeat candidate stayed on the same boot and recovered
its free heap after a concurrent root/runtime request pair, but one runtime
response returned HTTP 200 with Content-Length 0 in 0.279 seconds. The parallel
periodic runtime response contained 2944 bytes; subsequent pairs returned
2944/2945 bytes. At uptime about 94 seconds the diagnostic recorded one failed
`heap_caps_realloc` of 2960 bytes with free heap 2848 and largest block 1908.
The minimum reached 2728; later samples returned to about 33504 free and 28660
largest. This is a transient failure, not evidence of another reset or a measured
breakdown of every concurrent allocation.

`runtimeValuesToJSON()` returned `out.length() ? out : "{}"`. The conditional
expression copied the String while its JsonDocument and original output remained
alive. The pinned ESP32 String copy reserves `(length + 16) & ~0xf` bytes:
2944 becomes exactly 2960. A failed copy invalidates the returned temporary,
explaining how the nonempty branch could still produce an empty HTTP body.
ArduinoJson's String writer clears its destination on construction, so reserving
before `serializeJson` does not prevent this copy or reserve its final output.

The unpublished candidate now returns the local String directly by move/elision,
rejects document overflow and an output length different from `measureJson`, and
sends HTTP 503 for an empty runtime provider result. The focused host runner
extracts the actual serialization and route bodies. A counted String shell with
copy allocation failure reproduces the old empty return, while the new return
preserves all 2944 bytes with no copy even with optional elision disabled. Tests
also cover document overflow, partial/empty writer output and explicit empty
provider 503 versus valid provider 200. The shell does not model every target
allocator or concurrent provider; no WebSocket queue policy, TCP window, timeout
or heartbeat change is included in this follow-up. Consumer builds and another
bounded hardware gate are required before a device acceptance claim.


### Runtime-only WebSocket backpressure candidate (2026-10-21)

After the runtime return-copy correction, all six concurrent HTTP requests
completed, but the SBT final2 run recorded two failed 2308-byte
`heap_caps_malloc` allocations: free heap 5752, largest block 1524. A selected
Connectivity runtime payload was about 2307 bytes, matching the pinned
`AsyncWebSocketBasicMessage` payload allocation of `length + 1`. Heap later
recovered to about 33 KiB on the same boot, with minimum 1968. This size match was a hypothesis only, not an identified producer. Runtime
backpressure is independently bounded by its documented contract, but this
observation did not identify the failing caller or prove a new panic. See the
private probe below for a later2308-byte WiFi allocation stack. The earlier return-copy and one-shot heartbeat
corrections remain in place; their historical failed gates remain evidence.

The pinned public `AsyncWebSocketClient::message()` owns its message argument,
deletes it on rejection, and destroys queued messages on release/disconnect.
`AsyncWebSocketMessage` has a virtual destructor. CM now wraps its own runtime
messages around a private `AsyncWebSocketBasicMessage`, delegates its public
transport virtuals, and frees the inner copied payload before releasing a shared
atomic reservation. The reservation is independent of subscription-vector
addresses and survives detached copies/disconnect. A new connection receives its
own state. No upstream field or MessageBuffer reference count is inspected.

Each runtime client can have at most one reserved build or outstanding CM runtime
message. Claims occur before snapshot/provider/custom-builder work, so saturated
clients skip the current tick and receive a fresh snapshot after release instead
of accumulating old values. Only eligible clients contribute selected groups to
the current snapshot. Healthy clients proceed independently. The custom builder
keeps its opaque complete-payload contract and runs once when any eligible client
exists. General WebSocket text, events and heartbeat paths are unchanged.

The limit adds one shared atomic state per connected subscription and one small
wrapper per outstanding runtime message, while preventing up to 32 copied runtime
payloads per stalled peer. It is a count/lifetime limit, not a global byte budget;
multiple clients, snapshot documents, HTTP output and TCP buffers can still overlap.
No universal low-memory or network-delivery guarantee is claimed.

The focused host runner extracts the actual `pushRuntimeSnapshot` body. Its old
behavior failed the 10000-stalled-tick assertion before product changes. The new
body keeps one build/message until release, then emits a fresh snapshot. Tests
cover selected/legacy/custom/event modes, late ACK versus actual destruction,
payload-free-before-token-release, queue rejection/payload failure/missing client,
detached lifetime, reconnect, reentrant build, two-thread atomic contention, and
independent slow/healthy selections. Transport allocation is explicitly mocked;
consumer builds and the separately recorded final3 hardware gate remain pending.


### Runtime generation lifetime candidate (2026-10-21)

The final3 SBT gate still failed: two concurrent HTTP requests timed out and one
completed response took about 23.15 seconds. During uptime 111-113 seconds the
trace recorded failed 1622-byte allocations (`caps=0x80c`) and 2308-byte
allocations (`caps=0x1800`), whose callers were not captured in that run, with largest contiguous block 1524 despite
about 7 KiB free. Heap later recovered to about 33 KiB on the same boot and the
minimum was 5416. The 1622-byte allocation's caller is not established by its
capability flags alone. The per-client queue bound remains valid, while this
failed gate shows it did not by itself provide sufficient contiguous memory.

The WS generation path still kept its union JsonDocument and selected projected
JsonDocument alive when `message()` copied the serialized String into the SDK
message. Concurrent HTTP runtime generation could additionally hold another
JsonDocument/output. The final4 candidate addresses these specific lifetimes:

- RuntimeManager has one recursive generation mutex. HTTP runtime JSON waits for
  the provider/document/serialization scope; the main-loop WS path tries once and
  skips a busy tick. The subscription mutex is released before this acquisition.
  Registry callbacks retain their existing short inner mutex scope. The generation
  lock never extends across CM's transport handoff, ACK or network callbacks.
- WS output Strings outlive an inner generation scope; all JSON documents are
  destroyed before that scope releases the guard and before `message()` is called.
  Each selected output String is freed immediately after its handoff. Legacy and
  opaque custom payloads share one original String across their eligible peers.
  Recursive custom-builder calls into runtime JSON remain supported.
- Exactly one eligible selected client serializes its already-selected snapshot
  directly, avoiding a projected document. `fillRuntimeSnapshot` writes selected
  provider/control/alarm groups, uptime and the two OTA safety fields; this is
  the same output selection as the previous filter. Multiple eligible clients
  retain one union snapshot and individual projections, not per-client provider
  regeneration. Events and general text remain unchanged.

The new lifetime assertion first failed against the previous extracted production
push method because a JsonDocument was alive at the SDK boundary. Focused tests
now exercise the actual generation acquisition, HTTP serializer and WS push body:
HTTP-held generation makes WS skip; WS-held generation makes HTTP wait then
complete; recursive custom JSON succeeds; JSON documents are gone and the guard
is available before SDK handoff. A tracked document wraps the real host
ArduinoJson document, and thread-local lifetime checks distinguish each build
from an independently resumed HTTP task. Direct selected JSON equals the prior
filter for empty, A, system and A+B selections, including uptime, both OTA fields,
nested data/nulls and nested names that resemble other groups. The earlier
backpressure, copy-failure and heartbeat regressions remain green.

This does not impose a heap threshold, assign the missing-ACK cause or establish
all allocation ownership. Completed response Strings, queued message payloads,
multiple clients and TCP/WiFi allocations can still coexist. Consumer builds and
the separately preserved final4 device gate remain required; final3 is retained
as a failed acceptance stage.


### Final4 overload result and remaining transport work (2026-10-21)

The added three concurrent root/runtime GET pairs also failed with final4: one
root request timed out after 12.846 seconds and the periodic recorder reported
two 8-second timeouts. First, a 2308-byte allocation failed with 12788 free but
largest block 2292; later 1460-byte allocations failed with about 6700 free and
largest block 1076. The sampled failure counter reached 4480 (85 printed failure
lines are sampling, not the total). Later same-boot health recovered to 28136
free/largest 21492, with minimum 4080. This demonstrates inadequate contiguous
memory under that added load; the generation and queue corrections did not clear
it. Preserve the matching final4 artifacts and failed trace. Do not describe the
candidate as production-cleared or the overload as solved.

The owner authorized CM fixes for evidenced faults. Product source is currently frozen while missing allocation-caller and transport evidence is collected; no further speculative fix is being applied. The separately
named `*-cm471-final4-normal-ui` gates exercise the original Live/all Settings,
three reloads, five-second observer and at least 300-second follow-up, without
the additional parallel GET pairs. Record those normal UI results independently;
a pass cannot erase the failed overload cohorts.

Remaining work belongs to this existing investigation, not a second active plan:

- Obtain the actual allocation caller/task and correlate live HTTP requests,
  response-owned Strings/documents, WebSocket payloads, accepted/unacknowledged
  bytes and connection cleanup. Allocation sizes alone do not prove the 1460-byte
  caller or missing-ACK cause. Track simultaneous counts and lifetimes, not only
  periodic free-heap samples.
- Design admission before expensive generation/payload allocation across HTTP
  and runtime WebSocket work, with an explicit packet receive/control headroom
  contract. Choose limits from measured resource costs and required ordinary
  UI concurrency, not an invented free-heap threshold. Extra load may receive
  documented recoverable rejection/backoff; runtime values and control must
  continue progressing.
- Include resources retained after application-response destruction. The pinned
  `close(bool)` ignores its argument and uses graceful `tcp_close`; response
  destruction alone does not prove unacknowledged TCP buffers were freed.
  Public `abort()` has a different asynchronous error/discard path. Its synchronous
  TCP/IP call and blocking AsyncTCP error-event enqueue require lifecycle/queue
  analysis before any abort policy; do not substitute `close(true)` or release
  admission credits on an unproven kernel-resource lifetime boundary.
- Accept an eventual transport change only with bounded normal and excess-load
  cohorts, including slow/unacknowledged/cancelled peers and reconnect. Require
  complete valid successful responses or explicit recoverable rejection, zero
  allocator failures/reset/panic, continued MQTT/source/control progress and
  recovery without a reboot after load stops. Re-run the previously failed
  concurrency cohort; increasing client deadlines or dropping that cohort cannot
  establish overload recovery.


### Private allocation probe: measured WiFi/TCP callers and distinct AsyncTCP panic (2026-10-21)

A private SBT final4 source/profile copy added fixed bounded allocation-stack records without changing normal SBT/CM source. Its additional1168B RAM/1576B flash is diagnostic overhead; matching ELF SHA256 `F97B16FDA7D7FC197F9E52A2F06CB5DE72D0410F6E4B307E66BFD3B1CA089C36`. Independent source review and build passed. The normal UI cohort had78 complete responses/zero failures over387.172s, including305.029s after UI stress. This does not replace the failed final4 normal-UI gate.

A separate six-extra-GET cohort produced five timeouts, one connection reset, allocation failures and a new StoreProhibited panic. Five intact depth-limited records identify2308B as `wifi_malloc -> esf_buf_alloc_dynamic -> esf_buf_alloc`;1622B as WiFi `ieee80211_output_process`;1536B as lwIP `pbuf_alloc -> tcp_pbuf_prealloc / tcp_write`. No intact1460B caller was captured. Four corrupted/interleaved UART records were excluded. The earlier2307+1 WebSocket size match must not be presented as an identified allocation source; this2308B event is specifically WiFi. These records do not retroactively assign all older same-size events or identify the upper transport caller beyond the captured frames.

The matched core (raw SHA256 `AA1CC8543708F51D3F6B53EC4F20731EE975E2E238DAB30597EFF7B5237F89B3`, CRC32 `C57516F0`) independently proves AsyncTCP `_tcp_poll` immediately stores through null after `malloc(36)` fails. Its event allocation has no null guard in pinned AsyncTCP2.1.4. This is an immediate third-party failure path under extreme pressure, separate from the preceding retention/delivery cause; it is not a stack-walker fault. A null guard alone would not establish adequate receive/control headroom or successful overload recovery. No third-party or product patch was applied from this diagnostic.

The private probe was removed and the exact prior final4 diagnostic image restored on COM8 after target/partition/OTA/hash verification. Controls remain disabled and MQTT root isolated. Full source/resource lifetime accounting, packet ACK evidence and repeatable normal/overload recovery remain required before claiming a complete repair. The owner has authorized the bounded packet capture, but cannot physically confirm Windows UAC while away; no capture has started.
