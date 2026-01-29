# Changelog

This changelog is a curated overview.

## 3.3.0

- IOManager module (digital IO + analog IO incl. alarms + DAC outputs)
- Core settings templates / injection
- Runtime + WebUI robustness improvements
- Logging/MQTT-Logging module
  - Advanced LoggingManager with multiple outputs (Serial / GUI / MQTT)
  - MQTT log output topics (stream + retained last entries)

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
