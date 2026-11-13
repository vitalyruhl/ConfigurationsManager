# Optional Bluetooth transport

`bluetooth/BluetoothManager.h` is an explicit, header-only BLE transport. The core
`ConfigManager.h` never includes NimBLE and the library does not depend on it.
The consumer must pin `h2zero/NimBLE-Arduino@2.5.1`. See `examples/bluetooth` for
WiFi, configuration UI and OTA integration using CoreSettings/CoreWiFiServices.
No device address or protocol UUID is guessed by the example.

For an application-level **WiFi + OTA + BLE** example, see
[SolarBatteryTelemetry](https://github.com/vitalyruhl/SolarBatteryTelemetry)
(repository access required). It shows
network provisioning, configurable BLE battery/PV collectors, MQTT telemetry
and firmware updates together. Its setup and hardware limitations are described
in the example rather than assumed from a successful build.

## Registration and configuration

Create one static-lifetime `cm::BluetoothManager`. Only one manager may own the
radio. Call `attach(ConfigManager)` before `ConfigManager.loadAll()`, then
configure peers and call `begin()`. Repeated attachment does not duplicate settings.
`settings().enabled` has the persisted key `BluetoothEnable` and defaults false;
`begin()` with it disabled creates the bounded worker without initializing radio.
Call `begin()` before starting the web server/concurrent application tasks.
A module-local setting adapter forwards both persisted loads and normal setting
callbacks into an atomic enabled snapshot; worker, loop and runtime read that
snapshot. Do not replace this setting's module-owned callback.

```cpp
#include <bluetooth/BluetoothManager.h>
static cm::BluetoothManager ble;
// In setup, before loadAll():
ble.attach(ConfigManager);
ble.addBluetoothSettingsToSettingsGroup(ConfigManager, "Devices", "BLE", 40);
ConfigManager.loadAll();
// Supply a verified address and its explicit BLE address type:
// ble.configureAdvertisementPeer(address);
// ble.configureGattPeer(address, serviceUuid, notifyUuid, writeUuid);
ble.begin();
// In loop, after ConfigManager servicing:
ble.loop(millis());
```

The settings placement helper has `(cm, page, group, order)` and
`(cm, page, card, group, order)` overloads, matching MQTT placement conventions.
`addBluetoothRuntimeProviderToGUI(cm, group, providerOrder, baseOrder)` registers
runtime values once; live fields remain explicitly placed by the application.
Runtime values expose enabled/state/connected/scanning, queueDrops/reconnects,
lastError/lateEvents and workerStackHighWaterBytes. State uses the `BleState` enum.
No real device identifiers or payload history are exposed through runtime values.

## Ownership and execution

All APIs and the event handler run in the application loop, except
`cancelPendingCommands()` and `requestPause`/`requestResume`. Pause/resume atomically combine independent `Ota` and
`Configuration` reasons from callbacks/tasks. Resuming one reason preserves the
other. Do not destroy the manager, change the handler from another task, or invoke
NimBLE separately. The worker alone initializes/uses the scanner/client and
performs blocking connection/discovery/subscription/write/cancellation operations.
One client and fixed callback objects are reused. No affinity is assumed.

The task stack budget is 8192 bytes. ESP-IDF's `xTaskCreate` stack depth and
`uxTaskGetStackHighWaterMark` are measured in bytes (unlike upstream FreeRTOS
ports using words). Physical stack/heap stability remains an acceptance gate.

Event queue: 16 owned `BleEvent` slots with at most 272 data bytes each. Command
queue: 8 owned IO slots, with a default maximum of 272 outgoing bytes each.
Peer configuration is stored separately in three small snapshots; it never
reserves command payload storage. Callback producers use
nonblocking queue admission. Capacity/contention/oversize loss invalidates the
query and generation, cancels the link in the worker, and exposes an error.
There is no growing event vector or stored scan-result list.

Consumers with a smaller verified outgoing protocol can set
`-DCM_BLE_TX_MAX_BYTES=9` in their application build flags. The allowed range is
1 through 272; omitting it preserves the existing 272-byte write capacity. Define
the same value for **every translation unit** that includes BLE headers, including
consumer/library builds, because it changes the header-only manager's layout.
`writeCommand()` rejects a payload above that bound before copying or queueing
anything. Incoming advertisements and notifications retain the full 272-byte
capacity, and the 8-command/16-event queue depths remain unchanged.

With the ESP32 Xtensa ABI, peer configuration occupies 48 bytes and an IO command
occupies 288 bytes at the default limit or 24 bytes with a 9-byte limit. Replacing
the previous shared 336-byte configuration/command structure saves 1248 retained
bytes at the default limit, or 3360 bytes with the 9-byte limit, across three
configuration snapshots and eight command slots. These are compiler-verified
layout differences, not a measurement of free heap or worker stack use on hardware.

Advertisements carry manufacturer bytes **including the two company-ID bytes**,
not a full AD element. Empty/non-manufacturer advertisements are ignored.
Notifications carry exact received GATT fragments, copied before returning from
the backend callback. The handler must consume the event synchronously and must
not retain a reference. Check `generation()` when resetting protocol state.
Reconfiguration, pause and loss suppress stale events before publication.
Latest desired configuration is a separate bounded owned snapshot with revision
acknowledgement. Disposable IO/event generations cannot discard queued disabled,
enabled or peer changes; a superseded revision cannot clear a newer update.
A handler may change enabled state/reconfigure; remaining queued events are then
revalidated. The generic transport never decodes, publishes MQTT, or writes NVS
from a radio callback.

Scanning is passive, 1 second per 5 seconds, with `setMaxResults(0)`. Connection
setup stops scanning; bounded scanning resumes after setup. Reconnect delays are
1, 2, 4, 8, 16 and 30 seconds, capped at 30 seconds. Connect timeout is 2000 ms
with backend internal retries disabled. Discovery remains on the worker; target
latency and cancellation require hardware validation. Only one query is active;
protocol validation and device-specific write allowlists belong to the consumer.
`writeCommand()` returning true means queued, not acknowledged by the peer.
`cancelPendingCommands()` is thread-safe and invalidates buffered commands under
the same mutex as the final worker write admission. The worker rechecks each
write's generation inside that mutex. A backend write already holding the fence
completes before cancellation returns; it cannot be recalled. Cancellation does
not change peer configuration or pause reasons. Its generation change follows
the existing reconnect/requalification lifecycle; fresh reads can continue after
normal link recovery. Consumer gates must also prevent new command admission.
Call `bool completeQuery()` synchronously inside the notification event handler,
only after validating the complete response. `BleEvent::queryId` records the
outstanding query at reception, before callback-buffer reuse. Completion enqueues
that delivered event's token/generation; it never samples whichever query is
current later. Outside notification delivery it returns false. Old queued response
events are rejected once another query is active, and duplicate acknowledgements
cannot release a later query. This does not distinguish a device retransmission
that first arrives over the air during a different query; protocol correlation
and validation remain the consumer's responsibility. Queue admission failure
returns false and follows the same loss/invalidation recovery. The absolute
2-second reply deadline is never extended by partial fragments. Missing completion
by that deadline invalidates the generation and reconnects. Protocol-specific
fragment inactivity checks also belong to the consumer and may be stricter.

## OTA integration

Register one lifecycle observer before uploads; see [OTA](OTA.md). Starting
requests `BlePauseReason::Ota`; failure/abort/timeout resumes only that reason.
Success stays paused through reboot. Callback completion records pause intent,
not proof of radio quiescence. Late callbacks are counted/discarded. The stack
stays initialized during ordinary pause/resume; this does not release Bluetooth
memory. The terminal restart path below is separate from ordinary pause/disable.
Never wait for radio cleanup inside an asynchronous web callback.

## WiFi coexistence

CM's worker acquires a WiFi power-policy lease before its first BLE scan or GATT
operation. The application loop must confirm native `WIFI_PS_MIN_MODEM` before
radio work proceeds; an old driver read is insufficient. Keep servicing
`ConfigManager.handleClient()` while startup is pending.

The lease preserves the saved WiFi preference and overrides a requested NONE
only while coexistence requires MIN. Ordinary pause, disabling the setting,
disconnecting or removing peers does not deinitialize the controller. The lease
therefore remains required until reboot, including after terminal restart
shutdown. See [WiFi power saving](WIFI.md#power-saving-and-ble-coexistence) for
diagnostics, retries and the exact preference/runtime distinction.

## Terminal shutdown for HTTP OTA restart

For HTTP OTA, the application can install a nonblocking
[reboot-preparation callback](OTA.md#http-ota-restart-preparation) before web
initialization. From that application-loop callback, request
`requestShutdownForRestart()` and poll `isShutdownForRestartComplete()`.
`isShutdownForRestartFailed()` identifies a terminal failure. This path is for
an imminent restart; it cannot resume normal acquisition afterward.

Only the existing BLE worker stops the host/controller with `deinit(false)`,
confirms that the host is uninitialized and the native controller is IDLE,
then clears objects with `deinit(true)` and reconfirms both conditions. Failure
retains a terminal fence: no acquisition, configuration or cleanup retry is
admitted. Without a worker, the request can acknowledge only an already
uninitialized, native-IDLE radio with no other CM owner, using read-only checks.
The WiFi coexistence lease remains conservative until reboot.

Consumer test profiles can set `CM_TEST_RESTART_PHASES=1` for UART phase markers;
the default is `0`. Native SDK calls can still block. Host tests do not establish
flash-time timing, RF stability or the cause of an unrelated controller panic.

## Build and validation

The opt-in example retains central and observer roles and disables unused
peripheral/broadcaster roles with `CONFIG_BT_NIMBLE_ROLE_PERIPHERAL_DISABLED` and
`CONFIG_BT_NIMBLE_ROLE_BROADCASTER_DISABLED`. These are consumer flags, not core
library defaults. The example uses `-flto` and the 4 MB `min_spiffs.csv` layout,
which provides two larger OTA slots and a smaller SPIFFS partition than the board
default. Changing the partition layout on an existing device can affect stored
filesystem data and requires a suitable installation or migration procedure.

Run `tools/test/run-ble-host-tests.ps1 -Suite all` for hardware-free tests of the
same production lifecycle, scheduler and queues. Compile `pio run -d
examples/bluetooth -e usb`. These tests do not validate RF reception, actual flash
callbacks, phone contention, long-term memory stability, or cancellation timing.
No upload or device communication is implied by the build commands.

The pinned NimBLE 2.5.1 sources retain an upstream return-type declaration mismatch
for `is_ble_hs_resolv_enabled` (`int` versus `uint8_t`), reported by LTO. For the
verified example ELF the incompatible `ble_gap_host_check_status` call path is
discarded and the installed sources have no callers of that checker. The warning
is preserved; this exact-image assessment does not guarantee warning-free builds
or the same result for future consumers, dependencies or flags.
