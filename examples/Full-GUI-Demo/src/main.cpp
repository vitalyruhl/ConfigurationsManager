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
.card h3 { color: sandybrown !important; font-weight: 900 !important; font-size: 1.3rem !important; }
/* Apply to the whole row (label + value + unit) */
.myCSSTemperatureClass * { color:rgb(198, 16, 16) !important; font-weight:900 !important; font-size: 1.2rem !important; }

/* select the injected Value */
.rw[data-group="system"][data-key="testValue"]{ color:red !important; }
.rw[data-group="system"][data-key="testValue"] *{ color:red !important;}

)CSS";


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
static float mockedHumidity = 45.0f;
static float mockedPressure = 1013.0f;
static float mockedDewPointC = 10.0f;
static bool mockedDewpointRisk = false;
static int mockedAdjustValue = 0;
static bool demoCheckboxState = false;
static bool demoStateButton = false;
static bool demoMomentaryState = false;
static int demoIntInputValue = 5;
static float demoFloatInputValue = 1.0f;

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
    mockedHumidity = randomFloat(35.0f, 70.0f);
    mockedPressure = randomFloat(990.0f, 1030.0f);
    const float dewpointNoise = randomFloat(-1.5f, 3.0f);
    mockedDewPointC = mockedTemperatureC - ((100.0f - mockedHumidity) / 5.0f) + dewpointNoise;
    mockedDewpointRisk = mockedTemperatureC < mockedDewPointC;
}

void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

static void setupGUI();
static void setupGuiSensors();
static void setupGuiIoTest();
static void setupGuiSystem();
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
    
    setupGUI();

    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");
    ConfigManager.startWebServer();

    Serial.println("[MOCKED DATA] Sensor values are randomized every 3 seconds");
    updateMockedSensors();
    sensorMockTicker.attach(3.0f, updateMockedSensors);

    Serial.println("[MAIN] Setup completed successfully. Starting main loop...");
}

void loop()
{
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();

    alarmManager.update();

    delay(10);
}

static void setupGUI()
{
    Serial.println("[GUI] setupGUI() start");

    // Live layout is defined by the builder calls below.

    setupGuiSensors();
    setupGuiIoTest();
    setupGuiSystem();

    Serial.println("[GUI] setupGUI() end");
}

static void setupGuiSensors()
{
    // BME280-style sensor card (GUI-only demo, values are mocked).
    auto sensors = ConfigManager.liveGroup("sensors")
                       .page("Sensors", 10)
                       .card("BME280 - Temperature Sensor", 10);

    sensors.value("temp", []() { return roundf(mockedTemperatureC * 10.0f) / 10.0f; })
        .label("Temperature [MOCKED DATA]")
        .unit("°C")
        .precision(1)
        .order(10)
        .addCSSClass("myCSSTemperatureClass");

    sensors.value([]() { return roundf(mockedHumidity * 10.0f) / 10.0f; })
        .label("Humidity")
        .unit("%")
        .precision(1)
        .order(11);

    sensors.value([]() { return roundf(mockedPressure * 10.0f) / 10.0f; })
        .label("Pressure")
        .unit("hPa")
        .precision(1)
        .order(12);

    auto dewpointGroup = ConfigManager.liveGroup("sensors")
                            .page("Sensors", 10)
                            .card("BME280 - Temperature Sensor", 10)
                            .group("Dewpoint", 20);

    dewpointGroup.value("dew", []() { return roundf(mockedDewPointC * 10.0f) / 10.0f; })
        .label("Dewpoint")
        .unit("°C")
        .precision(1)
        .order(20);

    alarmManager.addDigitalWarning(
        {
            .id = "dewRisk",
            .name = "Condensation Risk",
            .kind = cm::AlarmKind::DigitalActive,
            .severity = cm::AlarmSeverity::Warning,
            .enabled = true,
            .getter = []() { return mockedDewpointRisk; },
        });
    alarmManager.addWarningToLive(
        "dewRisk",
        30,
        "Sensors",
        "BME280 - Temperature Sensor",
        "Dewpoint",
        "Condensation Risk");
}

static void setupGuiIoTest()
{
    // IO test page (GUI interaction demo, no hardware IO here).
    auto ioButtons = ConfigManager.liveGroup("io-test")
                         .page("IO Test", 20)
                         .card("Buttons", 10);

    ioButtons.button("testBtn", "Test Button", []() { cbTestButton(); })
        .order(10);

    ioButtons.checkbox(
        "demoCheckbox",
        "Demo Checkbox",
        []() { return demoCheckboxState; },
        [](bool state) { demoCheckboxState = state; })
        .order(11);

    ioButtons.stateButton(
        "demoState",
        "Demo State",
        []() { return demoStateButton; },
        [](bool state) { demoStateButton = state; },
        false,
        "On",
        "Off")
        .order(12);

    ioButtons.momentaryButton(
        "demoMomentary",
        "Momentary Button",
        []() { return demoMomentaryState; },
        [](bool state) { demoMomentaryState = state; },
        "Press",
        "Release")
        .order(13);

    auto ioSliders = ConfigManager.liveGroup("io-test")
                         .page("IO Test", 20)
                         .card("Sliders", 20);

    ioSliders.intSlider(
        "adjust",
        "Adjustment",
        -10,
        10,
        0,
        []() { return mockedAdjustValue; },
        [](int value) { mockedAdjustValue = value; },
        "UNIT")
        .order(10);

    ioSliders.floatSlider(
        "tempOffset",
        "Temperature Offset",
        -5.0f,
        5.0f,
        mockedTemperatureOffsetC,
        2,
        []() { return mockedTemperatureOffsetC; },
        [](float value) { mockedTemperatureOffsetC = value; },
        "°C")
        .order(11);

    auto ioInputs = ConfigManager.liveGroup("io-test")
                        .page("IO Test", 20)
                        .card("Inputs", 30);

    ioInputs.intInput(
        "demoIntInput",
        "Int Input",
        -100,
        100,
        demoIntInputValue,
        []() { return demoIntInputValue; },
        [](int value) { demoIntInputValue = value; },
        "UNIT")
        .order(10);

    ioInputs.floatInput(
        "demoFloatInput",
        "Float Input",
        -10.0f,
        10.0f,
        demoFloatInputValue,
        2,
        []() { return demoFloatInputValue; },
        [](float value) { demoFloatInputValue = value; },
        "°C")
        .order(11);
}

static void setupGuiSystem()
{
    auto systemCard = ConfigManager.liveGroup("system")
                         .page("System", 90)
                         .card("System", 90);

    systemCard.divider("Custom", 98);

    systemCard.value("testValue", []() { return roundf(mockedTemperatureC * 10.0f) / 10.0f; })
        .label("Injected Value")
        .unit("°C")
        .precision(1)
        .order(99);
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
