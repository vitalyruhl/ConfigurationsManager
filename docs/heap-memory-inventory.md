# Heap memory inventory and refactoring options

Snapshot: 2026-10-18. Scope: ConfigurationsManager (CM), with SolarBatteryTelemetry
(SBT) as the concrete ESP32 consumer. This is a source-backed first overview,
not a measured allocation census or an implementation plan.

## Evidence and interpretation

- CM checkout: `C:/Daten/_Codding/ESP32/ConfigurationsManager`,
  `feature/hybrid-json-output`, intentionally dirty, version 4.6.1 development.
- SBT checkout: `C:/Daten/_Codding/ESP32/SolarBatteryTelemetry`. Paths prefixed
  `SBT:` below are relative to that root; other paths are relative to CM.
- Current saved source is authoritative. Older incident evidence describes the
  named firmware, not proof that the current candidate has the same footprint.
- Static RAM means retained object/array storage; task stacks created with
  `xTaskCreate` consume heap at task creation and remain reserved afterward.
  Temporary local arrays consume task stack; temporary JSON pools and long
  `String` contents consume heap. Flash assets do not consume their full size
  as live heap merely because they are linked.
- `StaticJsonDocument<N>` is a compatibility name under ArduinoJson 7, not
  proof of an N-byte stack pool. CM's current JSON paths use dynamic allocation.
  See `docs/todo_http_delivery_panic.md:50`.
- No additive total is given: static storage, retained heap and simultaneous
  request peaks are different quantities. A short-lived saving does not
  enlarge an already-created task stack's allocation.
- No builds, device actions, stack resizing or feature changes were performed
  for this inventory. SolarInverterLimiter was not audited.

The recorded failure is real exhaustion/fragmentation, not just a large JSON
object: incident `961dc7e1` entered the page-index handler with DEFAULT
free/largest=976/500 bytes and reached JSON-ready with 640/172; integrity was
`ok`, then a 32-byte header allocation aborted. Earlier 1,536-byte requests
failed with several KB free and largest eligible block=1,524.
The pinned TCP path can request 1,536 bytes (MSS 1,436 + aligned header 76 +
pbuf structure 24), even for a 512-byte application write; NODELAY does not
remove that path in this build. HTTP versus MQTT attribution of each event
remains unproven. See `docs/todo_http_delivery_panic.md:145-151`.

## Ownership and retained memory

| Area | Owner and source evidence | Size / lifetime / actual off behavior |
| --- | --- | --- |
| BLE transport object | CM implementation; SBT constructs a static instance. `src/bluetooth/BluetoothManager.h:19,272-280`; `SBT:src/main.cpp:18`. | Inline rings: 16 events + 8 commands, each with 272-byte payload; two additional desired/worker commands also each carry 272 bytes. Payload arrays alone account for 7,072 bytes of object storage, before headers, locks, dispatch state and alignment. This is static RAM in SBT, not recurring heap allocation. Emptying queues does not release inline storage. |
| BLE worker | CM `BluetoothManager.h:57-69,220-261`; SBT calls `begin()` at `src/main.cpp:157`. | 8,192-byte dynamically allocated task stack plus task bookkeeping. Created even when the enabled setting is false. Runtime disable/pause cancels work; no worker deletion/deinit path is present. Do not equate disabled BLE with recovered 8 KiB. |
| NimBLE radio/host | CM `BluetoothManager.h:186-212`; consumer dependency/configuration in `SBT:platformio.ini:18-26`. | Lazy radio initialization; client allocation on first scan/connect. SBT removes peripheral/broadcaster roles at compile time. Host stack, mbufs and SDK allocations are additional and unmeasured here. Disable/pause retains initialized radio resources. |
| Runtime metadata and providers | CM owns vectors; apps decide what to register. `src/runtime/RuntimeManager.h:136-176,290-306`; `SBT:src/main.cpp:88-114`; `SBT:src/telemetry/MqttService.h:79-125`. | Each field contains 10 String objects, universal control/alarm fields and a style vector, even for a plain numeric field. Registered vectors and any allocated String contents remain. UI invisibility or a smaller subscription does not erase registrations/capacity. SBT source intentionally targets at most 64 UI fields; current live count/capacity was not measured. |
| Layout/settings registry | CM `src/ConfigManagerLayout.cpp:69-157`; metadata registration `src/runtime/RuntimeManager.cpp:71-89`. | Page/card/group names and placements are stored in addition to runtime metadata. Defaults and registered Config objects are app choices. Repeated identifiers and vector growth are plausible retained costs, with no measured byte total. |
| MQTT transport buffer | CM `src/mqtt/MQTTManager.h:23-24,443-444,768,1482`; SBT `src/telemetry/MqttService.h:97`. | CM default 1,024 bytes; SBT explicitly selects 1,536 bytes. PubSubClient buffer is retained heap. System-info may grow it to at most 2,048 and explicitly does not shrink it (`MQTTManager.h:1675-1685`). Runtime MQTT off does not establish buffer recovery. |
| App MQTT state | SBT `src/telemetry/StatusPublisher.h:12`, `MqttConfigMailbox.h:10-17`, `TelemetryPublisher.h:14-16`. | Four inline 512-byte status payloads = 2,048 payload bytes, plus state. Fixed copies of root/broker/user/password/client ID live in configuration/mailbox/view objects. These are app design costs, separate from CM's MQTT socket buffer. Do not remove guarded command/status semantics to save memory. |
| GUI logging | CM `src/logging/LoggingManager.h:136-160`, `LoggingManager.cpp:81-154`. | Optional output object: startup deque defaults to 30 entries; pending deque to 200; 8 sends/tick. Strings are variable length, not a byte-bounded log budget. SBT already compiles this out (`platformio.ini:23`); the guarded constructor requests 20 entries (`main.cpp:262-266`). No additional SBT saving from disabling it again. |
| WiFi/lwIP/AsyncTCP | CM WiFi module and shared SDK; both HTTP and app MQTT use the network. `src/wifi/WiFiManager.h:5`; incident source/ELF evidence above. | Driver pools, task stacks, per-socket state and pbufs are outside JSON ownership. Exact census unknown. Reducing application payload/window sizes alone does not guarantee smaller TCP allocations. WiFi removal also removes the collector's MQTT/UI/OTA connectivity. |

### Target ABI size check

A compile-only size probe using the installed `xtensa-esp32-elf-g++` and the
current ESP32 `WString.h` / `RuntimeManager.h` confirms: `sizeof(String)=16`,
`sizeof(RuntimeFieldMeta)=244`, `sizeof(RuntimeFieldStyle)=12`, and the metadata
vector handle itself is 12 bytes. This does not measure live field count or
vector capacity. It excludes heap storage for long text and populated style
rules. At an illustrative 64 records, the records alone occupy 15,616 bytes
(15.25 KiB): 10,240 bytes of String objects, 768 bytes of style-vector handles,
and 4,608 bytes of other members/padding. Short Strings may keep their text
inline; they do not each imply a separate heap allocation.

The same target probe confirms a default ArduinoJson variant pool of 1,024
bytes (`ARDUINOJSON_POOL_CAPACITY * ResourceManager::slotSize`). The 64-bit
host configuration uses 4,096 bytes; host pool measurements must not be used
as ESP32 byte savings.

The refactoring opportunity is shared immutable identifiers/text plus optional
control/alarm/style details. Plain measurement fields currently carry the
same complete record as sliders/buttons. API mutation and text ownership must
remain valid; replacing every String with a borrowed pointer is not safe.
## Peaks and duplication

| Trigger | Proven overlapping objects | Scope and reduction opportunity |
| --- | --- | --- |
| Runtime snapshot | Selected provider/control vector copies, JSON document and output String. `src/runtime/RuntimeManager.cpp:151-188`. | Per request/push; filtering reduces work and payload, but the generic JsonObject provider API still requires a document. Replacing only metadata serialization does not remove this runtime-value peak. |
| Runtime initialization | Full metadata copy before layout registration. `src/runtime/RuntimeManager.cpp:40-51`. | Startup-only peak. Avoiding a copy requires preserving locking/reentrancy and registry lifetime; it does not reduce the retained registry itself. |
| MQTT System-Info | Before the follow-up: two JSON documents, both serialized Strings and topics overlapped. Current `src/mqtt/detail/SystemInfoJson.h` releases each document before publication and the ESP payload/topic before WiFi construction; collected system information still spans the call. Wrapper: `src/mqtt/MQTTManager.h:1664-1694`. | On connection/root changes and every 60 s (`1884-1894,1944-1946`). Sequential lifetimes now pass five host fixtures and the SBT ESP32 build. Host64 ArduinoJson peak falls 8401 -> 4227 bytes (long-text fixture 8994 -> 4820), with allocation count unchanged at 10; these exclude String/TCP memory and are not target savings. |
| App telemetry formatting | Local `payload[1025]` for capabilities, and `payload[512], previous[512]` in another path. `SBT:src/telemetry/TelemetryChannels.h:8`; `TelemetryPublisher.h:109,116`. | Task-stack occupancy in separate paths, not a 2 KiB permanent heap allocation. Fixed buffers reduce allocator churn but move the budget onto the stack. |
| HTTP response construction | Output String, response object, response headers, TCP queue/pbufs. `src/web/WebServer.cpp:401-459,1165-1214`; `src/web/BoundedResponse.h`. | Response ownership lasts through delivery. Moving a String avoids a copy; it does not eliminate the required output nor TCP/header allocations. Tiny response/header allocations can still fail late. |
| Browser bootstrap | Theme/app identity/index/layout/meta/OTA/runtime/socket work previously overlapped. `docs/todo_http_delivery_panic.md:155`; current `webui/src/App.vue` and `components/RuntimeDashboard.vue`. | Sequential bootstrap passed unit, mocked-browser and firmware-build validation. Target-device heap benefit remains unmeasured. It cannot serialize independent browsers or guarantee that the final TCP ACK has been handled when the browser body resolves. |
| Browser sockets | Runtime and GUI/events clients can use two connections to the same CM `/ws` handler. `src/ConfigManager.h:2963-3035`; subscription modes in `src/runtime/detail/RuntimeSubscriptions.h:7-27`. | Each live connection adds TCP/WebSocket/subscription state and possible queued payloads. One server-side handler does not mean one connection. GUI logging compile-off does not by itself prove that the browser's events connection is unnecessary. |

Direct JSON output for pages/layout/meta/app identity and CSS static/snapshot
ownership are the current development direction; generic JsonObject providers
remain supported. Treat candidate source/tests separately from device acceptance.
No permanent full JSON cache is recommended: the metadata cache uses weak
ownership so it only reuses a snapshot while another owner retains it
(`src/runtime/RuntimeManager.h:278-281`).

## Existing switches: effective scope matters

Defaults are from `src/ConfigManagerConfig.h:3-106`. SBT explicitly enables
core/verbose diagnostics and disables GUI logging; it does not override the
other listed CM defaults in `platformio.ini` or `platformio.local.ini`.
A defined flag is not sufficient evidence that all related fields disappear.

| Existing supported switch / default | What it actually gates; loss and expected memory effect |
| --- | --- |
| `CM_EMBED_WEBUI=1` | `src/web/WebServer.h:12`, `WebServer.cpp:969-999`: embedded UI asset routes. Setting 0 removes embedded assets and local UI delivery. Primarily flash; does not remove server/runtime/WiFi registries or all API sockets. Live heap saving unknown. |
| `CM_ENABLE_WIFI=1` | `src/wifi/WiFiManager.h:5`, `WebServer.cpp:1031`: CM WiFi integration. Setting 0 is not a promise that an app's direct WiFi/MQTT usage or SDK buffers disappear. SBT requires network operation. |
| `CM_ENABLE_OTA=1` | `src/ota/OTAManager.h:5-84`, `OTAManager.cpp:333`: Arduino/web OTA logic and upload context. Setting 0 loses CM OTA; likely code/registration saving and OTA-time peak avoidance. Idle RAM saving needs measurement; no permanent OTA task size established. |
| `CM_ENABLE_SYSTEM_PROVIDER=1` | `src/runtime/RuntimeManager.h:309-316`, `RuntimeManager.cpp:481-625`: built-in runtime provider and timing state. Registration is opt-in via `enableBuiltinSystemProvider()`. Disabling removes that capability, not app providers or MQTT System-Info. Potential reduction depends on registrations actually present. |
| `CM_ENABLE_SYSTEM_TIME=1` | `src/runtime/RuntimeManager.cpp:540,607`: time fields inside the built-in provider. Setting 0 does not disable the application's clock or all time services. Small/unknown baseline saving; reduced output only when that provider is used. |
| `CM_ENABLE_THEMING=1` | `src/web/WebServer.cpp:218`, `src/ConfigManager.h:63-71`: theme CSS response/default branch. Does not remove RuntimeFieldMeta's style member. Mainly code/assets and conditional CSS work; retained heap saving unknown. |
| `CM_ENABLE_STYLE_RULES=1` | Declared flag; current source references only select the default CSS branch in `ConfigManager.h:63-71`. Do not claim that 0 removes style vectors, rules or all styling serialization. Both shown default CSS literals are empty. |
| `CM_ENABLE_USER_CSS=1` | Declared in ConfigManagerConfig.h, with no other reference found in current `src/`. No demonstrated memory-saving effect from setting it to 0. A complete compile-out boundary would be new work. |
| `CM_ENABLE_LOGGING=0`, `CM_ENABLE_VERBOSE_LOGGING=0` | Core/module diagnostic macros; separate from LoggingManager. SBT uses 1/1 for the active investigation. Removing verbose probes may reduce formatting/churn/flash but also loses essential allocation evidence. Do not disable diagnostics before comparing candidates. |
| `CM_DISABLE_GUI_LOGGING=0` | Set 1 to compile out GuiOutput. SBT already uses 1; no further saving available there. Other output objects are separately app-created. |
| `CM_LOGGING_LEVEL=CM_LOG_LEVEL_TRACE` | Caps LoggingManager output, not a general transport/registry compile-out switch. Runtime level changes do not delete existing queues/outputs. |

Removed/always-on boundaries:

- Defining `CM_ENABLE_WS_PUSH` is an explicit build error; CM defines it as 1.
  `disableWebSocketPush()` only clears `wsEnabled`; it neither deletes the
  handler nor registered subscriptions/callback storage
  (`src/ConfigManagerConfig.h:19-27`; `ConfigManager.h:3039-3040`).
- Dynamic visibility is always enabled
  (`src/ConfigManagerConfig.h:104-106`).
- No supported global CM compile-out flag for runtime controls, runtime schema,
  BLE or MQTT is established by ConfigManagerConfig.h. BLE/MQTT are optional
  consumer integrations; avoiding construction/use differs from a feature
  runtime setting.
- Proposed new flags below are proposals, not existing configuration options.

## Ranked candidates and rough saving estimates

Ranges are decision aids, not predicted device results. KiB means 1,024 bytes.
No rows may be summed without a simultaneous lifetime measurement.

| Priority / candidate | Permanent RAM or retained heap | Temporary peak heap / flash | Trade-off, effort/risk, confidence and next measurement |
| --- | --- | --- | --- |
| 1. Preserve selected runtime demand and validate serial bootstrap on device | Approximately 0 baseline saving by sequencing alone. | From 0 to the eliminated overlap of payload/document/socket-start work; byte range unknown until measured. Flash not a meaningful target here. | Low structural risk; startup may take longer. High confidence that overlap decreases, unmeasured device effect. Record DEFAULT and allocation-capability largest blocks at every boot phase with one browser and BLE+MQTT active. |
| 2. Share runtime/events browser connection | Removes one connection's retained state while UI is open; exact bytes unknown. | Rough conditional 0-3 KiB of queued pbuf peak if it avoids 0-2 simultaneous 1,536-byte allocations, plus unquantified socket/WebSocket objects. Not a guaranteed 3 KiB baseline saving. | Medium effort/risk: multiplex runtime selections, events, logs, reconnect and heartbeat. Preserve non-log events. Measure before/after open/close and queue counts; SDK tasks shared by connections must not be counted twice. |
| 3. Sequential MQTT System-Info lifetimes (implemented); direct writer remains optional future work | Approximately 0 baseline unless buffer sizing changes separately. | Release one document pool + first payload before constructing the second. The target default variant pool is 1 KiB (confirmed by compile-only size probe); 512 is not the pool size in ArduinoJson 7. Avoiding one such pool plus an illustrative 0.2-0.6 KiB first payload means roughly 1.2-1.6 KiB less overlap, before document-owned text and allocator overhead. Actual device peak improvement remains unmeasured. The earlier 4.2-4.6 KiB illustration used a 64-bit host pool and is not an ESP32 prediction. | Low/medium effort, preserve topics/escaping/retained semantics. Sequential scopes are simpler than a new provider API. Measure document allocation footprint and function minimum heap at publish/connect/60-second refresh. |
| 4. Compact immutable schema: shared IDs/string tables, lightweight field kind, separate optional control/style payload | Rough design range 2-8 KiB retained storage for a consumer with about 64 fields; excludes heap-allocated long text and layout duplication. Conditional arithmetic: replacing five 16-byte String handles with five 4-byte references saves 3,840 bytes across 64 records. The target size probe confirms those handle sizes; field count and a replacement layout still need validation. | Could also reduce registry-copy/startup peaks; unknown additional amount. Flash-resident immutable text can trade RAM for flash, with no net flash estimate yet. | High effort/risk: String ownership, public API compatibility, overrides, dynamic labels, synchronization. Medium/low confidence range. Target String uses inline/heap representation (`C:/Users/Vital/.platformio/packages/framework-arduinoespressif32/cores/esp32/WString.h:298-319`); the size probe above establishes current record size; obtain live field count/capacity and map storage before design approval. Do not assume every short String owns a heap block. |
| 5. Avoid temporary registry copies / stream stable snapshots | Baseline approximately 0; permanent full caches can make it worse. | Between 0 and one measured registry snapshot/document/output copy. Current byte bound unknown. | Medium/high risk: async response ownership, mutations, locks and callback reentrancy. Two passes may increase CPU/lock time; snapshots stabilize reads but cost RAM. Measure copy sizes, allocations and lock durations rather than returning pointers into mutable vectors. |
| 6. Offer real compile-out profiles for unused features | Known SBT GUI-log opportunity: 0, already disabled. Other savings unknown until map/heap comparison. New runtime/control/style compile-out flags could remove object members only if implemented across storage/API boundaries. | Embedded WebUI removal mainly saves flash; source/code presence alone is not a live-heap estimate. | Medium effort/risk and larger build matrix. First identify unused app features. Flag names above are supported; any broader profile/flag is hypothetical. Compare one feature at a time at identical firmware/device state. |
| 7. Right-size bounded app state and MQTT buffer from contract maxima | SBT status payload arrays contain 2 KiB; reclaimable part unknown. Hypothetical MQTT 1,536 -> 1,280 saves 256 retained bytes, only if all encoded packets/topic overhead and RX commands fit. | Reduces baseline, not necessarily TCP's separate 1,536-byte allocation. Fixed shared buffers may reduce churn but require explicit concurrency ownership. | Medium risk to telemetry/commands/retained cleanup. Check worst-case valid encoded payloads and topic lengths first; do not shrink on average samples. No changes proposed to existing command safety behavior. |
| 8. Entirely omit unused BLE integration | If an app needs no BLE: at least 7,072 inline payload bytes plus unmeasured object metadata, and 8,192 worker-stack bytes plus unmeasured NimBLE heap/task allocations. These are different RAM categories. | Also removes transport/protocol flash; exact linked saving unknown. | High functional loss for SBT: removes its purpose (BMS/Victron acquisition), so not recommended there. Useful for other consumers that never instantiate/start BLE. Runtime disable is not this optimization. No stack shrinking without representative high-water data. |

The highest-value structural direction is to reduce retained schema duplication
and simultaneous live objects while retaining generic provider compatibility.
For the immediate failure, compare the software-validated bootstrap/output
candidate including the MQTT lifetime change on device; a shared-connection
design remains a separate proposal.
Only a measured improvement in the constrained capability's largest block
establishes useful allocation headroom; INTERNAL totals alone can include memory
that the failed DEFAULT allocation cannot use.

## Measurement needed before selecting a redesign

1. Use identical firmware, registered schema, persisted settings and traffic.
   Record before browser, after each socket, worst runtime snapshot, MQTT
   connection/refresh, BLE scan/reconnect and after browser close.
2. Capture DEFAULT free/minimum/largest, failing-capability free/largest,
   registry sizes/capacities, response lengths and outstanding queue/pbuf work.
   SBT's app-owned failed-allocation hook avoids String/JSON/log-manager allocation
   (`SBT:src/diagnostics/HeapFailureDiagnostics.h:9-35`); Health separates DEFAULT
   figures from `ESP.getFreeHeap()` (`SBT:src/main.cpp:272-277`).
3. Compare one change at a time. Inspect linker map/static RAM separately from
   retained heap and peak heap. Measure all relevant stack high-water marks
   under concurrent BLE/MQTT/UI/OTA stress before considering stack sizes.
4. Require complete payload delivery and unchanged telemetry/control behavior;
   fewer allocations with truncated output or missed messages is not a saving.
