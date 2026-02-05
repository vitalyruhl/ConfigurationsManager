#include <Arduino.h>
#include "ConfigManager.h"
#include <WiFi.h>

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

#if __has_include("secret/wifiSecret.h")
#include "secret/wifiSecret.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif


#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Minimal-Demo"

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
void checkWifiCredentials();

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

// Leave SSID empty to start in AP mode and configure via Web UI.
static const char SETTINGS_PASSWORD[] = "";

// Built-in core settings templates (WiFi/System/NTP).
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

void setup()
{
    Serial.begin(115200);

    ConfigManagerClass::setLogger([](const char *msg)
        {
            Serial.print("[CM] ");
            Serial.println(msg);
        });

    ConfigManager.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setAppTitle(APP_NAME); // Set an application title, used for web UI display
    ConfigManager.setVersion(VERSION); // Set the application version for web UI display
    ConfigManager.enableBuiltinSystemProvider();
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);

    coreSettings.attachWiFi(ConfigManager, "WLAN", "WLAN-Einstellungen", 10);
    coreSettings.attachSystem(ConfigManager);
    // coreSettings.attachNtp(ConfigManager); // you dont need it for this minimal example, but you can easily add it back if you want to use the NTP features
    ConfigManager.loadAll();

    checkWifiCredentials();

    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");// you dont need it, bi ut it makes testing easier for me

    ConfigManager.startWebServer();
}

void loop()
{
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
}

void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    Serial.printf("[INFO] Station Mode: http://%s\n", WiFi.localIP().toString().c_str());
}

// These hooks are invoked internally by ConfigManager's WiFi manager on state transitions.
// If you don't provide them, the library provides weak no-op defaults (see src/default_hooks.cpp).

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    Serial.println("[ERROR] WiFi disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    Serial.printf("[INFO] AP Mode: http://%s\n", WiFi.softAPIP().toString().c_str());
}

void checkWifiCredentials()
{
    if (wifiSettings.wifiSsid.get().isEmpty())
    {
#if CM_HAS_WIFI_SECRETS
        Serial.println("-------------------------------------------------------------");
        Serial.println("SETUP: *** SSID is empty, setting My values *** ");
        Serial.println("-------------------------------------------------------------");
        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);

        // Optional secret fields (not present in every example).
#ifdef MY_WIFI_IP
        wifiSettings.staticIp.set(MY_WIFI_IP);
#endif
#ifdef MY_USE_DHCP
        wifiSettings.useDhcp.set(MY_USE_DHCP);
#endif
#ifdef MY_GATEWAY_IP
        wifiSettings.gateway.set(MY_GATEWAY_IP);
#endif
#ifdef MY_SUBNET_MASK
        wifiSettings.subnet.set(MY_SUBNET_MASK);
#endif
#ifdef MY_DNS_IP
        wifiSettings.dnsPrimary.set(MY_DNS_IP);
#endif
        ConfigManager.saveAll();
        Serial.println("-------------------------------------------------------------");
        Serial.println("Restarting ESP, after auto setting WiFi credentials");
        Serial.println("-------------------------------------------------------------");
        delay(500);
        ESP.restart();
#else
        Serial.println("SETUP: WiFi SSID is empty but secret/wifiSecret.h is missing; using UI/AP mode");
#endif
    }
}
