# IO Digital Inputs (IOManager)

This document describes how **digital inputs** work in `cm::IOManager`.

## Overview

`IOManager` digital inputs are settings-driven and provide:

- GPIO configuration via Settings (pin, polarity, pull mode)
- Optional non-blocking button-like events (press/release/click/double/long)
- Runtime UI indicator (bool-dot)

The input system is designed to be usable early in boot (e.g. startup actions) and in the main loop without blocking delays.

For analog channels (ADC), see `docs/IO-AnalogInputs.md`.

## Creating an Input

Example (active-high button wired to 3.3V, internal pulldown, idle LOW):

```cpp
ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
    .id = "testbutton",
    .name = "Test Button",
    .defaultPin = 33,
    .defaultActiveLow = false,
    .defaultPullup = false,
    .defaultPulldown = true,
    .defaultEnabled = true,
});
```

## Settings (Pins, Polarity, Pull)

Each input has a small set of settings.

- `GPIO` (`P`): Which GPIO pin to use
- `LOW-Active` (`L`): Logic inversion
- `Pull-up` (`U`): Use `INPUT_PULLUP`
- `Pull-down` (`D`): Use `INPUT_PULLDOWN`

### Key format (ESP32 NVS safe)

To keep keys short (ESP32 Preferences limit), IOManager uses **slot-based keys**:

- Inputs: `II%02uX` (e.g. `II00P`)
- Outputs: `IO%02uX` (see IO-DigitalOutputs.md)

Suffix meanings for inputs:

- `P` = pin
- `L` = active-low
- `U` = pull-up
- `D` = pull-down

Important: slot-based keys depend on the order of `addDigitalInput(...)` calls.
If you reorder inputs in code, previously stored pins may appear to "move" to another input.

## Runtime indicator (bool-dot)

Registering an input for the runtime UI:

```cpp
ioManager.addDigitalInputToLive(
    "testbutton",
    10,
    "Live",
    "Inputs",
    "inputs",
    "Test Button",
    false
);
```

- `pageName` / `cardName` / `groupName`: live layout placement
- `labelOverride`: label shown in the UI
- `alarmWhenActive`:
  - `true`: input active is treated as an alarm condition (red/orange styling)
  - `false`: input behaves like a normal boolean dot (green/gray)

## Settings placement

Place persisted inputs in the Settings UI:

```cpp
ioManager.addDigitalInputToSettingsGroup(
    "testbutton",
    "Digital - I/O",
    "Digital Inputs",
    "Test Button",
    10
);
```

## Event engine (non-blocking)

Events are optional and configured per input:

```cpp
ioManager.configureDigitalInputEvents(
    "testbutton",
    cm::IOManager::DigitalInputEventCallbacks{
        .onPress = [](){ /* ... */ },
        .onRelease = [](){ /* ... */ },
        .onClick = [](){ /* ... */ },
        .onDoubleClick = [](){ /* ... */ },
        .onMultiClick = [](uint8_t count){ /* ... */ },
        .onLongClick = [](){ /* ... */ },
    }
);
```

If `onMultiClick` is set, IOManager will fire it after the double-click timeout with the number of clicks (1..n), and `onClick`/`onDoubleClick` are not emitted.

### Default timings

These defaults are defined in `cm::IOManager::DigitalInputEventOptions`:

- `debounceMs = 40`
- `doubleClickMs = 350`
- `longClickMs = 700`

You can override them per input:

```cpp
cm::IOManager::DigitalInputEventOptions opt;
opt.longClickMs = 1200;
opt.doubleClickMs = 400;
opt.debounceMs = 60;

ioManager.configureDigitalInputEvents("testbutton", callbacks, opt);
```

## Startup-only long press

For dangerous actions (e.g. factory reset / AP mode), IOManager supports a dedicated callback:

- `onLongPressOnStartup`

This callback is only eligible during a short startup window.

### Constants

- Startup window duration: `STARTUP_LONG_PRESS_WINDOW_MS = 10000` (10 seconds)

### Behavior

- While the startup window is active:
  - a long press triggers `onLongPressOnStartup` (if set)
- After the window:
  - `onLongPressOnStartup` is disabled
  - normal events (`onPress`, `onRelease`, `onClick`, `onDoubleClick`, `onLongClick`) continue working

Example:

```cpp
cm::IOManager::DigitalInputEventOptions opt;
opt.longClickMs = 2500;

ioManager.configureDigitalInputEvents(
    "reset",
    cm::IOManager::DigitalInputEventCallbacks{
        .onLongPressOnStartup = [](){
            ConfigManager.clearAllFromPrefs();
            ConfigManager.saveAll();
            ESP.restart();
        }
    },
    opt
);
```

## Lifecycle

Typical sketch order:

1. `addDigitalInput(...)` / `addDigitalInputToSettingsGroup(...)` / `addDigitalInputToLive(...)` / `configureDigitalInputEvents(...)`
2. `ConfigManager.loadAll()` (loads persisted pins/polarity)
3. `ioManager.begin()` (applies `pinMode(...)` and initializes states)
4. In `loop()`: `ioManager.update()` continuously reads inputs and emits events

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.begin` | `begin()` | Starts ConfigManager services and web routes. | Used in examples: yes. |

