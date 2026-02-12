# IO Analog Outputs (IOManager)

This document describes how **analog outputs** work in `cm::IOManager`.

## Overview

`IOManager` analog outputs are settings-driven and provide:

- DAC pin configuration via Settings (GPIO)
- Optional value mapping (engineering units -> volts -> DAC)
- Runtime UI controls (slider)
- Runtime UI readouts (scaled value, volts, DAC 0..255)

Note: the initial implementation is **DAC-only**. PWM/LEDC is a planned follow-up.

## Creating an Analog Output

Example (scaled 0..100% mapped to 0..3.3V):

```cpp
ioManager.addAnalogOutput(cm::IOManager::AnalogOutputBinding{
    .id = "dac1",
    .name = "DAC 1",
    .defaultPin = 25,
    .defaultEnabled = true,
    .valueMin = 0.0f,
    .valueMax = 100.0f,
    .reverse = false,
});
```

## Settings (GPIO)

Analog outputs use short, slot-based keys (ESP32 Preferences safe).

### Key format (ESP32 NVS safe)

To keep keys short (ESP32 Preferences limit), IOManager uses **slot-based keys**:

- Analog outputs: `AO%02uX` (e.g. `AO00P`)

Suffix meanings for analog outputs:

- `P` = pin

Important: slot-based keys depend on the order of `addAnalogOutput(...)` calls.
If you reorder outputs in code, previously stored pins may appear to "move" to another output.

## Set/Get in different representations

IOManager exposes three representations:

- Scaled engineering value:
  - `setValue(id, value)` / `getValue(id)`
  - Range: `valueMin..valueMax`
- Volts-like value:
  - `setRawValue(id, volts)` / `getRawValue(id)`
  - Range: `0..3.3`
- Raw DAC value:
  - `setDACValue(id, dac)` / `getDACValue(id)`
  - Range: `0..255`

## Runtime controls and readouts (UI)

Slider (writes scaled value via `setValue/getValue`):

```cpp
ioManager.addAnalogOutputToSettingsGroup("dac1", "Analog - I/O", "Analog Outputs", "DAC 1", 10);
ioManager.addAnalogOutputToLive(
    "dac1",
    10,
    0.0f,
    100.0f,
    0,
    "AO",
    "Analog Outputs",
    "controls",
    "DAC 1",
    "%"
);
```

Read-only value displays:

```cpp
ioManager.addAnalogOutputValueToGUI("dac1", nullptr, 11, "DAC 1", "controls", "%", 0);
ioManager.addAnalogOutputValueVoltToGUI("dac1", nullptr, 12, "DAC 1 (V)", "controls", 3);
ioManager.addAnalogOutputValueRawToGUI("dac1", nullptr, 13, "DAC 1 (0..255)", "controls");
```

## Hardware constraints (ESP32)

- On ESP32 (WROOM/WROVER family), there are only **two hardware DAC channels**:
  - **GPIO25 = DAC1**
  - **GPIO26 = DAC2**
- DAC resolution is **8-bit**:
  - Raw range: **0..255**
  - Voltage step is approximately $3.3/255 \approx 0.01294$ V per step

Important behavior:

- If two IOManager analog outputs are configured to the **same GPIO (25 or 26)**, they will overwrite each other.
  - The physical output voltage will always match the **last value written**.

## Lifecycle

Typical sketch order:

1. `addAnalogOutput(...)` / `addAnalogOutputToSettingsGroup(...)` / optional `addAnalogOutputToLive(...)` / optional value readouts
2. `ConfigManager.loadAll()` (loads persisted pin)
3. `ioManager.begin()`
4. In `loop()`: `ioManager.update()` continuously applies desired output values

## Recommendations

- Use **at most 2 physical DAC outputs** on ESP32.
- If you need more than 2 analog outputs:
  - Use PWM/LEDC + filtering (RC) (planned follow-up)
  - Use an external DAC via I2C/SPI (planned follow-up)

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `cm::IOManager::addAnalogOutputToSettingsGroup` | `addAnalogOutputToSettingsGroup(..., const char* pageName, ..., const char* groupName, int order)` | Places analog output settings into Settings UI layout. | Supports page/card/group variants. |
| `cm::IOManager::addAnalogOutputToLive` | `addAnalogOutputToLive(const char* id, int order, float sliderMin, float sliderMax, int sliderPrecision, const char* pageName, const char* cardName, const char* groupName, const char* labelOverride = nullptr, const char* unit = nullptr)` | Adds analog output slider control in Live UI. | Uses scaled engineering value range. |
| `cm::IOManager::addAnalogOutputValueToGUI` | `addAnalogOutputValueToGUI(const char* id, const char* cardName, int order, const char* runtimeLabel = nullptr, const char* runtimeGroup = nullptr, const char* unit = nullptr, int precision = 1)` | Adds scaled read-only value field to Live UI. | Display helper for operator feedback. |
| `cm::IOManager::addAnalogOutputValueVoltToGUI` / `addAnalogOutputValueRawToGUI` | `addAnalogOutputValueVoltToGUI(...)`<br>`addAnalogOutputValueRawToGUI(...)` | Adds read-only voltage or raw DAC value fields. | Useful for diagnostics/calibration. |

