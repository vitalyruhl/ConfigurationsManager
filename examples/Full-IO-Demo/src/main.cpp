#include <Arduino.h>
#include "ConfigManager.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <Ticker.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "io/IOManager.h"
#include "alarm/AlarmManager.h"

#if __has_include("secret/wifiSecret.h")
#include "secret/wifiSecret.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Full-IO-Demo"

// Demo pin bindings and compile-time checks for common ESP32 pin pitfalls.
#define DEMO_PIN_AP_MODE_BUTTON 13
#define DEMO_PIN_RESET_BUTTON 14
#define DEMO_PIN_TEST_BUTTON 33
#define DEMO_PIN_HEATER_RELAY 23
#define DEMO_PIN_FAN_RELAY 27
#define DEMO_PIN_HOLD_BUTTON 32
#define DEMO_PIN_LDR_VN 39
#define DEMO_PIN_LDR_VP 36
#define DEMO_PIN_AO_PERCENT 25
#define DEMO_PIN_AO_VOLT 26

#define DEMO_PIN_IS_RESERVED(p) (((p) >= 6 && (p) <= 11) || (p) == 20 || (p) == 24 || ((p) >= 28 && (p) <= 31))
#define DEMO_PIN_IS_INPUT_ONLY(p) ((p) >= 34 && (p) <= 39)
#define DEMO_PIN_IS_ADC1(p) ((p) >= 32 && (p) <= 39)
#define DEMO_PIN_IS_ADC2(p) ((p) == 0 || (p) == 2 || (p) == 4 || ((p) >= 12 && (p) <= 15) || ((p) >= 25 && (p) <= 27))
#define DEMO_PIN_IS_ADC(p) (DEMO_PIN_IS_ADC1(p) || DEMO_PIN_IS_ADC2(p))

#if ((DEMO_PIN_TEST_BUTTON < 0) || (DEMO_PIN_TEST_BUTTON > 39) || DEMO_PIN_IS_RESERVED(DEMO_PIN_TEST_BUTTON))
#warning "[W] Full-IO-Demo: DEMO_PIN_TEST_BUTTON is invalid for ESP32"
#endif

#if ((DEMO_PIN_HOLD_BUTTON < 0) || (DEMO_PIN_HOLD_BUTTON > 39) || DEMO_PIN_IS_RESERVED(DEMO_PIN_HOLD_BUTTON) || DEMO_PIN_IS_INPUT_ONLY(DEMO_PIN_HOLD_BUTTON))
#warning "[W] Full-IO-Demo: DEMO_PIN_HOLD_BUTTON is invalid for digital output on ESP32"
#endif

#if ((DEMO_PIN_HEATER_RELAY < 0) || (DEMO_PIN_HEATER_RELAY > 39) || DEMO_PIN_IS_RESERVED(DEMO_PIN_HEATER_RELAY) || DEMO_PIN_IS_INPUT_ONLY(DEMO_PIN_HEATER_RELAY))
#warning "[W] Full-IO-Demo: DEMO_PIN_HEATER_RELAY is invalid for digital output on ESP32"
#endif

#if ((DEMO_PIN_FAN_RELAY < 0) || (DEMO_PIN_FAN_RELAY > 39) || DEMO_PIN_IS_RESERVED(DEMO_PIN_FAN_RELAY) || DEMO_PIN_IS_INPUT_ONLY(DEMO_PIN_FAN_RELAY))
#warning "[W] Full-IO-Demo: DEMO_PIN_FAN_RELAY is invalid for digital output on ESP32"
#endif

#if (!DEMO_PIN_IS_ADC(DEMO_PIN_LDR_VN))
#warning "[W] Full-IO-Demo: DEMO_PIN_LDR_VN is not ADC-capable on ESP32"
#endif

#if (!DEMO_PIN_IS_ADC(DEMO_PIN_LDR_VP))
#warning "[W] Full-IO-Demo: DEMO_PIN_LDR_VP is not ADC-capable on ESP32"
#endif

#if ((DEMO_PIN_AO_PERCENT != 25) && (DEMO_PIN_AO_PERCENT != 26))
#warning "[W] Full-IO-Demo: DEMO_PIN_AO_PERCENT is not a DAC pin on ESP32 (expected 25 or 26)"
#endif

#if ((DEMO_PIN_AO_VOLT != 25) && (DEMO_PIN_AO_VOLT != 26))
#warning "[W] Full-IO-Demo: DEMO_PIN_AO_VOLT is not a DAC pin on ESP32 (expected 25 or 26)"
#endif

static const char SETTINGS_PASSWORD[] = ""; // NOTE: Empty string disables password protection for the Settings tab.

// Global theme override demo.
// Served via /user_theme.css and auto-injected by the frontend if present.
static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.card h3 { color: sandybrown !important; font-weight: 900 !important; font-size: 1.2rem !important; }

/* Emphasize DI event rows */
.ioEvent * { color: #f2b705 !important; font-weight: 700 !important; }

/* Quick action buttons */
.ioAction * { color: #fde68a !important; font-weight: 800 !important; }

/* Highlight key outputs */
.rw[data-group="Digital Outputs"][data-key="heater"] * { color: #ff6b6b !important; font-weight: 800 !important; }
.rw[data-group="Digital Outputs"][data-key="fan"] * { color: #4cc9f0 !important; font-weight: 800 !important; }

/* Analog output readbacks */
.rw[data-group="Analog Outputs"][data-key="ao_pct_value"] * { color: #7dd3fc !important; }
.rw[data-group="Analog Outputs"][data-key="ao_v_value"] * { color: #a7f3d0 !important; }
)CSS";

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();              // Core container for settings templates
static cm::CoreSystemSettings &systemSettings = coreSettings.system;               // System: OTA, WiFi reboot timeout, version
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;                     // WiFi: SSID, password, DHCP/static networking
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;                        // NTP: sync interval, servers, timezone
static cm::IOManager ioManager;
static cm::AlarmManager alarmManager;
static cm::CoreWiFiServices wifiServices;

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
void setHeaterState(bool on);
void setFanState(bool on);
void setHoldButtonState(bool on);
void setupGUI();

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

static void registerIO();
static void registerGUIforIO();
static void registerGuiTools();

static void demoAnalogOutputApi();

void checkWifiCredentials();
//--------------------------------------------------------------------


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
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);

    registerIO();
    registerGUIforIO();
    ConfigManager.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)

    coreSettings.attachWiFi(ConfigManager);     // Register WiFi baseline settings
    coreSettings.attachSystem(ConfigManager);   // Register System baseline settings
    coreSettings.attachNtp(ConfigManager);   // Register optional NTP settings bundle

    ConfigManager.checkSettingsForErrors();

    ConfigManager.loadAll(); // Load all settings from preferences, is necessary before using the settings!
    ioManager.begin();

    checkWifiCredentials();

    ConfigManager.setAccessPointMacPriority("60:B5:8D:4C:E1:D5");   // Prefer this AP is my dev-station
    ConfigManager.startWebServer();

    demoAnalogOutputApi();

    Serial.println("\nSetup completed successfully!");
}

void loop()
{

    //-------------------------------------------------------------------------------------------------------------
    // for working with the ConfigManager nessesary in loop
    ConfigManager.getWiFiManager().update(); // Update WiFi Manager - handles all WiFi logic
    ioManager.update(); // Apply IO setting changes and keep inputs/outputs state current
    alarmManager.update(); // Evaluate alarms and fire callbacks
    ConfigManager.handleClient(); // Handle web server client requests
    //-------------------------------------------------------------------------------------------------------------

    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > 60000) { // Every 60 seconds
        lastLoopLog = millis();
        Serial.printf("[MAIN] Loop running, WiFi status: %d, heap: %d\n", WiFi.status(), ESP.getFreeHeap());
    }

    delay(10);
}


//----------------------------------------
// GUI creation functions
//----------------------------------------


static void registerGUIForDI(){
    ioManager.addDigitalInputToSettingsGroup("ap_mode", "Digital - I/O", "Digital Inputs", "AP Mode Button", 8);
    ioManager.addDigitalInputToLive("ap_mode", 8, "Digital Inputs", "Inputs", "Digital Inputs", "AP Mode", false);

    ioManager.addDigitalInputToSettingsGroup("reset", "Digital - I/O", "Digital Inputs", "Reset Button", 9);
    ioManager.addDigitalInputToLive("reset", 9, "Digital Inputs", "Inputs", "Digital Inputs", "Reset", false);

    ioManager.addDigitalInputToSettingsGroup("testbutton", "Digital - I/O", "Digital Inputs", "Test Button", 10);
    ioManager.addDigitalInputToLive("testbutton", 10, "Digital Inputs", "Inputs", "Digital Inputs", "Test Button", false);


    auto diEvents = ConfigManager.liveGroup("Digital Inputs")
                        .page("Digital Inputs")
                        .card("Events")
                        .group("Test Button Events");

    diEvents.divider("Test Button Events", 11);

    diEvents.value("test_press", []() { return isPulseActive(testPressPulseUntilMs); })
        .label("Press")
        .order(12)
        .addCSSClass("ioEvent");

    diEvents.value("test_release", []() { return isPulseActive(testReleasePulseUntilMs); })
        .label("Release")
        .order(13)
        .addCSSClass("ioEvent");

    diEvents.value("test_click", []() { return isPulseActive(testClickPulseUntilMs); })
        .label("Click")
        .order(14)
        .addCSSClass("ioEvent");

    diEvents.value("test_doubleclick_toggle", []() { return testDoubleClickToggle; })
        .label("DoubleClick (Toggle)")
        .order(15)
        .addCSSClass("ioEvent");

    diEvents.value("test_longpress_toggle", []() { return testLongPressToggle; })
        .label("LongPress (Toggle)")
        .order(16)
        .addCSSClass("ioEvent");

}

static void registerGUIForDO()
{
    ioManager.addDigitalOutputToSettingsGroup("heater", "Digital - I/O", "Digital Outputs", "Heater Relay", 2);
    ioManager.addDigitalOutputToLive(cm::IOManager::RuntimeControlType::Checkbox, "heater", 2, "Digital Outputs", "Outputs", "Digital Outputs", "Heater")
        .onChangeCallback([](bool state) { setHeaterState(state); });

    ioManager.addDigitalOutputToSettingsGroup("fan", "Digital - I/O", "Digital Outputs", "Cooling Fan Relay", 3);
    ioManager.addDigitalOutputToLive(cm::IOManager::RuntimeControlType::StateButton, "fan", 3, "Digital Outputs", "Outputs", "Digital Outputs", "Fan")
        .onChangeCallback([](bool state) {
            setFanState(state);
            Serial.printf("[FAN] State: %s\n", state ? "ON" : "OFF");
        });

    ioManager.addDigitalOutputToSettingsGroup("hbtn", "Digital - I/O", "Digital Outputs", "Hold Button", 4);
    ioManager.addDigitalOutputToLive(cm::IOManager::RuntimeControlType::MomentaryButton, "hbtn", 4, "Digital Outputs", "Outputs", "Digital Outputs", "Hold")
        .onChangeCallback([](bool state) {
            setHoldButtonState(state);
            Serial.printf("[HOLDBUTTON] State: %s\n", state ? "ON" : "OFF");
        });
}

static void registerGUIForAI(){
    ioManager.addAnalogInputToSettingsGroup("ldr_s", "Analog - I/O", "Analog Inputs", "LDR VN", 11);
    ioManager.addAnalogInputToLive("ldr_s", 11, "Analog Inputs", "Inputs", "Analog Inputs", "LDR VN RAW", true);

    auto aiGroup = ConfigManager.liveGroup("Analog Inputs")
                       .page("Analog Inputs")
                       .card("Inputs")
                       .group("Analog Inputs");

    aiGroup.divider("Raw + Scaled", 20);

    ioManager.addAnalogInputToSettingsGroup("ldr_w", "Analog - I/O", "Analog Inputs", "LDR VP", 21);
    ioManager.addAnalogInputToLive("ldr_w", 21, "Analog Inputs", "Inputs", "Analog Inputs", "LDR VP", false);
    ioManager.addAnalogInputToLive("ldr_w", 22, "Analog Inputs", "Inputs", "Analog Inputs", "LDR VP RAW", true);

    alarmManager.addAnalogAlarm(
        "ldr_w_alarm",
        "LDR VP",
        []() { return ioManager.getAnalogValue("ldr_w"); },
        cm::AlarmKind::AnalogOutsideWindow,
        30.0f,
        95.0f,
        true,
        true,
        true,
        cm::AlarmSeverity::Alarm
    )
    .onAlarmCome([]() {
        Serial.println("[ALARM][ldr_w] enter");
    })
    .onAlarmGone([]() {
        Serial.println("[ALARM][ldr_w] exit");
    });

    alarmManager.addAlarmToLive(
        "ldr_w_alarm",
        23,
        "Analog Inputs",
        "Alarms",
        "Thresholds",
        "LDR VP"
    );


}

static void registerGUIForAO()
{
    ioManager.addAnalogOutputToSettingsGroup("ao_pct", "Analog - I/O", "Analog Outputs", "AO 0..100%", 41);
    ioManager.addAnalogOutputToLive(
        "ao_pct",
        41,
        0.0f,
        100.0f,
        0,
        "Analog Outputs",
        "Outputs",
        "Analog Outputs",
        "AO 0..100%",
        "%"
    );

    ioManager.addAnalogOutputValueToGUI("ao_pct", "Analog Outputs", 43, "AO 0..100% (Value)", "Analog Outputs", "%", 1);
    ioManager.addAnalogOutputValueRawToGUI("ao_pct", "Analog Outputs", 44, "AO 0..100% (DAC 0..255)", "Analog Outputs");
    ioManager.addAnalogOutputValueVoltToGUI("ao_pct", "Analog Outputs", 45, "AO 0..100% (Volts)", "Analog Outputs", 3);

    auto aoGroup = ConfigManager.liveGroup("Analog Outputs")
                       .page("Analog Outputs")
                       .card("Outputs")
                       .group("Analog Outputs");

    aoGroup.divider("AO 0..3.3V", 50);

    ioManager.addAnalogOutputToSettingsGroup("ao_v", "Analog - I/O", "Analog Outputs", "AO 0..3.3V", 52);
    ioManager.addAnalogOutputToLive(
        "ao_v",
        52,
        0.0f,
        3.3f,
        2,
        "Analog Outputs",
        "Outputs",
        "Analog Outputs",
        "AO 0..3.3V",
        "V"
    );

    ioManager.addAnalogOutputValueToGUI("ao_v", "Analog Outputs", 53, "AO 0..3.3V (Value)", "Analog Outputs", "V", 2);
    ioManager.addAnalogOutputValueRawToGUI("ao_v", "Analog Outputs", 54, "AO 0..3.3V (DAC 0..255)", "Analog Outputs");
    ioManager.addAnalogOutputValueVoltToGUI("ao_v", "Analog Outputs", 55, "AO 0..3.3V (Volts)", "Analog Outputs", 3);

}

static void registerGuiTools()
{
    auto tools = ConfigManager.liveGroup("io-tools")
                     .page("IO Tools")
                     .card("Quick Actions")
                     .group("Outputs");

    tools.button("all_off", "All Outputs OFF", []() {
            ioManager.set("heater", false);
            ioManager.set("fan", false);
            ioManager.set("hbtn", false);
            ioManager.setValue("ao_pct", 0.0f);
            ioManager.setValue("ao_v", 0.0f);
        })
        .order(10)
        .addCSSClass("ioAction");

    tools.button("all_on", "All Outputs ON", []() {
            ioManager.set("heater", true);
            ioManager.set("fan", true);
            ioManager.set("hbtn", true);
            ioManager.setValue("ao_pct", 100.0f);
            ioManager.setValue("ao_v", 3.3f);
        })
        .order(11)
        .addCSSClass("ioAction");

    tools.divider("Analog Presets", 20);

    tools.button("analog_mid", "Analog 50% / 1.65V", []() {
            ioManager.setValue("ao_pct", 50.0f);
            ioManager.setValue("ao_v", 1.65f);
        })
        .order(21)
        .addCSSClass("ioAction");
}

static void registerGUIforIO(){
    registerGUIForDI();
    registerGUIForDO();
    registerGUIForAI();
    registerGUIForAO();
    registerGuiTools();

}

//----------------------------------------
// IO creation functions
//----------------------------------------

static void createDigitalInputs()
{
    // Boot/action buttons (wired to 3.3V => active-high).
    // Use internal pulldown so idle is stable LOW.
    ioManager.addDigitalInput("ap_mode", "AP Mode Button", DEMO_PIN_AP_MODE_BUTTON, true, true, false, true);

    ioManager.addDigitalInput("reset", "Reset Button", DEMO_PIN_RESET_BUTTON, true, true, false, true);

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

    ioManager.addDigitalInput("testbutton", "Test Button", DEMO_PIN_TEST_BUTTON, true, true, false, true);

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

static void createDigitalOutputs()
{
    // Digital outputs are settings-driven and owned by IOManager.
    ioManager.addDigitalOutput("heater", "Heater Relay", DEMO_PIN_HEATER_RELAY, true, true);

    ioManager.addDigitalOutput("fan", "Cooling Fan Relay", DEMO_PIN_FAN_RELAY, true, true);

    ioManager.addDigitalOutput("hbtn", "Hold Button", DEMO_PIN_HOLD_BUTTON, true, true);
}

static void createAnalogInputs()
{
    ioManager.addAnalogInput("ldr_s", "LDR VN", DEMO_PIN_LDR_VN, true, 0, 4095, 0.0f, 100.0f, "%", 1);
    ioManager.addAnalogInput("ldr_w", "LDR VP", DEMO_PIN_LDR_VP, true, 0, 4095, 0.0f, 100.0f, "%", 1);
}

static void createAnalogOutputs()
{
    // 0..100 % -> 0..3.3V
    ioManager.addAnalogOutput("ao_pct", "AO 0..100%", DEMO_PIN_AO_PERCENT, true, 0.0f, 100.0f, false);


    // 0..3.3V direct
    ioManager.addAnalogOutput("ao_v", "AO 0..3.3V", DEMO_PIN_AO_VOLT, true, 0.0f, 3.3f, false);

}

static void registerIO(){

    createDigitalInputs();
    createDigitalOutputs();
    createAnalogInputs();
    createAnalogOutputs();

}

//----------------------------------------
// WIFI MANAGER CALLBACK FUNCTIONS
//----------------------------------------

void onWiFiConnected()
{
    Serial.println("[MAIN] WiFi connected! Activating services...");
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);

    // Show correct IP address when connected
    Serial.printf("\n\n[MAIN] Webserver running at: %s (Connected)\n", WiFi.localIP().toString().c_str());
    Serial.printf("[MAIN] WLAN-Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[MAIN] WLAN-Strength is: %s\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak"));

    String bssid = WiFi.BSSIDstr();
    Serial.printf("[MAIN] BSSID: %s (Channel: %d)\n", bssid.c_str(), WiFi.channel());
    Serial.printf("[MAIN] Local MAC: %s\n\n", WiFi.macAddress().c_str());

}

void onWiFiDisconnected()
{
    Serial.println("[MAIN] WiFi disconnected! Deactivating services...");
    wifiServices.onDisconnected();
}

void onWiFiAPMode()
{
    Serial.println("[MAIN] WiFi in AP mode");
    wifiServices.onAPMode();
}

//----------------------------------------
// Other FUNCTIONS
//----------------------------------------

static void demoAnalogOutputApi()
{
    Serial.println("[DEMO] Analog output API demo start");

    struct DemoCase {
        const char* id;
        float value;
        float rawVolts;
        int dac;
    };

    const DemoCase cases[] = {
        {"ao_pct", 25.0f, 1.0f, 64},
        // {"ao_sym", -25.0f, 2.0f, 192}, // Disabled by default (see createAnalogOutputs())
        {"ao_v", 1.65f, 3.0f, 128},
    };

    for (const auto& c : cases) {
        Serial.printf("[DEMO] id=%s\n", c.id);

        ioManager.setValue(c.id, c.value);
        Serial.printf("[DEMO] setValue=%.3f -> getValue=%.3f\n", c.value, ioManager.getValue(c.id));
        Serial.printf("[DEMO] getRawValue=%.3f V, getDACValue=%d\n", ioManager.getRawValue(c.id), ioManager.getDACValue(c.id));

        ioManager.setRawValue(c.id, c.rawVolts);
        Serial.printf("[DEMO] setRawValue=%.3f V -> getRawValue=%.3f V\n", c.rawVolts, ioManager.getRawValue(c.id));
        Serial.printf("[DEMO] getValue=%.3f, getDACValue=%d\n", ioManager.getValue(c.id), ioManager.getDACValue(c.id));

        ioManager.setDACValue(c.id, c.dac);
        Serial.printf("[DEMO] setDACValue=%d -> getDACValue=%d\n", c.dac, ioManager.getDACValue(c.id));
        Serial.printf("[DEMO] getRawValue=%.3f V, getValue=%.3f\n", ioManager.getRawValue(c.id), ioManager.getValue(c.id));
    }

    Serial.println("[DEMO] Analog output API demo end");
}

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

void setHoldButtonState(bool on)
{
    if (on)
    {
        Serial.println("Hold Button ON");
        ioManager.set("hbtn", true);
    }
    else
    {
        Serial.println("Hold Button OFF");
        ioManager.set("hbtn", false);
    }
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

