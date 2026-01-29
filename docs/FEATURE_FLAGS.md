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
  - `CM_DISABLE_GUI_LOGGING` (default: `0`)
  - `CM_ENABLE_OTA` (default: `1`)
  - `CM_ENABLE_SYSTEM_PROVIDER` (default: `1`)
  - `CM_ENABLE_SYSTEM_TIME` (default: `1`)
  - `CM_ENABLE_STYLE_RULES` (default: `1`)
  - `CM_ENABLE_USER_CSS` (default: `1`)
  - `CM_ENABLE_THEMING` (default: `1`)

If your project still defines removed flags, the build will fail with a clear `#error` message so you can remove them (example: `CM_ENABLE_WS_PUSH`).

## Size impact (rough guidance)

These are rough estimates. Exact savings depend on compiler, LTO, and what your firmware actually uses.
For real numbers, build your target with and without each flag and compare the final `.bin` size.

- `CM_EMBED_WEBUI=0`
  - Flash: large savings (WebUI bundle is typically the biggest single chunk).
  - RAM: small savings (less static data kept by the UI).
  - Behavior: `/` serves a tiny stub page; REST API stays available.

- `CM_ENABLE_OTA=0`
  - Flash/RAM: medium savings (removes OTA routes and handler code).
  - Behavior: OTA upload endpoint not available.

- `CM_DISABLE_GUI_LOGGING=1`
  - Flash/RAM: small to medium savings (removes GUI log output, buffer, and WS log payload building).
  - Behavior: Log tab disappears; only Serial logging remains.

- `CM_ENABLE_LOGGING=0`
  - Flash/RAM: small savings (removes log formatting and calls).
  - Behavior: no CM_LOG output.

- `CM_ENABLE_VERBOSE_LOGGING=0`
  - Flash/RAM: small savings (removes verbose log sites).
  - Behavior: no CM_LOG_VERBOSE output.

- `CM_ENABLE_STYLE_RULES=0`
  - Flash: small savings (style metadata processing disabled).
  - Behavior: runtime styling rules are ignored.

- `CM_ENABLE_USER_CSS=0`
  - Flash: small savings (removes custom CSS endpoint).
  - Behavior: `/user_theme.css` no longer served.

- `CM_ENABLE_THEMING=0`
  - Flash: small savings (removes theming CSS support).
  - Behavior: theme toggle not available.


## OTA security notes (non-size related)

OTA is convenient but has security implications:

- OTA uses plain HTTP. There is no TLS. Anyone on the same network can observe traffic.
- Use a strong OTA password and do not reuse it elsewhere.
- Prefer OTA only on trusted LAN/VPN. Avoid open WiFi.
- If you do not need OTA in production, disable it (`CM_ENABLE_OTA=0`).
- Settings UI password and OTA password are separate; set both if you expose OTA.
