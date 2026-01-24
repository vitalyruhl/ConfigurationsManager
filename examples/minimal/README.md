# CM Minimal Demo (minimal)

This example is the smallest recommended starting point for the library.

## What it demonstrates

- Minimal ConfigManager setup (logging, app metadata)
- Core settings templates: WiFi + System + NTP via `cm::CoreSettings`
- Settings-driven startup via `ConfigManager.startWebServer()` (DHCP/static/AP fallback)
- WiFi hook lifecycle via `cm::CoreWiFiServices` (OTA init + NTP sync ticker)

## How to run

From the repo root:

```bash
pio run -d examples/minimal -e usb
pio run -d examples/minimal -e usb -t upload
```

## First start / AP mode

If no SSID is configured yet, the device starts in AP mode.
Open the printed AP URL from Serial (192.168.4.1) and configure WiFi via the Web UI.

## Screenshot

![CM Minimal Demo Web UI](CM-Minimal-Demo-V3.3.0.jpg)
