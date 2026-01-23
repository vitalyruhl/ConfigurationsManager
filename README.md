# ConfigurationsManager for ESP32

> Version 3.3.0

> Breaking changes in v3.0.0
>
> - Removed most build-time feature flags (`CM_ENABLE_*`, `CM_EMBED_WEBUI`, etc.)
> - No PlatformIO `extra_scripts` required anymore (embedded WebUI is included in `src/html_content.*`)
> - Only logging remains configurable: `CM_ENABLE_LOGGING`, `CM_ENABLE_VERBOSE_LOGGING`

## Overview

ConfigurationsManager is a C++17 helper library + example firmware for managing persistent configuration values on ESP32 (NVS / Preferences) and exposing them via a responsive Vue 3 single‑page web UI (settings + live runtime dashboard + OTA flashing). It focuses on:

- Type‑safe templated `Config<T>` wrappers
- Central registration + bulk load/save functions
- Optional pretty display names and pretty category names (decouple storage key from UI)
- Automatic key truncation safety (category + key <= 15 chars total in NVS) with friendly UI name preserved
  -- Dynamic conditional visibility (`showIf` lambdas)
- Callbacks on value change
- OTA update integration
- Static or DHCP WiFi startup helpers (multiple overloads)

## Note: This is a C++17 Project

> Requires `-std=gnu++17` (or newer) in `platformio.ini` or Arduino IDE settings. The library uses C++17 features like inline variables, structured bindings, lambdas with captures in unevaluated contexts, and `std::function` for callbacks.

## Features

- 📦 Non-Volatile Storage (NVS) integration (ESP Preferences)
- Declarative config registration with a `ConfigOptions<T>` aggregate
- 🌐 responsive web configuration interface (embedded in flash)
- 🚀 OTA firmware upload endpoint
- ⚡ Flash firmware directly from the web UI (password-protected HTTP OTA)
- 🪄 Dynamic visibility of settings via `showIf` lambda (e.g. hide static IP fields while DHCP enabled)
- 🔒 Password masking & selective exposure
- 🎨 Live-Values - two theming paths: per‑field JSON style metadata or global `/user_theme.css` override
- 🛎️ Per‑setting callbacks (`cb` or `setCallback`) on value changes
- 📡 AP Mode fallback / captive portal style entry
- Smart WiFi Roaming - Automatic switching to stronger access points in mesh networks
- Live runtime values (`/runtime.json`) (you can make your own Web-Interface by design the Vue 3 embedded Project)
- Manager API: `addRuntimeProvider({...})`, `enableWebSocketPush(intervalMs)`, `pushRuntimeNow()`, optional `setCustomLivePayloadBuilder()`
- 🧩 Boilerplate reduction helpers: `OptionGroup` factory + `showIfTrue()/showIfFalse()` visibility helpers (since 2.4.3)

### IOManager (Digital/Analog IO)

The optional `cm::IOManager` module provides a settings-driven IO abstraction for:

- Digital inputs (GPIO + polarity + pull modes + non-blocking button events)
- Digital outputs (GPIO + polarity + runtime controls)
- Analog inputs (ADC raw + scaled value, deadband/minEvent, optional min/max alarms)

## Documentation Index

| Topic                                     | File                    |
| ----------------------------------------- | ----------------------- |
| Settings & OptionGroup                    | `docs/SETTINGS.md`      |
| Runtime Providers & Alarms                | `docs/RUNTIME.md`       |
| IOManager: Digital Inputs                 | `docs/IO-DigitalInputs.md` |
| IOManager: Digital Outputs                | `docs/IO-DigitalOutputs.md` |
| IOManager: Analog Inputs                  | `docs/IO-AnalogInputs.md` |
| Smart WiFi Roaming                        | `docs/SMART_ROAMING.md` |
| Security Notes (password transport)       | `docs/SECURITY.md`      |
| Styling (per-field metadata)              | `docs/STYLING.md`       |
| Theming (global CSS + disabling metadata) | `docs/THEMING.md`       |
| Build Options (v3 change note)            | `docs/FEATURE_FLAGS.md` |

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

**1. Install the library** (see above)

**2. Configure `platformio.ini`:**
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
   
   > **Note:** Replace `usb` with your environment name if different. See `examples/example_min/platformio.ini` for a complete example.

**3. Dependencies (optional):**

- For normal firmware builds you don't need Node.js/Python.
- Only required if you want to rebuild the WebUI (see `webui/README.md`).

**4. Use the examples as reference:**
    - The examples in `examples/` are standalone PlatformIO projects (`platformio.ini` + `src/main.cpp`).
    - Start with `examples/example_min/src/main.cpp` and adapt as needed.

## Setup Requirements

### v3.0.0: No prebuild scripts

v3.0.0 ships the embedded WebUI directly in the library (`src/html_content.*`).

- No `extra_scripts` is required.
- No `CM_ENABLE_*` list is required.
- Only optional logging flags exist.

### PlatformIO Configuration

Your `platformio.ini` should include these settings:

```ini
[env:your_env]
platform = espressif32
board = your_board
framework = arduino

build_unflags = -std=gnu++11          # Remove old C++ standard (required!)
build_flags =
    -Wno-deprecated-declarations      # Suppress ArduinoJson warnings
    -std=gnu++17                      # Enable C++17 features (required!)
    ; Optional logging switches:
    ; -DCM_ENABLE_LOGGING=1
    ; -DCM_ENABLE_VERBOSE_LOGGING=0

lib_deps =
    vitaly.ruhl/ESP32 Configuration Manager@^3.3.0
```

> **📋 See complete examples in `examples/example_min/platformio.ini`**

### Dependencies

- Firmware build: no extra dependencies.
- WebUI development: see `webui/README.md`.

## Security Notice (v3.1.0)

- Password values are **masked** in the WebUI (shown as `***`).
- When you **set or change** a password in the WebUI, it is transmitted to the ESP32 **in cleartext over HTTP**.

ConfigurationsManager currently serves the Web UI and API over plain HTTP only (no on-device HTTPS server).
If you need transport security, provide it externally (e.g. trusted WiFi only, VPN, or a TLS reverse proxy that terminates HTTPS).

## Examples

> Example files live in the `examples/` directory:

Each example is a standalone PlatformIO project:

- [examples/example_min](examples/example_min) - minimal demo
- [examples/bme280](examples/bme280) - BME280 (light)
- [examples/BME280-Full-GUI-Demo](examples/BME280-Full-GUI-Demo) - full feature demo
- [examples/publish](examples/publish) - build-only smoke example

Build an example:

```bash
pio run -d examples/bme280 -e usb
```

Upload (USB example):

```bash
pio run -d examples/bme280 -e usb -t upload --upload-port COM5
```

Note (Windows): these example projects set `[platformio] build_dir` and `libdeps_dir` outside the repo to prevent recursive local installs when using `lib_deps = file://../..`.



### To see how I use it in my projects

- [https://github.com/vitalyruhl/SolarInverterLimiter](https://github.com/vitalyruhl/SolarInverterLimiter)
- [https://github.com/vitalyruhl/BoilerSaver](https://github.com/vitalyruhl/BoilerSaver)


### minimal pattern (using `ESPAsyncWebServer`)

```cpp
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"
AsyncWebServer server(80);

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Min-Demo"

ConfigManagerClass cfg;                                               // Create an instance of ConfigManager before using it in structures etc.
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr; // Initialize the logger to nullptr

// Basic setting using new aggregate initialization (ConfigOptions<T>)
Config<int> updateInterval(ConfigOptions<int>{
    .key = "interval",
    .name = "Update Interval (seconds)",
    .category = "main",
    .defaultValue = 30});

// Example of a structure for WiFi settings
struct WiFi_Settings // wifiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;

    // Shared OptionGroup constants to avoid repetition
    static constexpr OptionGroup WIFI_GROUP{"wifi", "WiFi Settings"};

    WiFi_Settings() : // Use OptionGroup helpers with shared constexpr instances
                      wifiSsid(WIFI_GROUP.opt<String>("ssid", "MyWiFi", "WiFi SSID")),
                      wifiPassword(WIFI_GROUP.opt<String>("password", "secretpass", "WiFi Password", true, true)),
                      useDhcp(WIFI_GROUP.opt<bool>("dhcp", false, "Use DHCP")),
                      staticIp(WIFI_GROUP.opt<String>("sIP", "192.168.2.126", "Static IP", true, false, nullptr, showIfFalse(useDhcp))),
                      gateway(WIFI_GROUP.opt<String>("GW", "192.168.2.250", "Gateway", true, false, nullptr, showIfFalse(useDhcp))),
                      subnet(WIFI_GROUP.opt<String>("subnet", "255.255.255.0", "Subnet-Mask", true, false, nullptr, showIfFalse(useDhcp)))
    {
        // Register settings with ConfigManager
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
        cfg.addSetting(&staticIp);
        cfg.addSetting(&gateway);
        cfg.addSetting(&subnet);
    }
};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct

void setup()
{
    Serial.begin(115200);
    ConfigManagerClass::setLogger([](const char *msg)
                                  { Serial.print("[CFG] "); Serial.println(msg); });

    cfg.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    cfg.setAppTitle(APP_NAME); // Set an application title, used for web UI display

    cfg.addSetting(&updateInterval);
    cfg.checkSettingsForErrors();

    try
    {
        cfg.loadAll(); // Load all settings from preferences, is nessesary before using the settings!
    }
    catch (const std::exception &e)
    {
        Serial.println(e.what());
    }

    Serial.println("Loaded configuration:");
    delay(300);

    // print loaded settings to Serial (optional)
    Serial.println("Configuration printout:");
    Serial.println(cfg.toJSON(false));

    // Example set a Value from Program
    updateInterval.set(15);
    cfg.saveAll();

    // nesseasary settings for webserver and wifi
    if (wifiSettings.wifiSsid.get().length() == 0)
    {
        Serial.printf("[WARNING] SETUP: SSID is empty! [%s]\n", wifiSettings.wifiSsid.get().c_str());
        cfg.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        Serial.println("[INFO] AP Mode");
        return; // Skip webserver setup in AP mode
    }

    // Enable built-in System provider (RSSI, freeHeap, optional loop avg) before starting the web server
    cfg.enableBuiltinSystemProvider();

    if (wifiSettings.useDhcp.get())
    {
        Serial.println("DHCP enabled");
        cfg.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }
    else
    {
        Serial.println("DHCP disabled");
        cfg.startWebServer(wifiSettings.staticIp.get(), wifiSettings.gateway.get(), wifiSettings.subnet.get(), wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }

    delay(1500);                    // wait for wifi connection a bit
    cfg.enableWebSocketPush(2000); // 2s Interval for push updates to clients - if not set, ui will use polling

    if (WiFi.status() == WL_CONNECTED)
    {
        // OTA setup (always call if feature compiled in & user enabled; handleOTA will defer until WiFi is up)
        cfg.setupOTA("esp32", "otapassword123");
    }
    Serial.printf("[INFO] Webserver running at: %s\n", WiFi.localIP().toString().c_str());
}

void loop()
{
    //nessesary for webserver and wifi to handle clients and websocket
    cfg.handleClient();
    cfg.handleWebsocketPush();
    cfg.handleOTA();
    cfg.updateLoopTiming();// Update rolling loop timing window so the system provider can expose avg loop time if desired and show the SystemProvider Information

    //reconnect if wifi is lost
    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP)
    {
        Serial.println("[WARNING] WiFi lost, reconnecting...");
        cfg.reconnectWifi();
        delay(1500); // wait for wifi connection a bit
        cfg.setupOTA("esp32", "otapassword123");
    }

    delay(updateInterval.get());
}

```

> Breaking changes in v2.7.x
>
> - Introduced modular architecture with separate WiFi, Web, OTA, and Runtime managers
> - Added integrated Smart WiFi Roaming feature for automatic access point switching
> - Some APIs have been moved into dedicated manager classes (use `getWiFiManager()`, `getWebManager()`, etc.)
> - ConfigOptions field rename: `keyName` → `key`, `prettyName` → `name`, `prettyCat` → `categoryPretty`
> - If you are upgrading from < 2.7, check your code for WiFi management calls and update accordingly

- **FRITZ!Box Mesh networks** with multiple repeaters
- **Enterprise WiFi** with multiple access points
- **Home mesh systems** where devices get "stuck" to distant APs

### How Smart Roaming Works

1. **Signal Monitoring**: Continuously monitors WiFi signal strength (RSSI)

    // Configure Smart WiFi Roaming (optional - defaults work well)
    ConfigManager.enableSmartRoaming(true);        // Enable/disable (default: true)
    ConfigManager.setRoamingImprovement(10);       // Minimum signal improvement required in dBm (default: 10)

    // ... continue with WiFi setup ...

### Default Settings

| Parameter       | Default Value | Description                                        |
| --------------- | ------------- | -------------------------------------------------- |
| **Enabled**     | `true`        | Smart roaming is active by default                 |
| **Threshold**   | `-75 dBm`     | Trigger roaming when signal is weaker than -75 dBm |
| **Cooldown**    | `120 seconds` | Wait 2 minutes between roaming attempts            |
| **Improvement** | `10 dBm`      | Only switch if new AP is 10+ dBm stronger          |

When Smart Roaming is active, you'll see logs like:

```
[ConfigManager] [WiFi] Smart Roaming enabled
[ConfigManager] [WiFi] Roaming threshold set to -75 dBm
[ConfigManager] [WiFi] Current RSSI (-92 dBm) below threshold (-75 dBm), scanning for better APs...
[ConfigManager] [WiFi] Found better AP: 60:B5:8D:4C:E1:D5 (RSSI: -45 dBm, improvement: 47 dBm)
[ConfigManager] [WiFi] Initiated roaming to better access point
```

### Advanced Usage

For complete control over the WiFi manager:

```cpp
// Check if roaming is enabled
if (wifiMgr.isSmartRoamingEnabled()) {
    Serial.println("Smart roaming is active");
}

// Disable roaming temporarily
wifiMgr.enableSmartRoaming(false);

```

### PlatformIO Environment Examples

```ini
[env:usb]
platform = espressif32
board = nodemcu-32s
framework = arduino
upload_port = COM[5]
monitor_speed = 115200
;build_unflags = -std=gnu++11 ;required, to deactivate old default
build_unflags = -std=gnu++11
build_flags =
	-DUNIT_TEST
	-Wno-deprecated-declarations
	-std=gnu++17
	-DCM_ENABLE_LOGGING=1
	-DCM_ENABLE_VERBOSE_LOGGING=0
	-DCONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE=1
	-DCONFIG_ESP32_BROWNOUT_DET_LVL0=1

lib_deps =
    vitaly.ruhl/ESP32 Configuration Manager@^3.3.0
	ks-tec/BME280_I2C@1.4.1+002
	knolleary/PubSubClient@^2.8
	adafruit/Adafruit GFX Library@^1.12.1
test_ignore = src/main.cpp

; v3.0.0: no extra_scripts required



[env:ota]
platform = espressif32
board = nodemcu-32s
framework = arduino
monitor_speed = 115200
;build_unflags = -std=gnu++11 ;required, to deactivate old default
build_unflags = -std=gnu++11
build_flags =
	-DUNIT_TEST
	-Wno-deprecated-declarations
	-std=gnu++17
	-DCM_ENABLE_LOGGING=1
	-DCM_ENABLE_VERBOSE_LOGGING=0
	-DCONFIG_BOOTLOADER_WDT_DISABLE_IN_USER_CODE=1
	-DCONFIG_ESP32_BROWNOUT_DET_LVL0=1
lib_deps =
    vitaly.ruhl/ESP32 Configuration Manager@^3.3.0
	ks-tec/BME280_I2C@1.4.1+002
	knolleary/PubSubClient@^2.8
	adafruit/Adafruit GFX Library@^1.12.1
test_ignore = src/main.cpp

;;test-esp
upload_port = 192.168.2.126
upload_flags = --auth=ota

; v3.0.0: no extra_scripts required
```

## Flash Firmware via Web UI

The embedded single-page app now exposes a `Flash` action beside the `Settings` tab so you can push new firmware without leaving the browser.

1. Enable `Allow OTA Updates` under **System** and set an `OTA Password` (leave empty to allow unauthenticated uploads).
2. Click **Flash** and pick the compiled `.bin` (or `.bin.gz`) image produced by PlatformIO / Arduino.
3. Enter the OTA password when prompted. The SPA sends it as the `X-OTA-PASSWORD` header so it never ends up inside the firmware payload.
4. Watch the toast notifications for progress. On success the device reboots automatically; the UI keeps polling until it comes back online.

The backend remains fully asynchronous (`ESPAsyncWebServer`)—the new `/ota_update` handler streams chunks into the `Update` API while still performing password checks. HTTP uploads are rejected when OTA is disabled, the password is missing/incorrect, or the upload fails integrity checks.

Note: The runtime JSON includes system-level OTA flags used by the WebUI to enable the Flash button and show status. Specifically, `runtime.system.allowOTA` is true when OTA is enabled on the device (HTTP endpoint ready), and `runtime.system.otaActive` is true after `ArduinoOTA.begin()` has run (informational).

### Async Build & Live Values

The project now always uses the Async stack; no `env:async` or `-DUSE_ASYNC_WEBSERVER` define is needed.

The WebUI prefers WebSocket live updates (`/ws`). If WebSocket is unavailable (blocked/disabled/network issues), it transparently falls back to polling `/runtime.json`.

### PlatformIO environments (usb / ota / publish)

```sh
# See platformio.ini for details

pio run -e usb -t upload # upload via usb

# Or via ota:
pio run -e ota -t upload

#or:
#pio run --target upload --upload-port <ESP32_IP_ADDRESS>
pio run -e ota -t upload --upload-port 192.168.2.126

# Or over the Web UI by press the "Flash" Button and upload the firmware.bin
```

## Troubleshooting

### Build Issues

v3.0.0 builds without PlatformIO prebuild scripts. If you are developing the WebUI, see `webui/README.md`.

**Web UI not building or old version showing:**
```sh
pio run -e usb -t clean # Clean previous build files
# Then rebuild
pio run -e usb
```

### Flash/Memory Issues

**ESP32 won't boot or guru meditation errors:**
```sh
pio run -e usb -t erase # WARNING: Deletes all flash data on ESP32!
pio run -e usb -t upload # Re-upload firmware
```

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

> CSS Theming
> ![CSS Theming](examples/screenshots/Info-CSS.jpg)

## Version History

- **1.x**: initial release, basic settings UI, logging callback.
- **2.0.0**: OTA support + examples; UI naming improvements.
- **2.3.0**: `ConfigOptions<T>` aggregate init + `showIf` visibility (major API style update).
- **2.4.0**: runtime JSON + WebSocket live values + runtime metadata + alarm styling.
- **2.7.0**: modular managers + Smart WiFi Roaming + runtime provider improvements.
- **2.7.4**: tools folder included in the library package (no manual copying).
- **3.0.0**: v3 release: removed most build-time feature flags, embedded WebUI committed, docs reorganized.
- **3.1.0**: v3 stabilization: runtime/UI improvements, ordering fixes in Settings/Live views, WebSocket/OTA fixes.
- **3.1.1**: WebUI header vs tab title split (`setAppName` vs `setAppTitle` via `/appinfo`), improved settings password prompt for passwordless setups, WiFi reconnect-storm mitigation during scans.
- **3.2.0**: minor bump / maintenance release.
- **3.3.0**: IOManager module (digital IO + analog IO incl. alarms + DAC outputs) + core settings templates/injection + runtime/WebUI robustness improvements.

## Smart WiFi Roaming Feature

Version 2.7.0 introduces **Smart WiFi Roaming** - a game-changing feature for mesh networks and multi-AP environments:

### Key Benefits

- **Automatic AP switching** when signal strength drops below threshold
- **Intelligent roaming** with configurable signal improvement requirements
- **Cooldown protection** to prevent frequent switching
- **Static IP preservation** during roaming transitions
- **FRITZ!Box Mesh optimized** - perfect for complex home networks

### Simple Configuration

```cpp
// Enable with defaults (works great out-of-the-box)
ConfigManager.enableSmartRoaming(true);

// Or customize for your environment
ConfigManager.setRoamingThreshold(-75);    // Trigger at -75 dBm
ConfigManager.setRoamingCooldown(120);     // Wait 2 minutes between attempts
ConfigManager.setRoamingImprovement(10);   // Require 10+ dBm improvement
```

### Real-world Performance

- Signal improved from **-90 dBm (very weak)** to **-45 dBm (good)**
- Automatic switching between FRITZ!Box main router and repeaters
- Zero configuration required - smart defaults work immediately

See `docs/SMART_ROAMING.md` for complete documentation and optimization guides.

## ToDo / Issues

- [see TODO.md in docs folder](docs/TODO.md)

