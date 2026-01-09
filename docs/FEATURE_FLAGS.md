# Feature Flags / Build Options

## v3.0.0 change

Starting with **v3.0.0**, the library no longer requires a list of `CM_ENABLE_*` build flags or PlatformIO `extra_scripts` to build.

- WebUI, OTA, runtime controls, WebSocket push, theming/styling are compiled in by default.
- **Only logging remains configurable**:
  - `CM_ENABLE_LOGGING` (default: `1`)
  - `CM_ENABLE_VERBOSE_LOGGING` (default: `0`)

If your project still defines removed flags (like `CM_ENABLE_OTA`, `CM_EMBED_WEBUI`, etc.), the build will fail with a clear `#error` message so you can remove them.

## Historical (v2.x)

In v2.x this library supported many feature flags and used an extra build step to prune and rebuild the embedded WebUI per flag set.
The information below is kept for reference only.

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
