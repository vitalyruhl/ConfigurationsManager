# Feature Flags / Build Options

## v3.0.0 change

Starting with **v3.0.0**, the library no longer requires a list of `CM_ENABLE_*` build flags or PlatformIO `extra_scripts` to build.

- WebUI, OTA, runtime controls, WebSocket push, theming/styling are compiled in by default.
- **Only logging remains configurable**:
  - `CM_ENABLE_LOGGING` (default: `1`)
  - `CM_ENABLE_VERBOSE_LOGGING` (default: `0`)

If your project still defines removed flags (like `CM_ENABLE_OTA`, `CM_EMBED_WEBUI`, etc.), the build will fail with a clear `#error` message so you can remove them.
