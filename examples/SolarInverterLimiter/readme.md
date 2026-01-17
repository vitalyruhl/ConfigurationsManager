# ESP32 RS485 Solar Inverter Limiter

This project is an ESP32-based solar inverter limiter that can be used to limit the power output of a solar inverter to a specific value. The project uses an ESP32 microcontroller and an RS485 module to communicate with the inverter.

This project is still under development and not considered stable yet.

## Features

- ESP32-based solar inverter limiter
- RS485 communication with solar inverter
- Publishes power and sensor values via MQTT
- Configurable power output limit
- Web-based settings UI (ConfigManager)

## Hardware Requirements

- ESP32 microcontroller
- RS485 module (e.g. MAX485 or similar)

## How to use

1. Build and upload with PlatformIO:
  - Build: `pio run -e usb`
  - Upload: `pio run -e usb -t upload`
2. On first boot (or when WiFi SSID is empty) the device starts an access point:
  - SSID: `ESP32_Config`
  - Password: `config1234`
3. Connect to the AP and open the web UI (usually `http://192.168.4.1/`) to configure WiFi/MQTT and other settings.
4. After WiFi is configured the web UI will be available on the device IP in your LAN.

## Wiring Diagram

- connect the RS485 module to the ESP32 microcontroller as follows:
- see Wokwi for a wiring example

```
ESP32 Pin  | RS485 Pin
-----------|----------
  GPIO 19   | TXD (Transmitter)
  GPIO 18   | RXD (Receiver) don't use this pin, only for RXD
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
