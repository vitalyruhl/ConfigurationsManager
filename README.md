# ConfigurationsManager for ESP32

>Version 1.0.0

## Overview

The ConfigurationsManager is a project designed to manage and store configuration settings for ESP32-based applications. It provides an easy-to-use interface for saving, retrieving, and updating configuration parameters, ensuring persistent storage and efficient management.

## Achtung das ist ein c++V17 Projekt

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
description = esp32 c++ V17 Project for manage settings


```


## Features

- **Persistent Storage**: Save configuration settings to non-volatile memory (e.g., SPIFFS or NVS).
- **Easy Retrieval**: Retrieve stored configurations with minimal code.
- **Web Interface**: Optional web interface for managing configurations via a web browser.


## Requirements

- ESP32 development board
- Arduino IDE or PlatformIO
- add *build_flags = -std=gnu++17* and *build_unflags = -std=gnu++11* to your platformio.ini file




## Usage

1. Include the ConfigurationsManager library in your project.

```cpp
//todo: add some example code

´´´

# todo

- HTTPS Support
- i18n Support
- make c++ V11 support (i hope for contribution, because i have not enough c++ knowledge for it)

