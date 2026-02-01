# TODO / Roadmap / Issues

## High Priority (Prio 1)

- WebSocket defaults
  - Initialize inside `ConfigManager.startWebServer()` by default
    - `enableWebSocketPush`
    - `setWebSocketInterval(1000)`
    - `setPushOnConnect(true)`
- Include, dependencies, memory impact
- check all module docs for consistency
- Add minimal usage examples for each module
- check/add all public methods to docs like in LoggingManager docs  `void log(Level level, const char* tag, const char* message, unsigned long timestampMs)` (how many overloads) - short description.`
- Remove unused WebUI runtime templates (`webui/src/components/runtime/templates/*.enabled.vue`)
- Deactivate Settings UI via build flag or runtime config
- Deactivate OTA UI when OTA is disabled (auto-hide or build flag)
- Add explicit PIO switch to disable OTA UI
- Verify external WebUI hosting switch (CM_EMBED_WEBUI) and document; if missing, add a switch

## Medium Priority (Prio 5)

- SD card logging (CSV)
  - Input logging with trends (analog/digital)
- Alarm helpers for IOManager
- IOManager improvements
  - PWM/LEDC backend
  - Output ramping
  - Fail-safe states
  - ID-based persistence + migration

## Low Priority (Prio 10)

- Headless mode (no HTTP server)
- Card layout/grid improvements
- WiFi failover
- HTTPS support (wait for ESP32 core)
- WebUI ToastStack feature planned but unused (put back into TODO / decide keep/remove)

## Ideas

- Matter support
- BACnet integration

### Done / Resolved

- Helper module added to core (shared pulse helpers + dewpoint/mapFloat utilities) and both BoilerSaver/SolarInverterLimiter/BME280 examples refactored to use it.
