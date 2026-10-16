# Deferred HTTP delivery and panic investigation

Status: open; evidence checkpoint 2026-10-16. Owner: ConfigurationsManager (CM), with separate SolarBatteryTelemetry (SBT) and SolarInverterLimiter (SIL) consumer checks.

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

This handoff changes documentation only. No firmware, dependency, version, changelog, device setting or test result is changed. Completion requires evidence for the actual fixes and owner confirmation; the unexplained historical panic must remain explicitly open if no causal evidence is recovered.
