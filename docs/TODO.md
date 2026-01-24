# TODO / Errors / Ideas

## Current Focus

### v3 stabilization & roadmap

- As of: 2026-01-09
- UI/UX (Settings & Runtime)

  - `order` / sorting: metadata for live cards and settings categories (stable ordering).
  - Analog: separate input (NumberInput) vs slider; adjust slider current-value styling.
- Architecture / API
  - Unified JSON handlers: route all POST/PUT config endpoints through robust JSON parsing (no manual body accumulation).
  - Settings schema versioning/migration (when new fields/structure are introduced).
- Test & CI
  - Minimal: unit/integration test for auth + password reveal + bulk save/apply.
  - PlatformIO CI: build matrix for at least one ESP32 env + WebUI build step.

### High Priority Bugs/Features (Prio 1)

- **[IDK]** remove all unnessesary switshes fom configmanager and GUI, leabe only logging and verboselogging - all other switches are not needed any more.


- **[FEATURE]** v3 follow-ups
  - Extract modules that can be imported separately:
    - Logger: split into 4 extras (serial, MQTT, display, sdcard) (also use more, then one logger at once?) (eg. log->Printf([Serial,Display,SDCard,GUI],Module, "Check and start BME280!").Debug();) - so i can use multiple loggers at once, but with diffrent loglevels (eg. log->Printf([Serial,SDCard,GUI],Module, "Check and start BME280!").Debug();log->Printf([Serial,SDCard],Module,"Temperature: %2.1lf °C", temperature).Debug(); ) - eg. module is an String local, or global defined, like "[MAIN]", "[BME280]", "[MQTT]", etc. | Serial,Display,SDCard,GUI are defines or enums for the different loggers depends on how we implement it.
      - or ist it better loke log1 = new Logger([Serial, Display], Debug); log2 = new Logger([Serial, SDCard], Debug); etc. ? - then we can set different loglevels for each logger instance.
      - Display logger: ensure buffer is cleared even when nothing is sent to the display.
    - Check where the latest version is (solarinverter project or boilerSaver).
    - MQTT manager (got it from solarinverter project = latest version)

  - Goal: keep `ConfigManager` as small as possible in compile size and dependencies
    - Suggested implementation order (one side-branch per item)
    - Test target: use `Full-IO-Demo` for steps 1–3; switch to `SolarInverterLimiter` at step 4.

    1) [TODO] Remove Settings List view; keep Tabs only. Add Darkmode toggle (remember choice in coockie?).
       - Update vue to latest version 2.5.27?
    2) MQTT manager module (+ baseline settings + ordering/injection)
       - Add an optional, separately importable `MQTTManager` module (e.g. `#include "mqtt/mqtt_manager.h"`).
       - Ensure the core library does not require MQTT dependencies unless the module is included/used.
       - Core settings auto-load (when the module is used)
         - Provide a module-owned settings bundle that registers a stable, reusable MQTT settings baseline:
           - `mqtt_port`, `mqtt_server`, `mqtt_username`, `mqtt_password`
           - `publishTopicBase`
           - `mqttPublishPeriodSec`, `mqttListenPeriodSec`
           - `enableMQTT`
         - Ensure these settings exist immediately after module initialization (no extra boilerplate in user code).
       - Publish items ordering / injection
         - Custom, user-defined MQTT publish items must appear directly after `publishTopicBase` in the settings category.
         - Provide an API to register additional publish items (and/or dynamic topics) while keeping the baseline order stable.
    3) Logging (lightweight core + optional advanced module)
       - Replace the default logger in the core library with a lightweight implementation (do not require `SigmaLogger` in the default build).
       - Add an optional, separately importable advanced logging module (e.g. `#include "logging/AdvancedLogger.h"`).
         - Provide a dedicated class that encapsulates the display logging logic (queue/buffer, update loop) and allows different display backends.
         - Can internally use `SigmaLogger`, but only inside the optional module (core must not depend on it).
         - Outputs: Serial and one or more display outputs (and optionally MQTT as an extra sink).
    4) Refactor `SolarInverterLimiter` example to consume modules
       - Keep `Smoother` and RS232/RS485 parts inside the example for now; only extract reusable parts (logging, MQTT manager, relay manager, helpers).
    5) Documentation for all modules
       - Add docs for the new modules (how to include, minimal example, dependencies, memory/flash impact).
    6) Update other examples to use the new core settings/modules where applicable [partialy done]

- **[FEATURE]** Bybass an Error/Info into live-view form code (toast or similar)
- **[FEATURE]** Bybass an Error/Info into live-view form code (as a overlay between buttons and cards - allways visible until deactivated from code)

### Medium Priority Bugs/Features (Prio 5) (eg. V3.4+ ?)

- **[FEATURE]** Add logging with simple trend for Analog/digital-Inputs (over time/on logDB)
  - like .addAnalogInputTrend("id", "name", "GUI-Card", interval, logTime in houers?, or max entrys to build an array = better, min, max or auto scale ...);
- **[FEATURE]** add sdcard support for logging data to csv files and logger extension to log to sdcard
- **[FEATURE]** Add separated Alarm handling for Analog-Inputs and shorthands for creation of Alarms
  - e.g. ioManager.addAnalogInputMaxAlarm("id", "name", "GUI-Card", maxValue, callback, ...);
- **[FEATURE]** IOManager follow-ups (after DAC)
  - PWM/LEDC backend (channel allocation, frequency, resolution, attach/detach lifecycle)
  - Output smoothing/ramp (slew-rate limiting)
  - Fail-safe defaults + safe-state on reboot/comm loss
  - Stable persistence: switch from slot-based keys to ID-based keys (requires migration/versioning)
  - Provider/backends: DAC + PWM + external DAC (I2C/SPI)
  - Per-output enable flag + UI visibility toggles + runtime readback


### Low Priority Bugs/Features (Prio 10)

- **[FEATURE]** Card layout/grid improvement
  - If there are more cards, the card layout breaks under the longest card above; make the grid more flexible.
- **[FEATURE]** Failover Wifi native support
- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

### Ideas / Suggestions

- **[IDEA]** what about Matter - the new standard for smart home devices?
  - maybe add a Matter component to the library in the future?
- **[IDEA]** what about bacNet? send data to a bacNet server like mqtt?


### Done / Resolved (but not tested yet)

- **COMPLETED / Bug** Browser tab title is configurable
  - H1 uses `.setAppName(APP_NAME)`
  - Browser tab uses `.setAppTitle("...")`
  - If `.setVersion(VERSION)` is set: it is appended to both

- **[Tooling]** WebUI debug logging toggle (v3.3.x)
  - Add a build-time flag (e.g. `VITE_CM_DEBUG`) and route noisy logs through a `dbg(...)` helper.
- **COMPLETED / Bug/Design** Uptime shows always seconds -> for mat it to human readable format (days, hours, minutes, seconds)
  - days is not tetsed yet
- Library does not include the docs folder.
- **COMPLETED / Bug** check restart behavior, sometimes the device restarts multiple times if wifi also good

- **[Tooling/Design]** Revisit "advanced opt-in" WebUI rebuild (v3.3.x)
  - Goal: library consumption must NOT require Node.js/npm by default.
  - Idea: gate WebUI rebuild + `src/html_content.h` regeneration behind a flag (e.g. `CM_WEBUI_REBUILD`).
  - Decide whether `webui/dist/*` should stay tracked or be generated-only (and ensure release workflow covers it).

### Done / Resolved

