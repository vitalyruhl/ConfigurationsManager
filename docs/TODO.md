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

- Flash optimization refactor: split large `.cpp` files into smaller modules so the linker can strip more unused code.

### Plan: Split large `.cpp` files for better dead-code elimination

1. Baseline + hotspot identification
   - Record size baseline (`firmware.elf`, `firmware.bin`, map file) for `examples/BoilerSaver`.
   - List largest translation units and symbols (`pio run -e usb -v`, map analysis).
2. Define module boundaries (no behavior change)
   - Split by feature/domain, not by random line ranges.
   - Keep stable public interfaces in headers; move internals to feature-local files.
3. Incremental extraction
   - Move one feature block at a time from monolithic files into dedicated `.cpp/.h` pairs.
   - Build and run after each extraction step.
4. Maximize linker strip opportunities
   - Keep helper functions and static data private to their module where possible.
   - Avoid central mega-files that reference all features at once.
5. Verification gates per step
   - Compile + upload check.
   - Runtime smoke test (web/routes/critical feature path still working).
   - Compare flash size delta against baseline and document result.
6. Completion criteria
   - No functional regressions.
   - Same external API behavior.
   - Measurable flash reduction or, if unchanged, clear report of limiting factors.

## Low Priority (Prio 10)

- SD card logging (CSV)
- IOManager improvements (PWM/LEDC backend, ramping, fail-safe states)
- Headless mode (no HTTP server)



## Done / Resolved

- build all examples and flash to a test device (USB) to verify no build issues and basic functionality (e.g. AP mode starts, web server responds)
- 2026-02-13: all examples except `ChipInfo` were flashed on COM3 and checked via HTTP/curl; behavior verified OK (BME280 AP-mode without creds is expected), OTA update also verified OK.

## Status Notes

- not stable yet, expect breaking changes as we refactor and improve the API!
