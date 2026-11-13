# Troubleshooting

## Build issues

The default library UI builds without PlatformIO `extra_scripts`. Some examples
use helper hooks for Web UI regeneration or local library refresh. Alternate
Live/Settings combinations require the hook described in
[Feature flags](FEATURE_FLAGS.md#matching-the-embedded-frontend). For frontend
development, see [the Web UI guide](../webui/README.md).

### Conflicting asynchronous web transports

Check [the required transport pair](GETTING_STARTED.md#build-and-dependency-compatibility)
and remove explicit legacy `esphome` transport dependencies from the consumer.
Do not patch installed dependency-cache headers to combine transport APIs.
Keep the dependency selection consistent with the source being built.

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

Capture the complete serial panic/reset log and keep the exact matching firmware
ELF and build identity before changing the device. Decode the backtrace against
that ELF; a backtrace from a different image can identify the wrong code. Record
the board, dependency versions, flags and actions preceding the failure.

For allocation failures, record both free heap and the largest suitable block,
not only total free bytes. A short successful run or a smaller host fixture does
not establish that the cause of an unclassified panic is repaired. See
[memory ownership](../dev-info/memory-ownership.md) and
[runtime delivery boundaries](GUI-Runtime.md#delivery-and-backpressure).

If a deliberate clean-flash reproduction is needed, back up required settings
first. Erasing is destructive and is not a general diagnosis for runtime panics:

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

## Inspect runtime metadata


```bash
curl.exe -sS http://<device-ip>/runtime_meta.json
```

