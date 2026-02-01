# TODO / Roadmap / Issues



## High Priority (Prio 1)

### refactoring / renames of IOManager methods

Proposed API vNext (nach deinen weiteren Vorgaben)

Grundsatz:
- `addDigital*/addAnalog*` = IO-Objekt anlegen (Programmzugriff)
- `add*ToSettings...` = nur Settings-Tab (nur wenn `persistSettings=true`)
- `add*ToLiveRT...` = nur Live/Runtime (fuer alle IOs, egal ob persistiert oder nicht)

### 1) Digital IO anlegen (keine JSON-Structs, sondern Parameterliste)

Warum: In der IDE sind Parameter sofort sichtbar (Hover/Signature Help), weniger "init noise".

Hinweis: `.defaultEnabled` kann entfallen (implizit immer enabled, oder enabled wird als eigener Settings-Schalter modelliert).

```cpp
// Digital Input
void addDigitalInput(const char* id,
                     const char* name,
                     int gpioPin,
                     bool activeLow,
                     bool pullup,
                     bool pulldown,
                     bool persistSettings);

// Digital Output
void addDigitalOutput(const char* id,
                      const char* name,
                      int gpioPin,
                      bool activeLow,
                      bool persistSettings);
```

Analog (skizze):
```cpp
void addAnalogInput(const char* id, const char* name, int adcPin, bool persistSettings /* + scaling stuff */);
void addAnalogOutput(const char* id, const char* name, int pinOrChannel, bool persistSettings /* + range stuff */);
```

### 2) Settings-Tab umbauen: Pages + Groups statt Category

Ziel: `settingsCategory` raus, stattdessen:
- `addSettingsPage(pageName, order)` => erzeugt "Settings Page"
- `addSettingsGroup(groupName, order)` => erzeugt Gruppe innerhalb der Settings Page
- `add*ToSettingsGroup(...)` => haengt IOs und normale Settings-Felder in Gruppen

API-Skizze:
```cpp
void addSettingsPage(const char* pageName, int order);
void addSettingsGroup(const char* groupName, int order, const char* pageName = "IO");

// IO -> Settings group
void addDigitalInputToSettingsGroup(const char* id, const char* groupName, int order);
void addDigitalOutputToSettingsGroup(const char* id, const char* groupName, int order);
void addAnalogInputToSettingsGroup(const char* id, const char* groupName, int order);
void addAnalogOutputToSettingsGroup(const char* id, const char* groupName, int order);

// Generic Settings-field -> Settings group (by key)
void addSettingKeyToSettingsGroup(const char* settingKey, const char* groupName, int order);
```

Erwartetes Verhalten:
- Nur persistierte IOs duerfen in Settings landen (`persistSettings=true`), sonst Warnung/Fehler.
- Entscheidung (siehe UI Screenshot): Settings-Struktur ist **Page (Tab)** -> **Card** -> **Group** -> **Fields**.
  - Page = oberer Tab (z.B. "MQTT-Topics")
  - Card = grosse Box mit Titel (z.B. "MQTT-Topics")
  - Group = Abschnitt innerhalb der Card (z.B. "Grid Import Topic", "Solar power Topic")

Beispiel (Door Sensor + zusaetzliches Config-Feld):
```cpp
ioManager.addDigitalInput("doorinputsensor", "Door Sensor", 34, false, true, false, true);

Config<int> warnDelay(ConfigOptions<int>{
  .key = "Warndelay",
  .name = "Delay before Warning pushes",
  .defaultValue = 42,
});

ioManager.addSettingsPage("Sensors", 1);
ioManager.addSettingsGroup("Door Sensor", 1, "Sensors");
ioManager.addDigitalInputToSettingsGroup("doorinputsensor", "Door Sensor", 1);
ioManager.addSettingKeyToSettingsGroup("Warndelay", "Door Sensor", 2);
```

### 3) Live/Runtime umbauen: Pages/Tabs + robustes Fallback

Neue Live-Funktionalitaet:
```cpp
void addLivePage(const char* pageName, int order);
```

Defaults:
- `livePage` default: `"Live"`
- `liveCard` default: `"Live Values"`

Robustheit:
- Page-Namen case-insensitive matchen.
- Unbekannte Page (Tippfehler) => fallback auf `"Live"` + Warnung bei verbose log.

### 4) Live/Runtime API: Typ als erster Parameter, Label default aus `.name`

Digital Input Live:
```cpp
void addDigitalInputToLiveRT(RuntimeControlType type,
                             const char* id,
                             int order,
                             const char* livePage = "Live",
                             const char* liveCard = "Live Values",
                             const char* liveLabelOverride = nullptr,
                             int colorOnTrue = rgb(0, 255, 0),       // default green
                             int colorOnFalse = rgb(128, 128, 128)); // default gray
```

Digital Output Live:
```cpp
// Stateful control (Checkbox/StateButton)
void addDigitalOutputToLiveRT(RuntimeControlType type,
                              const char* id,
                              int order,
                              const char* livePage = "Live",
                              const char* liveCard = "Live Values",
                              const char* onLabel = "On",
                              const char* offLabel = "Off",
                              const char* liveLabelOverride = nullptr);

// Momentary action
void addDigitalOutputToLiveRT(RuntimeControlType type,
                              const char* id,
                              int order,
                              std::function<void()> onPress,
                              const char* livePage = "Live",
                              const char* liveCard = "Live Values",
                              const char* liveLabelOverride = nullptr);
```

Analog Input Live:
```cpp
void addAnalogInputToLiveRT(RuntimeControlType type,
                            const char* id,
                            int order,
                            const char* livePage = "Live",
                            const char* liveCard = "Live Values",
                            bool showRaw = false,
                            const char* liveLabelOverride = nullptr);
```

Analog Output Live:
```cpp
void addAnalogOutputToLiveRT(RuntimeControlType type,
                             const char* id,
                             int order,
                             float sliderMin,
                             float sliderMax,
                             float sliderStep,
                             int sliderPrecision,
                             const char* unit = nullptr,
                             const char* livePage = "Live",
                             const char* liveCard = "Live Values",
                             const char* liveLabelOverride = nullptr);

void addAnalogOutputToLiveRT(RuntimeControlType type,
                             const char* id,
                             int order,
                             const char* unit = nullptr,
                             int precision = 2,
                             const char* livePage = "Live",
                             const char* liveCard = "Live Values",
                             const char* liveLabelOverride = nullptr);
```

### Alarms (generisch, getrennt von Live/Settings)

Ziel: Alarme werden nicht mehr "implizit" ueber `add*ToLiveRT(..., alarmWhenActive=...)` konfiguriert,
sondern explizit ueber eine generische `addAlarm(...)`-API. Damit ist Live-UI (Farben) von Alarm-Logik getrennt.

Vorgesehene Overloads:
```cpp
// Digital (bool) alarm: true/false trigger.
// Example: alarm on active input (pressed, door open, etc.).
void addAlarm(const char* id,
              bool whenTrue,
              AlarmCallbacks callbacks = {},
              const char* alarmLabelOverride = nullptr);

// Analog threshold alarm: min/max can be enabled independently.
// Example: temperature < min OR temperature > max.
void addAlarm(const char* id,
              float minValue,
              float maxValue,
              bool minActive,
              bool maxActive,
              AlarmCallbacks callbacks = {},
              const char* alarmLabelOverride = nullptr);
```

Callbacks (skizze):
```cpp
struct AlarmCallbacks {
  std::function<void()> onEnter;
  std::function<void()> onExit;
};
```

### Digital Events (UI + HW) - Builder/Fluent API (preferred)

Entscheidung: Fluent/Builder API (Variante 2) fuer Events/Controls, weil als Programmierer direkter lesbar.


- Wichtig: HW-Events (Input) und UI-Events (Runtime Buttons) sollen die gleiche Callback-API nutzen, aber unterschiedliche Quellen haben.

Vorgeschlagene Event-Callbacks (vereinheitlicht):
- `onPress` / `onRelease`
- `onClick`
- `onLongPress`
- `onReleaseAfterLongPress`
- `onChange(bool state)` (rising+falling mit state), on analog values with threshold
- `onRepeatWhilePressed(uint32_t repeatIndex)` oder `onHoldRepeat(...)` (konfigurierbares Intervall)
- `onLongPressAtStartup` (nur HW Input)
- `onMultiClick(std::function<void(uint8_t clicks)> cb)` statt separate double/triple

Vorschlag MultiClick:
```cpp
.onMultiClick([](uint8_t clicks){
  if (clicks == 2) { doDoubleClick(); }
  else if (clicks == 3) { doTripleClick(); }
});
```

Beispiel: Digital Output (UI Control) als Fluent Builder
```cpp
ioManager.addDigitalOutputToLiveRT(cm::IOManager::RuntimeControlType::Button, "reset", 1)
  .page("Manual Overrides")
  .card("Actions")
  .label("Reset")
  .onClick([](){ doReset(); })
  .onLongPress([](){ doFactoryReset(); });
```

Beispiel: Digital Input (HW Button) Events + optional Runtime Indicator
```cpp
ioManager.addDigitalInput("ap_btn", "AP Mode Button", 13, true, true, false, true);

ioManager.addDigitalInputEvents("ap_btn")
  .longPressMs(1200)
  .multiClickWindowMs(350)
  .repeatWhilePressedMs(250)
  .onClick([](){ showDisplay(); })
  .onLongPressAtStartup([](){ startApMode(); });

ioManager.addDigitalInputToLiveRT(cm::IOManager::RuntimeControlType::Indicator, "ap_btn", 10)
  .page("Live")
  .card("Inputs");
```

Open TODOs:
- Builder-Return-Typ definieren: `DigitalLiveHandle` / `DigitalEventHandle` (muss Referenz/Speicherort stabil sein).
- Sicherstellen, dass `.page(...)` case-insensitive gematcht wird und bei Tippfehlern sauber auf `"Live"` faellt + Warnung.
- Unit/precision/alarm-color styling: Farben gehoeren primär zur Live-UI; Alarm-Logik bleibt ueber `addAlarm(...)` getrennt.

### Gruppierte Beispiele

1) Minimal-Device: nur persistente Settings, keine Live-Ansicht
```cpp
ioManager.addDigitalInput("reset_btn", "Reset Button", 14, true, true, false, true);
ioManager.addDigitalInput("ap_btn", "AP Mode Button", 13, true, true, false, true);

ioManager.addSettingsPage("IO", 1);
ioManager.addSettingsGroup("Buttons", 1, "IO");
ioManager.addDigitalInputToSettingsGroup("reset_btn", "Buttons", 10);
ioManager.addDigitalInputToSettingsGroup("ap_btn", "Buttons", 11);
```

2) Live: Manual Overrides + System Info
```cpp
ioManager.addLivePage("Live", 0);
ioManager.addLivePage("Manual Overrides", 1);
ioManager.addLivePage("System Info", 2);
```

3) Typo-Fallback: Page falsch geschrieben => landet in Default "Live" + Warnung
```cpp
ioManager.addAnalogOutput("dac1", "DAC Output", /*pin*/25, /*persistSettings*/true);
ioManager.addAnalogOutputToLiveRT(cm::IOManager::RuntimeControlType::Slider,
                                 "dac1", 1,
                                 0.0f, 100.0f, 1.0f, 0, "%",
                                 /*livePage*/"outputs", /*liveCard*/"Out-Card"); // typo => fallback
```

### refactoring / renames of MQTT-Manager methods

Ziel: MQTTManager-API analog zum IOManager aufraeumen:
1) Receive/Publish "Definition" (programmatisch)
2) Settings-UI Gruppierung ueber Settings Pages/Groups (keine Category Strings in jedem Call)
3) Optional: Live/Runtime Anzeige klar getrennt und ohne Namens-Dopplung

#### 1) Receive/Mapping Definition (programmatisch)

Beispiel (wie bisher, aber ohne "addToSettings" Bool im Call):
```cpp
mqtt.addTopicReceiveInt(
  /*id*/ "grid_import_w",
  /*name*/ "Grid Import",
  /*topic*/ "tele/powerMeter/powerMeter/SENSOR",
  /*target*/ &currentGridImportW,
  /*unit*/ "W",
  /*jsonPath*/ "E320.Power_in");

mqtt.addTopicReceiveInt(
  /*id*/ "solar_power_w",
  /*name*/ "Solar power",
  /*topic*/ "tele/tasmota_1DEE45/SENSOR",
  /*target*/ &solarPowerW,
  /*unit*/ "W",
  /*jsonPath*/ "ENERGY.Power");
```

Vorgesehene Overloads/Typen:
```cpp
void addTopicReceiveInt(const char* id, const char* name, const char* topic, int* target, const char* unit = nullptr, const char* jsonPath = nullptr);
void addTopicReceiveFloat(const char* id, const char* name, const char* topic, float* target, const char* unit = nullptr, const char* jsonPath = nullptr, int precision = 2);
void addTopicReceiveBool(const char* id, const char* name, const char* topic, bool* target, const char* jsonPath = nullptr);
void addTopicReceiveString(const char* id, const char* name, const char* topic, String* target, const char* jsonPath = nullptr);
```

#### 2) Settings: Gruppierung ueber Settings Pages/Groups

Ziel: MQTT-Receive/Publish Settings sollen in Settings-Gruppen landen, analog zu IO:
```cpp
mqtt.addSettingsPage("MQTT", 2);
mqtt.addSettingsGroup("Gridimport Topic", 1, "MQTT");
mqtt.addTopicReceiveToSettingsGroup("grid_import_w", "Gridimport Topic", 1);

mqtt.addSettingsGroup("Solar Topic", 2, "MQTT");
mqtt.addTopicReceiveToSettingsGroup("solar_power_w", "Solar Topic", 1);
```

Vorgesehene Methoden:
```cpp
void addTopicReceiveToSettingsGroup(const char* id, const char* groupName, int order);
void addTopicPublishToSettingsGroup(const char* id, const char* groupName, int order);
```

#### 3) Live/Runtime (optional)

Option A: Keine Live-Registrierung im MQTTManager (Projects machen das selbst via RuntimeProvider).
Option B: Explizite Live-Helpers, aber getrennt:
```cpp
void addTopicReceiveToLiveRT(const char* id,
                             int order,
                             const char* livePage = "Live",
                             const char* liveCard = "MQTT",
                             const char* liveLabelOverride = nullptr);
```

Offene Punkte:
- Soll MQTTManager eigene SettingsPage/Group Methoden haben oder die von ConfigManager direkt nutzen?
- Wie verhindern wir doppelte Namensquellen (Topic-Name vs RuntimeLabel)? -> Default immer der Topic-Name aus addTopicReceive*.

### refactoring / Builder API for Settings (ConfigOptions replacement)

Ziel:
- Weg von `ConfigOptions<T>{ ... }` / Struct-Init (viel Boilerplate, Hover zwar ok aber schwer "Placement" zu trennen)
- Hin zu einer Builder-API `ConfigManager.settingX(...)...build()`
- "Placement" (Settings Page/Card/Group + Live Page/Card/Group) erfolgt separat ueber `addToSettings*` / `addToLiveRT*`.
- Keine Backward-Kompatibilitaet noetig (noch nicht released) -> Beispiele/Module danach komplett umstellen.

Wichtig: Settings-Strukturen (wie `TempSettings`, `WiFiSettings`, ...) bleiben weiterhin moeglich.
Die Struktur ist nur noch eine Sammlung von `Config<T>` (oder Referenzen/Pointer darauf), aber die GUI-Zuordnung passiert in einer separaten `attachToConfigManager()/registerGuiLayout()` Methode.

Beispiel: einzelnes Setting mit minimalen Metadaten
```cpp
auto& warnDelay = ConfigManager.settingInt("Warndelay")
  .name("Delay before Warning pushes")
  .defaultValue(42)
  .build();
```

Beispiel: Placement in Settings + Live
```cpp
ConfigManager.addSettingsPage("IO", 1);
ConfigManager.addSettingsGroup("Door Sensor", 1, "IO");
ConfigManager.addToSettingsGroup("Warndelay", "Door Sensor", 2);

ConfigManager.addLivePage("Live", 0);
ConfigManager.addToLiveRTGroup("doorinputsensor", "Door Sensor", 1);
```

Beispiel: Settings-Struct wie heute (TempSettings), aber mit Builder + separatem Layout
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
    tempCorrection = &ConfigManager.settingFloat("TCO")
      .name("Temperature Correction")
      .defaultValue(0.1f)
      .build();

    humidityCorrection = &ConfigManager.settingFloat("HYO")
      .name("Humidity Correction")
      .defaultValue(0.1f)
      .build();

    seaLevelPressure = &ConfigManager.settingInt("SLP")
      .name("Sea Level Pressure (hPa)")
      .defaultValue(1013)
      .build();

    readIntervalSec = &ConfigManager.settingInt("ReadTemp")
      .name("Read Temp/Humidity every (s)")
      .defaultValue(30)
      .build();

    dewpointRiskWindow = &ConfigManager.settingFloat("DPWin")
      .name("Dewpoint Risk Window (C)")
      .defaultValue(1.5f)
      .build();
  }

  void placeInUi()
  {
    ConfigManager.addSettingsPage("Temp", 1);
    ConfigManager.addSettingsGroup("BME280", 1, "Temp");

    ConfigManager.addToSettingsGroup("TCO", "BME280", 1);
    ConfigManager.addToSettingsGroup("HYO", "BME280", 2);
    ConfigManager.addToSettingsGroup("SLP", "BME280", 3);
    ConfigManager.addToSettingsGroup("ReadTemp", "BME280", 4);
    ConfigManager.addToSettingsGroup("DPWin", "BME280", 5);
  }
};
```

- Key hashing: wenn intern `keyName_hash` genutzt wird, 
  - unbedingt Alias-Mapping fuer den "human_key"(der name = human_key = quelle für hash) (sonst brechen REST/MQTT/Backups/Prefs).


### other high priority tasks

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
