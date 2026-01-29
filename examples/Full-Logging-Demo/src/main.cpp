#include <Arduino.h>
#include "ConfigManager.h"
#include <WiFi.h>

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "logging/LoggingManager.h"

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Full-Logging-Demo"

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

// Minimal skeleton: do not hardcode WiFi credentials in code.
// Leave SSID empty to start in AP mode and configure via Web UI.
static const char SETTINGS_PASSWORD[] = "";

// Built-in core settings templates (WiFi/System/NTP).
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;
static cm::LoggingManager& lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level; //shorthand alias for logging levels

void logging_Example();
void Initial_logging();



void setup()
{

    Initial_logging();

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

    logging_Example();
}

void loop()
{
    ConfigManager.updateLoopTiming();
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
    lmg.loop();
}

void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    CM_LOG("[Full-Logging-Demo][INFO] Station Mode: http://%s", WiFi.localIP().toString().c_str());
}

// These hooks are invoked internally by ConfigManager's WiFi manager on state transitions.
// If you don't provide them, the library provides weak no-op defaults (see src/default_hooks.cpp).

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    CM_LOG("[Full-Logging-Demo][ERROR] WiFi disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    CM_LOG("[Full-Logging-Demo][INFO] AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}

void Initial_logging()
{
    Serial.begin(115200);
    lmg.addOutput(std::make_unique<cm::LoggingManager::SerialOutput>(Serial));
    lmg.setGlobalLevel(LL::Trace);
    lmg.attachToConfigManager(LL::Info, LL::Trace, "");

    // Add a compact output that only logs warnings and above from the "LOG" tag
    {
        auto compactOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
        compactOut->setLevel(LL::Warn);
        compactOut->setFormat(cm::LoggingManager::Output::Format::Compact);
        compactOut->setPrefix("[SHORT] ");
        compactOut->setFilter([](LL level, const char* tag, const char*) {
            return (level <= LL::Warn) && (tag && strcmp(tag, "LOG") == 0);
        });
        lmg.addOutput(std::move(compactOut));
    }

}



void logging_Example()
{
    static int randomValue = 0;
    randomValue = static_cast<int>(random(0, 1000));

    lmg.log(LL::Fatal, "LOG", "Fatal example (value=%d)", randomValue);
    lmg.log(LL::Error, "LOG", "Error example (value=%d)", randomValue);
    lmg.log(LL::Error, "Other-Tag", "Error example (value=%d)", randomValue); // This will not appear in the compact output, wrong tag
    lmg.log(LL::Warn, "LOG", "Warn example (value=%d)", randomValue);
    lmg.log(LL::Info, "LOG", "Info example (value=%d)", randomValue);
    lmg.log(LL::Debug, "LOG", "Debug example (value=%d)", randomValue);
    lmg.log(LL::Trace, "LOG", "Trace example (value=%d)", randomValue);
}
