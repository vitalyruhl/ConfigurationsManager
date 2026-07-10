#include <Arduino.h>
#include <ETH.h>

#include "ConfigManager.h"
#include "core/CoreSettings.h"

#define VERSION CONFIGMANAGER_VERSION

#ifndef APP_NAME
#define APP_NAME "CM-WT32-ETH01"
#endif

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD "ota"
#endif

namespace
{
const IPAddress localIp(192, 168, 2, 127);
const IPAddress gateway(192, 168, 2, 1);
const IPAddress subnet(255, 255, 255, 0);
const IPAddress dnsPrimary(192, 168, 2, 1);

volatile bool ethernetHasIp = false;
bool networkServicesStarted = false;

cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
cm::CoreSystemSettings &systemSettings = coreSettings.system;

void onNetworkEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        ETH.setHostname(APP_NAME);
        Serial.println("[I] Ethernet started");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("[I] Ethernet link connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        ethernetHasIp = true;
        Serial.printf("[I] Ethernet ready: http://%s\n", ETH.localIP().toString().c_str());
        break;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    case ARDUINO_EVENT_ETH_LOST_IP:
        ethernetHasIp = false;
        Serial.println("[W] Ethernet lost IP");
        break;
#endif
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        ethernetHasIp = false;
        Serial.println("[W] Ethernet disconnected");
        break;
    case ARDUINO_EVENT_ETH_STOP:
        ethernetHasIp = false;
        Serial.println("[W] Ethernet stopped");
        break;
    default:
        break;
    }
}
}

void setup()
{
    Serial.begin(115200);
    delay(200);

    ConfigManagerClass::setLogger([](const char *message)
        {
            Serial.print("[CM] ");
            Serial.println(message);
        });

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    ConfigManager.enableBuiltinSystemProvider();
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);

    coreSettings.attachSystem(ConfigManager);
    ConfigManager.loadAll();

#if CM_ENABLE_OTA
    if (systemSettings.otaPassword.get() != OTA_PASSWORD)
    {
        systemSettings.otaPassword.save(OTA_PASSWORD);
    }
#endif

    WiFi.onEvent(onNetworkEvent);

    if (!ETH.begin())
    {
        Serial.println("[E] Ethernet PHY init failed");
        return;
    }

    if (!ETH.config(localIp, gateway, subnet, dnsPrimary))
    {
        Serial.println("[E] Ethernet static IP config failed");
    }
}

void loop()
{
    if (ethernetHasIp && !networkServicesStarted)
    {
        ConfigManager.startWebServerOnNetwork();
#if CM_ENABLE_OTA
        ConfigManager.setupOTA(APP_NAME, systemSettings.otaPassword.get());
#endif
        networkServicesStarted = true;
    }

    ConfigManager.handleClient();
    delay(1);
}
