# MQTT Module

This repository provides an **optional** MQTT helper module.

- Module header: `src/mqtt/MQTTManager.h`
- You must explicitly include it from your sketch.
- You must add `PubSubClient` to your project dependencies.
- The recommended init pattern is **Option A**: call `mqtt.attach(ConfigManager)` from `setup()`.

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

static cm::MQTTManager& mqtt = cm::MQTTManager::instance();
```

### 3) Attach + run

```cpp
void setup()
{
    // Registers baseline MQTT settings automatically.
    mqtt.attach(ConfigManager);

    // Optional: runtime card in the GUI
    mqtt.addToGUI(ConfigManager, "mqtt");

    // Optional: hooks
    mqtt.onMQTTConnect([]() { CM_LOG("[MQTT][INFO] connected"); });
    mqtt.onMQTTDisconnect([]() { CM_LOG("[MQTT][INFO] disconnected"); });

    // You can still override settings from code (optional)
    // mqtt.setServer("192.168.1.10", 1883);
    // mqtt.setCredentials("user", "pass");
}

void loop()
{
    mqtt.loop();   // or: mqtt.update();
}
```

### 4) Receive topics (plain payload or JSON key-path)

```cpp
static float powerInW = 0.0f;

void setup()
{
    mqtt.attach(ConfigManager);

    // Plain payload example: topic "device/power" => "123.4"
    mqtt.addMQTTTopicReceiveFloat("power_w", "Power", "device/power", &powerInW, "W", 1, "none");

    // JSON example: topic "tele/tasmota/SENSOR" => {"ENERGY":{"Total":12.34}}
    mqtt.addMQTTTopicReceiveFloat("energy_total", "Energy Total", "tele/tasmota/SENSOR", &powerInW, "kWh", 3, "ENERGY.Total");

    mqtt.addToGUI(ConfigManager, "mqtt");
}
```

## Notes

- The module uses a non-blocking state machine with retry logic.
- Settings include:
    - `MQTTEnable` (bool)
    - `MQTTHost`, `MQTTPort`, `MQTTUser`, `MQTTPass`, `MQTTClientId`
    - `MQTTPubPer` (publish interval in seconds; `0` means publish-on-change for send helpers)
    - `MQTTListenMs` (listen interval in ms; `0` means every loop)
