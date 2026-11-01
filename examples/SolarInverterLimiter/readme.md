# ESP32 RS485 Solar Inverter Limiter

This project is an ESP32-based solar inverter limiter that can be used to limit the power output of a solar inverter to a specific value. The project uses an ESP32 microcontroller and an RS485 module to communicate with the inverter.

## Features

- ESP32-based solar inverter limiter
- RS485 communication with solar inverter
- Publishes power and sensor values via MQTT
- Configurable power output limit
- Web-based settings UI (ConfigManager)
- Optional OLED and BME280 sensor support via compile-time feature flags

## ConfigManager integration

- The example consumes the repository library through `symlink://../..` in `platformio.ini`.
- The pre-build step uses `tools/precompile_wrapper.py`.
- The wrapper delegates to the library script inside `.pio/libdeps/<env>/ESP32 Configuration Manager/tools/preCompile_script.py`.
- This keeps WebUI/header generation in sync with the installed ConfigManager library version.

## Control inputs

- PID inverter regulation uses signed E320 grid power from `tele/powerMeter/powerMeter/SENSOR` at JSON path `E320.Power_in`.
- PID base target is `solarPowerW + signedGridPowerW + offset`; positive grid power means import and negative grid power means export.
- PID logging derives `gridIn = max(signedGridPowerW, 0)` and `gridOut = max(-signedGridPowerW, 0)` for live diagnostics.
- Home Assistant should publish electricity price inputs to `SolarLimiter/Input/ElectricityPrice` and `SolarLimiter/Input/NegativePrice`.
- Negative-price handling is available in the ESP32 settings: force output to configured minimum or explicitly set output to `0` when `NegativePrice` is `1`.

Tune and verify the controller for the connected inverter and metering setup.
Compilation does not validate physical regulation, PID tuning or behavior during
negative-price periods. Inspect the selected build's flash usage before choosing
an OTA variant.

## Hardware Requirements

- ESP32 microcontroller
- RS485 module (e.g. MAX485 or similar)

## How to use

1. Build and upload with PlatformIO:
  - Build: `pio run -e usb`
  - Upload: `pio run -e usb -t upload`
2. On first boot (or when WiFi SSID is empty) the device starts an access point:
3. Connect to the AP and open the web UI (usually `http://192.168.4.1/`) to configure WiFi/MQTT and other settings.
4. After WiFi is configured the web UI will be available on the device IP in your LAN.

## Build variants

- `usb`: default local build and USB upload workflow.
- `ota`: OTA build and upload workflow for the full example.
- `ota_no_oled`: disables the OLED display feature for smaller OTA builds.
- `ota_no_oled_no_bme`: disables both OLED and BME280 so the example also builds without the I2C sensor/display stack.

When `FEATURE_BME280_ENABLED=0`, the temperature, humidity, and dewpoint runtime values and MQTT topics are omitted. When both OLED and BME280 are disabled, the example also omits the shared I2C settings page.

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

- See the examples in `docs/HomeAssistant.yaml` and `docs/HomeAssistant_Dashboard.yaml`.
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
