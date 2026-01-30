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

If you are using `lib_deps = file://../..` and a build still behaves like it is using an older library snapshot, clean the example build and rebuild:

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
