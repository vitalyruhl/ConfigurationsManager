# IO Analog Inputs (IOManager)

This document describes how **analog inputs** (ADC) work in `cm::IOManager`.

## Overview

`IOManager` analog inputs are settings-driven and provide:

- ADC pin configuration via Settings (GPIO)
- Optional mapping (raw ADC -> scaled value)
- Deadband + minimum event interval support
- Optional alarm thresholds (min/max) with callbacks
- Runtime UI display (scaled value and optional raw view)

## Creating an Analog Input

Example (ESP32 ADC1 pin recommended when WiFi is used):

```cpp
ioManager.addAnalogInput(cm::IOManager::AnalogInputBinding{
    .id = "ldr_s",
    .name = "LDR South",
    .defaultPin = 35,
    .defaultRawMin = 0,
    .defaultRawMax = 4095,
    .defaultOutMin = 0.0f,
    .defaultOutMax = 100.0f,
    .defaultUnit = "%",
    .defaultPrecision = 1,
});
```

## Settings (GPIO + Mapping)

Analog inputs use short, slot-based keys (ESP32 Preferences safe).

### Key format (ESP32 NVS safe)

To keep keys short (ESP32 Preferences limit), IOManager uses **slot-based keys**:

- Analog inputs: `AI%02uX` (e.g. `AI00P`)

Suffix meanings for analog inputs:

- `P` = pin
- `R` = raw min
- `S` = raw max
- `M` = out min
- `N` = out max
- `U` = unit
- `D` = deadband
- `E` = min event (ms)
- `A` = alarm min
- `B` = alarm max

Important: slot-based keys depend on the order of `addAnalogInput(...)` calls.

### Settings list

The settings exposed in the UI include:

- `GPIO`: ADC pin
- `Raw Min` / `Raw Max`: input range for mapping
- `Out Min` / `Out Max`: output range (scaled)
- `Unit`: display only
- `Deadband`: minimum delta to trigger an event/update
- `Min Event (ms)`: emits an update at least every N ms (even if unchanged)

### Settings placement

Place persisted inputs in the Settings UI:

```cpp
ioManager.addAnalogInputToSettingsGroup(
    "ldr_s",
    "Analog - I/O",
    "Analog Inputs",
    "LDR South",
    10
);
```

## Runtime UI (raw vs scaled)

Analog channels can be registered for the runtime UI as:

- scaled value (mapped float)
- raw ADC value

These can be shown in different runtime cards/groups.

Scaled value:

```cpp
ioManager.addAnalogInputToLive(
    "ldr_s",
    10,
    "AI",
    "Analog Inputs",
    "analog",
    "LDR South",
    false
);
```

Raw value:

```cpp
ioManager.addAnalogInputToLive(
    "ldr_s",
    11,
    "AI",
    "Analog Inputs",
    "analog_raw",
    "LDR South (raw)",
    true
);
```

## Alarms (min/max)

You can configure optional alarm thresholds for the **scaled value**.

- min-only: set only `alarmMin`
- max-only: set only `alarmMax`
- both: set `alarmMin` and `alarmMax`

When enabled, IOManager exposes two additional runtime bool fields:

- `<id>_alarm_min`
- `<id>_alarm_max`

The WebUI uses these flags for alarm highlighting when present.

Alarm thresholds are also exposed as Settings only when configured:

- `Alarm Min`
- `Alarm Max`

## Callbacks

Use `AnalogAlarmCallbacks` to distinguish which boundary triggered:

- `onMinEnter` / `onMinExit`
- `onMaxEnter` / `onMaxExit`

You can also use combined callbacks (`onEnter`/`onExit` or `onStateChanged`) if you only care whether *any* alarm is active.

## Lifecycle

Typical sketch order:

1. `addAnalogInput(...)` / `addAnalogInputToSettingsGroup(...)` / optional `addAnalogInputToLive(...)` / optional `addAnalogInputToLiveWithAlarm(...)`
2. `ConfigManager.loadAll()` (loads persisted pins/mapping)
3. `ioManager.begin()`
4. In `loop()`: `ioManager.update()` continuously reads inputs, updates runtime values, and evaluates alarms

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.begin` | `begin()` | Starts ConfigManager services and web routes. | Used in examples: yes. |

