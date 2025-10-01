# Theming & Custom CSS

There are two layers for controlling appearance of the Live runtime view:

1. JSON style metadata per field
2. A single global CSS stylesheet at `/user_theme.css`

You can use either layer alone or mix them.

## Quick Start (Global CSS Only)

```cpp
// 1. Provide CSS (can be PROGMEM)
const char userTheme[] PROGMEM = R"CSS(
.rt-row[data-type="bool"] .rt-state-text { display:none; }
.rt-row[data-group="sensors"][data-key="temp"][data-state="alarm"] .rt-value { color:#ff2222; font-weight:700; }
)CSS";

// 2. Register before startWebServer
cfg.setCustomCss(userTheme, strlen_P(userTheme));

// 3. Disable metadata if desired
cfg.disableRuntimeStyleMeta(true);
```

Browser loads `/user_theme.css` once on page mount.

## Mixing Metadata + CSS

Keep semantic per‑field tweaks (e.g. hide just one unit or color one label) in metadata; apply brand colors, layout refinements, fonts via global CSS.

## Data Attributes Reference

| Attribute | Values | Purpose |
|-----------|--------|---------|
| data-group | provider name | Identify provider card a row belongs to |
| data-key | field key | Identify field inside provider |
| data-type | numeric / bool / string / divider | Styling differences |
| data-state | safe / warn / alarm / on / off | Status derived by frontend |

## Disabling Style Metadata Later

You can toggle at runtime prior to serving meta (typically once during setup):
```cpp
cfg.disableRuntimeStyleMeta(true); // turn off
cfg.disableRuntimeStyleMeta(false); // turn on again
```
A page reload reflects the change.

## Providing CSS Dynamically

You may build CSS at runtime (e.g. from user preferences) and call setCustomCss() again before clients fetch `/user_theme.css`. Reassigning updates immediately for new visitors (existing pages won't refetch unless reloaded).

## Size & Performance

- Metadata elimination saves a few hundred bytes for large sets of fields.
- One extra HTTP 200 (or 204 if empty) on initial load; then cached.
- DOM styling uses data-* selectors → fast in modern browsers.

## Debug Checklist

1. Visit `/user_theme.css` directly – expect your CSS or empty response.
2. Fetch `/runtime_meta.json` and confirm `style` sections absent when disabled.
3. Inspect a `.rt-row` element in dev tools for data-* attributes.
4. If a rule not applying, test a simpler selector (e.g. `.rt-row[data-key="temp"]`).
5. Ensure no conflicting inline style metadata overriding your global rule.

## Example Full Theme

```css
/* Hide all boolean text */
.rt-row[data-type="bool"] .rt-state-text { display:none; }
/* Active bool dot */
.rt-row[data-type="bool"][data-state="on"] .rt-state-dot { background:#18c61d; box-shadow:0 0 4px #18c61dAA; }
/* Alarmed numeric */
.rt-row[data-state="alarm"] .rt-value { color:#ff2a2a; font-weight:700; animation:blink 1.2s steps(2,start) infinite; }
/* Divider styling */
.rt-row[data-type="divider"] { border-bottom:1px solid #ccc; margin-top:4px; }
@keyframes blink { 50% { opacity:0.35; } }
```

