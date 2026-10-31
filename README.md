# ConfigurationsManager for ESP32

> Version 4.10.6 (unpublished test candidate)

The local 4.10.6 candidate requires ESP32Async/ESPAsyncWebServer 3.12.1 and
ESP32Async/AsyncTCP 3.5.0. Remove explicit legacy `esphome` transport dependencies
from consumer projects before building this candidate. It uses guarded server
operations by client ID and retains at most one outstanding runtime buffer per
client. This repairs a verified connection-lifetime defect; the historical
BoilerSaver panic remains unclassified without its backtrace. No production
deployment is authorized by this candidate's test results.

The maintained transport can release a runtime buffer after copying its bytes
into TCP, before a network ACK. CM keeps at most one transport-owned runtime
buffer per client; previously copied bytes remain bounded by the TCP window.
This is not an application-level one-snapshot-per-network-ACK guarantee.

The opt-in `CM_MQTT_BOUNDED_IO=1` transport assembles complete MQTT frames without
waiting in the application loop, expires partial frames after 30 seconds, and
closes the connection on malformed frames. A fixed 4608-byte transmit FIFO retains
unsent bytes across transient send pressure and positive short writes, preserving
byte order without allocating heap memory. Pending bytes expire after five seconds
without a positive TCP send. A successful partial send renews this stall deadline;
newly queued bytes and transient errors do not. Continuous progress may keep the
queue nonempty longer than five seconds: this is neither an absolute packet-age
limit nor a broker-delivery guarantee. The fixed queue capacity remains unchanged;
queue overflow, a zero-byte send, or a definitive socket error closes the connection
and clears its pending bytes. Transmit pumping shares at most 64 send attempts and
1000 microseconds per application loop, separately from the receive budget. During
the synchronous CONNACK wait, retries renew once per elapsed 1000-microsecond
epoch, rather than once per poll. These budgets bound work between nonblocking
socket calls; they are not a hard real-time guarantee for the socket implementation. This mode
requires a numeric IPv4 broker address and bounds TCP connect to 100 ms plus up to
one second for CONNACK. The legacy transport remains the default. ESP System-Info
includes `resetReason`; `CM_MQTT_WIFI_UPTIME=0` removes the duplicate ESP uptime
from WiFi System-Info while keeping the public default unchanged.

The transmit counters distinguish recovered pressure/short writes from fatal
failures, expiry, and overflow. This does not establish that graceful socket close
cancels bytes already queued in the TCP stack; delayed CONNECT/session replacement
remains a separate field investigation.

HTTP OTA can optionally prepare a restart through
`getOTAManager().setRebootPreparation(std::function<bool()>)`, installed before
web initialization. Register a nonblocking callback that requests/polls terminal
work; it is retained across setup and callback registration. It runs after the
500ms disconnect delay and has a terminal ten-second preparation deadline.
Timeout blocks automatic restart and sets `rebootPreparationBlocked()`; it does
not force reset, disable watchdogs, or retry cleanup. ArduinoOTA is unchanged.

The optional BLE integration uses `requestShutdownForRestart()` and
`isShutdownForRestartComplete()` (or `isShutdownForRestartFailed()`). Only the
existing BLE worker stops the host/controller with `deinit(false)`, confirms
host uninitialized and native controller IDLE, then clears objects with
`deinit(true)` and reconfirms both conditions. Failure retains a terminal fence;
no new acquisition/configuration or cleanup retry is admitted. A missing worker
can acknowledge only an uninitialized, native-IDLE radio without another CM
owner, using read-only checks. Consumer test profiles may set
`CM_TEST_RESTART_PHASES=1` for a few UART phase markers; the default is0. The
coexistence lease stays conservative until restart. The BLE-active HTTP OTA
correction has bounded isolated and production acceptance in SBT 1.4.12; native SDK calls can still block. See the
[acceptance record](docs/acceptance/2026-10-27-http-ota-ble-shutdown.md) for exact
source identity, observations and limits.

Successful HTTP OTA disconnect callbacks only publish a fixed-size atomic
reboot request. The application loop services it once after 500ms, before OTA
maintenance locks or disabled/uninitialized early returns. Duplicate callbacks
cannot extend that deadline or repeat the reboot. No request/context pointer is
retained. This keeps restart/shutdown work out of the watchdog-subscribed
AsyncTCP callback. The linked acceptance record covers initialized-BLE
same-image HTTP OTA on both the isolated fixture and production SBT.
ArduinoOTA reboot handling, authentication and settings are unchanged.

Full WiFi stack recovery uses Arduino's `WiFi.mode(WIFI_OFF)` and
`WiFi.mode(WIFI_STA)` lifecycle exclusively. Arduino therefore applies its own
runtime `WiFi.useStaticBuffers()` preference and initial TX/RX buffer policy;
the manager no longer mixes direct native stop/deinit/init calls into recovery.
A failed shutdown or station-mode transition aborts that reset attempt without
reporting completion. Further attempts use the existing reconnect cadence.
This does not establish the cause of the observed SBT panic or of a TCP outage;
hardware verification of reconnect and recovered heap remains required.

## Historical development checkpoints and reproduction limits

The earlier branch checkpoints were not Registry releases. The independently
frozen CM 4.10.1 package contains 188 files; its manifest SHA256 is
`6c04701572f9d45e10277ea6aa8eefa9763ab98d57bd6d6676ff3308ad1a6fe7`.
All 188 files matched the checkout before the later documentation-only rollout
notes. The frozen package and firmware artifacts remain unchanged; this commit's
README/changelog are later records, not byte-identical frozen package inputs.

SBT 1.4.8 uses that freeze. Its owner-authorized OTA on 2026-10-25 was followed by
verified Settings-only policy and real BMS/Victron progress. SIL 5.4.3 deliberately
retains the separate CM 4.10.0 WiFi-only freeze. Both consumer pins reference
ignored local packages. Each consumer now preserves its exact original source
ZIP, SHA256 and manifest under `dependencies/cm/`, with manual verified restore
instructions. These are source snapshots, not Registry releases. The current CM
checkout cannot substitute for SIL's historical 4.10.0 package. Those historical
checkpoints did not publish firmware binaries, tags or a Registry release. The 4.10.5 release includes the later Vue dependency update;
its regenerated UI is covered by the release CI rather than the earlier device
observation. Repeated MQTT interruptions remain open in [#89](https://github.com/vitalyruhl/ConfigurationsManager/issues/89);
the bounded device observations do not establish long-term stability.

## WiFi power saving

WiFi now defaults to modem power saving **off** (`WIFI_PS_NONE`) for mains-powered
applications. `cm::CoreWiFiSettings::powerSave`, attached with the core WiFi bundle,
is a boolean Settings control with key hint `WiFiPowerSave`. Saving it persists the
choice; the switch shows the configured choice. The diagnostic text below it is a
snapshot of the separately requested runtime state, actual `esp_wifi_get_ps` mode,
and last apply/read result. Reload Settings for a fresh snapshot. This information
is included in `/config.json` even with `CM_ENABLE_LIVE_UI=0`; read failure shows
an unknown driver mode, never an inferred On or Off state.

Precedence and lifecycle:

1. `CM_WIFI_POWER_SAVE_DEFAULT` accepts only `0` (default) or `1` (MIN_MODEM).
   It initializes a missing setting once; an existing NVS choice takes precedence.
   Without the core bundle it is the WiFi manager's fallback.
2. Loaded settings are applied before the first station connection. Reconnects and
   stack resets reapply the current requested value.
3. `ConfigManager.setWiFiPowerSave(bool)` queues a **non-persistent** runtime
   override. The last queued API or Settings change wins when `handleClient()`
   runs. It does not modify the stored switch value. Reloading/resetting Settings
   queues their values again; reboot restores the saved choice.

The low-level `getWiFiManager().setPowerSave(bool)` belongs to the application loop;
use the queued ConfigManager API from callbacks or other tasks. Diagnostics are
available through `getWiFiManager().getPowerSaveDiagnostics()` with fields
`requested`, `effectiveMode` (`-1` on read failure), `readError`, `applyAttempted`,
`applySucceeded`, and `applyError`. Reading diagnostics does not change the radio.
Before station startup a request remains pending until a connect path applies it.
Arduino's cached sleep request is synchronized, then `esp_wifi_set_ps` applies the
mode explicitly: after native WiFi reset, the cache may match while the driver
has reverted to its default. A changed mode can therefore cause two SDK calls;
only the explicit native result determines apply success.

Before the first CM BLE scan or GATT operation, its worker queues a coexistence
lease and waits for the application loop to confirm native `WIFI_PS_MIN_MODEM`.
ESP32/ESP-IDF 4.4.7 does not support `WIFI_PS_NONE` in WiFi/Bluetooth coexistence:
see [Espressif's power-saving constraints](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-guides/wifi.html#esp32-wi-fi-power-saving-mode).
A prior successful MIN read is not a lease acknowledgment. Native apply/read
failure leaves BLE startup pending and retries from the application loop at most
once per second (never during a WiFi stack reset); the explicit WiFi-uninitialized case may
proceed after synchronizing Arduino's cache to MIN. Subsequent station starts
and stack resets always reapply the lease before normal radio work.

The saved preference remains unchanged. While the lease is active, requested
false yields effective MIN with `reason=ble_coexistence`; requested true has
`reason=requested`. Settings JSON exposes `bleCoexistenceRequired` independently.
An applied coexistence override is not an apply error. Ordinary CM BLE pause,
disable, disconnect, removed peers or cancelled startup do not shut down its controller and cannot release the lease: MIN remains
required until reboot. The explicit terminal restart-shutdown path described
above also retains the lease conservatively until reboot.
Do not infer controller shutdown from the Bluetooth Enabled setting. WiFi-only
applications retain their requested policy, including the default NONE.

The optional CM Bluetooth manager still requires the consumer's NimBLE dependency.
The bounded SBT acceptance records cover real BLE acquisition/coexistence and
HTTP OTA shutdown; first-start host tests alone do not establish RF performance
or long-term stability.
Short WiFi-only measurements do not prove that power saving causes MQTT stalls.

## Preview Before Installing

If you want to quickly evaluate coding style and project structure before installing,
check the standalone examples in [examples/](examples/).

Recommended first look:

- [examples/minimal](examples/minimal)
- [examples/WT32-ETH01-v1.4](examples/WT32-ETH01-v1.4)
- [examples/Full-GUI-Demo](examples/Full-GUI-Demo)
- [examples/Full-IO-Demo](examples/Full-IO-Demo)

## Why this exists

Most ESP32 projects start the same way:
you need a web server, persistent configuration storage, WiFi handling, OTA updates, and some way to inspect or control runtime values.

Individually, none of these problems are difficult.
But together they quickly turn into repetitive boilerplate that distracts from the actual purpose of your project.

This library exists to solve that once.

It provides a structured, type-safe way to define config/settings, persist them, expose them through a responsive Web UI, and interact with live runtime data — without forcing you to design yet another custom web server or configuration layer.

You focus on your application logic.
The infrastructure is already there.

## Overview

> Focus on your project logic — not on web servers, storage, or configuration plumbing.

ConfigurationsManager is a C++17 helper library and example firmware for ESP32 projects that need persistent configuration, a web-based UI, and live runtime visibility — without building custom infrastructure from scratch. It focuses on:

- Type‑safe templated `Config<T>` wrappers
- Central registration + bulk load/save functions
- Optional pretty display names and pretty category names (decouple storage key from UI)
- Automatic key truncation safety (category + key <= 15 chars total in NVS) with friendly UI name preserved
- Dynamic conditional visibility (`showIf` lambdas)
- Callbacks on value change
- OTA update integration
- Static or DHCP WiFi startup helpers (multiple overloads)
- Optional logging module (Serial/GUI/MQTT outputs)

## Note: C++17 Required

>This project requires `-std=gnu++17` (or newer).
>It intentionally uses modern C++ features such as inline variables, structured bindings, and `std::function` to keep the API expressive and type-safe.

## Features

### Core
- Non-Volatile Storage (NVS / Preferences)
- Type-safe `Config<T>` wrappers
- Central registration + bulk load/save

### Web UI & Runtime
- Responsive WebUI (embedded in flash)
- Live runtime values (`/runtime.json`, WebSocket)
- Page-scoped dashboard loading and bounded runtime delivery
- Opt-in compact runtime metadata (legacy APIs preserved)
- Per-setting styling & theming

### Device & Connectivity
- Optional WiFi helpers (DHCP / static / AP fallback)
- External-network startup for Ethernet-based projects
- OTA firmware upload via WebUI
- Optional MQTT module (PubSubClient-based)
- Optional BLE transport (consumer-supplied NimBLE, bounded worker and queues)

### Compact runtime metadata

Version 4.8.0 includes opt-in small field definitions while retaining the legacy API.
See [Runtime metadata](docs/runtime-metadata.md) for typed registration, sparse extensions, promotion and memory measurements.

### Select Live and Settings at build time

`CM_ENABLE_LIVE_UI` and `CM_ENABLE_SETTINGS_UI` each default to `1`. Set either
to `0` to compile out that area's UI, data models, JSON routes and serialization.
Choose Live + Settings + Flash, Settings + Flash, Live + Flash, or a minimal
identity/authentication/Flash interface. Flash remains subject to `CM_ENABLE_OTA`
and the configured authentication. Configuration storage, NVS, MQTT, validators,
callbacks and operational alarms remain available in every combination.

The same flags select the Vue bundle, so disabled components and their JS/CSS
are excluded from the embedded page. See [Feature flags](docs/FEATURE_FLAGS.md)
for the mode/route matrix, compatible UI-registration no-ops and build-cache rules.

### Connectivity and loop recovery

`cm::CoreWiFiSettings` exposes one **Connectivity check target** field (`WiFiProbe`):
choose **Off** (`off`, the default), **Standard gateway** (`gateway`), or enter a
strict unicast IPv4 address. Gateway checks use the currently active
`WiFi.gatewayIP()`, including the DHCP-provided gateway. There is no separate mode
setting. Off preserves existing WiFi-loss recovery; an enabled target additionally
requires successful reachability checks every 10 seconds, with a one-second probe
deadline. The `WiFiRb` timeout defaults to one minute (60 seconds); zero disables
network-triggered restarts. It accepts whole minutes from 0 to 9999, so values such
as 60 or 120 provide one- or two-hour recovery windows. Existing saved values
remain unchanged. AP mode and OTA suspend network-triggered restart checks.

**Apply** changes the target in memory only; reboot restores the saved value.
**Save** persists it, including a validly written address that is unreachable.
A wrong saved target can therefore cause repeated recovery restarts. Test with
Apply before Save, and keep USB or AP access available to restore `off` or the
correct target if network access is lost. Validation checks syntax, not reachability.

A separate loop supervisor defaults to 30 seconds and starts with the first
`ConfigManager.handleClient()` call. `ConfigManager.setLoopWatchdogTimeout(ms)`
adjusts its timeout; passing zero disables it. Its independent timer does not
rely on the application loop progressing and does not install a global task
watchdog. The ESP timer task and scheduler must still be able to run; this
supervisor is not a replacement for interrupt/hardware watchdogs. ArduinoOTA
progress confirms synchronous loop work; asynchronous HTTP upload traffic does
not mask a blocked application loop. A failed synchronous ArduinoOTA transfer
receives a fresh network recovery window when the loop resumes. See the
[connectivity acceptance record](docs/acceptance/2026-10-21-connectivity-watchdog.md)
for the tested image, bounded device results and remaining limitations.

### Logging
- Optional logging module (Serial, GUI log tab, MQTT output)

### Extensibility
- Conditional visibility (`showIf`)
- Callbacks on value change
- Core settings templates & IOManager module

## Documentation Index

| Topic                                     | File                                                   |
| ----------------------------------------- | ------------------------------------------------------ |
| Getting Started (minimal pattern)         | [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)     |
| WiFi (DHCP/static/AP/reconnect)           | [docs/WIFI.md](docs/WIFI.md)                           |
| Ethernet / external-network startup       | [docs/ETHERNET.md](docs/ETHERNET.md)                   |
| Settings & OptionGroup                    | [docs/SETTINGS.md](docs/SETTINGS.md)                   |
| GUI Runtime (Live + Styling + Theming)    | [docs/GUI-Runtime.md](docs/GUI-Runtime.md)             |
| Logging                                   | [docs/LOGGING.md](docs/LOGGING.md)                     |
| IOManager: Digital Inputs                 | [docs/IO-DigitalInputs.md](docs/IO-DigitalInputs.md)   |
| IOManager: Digital Outputs                | [docs/IO-DigitalOutputs.md](docs/IO-DigitalOutputs.md) |
| IOManager: Analog Inputs                  | [docs/IO-AnalogInputs.md](docs/IO-AnalogInputs.md)     |
| IOManager: Analog Outputs                 | [docs/IO-AnalogOutputs.md](docs/IO-AnalogOutputs.md)   |
| OTA + Web UI flashing                     | [docs/OTA.md](docs/OTA.md)                             |
| Security Notes (password transport)       | [docs/SECURITY.md](docs/SECURITY.md)                   |
| Troubleshooting                           | [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)     |
| Bluetooth Module                          | [docs/BLUETOOTH.md](docs/BLUETOOTH.md)                 |
| MQTT Module                               | [docs/MQTT.md](docs/MQTT.md)                           |
| Changelog                                 | [docs/CHANGELOG.md](docs/CHANGELOG.md)                 |
| Build Options / Feature Flags             | [docs/FEATURE_FLAGS.md](docs/FEATURE_FLAGS.md)         |

## Requirements

- **ESP32 development board** (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)
- **PlatformIO** (preferred) or Arduino IDE (Arduino IDE not tested!)
- **Node.js 22.12.0+** (only if you want to rebuild the WebUI)
- **Python 3.8+** (optional; only used by legacy tooling)
- Add `-std=gnu++17` (and remove default gnu++11) in `platformio.ini`

## Installation

```bash
# PlatformIO
pio pkg install --library "vitaly.ruhl/ESP32 Configuration Manager"
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

### Example secrets

Most examples read optional defaults from `src/secret/secrets.h`.

1. Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` in the selected example.
2. Adjust credentials and optional defaults.
3. Build/upload as usual.

If `secrets.h` is missing, examples fall back to settings UI/AP workflow.

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
    vitaly.ruhl/ESP32 Configuration Manager@^4.0.0
```

For a complete minimal firmware pattern, see `docs/GETTING_STARTED.md`.

## Notes (v4)

- v4.0.0 continues to ship the embedded WebUI directly in the library (`src/html_content.*`).

## Security Notice

The Web UI and API are served over plain HTTP.
If you need transport security, provide it externally (trusted WiFi only, VPN, or a TLS reverse proxy that terminates HTTPS).

Details: see `docs/SECURITY.md`.

## Examples

> Example files live in the `examples/` directory:

Each example is a standalone PlatformIO project:

- [examples/minimal](examples/minimal) - minimal demo
- [examples/WT32-ETH01-v1.4](examples/WT32-ETH01-v1.4) - WT32-ETH01 V1.4 Ethernet demo
- [examples/BME280-Temp-Sensor](examples/BME280-Temp-Sensor) - BME280 temp sensor
- [examples/Full-Logging-Demo](examples/Full-Logging-Demo) - logging and runtime log output demo
- [examples/Full-MQTT-Demo](examples/Full-MQTT-Demo) - MQTT integration demo
- [examples/Full-GUI-Demo](examples/Full-GUI-Demo) - full GUI demo
- [examples/Full-IO-Demo](examples/Full-IO-Demo) - IOManager demo (incl. core settings templates)
- [examples/ChipInfo](examples/ChipInfo) - device/chip information demo
- [examples/BoilerSaver](examples/BoilerSaver) - real-world boiler controller example
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

If you want to consume the library as a local dependency in your own project, use a local `lib_deps = symlink://../ConfigurationsManager` setup.

Note (Windows): these example projects set `[platformio] build_dir` and `libdeps_dir` outside the repo to prevent recursive local installs when using `lib_deps = symlink://../..`.

Tips:

- Avoid `lib_extra_dirs = ../..` for the workspace library; it can cause cross-example compilation issues.
- Some examples include `tools/pio_force_local_lib_refresh.py` to refresh local library metadata before a build (disable via `CM_PIO_NO_LIB_REFRESH=1`).
- If you see `UnicodeEncodeError: 'charmap' codec can't encode character ...` on Windows, see `docs/TROUBLESHOOTING.md`.

More:

- Getting started (full minimal pattern): `docs/GETTING_STARTED.md`
- WiFi behavior / best practices: `docs/WIFI.md`
- Ethernet / WT32-ETH01: `docs/ETHERNET.md`
- OTA and Web UI flashing: `docs/OTA.md`
- Troubleshooting: `docs/TROUBLESHOOTING.md`

## Screenshots

### The embedded Web UI automatically adapts to desktop and mobile devices.

>Current UI snapshots
>![alt text](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/V4.0.0-GUI-STD.jpg)
>![alt text](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/V4.0.0-GUI-Dark.jpg)

> Example on Monitor HD
> ![Example on Monitor HD](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/test-hd.jpg)
> Example on mobile
> ![Example on mobile](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/test-mobile.jpg)
> Settings on HD
> ![Settings on HD 1](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/test-settings-HD.jpg)
> ![Settings on HD 2](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/test-settings2-HD.jpg)
> Settings on mobile
> ![Settings on mobile 1](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/test-settings-mobile.jpg)
> ![Settings on mobile 2](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/test-settings-mobile2.jpg)
> Darkmode
> ![Darkmode](https://raw.githubusercontent.com/vitalyruhl/ConfigurationsManager/main/examples/screenshots/cm-full-io-demo-gui-darkmode.jpg)

## Version History

Moved to [docs/CHANGELOG.md](docs/CHANGELOG.md).

## ToDo / Issues

- [see TODO.md in docs folder](docs/TODO.md)

<br>
<br>


## Donate

<table align="center" width="100%" border="0" bgcolor:=#3f3f3f>
<tr align="center">
<td align="center">  
if you prefer a one-time donation

[![donate-Paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://paypal.me/FamilieRuhl)

</td>

<td align="center">  
Become a patron, by simply clicking on this button (**very appreciated!**):

[![Become a patron](https://c5.patreon.com/external/logo/become_a_patron_button.png)](https://www.patreon.com/join/6555448/checkout?ru=undefined)

</td>
</tr>
</table>

<br>
<br>

---

## Copyright

`2024-2026 (c)Vitaly Ruhl`

License: GNU General Public License v3.0
