# WiFi

This document collects WiFi-related usage patterns and best practices for ConfigurationsManager.

## Topics

- DHCP vs static IP startup
- AP mode fallback
- Reconnect strategy
- Smart WiFi Roaming (mesh / multi-AP)

## DHCP vs Static IP

ConfigurationsManager provides overloads to start WiFi + web server:

- DHCP:
  - `startWebServer(ssid, password)`
- Static IP:
  - `startWebServer(ip, gateway, subnet, ssid, password)`

Recommended pattern:

1. Load settings from NVS (`loadAll()`)
2. Decide DHCP vs static based on a boolean setting
3. Start the web server using the appropriate overload

If you keep network settings in your own struct, prefer `OptionGroup` and `showIfFalse(useDhcp)` for static IP fields.

## AP Mode fallback

If no SSID is configured, starting an AP is a common fallback.

Typical flow:

- if SSID is empty → `startAccessPoint()`
- if the device runs in AP mode → skip normal web server startup

## Reconnect strategy

If WiFi is lost:

- call `reconnectWifi()`
- wait briefly
- re-initialize OTA (if you rely on ArduinoOTA)

Important notes:

- Avoid tight reconnect loops.
- Prefer backoff / cooldown logic if your environment has unstable WiFi.

## Smart WiFi Roaming

Smart WiFi Roaming is documented in its own file:

- `docs/SMART_ROAMING.md`

That document covers:

- how roaming decisions are made
- how to tune thresholds/cooldowns
- how to disable roaming

## ADC2 warning (ESP32)

On classic ESP32 (Arduino), ADC2 pins are typically not usable for `analogRead()` while WiFi is active.

If you use analog inputs in parallel with WiFi:

- prefer ADC1 pins (GPIO32–39)
- see also `docs/esp.md` and `docs/IO-AnalogInputs.md`
