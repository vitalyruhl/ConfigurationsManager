# ESP32 - Boiler Saver

This project is a simple ESP32-based boiler saver that can be used to limit the power output of a
boiler uses a Relay, that decrease temperature by add a parallel resistor to the given temperature sensor. 
The project uses an ESP32 microcontroller, a current sensor, and a relay module to control the power output of the boiler.

## Features

- not used

## Hardware Requirements

- ESP32 microcontroller


## Wiring Diagram (Not ready yet!)

- see Wokwi for a wiring example

## Home Assistant Integration (Not ready yet!)

- Add the following to your configuration.yaml file:

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
