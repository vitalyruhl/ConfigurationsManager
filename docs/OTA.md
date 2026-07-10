# OTA

This document describes OTA update support and the Web UI firmware flashing workflow.

OTA requires an IP connection but is not tied to WiFi. Call
`ConfigManager.setupOTA()` after WiFi or Ethernet has received an IP address.

## Web UI flashing

The embedded single-page app exposes a `Flash` action beside the `Settings` tab.

Workflow:

1. Enable `Allow OTA Updates` under **System** and set an `OTA Password`.
   - Leave empty to allow unauthenticated uploads.
2. Click **Flash** and select the compiled `.bin` (or `.bin.gz`) produced by PlatformIO.
3. Enter the OTA password when prompted.
   - The SPA sends it as an `X-OTA-PASSWORD` header.
4. Watch the upload progress popup.
   - The popup stays visible while the browser is actively uploading the firmware and warns before accidental page navigation.
   - On success, the device reboots automatically.

## Backend behavior

- The backend remains asynchronous (`ESPAsyncWebServer`).
- The `/ota_update` handler streams chunks into the Arduino `Update` API while performing password checks.

## PlatformIO ArduinoOTA uploads

An `espota` environment can upload over WiFi or Ethernet:

```ini
[env:ota]
upload_protocol = espota
upload_port = 192.168.2.127
upload_flags =
    -a
    ota-password
    -t
    30
```

`-a` supplies the ArduinoOTA password and `-t` sets the timeout in seconds.
Keep the password synchronized with the firmware setting. Do not specify both
`-a` and `--auth` for the same upload.

To remove OTA support from a firmware build:

```ini
[env:noota]
board_build.partitions = no_ota.csv
build_flags =
    ${env.build_flags}
    -DCM_ENABLE_OTA=0
```

The build flag removes ConfigurationsManager OTA code. The partition setting is
separate and reclaims the second OTA application slot.

Uploads are rejected when:

- OTA is disabled
- the password is missing/incorrect
- the upload fails integrity checks

## Runtime flags used by the Web UI

The runtime JSON includes OTA status information used by the Web UI:

- `runtime.system.otaActive`: true while an ArduinoOTA or WebUI firmware upload is active
- `runtime.system.otaHasPassword`: true when an OTA password is configured

See also `docs/GUI-Runtime.md`.

## Notes

- ConfigurationsManager serves the UI/API over plain HTTP only.
- If you need transport security, provide it externally (VPN, trusted LAN, or a TLS reverse proxy that terminates HTTPS).
- See `docs/SECURITY.md` for details.

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.setupOTA` | `setupOTA(const String& hostname, const String& password = "")` | Initializes ArduinoOTA endpoint and callbacks. | Call after the active IP interface is ready. |
| `ConfigManager.handleOTA` | `handleOTA()` | Processes OTA runtime events. | Must run regularly in `loop()`. |
| `ConfigManager.enableOTA` | `enableOTA(bool enabled = true)` | Enables/disables OTA handling at runtime. | Runtime control for OTA availability. |

