#include <Arduino.h>
#include "ConfigManager.h"
#include <WiFi.h>

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Minimal-Demo"

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

// Minimal skeleton: do not hardcode WiFi credentials in code.
// Leave SSID empty to start in AP mode and configure via Web UI.
static const char SETTINGS_PASSWORD[] = "cm";

// Built-in core settings templates (WiFi/System/NTP).
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

void setup()
{
    Serial.begin(115200);

    ConfigManagerClass::setLogger([](const char *msg)
        {
            Serial.print("[ConfigManager] ");
            Serial.println(msg);
        });

    ConfigManager.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setAppTitle(APP_NAME); // Set an application title, used for web UI display
    ConfigManager.setVersion(VERSION); // Set the application version for web UI display
    ConfigManager.enableBuiltinSystemProvider();
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);
    ConfigManager.loadAll();

    ConfigManager.startWebServer();
    ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());
}

void loop()
{
    ConfigManager.updateLoopTiming();
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
