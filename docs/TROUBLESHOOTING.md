# Troubleshooting

## Build issues

v3.0.0+ builds without PlatformIO `extra_scripts`. If you are developing the WebUI, see `webui/README.md`.

### Web UI not building or old version showing

Clean the build and rebuild:

```sh
pio run -e usb -t clean
pio run -e usb
```

## Flash / memory issues

### ESP32 won't boot or guru meditation errors

Erase flash and re-upload.

```sh
pio run -e usb -t erase
pio run -e usb -t upload
```

WARNING: `erase` deletes all flash data on the device.
