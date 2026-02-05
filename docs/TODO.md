# TODO / Roadmap / Refactor Notes (Planning)

## Terminology

- Top buttons: Live / Settings / Flash / Theme (always visible)
- Settings:
  - SettingsPage = tab in Settings (e.g. "MQTT-Topics")
  - SettingsCard = big card container with title one page can have multiple cards
  - SettingsGroup = group box inside a card (contains fields; shown stacked vertically)
- Live/Runtime:
  - LivePage = top-level page selector under "Live" (similar to Settings tabs)
  - LiveCard = card container inside a live page
  - LiveGroup = if we want a second nesting level like Settings

## High Priority (Prio 1) - Proposed API vNext (Draft)


### integrate update calls in alarmManager.update();

>The current approach of manually calling alarmManager.update() every 1.5 seconds in the main loop is error-prone and can lead to missed alarm evaluations if the user forgets to include it. We should integrate the timing mechanism directly into the AlarmManager class so that users only need to call update() without worrying about the timing.

#### This Time
```cpp
static unsigned long lastAlarmEval = 0;
    if (millis() - lastAlarmEval > 1500)
    {
        lastAlarmEval = millis();
        alarmManager.update();
    }
```

#### Proposed
```cpp
void setup()
{
    // other setup code...
    alarmManager.setUpdateInterval(3000); // you dont need this if you are fine with the default 1,5s interval
}

void loop()
{
    // other loop code...
    alarmManager.update(); // internally checks if enough time has passed and only evaluates alarms if needed
}
```

### check if the key is needed

>The key parameter in the value() method is currently required but not actually used for anything. We should check if we can remove it to simplify the API.

#### This Time

```cpp

tempCard.value("temp", []() { return roundf(mockedTemperatureC * 10.0f) / 10.0f; })
        .label("Temperature [MOCKED DATA]")
        .unit("°C")
        .precision(1)
        .order(10)
        .addCSSClass("myCSSTemperatureClass");
```

#### Proposed

```cpp

tempCard.value([]() { return roundf(mockedTemperatureC * 10.0f) / 10.0f; })
        .label("Temperature [MOCKED DATA]")
        .unit("°C")
        .precision(1)
        .order(10)
        .addCSSClass("myCSSTemperatureClass");
```

### refactor Full-GUI-Demo

>refactor the Live-Page

- Consolidate the Temereture Data exactly how its in BME280 Demo
- analog and Buttons should be in a separate Live-Page "IO Test" or similar
- add other New and missing Runtime feautures to the demo, except the IO and MQTT, because there are already separate demos for these (we can link to them in the README and say "see MQTT demo for MQTT features" and "see IO demo for IO features")


## Medium Priority (Prio 5)

- verify/implement compile-time warnings for invalid IO pin bindings (e.g., hold button test pin)
- add addCSSClass helper for all Live controls (buttons, sliders, inputs) so user CSS selectors can override styles (Live only, not Settings)



## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)



## Done / Resolved

## Status Notes

