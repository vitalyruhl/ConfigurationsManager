# WebSocket Connection Ownership Repair

## Scope and authorization

The owner requested investigation and repair of the BoilerSaver/CM panics in the test environment only. Production remains on recovered BoilerSaver 4.0.4. Do not upload to production, modify shared dependency installations, publish, or change SIL/SBT/uartTestEsp.

The observed production panic has no backtrace yet. Treat the separately verified WebSocket lifetime defect as a repairable defect, not as an established explanation of that panic.

## Verified defect and design

CM 4.10.5 obtains raw WebSocket client pointers in its main-loop delivery path while the pinned ESPAsyncWebServer-esphome 3.4.0 disconnect handler can remove and destroy those clients concurrently. CM's subscription/delivery reservation does not reserve the transport client.

Use a private CM worktree and the maintained ESP32Async/ESPAsyncWebServer 3.12.1 with AsyncTCP 3.5.0. Its server methods hold `_ws_clients_lock` across lookup and use; its queues and disconnect writes share `_queue_lock`. Replace main-loop raw-client operations with those guarded server methods. Merely upgrading while retaining borrowed pointers is insufficient.

Preserve one outstanding runtime payload per client by retaining the existing delivery reservation until the shared outgoing buffer is released. Preserve selected groups, custom runtime builders, independent slow/healthy clients, copied payload bytes, and nonblocking generation behavior.

## Implementation sequence

1. Add a deterministic host regression for a connection disappearing before delivery; establish failure against the current raw-client contract without accessing freed memory.
2. Adapt runtime delivery to guarded server-by-ID APIs and shared outgoing-buffer ownership. Remove remaining main-loop raw-client lookups from heartbeat/close/delivery paths.
3. Pin the maintained transport pair in package and consumer build metadata. Classify as a compatible patch; synchronize the canonical version and required mirrors to the unpublished 4.10.6 candidate.
4. Update host transport doubles and retain the existing backpressure/selection/generation checks.
5. Run focused host checks, relevant firmware builds, and an independent code review. Freeze exact artifacts before integrating the private candidate into a separate BoilerSaver test profile.
6. Test only on the documented COM8 development station, using OneWire18, I2C21/22, LED25 as the unloaded boiler output, buttons14/33/4, and no OLED reset pin. Do not register relay pins19/27/23.

## Acceptance and limits

- No main-loop client-pointer dereference spans an uncoordinated disconnect.
- Disconnected/full/rejected delivery releases its reservation; accepted delivery retains it until transport buffer release.
- A stalled transport queue retains at most one CM runtime payload per client.
  Transport release may precede a network ACK after TCP copies the bytes; TCP's
  own bounded window is separate from the CM buffer reservation.
- Selected payloads, legacy clients, and generation locks retain their existing behavior.
- Record real peripheral presence, exact BIN/ELF hashes, reset reasons, and any backtrace separately from synthetic host fixtures.
- A passing test does not authorize a production deployment or prove the historic panic's cause.

## Verification record

- The original helper failed the deterministic connection-closure regression with
  `Connection closed before handoff: sent=1 closed_queue_calls=1 busy=0`.
  The test simulates closure without accessing freed memory. The repaired helper
  passes this regression.
- `tools/test/run-runtime-host-tests.ps1` passes with the installed ArduinoJson
  headers from the isolated BoilerSaver consumer. Coverage includes selection,
  heartbeat, serialization, queue-stalled backpressure, rejection and cleanup,
  connection closure, callback replies while periodic pushes are paused, and
  transport-buffer release before a network ACK. The connection and callback
  regressions are part of that standard runner.
- An actual ESP32 minimal consumer builds with the pinned maintained transport:
  55,584 bytes static RAM and 1,217,581 bytes flash in a 1,310,720-byte slot.
  Log: `.local/ws-ownership/minimal-build.log`.
- The Settings-only feature check (`--modes 01`, Live disabled and Settings
  enabled) passes firmware compilation, route/field elimination, generated
  feature flags, and the frontend import checks. It uses 37,844 bytes static RAM
  and 699,441 bytes flash. Evidence:
  `.local/ui-settings-check/matrix.json` and its `01/` directory. These are build
  results, not device heap measurements. The complete four-mode matrix and full
  repository gate were not run for this test-only repair.
- Independent source review found no remaining blocker for the isolated COM8
  test. It did not establish the historical panic's cause or production fitness.

## Frozen consumer handoff

The unpublished CM 4.10.6 package is
`.local/ws-ownership/cm-4.10.6-test-b3cfc50834d8.tar.gz`, SHA256
`b3cfc50834d81853d9ebe09afb7aca3d5f70db73e24726e9b58400cc4604a592`.
It pins ESP32Async/ESPAsyncWebServer 3.12.1 and AsyncTCP 3.5.0. The archive was
checked for the expected source, metadata, and absence of local/private files.

The isolated BoilerSaver `bench_cm_fix` consumer uses a separate MQTT identity,
the documented station wiring, and disabled heating with an unloaded LED
output. Its frozen BIN has SHA256
`b74dfe76ceaec63af71739870ffa2cded5589e013d8ecac20aafbb826e4d331d`,
size 1,262,560 bytes, and 48,160 bytes application-slot reserve. ELF SHA256:
`2cb9eaef6f2f085e0d702b1cc56f7202da99deaedf04b18b6aade6916f9d5617`.
Consumer manifest and copied artifacts are under that worktree's
`.pio/evidence/cm4106-build-manifest.json` and
`.pio/evidence/cm4106-build-artifacts/`.

The exact frozen BIN was uploaded only to COM8, without recompilation. The
existing partition table was checked and NVS was preserved. The package, BIN,
and ELF hashes were independently rechecked after the run and remain unchanged.

## COM8 device result

The observation completed after 660.016 seconds. Its runtime/peripheral gate
passes: one controlled boot, no panic or unexpected reset, 131 diagnostic
records, 847 MQTT messages, 65/65 completed sensor callbacks, and one executed
display-off callback. The actual GPIO18 sensor supplied valid temperatures;
LED25 readback remained off. I2C acknowledged and display initialization
succeeded; this is not a visual verification of display pixels. Two controlled
WebSocket document close/open cycles restored the selected live data. The final
HTTP sample reports uptime 658,671 ms and temperature 24.9375 C.

The strict HTTP gate does **not** pass: one `TimeoutError: timed out` occurred
at 117.578 seconds. It is separate from the panic/reset checks; no accompanying
WiFi-disconnect event was reported. Do not describe the complete test as
unconditionally green or infer long-term stability from this bounded run.

Raw consumer evidence is in `.pio/evidence/cm4106-station-result.json`,
`cm4106-station-serial.log`, `cm4106-station-mqtt.json`, and
`cm4106-station-ui.json`. The recorder ended and its browser tab was closed.
Production remains on recovered BoilerSaver 4.0.4. No production upload, commit,
push, merge, or release is authorized by this result. The original production
panic still needs a backtrace for causal attribution; the independently
verified ownership defect and this successful bounded runtime test do not
provide that attribution.

## End-of-day Git checkpoint

After the bounded device result and its HTTP limitation were reported, the owner
explicitly requested committing and pushing today's project work. Preserve this
candidate on `fix/runtime-websocket-ownership`; this is a checkpoint, not main
integration or a release. The known HTTP timeout and unproven historical panic
attribution remain open. The owner did not authorize another production upload.
Keep private build/evidence archives and local tool metadata outside Git.
