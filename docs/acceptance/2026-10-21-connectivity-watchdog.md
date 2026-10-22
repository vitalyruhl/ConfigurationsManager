# CM connectivity and loop recovery acceptance - 2026-10-21

## Scope and identity

- Repository: ConfigurationsManager, branch `feature/ble-compact-runtime`.
- Historical test base: `0ef9b5f4aa5ad33aba7239bcccc9c6d2f5936854`; tested changes were uncommitted at that checkpoint.
- Consolidated CM source commit (2026-10-22): `4c2daf2238ca3fbb6ea9a5ecb8a802b09a8f1072`. All 19 files in the final HIL source manifest were compared unchanged against this commit.
- Compatible feature version: 4.8.0 Unreleased. Development-branch consolidation only; this record does not authorize main integration, release or a production upload.
- Hardware: test ESP32 on COM8; isolated CM HIL sketch, not a SIL plant test.
- Final firmware SHA-256: `BACA56E10D4A2A9D6FC078B6F2C3F119738E36C9A0092FF374007490744BBB56`.
- Final image: 1,188,896 bytes; flash usage 1,182,325 / 1,310,720; static RAM 54,500 bytes.
- Private scripts, source manifest and raw evidence: `.local/connectivity-hil/`.

## Implemented contract

`WiFiProbe` is one editable field: `off` (default), `gateway`, or strict unicast
IPv4. Gateway mode uses the active DHCP/static gateway. An enabled target
requires matching ICMP responses; driver connection state alone cannot refresh
its health timestamp. The probe is nonblocking, every 10 seconds, with a one-second
deadline and bounded receive work. AP mode and OTA suspend network recovery;
returning to normal operation grants a new full recovery window.

`WiFiRb` remains the existing stored key. Its default is now one minute;
whole values 0..9999 minutes are accepted. Zero disables network-triggered
restarts; 60/120 minutes support one/two hours. Existing saved values survive.
Apply changes RAM; Save changes NVS. Reset/load synchronize the loop-owned
runtime settings through a bounded mailbox.

The independent loop supervisor arms on the first `handleClient()` heartbeat,
with a 30-second default. `setLoopWatchdogTimeout(ms)` accepts zero to disable,
or 1000..3600000 ms. It does not reconfigure the global ESP task watchdog.
ArduinoOTA start/progress confirm synchronous loop work; HTTP upload callbacks
never feed the loop supervisor. Synchronous OTA suspension is recorded before
ArduinoOTA blocks, preserving the recovery grace after an aborted long transfer.

## Validation evidence

| Check | Result |
| --- | --- |
| Loop supervisor host tests | Pass: first heartbeat, expiry, bounds, disable/rearm, rollover, SDK failures and late callback lifetime. |
| Connectivity host tests | Pass: actual manager methods, socket boundary, matching replies, failed-target expiry, dynamic gateway, AP/OTA grace, rollover and malformed packets. |
| Synchronous OTA regression | RED before the hook; GREEN after 70 seconds without WiFi updates followed by abort, with a complete new recovery window. |
| Settings host tests | Pass: concurrent mailbox, actual reset/load paths, strict actual integer validator, default 1, 0..9999 and saved 120-minute preservation. |
| UI tests | Six pass, including rendering the actual Vue field, gateway/off choices and strict address validation. |
| ESP32 build | Pass for the isolated HIL sketch using the adjacent library checkout. Existing ArduinoJson deprecation warnings remain. |
| Temporary unreachable target | Pass: WiFi stayed connected; target loss triggered software recovery after about 60 seconds and restored saved gateway. |
| Blocked main loop | Pass: planned 45-second block was interrupted after about 30 seconds; marker identified the loop supervisor and the device rejoined. |
| Live timeout range | Pass: 120, 9999, 0 and 1 were accepted; -1, 10000, boolean and fractional input rejected by real HTTP endpoints. |
| Embedded UI delivery | HTTP 200; actual delivered HTML includes the new gateway option and setting. |
| Slow HTTP OTA | Pass: 76.531-second transfer, HTTP 200, completed-update restart and successful boot. No premature network/loop restart or Guru. |
| Interrupted HTTP OTA | Pass: intentional interruption after 65.141 seconds; incomplete update aborted, gateway checks resumed and uptime continued without recovery restart. |

The recovery fault image preceded only the final timeout-default/range and
synchronous-OTA entry changes. The unchanged loop/target recovery paths were
validated with an effective persisted one-minute timeout. The final image above
was additionally validated through its live settings API and successful slow
HTTP OTA. Hour-long wall-clock failure runs were not performed; exact policy
timing and rollover are covered by host tests.

## Corrected findings and limitations

- Reset previously left applied network settings running while UI/NVS showed
  defaults. Reset/load now queue both runtime settings; the actual callback
  regression was reproduced before the fix.
- The previous initialized RTC recovery marker did not survive software restart.
  The marker now uses retained uninitialized RTC storage and is consumed only
  for a software reset. Hardware recovery now reports its initiating path.
- A long synchronous ArduinoOTA transfer could finish with an error before the
  main loop had ever observed the active-OTA flag. Its next WiFi tick then
  evaluated an expired pre-transfer timestamp. The explicit start hook fixes
  this path and has an extracted-production regression test.
- Native ArduinoOTA transfers still reported `Error[3]: Receive Failed` and sender
  exit 1, including the unmodified sender. Their transport reliability is open;
  it is not claimed fixed. The corrected receiver resumed gateway checks without
  an immediate reboot after such an error. HTTP OTA completed successfully.
- The timer supervisor requires a functioning scheduler/ESP timer task. It is
  not proof of recovery from all CPU, interrupt, power or network hardware faults.
- No conclusion about the production SIL Guru/root cause or long-run stability
  follows from these bounded tests. Production SIL/SBT and HA were not changed.

## Consolidation and operator disposition - 2026-10-22

The consolidated source commit above preserves the HIL-tested source bytes.
Focused connectivity, loop-supervisor, WiFi-settings and BLE host checks, six
connectivity Vue tests and the CM USB build passed during consolidation. The
firmware hash and device results above remain the original test checkpoint;
source equivalence does not create a new deployed-image identity.

Separately, the operator accepted normal SIL/SBT web access as substantially
faster and without observed crashes, and requested closure of the current
HTTP/WebSocket iteration. See [the preserved investigation](../todo_http_delivery_panic.md).
That report does not pass the historical overload cohorts or resolve the native
ArduinoOTA receive failures recorded here. The complete release matrix was not
run; CM remains on its development branch.

## Focused commands

- `tools/test/run-loop-watchdog-host-tests.ps1`
- `tools/test/run-connectivity-host-tests.ps1`
- `tools/test/run-wifi-health-config-host-tests.ps1`
- `node --test webui/test/connectivityTarget.test.mjs`
- `pio run -e usb` in the isolated HIL project; explicit upload to COM8.
- `git diff --check`

A full repository test matrix, unrelated consumer builds, release and integration
checks were not run. They are outside this requested change/test scope.

The original test settings were saved and reboot-verified after validation:
`WiFiProbe=off`, `WiFiRb=15`. The standalone CM HIL firmware remains on the test
board. The one-minute default applies to new/reset values, not existing NVS.
