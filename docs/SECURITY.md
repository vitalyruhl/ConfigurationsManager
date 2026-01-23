# Security Notes (v3.0.0)

## Overview

ConfigurationsManager serves its Web UI and API over plain HTTP by default.

That means:

- Password fields are **masked** in the UI (displayed as `***`).
- When a user **sets or changes** a password in the WebUI, it is transmitted to the ESP32 **in cleartext over HTTP**.

## What is protected

- UI masking prevents accidental shoulder-surfing and avoids leaking stored secrets through normal UI rendering.

## What is NOT protected

- **Transport security**: HTTP traffic can be sniffed/modified on untrusted networks.
- **Storage security**: passwords are stored in **plaintext** in ESP32 NVS/Preferences (so the device can actually use them).
- **Physical access**: anyone with physical access to the device can potentially extract stored values.

## HTTPS / TLS

At the moment, ConfigurationsManager does **not** provide an on-device HTTPS server.
The built-in Web UI and API are served via plain HTTP.

If you need HTTPS/TLS, the recommended approach is to terminate TLS externally (e.g. a reverse proxy, VPN, or other trusted network boundary) and forward requests to the device over HTTP.

## Recommendations

- Use the UI only on a trusted network.
- If you need TLS, provide it externally (e.g. VPN, WiFi isolation + trusted clients, or a reverse proxy that terminates HTTPS).
- Treat OTA/MQTT passwords as sensitive and keep them unique per device/project.

## Related Documentation

- [Settings Configuration](SETTINGS.md)
- [Feature Flags](FEATURE_FLAGS.md)
- [Smart WiFi Roaming](SMART_ROAMING.md)
