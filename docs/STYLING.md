# Runtime Field Styling

Two complementary styling systems exist:

1. Dynamic Style Metadata (per-field JSON rules)
2. Global Override CSS (served as /user_theme.css)

You can combine them or disable metadata and rely purely on your own CSS.

## 1. Dynamic Style Metadata

Each runtime field may carry a `style` object inside `/runtime_meta.json`. That object groups rules keyed by logical targets:

Targets (examples):

- label
- values (numeric/text value span)
- unit (unit column)
- state (boolean text label)
- stateDotOnTrue / stateDotOnFalse / stateDotOnAlarm (boolean indicator dot)

A rule holds arbitrary CSS property/value pairs and optional visibility flag.

### Backend API

```cpp
auto s = ConfigManagerClass::defaultBoolStyle(/*alarmWhenTrue=*/true);
s.rule("stateDotOnAlarm")
 .set("background", "#f1c40f")
 .set("border", "none")
 .set("boxShadow", "0 0 4px rgba(241,196,15,0.7)");

cfg.defineRuntimeBool("alarms", "dewpoint_risk", "Dewpoint Risk", true, 100, s);
```

If a rule is missing it is created the first time you call rule("target").

Hide an element:

```cpp
s.rule("state").setVisible(false);
```

Runtime metadata now ships optional **style overrides** so you can tweak how individual values appear in the web UI without touching the Vue code. Each field carries a `RuntimeFieldStyle` structure that maps logical targets (`label`, `values`, `unit`, `state`, `stateDotOnTrue`, `stateDotOnFalse`, `stateDotOnAlarm`, …) to CSS property/value pairs plus an optional `visible` flag. The backend merges your overrides with type-specific defaults, ensuring the UI always has sensible fallbacks.

```cpp
// Boolean alarm rendered with a yellow dot and text when true
{
  auto dewpointStyle = ConfigManagerClass::defaultBoolStyle(/*alarmWhenTrue=*/true);
  dewpointStyle.rule("stateDotOnAlarm")
    .set("background", "#f1c40f")
    .set("border", "none")
    .set("boxShadow", "0 0 4px rgba(241,196,15,0.7)")
    .set("animation", "none");
  dewpointStyle.rule("state").set("color", "#f1c40f").set("fontWeight", "600");
  cfg.defineRuntimeBool("alarms", "dewpoint_risk", "Dewpoint Risk", true, 100, dewpointStyle);
}

// Thresholded numeric field with custom label/value/unit styling
{
  auto tempFieldStyle = ConfigManagerClass::defaultNumericStyle(true);
  tempFieldStyle.rule("label").set("color", "#d00000").set("fontWeight", "700");
  tempFieldStyle.rule("values").set("color", "#0b3d91").set("fontWeight", "700");
  tempFieldStyle.rule("unit").set("color", "#000000").set("fontWeight", "700");
  cfg.defineRuntimeFieldThresholds("sensors", "temp", "Temperature", "°C", 1,
                   1.0f, 30.0f,
                   0.0f, 32.0f,
                   true, true, true, true,
                   10,
                   tempFieldStyle);
}
```

If a rule doesn’t exist yet, `rule("target")` creates it. Use `.setVisible(false)` to hide an element (e.g. boolean state text). Frontend consumes the style object verbatim inside `/runtime_meta.json`.

Full documentation moved to: `docs/STYLING.md` and `docs/THEMING.md`.

1. **Boolean Runtime Fields**

```cpp
// Plain (no alarm styling)
cfg.defineRuntimeBool("flags", "tempToggle", "Temp Toggle", false);
// Alarm boolean (blink red when true, green when safe)
cfg.defineRuntimeBool("alarms", "dewpoint_risk", "Dewpoint Risk", true);
```

Metadata adds: `isBool`, `hasAlarm`, `alarmWhenTrue` (omitted if false). Frontend derives styling:

- Alarm bool safe → solid green dot  
- Alarm bool active → blinking red  
- Plain bool true → green, false → white outline

1. **Cross‑Field Alarms** (`defineRuntimeAlarm`)

Allows conditions spanning multiple providers/fields. Each alarm has:

- `name`  
- `condition(JsonObject &root)` → returns bool  
- `onEnter()` callback (fires once when condition becomes true)  
- `onExit()` callback (fires once when condition falls back to false)

```cpp
cfg.defineRuntimeAlarm(
  "dewpoint_risk",
  [](const JsonObject &root){
      if(!root.containsKey("sensors")) return false;
      auto sensors = root["sensors"].as<JsonObject>();
      if(!sensors.containsKey("temp") || !sensors.containsKey("dew")) return false;
      float t = sensors["temp"].as<float>();
      float d = sensors["dew"].as<float>();
      return (t - d) <= 5.0f; // risk window (≤5°C above dew point)
  },
  [](){ Serial.println("[ALARM] Dewpoint proximity risk ENTER"); },
  [](){ Serial.println("[ALARM] Dewpoint proximity risk EXIT"); }
);
```

Active alarm states are added to `/runtime.json` under an `alarms` object:

```json
{
  "uptime": 123456,
  "sensors": { "temp": 21.3, "hum": 44.8, "dew": 10.2 },
  "alarms": { "dewpoint_risk": true }
}
```

1. **Relay / Actuator Integration Example**

A temperature minimum alarm driving a heater relay with hysteresis:

```cpp
#define RELAY_HEATER_PIN 25
pinMode(RELAY_HEATER_PIN, OUTPUT);
digitalWrite(RELAY_HEATER_PIN, LOW); // assume LOW = off

cfg.defineRuntimeAlarm(
  "temp_low",
  [](const JsonObject &root){
      static bool active = false; // hysteresis state
      if(!root.containsKey("sensors")) return false;
      auto sensors = root["sensors"].as<JsonObject>();
      if(!sensors.containsKey("temp")) return false;
      float t = sensors["temp"].as<float>();
      if(active) active = (t < 0.5f); // release above +0.5°C
      else       active = (t < 0.0f); // enter below 0.0°C
      return active;
  },
  [](){ Serial.println("[ALARM] temp_low ENTER -> heater ON"); digitalWrite(RELAY_HEATER_PIN, HIGH); },
  [](){ Serial.println("[ALARM] temp_low EXIT -> heater OFF"); digitalWrite(RELAY_HEATER_PIN, LOW); }
);
```

1. **Evaluating Alarms**

Call periodically in `loop()` (a lightweight internal merge of runtime JSON + condition checks):

```cpp
cfg.handleRuntimeAlarms();
```

You can adjust frequency (e.g. every 1–3s) depending on responsiveness needed.

1. **WebSocket Push**

No compile-time flags needed; Async server + WebSocket are part of the default build.

```cpp
cfg.enableWebSocketPush(2000);   // broadcast every 2s
cfg.handleWebsocketPush();       // call in loop()
```

Frontend strategy: try WebSocket first → if not available, poll `/runtime.json` (2s default in UI).

1. **Custom Payload (Optional)**

Override the generated payload:

```cpp
cfg.setCustomLivePayloadBuilder([](){
    DynamicJsonDocument d(256);
    d["uptime"] = millis();
    d["heap"] = ESP.getFreeHeap();
    String out; serializeJson(d, out); return out;
});
```

1. **Frontend Rendering Logic (Summary)**

- Uses `/runtime_meta.json` for grouping, units, thresholds, boolean semantics.  
- `/runtime.json` supplies live values + `alarms` map.  
- Alarm booleans blink slower (1.6s) for readability; numeric threshold violations use color + blink.

1. **Memory / Footprint Notes**

- Runtime doc buffers kept modest (1–2 KB per build) – adjust if you add many providers/fields.  
- Keep provider `fill` lambdas fast; avoid blocking IO inside them.

### Numeric Example

```cpp
auto tempStyle = ConfigManagerClass::defaultNumericStyle(true);
tempStyle.rule("label").set("color", "#d00000").set("fontWeight", "700");
cfg.defineRuntimeFieldThresholds("sensors", "temp", "Temperature", "°C", 1,
  1.0f, 30.0f,
  0.0f, 32.0f,
  true,true,true,true,
  10,
  tempStyle);
```

### Frontend Consumption

The Vue UI applies style rules by target name onto elements with fallback defaults. Unrecognized properties are ignored by the browser.

## 2. Global Override CSS

Instead of (or in addition to) JSON style rules you can provide a single custom stylesheet returned from the firmware at `/user_theme.css`.

### Data Attributes

Each runtime row exposes stable selectors:

```html
<div class="rw" data-group="sensors" data-key="temp" data-type="numeric" data-state="safe|warn|alarm|on|off"></div>
```

Write CSS like:

```css
/* Temperature alarm highlight */
.rw[data-group="sensors"][data-key="temp"][data-state="alarm"] .val { color:#ff2222; font-weight:700; }

/* Boolean active */
.rw[data-type="bool"][data-state="on"] .bd { background:#12c912; }

/* Hide all boolean state text globally */
.rw[data-type="bool"] .val { display:none; }
```

### Disabling Style Metadata

Call:

```cpp
cfg.disableRuntimeStyleMeta(true);
```

Then define your look entirely in `/user_theme.css`.

### Supplying Custom CSS

Embed a string (PROGMEM or regular) and register:

```cpp
extern const char userTheme[] PROGMEM;
cfg.setCustomCss(userTheme, strlen_P(userTheme));
```

If absent, the route returns 204 and frontend skips insertion.

## Choosing an Approach

| Need | Use Metadata | Use Global CSS |
|------|--------------|----------------|
| Per-field quick tweak | ✅ | ➖ |
| Ship one cohesive brand theme | ➖ | ✅ |
| Smallest JSON payload | ➖ | ✅ (disable meta) |
| Dynamic visibility-based style changes | ✅ | ✅ (with attribute selectors) |
| Firmware decides style purely in C++ | ✅ | ➖ |

You can also mix: keep minimal defaults in metadata (or none) and layer global CSS for branding.

## Performance Notes

- Style metadata size is modest (tens of bytes per field). Disable if you want leanest meta.
- Global CSS is cached by browser—serve once.
- Frontend normalizes style objects only once per meta refresh.

## Debugging

1. Fetch `/runtime_meta.json` in browser → inspect style block for a field.
2. Toggle `cfg.disableRuntimeStyleMeta(true)` to confirm global CSS still matches.
3. Use dev tools to verify data-* selectors and final computed styles.

