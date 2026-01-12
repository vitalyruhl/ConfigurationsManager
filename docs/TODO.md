# TODO / Errors / Ideas

## Current Focus

### v3 stabilization & roadmap

- As of: 2026-01-09
- UI/UX (Settings & Runtime)
  - GUI display mode toggle: “Current” (cards) vs “Categories” (map/list view).
  - `order` / sorting: metadata for live cards and settings categories (stable ordering).
  - Analog: separate input (NumberInput) vs slider; adjust slider current-value styling.
- Architecture / API
  - Library does not include the docs folder.
  - Settings auth: remove or hard-disable legacy endpoint `/config/settings_password`.
  - Unified JSON handlers: route all POST/PUT config endpoints through robust JSON parsing (no manual body accumulation).
  - Settings schema versioning/migration (when new fields/structure are introduced).
- Test & CI
  - Minimal: unit/integration test for auth + password reveal + bulk save/apply.
  - PlatformIO CI: build matrix for at least one ESP32 env + WebUI build step.

### High Priority Bugs (Prio 1)

- **[Bug]** Settings password prompt when password is unset
- **[Bug]** Live-view cards are not sorted by `order`
- **[Bug/Design]** Custom runtime provider named `system` overrides built-in System provider
  - Symptom: System card shows `—` for default fields after adding a custom provider with group `system`.
  - Expected: allow adding extra fields without losing default System fields.
  - Options: provider chaining/merging for identical groups, or a dedicated API to extend the built-in System provider.
  - check if it possable to add/append to the system provider instead of overwriting it. (```.appendValue("system", [](JsonObject &data){data["testValue"] = 42;}, 99);``` or similar)
- **[Bug]** Browser tab title is not configurable
  - Currently always "ESP32 Configuration"; should be configurable via `.setAppName(APP_NAME)` or a dedicated `.setTitle`.
  - If `cfg.setSettingsPassword(SETTINGS_PASSWORD);` is not set, no password should be requested.
- **[Tooling]** VS Code include error for `#include <BME280_I2C.h>`

### Medium Priority Bugs/Features (Prio 5)

- **[FEATURE]** Failover Wifi native support

### Low Priority Bugs/Features (Prio 10)

- **[FEATURE]** Card layout/grid improvement
  - If there are more cards, the card layout breaks under the longest card above; make the grid more flexible.
- consolidate the Version-History before v3.0.0 into less detailed summary
- **[FEATURE]** v3 follow-ups
  - Add divider (hr-like) in full demo.
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

  - Create script that checks component on/off flags
  - Ensure compilation succeeds for all flag combinations
  - Integrate into test engine for comprehensive validation

- **[FEATURE] add HTTPS support, because its not in core ESP32 WiFi lib yet.** (Prio: not yet, wait for updates)

### Ideas / Suggestions

- **[IDEA]** what about Matter - the new standard for smart home devices?
  - maybe add a Matter component to the library in the future?
- **[IDEA]** what about bacNet? send data to a bacNet server like mqtt?
