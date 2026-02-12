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

- complete Phase-4 follow-up for undocumented WiFi API:
  - document `ConfigManager.performStackReset()` in `docs/WIFI.md` (advanced troubleshooting)
  - add a minimal diagnostic usage snippet in `examples/Full-Logging-Demo`
- normalize docs method-overview quality:
  - replace generic placeholder rows (only `ConfigManager.begin`) with topic-specific API entries where applicable
  - keep one valid `## Method overview` section per file with meaningful content
- convert `docs/MQTT.md` Method overview into the required table format:
  - `| Method | Overloads / Variants | Description | Notes |`
  - list overloads explicitly with `<br>` where needed
- align documentation guidance with logging severity policy:
  - use short severity tags (`[I]`, `[W]`, `[E]`, `[D]`, `[T]`) instead of long tags (`[INFO]`, `[WARNING]`, ...)

## Medium Priority (Prio 5)

- verify/implement compile-time warnings for invalid IO pin bindings (e.g., hold button test pin)
- add addCSSClass helper for all Live controls (buttons, sliders, inputs) so user CSS selectors can override styles (Live only, not Settings)



## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)



## Done / Resolved

## Status Notes

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.begin` | `begin()` | Starts ConfigManager services and web routes. | Used in examples: yes. |

