# Runtime Values, Providers & Alarms

This document explains how to expose live sensor/metric values, define metadata for rendering and threshold evaluation, and attach cross‑field alarms.

## Building Blocks

- Runtime Provider: named struct with a fill lambda that populates a JsonObject on each fetch.
- Runtime Field Meta: describes how to format a numeric/string/bool field (unit, precision, thresholds, ordering, style).
- Alarms: evaluators combining one or more runtime values with onEnter/onExit callbacks.

## Adding a Provider

```cpp
cfg.addRuntimeProvider({
  .name = "sensors",
  .fill = [](JsonObject &o){
      o["temp"] = readTemp();
      o["hum"]  = readHumidity();
      o["dew"]  = calcDewPoint();
  }
});
```

The manager calls `fill` only when building `/runtime.json`. Keep it fast (avoid blocking I/O).

## Defining Fields

```cpp
// Plain numeric
cfg.defineRuntimeField("sensors", "temp", "Temperature", "°C", 1);

// Thresholded numeric
cfg.defineRuntimeFieldThresholds("sensors", "temp", "Temperature", "°C", 1,
    /*warnMin*/ 1.0f, /*warnMax*/ 30.0f,
    /*alarmMin*/ 0.0f, /*alarmMax*/ 32.0f,
    /*warnLow*/true, /*warnHigh*/true, /*alarmLow*/true, /*alarmHigh*/true,
    /*order*/ 10);

// Boolean
cfg.defineRuntimeBool("flags", "heater_on", "Heater On", false);

// String (dynamic from provider value)
cfg.defineRuntimeString("system", "fw", "Firmware", CONFIGMANAGER_VERSION);

// Static string (always shown)
cfg.defineRuntimeString("system", "build", "Build", "2025-09-30", 5);

// Divider line
cfg.defineRuntimeDivider("sensors", "Environment", 0);
```

## Field Ordering

Provide an order (int). Lower numbers appear earlier. Default is 100.

## Provider Card Ordering

```cpp
cfg.setRuntimeProviderOrder("system", 1);
cfg.setRuntimeProviderOrder("sensors", 5);
```

## Boolean Semantics & Alarm Booleans

Alarm booleans visually distinguish safe vs active states (dot colors and optional blink). Boolean meta includes `isBool` and, when relevant, `alarmWhenTrue`.

## Dynamic Numeric Alarms (e.g. IOManager)

Some subsystems (like `IOManager` analog alarms) use **dynamic thresholds** that can be changed via Settings at runtime.

- In this case, static `alarmMin/alarmMax` values in `/runtime_meta.json` are treated as defaults / hints.
- The recommended mechanism for the Live UI is to additionally expose explicit alarm state flags next to the numeric value:
  - `<key>_alarm_min` (bool)
  - `<key>_alarm_max` (bool)

When present, the WebUI uses these flags to decide whether a numeric field is in alarm state.

## Cross-Field Alarms

```cpp
cfg.defineRuntimeAlarm(
  "dewpoint_risk",
  [](const JsonObject &root){
      if(!root.containsKey("sensors")) return false;
      auto s = root["sensors"].as<JsonObject>();
      if(!s.containsKey("temp") || !s.containsKey("dew")) return false;
      return (s["temp"].as<float>() - s["dew"].as<float>()) <= 5.0f;
  },
  [](){ Serial.println("ENTER dewpoint_risk"); },
  [](){ Serial.println("EXIT dewpoint_risk"); }
);
```

Active alarms appear under an `alarms` object inside `/runtime.json`.

## Manual & Timed Push

Enable periodic WebSocket broadcast or call `pushRuntimeNow()` manually:

```cpp
cfg.enableWebSocketPush(2000);
// loop()
cfg.handleWebsocketPush();
```

## Custom Live Payload

Provide a completely custom JSON payload instead of auto‑merging providers:

```cpp
cfg.setCustomLivePayloadBuilder([](){
  DynamicJsonDocument d(256);
  d["uptime"] = millis();
  d["heap"] = ESP.getFreeHeap();
  String out; serializeJson(d, out); return out;
});
```

## Alarm Evaluation Frequency

Call `cfg.handleRuntimeAlarms()` periodically (e.g. every 1–3s) if you use cross‑field alarms.

## Styling runtime fields

For per‑field style metadata (colors, visibility, boolean dot behavior) see `docs/STYLING.md`. For global CSS theming via `/user_theme.css`, see `docs/THEMING.md`.


### Minimal End‑to‑End Example (Live + Alarm)

```cpp
// Setup (after WiFi):
cfg.addRuntimeProvider({ .name="sys", .fill = [](JsonObject &o){ o["heap"] = ESP.getFreeHeap(); }});
cfg.defineRuntimeField("sys", "heap", "Free Heap", "B", 0);
cfg.addRuntimeProvider({ .name="env", .fill = [](JsonObject &o){ o["temp"] = readTempSensor(); }});
cfg.defineRuntimeFieldThresholds("env", "temp", "Temperature", "°C", 1,
   5.0f, 30.0f,   // warn range
   0.0f, 40.0f,   // alarm range
   true,true,true,true);
cfg.defineRuntimeAlarm("too_cold",
   [](const JsonObject &root){ return root["env"]["temp"].as<float>() < 0.0f; },
   [](){ Serial.println("cold ENTER"); },
   [](){ Serial.println("cold EXIT"); }
);
cfg.enableWebSocketPush(1500);
```

#### Runtime String Fields, Dividers & Ordering

You can enrich the Live view with informational text lines and visual separators and control ordering.

```cpp
// String value (dynamic: taken from provider runtime JSON key "fw")
cfg.defineRuntimeString("system", "fw", "Firmware", CONFIGMANAGER_VERSION);

// Static string (not looked up in /runtime.json)
cfg.defineRuntimeString("system", "build", "Build", "2025-09-29", /*order*/ 5);

// Divider at top of sensors group
cfg.defineRuntimeDivider("sensors", "Environment", 0);

// Ordered numeric fields
cfg.defineRuntimeField("sensors", "temp", "Temperature", "°C", 1, /*order*/ 10);
cfg.defineRuntimeField("sensors", "hum", "Humidity", "%", 1, /*order*/ 20);

// Provider (card) ordering
cfg.setRuntimeProviderOrder("system", 1);
cfg.setRuntimeProviderOrder("sensors", 5);
```

Metadata additions in `/runtime_meta.json`:

| Key | Meaning |
|-----|---------|
| `isString` | Render as plain text value (no unit/precision formatting) |
| `isDivider` | Render a horizontal rule with label |
| `staticValue` | Value to show even if absent from `/runtime.json` |
| `order` | Sort hint (lower first). Default 100 |

Older frontends ignore these keys gracefully.

