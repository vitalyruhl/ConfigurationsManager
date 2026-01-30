# TODO / Roadmap / Issues

## Current Focus

### v3 Stabilization & Roadmap (UI / Core)

- Goal: keep `ConfigManager` minimal (compile size & dependencies)

- UI / UX
  - Stable ordering for settings & live cards (metadata-based)
  - Separate NumberInput vs Slider (incl. current-value styling)

- Architecture
  - Unified JSON handling for all POST/PUT config endpoints
  - Settings schema versioning & migration support

- Test & CI
  - Minimal auth + bulk-save tests
  - PlatformIO CI (ESP32 env + WebUI build)

---

## High Priority (Prio 1)

### Wi-Fi / WebServer Defaults

- WebSocket defaults
  - Initialize inside `ConfigManager.startWebServer()` by default
    - `enableWebSocketPush`
    - `setWebSocketInterval(1000)`
    - `setPushOnConnect(true)`

---

## Feature Follow-ups (v3.x)

### Module Migration Plan


1) [COMPLETED] Refactor examples to newest v3 module structure
   - [COMPLETED] SolarInverterLimiter
     - remove logging module from there and apply new logging module usage from V3.3.0
     - migrate to newest IO + WiFi + MQTT module usage
     - keep IO pins, preserve logic

2) Documentation
   - Include, dependencies, memory impact
   - check all module docs for consistency
   - Add minimal usage examples for each module
   - check/add all public methods to docs like in LoggingManager docs  `void log(Level level, const char* tag, const char* message, unsigned long timestampMs)` (how many overloads) - short description.`

---

## Medium Priority (Prio 5)

- Remove unused WebUI runtime templates (`webui/src/components/runtime/templates/*.enabled.vue`)
- Deactivate Settings UI via build flag or runtime config
- Deactivate OTA UI when OTA is disabled (auto-hide or build flag)
- Add explicit switch to disable OTA UI
- Verify external WebUI hosting switch (CM_EMBED_WEBUI) and document; if missing, add a switch
- SD card logging (CSV)
  - Input logging with trends (analog/digital)
- Alarm helpers for IOManager
- IOManager improvements
  - PWM/LEDC backend
  - Output ramping
  - Fail-safe states
  - ID-based persistence + migration

---

## Low Priority (Prio 10)

- Headless mode (no HTTP server)
- Card layout/grid improvements
- WiFi failover
- HTTPS support (wait for ESP32 core)
- WebUI ToastStack feature planned but unused (put back into TODO / decide keep/remove)

---

## Ideas

- Matter support
- BACnet integration

---

### Done / Resolved
