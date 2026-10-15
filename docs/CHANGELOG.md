# Changelog

This changelog is a curated overview.

## 4.6.0 - Unreleased

- Add a lightweight live page index and optional page selection for layout and
  metadata. Existing unfiltered endpoints retain their full schemas. Load active
  page layout then metadata sequentially and cache successful results in the
  browser; defer configuration loading until Settings opens.
- Preserve exact page keys and source identity when different pages expose the
  same field key. Keep navigation during empty/error states and retry active
  timeouts without applying cancelled page results. OTA safety remains available
  independently of optional System telemetry.

- Release metadata snapshots after their final caller/response owner while still
  sharing concurrent responses. Preserve revision invalidation and single-flight
  rebuilds; later requests rebuild expired snapshots.
- Use safe character appends for JSON numeric slices in layout and metadata
  serialization, avoiding the pinned String pointer-concat source over-read.
- Assemble layout output from one page or placement fragment at a time,
  preserving JSON escaping, ordering and duplicate-key behavior without retaining
  the complete JSON document beside the output buffer.
- Validate complete layout serialization, including document overflow, output
  reservation and every append. Return 503 instead of a partial layout JSON 200
  when allocation or serialization fails.

- Bound static root WebUI gzip responses to 512 accepted-but-unacknowledged
  bytes, preserving headers and payload across zero/short TCP admission and
  failed flushes. Functional hardware acceptance passed for the final root and
  scoped JSON flow; general Wi-Fi/startup recovery remains outside this fix.
- Apply the same bounded sender to successful config, runtime, runtime metadata
  and live layout JSON responses. Move generated payloads into the response and
  retain shared metadata until final ACK or disconnect, without another complete
  payload copy. Preserve schemas, CORS, runtime cache headers and metadata request
  accounting; large config responses now use Content-Length instead of chunked
  framing. Provider allocation peaks and OTA routes remain outside this change.

- Evaluate runtime providers and controls only for the active dashboard page,
  using bounded HTTP/WebSocket group selections and one shared snapshot per
  push tick. Existing full-runtime consumers retain their complete data.
- Reuse successful runtime metadata/layout loads, pause ordinary runtime groups
  in hidden views, and retain GUI/log traffic and the small OTA safety state.
- Abort timed-out response bodies, serialize fallback requests and ignore stale
  page responses. Four-page hardware/browser acceptance passed; an initial
  Wi-Fi/startup refusal episode still required a USB reset. See the page-scoped
  dashboard plan for artifact identities, payload sizes and measured build deltas.

## 4.5.0 - Unreleased

- Add an opt-in BLE transport module with settings, runtime diagnostics, bounded
  queues, and a dedicated worker. Applications explicitly supply NimBLE;
  applications without Bluetooth keep their existing dependency set.
- Add independent OTA lifecycle observation for Arduino and HTTP uploads,
  including session ownership and failure recovery, so optional transports can
  pause during updates without replacing existing reboot or logging callbacks.
- Add a Bluetooth example and focused host checks for lifecycle and transport
  behavior. Physical BLE coexistence and OTA acceptance remain pending.

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

