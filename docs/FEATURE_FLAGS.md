# Feature Flags / Build Options

## v3.0.0 change

Starting with **v3.0.0**, the library no longer requires a list of `CM_ENABLE_*` build flags or PlatformIO `extra_scripts` to build.

- WebUI, OTA, runtime controls, WebSocket push, theming/styling are compiled in by default.
- **Supported build-time options**:
  - `CM_EMBED_WEBUI` (default: `1`)
    - `1`: embed the WebUI HTML into the firmware image (serves on `/`)
    - `0`: do not embed the WebUI HTML (saves flash); API routes stay available
  - `CM_ENABLE_LOGGING` (default: `0`, core/library logs only)
  - `CM_ENABLE_VERBOSE_LOGGING` (default: `0`)
  - `CM_DISABLE_GUI_LOGGING` (default: `0`)
  - `CM_LOGGING_LEVEL` (default: `CM_LOG_LEVEL_TRACE`, runtime/project logging cap)
  - `CM_ENABLE_WIFI` (default: `1`)
    - `1`: include ConfigManager WiFi connection, AP, and roaming support
    - `0`: compile out ConfigManager WiFi support for externally managed networks such as Ethernet
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

- `CM_ENABLE_WIFI=0`
  - Flash/RAM: removes ConfigManager WiFi manager, AP, roaming, captive-portal, and WiFi runtime paths.
  - Behavior: the sketch must initialize its network interface and call `ConfigManager.startWebServerOnNetwork()` after it receives an IP address.
  - Framework note: Ethernet event handling in Arduino-ESP32 2.x still uses generic APIs exposed through `WiFi.h`; this does not start the WiFi radio.

- `CM_DISABLE_GUI_LOGGING=1`
  - Flash/RAM: small to medium savings (removes GUI log output, buffer, and WS log payload building).
  - Behavior: Log tab disappears; only Serial logging remains.

- `CM_ENABLE_LOGGING=0`
  - Flash/RAM: small savings (removes library/core log formatting and calls).
  - Behavior: core/library `CM_LOG` callsites are compiled out.

- `CM_ENABLE_VERBOSE_LOGGING=0`
  - Flash/RAM: small savings (removes core/library verbose log sites).
  - Behavior: core/library `CM_LOG_VERBOSE` callsites are compiled out.

- `CM_LOGGING_LEVEL=CM_LOG_LEVEL_*`
  - Flash/RAM: negligible impact.
  - Behavior: caps LoggingManager runtime output level (project logging behavior).

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

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `startWebServerOnNetwork` | `()` | Starts ConfigManager services on a network interface initialized by the sketch. | Use after Ethernet or another external interface has an IP address. |

