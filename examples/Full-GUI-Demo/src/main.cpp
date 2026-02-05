#include <Arduino.h>

#include <Ticker.h>
#include <WiFi.h>
#include <esp_system.h>

#include "ConfigManager.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "alarm/AlarmManager.h"

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Full-GUI-Demo"

extern ConfigManagerClass ConfigManager;

static const char SETTINGS_PASSWORD[] = "cm";

// Global theme override demo.
// Served via /user_theme.css and auto-injected by the frontend if present.
static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.card h3 { color: orange; text-decoration: underline; font-weight: 900 !important; font-size: 1.2rem !important; }
/* Apply to the whole row (label + value + unit) */
.rw[data-group="sensors"][data-key="temp"]{ color:rgb(198, 16, 16) !important; font-weight:900; font-size: 1.2rem; }
.rw[data-group="sensors"][data-key="temp"] *{ color:rgb(198, 16, 16) !important; font-weight:900; font-size: 1.2rem; }

/* select the injected Value */
.rw[data-group="system"][data-key="testValue"]{ color:red !important; }
.rw[data-group="system"][data-key="testValue"] *{ color:red !important;}

)CSS";

static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); }

// Built-in core settings templates.
static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;
static cm::AlarmManager alarmManager;

// Settings shown in the Settings tab (GUI demo).
struct ExampleSettings
{
    Config<bool>* demoBool = nullptr;
    Config<int>* updateInterval = nullptr;
    Config<bool>* demoToggle = nullptr;
    Config<String>* demoVisibleWhenTrue = nullptr;
    Config<String>* demoVisibleWhenFalse = nullptr;

    void create()
    {
        demoBool = &ConfigManager.addSettingBool("tbool")
            .name("Demo Bool")
            .category("Example Settings")
            .defaultValue(true)
            .build();

        updateInterval = &ConfigManager.addSettingInt("interval")
            .name("Update Interval (seconds)")
            .category("Example Settings")
            .defaultValue(30)
            .build();

        demoToggle = &ConfigManager.addSettingBool("toggle")
            .name("Demo Toggle")
            .category("Dynamic visibility example")
            .defaultValue(true)
            .build();

        demoVisibleWhenTrue = &ConfigManager.addSettingString("trueS")
            .name("Visible When True")
            .category("Dynamic visibility example")
            .defaultValue(String("Shown if toggle = true"))
            .build();

        demoVisibleWhenFalse = &ConfigManager.addSettingString("falseS")
            .name("Visible When False")
            .category("Dynamic visibility example")
            .defaultValue(String("Shown if toggle = false"))
            .build();

        if (demoToggle && demoVisibleWhenTrue && demoVisibleWhenFalse)
        {
            demoVisibleWhenTrue->showIfFunc = [this]() { return demoToggle && demoToggle->get(); };
            demoVisibleWhenFalse->showIfFunc = [this]() { return demoToggle && !demoToggle->get(); };
        }
    }

    void placeInUi() const
    {
        if (!demoBool || !updateInterval || !demoToggle || !demoVisibleWhenTrue || !demoVisibleWhenFalse)
        {
            return;
        }

        ConfigManager.addSettingsPage("Example Settings", 40);
        ConfigManager.addSettingsGroup("Example Settings", "Example Settings", "Example Settings", 40);
        ConfigManager.addToSettingsGroup(demoBool->getKey(), "Example Settings", "Example Settings", "Example Settings", 10);
        ConfigManager.addToSettingsGroup(updateInterval->getKey(), "Example Settings", "Example Settings", "Example Settings", 20);

        ConfigManager.addSettingsPage("Dynamic visibility example", 50);
        ConfigManager.addSettingsGroup("Dynamic visibility example", "Dynamic visibility example", "Visibility Demo", 50);
        ConfigManager.addToSettingsGroup(demoToggle->getKey(), "Dynamic visibility example", "Dynamic visibility example", "Visibility Demo", 10);
        ConfigManager.addToSettingsGroup(demoVisibleWhenTrue->getKey(), "Dynamic visibility example", "Dynamic visibility example", "Visibility Demo", 20);
        ConfigManager.addToSettingsGroup(demoVisibleWhenFalse->getKey(), "Dynamic visibility example", "Dynamic visibility example", "Visibility Demo", 30);
    }
};

static ExampleSettings exampleSettings;

// [MOCKED DATA] Sensor demo values.
static Ticker sensorMockTicker;
static float mockedTemperatureC = 21.0f;
static float mockedTemperatureOffsetC = 0.0f;
static int mockedAdjustValue = 0;
static bool demoCheckboxState = false;
static bool demoStateButton = false;

static float randomFloat(float minValue, float maxValue)
{
    const uint32_t r = esp_random();
    const float normalized = (float)r / (float)UINT32_MAX;
    return minValue + normalized * (maxValue - minValue);
}

static void updateMockedSensors()
{
    const float base = randomFloat(18.0f, 28.0f);
    mockedTemperatureC = base + mockedTemperatureOffsetC + (float)mockedAdjustValue * 0.1f;
}

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

static void setupGUI();
static void cbTestButton();

void setup()
{
    Serial.begin(115200);

    ConfigManagerClass::setLogger([](const char *msg) {
        Serial.print("[ConfigManager] ");
        Serial.println(msg);
    });

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.enableBuiltinSystemProvider();
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);

    exampleSettings.create();
    exampleSettings.placeInUi();

    ConfigManager.checkSettingsForErrors();
    ConfigManager.loadAll();

    // Keep OTA enable flag reactive (optional), even though OTA init happens in wifiServices.onConnected().
    });

    // WiFi AP MAC priority (kept as requested).
    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");

    // Settings-driven WiFi startup (DHCP/static/AP fallback).
    ConfigManager.startWebServer();

    setupGUI();


    // Keep the runtime tabs ordered for the custom providers we register.
    ConfigManager.addLivePage("sensors", 10);
    ConfigManager.addLiveGroup("sensors", "Live Values", "Sensors", 10);
    ConfigManager.addLivePage("controls", 20);
    ConfigManager.addLiveGroup("controls", "Live Values", "Controls", 20);
    ConfigManager.addLivePage("alerts", 30);
    ConfigManager.addLiveGroup("alerts", "Live Values", "Alerts", 30);
    ConfigManager.addLivePage("system", 40);
    ConfigManager.addLiveGroup("system", "Live Values", "System", 40);

    Serial.println("[MOCKED DATA] Sensor values are randomized every 3 seconds");
    updateMockedSensors();
    sensorMockTicker.attach(3.0f, updateMockedSensors);

    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA)
    {
        Serial.printf("[INFO] Webserver running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        Serial.printf("[INFO] Webserver running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        Serial.println("[INFO] Webserver running (IP not available)");
    }

    Serial.println("[MAIN] Setup completed successfully. Starting main loop...");
}

void loop()
{
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();

    static unsigned long lastAlarmEval = 0;
    if (millis() - lastAlarmEval > 1500)
    {
        lastAlarmEval = millis();
        alarmManager.update();
    }

    delay(10);
}

static void setupGUI()
{
    Serial.println("[GUI] setupGUI() start");

    // Sensors card (GUI-only demo, values are mocked).
    CRM().addRuntimeProvider("sensors", [](JsonObject &data) {
        data["temp"] = roundf(mockedTemperatureC * 10.0f) / 10.0f;
    }, 2);

    RuntimeFieldMeta tempMeta;
    tempMeta.group = "sensors";
    tempMeta.key = "temp";
    tempMeta.label = "Temperature [MOCKED DATA]";
    tempMeta.unit = "°C";
    tempMeta.precision = 1;
    tempMeta.order = 10;
    CRM().addRuntimeMeta(tempMeta);

    // Controls card (GUI interaction demo, no hardware IO here).
    CRM().addRuntimeProvider("controls", [](JsonObject &data) {
        data["checkbox"] = demoCheckboxState;
        data["state"] = demoStateButton;
        data["adjust"] = mockedAdjustValue;
        data["tempOffset"] = mockedTemperatureOffsetC;
    }, 3);

    ConfigManager.defineRuntimeButton("controls", "testBtn", "Test Button", []() {
        cbTestButton();
    }, "", 20);

    ConfigManager.defineRuntimeCheckbox(
        "controls",
        "demoCheckbox",
        "Demo Checkbox",
        []() { return demoCheckboxState; },
        [](bool state) { demoCheckboxState = state; },
        "",
        21);

    ConfigManager.defineRuntimeStateButton(
        "controls",
        "demoState",
        "Demo State",
        []() { return demoStateButton; },
        [](bool state) { demoStateButton = state; },
        false,
        "",
        22);

    RuntimeFieldMeta analogDividerMeta;
    analogDividerMeta.group = "controls";
    analogDividerMeta.key = "analogDivider";
    analogDividerMeta.label = "Analog";
    analogDividerMeta.isDivider = true;
    analogDividerMeta.order = 23;
    CRM().addRuntimeMeta(analogDividerMeta);

    ConfigManager.defineRuntimeIntSlider(
        "controls",
        "adjust",
        "Adjustment",
        -10,
        10,
        0,
        []() { return mockedAdjustValue; },
        [](int value) { mockedAdjustValue = value; },
        "UNIT",
        "steps",
        25);

    ConfigManager.defineRuntimeFloatSlider(
        "controls",
        "tempOffset",
        "Temperature Offset",
        -5.0f,
        5.0f,
        mockedTemperatureOffsetC,
        2,
        []() { return mockedTemperatureOffsetC; },
        [](float value) { mockedTemperatureOffsetC = value; },
        "°C",
        "",
        26);

    // Alarms demo.
    alarmManager.addDigitalWarning(
        {
            .id = "overheat",
            .name = "Overheat Warning",
            .kind = cm::AlarmKind::DigitalActive,
            .severity = cm::AlarmSeverity::Warning,
            .enabled = true,
            .getter = []() { return mockedTemperatureC > 26.0f; },
        })
        .addCSSClass("stateDotOnAlarm", "alarm-overheat");
    alarmManager.addWarningToLive(
        "overheat",
        28,
        "alerts",
        "Live Values",
        "Alerts",
        "Overheat Warning");

    CRM().addRuntimeProvider("Alerts", [](JsonObject &data) {
        data["connected"] = WiFi.status() == WL_CONNECTED;
    }, 1);

    RuntimeFieldMeta connectedMeta;
    connectedMeta.group = "Alerts";
    connectedMeta.key = "connected";
    connectedMeta.label = "Connected";
    connectedMeta.order = 29;
    connectedMeta.isBool = true;
    CRM().addRuntimeMeta(connectedMeta);

    // Overheat alarm meta is provided by AlarmManager

    // Runtime provider injection into the built-in system card.
    CRM().addRuntimeProvider("system", [](JsonObject &data) {
        data["testValue"] = roundf(mockedTemperatureC * 10.0f) / 10.0f;
    }, 99);

    RuntimeFieldMeta systemCustomDivider;
    systemCustomDivider.group = "system";
    systemCustomDivider.key = "customDivider";
    systemCustomDivider.label = "Custom";
    systemCustomDivider.isDivider = true;
    systemCustomDivider.order = 98;
    CRM().addRuntimeMeta(systemCustomDivider);

    RuntimeFieldMeta testValueMeta;
    testValueMeta.group = "system";
    testValueMeta.key = "testValue";
    testValueMeta.label = "Injected Value";
    testValueMeta.order = 99;
    testValueMeta.unit = "°C";
    testValueMeta.precision = 1;
    CRM().addRuntimeMeta(testValueMeta);

    Serial.println("[GUI] setupGUI() end");
}

static void cbTestButton()
{
    Serial.println("[GUI] Test Button pressed");
}

// Global WiFi event hooks used by ConfigManager.
void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    Serial.printf("[INFO] Station Mode: http://%s\n", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    Serial.printf("[INFO] AP Mode: http://%s\n", WiFi.softAPIP().toString().c_str());
}
