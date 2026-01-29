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
    mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt");

    // Optional: global hooks (define them in your sketch)
    // See "Callbacks" section below.
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
    mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt");
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

- `addMQTTRuntimeProviderToGUI(...)` registers the runtime provider only.
- Receive items are shown **only** when explicitly added via `addMQTTTopicTooGUI(...)`.
- `getLastTopic()`, `getLastPayload()`, `getLastMessageAgeMs()` can be exposed via runtime providers.
- `addMQTTReceiveSettingsToGUI(...)` registers the MQTT receive-topic settings in the Settings UI (MQTT Topics tab).

## Callbacks

Classic (PubSubClient-style) hooks remain available:
- `onConnected(...)`
- `onDisconnected(...)`
- `onMessage(...)`

Optional global hook voids (define in your sketch if you want them):
- `void onMQTTConnected()`
- `void onMQTTDisconnected()`
- `void onMQTTStateChanged(int state)`
- `void onNewMQTTMessage(const char* topic, const char* payload, unsigned int length)`

If you implement these hooks in the same translation unit (Arduino sketch), define this before including the header:

```cpp
#define CM_MQTT_NO_DEFAULT_HOOKS
```

Example:

```cpp
namespace cm {
void onMQTTConnected()
{
    CM_LOG("[MQTT][INFO] connected");
}
} // namespace cm
```

## Last Will (LWT)

- Default last-will message: `"offline"`
- Default topic: `<MQTTBaseTopic>/System-Info/status`
- Default: retain=true, qos=0
- Override via `setLastWill(topic, message, retained, qos)`
- On successful connect, the module publishes `online` (retained) to the same will topic to clear stale `offline`.

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
- Defaults (unless overridden by parameters):
  - Non-bool: retain=true, qos=0
  - Bool: retain=false, qos=1
  - Immediately variants: qos=1
- You can override retain/qos via overloads that accept `(retained, qos)`.
- PubSubClient only supports QoS 0 for publish. If qos != 0 is requested, a warning is logged and QoS 0 is used.
- `publishAllNow(retained)` publishes System-Info and all receive items immediately.
- `clearRetain(topic)` clears the retained message by publishing an empty retained payload.

## MQTT logging (LoggingManager)

If you use the advanced logging module, you can publish logs via MQTT:

- Output class: `cm::MQTTLogOutput` (`src/mqtt/MQTTLogOutput.h`)
- Plain-text payloads, not retained (stream)
- Optional retained "last" entries per level

Topic scheme (base = `<MQTTBaseTopic>`):
- `<base>/log/<LEVEL>/LogMessages` (unretained stream)
- `<base>/log/last/INFO`, `/WARN`, `/ERROR` (retained)
- `<base>/log/last/Custom` (retained, tag prefix "Custom")

Minimal example:

```cpp
#include "mqtt/MQTTLogOutput.h"

auto mqttLog = std::make_unique<cm::MQTTLogOutput>(mqtt);
mqttLog->setLevel(cm::LoggingManager::Level::Trace);
mqttLog->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
lmg.addOutput(std::move(mqttLog));
```

## Wildcard subscribe example (Tasmota errors)

MQTT wildcard rules:
- `+` matches a single topic level
- `#` matches all remaining levels (must be the last token)

Example: subscribe to all Tasmota error topics and filter in the callback:

```cpp
mqtt.subscribeWildcard("tasmota/#");

namespace cm {
void onNewMQTTMessage(const char* topic, const char* payload, unsigned int length)
{
    if (!topic || !payload || length == 0) return;
    String t(topic);
    if (!t.endsWith("/main/error")) return;

    String msg(payload, length);
    msg.trim();
    CM_LOG((String("[TASMOTA][ERROR] ") + t + " => " + msg).c_str());
}
} // namespace cm
```

## Advanced example

```cpp
// Register runtime provider
mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt");

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
- `setServer(...)`, `setCredentials(...)`, `setClientId(...)`, `setLastWill(...)`
- `setKeepAlive(...)`, `setMaxRetries(...)`, `setRetryInterval(...)`, `setBufferSize(...)`

Publish / subscribe:
- `publish(...)` (2 overloads), `subscribe(...)` (2 overloads), `unsubscribe(...)` (1 overload)
- `subscribeWildcard(...)` (1 overload)
- `clearRetain(topic)` (1 overload)
- `publishSystemInfo(...)` (1 overload), `publishSystemInfoNow(...)` (1 overload)
- `publishAllNow(...)` (1 overload)
- `publishTopic(...)` (6 overloads), `publishTopicImmediately(...)` (6 overloads)
- `publishExtraTopic(...)` (6 overloads), `publishExtraTopicImmediately(...)` (6 overloads)
  - Overloads cover `(retained, qos)` and `ConfigManager` variants where applicable.

Receive helpers:
- `addMQTTTopicReceiveFloat(...)` (1 overload)
- `addMQTTTopicReceiveInt(...)` (1 overload)
- `addMQTTTopicReceiveBool(...)` (1 overload)
- `addMQTTTopicReceiveString(...)` (1 overload)

GUI helpers:
- `addMQTTRuntimeProviderToGUI(...)` (1 overload)
- `addMQTTReceiveSettingsToGUI(...)` (1 overload)
- `addMQTTTopicTooGUI(...)` (1 overload)
- `addLastTopicToGUI(...)` (1 overload)
- `addLastPayloadToGUI(...)` (1 overload)
- `addLastMessageAgeToGUI(...)` (1 overload)

## Notes

- The module uses a non-blocking state machine with retry logic.
- TX/RX topic logging is emitted only when `CM_ENABLE_VERBOSE_LOGGING=1`.
- Settings include:
  - `MQTTEnable` (bool)
  - `MQTTHost`, `MQTTPort`, `MQTTUser`, `MQTTPass`, `MQTTClientId`
  - `MQTTBaseTopic`
  - `MQTTPubPer` (publish interval in seconds; `0` means publish-on-change for send helpers)
  - `MQTTListenMs` (listen interval in ms; `0` means every loop)
