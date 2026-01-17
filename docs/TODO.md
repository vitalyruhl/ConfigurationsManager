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
    - Test target: use `IO-Full-Demo` for steps 1–5; switch to `SolarInverterLimiter` at step 6.
    1) IOManager module (buttons + digital IO + analog IO)
       - Goal: a single, general IO abstraction that auto-registers its settings into a dedicated category/card.
       - Category token: use a stable constant `cm::CoreCategories::IO` with value `"IO"`.
         - GUI label must be `"I/O"` (display only); do not bake `"I/O"` into the persisted category token.
       - Prefer stable IDs: each IO item/provider uses an `id` that is also used for settings key-prefixing (e.g. `io.<id>.*`) and runtime keys.
       - Buttons
         - API idea: `io.addButton({ .id=..., .name=..., .pin=..., ... })`
         - Settings: GPIO pin, active-low/high, pull-up/down., longpress in ms
         - Callbacks: pressed, double-pressed, long-pressed.
       - Digital outputs (relay-style)
         - API idea: `io.addDigitalOutput({ .id=..., .name=..., .pin=..., ... })`
         - Settings: GPIO pin, active-low/high, enabled.
         - Runtime API: `set(bool)` / `get()`.
       - Analog inputs
         - API idea: `io.addAnalogInput({ .id=..., .name=..., .pin=..., ... }).onChange(0.01f, callback)`
         - Settings: GPIO pin, read-rate (ms), deadband/sensitivity, unit.
       - Analog outputs
         - API idea: `io.addAnalogOutput({ .id=..., .name=..., .pin=..., ... }).setValue(1.5f)`
         - Settings: GPIO pin, extended-range flag.
       - Capability differences (important)
         - Not all boards support full ADC resolution, not all support PWM, and not all support both input and output.
         - Design should be provider-based (like runtime providers): users add only what the board supports, e.g. `.addAnalogInputProvider(...)`, `.addPwmOutputProvider(...)`, `.addDacOutputProvider(...)`.
       - Range/scaling model (baseline)
         - `raw.*` defaults MUST come from the selected provider / board capabilities (no hardcoded library defaults).
           - Example only: ESP32 ADC raw is typically `0..4095` (12-bit), but this can differ by board/config.
         - Default behavior: full raw range is mapped into the output range.
           - If the user does not override `out.*`, default `out.min/out.max` should represent a pass-through of the full raw range (i.e. identity scaling, so raw==out in meaning).
         - User override example: a potentiometer should show `0..100%`.
           - User sets `out.min = 0`, `out.max = 100` in code.
           - This must automatically enable/register the corresponding settings so the user can fine-tune in the GUI.
         - GUI activation rule
           - If a channel provides explicit `out.*` overrides in code (or sets an explicit `enableScaling` flag), IOManager registers `out.min/out.max` (and optionally `unit`, `sensitivity`) as editable settings under category `IO`.
           - If not overridden, keep those settings hidden/disabled by default to avoid UI clutter.
         - Unit example: `"°C"` (engineering units).
         - Sensitivity example: `0.01` relative to the output range (e.g. for 0..150°C, step is 0.01 in that out-range).
       - Key length caution (ESP32 Preferences)
         - ID-based key prefixing must respect the 15 character key-name limit (including category prefixing).
         - IDs therefore must be short, and generated keys must be validated via `checkSettingsForErrors()`.
       - add  gui-helper eg. addIOtoGUI("id", "card-name", order)
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
    6) Update other examples to use the new core settings/modules where applicable
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
- **[COMPLETED][TESTED]** Core settings templates / injection (WiFi/System/Buttons + optional NTP)
  - Implemented core settings templates (WiFi/System/Buttons + optional NTP) and a dedicated core demo example.
  - Validated via PlatformIO build and flashed to ESP32.
  - Documented category merge/injection behavior in `docs/SETTINGS.md` and added `cm::CoreCategories::*` constants to avoid typos.

- **[COMPLETED][TESTED]** Custom runtime provider named `system` overrides built-in System provider
  - Fixed by merging runtime providers that share the same group name ("system" etc.) instead of overwriting.
  - Validated by injecting `system.testValue` into the System card (with a divider).