# Deferred investigation: MQTT verbose preview crash

Status: open, priority 1. Recorded from the owner's supplied report on 2026-10-17; the evidence below has not been independently reverified in this documentation-only task. No product fix is claimed.

## Reproduction context

Consumer: `C:\Daten\_Codding\ESP32\SolarInverterLimiter`, ESP32 nodemcu-32s, Arduino/PlatformIO, PubSubClient 2.8, ArduinoJson 7. Reported CM dependency: `554869da7efc98e0c1874e74bf0c78d4bcd1664a`.

Installed reference files: `.pio/libdeps/testesp/ESP32 Configuration Manager/src/mqtt/MQTTManager.h` and `src/ConfigManager.cpp` within that dependency. A future fix belongs in the CM source repository, not only in the consumer dependency cache.

Active flags: `CM_ENABLE_LOGGING=1`, `CM_ENABLE_VERBOSE_LOGGING=1`, `CM_DISABLE_GUI_LOGGING=1`, `FEATURE_MQTT_LOGGER_ENABLED=0`, `SIL_MOCK_TRANSPORT=1`. Keep both logging flags enabled; SIL uses them for its diagnostics. The owner reports that earlier `-UCM_ENABLE_*` options were removed because PlatformIO applied them after `-D`.

During an HA restart, missing measurements correctly stop SIL at zero (`meters_missing`, unknown price band). WiFiClient write errors precede the crash. SerialOutput is configured at Info with `lmg.attachToConfigManager(LL::Info, LL::Debug, "")`; CM verbose text is formatted before the downstream Debug filter.

## Reported crash evidence

```text
[INFO] [MOCK] [MOCKED!] demand=0W reason=meters_missing band=unknown limit=0W; no hardware TX
[56215][E][WiFiClient.cpp:429] write(): fail on fd 49, errno: 11, "No more processes"
[INFO] [FORECAST] subscribe=1 topic=homeassistant/sensor/electricity_price/schedule
[56979][E][WiFiClient.cpp:429] write(): fail on fd 49, errno: 11, "No more processes"
Guru Meditation Error: Core 1 panic'ed (LoadProhibited)
EXCVADDR: 0x00000000
PC: 0x4008b759
A2: 0x00000000
```

Reported matching ELF SHA256 prefix: `2ed925ee6272ef1d`. The owner reports an exact match with the available testesp firmware ELF and the following decoded backtrace:

```text
0x4008b756:0x3ffb1930 strlen
0x4018b2d9:0x3ffb1940 _svfprintf_r
0x4019230e:0x3ffb1c50 _vsnprintf_r
0x4019234a:0x3ffb1ce0 vsnprintf
0x400f5f45:0x3ffb1d20 ConfigManagerClass::log_verbose_message(char const*, ...)
0x40113217:0x3ffb1e70 cm::MQTTManager::publish(char const*, char const*, bool)
0x401082b3:0x3ffb1ed0 cm::MQTTManager::publishWithQos_(...)
0x40107691:0x3ffb1ef0 publishMqttNow()
0x40109770:0x3ffb21e0 loop()
0x400e5f0d:0x3ffb2290 loopTask(void*)
```

Reported disassembly places return PC `0x40113217` at the verbose TX payload-preview call. `payloadPreview.c_str()` is passed to `%s`; execution reaches `strlen(nullptr)`. The installed TX path around MQTTManager.h lines 1601-1617 constructs a dynamic `String`, trims it, truncates after 200 characters using substring/concatenation, and logs it before publish. RX around line 2108 has the related allocation pattern with an explicitly sized, potentially non-null-terminated payload. `ConfigManager.cpp` around lines 82-90 formats verbose messages using `vsnprintf` into a 256-byte stack buffer.

Failed String allocation is a plausible explanation, not a proven root cause of the null/invalid pointer. HA restart and errno 11 do not establish that cause. errno 11 is EAGAIN despite the reported error text.

## Bounded future implementation scope

- [ ] Verify the concrete TX preview failure against the matching source/ELF and inspect the closely related RX preview path.
- [ ] Replace dynamic preview String allocation with bounded storage; preserve the 200-character preview limit and intentional whitespace/truncation semantics.
- [ ] Respect RX payload length; never assume null termination or pass null to `%s`, including empty/invalid payloads.
- [ ] Compile out preview preparation when verbose logging is disabled.
- [ ] Preserve MQTT payloads, publish behavior, QoS/retain, callbacks, buffer sizes and settings. Do not redesign the logger/client or disable diagnostics as a workaround.
- [ ] Statically review the affected paths and record the focused user build/reproduction checks. Do not mark fixed until the owner confirms the result.

The owner's supplied constraints prohibit agent tests, builds, uploads, monitor and Git operations. Preserve user edits, version and flags. User performs build/flash/testing.

## Separate observations; no established shared cause

- Earlier web reload OOM: `operator new -> std::terminate -> AsyncWebServerRequest::beginResponse -> ConfigManagerWeb::setupAPIRoutes` in the async web task. See the separate [HTTP delivery/panic handoff](todo_http_delivery_panic.md).
- Later stable SIL sample: free heap 94844, minimum 61844, maximum allocation 42996 bytes. These are not crash-time measurements.
- `MQTTListenMs` can limit receive processing; reported default 500 ms and at most one PubSubClient packet per loop may permit backlog. The actual setting at the crash is unconfirmed; do not silently change it.
- Consumer-side bounded JSON arenas, stack Planning JSON and publish checks no more often than every 250 ms were reported. The supplied crash stack still points specifically to CM TX preview formatting.
