# Getting Started

This guide is a practical starting point that mirrors the `examples/minimal` pattern.

If you prefer a full working project, open one of the example projects in `examples/` (each example is a standalone PlatformIO project).

## Examples: PlatformIO setup notes

The examples are set up to use this repo as a local library via `lib_deps = file://../..` and keep build outputs outside the repository using per-example `build_dir` / `libdeps_dir` under `${sysenv.HOME}/.pio-build/...`.

Avoid using `lib_extra_dirs = ../..` for the workspace library. If a build ever seems to use stale local library code, run:

```sh
pio run -t clean
pio run
```

Some examples include a pre-build helper script `tools/pio_force_local_lib_refresh.py` to force PlatformIO to refresh the local `file://../..` library copy automatically. You can disable that behavior by setting `CM_PIO_NO_LIB_REFRESH=1`.

## Quick start (recommended)

1. Clone this repository (or your fork).
2. Pick an example in `examples/`.
3. Build / upload it with PlatformIO:

```sh
pio run -d examples/minimal -e usb
pio run -d examples/minimal -e usb -t upload --upload-port COM5
```

> Tip: You can also open `examples/minimal` directly as a PlatformIO project in VS Code.

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
        Serial.printf("[WARNING] SETUP: SSID is empty! [%s]\n", wifiSettings.wifiSsid.get().c_str());
        cfg.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        Serial.println("[INFO] AP Mode");
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

    // Optional: adjust push interval for runtime live values.
    // The WebUI prefers WebSocket (/ws) and falls back to polling (/runtime.json).
    cfg.enableWebSocketPush(2000);

    if (WiFi.status() == WL_CONNECTED)
    {
        cfg.setupOTA("esp32", "otapassword123");
    }

    Serial.printf("[INFO] Webserver running at: %s\n", WiFi.localIP().toString().c_str());
}

void loop()
{
    cfg.handleClient();
    cfg.handleWebsocketPush();
    cfg.handleOTA();
    cfg.updateLoopTiming();

    if (WiFi.status() != WL_CONNECTED && WiFi.getMode() != WIFI_AP)
    {
        Serial.println("[WARNING] WiFi lost, reconnecting...");
        cfg.reconnectWifi();
        delay(1500);
        cfg.setupOTA("esp32", "otapassword123");
    }

    delay(updateInterval.get());
}

```

## Next

- WiFi details and best practices: see `docs/WIFI.md`
- OTA and Web UI flashing: see `docs/OTA.md`
- Runtime providers and alarms: see `docs/RUNTIME.md`
- Settings and OptionGroup patterns: see `docs/SETTINGS.md`
