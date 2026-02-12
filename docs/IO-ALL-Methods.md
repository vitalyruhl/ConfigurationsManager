# IOManager: Settings + Live helpers

This page lists the IOManager helper methods that place IOs in the Settings UI and/or the Live (runtime) UI.

Notes:
- "Settings" means entries under Settings (persisted via ConfigManager settings store).
- "Live" means entries under Runtime/Live view (runtime provider + metadata).
- Settings placement only works when the binding has `registerSettings=true`.

## Digital Outputs

### addDigitalOutputToSettingsGroup(id, pageName, cardName, groupName, order)
- Places output settings (GPIO + ActiveLow) into the Settings layout.

### addDigitalOutputToLive(type, id, order, pageName, cardName, groupName, labelOverride, onLabel, offLabel)
- Registers a runtime control (checkbox/state/momentary/button).
- Returns a handle to attach callbacks (e.g. `onChangeCallback`).

## Digital Inputs

### addDigitalInputToSettingsGroup(id, pageName, cardName, groupName, order)
- Places input settings (GPIO + ActiveLow + Pull-up + Pull-down) into the Settings layout.

### addDigitalInputToLive(id, order, pageName, cardName, groupName, labelOverride, alarmWhenActive)
- Registers a runtime indicator (bool-dot with optional alarm styling).
- Returns a handle to attach callbacks (change/click/press/release/long/multi-click).

## Analog Inputs

### addAnalogInputToSettingsGroup(id, pageName, cardName, groupName, order)
- Places analog input settings (GPIO + mapping + unit + deadband + min event) into the Settings layout.

### addAnalogInputToLive(id, order, pageName, cardName, groupName, labelOverride, showRaw)
- Registers a runtime value field (scaled or raw).

### addAnalogInputToLiveWithAlarm(id, order, alarmMin, alarmMax, callbacks, pageName, cardName, groupName, labelOverride)
- Registers a runtime value field with alarm thresholds and optional callbacks.

### configureAnalogInputAlarm(id, alarmMin, alarmMax, callbacks)
- Configures alarm thresholds/callbacks without changing runtime placement.

## Analog Outputs

### addAnalogOutputToSettingsGroup(id, pageName, cardName, groupName, order)
- Places analog output settings (GPIO) into the Settings layout.

### addAnalogOutputToLive(id, order, sliderMin, sliderMax, sliderPrecision, pageName, cardName, groupName, labelOverride, unit)
- Registers a runtime slider control for the analog output.

### addAnalogOutputValueToGUI(id, cardName, order, runtimeLabel, runtimeGroup, unit, precision)
- Adds a read-only runtime field showing the scaled output value.

### addAnalogOutputValueRawToGUI(id, cardName, order, runtimeLabel, runtimeGroup)
- Adds a read-only runtime field showing the raw DAC value (0..255).

### addAnalogOutputValueVoltToGUI(id, cardName, order, runtimeLabel, runtimeGroup, precision)
- Adds a read-only runtime field showing output voltage (0..3.3V).

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `cm::IOManager::addDigitalOutputToLive` | `addDigitalOutputToLive(RuntimeControlType type, const char* id, int order, const char* pageName, const char* cardName, const char* groupName, const char* labelOverride = nullptr, const char* onLabel = nullptr, const char* offLabel = nullptr)` | Adds a runtime control for a digital output. | Returns `LiveControlHandleBool`. |
| `cm::IOManager::addDigitalInputToLive` | `addDigitalInputToLive(const char* id, int order, const char* pageName, const char* cardName, const char* groupName, const char* labelOverride = nullptr, bool alarmWhenActive = false)` | Adds a runtime indicator for a digital input. | Returns `LiveControlHandleBool`. |
| `cm::IOManager::addAnalogInputToLiveWithAlarm` | `addAnalogInputToLiveWithAlarm(..., const AnalogAlarmThreshold* alarmMin, const AnalogAlarmThreshold* alarmMax, ...)` | Adds analog live value with min/max alarm handling. | Optional alarm callbacks. |
| `cm::IOManager::addAnalogOutputToLive` | `addAnalogOutputToLive(const char* id, int order, float sliderMin, float sliderMax, int sliderPrecision, const char* pageName, const char* cardName, const char* groupName, const char* labelOverride = nullptr, const char* unit = nullptr)` | Adds slider control for analog output. | Returns `LiveControlHandleFloat`. |

