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


## Medium Priority (Prio 5)

- not present

## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)



## Done / Resolved

- build all examples and flash to a test device (USB) to verify no build issues and basic functionality (e.g. AP mode starts, web server responds)
- 2026-02-13: all examples except `ChipInfo` were flashed on COM3 and checked via HTTP/curl; behavior verified OK (BME280 AP-mode without creds is expected), OTA update also verified OK.

## Status Notes

- not stable yet, expect breaking changes as we refactor and improve the API!
