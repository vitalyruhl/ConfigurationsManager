# Build And Test Commands

- Status: commands below are verified from repository configuration, documentation, or
  canonical governance.
- Required default validation after C++ source/header changes:
  `pio run -e usb`
- Build the minimal example:
  `pio run -d examples/minimal -e usb`
- Build the documented BME280 example:
  `pio run -d examples/BME280-Temp-Sensor -e usb`
- Clean the selected PlatformIO project:
  `pio run -t clean`
- Canonical governance requires relevant tests when tests are affected, but the repository
  currently documents no verified concrete test environment. Do not treat the stale
  `test` or `nodemcu-32s` test-environment references as runnable facts.
- WebUI development commands run from `webui/`: `npm install`, `npm run dev`, and
  `npm run build`.
- Regenerate embedded WebUI output from the repository root only when WebUI output is in
  scope: `node tools/webui_to_header.js`.
- Upload, monitor, erase, and OTA commands are not validation defaults and require explicit
  user approval.
- No enabled GitHub Actions workflow exists; `.github/workflows/pio-tests.yml.deactivate`
  is deactivated.

Sources: `.github/AGENTS.md`, `platformio.ini`, `README.md`, `docs/GETTING_STARTED.md`,
`webui/README.md`, `webui/package.json`, `.github/workflows/pio-tests.yml.deactivate`,
`test/README`.
