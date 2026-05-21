# Changelog

This changelog is a curated overview.

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

