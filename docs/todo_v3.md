# v3 TODO / Roadmap

As of: 2026-01-09

## [COMPLETED] v3.0.0 Stabilization
- Apply all / Save all: End-to-end tested (UI -> firmware -> persistence), including browser reload.
- Config endpoints: robust JSON parsing for `/config/auth`, `/config/save`, `/config/apply_all`, `/config/save_all`.
- Password security: `/config.json` masks passwords; reveal only after successful auth via token header.
- OTA: stabilized by immediate reboot at end of OTA (workaround for lwIP assert).
- Examples: secrets removed; `example_min` is a minimal GUI skeleton (AP mode when SSID is empty).

## [COMPLETED] v3.1.0 Stabilization (Bugfixes)
- WebSocket: investigate disconnects while loading settings (payload size, timing, reconnect loop, memory).
- WiFi reboot: document behavior (timeout vs phased reconnect restart) and make it clear in the UI.

## [CURRENT] UI/UX (Settings & Runtime)
- GUI display mode toggle: “Current” (cards) vs “Categories” (map/list view).
- `order` / sorting: metadata for live cards and settings categories (stable ordering).
- Analog: separate input (NumberInput) vs slider; adjust slider current-value styling.

## [NEXT] Architecture / API
- Library does not include the docs folder.
- Settings auth: remove or hard-disable legacy endpoint `/config/settings_password`.
- Unified JSON handlers: route all POST/PUT config endpoints through robust JSON parsing (no manual body accumulation).
- Settings schema versioning/migration (when new fields/structure are introduced).

## [LATER] Test & CI
- Minimal: unit/integration test for auth + password reveal + bulk save/apply.
- PlatformIO CI: build matrix for at least one ESP32 env + WebUI build step.

## [NEXT] bugs

- Live-view cards are not sorted by the `order` value.
- The browser tab title (Chrome/Firefox) is always "ESP32 Configuration"; it should be configurable via `.setAppName(APP_NAME)` or a dedicated `.setTitle`.
- If `cfg.setSettingsPassword(SETTINGS_PASSWORD);` is not set, no password should be requested. Currently it always prompts for a password even if none is configured (default CM is used).
- VS Code shows an error for `#include <BME280_I2C.h>`; can this be fixed?
- In tab mode, apply-all and save-all do not work; only in card mode.


## [LATER] add as feature:

```cpp
static void logNetworkIpInfo(const char *context)
{
  const WiFiMode_t mode = WiFi.getMode();
  const bool apActive = (mode == WIFI_AP) || (mode == WIFI_AP_STA);
  const bool staConnected = (WiFi.status() == WL_CONNECTED);

  if (apActive)
  {
    const IPAddress apIp = WiFi.softAPIP();
    sl->Printf("%s: AP IP: %s", context, apIp.toString().c_str()).Debug();
    sl->Printf("%s: AP SSID: %s", context, WiFi.softAPSSID().c_str()).Debug();
    sll->Printf("AP: %s", apIp.toString().c_str()).Debug();
  }

  if (staConnected)
  {
    const IPAddress staIp = WiFi.localIP();
    sl->Printf("%s: STA IP: %s", context, staIp.toString().c_str()).Debug();
    sll->Printf("IP: %s", staIp.toString().c_str()).Debug();
  }
}
```
- what about divider (hr -like) --> add it in fulldemo
- can i add something ins system card? - add it in fulldemo
- add new module, that can be imported extra:
  - logger - but then it should be split into 3 extras (serial, MQTT, display)
    - check logger or GUI extra tab!
    - for display logger: check whether the buffer is cleared even when nothing is sent to the display
    - check where the latest version is (solarinverter project or boilerSaver)
  - mqtt manager (got it from solarinverter project! = latest version)
- copy the current ones into the dev-info folder
- check whether there are other modules that can be extracted


