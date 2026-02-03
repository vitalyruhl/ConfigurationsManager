# TODO / Roadmap / Refactor Notes (Planning)

This file collects the intended refactor direction for the next API version.
Goal: clarify naming, reduce method explosion, keep parameter order consistent, and separate:
Goal: keep user programs minimal while offering maximal flexibility; defaults should be applied automatically and only overridden when explicitly requested.

1) Definition/registration of an IO/Setting (programmable object)
2) Persistence (NVS) (optional)
3) Placement in Settings UI (optional, only for persisted items)
4) Placement in Live/Runtime UI (optional, for any item)


## Terminology (as in the UI)

- Top buttons: Live / Settings / Flash / Theme (always visible)
- Settings:
  - SettingsPage = tab in Settings (e.g. "MQTT-Topics")
  - SettingsCard = big card container with title one page can have multiple cards
  - SettingsGroup = group box inside a card (contains fields; shown stacked vertically)
- Live/Runtime:
  - LivePage = top-level page selector under "Live" (similar to Settings tabs)
  - LiveCard = card container inside a live page
  - LiveGroup = if we want a second nesting level like Settings

Decision preference (based on screenshot):
- Keep SettingsCard + SettingsGroup (do not merge them), but provide strong defaults so typical usage is short:
  - default SettingsCard = SettingsPage name
  - SettingsGroup is the thing you actively name (e.g. "Grid Import Topic", "Solar power Topic")


## Guiding Principles (API)

- Consistent naming:
  - define/register = create the object (IO / Setting / Topic)
  - addToSettings* = UI placement in Settings
  - addToLive* = UI placement in Live/Runtime
- Consistent parameter order everywhere:
  - (type if needed) -> id/key -> order -> page -> card -> group -> label override -> misc
- Avoid dozens of overloads by returning small builder/handle objects for:
  - Live control events (onClick, onChange, multi-click, long press, ...)
  - Alarms (generic addAlarm() API instead of hidden bool flags)
- No backwards compatibility required (not released yet) => all examples will be migrated in the refactor branch.


## High Priority (Prio 1) - Proposed API vNext (Draft)

### A) Core: UI layout registry (Settings + Live)

These are "layout only" utilities. They should not create IOs/settings; only define where UI elements live.

```cpp
// Settings layout
void addSettingsPage(const char* pageName, int order);
void addSettingsCard(const char* pageName, const char* cardName, int order);
void addSettingsGroup(const char* pageName, const char* cardName, const char* groupName, int order);

// Live layout
void addLivePage(const char* pageName, int order);
void addLiveCard(const char* pageName, const char* cardName, int order);
void addLiveGroup(const char* pageName, const char* cardName, const char* groupName, int order);
```

Defaults / robustness:
- Case-insensitive page/card/group matching.
- Unknown names fall back to defaults and emit a warning once.
- Default SettingsCard = pageName (if card not specified).
- Default LiveCard = "Live Values" (or pageName).
- Each `ConfigManager.addSetting()` call now mirrors the associated category/card into the layout registry automatically, so examples only need explicit `addSettings*`/`addToSettingsGroup()` calls when deviating from the default grouping or order.

### B) Settings builder API (ConfigOptions replacement)

Target:
- Replace `ConfigOptions<T>{...}` boilerplate with a fluent builder that keeps IDE hover/signature help useful.
- Placement in UI happens separately (via `addToSettings...` and `addToLive...`).
- Internally we can still store `key_hashed` for NVS; but we must keep a stable "human key" alias.

```cpp
// Creation/build (no UI placement here)
auto& warnDelay = ConfigManager.addSettingInt("Warndelay")
  .name("Delay before Warning pushes")
  .defaultValue(42)
  .persist(true)     // aka: store in NVS; final name: persistSettings
  .build();
```

Placement (Settings):
```cpp
// Minimal: add to a SettingsGroup (page/card defaulted)
ConfigManager.addToSettingsGroup("Warndelay", /*page*/ "MQTT-Topics", /*group*/ "Grid Import Topic", /*order*/ 1);

// Full: explicit page/card/group
ConfigManager.addToSettingsGroup("Warndelay", "MQTT-Topics", "MQTT-Topics", "Grid Import Topic", 1);
```

Placement (Live/Runtime):
```cpp
ConfigManager.addToLiveGroup("Warndelay", /*page*/ "Live", /*group*/ "Door Sensor", /*order*/ 1);
```

Expected overloads (Settings placement):
```cpp
// Shorthand: add directly to the default card/group for that page (implementation defines what "default" means)
void addToSettings(const char* settingKey, const char* pageName, int order);

void addToSettingsGroup(const char* settingKey, const char* pageName, const char* groupName, int order);
void addToSettingsGroup(const char* settingKey, const char* pageName, const char* cardName, const char* groupName, int order);
```

Expected overloads (Live placement):
```cpp
// Shorthand: add directly to the default card/group for that page
void addToLive(const char* itemIdOrKey, const char* pageName, int order);

void addToLiveGroup(const char* itemIdOrKey, const char* pageName, const char* groupName, int order);
void addToLiveGroup(const char* itemIdOrKey, const char* pageName, const char* cardName, const char* groupName, int order);
```

Settings structs (TempSettings/WiFiSettings) remain possible:
- The struct becomes a collection of `Config<T>*` (or references), with a split:
  - `create()` builds settings
  - `placeInUi()` attaches to Settings/Live layout

Example (TempSettings):
```cpp
struct TempSettings
{
  Config<float>* tempCorrection = nullptr;
  Config<float>* humidityCorrection = nullptr;
  Config<int>* seaLevelPressure = nullptr;
  Config<int>* readIntervalSec = nullptr;
  Config<float>* dewpointRiskWindow = nullptr;

  void create()
  {
    tempCorrection = &ConfigManager.addSettingFloat("TCO")
      .name("Temperature Correction")
      .defaultValue(0.1f)
      .persist(true)
      .build();

    // ...
  }

  void placeInUi()
  {
    ConfigManager.addSettingsPage("Temp", 1);
    ConfigManager.addSettingsGroup("Temp", "Temp", "BME280", 1);

    ConfigManager.addToSettingsGroup("TCO", "Temp", "Temp", "BME280", 1);
    // ...
  }
};
```

Resolved decisions:
- `.persist(true)` defaults to `true` since persistence is the typical mode for settings; the fluent builder exposes `.persist(false)` for temporary configuration values.
- The hashed key stays internally constant (8 characters) and is derived from the sanitized `.name(...)` (drop spaces/special chars, lowercase, truncate) so REST/MQTT/backups continue to rely on a stable identifier while the human-readable key remains unchanged.


### C) IOManager: define/register IO objects (no JSON/struct init)

Preference: parameter list (so IDE signature help shows names).
Name decision: keep `addDigitalInput` (current mental model) or rename to `defineDigitalInput`.

Digital IO definition:
```cpp
// persistSettings = store pin/flags in NVS and allow Settings UI placement
void addDigitalInput(const char* id,
                     const char* name,
                     int gpioPin,
                     bool activeLow,
                     bool pullup,
                     bool pulldown,
                     bool persistSettings);

void addDigitalOutput(const char* id,
                      const char* name,
                      int gpioPin,
                      bool activeLow,
                      bool persistSettings);
```

Analog IO definition (draft; exact scaling/range TBD):
```cpp
void addAnalogInput(const char* id,
                    const char* name,
                    int adcPin,
                    bool persistSettings
                    /* + calibration/scaling */);

void addAnalogOutput(const char* id,
                     const char* name,
                     int dacOrPwmChannelOrPin,
                     bool persistSettings
                     /* + range */);
```

Decision:
- Remove `.defaultEnabled`. If we need enable/disable, model it as a separate persisted setting and/or runtime toggle.


### D) IOManager: add IOs to Settings UI (only for persisted IOs)

Replace `settingsCategory` with the Settings layout (page/card/group).

Minimal placement:
```cpp
// Input/Output placement - only allowed if persistSettings=true, otherwise warning/error.
void addDigitalInputToSettings(const char* id, const char* pageName, int order);
void addDigitalInputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
void addDigitalOutputToSettings(const char* id, const char* pageName, int order);
void addDigitalOutputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
void addAnalogInputToSettings(const char* id, const char* pageName, int order);
void addAnalogInputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
void addAnalogOutputToSettings(const char* id, const char* pageName, int order);
void addAnalogOutputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
```

Full placement (explicit card):
```cpp
void addDigitalInputToSettings(const char* id, const char* pageName, const char* cardName, int order);
void addDigitalInputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);
void addDigitalOutputToSettings(const char* id, const char* pageName, const char* cardName, int order);
void addDigitalOutputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);
void addAnalogInputToSettings(const char* id, const char* pageName, const char* cardName, int order);
void addAnalogInputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);
void addAnalogOutputToSettings(const char* id, const char* pageName, const char* cardName, int order);
void addAnalogOutputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);
```


### E) IOManager: add IOs to Live/Runtime UI (for any IO)

Remove hidden/ambiguous flags (e.g. alarmWhenActive). Alarms become a separate API.

Common enums:
```cpp
enum class RuntimeControlType
{
  ValueField,
  Slider,
  Button,
  StateButton,
  Checkbox
};
```

Digital input to Live (draft signature):
```cpp
// label default: use IO's registered name; override optional
auto addDigitalInputToLive(RuntimeControlType type,
                             const char* id,
                             int order,
                             const char* livePage = "Live",
                             const char* liveCard = "Live Values",
                             const char* liveGroup = nullptr,
                             const char* liveLabelOverride = nullptr,
                             int colorOnTrue = rgb(0, 255, 0),        // default green
                             int colorOnFalse = rgb(128, 128, 128));  // default gray
```

Digital output to Live:
```cpp
auto addDigitalOutputToLive(RuntimeControlType type,
                              const char* id,
                              int order,
                              const char* livePage = "Live",
                              const char* liveCard = "Live Values",
                              const char* liveGroup = nullptr,
                              const char* liveLabelOverride = nullptr,
                              const char* onLabel = "On",
                              const char* offLabel = "Off"
                              int colorOnTrue = rgb(0, 255, 0),        // default green
                              int colorOnFalse = rgb(128, 128, 128));  // default gray
                              );
```

Analog input to Live:
```cpp
auto addAnalogInputToLive(RuntimeControlType type,
                            const char* id,
                            int order,
                            const char* livePage = "Live",
                            const char* liveCard = "Live Values",
                            const char* liveGroup = nullptr,
                            const char* liveLabelOverride = nullptr,
                            const char* unit = nullptr);
```

Analog output to Live:
```cpp
auto addAnalogOutputToLive(RuntimeControlType type,
                             const char* id,
                             int order,
                             float minValue,
                             float maxValue,
                             const char* livePage = "Live",
                             const char* liveCard = "Live Values",
                             const char* liveGroup = nullptr,
                             const char* liveLabelOverride = nullptr,
                             const char* unit = nullptr);
```

Notes:
- All `add*ToLive(...)` should return a small handle/builder so callbacks can be attached without adding more overloads.

- Checkbox and `StateButton` controls get explicit label/color overrides, while boolean `ValueField` renders as a single colored dot without `On/Off` captions. When a slider is requested for a boolean IO, the UI should fall back to the checkbox representation, since both inputs and outputs share the same rendering except that outputs can be toggled. Add a dedicated `PressButton`/momentary control type (an IO that is `true` while pressed and instantly reverts on release) and plan to allow overriding input definitions later (see Medium Priority for follow-up tasks).

Handle API sketch (for documentation, not final names):
```cpp
struct LiveControlHandle
{
  // Digital
  LiveControlHandle& onChange(std::function<void(bool)> cb);
  LiveControlHandle& onClick(std::function<void()> cb);
  LiveControlHandle& onPress(std::function<void()> cb);
  LiveControlHandle& onRelease(std::function<void()> cb);
  LiveControlHandle& onLongPress(std::function<void()> cb);
  LiveControlHandle& onReleaseAfterLongPress(std::function<void()> cb);
  LiveControlHandle& onRepeatWhilePressed(std::function<void(uint32_t)> cb);
  LiveControlHandle& onLongPressAtStartup(std::function<void()> cb);
  LiveControlHandle& onMultiClick(std::function<void(uint8_t)> cb);

  // Analog
  LiveControlHandle& onChange(std::function<void(float)> cb); // add overload with hysteresis?
};
```


### F) Live callbacks/events: fluent builder (preferred)

User preference: builder is more direct/understandable than overload soup.

Example usage:
```cpp
ioManager.addDigitalOutputToLive(RuntimeControlType::Button, "heater", 2, "Manual", "Outputs")
  .onClick([]() { /* ... */ })
  .onPress([]() { /* ... */ })
  .onRelease([]() { /* ... */ });
```

Unified multi-click:
```cpp
ioManager.addDigitalInputToLive(RuntimeControlType::StateButton, "door", 1)
  .onMultiClick([](uint8_t clicks) {
    if (clicks == 2) { /* double click */ }
    if (clicks == 3) { /* triple click */ }
  });
```

Event list (draft; verify completeness):
- onChange(bool newState)
- onClick()
- onPress()
- onRelease()
- onLongPress()
- onReleaseAfterLongPress()
- onRepeatWhilePressed(uint32_t repeatCountOrMs)
- onLongPressAtStartup()
- onMultiClick(uint8_t clicks)

Open questions:
- Debounce and multi-click timing parameters: global defaults vs per-control configuration.
- Should onClick be derived from press/release automatically?
- Is it useful to surface the timing settings (long press, click intervals) in the system settings? If not, we should offer `.setLongPressTiming(...)` and `.setClickTiming(...)` overrides so the defaults are only overwritten when required.

- Decision: IOManager keeps a global timing set, but builders can call `.setLongPressTiming(...)`/`.setClickTiming(...)` if needed (only use overrides when RAM/flash impact is acceptable). Defaults stay in constructors and can be overridden system-wide during setup.
- Decision: `onClick()` is derived internally from `onPress()`/`onRelease()` and handler priority follows `onClick` → `onMultiClick` → `onReleaseAfterLongPress` → `onRepeatWhilePressed` when multiple callbacks exist.


### G) Alarms: generic addAlarm() API (no hidden bools)

Digital alarm example (trigger when active):
```cpp
// If we need a label override, it should default to IO/Setting name.
void addAlarmDigitalActive(const char* idOrKey,
                           const char* alarmId,
                           int order,
                           const char* pageName,
                           const char* cardName,
                           const char* groupName,
                           const char* labelOverride = nullptr);
```

// Instead of mixing UI placement and callbacks, split the APIs: `.addAlarmDigital(...)` only registers the trigger and lets the caller attach callbacks like `onAlarmCome`, `onAlarmGone`, `onAlarmStay` (e.g. shipping periodic callbacks every 10s while active). `addAlarmToLive(...)` becomes the separate path for wiring the alarm into a live page/card/group.

More generic approach (preferred):
```cpp
enum class AlarmKind { DigitalActive, DigitalInactive, AnalogBelow, AnalogAbove, AnalogOutsideWindow };

struct AlarmConfig
{
  const char* sourceIdOrKey;
  AlarmKind kind;
  float thresholdMin;
  float thresholdMax;
  bool minActive;
  bool maxActive;
  const char* labelOverride;
  const char* pageName;
  const char* cardName;
  const char* groupName;
  int order;
};

void addAlarm(const AlarmConfig& cfg);
```

Analog alarm overload (threshold + callbacks):
```cpp
void addAlarmAnalog(const char* idOrKey,
                    AlarmKind kind,
                    float threshold,
                    bool active,
                    std::function<void(bool active)> onStateChanged /* optional */);
```

Open question:
- Should alarms always appear in Live, or also as Settings toggles?
- Plan: Alarms are presented through the Live UI only; Settings toggles for alarms are not part of the current scope.

- Decision: UI toggles are off by default, but callers can add explicit Settings controls (`setMinAlarmActive`, etc.) when needed; analog alarms default to disabled unless explicitly activated, digital alarms default to enabled. Provide getters/setters for each alarm axis to control state programmatically.
- Decision: Internally model alarm status as an enum/byte field (e.g. 0=ok, 1=alarm, 2=low, 3=high) so the UI exposes `State::Alarm` while keeping space-efficient bitflags internally.


### H) MQTTManager: refactor plan (new section/branch)

Goals:
- Separate "topic definition" from "Settings UI placement" and "Live/Runtime placement".
- Keep method names parallel to IOManager and ConfigManager.

Definition (receive):
```cpp
// Define a topic subscription and how to extract value from JSON
void addTopicReceiveInt(const char* id,
                        const char* name,
                        const char* topic,
                        int* target,
                        const char* unit,
                        const char* jsonPath);

void addTopicReceiveFloat(const char* id,
                          const char* name,
                          const char* topic,
                          float* target,
                          const char* unit,
                          const char* jsonPath);
```

Placement in Settings:
```cpp
void addMqttTopicToSettingsGroup(const char* topicId, const char* pageName, const char* groupName, int order);
void addMqttTopicToSettingsGroup(const char* topicId, const char* pageName, const char* cardName, const char* groupName, int order);
```

Placement in Live:
```cpp
void addMqttTopicToLiveGroup(const char* topicId, const char* pageName, const char* groupName, int order);
void addMqttTopicToLiveGroup(const char* topicId, const char* pageName, const char* cardName, const char* groupName, int order);
```

Open questions:
- MQTT settings (host/user/pass/base) belong to ConfigManager settings. Should MQTTManager own page/group creation or only use ConfigManager layout?
- Currently an attach helper creates both the two MQTT buttons and the "MQTT-Topics" page while also injecting all settings. That attach should receive the button name as a parameter (defaulting to "MQTT") so callers can reuse it for secondary connections. The settings themselves must still be created explicitly via the dedicated method (e.g. `addMqttSetting(...)`) and then pushed into the desired group, providing the exact page/card/group target to place them (for example the default MQTT tab or an extra custom tab). Each setting should describe the topic and the value interpretation (none/raw or a JSON path like `E320.Power_in`).
- Settings should not auto-appear; they only show up after the caller places them with the right parameters, which allows pushing MQTT content into either the standard MQTT tab or an extra tab if needed.


### Implementation Sequence (Suggested)

1) [COMPLETED] Finalize UI terminology and defaults (SettingsPage/Card/Group; LivePage/Card/Group).
2) [COMPLETED] Implement layout registries for Settings + Live (no IO changes yet).
2a) [COMPLETED] Ensure the core settings bundles (WiFi/System/NTP) auto-register their standard page/group and allow overrides so sketches avoid repetitive layout boilerplate.
3) Introduce Settings builder (`ConfigManager.settingX(...).name(...).defaultValue(...).persistSettings(true).build()`).
3a) [COMPLETED] Switch ConfigManager settings to hash-derived storage keys (with legacy fallback), keep logs per display name, and emit warnings when duplicate hashes would collide.
4) [COMPLETED] Add generic placement methods for settings: `ConfigManager.addToSettingsGroup(...)`, `ConfigManager.addToLiveGroup(...)`.
5) Refactor IOManager:
   - Split IO definition from UI placement.
   - Implement `add*ToSettingsGroup` (guarded by persistSettings).
   - Implement `add*ToLive` returning a handle/builder.
6) Implement Live callback builder API (digital/analog) + unify multi-click.
7) Implement generic alarm registry (`addAlarm(...)`) + UI for alarms (Live first).
8) Refactor MQTTManager to the same pattern (define -> settings placement -> live placement) and let its attach helpers create the default MQTT tabs/groups (with override hooks) so the layout matches other core bundles.
9) Check all examples + docs:
   - Consistancy + completeness pass.
   - Ensure at the PlatformIO build passes (deploy all, let user test it all)

### Implementation Details (from refactoring-plan)

Workflow notes:
- Rebuild order:
  1. Minimal – must remain translatable
  2. BME280 – must be translatable and deployable
  3. Full-GUI
  4. Full-IO
  5. Full-Logging
  6. Full-MQTT
  7. Boilersaver
  8. SolarInverterLimiter – must be flashable; larger test
- After each step, rebuild and test the examples mentioned above while the remaining examples stay in refactor-only mode for the moment.
- Commit and push an intermediate snapshot after the associated examples pass, and only then start the next step.
- Mark each step as [COMPLETED] in both the Implementation Sequence and Implementation Details when done.

1. [COMPLETED] Terminology & Default Layout: Agree on SettingsPage/Card/Group vs LivePage/Card/Group names, describe fallback rules for unknown entries, and document defaults such as SettingsCard = page name, LiveCard = "Live Values".
2. [COMPLETED] Layout Registries: Build `addSettingsPage/Card/Group` and `addLivePage/Card/Group`, keep case-insensitive lookup, warn once on typos, and store order for rendering without creating any IO/setting data.
3. Fluent Settings Builder: Replace `ConfigOptions<T>` with builder methods (`addSettingInt`, `addSettingFloat`, etc.), compute stable human-readable keys, default `.persist()` to true, and avoid heap allocations in builder objects.
4. Hash-based storage keys: derive the Preferences key from an FNV1a hash of the provided `ConfigOptions::key` (or previous auto-generated key), keep the human-readable name for logs/UI, migrate legacy keys when the hash changes, and warn once if a hash collision would otherwise prevent persistence.
4. [COMPLETED] Placement Helpers for Settings/Live: Implement `addToSettings`, `addToSettingsGroup`, `addToLive`, and `addToLiveGroup`, reuse the layout registries for validation, and ensure settings only surface after builder construction.
5. IOManager Refactor: Define digital/analog IOs through parameter lists with a `persistSettings` flag, drop `settingsCategory`, add registry calls for settings placement, and ensure only persisted items reach the UI while `add*ToLive` returns callback handles.
6. Live Callback Builder & UI Handles: Create `RuntimeControlType`, return handles that configure events like `onChange`, `onClick`, `onMultiClick`, etc., add fallbacks (e.g., slider -> checkbox), and expose timing defaults with optional overrides.
7. Generic Alarm Registry: Provide `addAlarm(AlarmConfig)`/`addAlarmAnalog`, separate trigger definition from UI placement, surface `onAlarmCome/Gone/Stay` hooks, and keep alarms on the Live side unless an explicit Settings toggle is added.
8. MQTTManager Restructure: Define topics via `addTopicReceive*`, add settings/live grouping helpers, and keep layout decisions within `ConfigManager` while letting MQTTManager use its functions for button/page registration.
9. Migrate Examples & Docs: Update each example to the new APIs, refresh WebUI/docs to match the new naming, and verify at least one PlatformIO environment (`examples/Full-GUI-Demo` suggested) builds successfully.

### Example validation plan
- Steps 1‑2 (terminology/layout registries) use the `minimal` example for fast iteration.
- IOManager work (step 5) validates against `examples/Full-IO-Demo` to exercise digital/analog registration.
- Live/GUI step additions (steps 6–7) target `examples/Full-GUI-Demo` so Live and Settings UI remain stable.
- MQTTManager refactor (step 8) uses `examples/Full-MQTT-Demo` to cover topic definition/placement.
- Final migration run can sample whichever example changed most, but keep one PlatformIO build per touched area to catch integration issues.

### Feasibility / Risks (ESP32)

- Builder objects must not allocate heavily on heap; avoid accidental copies.
- `std::function` capture size can be expensive; consider lightweight delegates if needed.
- Multi-click + long press needs a clear state machine and robust debounce.
- Stable external keys vs internal hashed keys must be decided early to avoid breaking REST/MQTT/backups.
- Layout registry plus fluent builder pattern looks feasible in the existing codebase, but we must confirm that the hashed-key generation stays collision-free and consistent, and that the new registries remain performant for long-running firmware.


## Medium Priority (Prio 5)

- check where we kan safetly use  string_view  instead of  String  to reduce memory allocations/copies.
- Card layout/grid improvements
- Alarm helpers (more UI, better formatting)
- Allow overriding digital input definitions through a modular layer so incoming sensors can be remapped or replaced without touching the core registration (see Live Control override note above).


## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)
- WiFi failover
- HTTPS support (wait for ESP32 core)


## Done / Resolved

- Core HelperModule added (PulseOutput + computeDewPoint + mapFloat) and examples migrated.
