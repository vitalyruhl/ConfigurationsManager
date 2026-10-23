# Feature Flags / Build Options

## v3.0.0 change

Starting with **v3.0.0**, the library no longer requires a list of `CM_ENABLE_*` build flags or PlatformIO `extra_scripts` to build.

The default embedded UI is supplied with the library. Selecting a different
Live/Settings combination requires the explicit regeneration hook described
below; `library.json` does not install that hook automatically.

- WebUI, OTA, runtime controls, WebSocket push, theming/styling are compiled in by default.
- **Supported build-time options**:
  - `CM_EMBED_WEBUI` (default: `1`)
    - `1`: embed the WebUI HTML into the firmware image (serves on `/`)
    - `0`: do not embed the WebUI HTML (saves flash); enabled API routes stay available
  - `CM_ENABLE_LIVE_UI` (default: `1`, accepts `0` or `1`)
    - `0`: compile out Live UI, runtime providers/metadata, Live layout and runtime JSON/control routes
  - `CM_ENABLE_SETTINGS_UI` (default: `1`, accepts `0` or `1`)
    - `0`: compile out Settings UI, presentation/layout data and configuration read/write/password-reveal routes
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

## Live and Settings combinations (4.9.0 Unreleased)

Both switches default to `1`; unsupported values produce a compile error.
They remove the corresponding backend and frontend paths at build time, including
UI models, maps, registration storage and JSON serialization. They do not merely
hide navigation buttons.

| `CM_ENABLE_LIVE_UI` | `CM_ENABLE_SETTINGS_UI` | Available interface | WebSocket behavior |
|---|---|---|---|
| `1` | `1` | Live, Settings and Flash | Runtime subscriptions and GUI events |
| `0` | `1` | Settings and Flash | GUI events only; no runtime subscriptions |
| `1` | `0` | Live and Flash | Runtime subscriptions and GUI events |
| `0` | `0` | Minimal identity, authentication, OTA status and Flash | No WebSocket |

Flash/upload availability still follows `CM_ENABLE_OTA`, runtime OTA enablement
and authentication. With `CM_EMBED_WEBUI=0`, no interface is embedded; the same
Live/Settings switches still control which backend APIs exist.

Define both switches globally in the consumer environment's `build_flags`, so
the same values reach the application and every CM translation unit. A `#define`
in the sketch alone is insufficient and can create inconsistent class layouts
or a frontend/backend mismatch.

For example, keep Settings and Flash while removing Live. This consumer example
uses a project-local `tools/precompile_wrapper.py`:

```ini
build_flags =
    -DCM_ENABLE_LIVE_UI=0
    -DCM_ENABLE_SETTINGS_UI=1
extra_scripts =
    pre:tools/precompile_wrapper.py
```

Copy the supplied `tools/precompile_wrapper_template.py` into that consumer path
and make its `lib_path` point to the actual installed CM directory if a custom
`libdeps_dir` or local dependency is used. The wrapper runs the library's
`tools/preCompile_script.py` before firmware compilation, with the selected
compiler flags. Repository examples inside the CM checkout can register that
script directly, as `examples/minimal/platformio.ini` does. Retain any other
project build flags and hooks when adding these entries.

### Retained application behavior

Disabling Settings preserves registered operational configuration: keys/types,
values/defaults, NVS load/save, programmatic access, validators, callbacks and
MQTT use. Only GUI presentation/layout state and web configuration serialization
are removed. UI-placement/registration calls remain source-compatible no-ops for
the disabled area; they do not retain its CM-owned UI data.

Disabling Live prevents runtime provider evaluation and removes its metadata,
layout and web control registration storage. Operational alarm state, locking,
logging and application callbacks remain; disabling both areas does not imply
zero retained runtime memory. The flags do not disable the underlying application
or its device controls outside these web interfaces. Alarm diagnostics still
follow `CM_ENABLE_LOGGING`; disabling Live does not independently silence them.

### Routes, authentication and OTA status

- Live disabled: `/live_pages.json`, `/live_layout.json`, `/runtime.json`,
  `/runtime_meta.json` and runtime-action aliases are not registered.
- Settings disabled: `/config_raw`, `/config.json`, configuration apply/save/reset
  routes, `/config/password`, `/config/settings_password` and `/dev/export` are
  not registered. Configuration values and stored passwords cannot be fetched
  through those removed routes.
- `/config/auth` remains available for Flash authentication and issues a
  short-lived token. It does not reveal passwords. Settings authentication and
  the OTA upload password remain separate; with Settings disabled, Flash asks
  for the OTA password directly rather than fetching it from configuration.
- `/ui_capabilities` returns only `live`, `settings`, `otaEnabled`, `otaActive`,
  `otaHasPassword` and `settingsAuthRequired`, with `Cache-Control: no-store`.
  It reads OTA/authentication state directly, without running Live providers or
  configuration serializers, and returns no secret values.
- Shared identity/static routes and the existing reboot/OTA controls remain
  subject to their own enablement and security checks. `/gui/action` and generic
  GUI notifications remain when either Live or Settings is enabled; both are
  removed when both switches are `0`.

### Matching the embedded frontend

The precompile wrappers pass the compiler flags into Vue/Vite as
`VITE_CM_ENABLE_LIVE_UI` and `VITE_CM_ENABLE_SETTINGS_UI`. Disabled component
graphs and their JS/CSS are excluded from the generated embedded gzip. Use the
same values when building the frontend separately.

The `.pio/cm-webui-build.json` cache depends on the resolved feature values,
frontend/generator inputs and generated-header SHA-256. Changing a mode or its
source inputs regenerates the embedded page. The generated header records both
UI flags and rejects a compiler/bundle mismatch when embedding is enabled.
`CM_EMBED_WEBUI` remains an independent switch. Compare actual firmware/assets
for the intended target before quoting savings; no fixed flash/RAM reduction is
guaranteed for these combinations.

## Optional BLE transport

BLE is opt-in through `bluetooth/BluetoothManager.h`; it does not add NimBLE to
core consumers. `CM_BLE_TX_MAX_BYTES` defaults to 272 and accepts 1..272 for
outgoing commands. Apply the same value to every translation unit that includes
BLE headers; receive events keep their 272-byte capacity. See
[Bluetooth](BLUETOOTH.md) for dependency pinning, queue sizes and consumer setup.

## Size impact (rough guidance)

These are rough estimates. Exact savings depend on compiler, LTO, and what your firmware actually uses.
For real numbers, build your target with and without each flag and compare the final `.bin` size.

- `CM_EMBED_WEBUI=0`
  - Flash: large savings (WebUI bundle is typically the biggest single chunk).
  - RAM: small savings (less static data kept by the UI).
  - Behavior: `/` serves a tiny stub page; enabled REST API routes stay available.

- `CM_ENABLE_OTA=0`
  - Flash/RAM: medium savings (removes OTA routes and handler code).
  - Behavior: OTA upload endpoint not available.
  - Partitioning: use a no-OTA partition table separately when the application should reclaim the second OTA slot (the WT32 example uses `no_ota.csv`).

- `CM_ENABLE_WIFI=0`
  - Flash/RAM: removes ConfigManager WiFi manager, AP, roaming, captive-portal, and WiFi runtime paths.
  - Behavior: the sketch must initialize its network interface and call `ConfigManager.startWebServerOnNetwork()` after it receives an IP address.
  - Framework note: Ethernet event handling in Arduino-ESP32 2.x still uses generic APIs exposed through `WiFi.h`; this does not start the WiFi radio.
  - Example: see `examples/WT32-ETH01-v1.4` and `docs/ETHERNET.md`.

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

- WebUI OTA uses plain HTTP. There is no TLS. Anyone on the same network can observe traffic.
- Use a strong OTA password and do not reuse it elsewhere.
- Prefer OTA only on a trusted LAN/VPN, whether the device uses WiFi or Ethernet.
- If you do not need OTA in production, disable it (`CM_ENABLE_OTA=0`).
- Settings UI password and OTA password are separate; set both if you expose OTA.

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `startWebServerOnNetwork` | `()` | Starts ConfigManager services on a network interface initialized by the sketch. | Use after Ethernet or another external interface has an IP address. |

