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

- none (all former Prio-1 doc normalization tasks completed in `chore/prio1-doc-normalization`)

## Medium Priority (Prio 5)

- verify/implement compile-time warnings for invalid IO pin bindings (e.g., hold button test pin)
- add addCSSClass helper for all Live controls (buttons, sliders, inputs) so user CSS selectors can override styles (Live only, not Settings)



## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)



## Done / Resolved

- Prio 1: documented `ConfigManager.performStackReset()` in `docs/WIFI.md`
- Prio 1: added minimal diagnostic `performStackReset()` snippet in `examples/Full-Logging-Demo`
- Prio 1: converted `docs/MQTT.md` Method overview to required table format
- Prio 1: replaced generic Method overview placeholders with topic-specific entries (or `_none_` in non-API docs)
- Prio 1: aligned severity-tag guidance to short tags (`[I]`, `[W]`, `[E]`, `[D]`, `[T]`) in contributor guidance

## Status Notes

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| _none_ | - | Planning document; no callable API methods in this document. | Track tasks and status only. |

