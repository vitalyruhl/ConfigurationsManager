# ConfigurationsManager for ESP32

> Version 3.3.0

## Overview

ConfigurationsManager is a C++17 helper library + example firmware for managing persistent configuration values on ESP32 (NVS / Preferences) and exposing them via a responsive Vue 3 single‑page web UI (settings + live runtime dashboard + OTA flashing). It focuses on:

- Type‑safe templated `Config<T>` wrappers
- Central registration + bulk load/save functions
- Optional pretty display names and pretty category names (decouple storage key from UI)
- Automatic key truncation safety (category + key <= 15 chars total in NVS) with friendly UI name preserved
- Dynamic conditional visibility (`showIf` lambdas)
- Callbacks on value change
- OTA update integration
- Static or DHCP WiFi startup helpers (multiple overloads)

## Note: This is a C++17 Project

> Requires `-std=gnu++17` (or newer) in `platformio.ini` or Arduino IDE settings. The library uses C++17 features like inline variables, structured bindings, lambdas with captures in unevaluated contexts, and `std::function` for callbacks.

## Features

- Non-Volatile Storage (NVS) integration (ESP Preferences)
- Declarative config registration with a `ConfigOptions<T>` aggregate
- Responsive WebUI (embedded in flash)
- OTA firmware upload endpoint + flashing directly from the WebUI
- Dynamic visibility of settings via `showIf` lambda (e.g. hide static IP fields while DHCP enabled)
- Password masking & selective exposure
- Live-Values theming: per-field JSON style metadata or global `/user_theme.css` override
- Per-setting callbacks (`cb` / `setCallback`) on value changes
- AP Mode fallback / captive portal style entry
- Smart WiFi Roaming (mesh / multi-AP)
- Live runtime values (`/runtime.json`) + WebSocket updates (`/ws`) with polling fallback
- Boilerplate reduction helpers: `OptionGroup` factory + `showIfTrue()/showIfFalse()`
- Core settings templates / injection: standard WiFi/System/Buttons settings to keep `main.cpp` small (see examples)
- IOManager module: settings-driven Digital/Analog IO with immediate Settings + WebUI handling (runtime controls + live readouts)

## Documentation Index

| Topic                                     | File                                                   |
| ----------------------------------------- | ------------------------------------------------------ |
| Getting Started (minimal pattern)         | [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)     |
| WiFi (DHCP/static/AP/reconnect)           | [docs/WIFI.md](docs/WIFI.md)                           |
| Settings & OptionGroup                    | [docs/SETTINGS.md](docs/SETTINGS.md)                   |
| Runtime Providers & Alarms                | [docs/RUNTIME.md](docs/RUNTIME.md)                     |
| IOManager: Digital Inputs                 | [docs/IO-DigitalInputs.md](docs/IO-DigitalInputs.md)   |
| IOManager: Digital Outputs                | [docs/IO-DigitalOutputs.md](docs/IO-DigitalOutputs.md) |
| IOManager: Analog Inputs                  | [docs/IO-AnalogInputs.md](docs/IO-AnalogInputs.md)     |
| IOManager: Analog Outputs                 | [docs/IO-AnalogOutputs.md](docs/IO-AnalogOutputs.md)   |
| OTA + Web UI flashing                     | [docs/OTA.md](docs/OTA.md)                             |
| Security Notes (password transport)       | [docs/SECURITY.md](docs/SECURITY.md)                   |
| Styling (per-field metadata)              | [docs/STYLING.md](docs/STYLING.md)                     |
| Theming (global CSS + disabling metadata) | [docs/THEMING.md](docs/THEMING.md)                     |
| Troubleshooting                           | [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)     |
| Changelog                                 | [docs/CHANGELOG.md](docs/CHANGELOG.md)                 |
| Build Options (v3 change note)            | [docs/FEATURE_FLAGS.md](docs/FEATURE_FLAGS.md)         |

## Requirements

- **ESP32 development board** (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)
- **PlatformIO** (preferred) or Arduino IDE (Arduino IDE not tested!)
- **Node.js 18.0.0+** (only if you want to rebuild the WebUI)
- **Python 3.8+** (optional; only used by legacy tooling)
- Add `-std=gnu++17` (and remove default gnu++11) in `platformio.ini`

## Installation

```bash
# PlatformIO
pio pkg install --library "vitaly.ruhl/ESP32ConfigManager"
```

## Quick Start Guide

If you want to get up and running fast, use one of the example projects in `examples/`.

### 1) Build / upload an example (recommended)

Each folder in `examples/` is a standalone PlatformIO project.

Build:

```sh
pio run -d examples/minimal -e usb
```

Upload:

```sh
pio run -d examples/minimal -e usb -t upload --upload-port COM5
```

### 2) Use as a library in your own project

Minimal `platformio.ini` snippet:

```ini
[env:usb]
platform = espressif32
board = your_board
framework = arduino

build_unflags = -std=gnu++11
build_flags =
    -Wno-deprecated-declarations
    -std=gnu++17
    ; Optional logging switches:
    ; -DCM_ENABLE_LOGGING=1
    ; -DCM_ENABLE_VERBOSE_LOGGING=0

lib_deps =
    vitaly.ruhl/ESP32 Configuration Manager@^3.3.0
```

For a complete minimal firmware pattern, see `docs/GETTING_STARTED.md`.

## Notes (v3)

- v3.0.0+ ships the embedded WebUI directly in the library (`src/html_content.*`).

## Security Notice

The Web UI and API are served over plain HTTP.
If you need transport security, provide it externally (trusted WiFi only, VPN, or a TLS reverse proxy that terminates HTTPS).

Details: see `docs/SECURITY.md`.

## Examples

> Example files live in the `examples/` directory:

Each example is a standalone PlatformIO project:

- [examples/minimal](examples/minimal) - minimal demo
- [examples/BME280-Temp-Sensor](examples/BME280-Temp-Sensor) - BME280 temp sensor
- [examples/BME280-Full-GUI-Demo](examples/BME280-Full-GUI-Demo) - full feature demo
- [examples/IO-Full-Demo](examples/IO-Full-Demo) - IOManager demo (incl. core settings templates)
- [examples/ChipInfo](examples/ChipInfo) - device/chip information demo
- [examples/SolarInverterLimiter](examples/SolarInverterLimiter) - real-world larger project using this library

Build an example:

```bash
pio run -d examples/BME280-Temp-Sensor -e usb
```

Upload (USB example):

```bash
pio run -d examples/BME280-Temp-Sensor -e usb -t upload --upload-port COM5
```

If you use a fork / local clone of this repo, you can build and upload any example the same way (the `-d examples/<name>` flag points PlatformIO to the example project directory).

If you want to consume the library as a local dependency in your own project, you can also use a local `lib_deps = file://../ConfigurationsManager` setup (see the note about recursive local installs below).

Note (Windows): these example projects set `[platformio] build_dir` and `libdeps_dir` outside the repo to prevent recursive local installs when using `lib_deps = file://../..`.

More:

- Getting started (full minimal pattern): `docs/GETTING_STARTED.md`
- WiFi behavior / best practices: `docs/WIFI.md`
- OTA and Web UI flashing: `docs/OTA.md`
- Troubleshooting: `docs/TROUBLESHOOTING.md`

## Screenshots

> Example on Monitor HD
> ![Example on Monitor HD](examples/screenshots/test-hd.jpg)
> Example on mobile
> ![Example on mobile](examples/screenshots/test-mobile.jpg)
> Settings on HD
> ![Settings on HD 1](examples/screenshots/test-settings-HD.jpg)
> ![Settings on HD 2](examples/screenshots/test-settings2-HD.jpg)
> Settings on mobile
> ![Settings on mobile 1](examples/screenshots/test-settings-mobile.jpg)
> ![Settings on mobile 2](examples/screenshots/test-settings-mobile2.jpg)

## Version History

Moved to [docs/CHANGELOG.md](docs/CHANGELOG.md).

## ToDo / Issues

- [see TODO.md in docs folder](docs/TODO.md)
