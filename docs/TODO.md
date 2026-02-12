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

- new bug after refactoring: [ 49629][E][WiFiSTA.cpp:232] begin(): SSID too long or missing! ( pio run -e usb -t clean; pio run -e usb -t upload - does no matter) 


## Medium Priority (Prio 5)

- not present

## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)



## Done / Resolved

- build all examples ant flash to a test device (USB) to verify no build issues and basic functionality (e.g. AP mode starts, web server responds)

## Status Notes

- not stable yet, expect breaking changes as we refactor and improve the API!