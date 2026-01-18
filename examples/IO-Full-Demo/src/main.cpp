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

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp
static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); } // Shorthand helper for RuntimeManager access


// Built-in core settings templates.
// These references provide shorter names for the settings bundles used in this sketch.
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();              // Core container for settings templates
static cm::CoreSystemSettings &systemSettings = coreSettings.system;               // System: OTA, WiFi reboot timeout, version
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;                     // WiFi: SSID, password, DHCP/static networking
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;                        // NTP: sync interval, servers, timezone
static cm::IOManager ioManager;
// -------------------------------------------------------------------

static uint32_t testPressPulseUntilMs = 0;
static uint32_t testReleasePulseUntilMs = 0;
static uint32_t testClickPulseUntilMs = 0;
static bool testDoubleClickToggle = false;
static bool testLongPressToggle = false;

static constexpr uint32_t TEST_EVENT_PULSE_MS = 700;

static bool isPulseActive(uint32_t untilMs)
{
    return static_cast<int32_t>(millis() - untilMs) <= 0;
}

//--------------------------------------------------------------------
// Forward declarations of functions
void updateStatusLED(); // new non-blocking status LED handler
void setHeaterState(bool on);
void setFanState(bool on);
void setupGUI();
bool SetupStartWebServer();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

static void createDigitalOutputs();
static void registerDigitalOutputsGui();
static void createDigitalInputs();
static void createAnalogInputs();
static void createAnalogOutputs();


static void createDigitalOutputs()
{
    // Digital outputs are settings-driven and owned by IOManager.
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
        .id = "holdbutton",
        .name = "holdbutton",
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
}

static void registerDigitalOutputsGui()
{
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

    // Momentary button: hold-to-activate (pressed -> ON, released -> OFF)
    ioManager.addIOtoGUI(
        "holdbutton",
        nullptr,
        4,
        cm::IOManager::RuntimeControlType::MomentaryButton,
        []() { return ioManager.getState("holdbutton"); },
        [](bool state) { ioManager.set("holdbutton", state); },
        "Holdbutton",
        "controls",
        "Running",
        "Push"
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
        cm::IOManager::RuntimeControlType::StateButton,
        []() { return ioManager.getState("relay14"); },
        [](bool state) { ioManager.set("relay14", state); },
        "Start/Stop",
        "controls",
        "Stop",
        "Start"
    );
}

static void createDigitalInputs()
{
    // Boot/action buttons (wired to 3.3V => active-high).
    // Use internal pulldown so idle is stable LOW.
    ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
        .id = "ap_mode",
        .name = "AP Mode Button",
        .defaultPin = 13,
        .defaultActiveLow = false,
        .defaultPullup = false,
        .defaultPulldown = true,
        .defaultEnabled = true,
    });

    ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
        .id = "reset",
        .name = "Reset Button",
        .defaultPin = 15,
        .defaultActiveLow = false,
        .defaultPullup = false,
        .defaultPulldown = true,
        .defaultEnabled = true,
    });

    ioManager.addInputToGUI("ap_mode", nullptr, 8, "AP Mode", "inputs", false);
    ioManager.addInputToGUI("reset", nullptr, 9, "Reset", "inputs", false);

    cm::IOManager::DigitalInputEventOptions apOptions;
    apOptions.longClickMs = 1200;
    ioManager.configureDigitalInputEvents(
        "ap_mode",
        cm::IOManager::DigitalInputEventCallbacks{
            .onLongPressOnStartup = []() {
                Serial.println("[INPUT][ap_mode] LongPressOnStartup -> starting AP mode");
                ConfigManager.startAccessPoint("ESP32_Config", "");
            }
        },
        apOptions
    );

    cm::IOManager::DigitalInputEventOptions resetOptions;
    resetOptions.longClickMs = 2500;
    ioManager.configureDigitalInputEvents(
        "reset",
        cm::IOManager::DigitalInputEventCallbacks{
            .onLongPressOnStartup = []() {
                Serial.println("[INPUT][reset] LongPressOnStartup -> reset settings and restart");
                ConfigManager.clearAllFromPrefs();
                ConfigManager.saveAll();
                ESP.restart();
            }
        },
        resetOptions
    );

    // Digital input demo: button wired to 3.3V (active-high).
    // With defaultPulldown=true we enable the internal pulldown (idle = LOW, pressed = HIGH).
    ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
        .id = "testbutton",
        .name = "Test Button",
        .defaultPin = 33,
        .defaultActiveLow = false,
        .defaultPullup = false,
        .defaultPulldown = true,
        .defaultEnabled = true,
    });

    // Show as bool dot in runtime.
    ioManager.addInputToGUI(
        "testbutton",
        nullptr,
        10,
        "Test Button",
        "inputs",
        false
    );

    // Divider + per-event indicators for test button
    {
        RuntimeFieldMeta divider;
        divider.group = "inputs";
        divider.key = "testbutton_events";
        divider.label = "Test Button Events";
        divider.isDivider = true;
        divider.order = 11;
        CRM().addRuntimeMeta(divider);

        RuntimeFieldMeta meta;
        meta.group = "inputs";

        meta.key = "test_press";
        meta.label = "Press";
        meta.isBool = true;
        meta.order = 12;
        CRM().addRuntimeMeta(meta);

        meta.key = "test_release";
        meta.label = "Release";
        meta.isBool = true;
        meta.order = 13;
        CRM().addRuntimeMeta(meta);

        meta.key = "test_click";
        meta.label = "Click";
        meta.isBool = true;
        meta.order = 14;
        CRM().addRuntimeMeta(meta);

        meta.key = "test_doubleclick_toggle";
        meta.label = "DoubleClick (Toggle)";
        meta.isBool = true;
        meta.order = 15;
        CRM().addRuntimeMeta(meta);

        meta.key = "test_longpress_toggle";
        meta.label = "LongPress (Toggle)";
        meta.isBool = true;
        meta.order = 16;
        CRM().addRuntimeMeta(meta);

        CRM().addRuntimeProvider("inputs", [](JsonObject& data) {
            data["test_press"] = isPulseActive(testPressPulseUntilMs);
            data["test_release"] = isPulseActive(testReleasePulseUntilMs);
            data["test_click"] = isPulseActive(testClickPulseUntilMs);
            data["test_doubleclick_toggle"] = testDoubleClickToggle;
            data["test_longpress_toggle"] = testLongPressToggle;
        }, 6);
    }

    ioManager.configureDigitalInputEvents(
        "testbutton",
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []() {
                testPressPulseUntilMs = millis() + TEST_EVENT_PULSE_MS;
                Serial.println("[INPUT][testbutton] Press");
            },
            .onRelease = []() {
                testReleasePulseUntilMs = millis() + TEST_EVENT_PULSE_MS;
                Serial.println("[INPUT][testbutton] Release");
            },
            .onClick = []() {
                testClickPulseUntilMs = millis() + TEST_EVENT_PULSE_MS;
                Serial.println("[INPUT][testbutton] Click");
            },
            .onDoubleClick = []() {
                testDoubleClickToggle = !testDoubleClickToggle;
                Serial.printf("[INPUT][testbutton] DoubleClick -> toggle=%s\n", testDoubleClickToggle ? "true" : "false");
            },
            .onLongClick = []() {
                testLongPressToggle = !testLongPressToggle;
                Serial.printf("[INPUT][testbutton] LongClick -> toggle=%s\n", testLongPressToggle ? "true" : "false");
            },
        }
    );
}

static void createAnalogInputs()
{
    // LDR cross (solar tracker) - ADC1 pins (WiFi-safe): 34, 35, 36(VP), 39(VN)
    // Note: These pins are input-only, which is fine for analog sensors.

    ioManager.addAnalogInput(cm::IOManager::AnalogInputBinding{
        .id = "ldr_n",
        .name = "LDR North",
        .defaultPin = 34,
        .defaultRawMin = 0,
        .defaultRawMax = 4095,
        .defaultOutMin = 0.0f,
        .defaultOutMax = 100.0f,
        .defaultUnit = "%",
        .defaultPrecision = 1,
    });
    ioManager.addAnalogInputToGUI("ldr_n", nullptr, 13, "LDR North", "sensors", true, true);

    ioManager.addAnalogInput(cm::IOManager::AnalogInputBinding{
        .id = "ldr_e",
        .name = "LDR East",
        .defaultPin = 35,
        .defaultRawMin = 0,
        .defaultRawMax = 4095,
        .defaultOutMin = 0.0f,
        .defaultOutMax = 100.0f,
        .defaultUnit = "%",
        .defaultPrecision = 1,
    });
    ioManager.addAnalogInputToGUI("ldr_e", nullptr, 15, "LDR East", "sensors", true, true);

    ioManager.addAnalogInput(cm::IOManager::AnalogInputBinding{
        .id = "ldr_s",
        .name = "LDR South",
        .defaultPin = 36,
        .defaultRawMin = 0,
        .defaultRawMax = 4095,
        .defaultOutMin = 0.0f,
        .defaultOutMax = 100.0f,
        .defaultUnit = "%",
        .defaultPrecision = 1,
    });
    ioManager.addAnalogInputToGUI("ldr_s", nullptr, 17, "LDR South", "sensors", true, true);

    ioManager.addAnalogInput(cm::IOManager::AnalogInputBinding{
        .id = "ldr_w",
        .name = "LDR West",
        .defaultPin = 39,
        .defaultRawMin = 0,
        .defaultRawMax = 4095,
        .defaultOutMin = 0.0f,
        .defaultOutMax = 100.0f,
        .defaultUnit = "%",
        .defaultPrecision = 1,
    });
    ioManager.addAnalogInputToGUI("ldr_w", nullptr, 19, "LDR West", "sensors", true, true);
}

static void createAnalogOutputs()
{
    // TODO: Placeholder for future analog output initialization.
}


Ticker NtpSyncTicker;
bool tickerActive = false; // Used as a generic "services active" flag (WiFi/OTA/NTP)

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);

    ConfigManagerClass::setLogger([](const char *msg)
        {
            Serial.print("[ConfigManager] ");
            Serial.println(msg);
        });

    //-----------------------------------------------------------------
    ConfigManager.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setVersion(VERSION); // Set the application version for web UI display
    ConfigManager.setAppTitle(APP_NAME); // Set an application title, used for web UI display
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD); // Set the settings password from wifiSecret.h
    ConfigManager.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    //----------------------------------------------------------------------------------------------------------------------------------

    coreSettings.attachWiFi(ConfigManager);     // Register WiFi baseline settings
    coreSettings.attachSystem(ConfigManager);   // Register System baseline settings
    coreSettings.attachNtp(ConfigManager);   // Register optional NTP settings bundle

    createDigitalOutputs();
    createDigitalInputs();
    createAnalogInputs();
    createAnalogOutputs();
    registerDigitalOutputsGui();

    //----------------------------------------------------------------------------------------------------------------------------------

    ConfigManager.checkSettingsForErrors(); // 2025.09.04 New function to check all settings for errors (e.g., duplicate keys after truncation etc.)

    ConfigManager.loadAll(); // Load all settings from preferences, is necessary before using the settings!
    ioManager.begin();

    // Boot behavior:
    // - If WiFi SSID is empty (fresh reset/unconfigured), start AP mode automatically.
    // - Avoid instant reset loops: do NOT reset on "pressed at boot"; reset/AP are handled via LongPressOnStartup event.
    const bool ssidEmpty = (wifiSettings.wifiSsid.get().length() == 0);
    if (ssidEmpty) {
        Serial.println("[BOOT] WiFi SSID is empty -> starting AP mode");
        ConfigManager.startAccessPoint("ESP32_Config", "");
    }

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

    // perform the wifi connection (skip if we are in AP mode)
    bool startedInStationMode = false;
    if (!ssidEmpty && WiFi.getMode() != WIFI_AP) {
        startedInStationMode = SetupStartWebServer();
    }
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
    ConfigManager.setWebSocketInterval(250); // Faster updates - every 250ms
    ConfigManager.setPushOnConnect(true);     // Immediate data on client connect
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
    ioManager.update(); // Apply IO setting changes and keep inputs/outputs state current
    ConfigManager.handleClient(); // Handle web server client requests
    ConfigManager.handleWebsocketPush(); // Handle WebSocket push updates
    ConfigManager.handleOTA();           // Handle OTA updates
    //-------------------------------------------------------------------------------------------------------------

    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > 60000) { // Every 60 seconds
        lastLoopLog = millis();
        Serial.printf("[MAIN] Loop running, WiFi status: %d, heap: %d\n", WiFi.status(), ESP.getFreeHeap());
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
    // #region Controls Card with Buttons, Toggles, and Sliders
        // Add interactive controls provider
        Serial.println("[GUI] Adding runtime provider: controls");
        CRM().addRuntimeProvider("controls", [](JsonObject &data)
        {
            // Optionally expose control states
        },3);

    // #endregion Controls Card with Buttons, Toggles, and Sliders
    Serial.println("[GUI] setupGUI() end");
}

//----------------------------------------
// HELPER FUNCTIONS
//----------------------------------------

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

