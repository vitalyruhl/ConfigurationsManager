# Troubleshooting

## Build issues

The library itself builds without PlatformIO `extra_scripts`. Some examples use `extra_scripts` for helper tasks (e.g. WebUI embed or local library refresh). If you are developing the WebUI, see `webui/README.md`.

### Windows: UnicodeEncodeError while running PlatformIO

If you see something like `UnicodeEncodeError: 'charmap' codec can't encode character ...`, your terminal encoding is not UTF-8.

Workarounds (PowerShell):

```powershell
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
```

### Web UI not building or old version showing

Clean the build and rebuild:

```sh
pio run -e usb -t clean
pio run -e usb
```

### Example builds use old local library code

If you are using `lib_deps = symlink://../..` and a build still behaves like it is using older library metadata, clean the example build and rebuild:

```sh
pio run -t clean
pio run
```

## Flash / memory issues

### ESP32 won't boot or guru meditation errors

Erase flash and re-upload.

```sh
pio run -e usb -t erase
pio run -e usb -t upload
```

WARNING: `erase` deletes all flash data on the device.

### WT32-ETH01: no serial data during upload

`Failed to connect to ESP32: No serial data received` means `esptool` opened
the COM port but received no ROM bootloader response.

- Use UART0: adapter TX to RX0/GPIO3 and adapter RX to TX0/GPIO1.
- Do not use the unnumbered UART2 pins on GPIO5/GPIO17.
- Hold GPIO0 low while resetting or powering up.
- Use 3.3 V UART logic and a shared ground.

### WT32-ETH01: repeated brownout resets

`Brownout detector was triggered` indicates that the module supply is dropping
below the safe operating voltage. Use one stable 5 V or 3.3 V supply rated for
at least 500 mA, keep wiring short, and share ground with the serial adapter.
Do not power both module supply inputs and do not disable brownout detection to
hide an unstable supply.

See [Ethernet](ETHERNET.md) for the complete WT32 setup.

## check the meta data


```bash
curl.exe -sS http://<deine-ip>/runtime_meta.json
curl.exe -sS http://192.168.2.126/runtime_meta.json
```

