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

    // Registers runtime provider only (no GUI fields are auto-added).
    mqtt.addToGUI(ConfigManager, "mqtt");

    // Optional: hooks
    mqtt.onMQTTConnect([]() { CM_LOG("[MQTT][INFO] connected"); });
    mqtt.onMQTTDisconnect([]() { CM_LOG("[MQTT][INFO] disconnected"); });
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
    mqtt.addMQTTTopicReceiveFloat("power_w", "Power", "device/power", &powerInW, "W", 1, "none", true);

    // JSON example: topic "tele/tasmota/SENSOR" => {"ENERGY":{"Total":12.34}}
    mqtt.addMQTTTopicReceiveFloat("energy_total", "Energy Total", "tele/tasmota/SENSOR", &powerInW, "kWh", 3, "ENERGY.Total", true);

    // GUI entries are explicit
    mqtt.addToGUI(ConfigManager, "mqtt");
    mqtt.addMQTTTopicTooGUI(ConfigManager, "power_w", "MQTT-Received");
    mqtt.addMQTTTopicTooGUI(ConfigManager, "energy_total", "MQTT-Received");
}
```

## Client ID

- The Client ID must be **unique** per device on the broker.
- It is used as a **fallback base topic** when `MQTTBaseTopic` is empty.
- If not set, the module auto-generates one from the MAC address, e.g. `ESP32_AABBCCDDEEFF`.

## Settings for receive topics (MQTT Topics tab)

All `addMQTTTopicReceive*` methods accept `addToSettings` (default: `false`).

- If `true`, two settings are created under the **MQTT Topics** tab:
  - `<Label> Topic`
  - `<Label> JSON Key`
- Changes apply immediately on Save/Apply (subscriptions update on the fly).
- If `false`, defaults are used and **no settings** are created.

## GUI / Runtime helpers

- `addToGUI(...)` registers the runtime provider only.
- Receive items are shown **only** when explicitly added via `addMQTTTopicTooGUI(...)`.
- `getLastTopic()`, `getLastPayload()`, `getLastMessageAgeMs()` can be exposed via runtime providers.

## Callbacks

Preferred hooks (typed and consistent naming):
- `onMQTTConnect(...)`
- `onMQTTDisconnect(...)`
- `onNewMQTTMessage(...)`
- `onMQTTStateChanged(...)`

Classic (PubSubClient-style) hooks remain available:
- `onConnected(...)`
- `onDisconnected(...)`
- `onMessage(...)`

Optional global hook voids (define in your sketch if you want them):
- `void onMQTTConnected()`
- `void onMQTTDisconnected()`
- `void onMQTTStateChanged(int state)`
- `void onNewMQTTMessage(const char* topic, const char* payload, unsigned int length)`

## System info publishing

- Base topic: `<MQTTBaseTopic>/System-Info`
- Publishes **every 60s** while connected (automatic)
- Explicit publish: `publishSystemInfoNow(retained)`
- Payloads are split into two JSON messages:
  - `<base>/System-Info/ESP` (chip + memory)
  - `<base>/System-Info/WiFi` (connection info)
- `uptimeMs` and `uptimeHuman` are included in each payload.

## Publish helpers

- `publishTopic(id)` publishes a receive item value to `<base>/<id>`
- `publishExtraTopic(id, topic, value)` publishes a custom value to a custom topic
- Interval is taken from `MQTTPubPer` (seconds). Use the `Immediately` variants to bypass it.

## Advanced example

```cpp
// Register runtime provider
mqtt.addToGUI(ConfigManager, "mqtt");

// Explicit GUI entries
mqtt.addMQTTTopicTooGUI(ConfigManager, "boiler_temp_c", "MQTT-Received");
mqtt.addMQTTTopicTooGUI(ConfigManager, "powermeter_power_in_w", "MQTT-Received");

// Runtime provider for "other infos"
ConfigManager.getRuntime().addRuntimeProvider("mqtt", [](JsonObject& data) {
    data["lastTopic"] = mqtt.getLastTopic();
    data["lastPayload"] = mqtt.getLastPayload();
    data["lastMsgAgeMs"] = mqtt.getLastMessageAgeMs();
}, 3);
```

## Method overview

Settings + connection:
- `attach(...)`, `begin()`, `loop()`, `disconnect()`
- `setServer(...)`, `setCredentials(...)`, `setClientId(...)`
- `setKeepAlive(...)`, `setMaxRetries(...)`, `setRetryInterval(...)`, `setBufferSize(...)`

Publish / subscribe:
- `publish(...)`, `subscribe(...)`, `unsubscribe(...)`
- `publishSystemInfo(...)`, `publishSystemInfoNow(...)`
- `publishTopic(...)`, `publishTopicImmediately(...)`
- `publishExtraTopic(...)`, `publishExtraTopicImmediately(...)`

Receive helpers:
- `addMQTTTopicReceiveFloat(...)`
- `addMQTTTopicReceiveInt(...)`
- `addMQTTTopicReceiveBool(...)`
- `addMQTTTopicReceiveString(...)`

GUI helpers:
- `addToGUI(...)`
- `addMQTTTopicTooGUI(...)`
- `addLastTopicToGUI(...)`
- `addLastPayloadToGUI(...)`
- `addLastMessageAgeToGUI(...)`

## Notes

- The module uses a non-blocking state machine with retry logic.
- TX/RX topic logging is emitted only when `CM_ENABLE_VERBOSE_LOGGING=1`.
- Settings include:
  - `MQTTEnable` (bool)
  - `MQTTHost`, `MQTTPort`, `MQTTUser`, `MQTTPass`, `MQTTClientId`
  - `MQTTBaseTopic`
  - `MQTTPubPer` (publish interval in seconds; `0` means publish-on-change for send helpers)
  - `MQTTListenMs` (listen interval in ms; `0` means every loop)
