# BoilerSaver (ESP32)

This example is a larger real-world project based on the ConfigurationsManager library.
It combines runtime controls, sensor input, optional MQTT integration, and Web UI configuration.

## What it demonstrates

- Complex settings structure with grouped pages/cards
- Runtime actions and live values (`/runtime.json`, `/runtime_meta.json`)
- Optional MQTT defaults via `src/secret/secrets.h`
- Integration of external components (e.g. OneWire/DallasTemperature, OLED)

## How to run

From the repo root:

```bash
pio run -d examples/BoilerSaver -e usb
pio run -d examples/BoilerSaver -e usb -t upload
```

## First start / AP mode

If no SSID is configured yet, the device starts in AP mode.
Open the printed AP URL from Serial (usually `http://192.168.4.1`) and configure WiFi via the Web UI.

## Optional secrets defaults

You can copy `src/secret/secrets.example.h` to `src/secret/secrets.h` and set default WiFi/MQTT credentials.

## Additional docs

- Wiring and integration notes: `examples/BoilerSaver/docs/`
