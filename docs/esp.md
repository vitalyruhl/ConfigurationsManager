# ESP32 Notes

## Pinout (Board Layout)

```text
__________________________
| .--------------------.  |
| .   ~~~~~~~~~~~~~~   .  |
| .   ~~~~~~~~~~~~~~   .  |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
| [ ]                 [ ] |
|                         |
|   [pwr-LED]  [D2-LED]   |
|        _______          |
|        |     |          |
'--------|-----|---------'
```

Legend:

- [x] in use - pin is already used in your project
- ADC1 - 12-bit ADC, usable even with WiFi
- ADC1 (RO) - input-only pins with ADC1 (GPIO36-39)
- ADC2 - 12-bit ADC, typically not usable for analogRead while WiFi is active (ESP32 Arduino)
- NO ADC - no analog capability
- BOOT PIN - must be LOW or unconnected at boot to avoid boot failure

Notes:

- GPIO0, GPIO2, GPIO12, GPIO15 are boot strapping pins. Avoid forcing a wrong level during boot.
- GPIO6-11 are used for internal flash. Never use.
- GPIO1 (TX) and GPIO3 (RX) are used for serial output (UART0). Use only if UART0 is not needed.
- EN: Pull LOW to reset the ESP32. When it goes HIGH, the ESP32 boots.
- VIN (5V): Power input, connect 5V.
- VP (GPIO36) / VN (GPIO39): ADC1 input-only, no internal pull-up/down.

## UART Pins

| UART | TX | RX |
| --- | --- | --- |
| UART0 | GPIO1 | GPIO3 |
| UART1 | GPIO10 | GPIO9 |
| UART2 | GPIO17 | GPIO16 |

## PlatformIO: Set Monitor/Upload Port (Windows)

In your example `platformio.ini`, you can set the COM port explicitly (recommended):

```ini
monitor_speed = 115200
; monitor_port = COM5
; upload_port = COM5
```

## PlatformIO: Local library (recommended)

Use this repository as a local library via `lib_deps = file://../..` in your example `platformio.ini`.

Recommendations:

- Keep build artifacts out of the repo using per-example `build_dir` and `libdeps_dir` under `${sysenv.HOME}/.pio-build/...`.
- Do not use `lib_extra_dirs = ../..` for the workspace library; it can cause cross-example compilation issues and confusing include resolution.
- If PlatformIO appears to use an old local library copy, clean and rebuild (or use the helper script `tools/pio_force_local_lib_refresh.py`):

```sh
pio run -t clean
pio run
```
