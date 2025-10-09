# Feature Flags (Build-Time)

This project exposes many optional features that you can enable/disable at compile time to trim flash/RAM and reduce the embedded web UI size. Flags are passed via PlatformIO `build_flags` using `-D<FLAG>=0/1`.

Why build flags?

- Keep your sketch clean and portable
- Different environments (USB/OTA) can use different feature sets
- The extra build script uses these flags to prune the frontend bundle for real space savings

How to set flags

- Edit `platformio.ini` under each `[env:*]` → `build_flags`
- Example (enable all):
  - `-DCM_ENABLE_OTA=1`
  - `-DCM_ENABLE_WS_PUSH=1`
  - `-DCM_ENABLE_RUNTIME_INT_SLIDERS=1`
- Turn a feature off by setting `=0`

Core flags

- CM_EMBED_WEBUI: Embed the Vue SPA into flash (HTML/CSS/JS)
- CM_ENABLE_OTA: Enable OTA HTTP endpoint and web Flash button
- CM_ENABLE_WS_PUSH: Enable WebSocket push for live runtime updates
- CM_ENABLE_SYSTEM_PROVIDER: Show system card (uptime, heap, RSSI)
- CM_ENABLE_RUNTIME_CONTROLS: Enable base runtime controls support
- CM_ENABLE_RUNTIME_ANALOG_SLIDERS: Enable analog (numeric) sliders in Runtime view (replaces separate INT/FLOAT flags)
- CM_ENABLE_RUNTIME_NUMBER_INPUTS: Enable manual numeric input fields (with Set button) in Runtime view
- CM_ENABLE_RUNTIME_BUTTONS / CM_ENABLE_RUNTIME_STATE_BUTTONS / CM_ENABLE_RUNTIME_CHECKBOXES: UI controls
- CM_ENABLE_RUNTIME_ALARMS: Cross-field and per-field alarm support
- CM_ENABLE_DYNAMIC_VISIBILITY: `showIf` callbacks to hide/show settings dynamically
- CM_ENABLE_STYLE_RULES: Per-field style overrides through metadata
- CM_ENABLE_USER_CSS: Serve and auto-inject optional `/user_theme.css`
- CM_ENABLE_LOGGING / CM_ENABLE_VERBOSE_LOGGING: Serial logging controls
- CM_ALARM_GREEN_ON_FALSE: Render alarm boolean "safe" state as green=false

Defaults in this repo

- Both `env:usb` and `env:ota` enable ALL features by default. Start with everything on, then turn off what you don't use.

Frontend pruning and compression

- The extra build step reads your flags and:
  - Exposes them to the Vite build (so dead code is dropped)
  - Stubs certain components when disabled
  - Compresses the generated `index.html` to `index.html.gz`

Notes

- Changing flags triggers a full frontend rebuild automatically.
- Backward compatibility: old flags (CM_ENABLE_RUNTIME_INT_SLIDERS / CM_ENABLE_RUNTIME_FLOAT_SLIDERS) still enable sliders if present, but prefer the new combined flag.
- If you disable OTA at compile time, the Flash button and endpoint disappear.
- If WebSocket push is disabled, the UI falls back to polling `/runtime.json`.

Troubleshooting

- If Node/npm not found, install from nodejs.org and ensure `npm` is on PATH.
- If the web UI looks unchanged, delete `webui/node_modules` and build again.
- For deeper control, see `webui/vite.config.mjs` and `extra_script.py`.
