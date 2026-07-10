# Ethernet

ConfigurationsManager can run on an IP interface initialized by the sketch.
The library-managed WiFi connection, access-point, captive-portal, and roaming
code can be removed with `CM_ENABLE_WIFI=0` while the WebUI and OTA remain
available over Ethernet.

The complete hardware example is
[`examples/WT32-ETH01-v1.4`](../examples/WT32-ETH01-v1.4).

## PlatformIO configuration

Use the PlatformIO board definition and disable ConfigManager WiFi support in
the common environment so USB, OTA, and no-OTA builds inherit it:

```ini
[env]
platform = espressif32
board = wt32-eth01
framework = arduino
build_flags =
    -std=gnu++17
    -DCM_ENABLE_WIFI=0

[env:usb]
upload_protocol = esptool
upload_port = COM6

[env:ota]
upload_protocol = espota
upload_port = 192.168.2.127
upload_flags = --auth=ota-password

[env:noota]
board_build.partitions = no_ota.csv
build_flags =
    ${env.build_flags}
    -DCM_ENABLE_OTA=0
```

Keep the authentication value synchronized with the OTA password used by the
firmware. `-a ota-password` is equivalent to `--auth=ota-password`. An optional
`-t 30` pair sets the `espota` timeout to 30 seconds; do not specify both
authentication forms in the same environment.

## Startup sequence

The sketch owns Ethernet initialization and static/DHCP configuration. Start
ConfigurationsManager services only after the interface has an IP address:

```cpp
if (ethernetHasIp && !networkServicesStarted)
{
    ConfigManager.startWebServerOnNetwork();
#if CM_ENABLE_OTA
    ConfigManager.setupOTA(APP_NAME, otaPassword);
#endif
    networkServicesStarted = true;
}
```

Call `ConfigManager.handleClient()` regularly from `loop()`.

`startWebServerOnNetwork()` initializes the WebUI, runtime provider, WebSocket,
HTTP OTA routes, and other ConfigManager network services. It does not start or
configure Ethernet itself.

## WT32 persisted settings

The WT32 example registers these values in NVS and exposes them in the WebUI:

- Ethernet IP address
- subnet mask
- gateway
- primary DNS server
- Settings password
- OTA enable state and OTA password
- NTP servers, interval, and POSIX timezone

Ethernet address changes take effect after a reboot. NTP is initialized after
Ethernet receives its IP address. The NTP interval field is persisted for
settings compatibility; the Ethernet example calls `configTzTime()` once and
then uses the ESP32 SNTP client's internal resynchronization schedule.

## First-run defaults

Copy the tracked template and edit the local file:

```powershell
Copy-Item examples/WT32-ETH01-v1.4/src/secret/secrets.example.h `
          examples/WT32-ETH01-v1.4/src/secret/secrets.h
```

`secrets.h` is ignored by Git. The example imports secret defaults only while
the persisted Ethernet IP is empty. Later WebUI changes are therefore not
overwritten on reboot.

## Build and upload

Run from the repository root:

```powershell
pio run -d examples/WT32-ETH01-v1.4 -e usb
pio run -d examples/WT32-ETH01-v1.4 -e usb -t upload
pio run -d examples/WT32-ETH01-v1.4 -e ota -t upload
pio run -d examples/WT32-ETH01-v1.4 -e noota
```

## WT32-ETH01 UART and power

USB serial flashing uses UART0:

- adapter TX to board RX0/GPIO3
- adapter RX to board TX0/GPIO1
- adapter GND to board GND
- hold GPIO0 low while resetting to enter the ROM bootloader

The unnumbered RXD/TXD pins are UART2 on GPIO5/GPIO17 and cannot be used by
`esptool` for ROM flashing.

[WARNING] Use 3.3 V UART logic. Power the WT32-ETH01 from one suitable 5 V or
3.3 V supply rated for at least 500 mA; never drive both supply inputs. Share
ground between the supply, module, and USB-to-TTL adapter. A repeated
`Brownout detector was triggered` boot loop indicates an inadequate or
unstable supply and should not be hidden by disabling brownout detection.

## Related documentation

- [Feature Flags](FEATURE_FLAGS.md)
- [OTA](OTA.md)
- [Security](SECURITY.md)
- [Troubleshooting](TROUBLESHOOTING.md)

## Method overview

| Method | Description |
| --- | --- |
| `ConfigManager.startWebServerOnNetwork()` | Starts ConfigManager services on an already configured IP interface. |
| `ConfigManager.setupOTA(hostname, password)` | Starts ArduinoOTA after the interface has an IP address. |
| `ConfigManager.handleClient()` | Processes WebSocket, runtime, and OTA work from `loop()`. |
