# TODO / Errors / Ideas

## Current Focus

### v3 stabilization & roadmap

- As of: 2026-01-09
- UI/UX (Settings & Runtime)

  - `order` / sorting: metadata for live cards and settings categories (stable ordering).
  - Analog: separate input (NumberInput) vs slider; adjust slider current-value styling.
  - [COMPLETED] Runtime: add `MomentaryButton` (press-and-hold) control type (no label heuristics).
- Architecture / API
  - Unified JSON handlers: route all POST/PUT config endpoints through robust JSON parsing (no manual body accumulation).
  - Settings schema versioning/migration (when new fields/structure are introduced).
- Test & CI
  - Minimal: unit/integration test for auth + password reveal + bulk save/apply.
  - PlatformIO CI: build matrix for at least one ESP32 env + WebUI build step.

### High Priority Bugs/Features (Prio 1)

- Goal: keep `ConfigManager` as small as possible in compile size and dependencies
  - Suggested implementation order (one side-branch per item)
    - Test target: use `IO-Full-Demo` for steps 1–4; switch to `SolarInverterLimiter` at step 5.
    1) IOManager module (buttons + digital IO + analog IO)
       - Goal: a single, general IO abstraction that auto-registers its settings into a dedicated category/card.
       - Category token: use a stable constant `cm::CoreCategories::IO` with value `"IO"`.
         - GUI label must be `"I/O"` (display only); do not bake `"I/O"` into the persisted category token.
       - Prefer stable IDs: each IO item/provider uses an `id` that is also used for settings key-prefixing (e.g. `io.<id>.*`) and runtime keys.
       - [COMPLETED] Buttons
         - API idea: `io.addButton({ .id=..., .name=..., .pin=..., ... })`
         - Settings: GPIO pin, active-low/high, pull-up/down., longpress in ms
         - Callbacks: pressed, double-pressed, long-pressed.
       - [COMPLETED] Digital outputs (relay-style)
         - API idea: `io.addDigitalOutput({ .id=..., .name=..., .pin=..., ... })`
         - Settings: GPIO pin, active-low/high, enabled.
         - Runtime API: `set(bool)` / `get()`.
       - [COMPLETED] Analog inputs
         - **[COMPLETED][TESTED]** Analog inputs (raw + scaled mapping, deadband + minEvent)
         - **[COMPLETED][TESTED]** Runtime UI: raw/scaled values can be shown on different cards/groups
         - **[COMPLETED][TESTED]** Alarms: optional min/max thresholds (min-only/max-only/both) with callbacks + runtime indicators
       - Analog outputs
         - API idea: `io.addAnalogOutput({ .id=..., .name=..., .pin=..., ... }).setValue(1.5f)`
         - Settings: GPIO pin, extended-range flag.
       - Capability differences (important)
         - Not all boards support full ADC resolution, not all support PWM, and not all support both input and output.
         - Design should be provider-based (like runtime providers): users add only what the board supports, e.g. `.addAnalogInputProvider(...)`, `.addPwmOutputProvider(...)`, `.addDacOutputProvider(...)`.
       - Key length caution (ESP32 Preferences)
         - ID-based key prefixing must respect the 15 character key-name limit (including category prefixing).
         - IDs therefore must be short, and generated keys must be validated via `checkSettingsForErrors()`.
       - add  gui-helper eg. addIOtoGUI("id", "card-name", order)

       - [COMPLETED] Docs: IO inputs/outputs
         - docs/IO-DigitalInputs.md
         - docs/IO-DigitalOutputs.md
         - docs/IO-AnalogInputs.md
    2) [TODO] Remove Settings List view; keep Tabs only.
    3) MQTT manager module (+ baseline settings + ordering/injection)
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
    4) Logging (lightweight core + optional advanced module)
       - Replace the default logger in the core library with a lightweight implementation (do not require `SigmaLogger` in the default build).
       - Add an optional, separately importable advanced logging module (e.g. `#include "logging/AdvancedLogger.h"`).
         - Provide a dedicated class that encapsulates the display logging logic (queue/buffer, update loop) and allows different display backends.
         - Can internally use `SigmaLogger`, but only inside the optional module (core must not depend on it).
         - Outputs: Serial and one or more display outputs (and optionally MQTT as an extra sink).
    5) Refactor `SolarInverterLimiter` example to consume modules
       - Keep `Smoother` and RS232/RS485 parts inside the example for now; only extract reusable parts (logging, MQTT manager, relay manager, helpers).
    6) Documentation for all modules
       - Add docs for the new modules (how to include, minimal example, dependencies, memory/flash impact).
    7) Update other examples to use the new core settings/modules where applicable

### Medium Priority Bugs/Features (Prio 5)

- **[FEATURE]** Add separated Alarm handling for Analog-Inputs and shorthands for creation of Alarms
  - e.g. ioManager.addAnalogInputMaxAlarm("id", "name", "GUI-Card", maxValue, callback, ...);
- **[FEATURE]** Bybass an Error/Info into live-view form code (toast or similar)
- **[FEATURE]** Bybass an Error/Info into live-view form code (as a overlay between buttons and cards - allways visible until deactivated from code)
- consolidate the Version-History before v3.0.0 into less detailed summary
- **[FEATURE]** v3 follow-ups
  - Extract modules that can be imported separately:
    - Logger: split into 3 extras (serial, MQTT, display)
      - Check logger or GUI extra tab.
      - Display logger: ensure buffer is cleared even when nothing is sent to the display.
      - Check where the latest version is (solarinverter project or boilerSaver).
    - MQTT manager (got it from solarinverter project = latest version)

### Low Priority Bugs/Features (Prio 10)

- **[FEATURE]** Card layout/grid improvement
  - If there are more cards, the card layout breaks under the longest card above; make the grid more flexible.
- **[FEATURE]** Automated component testing
- **[FEATURE]** add templates for common Settings, like
  - [COMPLETED] WiFi,
  - [COMPLETED] System,
  - [Todo] non blocking blinking LED, etc.
- **[FEATURE]** Failover Wifi native support
- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

### Ideas / Suggestions

- **[IDEA]** what about Matter - the new standard for smart home devices?
  - maybe add a Matter component to the library in the future?
- **[IDEA]** what about bacNet? send data to a bacNet server like mqtt?


### Done / Resolved (but not tested yet)

- **[Tooling]** WebUI debug logging toggle (v3.3.x)
  - Add a build-time flag (e.g. `VITE_CM_DEBUG`) and route noisy logs through a `dbg(...)` helper.
- **[COMPLETED][Bug/Design]** Uptime shows always seconds -> for mat it to human readable format (days, hours, minutes, seconds)
  - days is not tetsed yet
- Library does not include the docs folder.
- **[COMPLETED][Bug]** check restart behavior, sometimes the device restarts multiple times if wifi also good

- **[Tooling/Design]** Revisit "advanced opt-in" WebUI rebuild (v3.3.x)
  - Goal: library consumption must NOT require Node.js/npm by default.
  - Idea: gate WebUI rebuild + `src/html_content.h` regeneration behind a flag (e.g. `CM_WEBUI_REBUILD`).
  - Decide whether `webui/dist/*` should stay tracked or be generated-only (and ensure release workflow covers it).

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

- **[COMPLETED][TESTED]** IOManager Digital inputs: GPIO + polarity + pull-up/pull-down + runtime bool-dot
- **[COMPLETED][TESTED]** IOManager Digital inputs: non-blocking events (press/release/click/double/long)
- **[COMPLETED][TESTED]** IOManager Digital inputs: startup-only long press (`onLongPressOnStartup`, 10s window)

- **[COMPLETED][TESTED]** IOManager Analog inputs: raw/scaled mapping + deadband/minEvent + runtime multi-card registration
- **[COMPLETED][TESTED]** IOManager Analog alarms: dynamic min/max thresholds via settings + separate min/max callbacks + runtime flags (`<id>_alarm_min/_alarm_max`)
- **[COMPLETED][TESTED]** WebUI Runtime: numeric alarm highlighting prefers runtime alarm flags when present; WS frames validated + polling fallback prevents stuck UI