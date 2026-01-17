# TODO / Errors / Ideas

## Current Focus

### v3 stabilization & roadmap

- As of: 2026-01-09
- UI/UX (Settings & Runtime)
  - GUI display mode toggle: “Current” (cards) vs “Categories” (map/list view).
  - `order` / sorting: metadata for live cards and settings categories (stable ordering).
  - Analog: separate input (NumberInput) vs slider; adjust slider current-value styling.
- Architecture / API
  - Settings auth: remove or hard-disable legacy endpoint `/config/settings_password`.
  - Unified JSON handlers: route all POST/PUT config endpoints through robust JSON parsing (no manual body accumulation).
  - Settings schema versioning/migration (when new fields/structure are introduced).
- Test & CI
  - Minimal: unit/integration test for auth + password reveal + bulk save/apply.
  - PlatformIO CI: build matrix for at least one ESP32 env + WebUI build step.

### High Priority Bugs/Features (Prio 1)

  - Goal: keep `ConfigManager` as small as possible in compile size and dependencies.
  - Suggested implementation order (one side-branch per item)
    - Test target: use `BME280-Full-GUI-Demo` for steps 1–5; switch to `SolarInverterLimiter` at step 6.
    1) Core settings templates / injection (WiFi/System/Buttons)
    2) Fix/extend built-in `system` runtime provider behavior
    3) Relay manager module
    4) MQTT manager module (+ baseline settings + ordering/injection)
    5) Logging (lightweight core + optional advanced module)
    6) Refactor `SolarInverterLimiter` example to consume modules
    7) Documentation for all modules
    8) Update other examples to use the new core settings/modules where applicable
  - Core settings templates / injection
    - WiFi settings baseline must be available immediately during initialization (same keys/categories across projects).
      - Use the `BME280-Full-GUI-Demo` WiFi struct as the baseline reference.
      - Allow user projects to inject additional settings into the same `WiFi` category without forking the baseline.
    - System settings baseline must be available immediately during initialization.
      - Use a stable default set equivalent to the common `SystemSettings` struct:
        - `allowOTA`, `otaPassword`, `wifiRebootTimeoutMin`, `version`
      - Goal: shrink example `main.cpp` by removing repeated boilerplate.
    - Button / IO core settings must be available immediately during initialization.
      - Provide a baseline equivalent to `ButtonSettings` (`apModePin`, `resetDefaultsPin`, ...).
      - Consider IO preset vs settings ownership:
        - Allow a compile-time preset for boot-critical pins (e.g. default AP-mode pin = 13) that is used very early.
        - Still expose the pins as settings so they can be re-mapped in the GUI later.
        - Support inversion/active-low/active-high so the boot check can be configured safely.
    - Treat this as a dedicated refactor item (clean API + stable ordering).
  - **[Bug/Design]** Custom runtime provider named `system` overrides built-in System provider
    - Symptom: System card shows `—` for default fields after adding a custom provider with group `system`.
    - Expected: allow adding extra fields without losing default System fields.
    - Options: provider chaining/merging for identical groups, or a dedicated API to extend the built-in System provider.
    - check if it possable to add/append to the system provider instead of overwriting it. (```.appendValue("system", [](JsonObject &data){data["testValue"] = 42;}, 99);``` or similar)
  - Relays
    - Turn the current `relay(s).h` idea into a proper relay manager class that can be "married" with `ConfigManager`.
      - Settings are owned by `ConfigManager` (pin, active-low, enabled, etc.).
      - Runtime API for code: e.g. `relay.set("fan", true)` / `relay.get("fan")`.
      - Support callbacks/on-change handling so GPIO changes happen when settings change.
      - Do not hard-couple the module to example-specific `settings.h`; use a clean API/config structure.
  - MQTT
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
  - Logging
    - Replace the default logger in the core library with a lightweight implementation (do not require `SigmaLogger` in the default build).
    - Add an optional, separately importable advanced logging module (e.g. `#include "logging/AdvancedLogger.h"`).
      - Provide a dedicated class that encapsulates the display logging logic (queue/buffer, update loop) and allows different display backends.
      - Can internally use `SigmaLogger`, but only inside the optional module (core must not depend on it).
      - Outputs: Serial and one or more display outputs (and optionally MQTT as an extra sink).
  - Example
    - Refactor the `SolarInverterLimiter` example to use the new optional modules.
    - Keep `Smoother` and RS232/RS485 parts inside the example for now; only extract reusable parts (logging, MQTT manager, relay manager, helpers).
  - Documentation
    - Add docs for the new modules (how to include, minimal example, dependencies, memory/flash impact).
- **[Tooling]** VS Code include error for `#include <BME280_I2C.h>`

### Medium Priority Bugs/Features (Prio 5)

- **[FEATURE]** Failover Wifi native support

### Low Priority Bugs/Features (Prio 10)

- **[FEATURE]** Card layout/grid improvement
  - If there are more cards, the card layout breaks under the longest card above; make the grid more flexible.
- consolidate the Version-History before v3.0.0 into less detailed summary
- **[FEATURE]** v3 follow-ups
  - Add something into the system card (full demo).
  - Extract modules that can be imported separately:
    - Logger: split into 3 extras (serial, MQTT, display)
      - Check logger or GUI extra tab.
      - Display logger: ensure buffer is cleared even when nothing is sent to the display.
      - Check where the latest version is (solarinverter project or boilerSaver).
    - MQTT manager (got it from solarinverter project = latest version)
  - Copy the current ones into the dev-info folder.
  - Check whether there are other modules that can be extracted.

- **[FEATURE]** Automated component testing
- **[FEATURE]** Bybass an Error/Info into live-view form code (toast or similar)
- **[FEATURE]** Bybass an Error/Info into live-view form code (as a overlay between buttons and cards - allways visible until deactivated from code)
- **[FEATURE]** add templates for common Settings, like WiFi, System, non blocking blinking LED, etc.

- **[Tooling/Design]** Revisit "advanced opt-in" WebUI rebuild (v3.3.x)
  - Goal: library consumption must NOT require Node.js/npm by default.
  - Idea: gate WebUI rebuild + `src/html_content.h` regeneration behind a flag (e.g. `CM_WEBUI_REBUILD`).
  - Decide whether `webui/dist/*` should stay tracked or be generated-only (and ensure release workflow covers it).

- **[Tooling]** WebUI debug logging toggle (v3.3.x)
  - Add a build-time flag (e.g. `VITE_CM_DEBUG`) and route noisy logs through a `dbg(...)` helper.

  - Create script that checks component on/off flags
  - Ensure compilation succeeds for all flag combinations
  - Integrate into test engine for comprehensive validation

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

### Ideas / Suggestions

- **[IDEA]** what about Matter - the new standard for smart home devices?
  - maybe add a Matter component to the library in the future?
- **[IDEA]** what about bacNet? send data to a bacNet server like mqtt?


### Done / Resolved (but not tested yet)

- **[COMPLETED][Bug/Design]** Uptime shows always seconds -> for mat it to human readable format (days, hours, minutes, seconds)
  - days is not tetsed yet
- Library does not include the docs folder.
- **[COMPLETED][Bug]** check restart behavior, sometimes the device restarts multiple times if wifi also good

### Done / Resolved

- **[COMPLETED][Bug]** Settings password prompt when password is unset
- **[COMPLETED][Bug]** Browser tab title is configurable
  - H1 uses `.setAppName(APP_NAME)`
  - Browser tab uses `.setAppTitle("...")`
  - If `.setVersion(VERSION)` is set: it is appended to both
- **[COMPLETED][Bug/Design]** "WifiConnected" in system card --> "Wifi Connected", and position at first place
- **[COMPLETED][Bug/Design]** Live-view cards are not sorted by `order`
- Add divider (hr-like) in full demo.
