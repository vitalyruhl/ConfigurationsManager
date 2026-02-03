# IO Digital Outputs (IOManager)

This document describes how **digital outputs** work in `cm::IOManager`.

## Overview

`IOManager` digital outputs are settings-driven and provide:

- GPIO configuration via Settings (pin, polarity, enabled)
- GPIO configuration via Settings (pin, polarity)
- A consistent API to set and read output state
- Optional runtime controls (checkbox/state button/momentary button)

For analog channels, see:

- `docs/IO-AnalogInputs.md`
- `docs/IO-AnalogOutputs.md`

## Creating an Output

Example (active-low relay):

```cpp
ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
    .id = "relay27",
    .name = "Relay 27",
    .defaultPin = 27,
    .defaultActiveLow = true,
    .defaultEnabled = true,
});
```

## Settings

Each output has settings for:

- `GPIO` (`P`): which GPIO pin to use
- `LOW-Active` (`L`): logic inversion

Outputs also have an enable concept (`defaultEnabled`), which is applied before the first preferences load.
Note: currently the enable flag is not persisted as a setting (defaults only).

### Key format (ESP32 NVS safe)

To keep keys short (ESP32 Preferences limit), IOManager uses **slot-based keys**:

- Outputs: `IO%02uX` (e.g. `IO00P`)

Suffix meanings for outputs:

- `P` = pin
- `L` = active-low

Important: slot-based keys depend on the order of `addDigitalOutput(...)` calls.
If you reorder outputs in code, previously stored pins may appear to "move" to another output.

## Applying outputs in the loop

`IOManager` applies the desired output state in `update()`:

```cpp
void loop() {
  ioManager.update();
}
```

When you call:

```cpp
ioManager.set("relay27", true);
```

the manager stores `desiredState` and ensures the GPIO is configured and written.

## Runtime controls (UI)

`IOManager::addIOtoGUI(...)` registers the output settings and can also register runtime controls.

Checkbox (toggle):

```cpp
ioManager.addIOtoGUI(
  "relay27",
  nullptr,
  5,
  cm::IOManager::RuntimeControlType::Checkbox,
  [](){ return ioManager.getState("relay27"); },
  [](bool v){ ioManager.set("relay27", v); },
  "Relay 27",
  "controls"
);
```

Momentary button (press=true, release=false):

```cpp
ioManager.addIOtoGUI(
  "holdbutton",
  nullptr,
  4,
  cm::IOManager::RuntimeControlType::MomentaryButton,
  [](){ return ioManager.getState("holdbutton"); },
  [](bool v){ ioManager.set("holdbutton", v); },
  "Holdbutton",
  "controls",
  "Running",
  "Push"
);
```

## Notes

- Inputs and outputs can share the same runtime group (`"inputs"`, `"controls"`, etc.).
- Use short IDs and keep the number of IO items stable to avoid slot key drift.
- If you need stable persisted mapping independent of add-order, the next step is to switch from slot keys to ID-based keys (requires migration strategy).

## Lifecycle

Typical sketch order:

1. `addDigitalOutput(...)` / `addIOtoGUI(...)` (settings + optional runtime control)
2. `ConfigManager.loadAll()` (loads persisted pins/polarity)
3. `ioManager.begin()` (applies `pinMode(...)` and initializes states)
4. In `loop()`: `ioManager.update()` continuously applies desired output states
