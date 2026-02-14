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

- no items yet
- in pio under dependencys - is empty, but we need some...
- in pio under Readme - i need min 1 screenshot in the front
- in pio under Readme - new hook - perfect for beginners...
- take neu screenshot of the new API design and add it to the Readme (and maybe also to the wiki)

## Medium Priority (Prio 5)

- no items yet

## Low Priority (Prio 10)

- SD card modulle + logging (CSV) on SD card (e.g. for data logging, or for storing config files)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)
- GUI: add simple Trend for DI/AI values

## need a check / review

- check other mqtt libraries for better API design (e.g. async, callback-based, etc.) AND QOS support!
 - https://registry.platformio.org/libraries/elims/PsychicMqttClient

## Done / Resolved

- New API design and refactor

## Status Notes

- stable
