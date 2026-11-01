# BoilerSaver (ESP32)

This example is a larger real-world project based on the ConfigurationsManager library.
It combines runtime controls, sensor input, optional MQTT integration, and Web UI configuration.

## What it demonstrates

- Complex settings structure with grouped pages/cards
- Runtime actions and live values (`/runtime.json`, `/runtime_meta.json`)
- Optional MQTT defaults via `src/secret/secrets.h`
- Integration of external components (e.g. OneWire/DallasTemperature, OLED)

## How to run

From the ConfigurationsManager repository root. The shared `[env]` declaration
uses this enclosing checkout (`symlink://../..`), including its current work
branch, for USB, OTA and the optional test station profile:

```bash
pio run -d examples/BoilerSaver -e usb
pio run -d examples/BoilerSaver -e testesp
python -m unittest discover -s examples/BoilerSaver/test -p "test_*.py"
```

## First start / AP mode

If no SSID is configured yet, the device starts in AP mode.
Open the printed AP URL from Serial (usually `http://192.168.4.1`) and configure WiFi via the Web UI.

## Optional secrets defaults

Inside this example, copy `src/secret/secrets.example.h` to ignored
`src/secret/secrets.h` and set default WiFi/MQTT credentials. Leave the optional
access-point MAC filter empty unless you intentionally configure one.

## Additional docs

- [Wiring, profiles and validation limits](docs/BoilerSaver-esp32-connection.md)
- [Home Assistant entities](docs/HomeAssistant.yaml)
- [Home Assistant dashboard](docs/HomeAssistant_Dashboard.yaml)

## Source and build profiles

This maintained example snapshots the public application source of
[BoilerSaver](https://github.com/vitalyruhl/BoilerSaver) 4.1.2. Application source,
the build wrapper and its unit tests match that snapshot; CM resolves from the
enclosing checkout rather than a frozen package. The original application license notice is preserved in [LICENSE](LICENSE);
ConfigurationsManager has its own [repository license](../../LICENSE).
Common dependencies and flags are declared once in `[env]`. This nested example keeps third-party dependencies
in its own PlatformIO cache under
`${platformio.core_dir}/libdeps/ConfigurationsManager-examples/BoilerSaver`; CM
still resolves directly from this enclosing source checkout.

`usb` uses the normal wiring. `testesp` uses the operator-confirmed COM8 station
mapping and isolated test settings described in the wiring guide. A build command
does not authorize an upload. Verify the actual board, port and wiring before any
separately authorized hardware action.

## Local OTA configuration

The public OTA target is `example.invalid` and contains no upload password. For an
authorized target, copy `platformio.local.ini.example` to ignored
`platformio.local.ini` and provide `BOILERSAVER_OTA_HOST` and
`BOILERSAVER_OTA_PASSWORD` in your local environment. PlatformIO reads that
optional local file through `extra_configs`. The password must match the device;
never put it or a production device address in public configuration.

The standalone five-minute COM8 observation was reset-free but recorded five
HTTP timeouts. This example copy has not been hardware-tested. See the
[wiring guide](docs/BoilerSaver-esp32-connection.md) for the precise coverage limits;
the historical production panic is not established as fixed.
