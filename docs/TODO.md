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

- Auto reboot timeout
  - Initialize by default
  - Move [Reboot if WiFi lost (min):] setting from System → WiFi

- WebSocket defaults
  - Initialize inside `ConfigManager.startWebServer()` by default
    - `enableWebSocketPush`
    - `setWebSocketInterval(1000)`
    - `setPushOnConnect(true)`

---

## Feature Follow-ups (v3.x)

### Modularization

- Extract optional modules:
  - MQTTManager (already mostly done)
  - Logging module (advanced, optional)

---

### Module Migration Plan

1) MQTTManager
   - Publish helpers (typed, retained)
   - Publish scheduling:
     - Interval
     - Publish-on-change
   - Stable publish item ordering
   - Decide singleton vs multi-instance

2) Logging module
   - Core: minimal logger
   - Optional: advanced logger

3) Refactor examples
   - Start with Full-IO-Demo
   - Later: SolarInverterLimiter

4) Documentation
   - Include, dependencies, memory impact

---

## Medium Priority (Prio 5)

- Input logging with trends (analog/digital)
- SD card logging (CSV)
- Alarm helpers for IOManager
- IOManager improvements
  - PWM/LEDC backend
  - Output ramping
  - Fail-safe states
  - ID-based persistence + migration
- Remove unused WebUI runtime templates (`webui/src/components/runtime/templates/*.enabled.vue`)
- Deactivate Settings UI via build flag or runtime config
- Deactivate OTA UI when OTA is disabled (auto-hide or build flag)
- Add explicit switch to disable OTA UI
- Verify external WebUI hosting switch (CM_EMBED_WEBUI) and document; if missing, add a switch

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

- [COMPLETED] Logging redesign (core logger + advanced LoggingManager baseline)
  - Multiple outputs with independent levels
  - GUI log output buffer flush on WebSocket connect
- [COMPLETED] WebUI log tab (live logging view)
- [COMPLETED] Human-readable uptime
- [COMPLETED] Restart-loop bug fixed
- [COMPLETED] Remove obsolete config switches
- [COMPLETED] WebUI redesign (Vue 3, theming, tabs)
- [COMPLETED] Configurable browser title
- [COMPLETED] WebUI debug logging flag
