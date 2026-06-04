# Known Pitfalls

- Status: verified facts and canonical risk rules.
- Settings structure, storage keys, and NVS layout are compatibility-sensitive under
  canonical governance.
- The WebUI and API use plain HTTP. Passwords are masked in the UI but transmitted over
  HTTP in cleartext and stored in plaintext in ESP32 NVS/Preferences.
- `src/html_content.h` is generated from `webui/dist/` and checked in. `webui/dist/` and
  PlatformIO output directories are generated and ignored.
- Examples may have independent firmware/application versions and machine-specific build or
  upload settings. Inspect the affected example before changing it.
- PlatformIO local dependency caches can make example builds appear stale; clean/rebuild
  before diagnosing source behavior.
- On classic ESP32, ADC2 pins are typically unavailable to `analogRead()` while WiFi is
  active.
- MQTT publishing through PubSubClient supports QoS 0 only.
- SolarInverterLimiter is documented as close to the flash limit.
- Verified documentation/tooling drift:
  - README project version is `4.2.0`, while canonical `library.json` is `4.2.1`.
  - `docs/CONTRIBUTING.md` references missing `docs/NAMING.md`.
  - Disabled CI and `test/README` reference test environments absent from PlatformIO config.
  - `docs/SETTINGS.md` describes truncated keys, while current source stores hashed keys.
  - `tools/publish.py` expects README changelog entries, but README points to
    `docs/CHANGELOG.md`.
- Code/log output must use English, ASCII, and short severity tags `[E]`, `[W]`, `[I]`,
  `[D]`, or `[T]`.

Sources: `.github/AGENTS.md`, `.gitignore`, `README.md`, `library.json`,
`src/ConfigManager.h`, `tools/webui_to_header.js`, `tools/publish.py`,
`docs/SECURITY.md`, `docs/WIFI.md`, `docs/MQTT.md`, `docs/CHANGELOG.md`,
`docs/SETTINGS.md`, `docs/CONTRIBUTING.md`, `.github/workflows/pio-tests.yml.deactivate`,
`test/README`.
