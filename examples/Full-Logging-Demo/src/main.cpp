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

extern ConfigManagerClass ConfigManager; // Use extern to reference the instance from ConfigManager.cpp

// Minimal skeleton: do not hardcode WiFi credentials in code.
// Leave SSID empty to start in AP mode and configure via Web UI.
static const char SETTINGS_PASSWORD[] = "";

// Global theme override demo.
// Served via /user_theme.css and auto-injected by the frontend if present.
static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.card h3 { color: sandybrown !important; font-weight: 900 !important; font-size: 1.2rem !important; }
.log-line--warn { color: #f59e0b !important; }
.log-line--error { color: #ef4444 !important; font-weight: 700 !important; }
)CSS";

// Built-in core settings templates (WiFi/System/NTP).
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;
static cm::LoggingManager &lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level; // shorthand alias for logging levels

void Initial_logging_Serial();
void Initial_logging_GUI();
void logging_Example1();
void logging_Example2();
void logging_Example3();

void setup()
{

    Initial_logging_Serial();
    Initial_logging_GUI();

    ConfigManager.setAppName(APP_NAME);  // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setAppTitle(APP_NAME); // Set an application title, used for web UI display
    ConfigManager.setVersion(VERSION);   // Set the application version for web UI display
    ConfigManager.enableBuiltinSystemProvider();
    // coreSettings owns the layout for the built-in bundles now.
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);
    ConfigManager.loadAll();

    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");// dev-Station
    ConfigManager.startWebServer();

    delay(1000);
    logging_Example1();
    logging_Example2();
    logging_Example3();
}

void loop()
{
    static unsigned long lastDtLogMs = 0;
    const unsigned long nowMs = millis();
    if (nowMs - lastDtLogMs >= 30000UL) {
        lastDtLogMs = nowMs;
        const int randomValue = static_cast<int>(random(0, 1000));
        lmg.logTag(LL::Info, "DT-Info", "DateTime tagged info example (value=%d)", randomValue);
        lmg.logTag(LL::Warn, "DT-Warn", "DateTime tagged warn example (value=%d)", randomValue);
        lmg.logTag(LL::Error, "DT-Error", "DateTime tagged error example (value=%d)", randomValue);
    }

    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
    lmg.loop(); // process logging tasks
    delay(10);  // avoid busy loop
}

void onWiFiConnected()
{
    auto scoped = lmg.scopedTag("onWiFiConnected");
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    lmg.log(LL::Info,"Station Mode: http://%s", WiFi.localIP().toString().c_str());
}

// These hooks are invoked internally by ConfigManager's WiFi manager on state transitions.
// If you don't provide them, the library provides weak no-op defaults (see src/default_hooks.cpp).

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    lmg.log(LL::Error, "WiFi disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    lmg.log(LL::Info,"AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}

void Initial_logging_Serial()
{
    Serial.begin(115200);
    auto serialOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
    serialOut->setLevel(LL::Trace);
    serialOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::Millis); // add millisecond timestamp - uncommented to disable timestamp
    serialOut->setRateLimitMs(2); // limit to 1 message per 2 ms
    lmg.addOutput(std::move(serialOut)); // Default serial output

    lmg.setGlobalLevel(LL::Trace);
    auto scopedSetup = lmg.scopedTag("SETUP");
    lmg.attachToConfigManager(LL::Info, LL::Trace, "");

    // DateTime output for tags starting with "DT-"
    {
        auto dtOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
        dtOut->setLevel(LL::Warn);
        dtOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
        dtOut->setRateLimitMs(50); // limit to 1 message per 50 ms
        dtOut->setFilter([](LL, const char *tag, const char *)
                          { return tag && strncmp(tag, "DT-", 3) == 0; });
        lmg.addOutput(std::move(dtOut));
    }
}

void Initial_logging_GUI()
{

    auto guiOut = std::make_unique<cm::LoggingManager::GuiOutput>(ConfigManager, 30); // default 30-message startup buffer
    guiOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
    guiOut->setLevel(LL::Trace);
    lmg.addOutput(std::move(guiOut));


}

void logging_Example1()
{
    // Log messages with various levels and tags - explicit and implicit
    static int randomValue = 0;
    randomValue = static_cast<int>(random(0, 10));

    lmg.log(LL::Info, "Info without explicit tag (value=%d)", randomValue);

    lmg.logTag(LL::Fatal, "LOG", "Fatal example (value=%d)", randomValue);
    lmg.logTag(LL::Error, "LOG", "Error example (value=%d)", randomValue);
    lmg.logTag(LL::Warn, "LOG", "Warn example (value=%d)", randomValue);
    lmg.logTag(LL::Info, "LOG", "Info example (value=%d)", randomValue);
    lmg.logTag(LL::Debug, "LOG", "Debug example (value=%d)", randomValue);
    lmg.logTag(LL::Trace, "LOG", "Trace example (value=%d)", randomValue);

}

void logging_Example2()
{
    static int randomValue = 0;
    randomValue = static_cast<int>(random(20, 30));

    auto scoped = lmg.scopedTag("Ex2");
    auto scopedTag = lmg.scopedTag("Example2");
    lmg.log("Default level example (value=%d)", randomValue);
    lmg.log(LL::Info, "Info without explicit tag (value=%d)", randomValue);
    lmg.log(LL::Error, "Error without explicit tag (value=%d)", randomValue);
}

void logging_Example3()
{
    static int randomValue = 0;
    randomValue = static_cast<int>(random(40, 50));

    auto scoped = lmg.scopedTag("Ex3");
    auto scopedTag = lmg.scopedTag("Example-3");
    lmg.log("Default level example (value=%d)", randomValue);
    lmg.log(LL::Info, "Info without explicit tag (value=%d)", randomValue);
    lmg.log(LL::Warn, "Warn without explicit tag (value=%d)", randomValue);
    lmg.log(LL::Error, "Error without explicit tag (value=%d)", randomValue);
}

void logging_Example4()
{
    auto scopedTag = lmg.scopedTag("Example-4");
    lmg.log("Simple Info in default level");
    {
        auto scoped = lmg.scopedTag("Ex4-Inner");
        lmg.log(LL::Debug, "Debug inside scoped tag");
    }
    lmg.log(LL::Info, "Info after scoped tag");

}
