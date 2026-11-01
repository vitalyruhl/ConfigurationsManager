# SolarBatteryTelemetry: WiFi + OTA + BLE example

This standalone PlatformIO application demonstrates ConfigurationsManager's WiFi
settings/setup AP, embedded Settings UI and OTA lifecycle together with the
CM-owned BLE worker. It polls a JBD 14-series-cell BMS over GATT and can receive
Victron SmartSolar Instant Readout advertisements on the same ESP32. Optional
MQTT publishes measured values and availability. No second ESP32, display,
relay, external sensor or inverter connection is required.

## Source and dependency baseline

The application source is derived from SolarBatteryTelemetry commit
`d04d147913d23856f27bcd9ce496a5be1c5c6087` (application version `1.5.1`,
2026-10-31). The source, version and existing guarded BMS control implementation
are retained. Only public application files, the dual-OTA partition table,
LTO hook and selected synthetic native tests are included.

The example binds exactly one CM library, the enclosing checkout, through
`symlink://../..`. Run it at this location inside ConfigurationsManager; copying
only this folder somewhere else will break that binding. It has no private
CM freeze, absolute source path, owner credentials or binary controller archive.
The enclosing CM WebUI build hook requires Node.js/npm and uses its existing
lockfile. It generates shared `src/html_content.h` and `webui/dist` in the CM
checkout for this example's Settings-only feature profile. Build examples with
different UI flags sequentially.

| Component | Selection |
| --- | --- |
| Board | Classic ESP32, `nodemcu-32s`, 4 MiB flash |
| Platform/framework | `platformio/espressif32@7.0.1`, Arduino-ESP32 2.0.17 |
| CM | Enclosing ConfigurationsManager checkout |
| Async web transport | ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0 |
| JSON | ArduinoJson `~7.4.3` |
| BLE | NimBLE-Arduino 2.5.1, central/observer roles |
| Application MQTT | PubSubClient 2.8 |

The original 1.5.1 source branch also selects a separately downloaded Espressif
controller candidate and an unpublished frozen CM package. **This example uses
the platform's stock Bluetooth controller**, without that experimental override.
Its `1.5.1` application label therefore does not identify a byte-for-byte copy of
the original firmware or imply the controller assertion is fixed. A prior
`llm_util.c:215` controller assertion was investigated in that application; the
trigger and long-term RF/WiFi/MQTT stability remain unproven. Prior device tests
on the original application do not validate this example, its enclosing CM
checkout or its stock-controller combination.

## Build without private inputs

Install PlatformIO and Node.js/npm. From this directory:

```sh
pio run -e usb
pio run -e ota
python tools/test/run-native-tests.py
```

`usb` and `ota` share all compiler flags, dependencies and partitions. Their
upload protocols differ; these commands compile only and do not upload or
connect to devices. The native checks require a C++17 compiler (`clang++` or
`g++`); use `--compiler /path/to/compiler` if it is not on PATH. They cover JBD
framing/decoding/freshness, guarded MOS request policy and WiFi provisioning with
synthetic data, without radio or MQTT access. The host-only fixtures live in
`tests/native/`; they are not embedded PlatformIO test targets.
Build/native outputs stay under
`.pio/`. Registry dependencies use an example-specific directory under the
normal PlatformIO core cache, with separate USB/OTA subdirectories. Only a
`.pio-link` points back to the enclosing CM source. Keeping dependency packages
outside that library root avoids PlatformIO misclassifying their headers as CM
files; no second CM source checkout or private cache is needed.

Use a classic ESP32 board with at least 4 MiB flash. The included
`partitions/dual-ota-1792k.csv` provides two 1792 KiB application slots, NVS,
OTA selection, a filesystem region and a 64 KiB coredump region. The first
installation must install this partition table along with the application.
An application-only OTA cannot change an incompatible partition layout.

## First setup

A clean build needs no `secrets.h`. The template leaves station credentials,
MQTT broker, BMS MAC and Victron MAC/key empty. On a board without existing
settings, CM starts its setup AP; configure WiFi through its Settings UI. DHCP
is the initial network mode. MQTT starts disabled and BLE has no configured
source. Existing persisted settings remain authoritative, including when a
board previously ran another CM application; these defaults do not reset NVS.

For optional initial local provisioning, copy `src/secret/secrets.example.h` to
`src/secret/secrets.h` and fill in your own values. That file is ignored. The
application uses local WiFi credentials only when valid persisted WiFi settings
are absent. MQTT defaults to enabled when `secrets.h` exists, so configure its
broker and credentials before enabling it. In Settings, set a unique MQTT base
topic/client identity for each collector. The default base is
`SolarBatteryTelemetry`; the client ID is derived from the ESP32 identity.

Set your own Settings password and OTA password before commissioning. The
example intentionally includes no shared password or upload destination.
`SETTINGS_PASSWORD` supplies Settings access authentication; `OTA_PASSWORD` only
seeds an absent stored OTA password. Edit the System settings to change a stored
OTA password. For a later owner-operated upload, supply your actual serial port
or OTA host with PlatformIO's `--upload-port` and matching OTA authentication
locally. The embedded CM OTA page uses the same application lifecycle.

## BLE configuration and ownership

- **Bluetooth:** enable the CM Bluetooth setting when ready to operate the radio.
  The inherited first-configured-source migration enables Bluetooth once;
  subsequent saved user choices are retained. This example does not enforce the
  original installation's temporary RAM-only recovery activation procedure.
- **JBD BMS:** enter your own MAC in Bluetooth > BMS > Readout. The implemented
  protocol expects a JBD 14-cell pack, GATT service `0xff00`, notification
  characteristic `0xff01` and write characteristic `0xff02`. Empty MAC means
  unconfigured; it does not select the nearest battery. Basic/cell reads use
  registers `0x03` and `0x04`; unknown, incomplete and expired readings remain
  unavailable. The configurable sample timeout defaults to 60 seconds.
- **Victron (optional):** enter the controller's MAC and its 32-hex-digit Instant
  Readout key in Bluetooth > Victron. Enable compatible Instant Readout
  advertisements in VictronConnect. The encryption key is different from the
  pairing PIN. The adapter passively decodes supported Solar Charger records;
  do not infer compatibility with every Victron product. Leave both values
  empty to use the JBD source alone.

Do not call `NimBLEDevice::init()` or create another radio owner. The application
attaches both adapters to one `cm::BluetoothManager`, routes its event callback
to them, and services adapter/CM work from `loop()`. The OTA lifecycle observer
pauses BLE and MQTT while an update runs, resumes after cancellation/failure,
and leaves successful updates paused. The reboot-preparation callback asks the
BLE worker to shut down and waits for acknowledgment before CM restarts.

The inherited application also contains **guarded JBD charge/discharge MOS
commands** through UI/runtime and MQTT. `BMS control enabled` defaults to false;
leave it disabled for telemetry-only use. This is not a strictly read-only
firmware once that gate is enabled. The gate can also be changed through the
existing MQTT command interface, so use a dedicated broker/topic and appropriate
access control if enabling MQTT. Victron remains passive; no charger control,
new automation policy or battery protection strategy is added by this example.

The Settings-only UI is intentional (`CM_ENABLE_LIVE_UI=0`,
`CM_ENABLE_SETTINGS_UI=1`); runtime registration remains available in the
application, but no live dashboard is embedded. BMS telemetry includes 14
series-group voltages, supported temperatures/status and availability. MQTT
uses the existing source-health, delta and periodic-refresh logic. Its legacy
synchronous transport is retained; the example does not claim bounded network
latency or long-term stability. Hardware readout, RF coexistence, partition
installation, OTA reboot and physical MOS behavior require separate validation
on your own installation.
