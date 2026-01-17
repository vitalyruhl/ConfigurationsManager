# BME280 Full GUI Demo (Core Settings Variant)

This example is a **variant** of `examples/BME280-Full-GUI-Demo`.

Goal: keep a clean separation between:

- **"Manual" demo**: you define and register WiFi/System/Button settings yourself.
- **"Core settings" demo (this one)**: you **opt-in** to library-provided core settings templates (WiFi/System/Buttons) to keep `main.cpp` small.

## Current status

- The library-side core settings API is not implemented yet (see Step 1 in `docs/TODO.md`).
- Therefore this example currently still contains the local structs, but it is already prepared for the switch.

## How to switch later

When Step 1 is implemented in the library, set this build flag:

- `-DCM_EXAMPLE_USE_BUILTIN_CORE_SETTINGS=1`

Until then it stays at `0` to keep the example compiling.
