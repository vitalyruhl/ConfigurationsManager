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

```
<div class="rt-row" data-group="sensors" data-key="temp" data-type="numeric" data-state="safe|warn|alarm|on|off"></div>
```

Write CSS like:

```css
/* Temperature alarm highlight */
.rt-row[data-group="sensors"][data-key="temp"][data-state="alarm"] .rt-value { color:#ff2222; font-weight:700; }

/* Boolean active */
.rt-row[data-type="bool"][data-state="on"] .rt-state-dot { background:#12c912; }

/* Hide all boolean state text globally */
.rt-row[data-type="bool"] .rt-state-text { display:none; }
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

