# Deferred HTTP delivery and panic investigation

Status: open; original evidence checkpoint 2026-10-16, follow-up 2026-10-18. Owner: ConfigurationsManager (CM), with separate SolarBatteryTelemetry (SBT) and SolarInverterLimiter (SIL) consumer checks.

The owner requested a durable handoff because investigation may resume a week later. Preserve the current CM feature branch on origin; do not integrate it into main while these failures remain unresolved. This document records an investigation, not an implemented fix or a proven shared root cause.

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
