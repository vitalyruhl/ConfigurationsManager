# IO Digital Outputs (IOManager)

This document describes how **digital outputs** work in `cm::IOManager`.

## Overview

`IOManager` digital outputs are settings-driven and provide:

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

Place settings separately, then register runtime controls via `addDigitalOutputToLive(...)`.

Checkbox (toggle):

```cpp
ioManager.addDigitalOutputToSettingsGroup("relay27", "Digital - I/O", "Digital Outputs", "Relay 27", 5);
ioManager.addDigitalOutputToLive(
    cm::IOManager::RuntimeControlType::Checkbox,
    "relay27",
    5,
    "DO",
    "Digital Outputs",
    "controls",
    "Relay 27"
).onChangeCallback([](bool v){ /* ... */ });
```

Momentary button (press=true, release=false):

```cpp
ioManager.addDigitalOutputToSettingsGroup("holdbutton", "Digital - I/O", "Digital Outputs", "Holdbutton", 4);
ioManager.addDigitalOutputToLive(
    cm::IOManager::RuntimeControlType::MomentaryButton,
    "holdbutton",
    4,
    "DO",
    "Digital Outputs",
    "controls",
    "Holdbutton",
    "Running",
    "Push"
).onChangeCallback([](bool v){ /* ... */ });
```

## Notes

- Inputs and outputs can share the same runtime group (`"inputs"`, `"controls"`, etc.).
- Use short IDs and keep the number of IO items stable to avoid slot key drift.
- If you need stable persisted mapping independent of add-order, the next step is to switch from slot keys to ID-based keys (requires migration strategy).

## Lifecycle

Typical sketch order:

1. `addDigitalOutput(...)` / `addDigitalOutputToSettingsGroup(...)` / `addDigitalOutputToLive(...)`
2. `ConfigManager.loadAll()` (loads persisted pins/polarity)
3. `ioManager.begin()` (applies `pinMode(...)` and initializes states)
4. In `loop()`: `ioManager.update()` continuously applies desired output states
