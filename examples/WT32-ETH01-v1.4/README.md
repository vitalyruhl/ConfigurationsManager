# WT32-ETH01 V1.4 Ethernet Example

This example is based on `examples/minimal` and targets the Wireless-Tag
WT32-ETH01 V1.4 (WT32-S1). ConfigManager WiFi support is compiled out and the
WebUI is served through the LAN8720 Ethernet interface at `192.168.2.127/24`.

## Network configuration

- Address: `192.168.2.127`
- Netmask: `255.255.255.0`
- Gateway: `192.168.2.1`
- Primary DNS: `192.168.2.1`
- WebUI: `http://192.168.2.127/`

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

The default OTA password is `ota`. Change `OTA_PASSWORD` and the matching
`upload_flags` value before using this outside a trusted network.

## USB-to-TTL wiring

Use 3.3 V UART logic levels. Flashing uses UART0, not the unnumbered UART2 pins:

- Adapter TX to board RX0 (GPIO3)
- Adapter RX to board TX0 (GPIO1)
- Adapter GND to board GND
- Hold GPIO0 low while resetting or powering up to enter the bootloader

[WARNING] A USB-to-TTL adapter's 3.3 V regulator may not provide enough current
for the ESP32 and Ethernet PHY. If boot or upload is unstable, use a suitable
external supply with a shared ground while keeping UART logic at 3.3 V.
