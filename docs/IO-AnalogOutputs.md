# IO Analog Outputs (DAC)

This document describes the **IOManager analog output** feature.

## Overview

- Analog outputs in this project are **DAC-based** (initial implementation).
- On ESP32 (WROOM/WROVER family), there are only **two hardware DAC channels**:
  - **GPIO25 = DAC1**
  - **GPIO26 = DAC2**
- Because there are only two DAC pins, you can only have **two independent physical analog outputs** at a time.

## Hardware Constraints (ESP32)

- DAC resolution is **8-bit**.
  - Raw range: **0..255**
- The project maps DAC values to a “voltage-like” range:
  - **0..255** is treated as **0..3.3 V** (approx.)
  - Voltage step is approximately $3.3/255 \approx 0.01294$ V per step.

Important behavior:

- If two IOManager analog outputs are configured to the **same GPIO (25 or 26)**, then:
  - They will **overwrite each other**.
  - The physical output voltage will always match the **last value written**.

## IOManager API

### Register an analog output

- Register one output item:
  - `addAnalogOutput({ .id, .name, .defaultPin, .valueMin, .valueMax, .reverse })`

Notes:

- `defaultPin` should be **25 or 26** (ESP32 DAC pins).
- `valueMin`/`valueMax` define the **engineering unit range** for `setValue()`/`getValue()`.
- `reverse=true` inverts the mapping.

### Set/Get in different representations

IOManager exposes three representations:

- Scaled engineering value:
  - `setValue(id, value)` / `getValue(id)`
- Raw volts-like value:
  - `setRawValue(id, volts)` / `getRawValue(id)`
  - Range is expected to be **0..3.3**
- Raw DAC value:
  - `setDACValue(id, dac)` / `getDACValue(id)`
  - Range is **0..255**

## GUI Helpers (recommended naming)

IOManager provides explicit helper methods for analog outputs:

- Slider (writes scaled value):
  - `addAnalogOutputSliderToGUI(...)`
- Read-only value display (scaled value + unit):
  - `addAnalogOutputValueToGUI(...)`
- Read-only value display (raw DAC 0..255):
  - `addAnalogOutputValueRawToGUI(...)`
- Read-only value display (volts):
  - `addAnalogOutputValueVoltToGUI(...)`

These helpers are designed to mirror the naming style of analog input helpers (e.g. `addAnalogInputToGUI`).

## Demo Notes (examples/IO-Full-Demo)

The IO-Full-Demo example demonstrates analog outputs.

Key points:

- The demo shows sliders plus additional readouts (scaled value, DAC value, volts).
- The demo intentionally keeps the number of active analog outputs aligned with the **two physical DAC pins**.
  - Additional mapping modes can be enabled, but must not share pins unless you explicitly accept overwrites.

## Troubleshooting

### Slider value “jumps back” in UI

Symptoms:

- You move a slider, hardware output changes, but UI snaps back to a previous/default value.

Typical cause:

- The runtime group that contains the slider does not exist in `runtime.json` (no provider), so the frontend can’t read back current state.

Fix:

- Ensure runtime values include interactive controls for groups even without explicit providers.

### Output voltage does not match expectation

Checklist:

- Verify which physical DAC pin is used (25 vs 26).
- Ensure you are not writing multiple outputs to the same pin.
- Compare readouts:
  - scaled value
  - DAC 0..255
  - volts

## Recommendations

- Use **at most 2 physical DAC outputs** on ESP32.
- If you need more than 2 analog outputs:
  - Use PWM/LEDC + filtering (RC) (future extension)
  - Use an external DAC via I2C/SPI (future extension)
