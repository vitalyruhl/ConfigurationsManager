# ConfigurationsManager for ESP32

> Version 1.0.0

## Overview

The ConfigurationsManager is a project designed to manage and store configuration settings for ESP32-based applications. It provides an easy-to-use interface for saving, retrieving, and updating configuration parameters, ensuring persistent storage and efficient management.

## Note: This is a C++17 Project

```ini
[env:nodemcu-32s]
platform = espressif32
board = nodemcu-32s
framework = arduino
monitor_speed = 115200
upload_port = COM[4]
build_unflags = -std=gnu++11
build_flags =
    -Wno-deprecated-declarations
    -std=gnu++17
lib_deps = bblanchon/ArduinoJson@^7.4.1

[platformio]
description = ESP32 C++17 Project for managing settings
```

## Features

- 📦 Non-Volatile Storage (NVS) integration
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
pio pkg install --library "idk/ESP32ConfigManager"
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
´´´

# todo

- HTTPS Support
- i18n Support
- make c++ V11 support (i hope for contribution, because i have not enough c++ knowledge for it)

```
