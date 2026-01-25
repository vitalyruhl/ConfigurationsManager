# MQTT Module

This repository provides an **optional** MQTT helper module that is **not auto-enabled**.

- Module header: `src/mqtt/MQTTManager.h`
- You must explicitly include it from your sketch.
- You must add `PubSubClient` to your project dependencies.

## Minimal Usage

### 1) Add dependency (PlatformIO)

Add to your project `platformio.ini`:

```ini
lib_deps =
  knolleary/PubSubClient@^2.8
```

### 2) Include + instance

```cpp
#include "mqtt/MQTTManager.h"

static cm::MQTTManager mqtt;
```

### 3) Configure + run

```cpp
void setup()
{
    mqtt.setServer("192.168.1.10", 1883);
    mqtt.setCredentials("user", "pass");

    mqtt.onConnected([]() {
        mqtt.subscribe("device/cmd");
        mqtt.publish("device/status", "online", true);
    });

    mqtt.onMessage([](char* topic, byte* payload, unsigned int length) {
        String msg(reinterpret_cast<const char*>(payload), length);
        // Handle message
    });

    mqtt.begin();
}

void loop()
{
    mqtt.loop();   // or: mqtt.update();
}
```

## Notes

- The module uses a non-blocking state machine with retry logic.
- `configurePowerUsage()` is an optional helper to parse a single watts value from either a plain numeric payload or JSON with a dot-path (e.g. `"sensor.power"`).
