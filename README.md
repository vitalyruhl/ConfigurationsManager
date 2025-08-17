# ConfigurationsManager for ESP32

> Version 1.2.1

[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)]

[![GitHub issues](https://img.shields.io/github/issues/vitaly.ruhl/ESP32ConfigManager.svg)]
[![GitHub last commit](https://img.shields.io/github/last-commit/vitaly.ruhl/ESP32ConfigManager.svg)]


## Overview

The ConfigurationsManager is a project designed to manage and store configuration settings for ESP32-based applications. It provides an easy-to-use interface for saving, retrieving, and updating configuration parameters, ensuring persistent storage and efficient management.

## Note: This is a C++17 Project

```ini
[env:nodemcu-32s]
platform = espressif32
board = nodemcu-32s
framework = arduino
monitor_speed = 115200
upload_port = COM[3]
build_unflags = -std=gnu++11
build_flags =
    -Wno-deprecated-declarations
    -std=gnu++17
lib_deps = bblanchon/ArduinoJson@^7.4.1

[platformio]
description = ESP32 C++17 Project for managing settings
```

## Features

- 📦 Non-Volatile Storage (NVS) integration (esp preferences)
- 🌐 Responsive Web Configuration Interface
- 🔒 Password masking & secret handling
- 🔄 Automatic WiFi reconnect
- 📡 AP Mode fallback

## Requirements

- ESP32 development board
- Arduino IDE or PlatformIO
- add _build_flags = -std=gnu++17_ and _build_unflags = -std=gnu++11_ to your platformio.ini file

## Installation

```bash
# PlatformIO
pio pkg install --library "vitaly.ruhl/ESP32ConfigManager"
```

1. Include the ConfigurationsManager library in your project.

```cpp
#include <ConfigManager.h>

Config<String> wifiSSID("ssid", "network", "MyWiFi");
Config<String> wifiPass("password", "network", "", true, true);

void setup() {
  ConfigManager.addSetting(&wifiSSID);
  ConfigManager.addSetting(&wifiPass);
  configManager.saveAll();
  ConfigManager.startWebServer();
}
```

## Version History

- **1.0.0**: Initial release with basic features.
- **1.0.2**: make an library
- **1.1.0**: add Structure example, bugfix, add delete settings functions
- **1.1.1**: forgot to change library version in library.json
- **1.1.2**: Bugfix: add forgotten function applyAll() in html
- **1.2.0**: add logging function as callback for flexible logging
- **1.2.1**: bugfix in logger over more, then one headder using, add dnsserver option for static ip.
- **1.2.2**: bugfix remove throwing errors, becaus it let esp restart without showing the error message.
- **1.3.0**: Add OTA support, add new example for OTA, add new example for WiFiManager with OTA.

## ToDo / known Issues

- **Save all** button works only, if you saved value ones over single save-button
- add test for new functions
- add more examples
- HTTPS Support
- i18n Support
- make c++ V11 support (i hope for contribution, because i have not enough c++ knowledge for make it typ-safe)
