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

### MQTT – Core & GUI

- GUI population
  - `addMQTTTopicReceive*` must NOT auto-add GUI entries
  - GUI entries only via `addMQTTTopicTooGUI`

- System card
  - Move `settings_.enableMQTT.get()` under "WiFi Connected"
  - Add "MQTT Connected"
  - Show `getReconnectCount()` (integer only)

- Cleanup
  - Remove all other MQTT GUI elements

- Logging
  - Log all send/receive topics
  - Only if verbose flag is enabled

- System info publishing
  - Publish every 60s (not only on reboot)
  - `info.uptimeMs = millis()` as standalone tag
  - Split into JSON payloads:
    - ESP (chip + memory)
    - WiFi (connection info)

---

### examples/Full-MQTT-Demo/src/main.cpp

- GUI examples using `addMQTTTopicTooGUI`
  - "MQTT-Received":
    - `boiler_temp_c`
    - `powermeter_power_in_w`

  - "MQTT Other Infos":
    - `lastTopic`
    - `lastPayload`
    - `lastMsgAgeMs`

- Callback examples
  - `onMQTTConnect`
  - `onMQTTDisconnect`
  - `onNewMQTTMessage`
  - `onMQTTStateChanged`

- Explain difference vs classic callbacks:
  - `onConnected(callback)`
  - `onDisconnected(callback)`
  - `onMessage(callback)`

- Calculation example
  - Convert `washing_machine_energy_total` (kWh → MWh)
  - Export as MQTT topic
  - Show in GUI (2 decimal places)

---

### MQTT Documentation

- Explain purpose of "Client ID"
- Complete `docs/MQTT.md`
  - All methods
  - Minimal + advanced examples

---

### Wi-Fi / WebServer Defaults

- Auto reboot timeout
  - Initialize by default
  - Move setting from System → WiFi

- WebSocket defaults
  - Initialize inside `ConfigManager.startWebServer()`
    - `enableWebSocketPush`
    - `setWebSocketInterval(1000)`
    - `setPushOnConnect(true)`

---

## Feature Follow-ups (v3.x)

### Modularization

- Extract optional modules:
  - MQTTManager (already mostly done)
  - Logging module (advanced, optional)

#### Logging redesign
- Lightweight core logger
- Optional advanced logger module
  - Multiple outputs (Serial, Display, SD, MQTT)
  - Independent log levels per instance
  - Display buffer must clear even without updates

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
   - Optional: advanced logger (SigmaLogger internal only)

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

---

## Low Priority (Prio 10)

- Headless mode (no HTTP server)
- Card layout/grid improvements
- WiFi failover
- HTTPS support (wait for ESP32 core)

---

## Ideas

- Matter support
- BACnet integration

---

## Done (not fully tested)

- WebUI redesign (Vue 3, theming, tabs)
- Configurable browser title
- WebUI debug logging flag
- Human-readable uptime
- Restart-loop bug fixed
- Remove obsolete config switches


### Done / Resolved

