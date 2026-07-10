# Getting Started

This guide is a practical starting point that mirrors the `examples/minimal` pattern.

If you prefer a full working project, open one of the example projects in `examples/` (each example is a standalone PlatformIO project).

## Examples: PlatformIO setup notes

The examples use this repository as a local library via
`lib_deps = symlink://../..`. Per-example `build_dir` and `libdeps_dir` values
under `${sysenv.HOME}/.pio/CM/ex/...` keep build outputs outside the repository.

Avoid using `lib_extra_dirs = ../..` for the workspace library. If a build ever seems to use stale local library code, run:

```sh
pio run -t clean
pio run
```

Some examples include `tools/pio_force_local_lib_refresh.py` to refresh local
library metadata automatically. Set `CM_PIO_NO_LIB_REFRESH=1` to disable it.

## Quick start (recommended)

1. Clone this repository (or your fork).
2. Pick an example in `examples/`.
3. Build / upload it with PlatformIO:

```sh
pio run -d examples/minimal -e usb
pio run -d examples/minimal -e usb -t upload --upload-port COM5
```

> Tip: You can also open `examples/minimal` directly as a PlatformIO project in VS Code.

For a wired WT32-ETH01 project:

```sh
pio run -d examples/WT32-ETH01-v1.4 -e usb
pio run -d examples/WT32-ETH01-v1.4 -e usb -t upload
```

See [Ethernet](ETHERNET.md) for static network settings, OTA, no-OTA builds,
UART0 wiring, and power requirements.

## Minimal pattern (ESPAsyncWebServer)

This is the full minimal pattern that used to live in the README. It demonstrates:

- declaring settings via `Config<T>`
- registering settings
- loading/saving
- WiFi startup (DHCP/static)
- starting the Web UI + API
- runtime provider + live values
- OTA setup

```cpp
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "ConfigManager.h"

AsyncWebServer server(80);

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Min-Demo"

ConfigManagerClass cfg;
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr;

Config<int> updateInterval(ConfigOptions<int>{
    .key = "interval",
    .name = "Update Interval (seconds)",
    .category = "main",
    .defaultValue = 30});

struct WiFi_Settings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;

    static constexpr OptionGroup WIFI_GROUP{"wifi", "WiFi Settings"};

    WiFi_Settings() :
                      wifiSsid(WIFI_GROUP.opt<String>("ssid", "MyWiFi", "WiFi SSID")),
                      wifiPassword(WIFI_GROUP.opt<String>("password", "secretpass", "WiFi Password", true, true)),
                      useDhcp(WIFI_GROUP.opt<bool>("dhcp", false, "Use DHCP")),
                      staticIp(WIFI_GROUP.opt<String>("sIP", "192.168.2.126", "Static IP", true, false, nullptr, showIfFalse(useDhcp))),
                      gateway(WIFI_GROUP.opt<String>("GW", "192.168.2.250", "Gateway", true, false, nullptr, showIfFalse(useDhcp))),
                      subnet(WIFI_GROUP.opt<String>("subnet", "255.255.255.0", "Subnet-Mask", true, false, nullptr, showIfFalse(useDhcp)))
    {
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
        cfg.addSetting(&staticIp);
        cfg.addSetting(&gateway);
        cfg.addSetting(&subnet);
    }
};

WiFi_Settings wifiSettings;

void setup()
{
    Serial.begin(115200);
    ConfigManagerClass::setLogger([](const char *msg)
                                  { Serial.print("[CFG] "); Serial.println(msg); });

    cfg.setAppName(APP_NAME);
    cfg.setAppTitle(APP_NAME);

    cfg.addSetting(&updateInterval);
    cfg.checkSettingsForErrors();

    try
    {
        cfg.loadAll();
    }
    catch (const std::exception &e)
    {
        Serial.println(e.what());
    }

    Serial.println("Loaded configuration:");
    delay(300);

    Serial.println("Configuration printout:");
    Serial.println(cfg.toJSON(false));

    updateInterval.set(15);
    cfg.saveAll();

    if (wifiSettings.wifiSsid.get().length() == 0)
    {
        Serial.printf("[W] SSID empty: %s\n", wifiSettings.wifiSsid.get().c_str());
        cfg.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        Serial.println("[I] AP mode");
        return;
    }

    cfg.enableBuiltinSystemProvider();

    if (wifiSettings.useDhcp.get())
    {
        Serial.println("DHCP enabled");
        cfg.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }
    else
    {
        Serial.println("DHCP disabled");
        cfg.startWebServer(wifiSettings.staticIp.get(), wifiSettings.gateway.get(), wifiSettings.subnet.get(), wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }

    delay(1500);

    // Optional: enable push for runtime live values.
    // The WebUI prefers WebSocket (/ws) and falls back to polling (/runtime.json).
    // The default push interval is 5000 ms.
    cfg.enableWebSocketPush();

    if (WiFi.status() == WL_CONNECTED)
    {
        cfg.setupOTA("esp32", "otapassword123");
    }

    Serial.printf("[I] Web server: %s\n", WiFi.localIP().toString().c_str());
}

void loop()
{
    cfg.getWiFiManager().update();
    cfg.handleClient();

    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP)
    {
        Serial.println("[W] WiFi lost, reconnecting");
        cfg.reconnectWifi();
        delay(1500);
        cfg.setupOTA("esp32", "otapassword123");
    }

    delay(updateInterval.get());
}

```

        > **Note:** Every setting registered via `ConfigManager.addSetting()` now automatically gets a layout placement (page/card/group) derived from its category and Card name. If you need custom tabs or groups, call `ConfigManager.addSettingsPage()/addSettingsCard()/addSettingsGroup()` before `addSetting()` and then fine-tune placement with `ConfigManager.addToSettingsGroup(setting.getKey(), ...)`.

## Next

- WiFi details and best practices: see `docs/WIFI.md`
- Ethernet and WT32-ETH01: see `docs/ETHERNET.md`
- OTA and Web UI flashing: see `docs/OTA.md`
- GUI runtime (live, styling, theming): see `docs/GUI-Runtime.md`
- Settings and OptionGroup patterns: see `docs/SETTINGS.md`

## Method overview

| Method | Overloads / Variants | Description | Notes |
|---|---|---|---|
| `ConfigManager.startWebServer` | `startWebServer()`<br>`startWebServer(const String& ssid, const String& password)`<br>`startWebServer(const IPAddress& staticIP, const IPAddress& gateway, const IPAddress& subnet, const String& ssid, const String& password, const IPAddress& dns1 = IPAddress(), const IPAddress& dns2 = IPAddress())` | Starts WiFi + web server using settings, DHCP, or static IP. | Core startup method family. |
| `ConfigManager.startWebServerOnNetwork` | `startWebServerOnNetwork()` | Starts WebUI/runtime/OTA services on an IP interface initialized by the sketch. | Call after Ethernet or another external interface receives an IP address. |
| `ConfigManager.handleClient` | `handleClient()` | Processes HTTP/WebSocket/runtime events. | Call in `loop()`. |
| `ConfigManager.setupOTA` | `setupOTA(const String& hostname, const String& password = "")` | Enables OTA update handling after the active interface receives an IP address. | Works with WiFi or Ethernet. |

