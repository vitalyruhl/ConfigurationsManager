# ESP32 And PlatformIO Notes

- Status: verified.
- C++17 is required. Root and example PlatformIO configurations remove GNU++11 and add
  `-std=gnu++17`.
- The root environment is `usb`, using `platform = espressif32`, `board = nodemcu-32s`,
  and `framework = arduino`.
- Example environment names are `usb`, `ota`, `ota_no_oled`, and
  `ota_no_oled_no_bme`; the last two occur only in SolarInverterLimiter.
- SolarInverterLimiter pins `platformio/espressif32@7.0.1`; the root and other audited
  examples use unpinned `espressif32`.
- Core dependencies are ArduinoJson, ESPAsyncWebServer-esphome, and AsyncTCP-esphome.
  MQTT examples additionally use PubSubClient.
- Most examples keep `build_dir` and `libdeps_dir` outside the repository to avoid recursive
  local-library installs. Several examples consume this repository through `../..` and use
  `tools/pio_force_local_lib_refresh.py`.
- If an example appears to use stale local library code, clean and rebuild the example.
  `CM_PIO_NO_LIB_REFRESH=1` disables the forced local refresh helper.
- Secrets, local WiFi details, serial ports, OTA addresses, and credentials are local data;
  never put them in shared Serena memories.
- Feature flags and their defaults are documented in `docs/FEATURE_FLAGS.md`.

Sources: `platformio.ini`, `examples/**/platformio.ini`, `library.json`,
`docs/GETTING_STARTED.md`, `docs/FEATURE_FLAGS.md`.
