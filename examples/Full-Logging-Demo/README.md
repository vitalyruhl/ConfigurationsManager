# CM Full Logging Demo (Full-Logging-Demo)

This example focuses on the logging module and runtime log visibility in the Web UI.

## What it demonstrates

- Core settings templates: WiFi + System + NTP via `cm::CoreSettings`
- Settings-driven startup via `ConfigManager.startWebServer()` (DHCP/static/AP fallback)
- Logging output levels and centralized logging callbacks
- Runtime log stream visible in GUI (when logging is enabled)

## How to run

From the repo root:

```bash
pio run -d examples/Full-Logging-Demo -e usb
pio run -d examples/Full-Logging-Demo -e usb -t upload
```

## First start / AP mode

If no SSID is configured yet, the device starts in AP mode.
Open the printed AP URL from Serial (usually `http://192.168.4.1`) and configure WiFi via the Web UI.

## Optional secrets defaults

You can copy `src/secret/secrets.example.h` to `src/secret/secrets.h` and set default WiFi credentials for faster startup.
