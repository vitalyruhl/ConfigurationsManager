# Full IO Demo (Full-IO-Demo)

This example demonstrates IO-related features (via `IOManager`) plus a rich runtime GUI.

The IO wiring is split into definition, settings placement, and runtime layout so you can see how the refactor-friendly builder APIs apply in a real sketch. Each IO category now gets its own `I/O` card and live page, and the runtime controls sit on dedicated Live tabs (`controls`, `inputs`, `sensors`, `analog-outputs`, your alarm card, etc.).

If you want a GUI-only showcase without IO-related parts, see `examples/Full-GUI-Demo`.

## How to run

From the repo root:

```bash
pio run -d examples/Full-IO-Demo -e usb
pio run -d examples/Full-IO-Demo -e usb -t upload
```

## First start / AP mode

If no SSID is configured yet, the device starts in AP mode.
Open the printed AP URL from Serial (usually `http://192.168.4.1`) and configure WiFi via the Web UI.

## Optional secrets defaults

You can copy `src/secret/secrets.example.h` to `src/secret/secrets.h` and set default WiFi credentials for faster startup.

## Screenshots

![Runtime GUI](cm-full-io-demo-gui.jpg)
![Settings 1](cm-full-io-demo-settings1.jpg)
![Settings 2](cm-full-io-demo-settings2.jpg)
![Settings 3](cm-full-io-demo-settings3.jpg)
