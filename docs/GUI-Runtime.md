# GUI Runtime (Live) - Providers, Layout, Styling, Theming

This document consolidates all runtime (live) topics: providers, layout, runtime fields, alarms, styling metadata, and global CSS theming.

## 1. Overview

Runtime values are served by the Web UI using:

- `/runtime.json` for live values
- `/runtime_meta.json` for field metadata
- `/live_layout.json` for the optional page/card/group layout
- `/user_theme.css` for optional global CSS

### Demand-driven updates (4.8.0 Unreleased)

`/runtime.json` without a selector remains a full snapshot. Pass a URL-encoded
JSON array in `groups`, for example `groups=%5B%22sensors%22%5D`, to evaluate only
those exact, case-sensitive source groups. `groups=[]` selects none; unknown
valid names do not expand to all. Malformed selectors return HTTP 400 before
providers run. Limits: 32 groups, 64 UTF-8 bytes per group, 2048 bytes in the
serialized selector. Control-only groups and the `alarms` group work as usual.
Selected snapshots retain `uptime` and, when the built-in system provider is
enabled, `system.otaActive` and `system.otaHasPassword`; this safety envelope does
not execute unrelated system telemetry. Each selected control getter runs once.

Legacy `/ws` connections still receive full snapshots. New clients connect to
`/ws?runtime=selected` (initially empty) and send
`{"type":"runtimeSubscribe","groups":["sensors"]}`. `/ws?runtime=none` receives
independent GUI/log events only. Unknown or explicitly empty runtime modes are
closed. At most eight selective subscriptions are retained; legacy/event
connections keep their existing connection behavior. Only complete text frames
up to 2112 bytes are considered, and invalid selections retain the last valid
subscription. The server evaluates the union once per tick and projects it per
client. Selected frames include `_runtimeGroups` so clients can discard queued
frames for an old page. A legacy client intentionally forces a full union.

Custom WebSocket payload builders keep their complete opaque payload and are
called once per tick with runtime recipients; selection does not reinterpret
custom fields. GUI-only clients do not trigger them. Standalone legacy HTTP
`setCallbacks` providers remain full even when `groups` is supplied unless an
additive `setSelectedRuntimeJsonProvider` callback is installed. ConfigManager
installs that callback itself and filters before provider/getter invocation.

The bundled UI uses one union for visible cards on the active page, an event-only
GUI socket and one selective dashboard socket. Settings, log-only view and hidden
browser documents request only the safety envelope. Without WebSocket support,
the fallback polls that small envelope while ordinary groups are paused. A page
change/resume obtains a fresh selected snapshot. Successful metadata/layout are
shared in browser memory until reload; failed loads can retry. A metadata-less
legacy dashboard performs one shared full discovery request. No runtime JSON
files or persistent per-card copies are created. Response-body timeouts abort,
and at most one fallback request is active; superseded replies are ignored.

### Delivery and backpressure

Runtime JSON construction is serialized across HTTP and WebSocket requests; a
busy generator makes the loop's WebSocket tick skip rather than wait. Each
runtime recipient has at most one outstanding message; slow peers skip ticks
without suppressing independent healthy peers. General text/log/event APIs keep
their existing behavior. HTTP static/JSON responses use bounded TCP admission;
this limits in-flight bytes, not the total memory held by a generated response.
An empty runtime provider result returns HTTP 503.

Normal SIL/SBT access was operator-accepted on 2026-10-22 as faster and without
observed crashes. See [the investigation record](todo_http_delivery_panic.md)
for the acceptance scope and preserved failed diagnostic/overload cohorts.

## 2. Runtime Providers

A runtime provider fills a `JsonObject` when `/runtime.json` is requested.

```cpp
ConfigManager.getRuntime().addRuntimeProvider("sensors", [](JsonObject &o) {
    o["temp"] = readTemp();
    o["hum"] = readHumidity();
    o["dew"] = calcDewPoint();
}, 10);
```

Keep providers fast and non-blocking.

## 3. Runtime Fields (Legacy API)

The classic API still works and is useful for quick fields:

```cpp
// Numeric
ConfigManager.defineRuntimeField("sensors", "temp", "Temperature", "C", 1);

// Numeric with thresholds
ConfigManager.defineRuntimeFieldThresholds(
    "sensors", "temp", "Temperature", "C", 1,
    1.0f, 30.0f,
    0.0f, 32.0f,
    true, true, true, true,
    10);

// Boolean
ConfigManager.defineRuntimeBool("flags", "heater_on", "Heater On", false);

// String (dynamic)
ConfigManager.defineRuntimeString("system", "fw", "Firmware", CONFIGMANAGER_VERSION);

// String (static)
ConfigManager.defineRuntimeString("system", "build", "Build", "2025-09-30", 5);

// Divider
ConfigManager.defineRuntimeDivider("sensors", "Environment", 0);
```

Field ordering uses `order` (lower first). Default is `100`.

### Meta Keys in `/runtime_meta.json`

| Key | Meaning |
| --- | --- |
| `isString` | Render as plain text |
| `isDivider` | Render a divider line |
| `staticValue` | Value shown even without runtime value |
| `order` | Sort hint |

## 4. Live Layout (Pages, Cards, Groups)

Layout is optional.

- No pages defined -> no tabs, cards are derived from providers.
- Pages defined -> tabs are enabled.
- Card is the H3 title.
- Group is optional and only inside a card (not recursive).
- If group is omitted, items render directly inside the card.

## 5. Builder API (Recommended)

The builder lets you define layout and fields in one flow.

### Basic (no group)

```cpp
auto live = ConfigManager.liveGroup("sensors")
    .page("Sensors", 10)
    .card("BME280 - Temperature Sensor");

live.value("temp", []() { return temperature; })
    .label("Temperature")
    .unit("C")
    .precision(1)
    .order(10);
```

### Group inside card

```cpp
auto dew = ConfigManager.liveGroup("sensors")
    .page("Sensors", 10)
    .card("BME280 - Temperature Sensor")
    .group("Dewpoint", 20);

dew.value("dew", []() { return dewPoint; })
    .label("Dewpoint")
    .unit("C")
    .precision(1)
    .order(20);
```

### Supported field types (builder)

- `value(key, getter)` for numeric/string/bool (auto type)
- `value(getter)` auto key (`field_#`) for quick demos
- `boolValue(key, getter)` with alarm semantics
- `divider(label, order)`
- `button`, `checkbox`, `stateButton`, `momentaryButton`
- `intSlider`, `floatSlider`
- `intInput`, `floatInput`

Auto keys are generated in creation order; use explicit keys for stable CSS/automation.

## 6. Alarms

Use `AlarmManager` for computed alarms and place them inside the layout.

```cpp
cm::AlarmManager alarmManager;

alarmManager.addDigitalWarning({
    .id = "dewRisk",
    .name = "Dewpoint Risk",
    .kind = cm::AlarmKind::DigitalActive,
    .severity = cm::AlarmSeverity::Warning,
    .enabled = true,
    .getter = []() { return temperature < dewPoint; },
});

alarmManager.addWarningToLive(
    "dewRisk",
    30,
    "Sensors",
    "BME280 - Temperature Sensor",
    "Dewpoint",
    "Dewpoint Risk");
```

Call `alarmManager.update()` periodically (every 1-3s is typical).

### Cross-Field Alarms (Legacy)

If you use `defineRuntimeAlarm`, call `handleRuntimeAlarms()` periodically:

```cpp
ConfigManager.defineRuntimeAlarm(
    "dewpoint_risk",
    [](const JsonObject &root) {
        if (!root.containsKey("sensors")) return false;
        auto s = root["sensors"].as<JsonObject>();
        if (!s.containsKey("temp") || !s.containsKey("dew")) return false;
        return (s["temp"].as<float>() - s["dew"].as<float>()) <= 5.0f;
    },
    []() { Serial.println("[I] dewpoint_risk ENTER"); },
    []() { Serial.println("[I] dewpoint_risk EXIT"); }
);

ConfigManager.handleRuntimeAlarms();
```

## 7. Dynamic Threshold Alarms (Numeric)

Some subsystems (for example IOManager) expose dynamic thresholds.

When thresholds change at runtime, expose explicit boolean flags to reflect alarm state:

- `<key>_alarm_min`
- `<key>_alarm_max`

The UI will use those flags to decide whether the numeric value is in alarm state.

## 8. Styling with Metadata

Runtime fields can ship style metadata for specific targets:

- `row`
- `label`
- `values`
- `unit`
- `state`
- `stateDotOnTrue`
- `stateDotOnFalse`
- `stateDotOnAlarm`

Example:

```cpp
auto style = ConfigManagerClass::defaultNumericStyle(true);
style.rule("label").set("color", "#d00000").set("fontWeight", "700");
style.rule("values").set("color", "#0b3d91").set("fontWeight", "700");
style.rule("unit").set("color", "#0b3d91").set("fontWeight", "700");

ConfigManager.defineRuntimeFieldThresholds(
    "sensors", "temp", "Temperature", "C", 1,
    1.0f, 30.0f,
    0.0f, 32.0f,
    true, true, true, true,
    10,
    style);
```

Hide an element:

```cpp
style.rule("state").setVisible(false);
```

## 9. addCSSClass (Builder Helper)

The default overload applies the class to row + label + value + unit.

```cpp
live.value("temp", []() { return temperature; })
    .addCSSClass("myTempClass");
```

Targeted overload:

```cpp
live.value("temp", []() { return temperature; })
    .addCSSClass("myLabelClass", ConfigManagerClass::LiveCssTarget::Label)
    .addCSSClass("myDotAlarm", ConfigManagerClass::LiveCssTarget::DotOnAlarm);
```

Available targets:

- `Row`
- `Label`
- `Value`
- `Unit`
- `State`
- `Dot`
- `DotOnTrue`
- `DotOnFalse`
- `DotOnAlarm`

## 10. Global CSS (`/user_theme.css`)

You can serve a custom stylesheet from the device.

```cpp
extern const char userTheme[] PROGMEM;
ConfigManager.setCustomCss(userTheme, strlen_P(userTheme));
```

Disable metadata if you want CSS-only control:

```cpp
ConfigManager.disableRuntimeStyleMeta(true);
```

### Data Attributes

Runtime rows expose:

- `data-group` (provider)
- `data-key` (field key)
- `data-type` (numeric/bool/string)
- `data-state` (safe/warn/alarm/on/off)

### Example CSS

```css
.rw[data-group="sensors"][data-key="temp"][data-state="alarm"] .val {
  color: #ff2222;
  font-weight: 700;
}
```

## 11. Theming Variables

You can override theme variables in `/user_theme.css`:

```css
:root {
  --cm-bg: #f6f7f9;
  --cm-fg: #111827;
  --cm-card-bg: #ffffff;
  --cm-card-border: rgba(0,0,0,.06);
  --cm-group-bg: rgba(17,24,39,.03);
  --cm-group-border: rgba(0,0,0,.08);
  --cm-group-title: #374151;
}
```

## 12. WebSocket Push

```cpp
ConfigManager.enableWebSocketPush();
ConfigManager.handleWebsocketPush();
```

The UI prefers WebSocket and falls back to polling `/runtime.json`.
The default push interval is 5000 ms; explicit values are clamped to
550..60000 ms.

## 13. Custom Payload (Optional)

Provide a custom runtime JSON payload:

```cpp
ConfigManager.setCustomLivePayloadBuilder([]() {
    DynamicJsonDocument d(256);
    d["uptime"] = millis();
    d["heap"] = ESP.getFreeHeap();
    String out;
    serializeJson(d, out);
    return out;
});
```

## 14. GUI Notifications from Runtime Code

You can trigger the same popup overlay used by validation:

```cpp
ConfigManager.sendWarnMessage(
    "Pump overheating",
    "Outlet temperature above 70C",
    "Automatic cooling disabled",
    ConfigManagerClass::GUIMessageButtons::OkCancelRetry,
    []() { Serial.println("[I] Cooling engaged"); },
    []() { Serial.println("[I] User cancelled"); },
    []() { Serial.println("[I] User requested retry"); },
    [](JsonObject &ctx) { ctx["runtimeProvider"] = "cooling"; }
);
```

## 15. Performance Notes

- Provider fill functions should be fast and non-blocking.
- Style metadata adds a small JSON overhead.
- Global CSS is cached by the browser.

## 16. Debug Checklist

1. Open `/runtime.json` and verify live values.
2. Open `/runtime_meta.json` and verify field meta + style.
3. Open `/live_layout.json` and verify pages/cards/groups.
4. Open `/user_theme.css` and verify CSS output.
5. Inspect a `.rw` element in dev tools for data attributes.

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.addRuntimeProvider` | `addRuntimeProvider(const RuntimeValueProvider& provider)`<br>`addRuntimeProvider(const String& name, std::function<void(JsonObject&)> fillFunc, int order = 100)` | Registers runtime data providers for the Live UI. | Provider callbacks should stay non-blocking. |
| `ConfigManager.enableWebSocketPush` | `enableWebSocketPush(uint32_t intervalMs = 5000)` | Enables push updates for runtime/live data. | UI falls back to polling when push is disabled; intervals are clamped to 550..60000 ms. |
| `ConfigManager.setCustomLivePayloadBuilder` | `setCustomLivePayloadBuilder(std::function<String()> fn)` | Replaces default runtime payload generation with custom JSON. | Advanced customization hook. |
| `ConfigManager.sendWarnMessage` | `sendWarnMessage(...)` | Shows runtime warning dialog with optional callbacks/context. | Use for operator-visible runtime events. |



## Page-scoped dashboard discovery

`GET /live_pages.json` returns
`{pageScoped,hasLiveContent,pages:[{key,name,title,order}],defaultPage}`.
The index describes navigation without invoking runtime provider getters. Page
keys are exact backend identifiers (trimmed, lowercase layout names); retain
spaces and punctuation and URL-encode the complete key.

When `pageScoped` is true, request `/live_layout.json?page=<key>` followed by
`/runtime_meta.json?page=<key>`. Layout keeps its existing
`{pages,placements,defaultPage}` schema and metadata remains an array, each limited
to the selected page. The optional `__unassigned__` index entry covers metadata
without a rendered placement. Empty/oversized selectors return 400; unknown keys
return 404. A failed allocation returns 503, never a partial successful payload.
Omitting `page` preserves the original complete endpoints.

The built-in dashboard shares the index request, caches successful page pairs in
browser memory and loads layout before metadata. Failed or aborted loads are
retryable. Tabs remain visible while content loads. Page changes and hidden or
Settings views clear runtime group demand; only the active loaded page resumes
its selected source groups. GUI/log events and OTA safety remain available.
Settings configuration is loaded when Settings opens. Unsupported index routes
or `pageScoped:false` retain legacy discovery; an empty scoped page does not
trigger full discovery.

Automatic placements retain both field key and source group, so identical field
keys on separate pages resolve to the correct provider. Explicit legacy layout
bindings without a source group remain global overrides. Two different sources
with the same bare field ID on the same page remain ambiguous in the legacy
layout wire schema; use distinct field IDs for that arrangement.
