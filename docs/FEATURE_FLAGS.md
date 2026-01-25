# Feature Flags / Build Options

## v3.0.0 change

Starting with **v3.0.0**, the library no longer requires a list of `CM_ENABLE_*` build flags or PlatformIO `extra_scripts` to build.

- WebUI, OTA, runtime controls, WebSocket push, theming/styling are compiled in by default.
- **Supported build-time options**:
  - `CM_EMBED_WEBUI` (default: `1`)
    - `1`: embed the WebUI HTML into the firmware image (serves on `/`)
    - `0`: do not embed the WebUI HTML (saves flash); API routes stay available
  - `CM_ENABLE_LOGGING` (default: `1`)
  - `CM_ENABLE_VERBOSE_LOGGING` (default: `0`)
  - `CM_ENABLE_OTA` (default: `1`)
  - `CM_ENABLE_SYSTEM_PROVIDER` (default: `1`)
  - `CM_ENABLE_SYSTEM_TIME` (default: `1`)
  - `CM_ENABLE_STYLE_RULES` (default: `1`)
  - `CM_ENABLE_USER_CSS` (default: `1`)
  - `CM_ENABLE_THEMING` (default: `1`)

If your project still defines removed flags, the build will fail with a clear `#error` message so you can remove them (example: `CM_ENABLE_WS_PUSH`).
