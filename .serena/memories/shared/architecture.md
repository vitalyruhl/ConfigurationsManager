# Architecture

- Status: verified.
- `ConfigManagerClass` in `src/ConfigManager.h` is the central class for settings,
  persistence, WiFi, web, OTA, and runtime modules.
- `Config<T>`, `ConfigOptions<T>`, and `BaseSetting` define typed settings and persistence
  behavior.
- Main modules:
  - `src/wifi/`: connection lifecycle, recovery, AP mode, and smart roaming.
  - `src/web/`: AsyncWebServer routes, request handling, and WebUI/API integration.
  - `src/ota/`: OTA state and upload handling.
  - `src/runtime/`: live values, metadata, controls, styling, and WebSocket payloads.
  - `src/io/`, `src/alarm/`, `src/logging/`, `src/mqtt/`: optional application helpers.
  - `src/core/`: reusable core settings and WiFi service composition.
- `ConfigManagerClass` contains `ConfigManagerWiFi`, `ConfigManagerWeb`,
  `ConfigManagerOTA`, and `ConfigManagerRuntime`.
- No `WebConfig` or `Webconfig` type exists; the web class is `ConfigManagerWeb`.
- Public headers live under `src/`; this repository has no `include/` directory.
- `webui/` builds the Vue application. `tools/webui_to_header.js` converts WebUI build
  output into checked-in `src/html_content.h`; `src/html_content.cpp` is the wrapper.

Sources: `src/ConfigManager.h`, `src/core/`, `src/wifi/`, `src/web/`, `src/ota/`,
`src/runtime/`, `src/io/`, `src/alarm/`, `src/logging/`, `src/mqtt/`,
`tools/webui_to_header.js`.
