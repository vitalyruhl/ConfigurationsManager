# Feature Flags (Build-Time)

> **⚠️ Prerequisites:** Before using feature flags, ensure you have copied the `tools/` folder from `examples/tools/` to your project root and configured `extra_scripts = pre:tools/precompile_wrapper.py` in your `platformio.ini`. See the main README for setup instructions.

This project exposes many optional features that you can enable/disable at compile time to trim flash/RAM and reduce the embedded web UI size. Flags are passed via PlatformIO `build_flags` using `-D<FLAG>=0/1`.

Why build flags?

- Keep your sketch clean and portable
- Different environments (USB/OTA) can use different feature sets
- The extra build script uses these flags to prune the frontend bundle for real space savings

How to set flags

- Edit `platformio.ini` under each `[env:*]` → `build_flags`
- Example (commonly enabled):
  - `-DCM_ENABLE_OTA=1`
  - `-DCM_ENABLE_WS_PUSH=1`
  - `-DCM_ENABLE_RUNTIME_ANALOG_SLIDERS=1`
- Turn a feature off by setting `=0`

Core flags

- CM_EMBED_WEBUI: Embed the Vue SPA into flash (HTML/CSS/JS)
- CM_ENABLE_OTA: Enable OTA HTTP endpoint and web Flash button
- CM_ENABLE_WS_PUSH: Enable WebSocket push for live runtime updates
- CM_ENABLE_SYSTEM_PROVIDER: Show system card (uptime, heap, RSSI)
- CM_ENABLE_RUNTIME_ANALOG_SLIDERS: Enable analog (numeric) sliders in Runtime view
- CM_ENABLE_RUNTIME_NUMBER_INPUTS: Enable manual numeric input fields (with Set button) in Runtime view
- CM_ENABLE_RUNTIME_BUTTONS / CM_ENABLE_RUNTIME_STATE_BUTTONS / CM_ENABLE_RUNTIME_CHECKBOXES: UI controls
- CM_ENABLE_RUNTIME_ALARMS: Cross-field and per-field alarm support
- CM_ENABLE_STYLE_RULES: Per-field style overrides through metadata
- CM_ENABLE_USER_CSS: Serve and auto-inject optional `/user_theme.css`
- CM_ENABLE_LOGGING / CM_ENABLE_VERBOSE_LOGGING: Serial logging controls

Defaults in this repo

- The example environments enable most features by default (some may be toggled for demonstration). Adjust as needed for your firmware size and use case.

Frontend pruning and compression

- The extra build step reads your flags and:
  - Exposes them to the Vite build (so dead code is dropped)
  - Stubs certain components when disabled
  - Compresses the generated `index.html` to `index.html.gz`

Notes

- Changing flags triggers a full frontend rebuild automatically.
- **DEPRECATED:** `CM_ENABLE_RUNTIME_INT_SLIDERS` and `CM_ENABLE_RUNTIME_FLOAT_SLIDERS` are deprecated as of v2.7.0. Use `CM_ENABLE_RUNTIME_ANALOG_SLIDERS=1` instead for both int and float sliders.
- **DEPRECATED:** The `DCM_` prefix is deprecated. Use `CM_` prefix for all flags.
- For deeper control, see `webui/vite.config.mjs` and `tools/preCompile_script.py`.

Migration note: In some earlier notes the prefix `DCM_` was used—current flags use the `CM_` prefix.

## Flash size impact by flags (v2.6.x)

The following measurements were taken with an ESP32 NodeMCU-32S, Arduino framework, builder v2.6.x. Values are approximate and will vary by project.

| Option | Description | Flash usage | Bytes used | Delta |
|--------|-------------|-------------|------------|-------|
| CM_EMBED_WEBUI=0 | External UI mode (no SPA embedded) | 75.3% | 986,885 | baseline |
| CM_EMBED_WEBUI=1 (raw) | Embedded UI, uncompressed HTML/CSS/JS | 83.7% | 1,097,177 | +≈110 KB |
| CM_EMBED_WEBUI=1 (gz) | Embedded UI, gzipped HTML/CSS/JS | 78.3% | 1,025,957 | +≈39 KB |
| CM_ENABLE_WS_PUSH=0 | Disable WebSocket push | 77.4% | 1,014,121 | −≈11 KB |
| CM_ENABLE_SYSTEM_PROVIDER=0 | Hide system card | 78.0% | 1,023,005 | −≈2 KB |
| CM_ENABLE_OTA=0 | Disable OTA endpoint and UI | 73.4% | 962,645 | −≈63 KB |
| CM_ENABLE_RUNTIME_BUTTONS=0 | Disable action buttons | 78.1% | 1,023,737 | −≈0.6 KB |
| CM_ENABLE_RUNTIME_CHECKBOXES=0 | Disable runtime checkboxes | 78.1% | 1,023,233 | −≈0.5 KB |
| CM_ENABLE_RUNTIME_STATE_BUTTONS=0 | Disable state buttons | 78.1% | 1,023,137 | −≈0.6 KB |
| CM_ENABLE_RUNTIME_ANALOG_SLIDERS=0 | Disable numeric sliders | 77.7% | 1,017,957 | −≈5.2 KB |
| CM_ENABLE_RUNTIME_ALARMS=0 | Disable alarms | 78.1% | 1,024,213 | −≈0.4 KB |
| CM_ENABLE_RUNTIME_NUMBER_INPUTS=0 | Disable number inputs | 78.3% | 1,025,957 | −≈2.0 KB |
| CM_ENABLE_STYLE_RULES=0 | Disable per-field style metadata | 77.8% | 1,019,421 | −≈5.0 KB |
| CM_ENABLE_USER_CSS=0 | Disable `/user_theme.css` route | 78.2% | 1,024,477 | −≈0.3 KB |
| CM_ENABLE_LOGGING=0 | Disable serial logging | 77.9% | 1,021,649 | −≈3.0 KB |
| CM_ENABLE_VERBOSE_LOGGING=0 | Disable verbose logs | 78.0% | 1,022,793 | −≈2.0 KB |

Summary: with gzip embedding, the UI adds roughly 39 KB over external-UI mode. If you’re very tight on flash, consider `CM_EMBED_WEBUI=0` and hosting the UI externally.
