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
