#include <Arduino.h>

// ---------------------------------------------------------------------------------------------------------------------
// ConfigManager compile-time feature toggles (moved to platformio.ini)
//
// To keep the sketch clean and allow per-environment tuning, all feature switches are now defined via
// PlatformIO build flags instead of #defines in this file.
//
// How to use:
// - Open platformio.ini and add -D flags under each [env] in the build_flags section, e.g.:
//     -DCM_ENABLE_OTA=1
//     -DCM_ENABLE_WS_PUSH=1
//     -DCM_ENABLE_RUNTIME_ANALOG_SLIDERS=1
// - You can turn features off by setting =0. The extra build script will also prune the Web UI accordingly.
//
// See docs/FEATURE_FLAGS.md for the complete list and examples. For convenience, this project enables
// most features by default in platformio.ini so you can test and then disable what you don’t need.
// [WARNING] Warning
//   ESP32 has a limitation of 15 characters for the key name.
//      The key name is built from the category and the key name (<category>_<key>).
//      The category is limited to 13 characters, the key name to 1 character.
// ---------------------------------------------------------------------------------------------------------------------
#include "ConfigManager.h"
// ---------------------------------------------------------------------------------------------------------------------

// Demo defaults (do not store real credentials in repo)
static const char SETTINGS_PASSWORD[] = ""; // emty settings deaktivates password protection for settings-tab
static const char OTA_PASSWORD[] = "ota";

#include <WiFi.h>
#include <esp_wifi.h>
#include <Ticker.h>
#include <math.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// -------------------------------------------------------------------
// Core settings templates demo
//
// This example demonstrates using the built-in core settings templates (WiFi/System/Buttons/NTP)
// from the library to keep the sketch smaller and consistent across projects.
// -------------------------------------------------------------------
#include "core/CoreSettings.h"

// IO manager demo (settings-driven digital outputs)
#include "io/IOManager.h"

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-IO-Full-Demo"
#define BUTTON_PIN_AP_MODE 13

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp
static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); } // Shorthand helper for RuntimeManager access


// Built-in core settings templates.
// These references provide shorter names for the settings bundles used in this sketch.
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();              // Core container for settings templates
static cm::CoreSystemSettings &systemSettings = coreSettings.system;               // System: OTA, WiFi reboot timeout, version
static cm::CoreButtonSettings &buttonSettings = coreSettings.buttons;              // Buttons: GPIO pins, polarity, pull configuration
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;                     // WiFi: SSID, password, DHCP/static networking
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;                        // NTP: sync interval, servers, timezone
static cm::IOManager ioManager;
// -------------------------------------------------------------------

// Taster: relay pulse helper (non-blocking)
static Ticker tasterOffTicker;


// -------------------------------------------------------------------
// Global theme override test: make all h3 headings orange without underline
// Served via /user_theme.css and auto-injected by the frontend if present.
// NOTE: We only have setCustomCss() (no _P variant yet) so we pass the PROGMEM string pointer directly.

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.rw[data-group="sensors"][data-key="temp"] .rw{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="sensors"][data-key="temp"] .val{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="sensors"][data-key="temp"] .un{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
)CSS";



//--------------------------------------------------------------------
// Forward declarations of functions
void SetupCheckForAPModeButton();
void SetupCheckForResetButton();
void updateStatusLED(); // new non-blocking status LED handler
void setHeaterState(bool on);
void setFanState(bool on);
void setupGUI();
bool SetupStartWebServer();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
void updateMockTemperature();

// #region Temperature Measurement (mocked)

float temperature = NAN; // current temperature in Celsius
static unsigned long lastTempReadMs = 0;


//this ist temporary, because all will be handled by IOManager later
struct TempSensorSettings {
    Config<int> gpioPin;      // DS18B20 data pin
    Config<float> corrOffset; // correction offset in °C
    Config<int> readInterval; // seconds

    TempSensorSettings() :
        gpioPin(ConfigOptions<int>{.key = "TsPin", .name = "GPIO Pin", .category = "Temp Sensor", .defaultValue = 21}),
        corrOffset(ConfigOptions<float>{.key = "TsOfs", .name = "Correction Offset", .category = "Temp Sensor", .defaultValue = 0.0f, .showInWeb = true}),
        readInterval(ConfigOptions<int>{.key = "TsInt", .name = "Read Interval (s)", .category = "Temp Sensor", .defaultValue = 30, .showInWeb = true})
    {}

    void init() {
        ConfigManager.addSetting(&gpioPin);
        ConfigManager.addSetting(&corrOffset);
        ConfigManager.addSetting(&readInterval);
    }
};
static TempSensorSettings tempSensorSettings;

// #endregion Temperature Measurement (mocked)


Ticker NtpSyncTicker;
bool tickerActive = false; // Used as a generic "services active" flag (WiFi/OTA/NTP)

void updateMockTemperature()
{
    // [MOCKED DATA] Replace later with real DS18B20 (and/or analog input based) reading.
    // Stable but slightly moving value for UI testing.
    const float seconds = millis() / 1000.0f;
    const float base = 42.0f;
    const float wave = 0.5f * sinf(seconds / 30.0f);
    temperature = base + wave + tempSensorSettings.corrOffset.get();
}

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);

    ConfigManagerClass::setLogger([](const char *msg)
        {
            Serial.print("[ConfigManager] ");
            Serial.println(msg);
        });

    //-----------------------------------------------------------------
    ConfigManager.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setVersion(VERSION); // Set the application version for web UI display
    ConfigManager.setAppTitle(APP_NAME); // Set an application title, used for web UI display
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1); // Register global CSS override
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD); // Set the settings password from wifiSecret.h
    ConfigManager.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    //----------------------------------------------------------------------------------------------------------------------------------

    // Initialize structured settings using Delayed Initialization Pattern
    // This avoids static initialization order problems - see docs/SETTINGS_STRUCTURE_PATTERN.md
    tempSensorSettings.init();      // DS18B20 (mocked for now) settings

    coreSettings.attach(ConfigManager);      // Register WiFi/System/Buttons core settings
    coreSettings.attachNtp(ConfigManager);   // Register optional NTP settings bundle

    // IO: declare outputs and let IOManager own/register all settings
    //todo: move it to a function
    ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
        .id = "heater",
        .name = "Heater",
        .defaultPin = 25,
        .defaultActiveLow = true,
        .defaultEnabled = true,
    });
    ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
        .id = "fan",
        .name = "Cooling Fan",
        .defaultPin = 23,
        .defaultActiveLow = true,
        .defaultEnabled = true,
    });

    // Additional relays
    ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
        .id = "taster",
        .name = "Taster",
        .defaultPin = 26,
        .defaultActiveLow = true,
        .defaultEnabled = true,
    });
    ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
        .id = "relay27",
        .name = "Relay 27",
        .defaultPin = 27,
        .defaultActiveLow = true,
        .defaultEnabled = true,
    });
    ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
        .id = "relay14",
        .name = "Relay 14",
        .defaultPin = 14,
        .defaultActiveLow = true,
        .defaultEnabled = true,
        .registerSettings = false,
    });
    // Create dedicated Settings cards for each IO item (category token stays "IO")
    ioManager.addIOtoGUI(
        "heater",
        nullptr,
        2,
        cm::IOManager::RuntimeControlType::Checkbox,
        []() { return ioManager.getState("heater"); },
        [](bool state) { setHeaterState(state); },
        "Heater"
    );

    ioManager.addIOtoGUI(
        "fan",
        nullptr,
        3,
        cm::IOManager::RuntimeControlType::StateButton,
        []() { return ioManager.getState("fan"); },
        [](bool state) {
            setFanState(state);
            Serial.printf("[FAN] State: %s\n", state ? "ON" : "OFF");
        },
        "Fan",
        "sensors"
    );

    // Taster: momentary button (pulses relay)
    ioManager.addIOtoGUI(
        "taster",
        nullptr,
        4,
        cm::IOManager::RuntimeControlType::Button,
        []() {
            ioManager.set("taster", true);
            tasterOffTicker.detach();
            tasterOffTicker.attach_ms(200, +[]() {
                ioManager.set("taster", false);
            });
        },
        "Taster"
    );

    // Additional relays as switches
    ioManager.addIOtoGUI(
        "relay27",
        nullptr,
        5,
        cm::IOManager::RuntimeControlType::Checkbox,
        []() { return ioManager.getState("relay27"); },
        [](bool state) { ioManager.set("relay27", state); },
        "Relay 27"
    );
    ioManager.addIOtoGUI(
        "relay14",
        nullptr,
        6,
        cm::IOManager::RuntimeControlType::Checkbox,
        []() { return ioManager.getState("relay14"); },
        [](bool state) { ioManager.set("relay14", state); },
        "Relay 14"
    );

    //----------------------------------------------------------------------------------------------------------------------------------

    ConfigManager.checkSettingsForErrors(); // 2025.09.04 New function to check all settings for errors (e.g., duplicate keys after truncation etc.)

    ConfigManager.loadAll(); // Load all settings from preferences, is necessary before using the settings!
    ioManager.begin();

    //----------------------------------------------------------------------------------------------------------------------------------
    // Configure Smart WiFi Roaming with default values (can be customized in setup if needed)
    ConfigManager.enableSmartRoaming(true);            // Re-enabled now that WiFi stack is fixed
    ConfigManager.setRoamingThreshold(-75);            // Trigger roaming at -75 dBm
    ConfigManager.setRoamingCooldown(30);              // Wait 30 seconds between attempts (reduced from 120)
    ConfigManager.setRoamingImprovement(10);           // Require 10 dBm improvement
    Serial.println("[MAIN] Smart WiFi Roaming enabled with WiFi stack fix");

    //----------------------------------------------------------------------------------------------------------------------------------
    // Configure WiFi AP MAC filtering/priority (example - customize as needed)
    // ConfigManager.setWifiAPMacFilter("60:B5:8D:4C:E1:D5");     // Only connect to this specific AP
    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");   // Prefer this AP, fallback to others

    SetupCheckForResetButton();
    
    SetupCheckForAPModeButton(); // check for AP mode button AFTER setting WiFi credentials

    // perform the wifi connection
    bool startedInStationMode = SetupStartWebServer();
    if (startedInStationMode)
    {
        // setupMQTT();
    }
    else
    {
        Serial.println("[SETUP] we are in AP mode");
    }

    setupGUI();

    // Enhanced WebSocket configuration
    ConfigManager.enableWebSocketPush(); // Enable WebSocket push for real-time updates
    ConfigManager.setWebSocketInterval(1000); // Faster updates - every 1 second
    ConfigManager.setPushOnConnect(true);     // Immediate data on client connect

    updateMockTemperature();
    lastTempReadMs = millis();
    //----------------------------------------------------------------------------------------------------------------------------------

    Serial.println("Loaded configuration:");

    // Show correct IP address depending on WiFi mode
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        Serial.printf("[INFO] Webserver running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[INFO] Webserver running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[INFO] Webserver running (IP not available)");
    }

    Serial.println("Configuration printout:");
    Serial.println(ConfigManager.toJSON(true)); // Show ALL settings, not just web-visible ones

    Serial.println("\nSetup completed successfully!");

    // NOTE: Avoid auto-modifying and persisting settings in examples.

    Serial.println("\n[MAIN] Setup completed successfully! Starting main loop...");
    Serial.println("=================================================================");
}

void loop()
{

    //-------------------------------------------------------------------------------------------------------------
    // for working with the ConfigManager nessesary in loop
    ConfigManager.updateLoopTiming(); // Update internal loop timing metrics for system provider
    ConfigManager.getWiFiManager().update(); // Update WiFi Manager - handles all WiFi logic
    ConfigManager.handleClient(); // Handle web server client requests
    ConfigManager.handleWebsocketPush(); // Handle WebSocket push updates
    ConfigManager.handleOTA();           // Handle OTA updates
    ioManager.update(); // Apply IO setting changes and keep outputs consistent
    //-------------------------------------------------------------------------------------------------------------


    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > 60000) { // Every 60 seconds
        lastLoopLog = millis();
        Serial.printf("[MAIN] Loop running, WiFi status: %d, heap: %d\n", WiFi.status(), ESP.getFreeHeap());
    }

    const int intervalSec = tempSensorSettings.readInterval.get() < 1 ? 1 : tempSensorSettings.readInterval.get();
    const unsigned long intervalMs = (unsigned long)intervalSec * 1000UL;
    if (millis() - lastTempReadMs >= intervalMs)
    {
        lastTempReadMs = millis();
        updateMockTemperature();
    }


    updateStatusLED();
    delay(10);
}

//----------------------------------------
// GUI SETUP
//----------------------------------------

void setupGUI()
{
    Serial.println("[GUI] setupGUI() start");
    // #region Sensors Card (Temperature)

        // Register sensor runtime provider for temperature (mocked for now)
        Serial.println("[GUI] Adding runtime provider: sensors");
        CRM().addRuntimeProvider("sensors", [](JsonObject &data)
        {
            // Apply precision to sensor values to reduce JSON size
            data["temp"] = roundf(temperature * 10.0f) / 10.0f;     // 1 decimal place
        },2);

        // Define sensor display fields using addRuntimeMeta
        Serial.println("[GUI] Adding meta: sensors.temp");
        RuntimeFieldMeta tempMeta;
        tempMeta.group = "sensors";
        tempMeta.key = "temp";
        tempMeta.label = "Temperature";
        tempMeta.unit = "°C";
        tempMeta.precision = 1;
        tempMeta.order = 10;
        CRM().addRuntimeMeta(tempMeta);
    // #endregion Sensors Card (Temperature)

    // #region Controls Card with Buttons, Toggles, and Sliders
        // Add interactive controls provider
        Serial.println("[GUI] Adding runtime provider: controls");
        CRM().addRuntimeProvider("controls", [](JsonObject &data)
        {
            // Optionally expose control states
        },3);

        // Divider between discrete controls (buttons/toggles) and analog controls
        Serial.println("[GUI] Adding meta divider: controls.analogDivider");
        RuntimeFieldMeta analogDividerMeta;
        analogDividerMeta.group = "controls";
        analogDividerMeta.key = "analogDivider";
        analogDividerMeta.label = "Analog";
        analogDividerMeta.isDivider = true;
        analogDividerMeta.order = 23;
        CRM().addRuntimeMeta(analogDividerMeta);

        // Float slider synchronized with the Temp setting (Temp.TCO)
        Serial.println("[GUI] Defining runtime float slider: controls.tempOffset");
        ConfigManager.defineRuntimeFloatSlider(
            "controls",
            "tempOffset",
            "Temperature Offset",
            -5.0f,
            5.0f,
            tempSensorSettings.corrOffset.get(),
            2,
            []() {
                return tempSensorSettings.corrOffset.get();
            },
            [](float value) {
                tempSensorSettings.corrOffset.set(value);
                Serial.printf("[TEMP_OFFSET] Value: %.2f°C\n", value);
            },
            "°C",
            "",
            24
        );

    // #endregion Controls Card with Buttons, Toggles, and Sliders
    Serial.println("[GUI] setupGUI() end");
}

//----------------------------------------
// HELPER FUNCTIONS
//----------------------------------------

// TODO(IOManager): Move this helper into a dedicated IO manager module.
// Goal: Centralize GPIO button handling (pull mode, active level, debouncing).
// API idea:
//   - bool IOManager::checkResetToDefaultsButton(const cm::CoreButtonSettings& cfg,
//       std::function<void()> onPressed);
// Notes:
//   - The callback should decide whether/when to reboot (library should not force ESP.restart()).
//   - Keep this example as the reference behavior for the future IOManager implementation.
void SetupCheckForResetButton()
{
    const int resetPin = buttonSettings.resetDefaultsPin.get();
    if (resetPin < 0)
    {
        return; // Button not present
    }

    // Configure pin based on settings (boot-time).
    pinMode(resetPin, buttonSettings.resetUsePullup.get() ? INPUT_PULLUP : INPUT_PULLDOWN);

    // check for pressed reset button
    const int resetLevel = digitalRead(resetPin);
    const bool resetPressed = buttonSettings.resetActiveLow.get() ? (resetLevel == LOW) : (resetLevel == HIGH);
    if (resetPressed)
    {
        Serial.println("[MAIN] Reset button pressed -> Reset all settings...");
        ConfigManager.clearAllFromPrefs(); // Clear all settings from EEPROM
        ConfigManager.saveAll();           // Save the default settings to EEPROM

        // Show user feedback that reset is happening
        Serial.println("[MAIN] restarting...");
        //ToDo: add non blocking delay to show message on display before restart
        ESP.restart(); // Restart the ESP32
    }
}

// TODO(IOManager): Move this helper into a dedicated IO manager module.
// API idea:
//   - bool IOManager::checkApModeButton(const cm::CoreButtonSettings& cfg,
//       std::function<void()> onPressed);
// Notes:
//   - The callback should decide what "AP mode" means (SSID, password, portal, etc.).
void SetupCheckForAPModeButton()
{
    String APName = "ESP32_Config";
    String pwd = "config1234"; // Default AP password

    if (wifiSettings.wifiSsid.get().length() == 0)
    {
        Serial.println("[MAIN] WiFi SSID is empty (fresh/unconfigured)");
        ConfigManager.startAccessPoint(APName, ""); // Only SSID and password
    }

    // check for pressed AP mode button

    // Configure pin based on settings (boot-time).
    const int apPin = buttonSettings.apModePin.get();
    if (apPin < 0)
    {
        return; // Button not present
    }

    pinMode(apPin, buttonSettings.apModeUsePullup.get() ? INPUT_PULLUP : INPUT_PULLDOWN);

    const int apLevel = digitalRead(apPin);
    const bool apPressed = buttonSettings.apModeActiveLow.get() ? (apLevel == LOW) : (apLevel == HIGH);
    if (apPressed)
    {
        Serial.println("[MAIN] AP mode button pressed -> starting AP mode...");
        ConfigManager.startAccessPoint(APName, ""); // Only SSID and password
    }
}

//----------------------------------------
// WIFI MANAGER CALLBACK FUNCTIONS
//----------------------------------------

bool SetupStartWebServer()
{
    Serial.println("[MAIN] Starting Webserver...!");

    if (WiFi.getMode() == WIFI_AP)
    {
        return false; // Skip webserver setup in AP mode
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        if (wifiSettings.useDhcp.get())
        {
            Serial.println("[MAIN] startWebServer: DHCP enabled");
            ConfigManager.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
            ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());
        }
        else
        {
            Serial.println("[MAIN] startWebServer: DHCP disabled - using static IP");
            IPAddress staticIP, gateway, subnet, dns1, dns2;
            staticIP.fromString(wifiSettings.staticIp.get());
            gateway.fromString(wifiSettings.gateway.get());
            subnet.fromString(wifiSettings.subnet.get());

            const String dnsPrimaryStr = wifiSettings.dnsPrimary.get();
            const String dnsSecondaryStr = wifiSettings.dnsSecondary.get();
            if (!dnsPrimaryStr.isEmpty())
            {
                dns1.fromString(dnsPrimaryStr);
            }
            if (!dnsSecondaryStr.isEmpty())
            {
                dns2.fromString(dnsSecondaryStr);
            }

            ConfigManager.startWebServer(staticIP, gateway, subnet, wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get(), dns1, dns2);
            ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());
        }
    }

    return true; // Webserver setup completed
}

void onWiFiConnected()
{
    Serial.println("[MAIN] WiFi connected! Activating services...");

    if (!tickerActive)
    {
        // Ensure ArduinoOTA is initialized once WiFi is connected and OTA is allowed
        // This runs on every (re)connection to guarantee espota responds
        if (systemSettings.allowOTA.get() && !ConfigManager.getOTAManager().isInitialized())
        {
            ConfigManager.setupOTA(APP_NAME, systemSettings.otaPassword.get().c_str());
        }

        tickerActive = true;
    }

    // Show correct IP address when connected
    Serial.printf("\n\n[MAIN] Webserver running at: %s (Connected)\n", WiFi.localIP().toString().c_str());
    Serial.printf("[MAIN] WLAN-Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[MAIN] WLAN-Strength is: %s\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak"));

    String bssid = WiFi.BSSIDstr();
    Serial.printf("[MAIN] BSSID: %s (Channel: %d)\n", bssid.c_str(), WiFi.channel());
    Serial.printf("[MAIN] Local MAC: %s\n\n", WiFi.macAddress().c_str());

    // Start NTP sync now and schedule periodic resyncs
    auto doNtpSync = []()
    {
        // Use TZ-aware sync for correct local time (Berlin: CET/CEST)
        configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str());
    };
    doNtpSync();
    NtpSyncTicker.detach();
    int ntpInt = ntpSettings.frequencySec.get();
    if (ntpInt < 60)
        ntpInt = 3600; // default to 1 hour
    NtpSyncTicker.attach(ntpInt, +[]()
                                 { configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str()); });
}

void onWiFiDisconnected()
{
    Serial.println("[MAIN] WiFi disconnected! Deactivating services...");
    if (tickerActive)
    {
        tickerActive = false;
    }
}

void onWiFiAPMode()
{
    Serial.println("[MAIN] WiFi in AP mode");

    // Ensure services are stopped in AP mode
    if (tickerActive)
    {
        onWiFiDisconnected(); // Reuse disconnected logic
    }
}

//----------------------------------------
// Other FUNCTIONS
//----------------------------------------


void setHeaterState(bool on)
{
    if (on)
    {
        Serial.println("Heater ON");
        ioManager.set("heater", true);
    }
    else
    {
        Serial.println("Heater OFF");
        ioManager.set("heater", false);
    }
}

void setFanState(bool on)
{
    if (on)
    {
        Serial.println("Fan ON");
        ioManager.set("fan", true);
    }
    else
    {
        Serial.println("Fan OFF");
        ioManager.set("fan", false);
    }
}

// ------------------------------------------------------------------
// Non-blocking status LED pattern
//  States / patterns:
//   - AP mode: fast blink (100ms on / 100ms off)
//   - Connected STA: slow heartbeat (on 60ms every 2s)
//   - Connecting / disconnected: double blink (2 quick pulses every 1s)
// ------------------------------------------------------------------
void updateStatusLED()
{
    static unsigned long lastChange = 0;
    static uint8_t phase = 0;
    unsigned long now = millis();

    bool apMode = WiFi.getMode() == WIFI_AP;
    bool connected = !apMode && WiFi.status() == WL_CONNECTED;

    if (apMode)
    {
        // simple fast blink 5Hz (100/100)
        if (now - lastChange >= 100)
        {
            lastChange = now;
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        }
        return;
    }

    if (connected)
    {
        // heartbeat: brief flash every 2s
        switch (phase)
        {
        case 0: // LED off idle
            if (now - lastChange >= 2000)
            {
                phase = 1;
                lastChange = now;
                digitalWrite(LED_BUILTIN, HIGH);
            }
            break;
        case 1: // LED on briefly
            if (now - lastChange >= 60)
            {
                phase = 0;
                lastChange = now;
                digitalWrite(LED_BUILTIN, LOW);
            }
            break;
        }
        return;
    }

    // disconnected / connecting: double blink every ~1s
    switch (phase)
    {
    case 0: // idle off
        if (now - lastChange >= 1000)
        {
            phase = 1;
            lastChange = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
        break;
    case 1: // first on
        if (now - lastChange >= 80)
        {
            phase = 2;
            lastChange = now;
            digitalWrite(LED_BUILTIN, LOW);
        }
        break;
    case 2: // gap
        if (now - lastChange >= 120)
        {
            phase = 3;
            lastChange = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
        break;
    case 3: // second on
        if (now - lastChange >= 80)
        {
            phase = 4;
            lastChange = now;
            digitalWrite(LED_BUILTIN, LOW);
        }
        break;
    case 4: // tail gap back to idle
        if (now - lastChange >= 200)
        {
            phase = 0;
            lastChange = now;
        }
        break;
    }
}

