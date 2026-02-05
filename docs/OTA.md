# OTA

This document describes OTA update support and the Web UI firmware flashing workflow.

## Web UI flashing

The embedded single-page app exposes a `Flash` action beside the `Settings` tab.

Workflow:

1. Enable `Allow OTA Updates` under **System** and set an `OTA Password`.
   - Leave empty to allow unauthenticated uploads.
2. Click **Flash** and select the compiled `.bin` (or `.bin.gz`) produced by PlatformIO.
3. Enter the OTA password when prompted.
   - The SPA sends it as an `X-OTA-PASSWORD` header.
4. Watch progress notifications.
   - On success, the device reboots automatically.

## Backend behavior

- The backend remains asynchronous (`ESPAsyncWebServer`).
- The `/ota_update` handler streams chunks into the Arduino `Update` API while performing password checks.

Uploads are rejected when:

- OTA is disabled
- the password is missing/incorrect
- the upload fails integrity checks

## Runtime flags used by the Web UI

The runtime JSON includes OTA status information used by the Web UI:

- `runtime.system.allowOTA`: true when OTA is enabled on the device (HTTP endpoint ready)
- `runtime.system.otaActive`: true after `ArduinoOTA.begin()` has run (informational)

See also `docs/GUI-Runtime.md`.

## Notes

- ConfigurationsManager serves the UI/API over plain HTTP only.
- If you need transport security, provide it externally (VPN, trusted WiFi only, or a TLS reverse proxy that terminates HTTPS).
- See `docs/SECURITY.md` for details.
