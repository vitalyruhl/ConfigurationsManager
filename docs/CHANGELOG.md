# Changelog

This changelog is a curated overview. Development versions 4.5.0 through 4.7.1
are consolidated into 4.8.0; they were not separate releases.

## 4.10.5 - 2026-10-28

Development versions 4.9.1, 4.9.2 and 4.10.0 through 4.10.4 are consolidated
into this release; they were not separate releases.

- Add an opt-in bounded MQTT transport for numeric IPv4 brokers, with bounded
  connection/loop work, complete-frame receive handling and a fixed 4608-byte
  transmit FIFO. Preserve byte order across temporary pressure and short writes;
  expire transmission only after five seconds without a positive TCP send.
  The legacy transport remains the default. Add compact transport counters,
  ESP reset reason and an optional switch to omit duplicate WiFi uptime.
- Add persisted WiFi power-saving preferences, defaulting to off for WiFi-only
  applications, a thread-safe runtime override and Settings diagnostics. Before
  CM starts BLE, require a fresh acknowledgment of native MIN_MODEM coexistence;
  preserve the saved preference and reapply the effective policy on reconnects
  and resets. Ordinary BLE pause/disable does not release the controller lease.
- Recover the WiFi stack through Arduino-owned OFF/STA transitions, preserving
  the runtime buffer policy and aborting failed transitions without false success.
- Move successful HTTP OTA restart work out of the AsyncTCP disconnect callback
  into the application loop. Keep a one-shot 500ms delay and support optional
  nonblocking reboot preparation. Terminal BLE shutdown runs only in its worker;
  confirm stopped host/native IDLE before object cleanup and restart. Failure or
  a ten-second preparation timeout blocks automatic restart. ArduinoOTA,
  authentication, saved settings and watchdog policy are unchanged.
- Update Vue and its locked renderer/compiler dependencies to 3.5.44, including
  compatible locked parser/source-map updates, and regenerate the embedded page
  ([#86](https://github.com/vitalyruhl/ConfigurationsManager/pull/86)).
- Validate the HTTP/BLE shutdown correction with focused host regressions and
  isolated plus production same-image SBT 1.4.12 OTA. Production restarted with
  SDK reason 3, restored BMS/Victron samples and preserved all 33 settings; the
  following 900-second observation had no new reboot or offline transition.
  See the [bounded acceptance record](acceptance/2026-10-27-http-ota-ble-shutdown.md).
  This is not long-run stability qualification. MQTT reconnect/session replacement
  remains open in [#89](https://github.com/vitalyruhl/ConfigurationsManager/issues/89);
  no root cause, thermal explanation or general TCP/heap recovery is claimed.

## 4.9.0 - 2026-10-23

- Update development-only `shell-quote` to 1.12.0 and `brace-expansion` to
  1.1.21/5.0.12 for local command parsing and file matching.
- Update the Web UI build toolchain to Vite 8.3.4 and its locked bundler/CSS
  dependencies. Regenerate the default embedded page.
- Add independent `CM_ENABLE_LIVE_UI` and `CM_ENABLE_SETTINGS_UI` build switches,
  both enabled by default. Compile out each disabled area's UI, presentation/data
  models, registration storage, JSON serialization and web routes. Support Live
  and Settings together, either area alone with Flash, or a minimal Flash
  interface. Preserve operational configuration/NVS, MQTT, validators, callbacks
  and alarms; disabled UI registration APIs remain compatible no-ops.
- Add a small `/ui_capabilities` response for enabled areas and OTA/authentication
  status without provider evaluation or configuration/password serialization.
  Preserve Flash authentication when Settings is disabled, remove configuration
  read/write/password-reveal routes, and retain OTA controls independently of Live.
- Propagate both switches to Vue/Vite and exclude disabled component graphs and
  their JS/CSS from the embedded page. Key generated-UI reuse by feature values,
  source inputs and header identity; reject mismatched compiler/bundle UI flags.

## 4.8.0 - 2026-10-22

- Add an opt-in BLE transport with consumer-supplied NimBLE, persisted enable
  setting, runtime diagnostics, a dedicated worker and bounded command/event
  queues. Keep protocol decoding and device write permissions in the consumer.
  Reduce retained queue/configuration storage and allow a verified smaller
  outgoing payload limit through `CM_BLE_TX_MAX_BYTES` (default 272 bytes).
  Add thread-safe `cancelPendingCommands()` to fence buffered backend writes;
  already-started writes finish before cancellation returns.
  Cancellation also recovers stale connection/scan state so fresh telemetry can
  resume ([#83](https://github.com/vitalyruhl/ConfigurationsManager/issues/83)).
  Remove one advertisement/GATT peer independently and select acknowledged or
  unacknowledged GATT writes from the characteristic's supported properties;
  reject non-writable targets.
- Add independent Arduino/HTTP OTA lifecycle observation and session ownership
  so optional transports can pause and recover without replacing existing reboot
  or logging callbacks. Preserve successful HTTP acknowledgement/reboot handoff
  and serialize shared flash operations against concurrent update attempts.
- Load dashboard layout, metadata and runtime values for the active page; retain
  unfiltered endpoints and legacy WebSocket clients. Cache successful browser
  schema loads, defer configuration loading until Settings opens and preserve
  OTA safety telemetry while ordinary runtime groups are paused. Cancel stale
  requests, bound body deadlines/retries and serialize startup HTTP/socket work.
- Add opt-in compact runtime field metadata with shared routing text and sparse
  alarm/style/label data. Preserve legacy registration, mutable-pointer access
  through field promotion and JSON output. Use compact metadata for built-in
  System fields and the four BME280 example fields; preserve saved settings.
- Reduce temporary JSON and payload copies in live page indexes, layouts,
  metadata, app identity, theme CSS and MQTT System-Info. Preserve wire formats,
  escaping and response ownership; reject incomplete generated JSON with a
  recoverable error instead of returning a partial successful body. Recover the
  application name after interrupted app-info requests with bounded fallback
  and retries.
- Bound static/JSON HTTP delivery to at most two negotiated TCP segments
  (maximum 2920 bytes; 512-byte fallback for unknown MSS) with a 15-second ACK
  budget. Serialize runtime JSON generation across HTTP and WebSocket paths,
  avoid a runtime return-value copy, allow one outstanding runtime message per
  WebSocket client and close each expired subscription once. Preserve healthy
  peers, selected groups, custom payloads and general text/event APIs.
- Add the WiFi **Connectivity check target** setting (`WiFiProbe`): `off` by
  default, `gateway` for the active DHCP/static gateway, or strict unicast IPv4.
  Enabled checks run every ten seconds with a one-second deadline. `WiFiRb`
  accepts whole minutes 0..9999 and defaults to one minute for new/reset values;
  existing saved values survive and zero disables network-triggered restarts.
  AP mode and OTA suspend network recovery and grant a fresh window afterward.
  Apply is temporary; Save persists the target, including an unreachable one.
- Add an independent 30-second loop supervisor from the first `handleClient()`
  call, configurable/disableable through `setLoopWatchdogTimeout(ms)`. Preserve
  the recovery reason across software resets. Synchronous ArduinoOTA progress
  feeds the loop supervisor and records network-recovery suspension before its
  blocking transfer; asynchronous HTTP traffic cannot mask a blocked loop.
- On 2026-10-22 the operator accepted normal SIL/SBT web access as substantially
  faster and without observed crashes, closing this development iteration's
  HTTP/WebSocket item. Earlier diagnostic/overload failures remain recorded in
  [the investigation history](todo_http_delivery_panic.md); their causes and
  overload recovery are not claimed resolved. This operator acceptance does not
  establish a full repository validation result.

## 4.4.10 - 2026-08-09

- Stop shipping generated full-gate and build logs in the PlatformIO package,
  preventing Windows `WinError 206` failures on deep consumer project paths.
- Preserve the consumer-side precompile toolchain and WebUI regeneration based
  on ConfigManager feature and space-saving flags.
- Add a package-content regression check that rejects generated build-log trees
  while requiring the consumer precompile files.

## 4.4.9 - 2026-08-04

- Updated WebUI dependencies to `vue` 3.5.40, `vite` 8.2.0, and
  `@vitejs/plugin-vue` 6.0.8.
- Applied `npm audit fix` for WebUI development dependencies; audit now reports
  no known vulnerabilities.
- Pinned CI toolchain to Python 3.11 and PlatformIO Core 6.1.19 for reproducible
  dependency resolution.
- Switched embedded WebUI precompile install step from `npm install` to
  deterministic `npm ci`.
- Standardized root and example PlatformIO environments to
  `platformio/espressif32@7.0.1` and aligned key dependency ranges with
  `library.json`.

## 4.4.8 - 2026-07-26

- Improve runtime dashboard reliability during frequent browser reloads (#71).
- Ensure dynamically sized runtime metadata is delivered as complete JSON and
  release completed runtime response connections promptly.
- Keep the existing dashboard visible while recovering from a temporary runtime
  metadata response failure.

## 4.4.1 - 2026-07-18

- Correct Arduino-ESP32 header casing from `ESP.h` to `Esp.h`.
- Restore Linux and other case-sensitive PlatformIO builds.
- No public API or runtime behavior changes.

## 4.4.0

- Added persisted Ethernet IP, subnet, gateway, and DNS settings to the
  `WT32-ETH01-v1.4` example.
- Added Settings password, OTA password, and NTP settings to the WT32 WebUI.
- Added first-run initialization from an optional local `secrets.h` file.
- Added `CM_ENABLE_WIFI=0` to compile out ConfigManager WiFi connection, AP,
  roaming, captive-portal, and WiFi runtime paths.
- Added `ConfigManager.startWebServerOnNetwork()` for Ethernet and other
  externally managed IP interfaces.
- Removed the WiFi station status requirement from OTA initialization so OTA
  can operate over Ethernet.
- Added the `WT32-ETH01-v1.4` example with static Ethernet address
  `192.168.2.127/24`, COM6 serial upload, Ethernet OTA, and a `noota` build.
- Updated the SolarInverterLimiter `espota` authentication value.
- WebUI package version aligned with the ConfigurationsManager package/library
  version at 4.4.0.

## 4.2.3

- Updated WebUI dependencies to `vue` 3.5.38 and `vite` 8.0.16.
- Updated PlatformIO example dependencies to current compatible releases,
  including `ArduinoJson` 7.4.3, `ESPAsyncWebServer-esphome` 3.4.0,
  `Adafruit SSD1306` 2.5.17, and `OneWire` 2.3.8.
- Updated the BoilerSaver published ConfigurationsManager dependency to the
  latest registry release, 4.2.2.
- Excluded `.git` metadata from PlatformIO library exports so local
  `file://../..` example installs cannot copy transient Git refs.
- Switched local example library references to relative `symlink://../..`
  dependencies to avoid copying repository metadata during validation builds.
- WebUI package version aligned with the ConfigurationsManager package/library
  version at 4.2.3.

## 4.2.2

- Updated WebUI build dependencies to `esbuild` 0.28.1 and `shell-quote` 1.8.4,
  resolving their known security advisories.
- WebUI package version aligned with the ConfigurationsManager package/library
  version at 4.2.2.

## 4.2.1

- WiFi connection health handling now bounds `WL_IDLE_STATUS` and
  `WL_SCAN_COMPLETED` transients with a grace window so long scan/roaming
  phases cannot keep a false connected state indefinitely.
- `lastGoodConnectionMillis` is now refreshed only on healthy STA connectivity
  (`WL_CONNECTED` with a valid local IP), allowing reconnect escalation and
  auto-reboot fallback to trigger when recovery fails.
- Smart Roaming remains enabled and supported; roaming transitions now include
  additional diagnostics around transient start, grace, expiry, and recovery.
- WebUI package version aligned with the ConfigurationsManager package/library
  version at 4.2.1.

## 4.2.0

- WebUI OTA flashing now shows an active upload progress popup and guards
  against accidental page navigation while the firmware upload is running.
- WebUI now also reacts to externally initiated OTA activity, such as
  PlatformIO/ArduinoOTA uploads, with a global lock overlay for unsafe actions.
- OTA activity state changes now push an immediate runtime update and stale OTA
  activity is cleared automatically after a 120 second inactivity timeout.
- AP/setup mode now redirects minimal captive-portal compatibility probes
  (`/generate_204`, `/canonical.html`) and guarded unknown navigation routes to
  the Web UI entry point while preserving normal 404 behavior in STA/runtime mode.
- WebUI package version aligned with the ConfigurationsManager package/library version at 4.2.0.

## 4.1.1

- Expanded PlatformIO library metadata keywords for config-related discovery, including `config`, `config-manager`, `settings`, `wi-fi`, and `nvs` variants.
- Updated package description wording to include both "config" and "configuration" search intents.
- WebUI package version aligned with the ConfigurationsManager package/library version at 4.1.1.

## 4.1.0

- Added lazy MQTT extra-topic publish helpers so payload callbacks run only after
  topic validation, MQTT connectivity, and publish-interval checks pass.
- WebSocket runtime push now defaults to 5000 ms and clamps configured intervals
  to 550..60000 ms while keeping the existing API.
- Smart WiFi roaming now throttles scan attempts separately from actual roaming
  reconnect attempts to avoid repeated scans when no better AP is available.
- SolarInverterLimiter now offers an `ota_no_oled_no_bme` build variant and hides
  BME280/I2C-only settings and telemetry when the optional sensor/display stack is disabled.

## 4.0.6

- SolarInverterLimiter example now defaults to ConfigurationsManager 4.0.5 and
  keeps relay output code behind compile-time feature flags to reduce firmware
  size when fan and heater outputs are unused.
- SolarInverterLimiter firmware version bumped to 4.2.0.

## 4.0.5

- Web and HTTP OTA routes are now registered only once, preventing repeated
  WiFi reconnects from growing the AsyncWebServer handler list.
- WebSocket live updates now default to a 1000 ms interval and skip runtime JSON
  generation while no WebSocket clients are connected.

## 4.0.4

- OTA uploads now hold an active OTA guard so WiFi roaming scans, reconnect
  attempts, and WiFi stack resets cannot interrupt an in-progress transfer.
- WebUI flash now reuses the stored OTA password after Settings/Auth instead of
  opening a second OTA password prompt, including setups that expose hashed
  storage keys in `config.json`.
- SolarInverterLimiter now declares `Adafruit BusIO` explicitly so the SSD1306
  display stack builds cleanly in both `usb` and `ota` environments.

## 4.0.3

- WiFi reconnect recovery now retries after stalled ESP32 WiFi idle/scan states
  instead of waiting indefinitely.
- SolarInverterLimiter firmware version bumped to 4.1.2 for the runtime WiFi
  recovery behavior change.

## 4.0.2

- WebUI npm dependencies updated; Vite upgraded to 8.0.13.
- Added esbuild as an explicit devDependency for the Vite 8 build path.
- WebUI build target changed to `esnext` for Vite 8 / Rolldown compatibility.
- WebUI package version aligned with the ConfigurationsManager package/library version at 4.0.2.
- Independent example firmware/app versions were not changed.
- SolarInverterLimiter remains close to the flash limit after the update.
- Older browser compatibility may be reduced because the WebUI no longer targets the previous legacy browser output.

## 4.0.0

- Breaking release from the 3.2.x line (public API and behavior refactors across modules/examples).
- IOManager module (digital IO + analog IO incl. alarms + DAC outputs)
- Core settings templates / injection
- Runtime + WebUI robustness improvements
- Logging/MQTT-Logging module
  - Advanced LoggingManager with multiple outputs (Serial / GUI / MQTT)
  - MQTT log output topics (stream + retained last entries)
- Helper module: shared pulse/blink utility (blocking + non-blocking), dewpoint helper, mapFloat reuse across examples (BoilerSaver/SolarInverterLimiter/BME280).
- SolarInverterLimiter example exposes a "Manual Override" runtime switch so the fan/heater UI buttons can pause automatic control.

## 3.2.0

- Minor bump / maintenance release

## 3.1.1

- WebUI header vs tab title split (`setAppName` vs `setAppTitle` via `/appinfo`)
- Improved settings password prompt for passwordless setups
- WiFi reconnect-storm mitigation during scans

## 3.1.0

- v3 stabilization: runtime/UI improvements
- Ordering fixes in Settings/Live views
- WebSocket/OTA fixes

## 3.0.0

- v3 release: removed most build-time feature flags
- Embedded WebUI committed to the library (`src/html_content.*`)
- Docs reorganized

## 2.7.4

- Tools folder included in the library package (no manual copying)

## 2.7.0

- Modular managers
- Smart WiFi Roaming
- Runtime provider improvements

## 2.4.0

- Runtime JSON + WebSocket live values + runtime metadata + alarm styling

## 2.3.0

- `ConfigOptions<T>` aggregate init + `showIf` visibility

## 2.0.0

- OTA support + examples
- UI naming improvements

## 1.x

- Initial release

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| _none_ | - | Changelog only; no dedicated public API in this document. | See topic docs for API details. |
