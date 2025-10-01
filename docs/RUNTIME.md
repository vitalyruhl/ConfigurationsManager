# Runtime Values, Providers & Alarms

This document explains how to expose live sensor/metric values, define metadata for rendering & threshold evaluation, and attach cross-field alarms.

## Building Blocks

- Runtime Provider: named struct with a fill lambda populating a JsonObject each fetch.
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

Call fill only when building `/runtime.json`. Keep it fast (no blocking IO).

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

Provide an order (int). Lower numbers appear earlier. Default 100.

## Provider Card Ordering

```cpp
cfg.setRuntimeProviderOrder("system", 1);
cfg.setRuntimeProviderOrder("sensors", 5);
```

## Boolean Semantics & Alarm Booleans

Alarm bools visually distinguish safe vs active (dot colors & optional blink). bool meta includes `isBool`, plus `alarmWhenTrue` when relevant.

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

Provide a completely custom JSON string instead of auto-merging providers:

```cpp
cfg.setCustomLivePayloadBuilder([](){
  DynamicJsonDocument d(256);
  d["uptime"] = millis();
  d["heap"] = ESP.getFreeHeap();
  String out; serializeJson(d, out); return out;
});
```

## Alarm Evaluation Frequency

Call `cfg.handleRuntimeAlarms()` periodically (e.g. every 1–3s) if you have cross-field alarms.

## Performance Guidelines

- Keep provider fill < 1ms typical.
- Avoid dynamic allocation inside hot paths; reuse sensors & caches.
- Keep runtime_meta (field definitions) stable—only rebuild when definitions change.

