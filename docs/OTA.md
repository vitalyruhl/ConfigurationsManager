# OTA

This document describes OTA update support and the Web UI firmware flashing workflow.

OTA requires an IP connection but is not tied to WiFi. Call
`ConfigManager.setupOTA()` after WiFi or Ethernet has received an IP address.

## Web UI flashing

The embedded single-page app exposes a `Flash` action beside the `Settings` tab.

Workflow:

1. Enable `Allow OTA Updates` under **System** and set an `OTA Password`.
   - Leave empty to allow unauthenticated uploads.
2. Click **Flash** and select the compiled `.bin` (or `.bin.gz`) produced by PlatformIO.
3. Enter the OTA password when prompted.
   - The SPA sends it as an `X-OTA-PASSWORD` header.
4. Watch the upload progress popup.
   - The popup stays visible while the browser is actively uploading the firmware and warns before accidental page navigation.
   - On success, the device reboots automatically.

## Backend behavior

- The backend remains asynchronous (`ESPAsyncWebServer`).
- The `/ota_update` handler streams chunks into the Arduino `Update` API while performing password checks.

## PlatformIO ArduinoOTA uploads

An `espota` environment can upload over WiFi or Ethernet:

```ini
[env:ota]
upload_protocol = espota
upload_port = 192.168.2.127
upload_flags =
    -a
    ota-password
    -t
    30
```

`-a` supplies the ArduinoOTA password and `-t` sets the timeout in seconds.
Keep the password synchronized with the firmware setting. Do not specify both
`-a` and `--auth` for the same upload.

To remove OTA support from a firmware build:

```ini
[env:noota]
board_build.partitions = no_ota.csv
build_flags =
    ${env.build_flags}
    -DCM_ENABLE_OTA=0
```

The build flag removes ConfigurationsManager OTA code. The partition setting is
separate and reclaims the second OTA application slot.

Uploads are rejected when:

- OTA is disabled
- the password is missing/incorrect
- the upload fails integrity checks

## Runtime flags used by the Web UI

The runtime JSON includes OTA status information used by the Web UI:

- `runtime.system.otaActive`: true while an ArduinoOTA or WebUI firmware upload is active
- `runtime.system.otaHasPassword`: true when an OTA password is configured

See also `docs/GUI-Runtime.md`.

## Notes

- ConfigurationsManager serves the UI/API over plain HTTP only.
- If you need transport security, provide it externally (VPN, trusted LAN, or a TLS reverse proxy that terminates HTTPS).
- See `docs/SECURITY.md` for details.

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.setupOTA` | `setupOTA(const String& hostname, const String& password = "")` | Initializes ArduinoOTA endpoint and callbacks. | Call after the active IP interface is ready. |
| `ConfigManager.handleOTA` | `handleOTA()` | Processes OTA runtime events. | Must run regularly in `loop()`. |
| `ConfigManager.enableOTA` | `enableOTA(bool enabled = true)` | Enables/disables OTA handling at runtime. | Runtime control for OTA availability. |


## Lifecycle observer for optional transports

Register `ConfigManager.getOTAManager().setLifecycleObserver(observer)` before
uploads. This additive observer is separate from `setCallbacks(reboot, logger)`
and survives OTA setup/reconfiguration. It does not replace ArduinoOTA callbacks.
With `CM_ENABLE_OTA=0`, registration remains valid and no events are emitted.

`OtaLifecycleEvent` carries `stage`, `transport`, `sessionId` and `errorCode`.
Stages are `Starting`, `Succeeded`, `Failed`, `Aborted`, `TimedOut`; transports
are `Arduino` and `Http`. Accepted sessions have nonzero IDs and exactly one
terminal event. Authentication probes/rejected overlap do not receive IDs and
cannot release another session. Stale IDs and duplicate cleanup are ignored.
Success latches admission closed and the associated pause until reboot.

HTTP authenticates and obtains session ownership before `Update.begin` and any
flash write. Arduino core 2.0.17 invokes its onStart after `Update.begin`, before
the first `Update.write`; authentication/begin failures before onStart therefore
have no accepted lifecycle session. Its existing error/log path is preserved.
Arduino polling is deferred while HTTP owns Update. A concurrent Arduino sender
receives no admission during that interval and follows its ordinary timeout/error
path; its flash transfer cannot overlap or abort HTTP. Shared Update operations are serialized. Ordinary ArduinoOTA servicing yields
its lock entirely while an accepted HTTP upload owns Update; only a real owning
abort/disable/expiry may run maintenance during that session. Owner chunks and
completion are therefore not rejected merely because the application loop polls.
HTTP admission/completion/disconnect never wait behind an Arduino transfer and respond
with HTTP 409 where appropriate. Disconnect/contended chunks defer an abort of
only the owning session to the servicing loop when immediate cleanup cannot lock.
A failed chunk is never silently accepted as a truncated successful upload.

Disable/incomplete disconnect produces `Aborted`; stale uploads produce
`TimedOut`; begin/write/end failures produce `Failed` for an accepted session.
The existing 120-second stale limit uses wrap-safe elapsed subtraction. Continue
servicing `handleClient()` for timeout/deferred-abort recovery.

Observers must only set atomic flags or enqueue bounded intent. They must not
wait for workers, deinitialize radio, save NVS, or perform network work. For BLE:

```cpp
ConfigManager.getOTAManager().setLifecycleObserver([](const OtaLifecycleEvent& e) {
  if (e.stage == OtaStage::Starting || e.stage == OtaStage::Succeeded)
    ble.requestPause(cm::BlePauseReason::Ota);
  else
    ble.requestResume(cm::BlePauseReason::Ota);
});
```

Observer return is not radio-quiescence confirmation. Hardware acceptance must
exercise Arduino and HTTP success/failure/disconnect/timeout, concurrent attempts,
and BLE cancellation before release. Host lifecycle tests exercise the production
session helper; real flash/network callbacks remain hardware-only acceptance.

After an HTTP final chunk succeeds, the accepted success session ID remains
latched even though active flash ownership is zero. Its request completion checks
that retained identity and installs the success response and reboot-on-disconnect
handler without taking the flash-operation mutex: these operations perform no
flash writes. Ordinary servicing after Succeeded cannot turn that already accepted
update into HTTP 409 or prevent its reboot handoff. Stale IDs, contenders and late
cleanup cannot acquire the successful owner's acknowledgement or release its pause.

## Connectivity and loop recovery during OTA

AP/OTA suspend network-triggered recovery; normal operation resumes with a fresh
recovery window. Synchronous ArduinoOTA records suspension before entering its
blocking transfer, including the failed-transfer path. Its start/progress confirms
loop work to the independent supervisor; asynchronous HTTP upload traffic does
not feed that supervisor. Continue servicing `handleClient()` and keep the
application loop responsive during HTTP uploads. See [WiFi recovery](WIFI.md)
and the [bounded acceptance record](acceptance/2026-10-21-connectivity-watchdog.md)
for the 30-second default, timeout API and tested slow/interrupted HTTP OTA cases.
The recorded native ArduinoOTA receive failures remain a separate limitation.
