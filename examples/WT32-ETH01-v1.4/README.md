# WT32-ETH01 V1.4 Ethernet Example

This example is based on `examples/minimal` and targets the Wireless-Tag
WT32-ETH01 V1.4 (WT32-S1). ConfigManager WiFi support is compiled out and the
WebUI is served through the LAN8720 Ethernet interface at `192.168.2.127/24`.

## Network configuration

- Address: `192.168.2.127`
- Netmask: `255.255.255.0`
- Gateway: configurable; tracked default `192.168.2.1`
- Primary DNS: configurable; tracked default `192.168.2.1`
- WebUI: `http://192.168.2.127/`

Ethernet IP, netmask, gateway, DNS, Settings password, OTA password, and NTP
configuration are persisted and available in the WebUI. Ethernet address
changes take effect after a reboot. NTP servers and timezone are applied at
network startup; the interval field is stored while ESP32 SNTP handles its own
periodic resynchronization.

Copy `src/secret/secrets.example.h` to `src/secret/secrets.h` for local
first-run defaults. The local file is ignored by Git. Secret defaults are
imported only while the persisted Ethernet IP is empty, so later WebUI changes
are not overwritten on reboot.

OTA is IP-based and remains enabled for the `usb` and `ota` environments. The
`noota` environment removes ConfigManager OTA support and uses `no_ota.csv`.

## Build and upload

Run from the repository root:

```bash
pio run -d examples/WT32-ETH01-v1.4 -e usb
pio run -d examples/WT32-ETH01-v1.4 -e usb -t upload
pio run -d examples/WT32-ETH01-v1.4 -e ota -t upload
pio run -d examples/WT32-ETH01-v1.4 -e noota
```

The example uses `--auth=...`; `-a` is the equivalent short form and `-t` sets
an optional timeout. Keep the firmware `OTA_PASSWORD` and PlatformIO
authentication value synchronized. Do not specify both authentication forms.

## USB-to-TTL wiring

Use 3.3 V UART logic levels. Flashing uses UART0, not the unnumbered UART2 pins:

- Adapter TX to board RX0 (GPIO3)
- Adapter RX to board TX0 (GPIO1)
- Adapter GND to board GND
- Hold GPIO0 low while resetting or powering up to enter the bootloader

[WARNING] A USB-to-TTL adapter's 3.3 V regulator may not provide enough current
for the ESP32 and Ethernet PHY. Use one suitable 5 V or 3.3 V supply rated for
at least 500 mA, never drive both supply inputs, and share ground while keeping
UART logic at 3.3 V. Repeated brownout resets indicate an unstable supply.
