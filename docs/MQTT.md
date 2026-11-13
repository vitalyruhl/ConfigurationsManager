# MQTT Module

This repository provides an **optional** MQTT helper module.

- Module header: `src/mqtt/MQTTManager.h`
- You must explicitly include it from your sketch.
- You must add `PubSubClient` to your project dependencies.
- The recommended init pattern is **Option A**: call `mqtt.attach(ConfigManager)` from `setup()` before `loadAll()`. Attach registers baseline settings once and creates the default layout; custom placement and runtime fields remain explicit.

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
    // Layout + MQTT settings registration (MQTT + MQTT-Topics pages/groups).
    mqtt.attach(ConfigManager);

    // Optional: place baseline MQTT settings into a custom card/group.
    mqtt.addMqttSettingsToSettingsGroup(ConfigManager, "MQTT", "MQTT Settings", 40);

    // Registers runtime provider only (live fields are explicit).
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
    mqtt.addTopicReceiveFloat("power_w", "Power", "device/power", &powerInW, "W", 1, "none");

    // JSON example: topic "tele/tasmota/SENSOR" => {"ENERGY":{"Total":12.34}}
    mqtt.addTopicReceiveFloat("energy_total", "Energy Total", "tele/tasmota/SENSOR", &powerInW, "kWh", 3, "ENERGY.Total");

    // GUI entries are explicit
    mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt");

    // Settings placement (MQTT Topics tab)
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "power_w", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 50);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "energy_total", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 50);

    // Live placement
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "power_w", "mqtt", "MQTT-Received", 1); // card only (no group)
    mqtt.addMqttTopicToLiveGroup(ConfigManager, "energy_total", "mqtt", "MQTT-Received", "MQTT-Received", 2);
}
```

## Client ID

- The Client ID must be **unique** per device on the broker.
- It is used as a **fallback base topic** when `MQTTBaseTopic` is empty.
- If not set, the module auto-generates one from the MAC address, e.g. `ESP32_AABBCCDDEEFF`.

## Settings for receive topics (MQTT Topics tab)

`addTopicReceive*` only defines items. Settings are explicit:

- Call `addMqttTopicToSettingsGroup(...)` to create two settings under the **MQTT Topics** tab:
  - `<Label> Topic`
  - `<Label> JSON Key`
- Changes apply immediately on Save/Apply (subscriptions update on the fly).
- If you never call `addMqttTopicToSettingsGroup(...)`, defaults are used and **no settings** are created.

## GUI / Runtime helpers

- `addMQTTRuntimeProviderToGUI(...)` registers the runtime provider only.
- Receive items are shown **only** when explicitly placed via `addMqttTopicToLiveGroup(...)`.
- `getLastTopic()`, `getLastPayload()`, `getLastMessageAgeMs()` can be exposed via runtime providers.
- `addMqttTopicToSettingsGroup(...)` registers the MQTT receive-topic settings in the Settings UI (MQTT Topics tab).

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

Notes:
- MQTT is an optional module. The hooks are discovered only in `namespace cm` so the core does not require global symbols when MQTT is not included.
- This is why `onWiFiConnected()` can be global (core), while MQTT hooks must live in `namespace cm`.

If you implement these hooks in the same translation unit (Arduino sketch), define this before including the header:

```cpp
#define CM_MQTT_NO_DEFAULT_HOOKS
```

Example:

```cpp
namespace cm {
void onMQTTConnected()
{
    CM_LOG("[MQTT][I] connected");
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
- `uptimeMs` and `uptimeHuman` are included in each payload by default.
- ESP System-Info includes `resetReason` from the SDK.
- Set `CM_MQTT_WIFI_UPTIME=0` to omit duplicate uptime fields from WiFi
  System-Info while retaining them in ESP System-Info; the default is `1`.

## Publish helpers

- `publishTopic(id)` publishes a receive item value to `<base>/<id>`
- `publishExtraTopic(id, topic, value)` publishes a custom value to a custom topic
- `publishExtraTopicLazy(id, topic, callback)` publishes a custom value built by
  `callback` only after the id/topic, MQTT connection, and interval checks pass.
- Interval is taken from `MQTTPubPer` (seconds). Use the `Immediately` variants to bypass it.
- Defaults (unless overridden by parameters):
  - Non-bool: retain=true, qos=0
  - Bool: retain=false, qos=1
  - Immediately variants: qos=1
- You can override retain/qos via overloads that accept `(retained, qos)`.
- PubSubClient only supports QoS 0 for publish. If qos != 0 is requested, a warning is logged and QoS 0 is used.
- `publishAllNow(retained)` publishes System-Info and all receive items immediately.
- `clearRetain(topic)` clears the retained message by publishing an empty retained payload.

## Subscriptions

- `subscribe(topic, qos)` / `unsubscribe(topic)` are available for direct topic filters.
- `subscribeWildcard(topicFilter)` supports `+` / `#` wildcards.

## Buffer size (important for Tasmota JSON payloads)

`PubSubClient` uses a fixed packet buffer for RX/TX. Larger JSON payloads (for example
`tele/<device>/SENSOR` from Tasmota) may exceed small defaults and will then not be processed.

Defaults in this module:
- `CM_MQTT_DEFAULT_BUFFER_SIZE = 1024` bytes
- The default is applied automatically in `MQTTManager` constructor.
- You can still override at runtime with `mqtt.setBufferSize(...)`.

Build-time override (example in `platformio.ini`):

```ini
build_flags =
  -DCM_MQTT_DEFAULT_BUFFER_SIZE=1536
```

Notes:
- Runtime system-info publishing may grow the buffer if needed.
- The module keeps the grown size and does not shrink it again during operation.

## Optional bounded transport

`CM_MQTT_BOUNDED_IO` defaults to `0`, retaining the legacy transport. Define it
as `1` consistently in the consumer build to select complete-frame receive
handling and a bounded transmit queue. This mode requires a numeric IPv4 broker
address. TCP connect is bounded to 100 ms plus up to one second for CONNACK;
it is not a DNS-capable transport.

The receive path assembles complete MQTT frames without waiting in the
application loop, expires partial frames after 30 seconds and closes malformed
connections. The transmit FIFO retains unsent bytes across transient pressure
and positive short writes. The MQTT settings expose `TX Queue Size (bytes)`
(default 4608, effective range 4608-16384) and `TX Stall Timeout (ms)`
(default 5000, effective range 2000-120000). Values outside these ranges are
clamped when applied. The default queue uses fixed storage; a larger queue
additionally allocates its configured size on the heap. Allocation failures
retain the previous capacity and are retried; a resize waits until the FIFO is
empty and never discards its pending bytes. Inspect the effective capacity as
`getTxQueueUsed() + getTxQueueFree()`.

Pending bytes expire after the configured time without a positive TCP send. A
successful partial send renews that stall deadline; newly queued bytes and
transient errors do not. Continuous progress can therefore keep the queue
nonempty for longer than the timeout. This is neither an absolute packet-age
limit nor a broker-delivery guarantee. By default, application publishes are
rejected before writing when the queue cannot hold the packet plus 64 bytes of
control headroom. `canPublish(topicBytes, payloadBytes)` is an advisory snapshot.
Rejected publish attempts are counted by `txRejectedPublishes`, including
invalid arguments or insufficient PubSubClient buffer space. Unexpected
transport overflow, a zero-byte send or a definitive socket error closes the
connection and clears pending bytes. Headroom does not guarantee timely
control-packet delivery. Bounded mode does not support
`MQTT_MAX_TRANSFER_SIZE`.

### Optional discard of older queued publishes

By default, the bounded transport keeps existing queued data and rejects a new
publish when it cannot fit. Applications may instead explicitly allow older
queued publishes to be discarded:

```cpp
// Call during setup. The default is false.
mqtt.setDropOldestQueuedPublishes(true);
```

When enabled and the new publish needs queue space or a packet-tracking slot, the transport discards
the oldest eligible, completely unsent PUBLISH packets across all topics until
the new packet and control headroom fit. This applies to all publishes,
including retained messages, events, command-like data and logs. Enabling or
disabling the option does not itself discard any messages. The option only
affects the bounded transport; it has no effect with the legacy transport.
With dropping enabled, `canPublish(topicBytes, payloadBytes)` also considers
space reclaimable from eligible packets. It does not discard anything or
reserve space; the result may change before the actual publish.
Tracking is limited to 64 queued segments; unknown existing data may be grouped
as a protected segment and cannot be selected for dropping.

Partially transmitted packets and MQTT control packets are preserved. If
there are not enough safely removable packets or another capacity limit
prevents admission, the new publish returns `false` and already queued data
is retained. Once admission has discarded older packets and enqueued the new
one, a subsequent TCP failure can still make that publish return `false`;
the discarded packets cannot be recovered. Bytes already passed to TCP cannot
be removed from the TCP stream. An independent socket failure or stall timeout
can still close the connection.

**A `true` publish result means accepted locally at that time, not received
by the broker.** With this option enabled, a later publish can cause an older
publish that already returned `true` to be discarded. Neither a `true`
return nor this option guarantees broker delivery.

`getBoundedIoStats().txDroppedPublishes` and `txDroppedBytes` count removed
PUBLISH packets and their encoded MQTT packet bytes separately from
`txRejectedPublishes`; as with the other transport counters, they are
cumulative across reconnects. Applications that cannot lose messages should
leave dropping disabled and handle `false` results, for example by retrying
later. A larger TX queue is another option when heap capacity allows it.
These counters are `uint32_t` and wrap after their maximum value.

```cpp
const auto& stats = mqtt.getBoundedIoStats();
uint32_t rejected = stats.txRejectedPublishes;
uint32_t dropped = stats.txDroppedPublishes;
uint32_t droppedBytes = stats.txDroppedBytes;
```

For notification of drops after registration, an application can register a
callback:

```cpp
mqtt.onDroppedMessage([](uint32_t messageCount, uint32_t byteCount) {
  // Handle the count and encoded MQTT packet bytes discarded since the last notification.
});
```

The callback runs deferred in `mqtt.loop()`, no earlier than the next call,
and may combine several drops. It receives counts and encoded byte totals,
not the affected topics or payloads. Historical drops before registration are
not replayed; use the cumulative counters to inspect them. Publishing again
from the callback is supported; any additional drops caused there are reported
on a later `mqtt.loop()`. Registering or replacing a callback establishes a
new notification baseline. Use `mqtt.onDroppedMessage({})` to unregister it.
In legacy transport mode this callback has no effect. Keep callback work short
and nonblocking. `MQTTManager` is not thread-safe: run `loop()`, publishes,
setters and callback registration in the same application context.

Transmit pumping shares at most 64 send attempts and 1000 microseconds per
application loop, separately from the receive budget. During synchronous
CONNACK wait, retries renew once per elapsed 1000-microsecond epoch rather than
once per poll. These budgets bound work between nonblocking socket calls;
they are not a hard real-time guarantee for the socket implementation.

Transport counters distinguish recovered pressure/short writes from fatal
failure, expiry and overflow. Closing a socket does not establish that bytes
already queued in TCP have been cancelled. Session replacement and delayed
CONNECT behavior require separate device/broker evidence. Neither this mode nor
short device observations establish that every MQTT interruption is resolved.

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
    CM_LOG((String("[TASMOTA][E] ") + t + " => " + msg).c_str());
}
} // namespace cm
```

## Advanced example

```cpp
// Register runtime provider
mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt");

// Explicit live entries
mqtt.addMqttTopicToLiveGroup(ConfigManager, "boiler_temp_c", "mqtt", "MQTT-Received", 1); // card only
mqtt.addMqttTopicToLiveGroup(ConfigManager, "powermeter_power_in_w", "mqtt", "MQTT-Received", "MQTT-Received", 2);

// Runtime provider for "other infos"
ConfigManager.getRuntime().addRuntimeProvider("mqtt", [](JsonObject& data) {
    data["lastTopic"] = mqtt.getLastTopic();
    data["lastPayload"] = mqtt.getLastPayload();
    data["lastMsgAgeMs"] = mqtt.getLastMessageAgeMs();
}, 3);
```

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `cm::MQTTManager::attach` | `attach(ConfigManagerClass& configManager, const char* basePageName = "MQTT")` | Attaches MQTT manager to ConfigManager and registers settings/runtime integration. | Recommended init path. |
| `cm::MQTTManager::begin` / `loop` / `disconnect` | `begin()`<br>`loop()`<br>`disconnect()` | Starts and maintains MQTT state machine connection lifecycle. | `loop()` or `update()` must run continuously. |
| `cm::MQTTManager::publishTopic` / `publishTopicImmediately` | `publishTopic(...)` (6 overloads)<br>`publishTopicImmediately(...)` (6 overloads) | Publishes registered receive-item values to MQTT topics. | Overloads cover retained/qos and `ConfigManager` variants. |
| `cm::MQTTManager::publishExtraTopic` / `publishExtraTopicImmediately` | `publishExtraTopic(...)` (6 overloads)<br>`publishExtraTopicImmediately(...)` (6 overloads) | Publishes custom values to explicit topics. | Useful for ad-hoc telemetry. |
| `cm::MQTTManager::publishExtraTopicLazy` / `publishExtraTopicImmediatelyLazy` | `publishExtraTopicLazy(...)` (6 overloads)<br>`publishExtraTopicImmediatelyLazy(...)` (6 overloads) | Builds custom payloads from callbacks only when a publish will be attempted. | Use for values whose payload construction allocates memory or is relatively expensive. |
| `cm::MQTTManager::addTopicReceive*` | `addTopicReceiveFloat(...)`<br>`addTopicReceiveInt(...)`<br>`addTopicReceiveBool(...)`<br>`addTopicReceiveString(...)` | Registers inbound MQTT topics and parsing targets. | Pair with settings/live placement helpers. |
| `cm::MQTTManager` UI helpers | `addMqttSettingsToSettingsGroup(...)` (2 overloads)<br>`addMqttTopicToSettingsGroup(...)` (2 overloads)<br>`addMqttTopicToLiveGroup(...)` (2 overloads)<br>`addMQTTRuntimeProviderToGUI(...)`<br>`addLastTopicToGUI(...)`<br>`addLastPayloadToGUI(...)`<br>`addLastMessageAgeToGUI(...)` | Places MQTT data/settings into Settings and Live UI. | Explicit placement model; nothing is auto-shown in Live without helper calls. |

## Notes

- The module uses a non-blocking state machine with retry logic.
- TX/RX topic logging is emitted only when `CM_ENABLE_VERBOSE_LOGGING=1`.
- Settings include:
  - `MQTTEnable` (bool)
  - `MQTTHost`, `MQTTPort`, `MQTTUser`, `MQTTPass`, `MQTTClientId`
  - `MQTTBaseTopic`
  - `MQTTPubPer` (publish interval in seconds; `0` means publish-on-change for send helpers)
  - `MQTTListenMs` (listen interval in ms; `0` means every loop)
