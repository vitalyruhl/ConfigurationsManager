# Project Overview

- Status: verified unless marked canonical.
- ConfigurationsManager is a C++17 Arduino/ESP32 library and example firmware built with
  PlatformIO.
- Verified features include typed settings, ESP32 Preferences/NVS persistence, JSON
  configuration, an embedded web UI, runtime values, WebSocket updates, WiFi management,
  OTA, and optional MQTT, logging, alarm, and IO helpers.
- `library.json` is the canonical package/library metadata and version source.
- Main areas:
  - `src/`: library source and checked-in embedded WebUI output.
  - `examples/`: standalone PlatformIO example applications.
  - `webui/`: Vue 3 WebUI sources and npm tooling.
  - `docs/`: user and contributor documentation.
  - `test/`: PlatformIO tests.
  - `tools/`: build, validation, WebUI embedding, and release helpers.
- `README.md` is the public entry point. `.github/AGENTS.md` is canonical agent governance.
- Serena project indexing is configured for `cpp`.

Sources: `README.md`, `library.json`, `platformio.ini`, `src/ConfigManager.h`,
`webui/package.json`, `.serena/project.yml`.
