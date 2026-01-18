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

Analog inputs use short, slot-based keys (ESP32 Preferences safe). The exact key names are internal, but the settings exposed in the UI include:

- `GPIO`: ADC pin
- `Raw Min` / `Raw Max`: input range for mapping
- `Out Min` / `Out Max`: output range (scaled)
- `Unit`: display only
- `Deadband`: minimum delta to trigger an event/update
- `Min Event (ms)`: emits an update at least every N ms (even if unchanged)

Note: slot-based keys depend on the order of `addAnalogInput(...)` calls.

## Runtime UI (raw vs scaled)

Analog channels can be registered for the runtime UI as:

- scaled value (mapped float)
- raw ADC value

These can be shown in different runtime cards/groups.

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
