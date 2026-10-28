# Bounded CM 4.10.5 HTTP OTA and BLE shutdown acceptance

## Source identity and scope

The operator's SBT 1.4.12 acceptance used immutable CM 4.10.5. The 193-file
consumed source manifest SHA256 is
`70eceafa310481762f4f89ae06daf1d21f9aa72d46c0575b4a09ea7ed0c9b568`.
The SBT production BIN SHA256 is
`037eccd7ae13cc1e8d7b69b4bb6e768ecbe2b46e0af361acac1e6796caf1cf5a`;
the ELF SHA256 is
`ed82ee8b8b7f6428826a92278597f1b2b7003da7ec4c99af034f0efb2e4e0dee`.
The [original SBT acceptance report](https://github.com/vitalyruhl/SolarBatteryTelemetry/blob/957eb01949f9fdf4278b3c7fe541db774e8d147f/docs/acceptance/2026-10-27-http-ota-ble-shutdown.md)
records the event chronology and diagnostic limits. All times below are UTC.

A read-only comparison before release preparation checked all 193 manifest
entries against CM checkpoint `49d0b3dda5be38620731fa9581a93962d9fb736f`.
All files matched except generated `webui/dist/index.html.gz`; its decompressed
HTML was identical. In particular, every consumed `src/` file, including the
actual embedded `src/html_content.h`, matched exactly. Release documentation
and the later Vue 3.5.44 update are outside that original device-tested snapshot.
The updated UI is regenerated and validated by the release software checks;
no repeat hardware observation of that dependency update is claimed.

## Observed correction

The earlier initialized-BLE upload reproduced watchdog failures first from
`async_tcp`, then from `IDLE0` after restart moved into the application loop.
The final correction adds nonblocking loop-owned preparation and terminal
worker-owned BLE shutdown: stop without deletion, verify stopped host/native
IDLE, clear objects and acknowledge before restart. Failure or a ten-second
preparation deadline blocks automatic restart. Watchdogs remain enabled.

The isolated same-image OTA at 2026-10-26 23:50:18.278287 confirmed stop,
native-IDLE, cleanup and acknowledgment phase markers before the main-loop
restart. Stop and confirmation took about 16 ms. Reset reason was 3, and the
fixture ran for more than two minutes without a watchdog or new panic.

The first production installation at 23:54:53.736541 still rebooted through
the outgoing 1.4.9 firmware's old path; its reason-6 event does not qualify
the newly installed code. A separate production same-image OTA at
2026-10-27 00:00:19.909563 exercised the new path with actual BLE connections:
reset reason 3, resumed BMS/Victron samples and all 33 settings unchanged.
The following 00:00:32 through 00:15:32 observation completed without another
reboot or offline transition and retained available BMS/Victron health.

## Limits and open tracking

These bounded observations establish the tested HTTP/BLE restart path only.
They do not qualify long-run RF or runtime stability, all ArduinoOTA scenarios,
a thermal cause, or general TCP/heap recovery. A historical stored panic dump
has no crash timestamp and is not evidence of a fresh panic after restart.
MQTT reconnect/session replacement remains open in
[#89](https://github.com/vitalyruhl/ConfigurationsManager/issues/89), In Progress;
this release does not mark that investigation fixed.
