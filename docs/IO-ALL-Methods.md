# IOManager: add*ToGUI Methods

This page lists the IOManager helper methods that register IO settings and/or runtime (live) fields in the Web UI.

Notes:
- "Settings" means entries under Settings -> IO (persisted via ConfigManager settings store).
- "Runtime" means entries under Runtime/Live view (runtime provider + metadata).
- Most `add*ToGUI` helpers only have an effect when the corresponding binding has `registerSettings=true`.

## Digital Outputs

### addIOtoGUI(id, cardName, order)
- Adds Settings -> IO entries for a digital output (GPIO + ActiveLow) grouped into a card.
- Does not add any runtime control by itself.

### addIOtoGUI(id, cardName, order, type, onPress, runtimeLabel, runtimeGroup)
- Adds Settings -> IO entries (if enabled) and a runtime control of the given type.
- Intended for action-like controls (button press).

### addIOtoGUI(id, cardName, order, type, getter, setter, runtimeLabel, runtimeGroup, runtimeOnLabel, runtimeOffLabel)
- Adds Settings -> IO entries (if enabled) and a runtime control with state (checkbox/state button).

### addIOtoGUI(id, cardName, order, sliderMin, sliderMax, sliderStep, sliderPrecision, runtimeLabel, runtimeGroup, unit)
- Adds Settings -> IO entries (if enabled) and a runtime slider control.
- Used mainly for analog outputs but exposed via the generic addIOtoGUI overload.

## Digital Inputs

### addInputToGUI(id, cardName, order, runtimeLabel, runtimeGroup, alarmWhenActive)
- Adds Settings -> IO entries for a digital input (GPIO + ActiveLow + Pull-up + Pull-down).
- Also adds a runtime boolean indicator (and optional alarm styling).

### addInputSettingsToGUI(id, cardName, order)
- Adds only Settings -> IO entries for a digital input.
- No runtime/live registration.

### addInputRuntimeToGUI(id, order, runtimeLabel, runtimeGroup, alarmWhenActive)
- Adds only runtime/live registration for a digital input.
- No settings/persistence registration (use when Settings should be handled separately).

## Analog Inputs

### addAnalogInputToGUI(id, cardName, order, runtimeLabel, runtimeGroup, showRaw)
- Adds Settings -> IO entries for an analog input and adds runtime value fields.
- If `showRaw=true`, runtime shows raw values instead of scaled values.

### addAnalogInputToGUIWithAlarm(id, cardName, order, alarmMin, alarmMax, runtimeLabel, runtimeGroup)
- Same as addAnalogInputToGUI, but also registers alarm thresholds for the scaled value.

### addAnalogInputToGUIWithAlarm(id, cardName, order, alarmMin, alarmMax, callbacks, runtimeLabel, runtimeGroup)
- Same as above, but also invokes callbacks when alarm state changes.

### configureAnalogInputAlarm(id, alarmMin, alarmMax, callbacks)
- Configures alarm thresholds/callbacks without necessarily changing what is shown in Runtime.
- Alarm is evaluated on the scaled analog value.

## Analog Outputs

### addAnalogOutputSliderToGUI(id, cardName, order, sliderMin, sliderMax, sliderStep, sliderPrecision, runtimeLabel, runtimeGroup, unit)
- Adds Settings -> IO entries for an analog output and a runtime slider to control it.

### addAnalogOutputValueToGUI(id, cardName, order, runtimeLabel, runtimeGroup, unit, precision)
- Adds a read-only runtime field showing the current analog output value (scaled).

### addAnalogOutputValueRawToGUI(id, cardName, order, runtimeLabel, runtimeGroup)
- Adds a read-only runtime field showing the raw DAC value (0..255).

### addAnalogOutputValueVoltToGUI(id, cardName, order, runtimeLabel, runtimeGroup, precision)
- Adds a read-only runtime field showing output voltage (0..3.3V).

