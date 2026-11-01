# ESP32 RS485 Solar Inverter Limiter

This project is an ESP32-based solar inverter limiter that can be used to limit the power output of a solar inverter to a specific value. The project uses an ESP32 microcontroller and an RS485 module to communicate with the inverter.

The bundled SolarInverterLimiter application retains its [MIT license](LICENSE).

## Features

- ESP32-based solar inverter limiter
- RS485 communication with solar inverter
- Four persisted operating modes: automatic, household following, charge priority, and measured-PV following
- Receives battery/BMS and MPPT charger telemetry from SolarBatteryTelemetry (SBT) via MQTT
- Publishes power, planning, protection, mode, and runtime diagnostics via MQTT
- Configurable power output limit
- Embedded web-based settings UI (ConfigManager); Live UI is disabled in the shared build profile
- Optional OLED and BME280 sensor support via compile-time feature flags

## ConfigManager integration

- Build from this example directory inside the ConfigurationsManager checkout.
- `symlink://../..` selects exactly that enclosing library checkout; no registry or frozen CM package is used.
- PlatformIO keeps installed dependencies in its per-example core cache (`libdeps/ConfigurationsManager-examples/SolarInverterLimiter`), outside the enclosing library tree. CM itself remains a source symlink to this checkout.
- `tools/precompile_wrapper.py` resolves the source checkout before PlatformIO installs dependencies and runs its `tools/preCompile_script.py` directly. It never copies or replaces CM source trees.
- The wrapper forwards Settings-only feature flags (`CM_ENABLE_LIVE_UI=0`, `CM_ENABLE_SETTINGS_UI=1`, `CM_EMBED_WEBUI=1`) to the WebUI build.
- The shared hook generates the enclosing checkout's `src/html_content.h` and WebUI output. Builds of examples with different UI flags must run sequentially.
- The example pins `ESP32Async/ESPAsyncWebServer@3.12.1` and `ESP32Async/AsyncTCP@3.5.0`, matching the library transport dependencies.
- `src/main.cpp` owns setup, the control loop, MQTT topics, and RS485 commands. `src/control/` owns planning and mode policies; `src/sbt/` validates SBT input and protection state; `src/runtime/` provides compact metadata and progress diagnostics.

## Control inputs

- PID inverter regulation uses signed E320 grid power from `tele/powerMeter/powerMeter/SENSOR` at JSON path `E320.Power_in`.
- Automatic PID base target is fresh measured inverter AC power plus signed grid power; positive grid power means import and negative grid power means export. Input correction is applied once at the final output boundary.
- Control diagnostics report grid import and export separately.
- Home Assistant should publish electricity price inputs to `SolarLimiter/Input/ElectricityPrice` and `SolarLimiter/Input/NegativePrice`.
- Negative-price handling is available in the ESP32 settings: force output to configured minimum or explicitly set output to `0` when `NegativePrice` is `1`.
- Configure the SBT input root (`SBTInputRoot`) in `MQTT-Topics` to match the SBT publisher. SIL validates source sessions, sequence/freshness, battery protection, and charger state; missing or stale feedback is not proof of physical battery isolation.
- Price and PV forecast input topics are configurable in the Limiter settings. Automatic mode combines validated measurements, reserve limits, forecasts, and economic planning.
- The existing emergency-OFF request path to SBT remains active for protection events. Selecting an operating mode does not change MOS ownership or protection thresholds.

## Operating modes

Select `opMode` in the Limiter settings. `opChargeSoc` and `opPvShare` configure charge priority. Confirmed mode settings are persisted together in the application's `SILModes` NVS namespace.

| Mode | Behavior |
| --- | --- |
| `automatic` | Economic controller with PID, forecasts, reserve limits, and output correction. Missing BMS feedback holds output. |
| `household_follow` | Follows measured household demand, with output correction applied once and a configured maximum. Explicit selection permits unverified BMS feedback and reports `manual_bms_unverified`. |
| `charge_priority` | Holds output below the charge threshold (default 95%). Above it, the configured PV share may supply the inverter; the default share is 0%. Reaching 100% latches household following until SOC falls below the threshold. Missing SOC holds output. |
| `pv_follow` | Caps corrected household demand at 90% of fresh measured charger DC power. It does not spend a stored-battery budget or forecast credit; missing or invalid charger power produces zero output. |

Every mode still requires fresh grid and actual inverter measurements, valid settings, enabled inverter/transport, and healthy applicable protection gates. Household following and charge priority retain connected-battery discharge permission and reserve hysteresis. A physical MOS-OFF state must be confirmed by fresh feedback; missing telemetry does not establish isolation. Manual-mode STOP is not overridden by configured minimum output or negative-price settings.

MQTT mode topics derive from the configured publish root (shown here as `SolarLimiter`):

- `SolarLimiter/Control/Mode`: retained confirmed enum.
- `SolarLimiter/Control/ModeStatus`: retained JSON with configured/effective mode, reason, storage result, charge threshold, and inverter share.
- `SolarLimiter/Control/Mode/set`: non-retained command such as `{"mode":"household_follow","expires_at":1800000030}`. Replace the illustrative expiry with a UTC timestamp in the future, at most 60 seconds ahead. Commands require a synchronized clock and are checked again at the loop boundary.

Tune and verify the controller for the connected inverter and metering setup.
Compilation does not validate physical regulation, PID tuning or behavior during
negative-price periods. Inspect the selected build's flash usage before choosing
an OTA variant.

## Hardware Requirements

- ESP32 microcontroller
- RS485 module (e.g. MAX485 or similar)

## How to use

1. Open a terminal in `examples/SolarInverterLimiter` within the CM checkout.
2. Build without uploading: `pio run -e usb`.
3. Optional local defaults: copy `src/secret/secrets.example.h` to the ignored `src/secret/secrets.h` and replace placeholders. Existing saved device settings take precedence. WiFi and MQTT can also be configured through the settings UI.
4. For an intended USB upload, select the actual port: `pio run -e usb -t upload --upload-port COM_PORT`.
5. On first boot, or when no WiFi SSID is configured, connect to the device AP and open the web UI (usually `http://192.168.4.1/`). After WiFi configuration, use the device's LAN address.
6. Before an intended OTA upload, supply the actual device host with `--upload-port` and configure matching authentication in `upload_flags` when a local `OTA_PASSWORD` is set. The checked-in configuration contains no production host or password.

## Build variants

- `usb`: default application profile and automatic USB-port detection.
- `ota`: the same firmware with the ESP OTA uploader; `pio run -e ota` only builds.
- `ota_no_oled`: retained compatibility alias for `ota`; OLED and the MQTT logger are already disabled in the shared profile.
- `ota_no_oled_no_bme`: additionally disables BME280; build with `pio run -e ota_no_oled_no_bme`.

The shared profile retains BME280 support but suppresses its Live card. To enable OLED, change `FEATURE_OLED_DISPLAY_ENABLED` in the shared flags; the display libraries remain declared. When `FEATURE_BME280_ENABLED=0`, temperature, humidity, dewpoint runtime values, and MQTT topics are omitted. With OLED and BME280 disabled, the shared I2C settings page is omitted too.

The settings WebUI is generated in the enclosing CM checkout. Build examples sequentially and review flash usage for the selected profile before an upload. A successful build does not validate physical regulation, settings persistence on hardware, or long-term stability.

### Mock transport configuration

For a separately configured build using `SIL_MOCK_TRANSPORT`, define `SIL_MOCK_MQTT_EXPECTED_HOST` (a quoted hostname) and `SIL_MOCK_MQTT_EXPECTED_PORT` locally, for example in the ignored `src/secret/secrets.h`. The public defaults are an empty host and port `0`, so the isolation guard remains closed until an explicit broker and a valid port are configured. The existing isolated-topic and SBT-root checks still apply. These overrides do not affect the normal USB or OTA profiles.

## Wiring Diagram

- connect the RS485 module to the ESP32 microcontroller as follows:
- see Wokwi for a wiring example

```
ESP32 Pin | RS485 Pin
--------- | ---------
GPIO 19   | TXD (Transmitter)
GPIO 18   | RXD (Receiver)
GND       | GND
VCC       | VCC (5V or 3.3V depending on the RS485 module)
```

## Home Assistant Integration

- See the sensor and dashboard examples in `docs/HomeAssistant.yaml` and `docs/HomeAssistant_Dashboard.yaml`; adapt existing entity names and MQTT roots to your installation.
- Mode selection uses the `Control/Mode`, `Control/ModeStatus`, and `Control/Mode/set` contract above. Publish commands without retain and derive expiry from the current UTC time. Merge any HA additions into the existing configuration.
- Minimal MQTT sensor example (topics depend on the configured MQTT base topic):

```yaml
mqtt:
  sensor:
    - name: "SolarLimiter_SetValue"
      state_topic: "SolarLimiter/SetValue"
      unique_id: SolarLimiter_SetValue
      device_class: power
      unit_of_measurement: "W"

    - name: "SolarLimiter_GetValue"
      state_topic: "SolarLimiter/GetValue"
      unique_id: SolarLimiter_GetValue
      device_class: power
      unit_of_measurement: "W"

    - name: "SolarLimiter_Temperature"
      state_topic: "SolarLimiter/Temperature"
      unique_id: SolarLimiter_Temperature
      device_class: temperature
      unit_of_measurement: "°C"

    - name: "SolarLimiter_Humidity"
      state_topic: "SolarLimiter/Humidity"
      unique_id: SolarLimiter_Humidity
      device_class: humidity
      unit_of_measurement: "%"

    - name: "SolarLimiter_Dewpoint"
      state_topic: "SolarLimiter/Dewpoint"
      unique_id: SolarLimiter_Dewpoint
      device_class: temperature
      unit_of_measurement: "°C"

```
