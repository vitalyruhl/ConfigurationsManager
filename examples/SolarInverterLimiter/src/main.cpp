#include <Arduino.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <Ticker.h>
#include <Wire.h>
#include <WiFi.h>
#include <time.h>
#include <BME280_I2C.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// AsyncWebServer instance is provided by ConfigManager library (extern AsyncWebServer server)

#include "ConfigManager.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "io/IOManager.h"
#include "alarm/AlarmManager.h"
#include "logging/LoggingManager.h"

#define CM_MQTT_NO_DEFAULT_HOOKS
#include "mqtt/MQTTManager.h"
#include "mqtt/MQTTLogOutput.h"

#include "RS485Module/RS485Module.h"
#include "helpers/HelperModule.h"
#include "Smoother/Smoother.h"

#if __has_include("secret/secrets.h")
#include "secret/secrets.h"
#define CM_HAS_WIFI_SECRETS 1
#else
#define CM_HAS_WIFI_SECRETS 0
#endif

#ifndef APMODE_SSID
#define APMODE_SSID "ESP32_Config"
#endif

#ifndef APMODE_PASSWORD
#define APMODE_PASSWORD "config1234"
#endif

#ifndef APP_NAME
#define APP_NAME "SolarInverterLimiter"
#endif

#ifndef SETTINGS_PASSWORD
#define SETTINGS_PASSWORD ""
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD SETTINGS_PASSWORD
#endif

#ifndef VERSION
#define VERSION "4.2.0"
#endif

#ifndef FEATURE_FAN_ENABLED
#define FEATURE_FAN_ENABLED 0
#endif

#ifndef FEATURE_HEATER_ENABLED
#define FEATURE_HEATER_ENABLED 0
#endif

#define FEATURE_ANY_RELAY_OUTPUTS (FEATURE_FAN_ENABLED || FEATURE_HEATER_ENABLED)

enum class NegativePriceSettingPreference : uint8_t
{
    None,
    ForceMin,
    SetZero
};

// predeclare the functions (prototypes)
void SetupStartDisplay();
void cb_RS485Listener();
void testRS232();
void readBme280();
void WriteToDisplay();
void SetupStartTemperatureMeasuring();
void ProjectConfig();
#if FEATURE_FAN_ENABLED
void CheckVentilator(float currentTemperature);
#endif
#if FEATURE_HEATER_ENABLED
void EvaluateHeater(float currentTemperature);
#endif
void ShowDisplayOn();
void ShowDisplayOff();
static void logNetworkIpInfo(const char *context);
void setupGUI();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
static void setupLogging();
static void setupMqtt();
static void setupNetworkDefaults();
static void attachSystemSettings();
static void syncStoredCodeVersion();
static void registerProjectSettings();
static void configureLimiterSettingsBehavior();
static void normalizeNegativePriceSettings(NegativePriceSettingPreference preferred, bool persist, bool logCorrection);
static void registerIOBindings();
static bool ensureNvsReady();
static const char *resetReasonToText(esp_reset_reason_t reason);
static bool isValidMacAddress(const String &value);
static void applyAccessPointMacPriority();
static void resetPidController();
static int computePidStabilizedTarget(int baseTarget, int configuredMin, int configuredMax);
static void updateMqttTopics();
static void publishMqttNow();
static void updateStatusLED();
#if FEATURE_FAN_ENABLED
static void setFanRelay(bool on);
static bool getFanRelay();
#endif
#if FEATURE_HEATER_ENABLED
static void setHeaterRelay(bool on);
static bool getHeaterRelay();
#endif
#if FEATURE_ANY_RELAY_OUTPUTS
static void setManualOverride(bool on);
#endif
//--------------------------------------------------------------------------------------------------------------

#pragma region configuration variables

// Project settings (ConfigOptions API)
struct LimiterSettings
{
    Config<bool> enableController{ConfigOptions<bool>{.key = "LimiterEnable", .name = "Limiter Enabled", .category = "Limiter", .defaultValue = true, .sortOrder = 1}};
    Config<int> maxOutput{ConfigOptions<int>{.key = "LimiterMaxW", .name = "Max Output (W)", .category = "Limiter", .defaultValue = 1100, .sortOrder = 2}};
    Config<int> minOutput{ConfigOptions<int>{.key = "LimiterMinW", .name = "Min Output (W)", .category = "Limiter", .defaultValue = 500, .sortOrder = 3}};
    Config<int> inputCorrectionOffset{ConfigOptions<int>{.key = "LimiterCorrW", .name = "Input Correction Offset (W)", .category = "Limiter", .defaultValue = 50, .sortOrder = 4}};
    Config<int> smoothingSize{ConfigOptions<int>{.key = "LimiterSmooth", .name = "Smoothing Level", .category = "Limiter", .defaultValue = 10, .sortOrder = 5}};
    Config<bool> usePidSmoothing{ConfigOptions<bool>{.key = "LimiterUsePID", .name = "Use PID Smoothing", .category = "Limiter", .defaultValue = false, .sortOrder = 6}};
    Config<float> pidKp{ConfigOptions<float>{.key = "LimiterPIDKp", .name = "PID Kp", .category = "Limiter", .defaultValue = 0.35f, .sortOrder = 7}};
    Config<float> pidKi{ConfigOptions<float>{.key = "LimiterPIDKi", .name = "PID Ki", .category = "Limiter", .defaultValue = 0.05f, .sortOrder = 8}};
    Config<float> pidKd{ConfigOptions<float>{.key = "LimiterPIDKd", .name = "PID Kd", .category = "Limiter", .defaultValue = 0.02f, .sortOrder = 9}};
    Config<bool> forceMinOnNegativePrice{ConfigOptions<bool>{.key = "LimiterNegPrice", .name = "Force Min On Negative Price", .category = "Limiter", .defaultValue = false, .sortOrder = 10}};
    Config<bool> setZeroOnNegativePrice{ConfigOptions<bool>{.key = "LimNegZero", .name = "Set 0 On Negative Price", .category = "Limiter", .defaultValue = false, .sortOrder = 11}};
    Config<float> RS232PublishPeriod{ConfigOptions<float>{.key = "LimiterRS485P", .name = "RS485 Publish Period (s)", .category = "Limiter", .defaultValue = 2.0f, .sortOrder = 12}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&enableController);
        cfg.addSetting(&maxOutput);
        cfg.addSetting(&minOutput);
        cfg.addSetting(&inputCorrectionOffset);
        cfg.addSetting(&smoothingSize);
        cfg.addSetting(&usePidSmoothing);
        cfg.addSetting(&pidKp);
        cfg.addSetting(&pidKi);
        cfg.addSetting(&pidKd);
        cfg.addSetting(&forceMinOnNegativePrice);
        cfg.addSetting(&setZeroOnNegativePrice);
        cfg.addSetting(&RS232PublishPeriod);
    }
};

struct TempSettings
{
    Config<float> tempCorrection{ConfigOptions<float>{.key = "TCO", .name = "Temperature Correction", .category = "Temp", .defaultValue = 0.1f, .sortOrder = 1}};
    Config<float> humidityCorrection{ConfigOptions<float>{.key = "HYO", .name = "Humidity Correction", .category = "Temp", .defaultValue = 0.1f, .sortOrder = 2}};
    Config<int> seaLevelPressure{ConfigOptions<int>{.key = "SLP", .name = "Sea Level Pressure (hPa)", .category = "Temp", .defaultValue = 1013, .sortOrder = 3}};
    Config<int> readIntervalSec{ConfigOptions<int>{.key = "ReadTemp", .name = "Read Temp/Humidity every (s)", .category = "Temp", .defaultValue = 30, .sortOrder = 4}};
    Config<float> dewpointRiskWindow{ConfigOptions<float>{.key = "DPWin", .name = "Dewpoint Risk Window (C)", .category = "Temp", .defaultValue = 1.5f, .sortOrder = 5}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&tempCorrection);
        cfg.addSetting(&humidityCorrection);
        cfg.addSetting(&seaLevelPressure);
        cfg.addSetting(&readIntervalSec);
        cfg.addSetting(&dewpointRiskWindow);
    }
};

struct I2CSettings
{
    Config<int> sdaPin{ConfigOptions<int>{.key = "I2CSDA", .name = "SDA Pin", .category = "I2C", .defaultValue = 21, .sortOrder = 1}};
    Config<int> sclPin{ConfigOptions<int>{.key = "I2CSCL", .name = "SCL Pin", .category = "I2C", .defaultValue = 22, .sortOrder = 2}};
    Config<int> busFreq{ConfigOptions<int>{.key = "I2CFreq", .name = "Bus Frequency (Hz)", .category = "I2C", .defaultValue = 400000, .sortOrder = 3}};
    Config<int> displayAddr{ConfigOptions<int>{.key = "I2CDisp", .name = "Display Address", .category = "I2C", .defaultValue = 0x3C, .sortOrder = 4}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&sdaPin);
        cfg.addSetting(&sclPin);
        cfg.addSetting(&busFreq);
        cfg.addSetting(&displayAddr);
    }
};

#if FEATURE_FAN_ENABLED
struct FanSettings
{
    Config<bool> enabled{ConfigOptions<bool>{.key = "FanEnable", .name = "Enable Fan Control", .category = "Fan", .defaultValue = false, .sortOrder = 1}};
    Config<float> onThreshold{ConfigOptions<float>{.key = "FanOn", .name = "Fan ON above (C)", .category = "Fan", .defaultValue = 30.0f, .sortOrder = 2}};
    Config<float> offThreshold{ConfigOptions<float>{.key = "FanOff", .name = "Fan OFF below (C)", .category = "Fan", .defaultValue = 27.0f, .sortOrder = 3}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&enabled);
        cfg.addSetting(&onThreshold);
        cfg.addSetting(&offThreshold);
    }
};
#endif

#if FEATURE_HEATER_ENABLED
struct HeaterSettings
{
    Config<bool> enabled{ConfigOptions<bool>{.key = "HeatEnable", .name = "Enable Heater Control", .category = "Heater", .defaultValue = false, .sortOrder = 1}};
    Config<float> onTemp{ConfigOptions<float>{.key = "HeatOn", .name = "Heater ON below (C)", .category = "Heater", .defaultValue = 0.0f, .sortOrder = 2}};
    Config<float> offTemp{ConfigOptions<float>{.key = "HeatOff", .name = "Heater OFF above (C)", .category = "Heater", .defaultValue = 0.5f, .sortOrder = 3}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&enabled);
        cfg.addSetting(&onTemp);
        cfg.addSetting(&offTemp);
    }
};
#endif

struct DisplaySettings
{
    Config<bool> turnDisplayOff{ConfigOptions<bool>{.key = "DispSleep", .name = "Turn Display Off", .category = "Display", .defaultValue = true, .sortOrder = 1}};
    Config<int> onTimeSec{ConfigOptions<int>{.key = "DispOnS", .name = "On Time (s)", .category = "Display", .defaultValue = 60, .sortOrder = 2}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&turnDisplayOff);
        cfg.addSetting(&onTimeSec);
    }
};

struct WiFiRoamingSettings
{
    Config<String> preferredApMac{ConfigOptions<String>{.key = "WiFiMacPrio", .name = "Preferred AP MAC", .category = "WiFi", .defaultValue = "", .sortOrder = 90}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&preferredApMac);
    }
};

BME280_I2C bme280;

static constexpr int OLED_WIDTH = 128;
static constexpr int OLED_HEIGHT = 32;
static constexpr int OLED_RESET_PIN = 4; // keep legacy wiring default
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);

Ticker RS485Ticker;
Ticker temperatureTicker;
Ticker displayTicker;

Smoother *powerSmoother = nullptr; // there is a memory allocation in setup, better use a pointer here

// global helper variables
int currentGridImportW = 0; // signed grid power: positive import, negative export
int inverterCalculatedValue = 0; // calculated controller output before final send decision
int inverterSetValue = 0;        // value sent to RS485 (with offset and min-max, or max when limiter off)
int solarPowerW = 0;        // current solar production
bool negativePriceActive = false; // MQTT input: true forces minimum output when enabled
float electricityPriceEurKwh = 0.0f; // optional MQTT input for status/logging
float temperature = 0.0;    // current temperature in Celsius
float Dewpoint = 0.0;       // current dewpoint in Celsius
float Humidity = 0.0;       // current humidity in percent
float Pressure = 0.0;       // current pressure in hPa

bool displayActive = true; // flag to indicate if the display is active
static bool displayInitialized = false;
static bool bme280Initialized = false;
#if FEATURE_HEATER_ENABLED
static bool dewpointRiskActive = false;   // tracks dewpoint alarm state
static bool heaterLatchedState = false;   // hysteresis latch for heater
#endif
#if FEATURE_ANY_RELAY_OUTPUTS
static bool manualOverrideActive = false; // when enabled, buttons control relays and automation pauses
#endif

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.rw[data-group="sensors"][data-key="temp"]{ color:rgb(198, 16, 16) !important; font-weight:900; font-size: 1.2rem; }
.rw[data-group="sensors"][data-key="temp"] *{ color:rgb(198, 16, 16) !important; font-weight:900; font-size: 1.2rem; }
)CSS";

// Built-in LED pulse helper for WiFi status patterns.
static cm::helpers::PulseOutput buildinLED(LED_BUILTIN, cm::helpers::PulseOutput::ActiveLevel::ActiveHigh);

static cm::LoggingManager &lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level;
static cm::MQTTManager &mqtt = cm::MQTTManager::instance();
static cm::MQTTManager::Settings &mqttSettings = mqtt.settings();
static cm::IOManager ioManager;
static cm::AlarmManager alarmManager;

LimiterSettings limiterSettings;
TempSettings tempSettings;
I2CSettings i2cSettings;
#if FEATURE_FAN_ENABLED
FanSettings fanSettings;
#endif
#if FEATURE_HEATER_ENABLED
HeaterSettings heaterSettings;
#endif
DisplaySettings displaySettings;
WiFiRoamingSettings wifiRoamingSettings;
RS485_Settings rs485settings;

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

#if FEATURE_FAN_ENABLED
static constexpr char IO_FAN_ID[] = "fan_relay";
#endif
#if FEATURE_HEATER_ENABLED
static constexpr char IO_HEATER_ID[] = "heater_relay";
#endif
static constexpr char IO_RESET_ID[] = "reset_btn";
static constexpr char IO_AP_ID[] = "ap_btn";

static String mqttBaseTopic;
static String topicPublishSetValueW;
static String topicPublishCalculatedValueW;
static String topicPublishGridImportW;
static String topicPublishTempC;
static String topicPublishHumidityPct;
static String topicPublishDewpointC;

struct PidControllerState
{
    bool initialized = false;
    float output = 0.0f;
    float integral = 0.0f;
    float previousError = 0.0f;
    unsigned long lastUpdateMs = 0;
};

static PidControllerState pidController;
static bool lastLimiterModeWasPid = false;
static int lastNegativePriceOverrideTarget = -1;
static NegativePriceSettingPreference lastNegativePriceSettingPreference = NegativePriceSettingPreference::None;
static bool normalizingNegativePriceSettings = false;

#pragma endregion configurationn variables

//----------------------------------------
// MAIN FUNCTIONS
//----------------------------------------

void setup()
{
    Serial.begin(115200);
    ensureNvsReady();

    setupLogging();
    lmg.scopedTag("SETUP");
    lmg.logTag(LL::Info, "SETUP", "Reset reason: %s (%d)", resetReasonToText(esp_reset_reason()), static_cast<int>(esp_reset_reason()));
    lmg.log("System setup start...");

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(VERSION);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);
    ConfigManager.enableBuiltinSystemProvider();

    coreSettings.attachWiFi(ConfigManager);
    attachSystemSettings();
    coreSettings.attachNtp(ConfigManager);

    registerProjectSettings();
    configureLimiterSettingsBehavior();
    registerIOBindings();
    setupMqtt();

    ConfigManager.checkSettingsForErrors();
    ConfigManager.loadAll();
    normalizeNegativePriceSettings(NegativePriceSettingPreference::None, true, true);
    syncStoredCodeVersion();
    delay(100);
    setupNetworkDefaults();
    ioManager.begin();

    ConfigManager.startWebServer();

    ConfigManager.enableSmartRoaming(true);
    ConfigManager.setRoamingThreshold(-75);
    ConfigManager.setRoamingCooldown(30);
    ConfigManager.setRoamingImprovement(10);
    applyAccessPointMacPriority();

    updateMqttTopics();
    setupGUI();
    SetupStartDisplay();
    ShowDisplayOn();

    cm::helpers::pulseWait(LED_BUILTIN, cm::helpers::PulseOutput::ActiveLevel::ActiveHigh, 3, 100);

    powerSmoother = new Smoother(limiterSettings.smoothingSize.get());
    powerSmoother->fillBufferOnStart(currentGridImportW);

    RS485begin();
    SetupStartTemperatureMeasuring();

    RS485Ticker.attach(limiterSettings.RS232PublishPeriod.get(), cb_RS485Listener);

#if FEATURE_FAN_ENABLED
    setFanRelay(false);
#endif
#if FEATURE_HEATER_ENABLED
    setHeaterRelay(false);
#endif

    lmg.logTag(LL::Info, "SETUP", "Completed successfully. Starting main loop...");
}

static bool ensureNvsReady()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK)
    {
        return true;
    }

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND || err == ESP_ERR_NVS_INVALID_STATE)
    {
        const esp_err_t eraseErr = nvs_flash_erase();
        if (eraseErr != ESP_OK)
        {
            Serial.printf("[E] NVS erase failed (%d)\n", static_cast<int>(eraseErr));
            return false;
        }

        err = nvs_flash_init();
        if (err == ESP_OK)
        {
            return true;
        }
    }

    Serial.printf("[E] NVS init failed (%d)\n", static_cast<int>(err));
    return false;
}

void loop()
{
    ConfigManager.getWiFiManager().update();
    mqtt.loop();
    publishMqttNow();
    lmg.loop();
    ioManager.update();

    // Services managed by ConfigManager.
    ConfigManager.handleClient();
    alarmManager.update();

    updateStatusLED();
    cm::helpers::PulseOutput::loopAll();

    WriteToDisplay();

#if FEATURE_ANY_RELAY_OUTPUTS
    if (!manualOverrideActive)
    {
#if FEATURE_FAN_ENABLED
        CheckVentilator(temperature);
#endif
#if FEATURE_HEATER_ENABLED
        EvaluateHeater(temperature);
#endif
    }
#endif
    delay(10);
}

void setupGUI()
{
    // Settings layout
    // coreSettings owns the WiFi/System/NTP pages now; MQTT module registers its own layout.
    ConfigManager.addSettingsPage("Limiter", 60);
    ConfigManager.addSettingsGroup("Limiter", "Limiter", "Limiter Settings", 60);
    ConfigManager.addSettingsPage("Temp", 70);
    ConfigManager.addSettingsGroup("Temp", "Temp", "Temp Settings", 70);
    ConfigManager.addSettingsPage("I2C", 80);
    ConfigManager.addSettingsGroup("I2C", "I2C", "I2C Settings", 80);
#if FEATURE_FAN_ENABLED
    ConfigManager.addSettingsPage("Fan", 90);
    ConfigManager.addSettingsGroup("Fan", "Fan", "Fan Settings", 90);
#endif
#if FEATURE_HEATER_ENABLED
    ConfigManager.addSettingsPage("Heater", 100);
    ConfigManager.addSettingsGroup("Heater", "Heater", "Heater Settings", 100);
#endif
    ConfigManager.addSettingsPage("Display", 110);
    ConfigManager.addSettingsGroup("Display", "Display", "Display Settings", 110);
    ConfigManager.addSettingsPage("RS485", 120);
    ConfigManager.addSettingsGroup("RS485", "RS485", "RS485 Settings", 120);
    ConfigManager.addSettingsPage("I/O", 130);
    ConfigManager.addSettingsGroup("I/O", "I/O", "I/O Settings", 130);

    // region sensor fields BME280
    auto sensors = ConfigManager.liveGroup("sensors")
                       .page("Limiter", 10)
                       .card("Sensors", 10)
                       .group("Sensor Readings", 10);

    sensors.value("temp", []()
                  { return roundf(temperature * 10.0f) / 10.0f; })
        .label("Temperature")
        .unit("°C")
        .precision(1)
        .order(2);

    sensors.value("hum", []()
                  { return roundf(Humidity * 10.0f) / 10.0f; })
        .label("Humidity")
        .unit("%")
        .precision(1)
        .order(11);

    sensors.value("dew", []()
                  { return roundf(Dewpoint * 10.0f) / 10.0f; })
        .label("Dewpoint")
        .unit("°C")
        .precision(1)
        .order(12);

    sensors.value("pressure", []()
                  { return roundf(Pressure * 10.0f) / 10.0f; })
        .label("Pressure")
        .unit("hPa")
        .precision(1)
        .order(13);
    // endregion sensor fields

    // region Limiter
    auto limiter = ConfigManager.liveGroup("Limiter")
                       .page("Limiter", 20)
                       .card("Limiter", 20)
                       .group("Limiter Status", 20);

    limiter.value("enabled", []()
                  { return limiterSettings.enableController.get(); })
        .label("Limiter Enabled")
        .order(1);

    limiter.value("gridIn", []()
                  { return currentGridImportW; })
        .label("Grid Import")
        .unit("W")
        .precision(0)
        .order(2);

    limiter.value("invSet", []()
                  { return inverterSetValue; })
        .label("Inverter Setpoint")
        .unit("W")
        .precision(0)
        .order(3);

    limiter.value("invCalc", []()
                  { return inverterCalculatedValue; })
        .label("Calculated Setpoint")
        .unit("W")
        .precision(0)
        .order(4);

    limiter.value("solar", []()
                  { return solarPowerW; })
        .label("Solar power")
        .unit("W")
        .precision(0)
        .order(5);

    limiter.value("negPrice", []()
                  { return negativePriceActive; })
        .label("Negative Price")
        .order(6);

    limiter.value("price", []()
                  { return electricityPriceEurKwh; })
        .label("Electricity Price")
        .unit("EUR/kWh")
        .precision(3)
        .order(7);
    // endregion Limiter

    // region relay outputs
#if FEATURE_ANY_RELAY_OUTPUTS
    auto outputs = ConfigManager.liveGroup("Outputs")
                       .page("Limiter", 30)
                       .card("Outputs", 30)
                       .group("Relay Status", 30);

    outputs.checkbox(
               "manual_override",
               "Manual Override",
               []()
               { return manualOverrideActive; },
               [](bool on)
               { setManualOverride(on); })
        .order(0);

#if FEATURE_FAN_ENABLED
    outputs.stateButton(
               "ventilator",
               "Ventilator Relay Active",
               []()
               { return getFanRelay(); },
               [](bool on)
               { setFanRelay(on); },
               getFanRelay(),
               "On",
               "Off")
        .order(1);
#endif

#if FEATURE_HEATER_ENABLED
    outputs.stateButton(
               "heater",
               "Heater Relay Active",
               []()
               { return getHeaterRelay(); },
               [](bool on)
               { setHeaterRelay(on); },
               getHeaterRelay(),
               "On",
               "Off")
        .order(2);

    outputs.boolValue("dewpoint_risk", []()
                      { return dewpointRiskActive; })
        .label("Dewpoint Risk")
        .order(3);

    alarmManager.addDigitalWarning(
                    {
                        .id = "dewpoint_risk",
                        .name = "Dewpoint Risk",
                        .kind = cm::AlarmKind::DigitalActive,
                        .severity = cm::AlarmSeverity::Warning,
                        .enabled = true,
                        .getter = []()
                        { return (temperature - Dewpoint) <= tempSettings.dewpointRiskWindow.get(); },
                    })
        .onAlarmCome([]()
                     {
              dewpointRiskActive = true;
              lmg.logTag(LL::Warn, "ALARM", "Dewpoint risk ENTER");
              EvaluateHeater(temperature); })
        .onAlarmGone([]()
                     {
              dewpointRiskActive = false;
              lmg.logTag(LL::Info, "ALARM", "Dewpoint risk EXIT");
              EvaluateHeater(temperature); });
#endif
#endif
    // endregion relay outputs
}

//----------------------------------------
// LOGGING / IO / MQTT SETUP
//----------------------------------------

static void logNetworkIpInfo(const char *context)
{
    const WiFiMode_t mode = WiFi.getMode();
    const bool apActive = (mode == WIFI_AP) || (mode == WIFI_AP_STA);
    const bool staConnected = (WiFi.status() == WL_CONNECTED);

    if (apActive)
    {
        const IPAddress apIp = WiFi.softAPIP();
        lmg.logTag(LL::Debug, "WiFi", "%s: AP IP: %s", context, apIp.toString().c_str());
        lmg.logTag(LL::Debug, "WiFi", "%s: AP SSID: %s", context, WiFi.softAPSSID().c_str());
    }

    if (staConnected)
    {
        const IPAddress staIp = WiFi.localIP();
        lmg.logTag(LL::Debug, "WiFi", "%s: STA IP: %s", context, staIp.toString().c_str());
    }
}

static void setupLogging()
{
    Serial.begin(115200);

    auto serialOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
    serialOut->setLevel(LL::Info);
    serialOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
    serialOut->setRateLimitMs(2);
    lmg.addOutput(std::move(serialOut));

    lmg.setGlobalLevel(LL::Info);
    lmg.attachToConfigManager(LL::Info, LL::Info, "");

    auto guiOut = std::make_unique<cm::LoggingManager::GuiOutput>(ConfigManager, 50);
    guiOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
    guiOut->setLevel(LL::Info);
    lmg.addOutput(std::move(guiOut));
}

static void registerIOBindings()
{
    lmg.scopedTag("IO");
    analogReadResolution(12);

#if FEATURE_FAN_ENABLED
    ioManager.addDigitalOutput(IO_FAN_ID, "Cooling Fan Relay", 23, true, true);
    ioManager.addDigitalOutputToSettingsGroup(IO_FAN_ID, "I/O", "Cooling Fan Relay", "Cooling Fan Relay", 1);
#endif

#if FEATURE_HEATER_ENABLED
    ioManager.addDigitalOutput(IO_HEATER_ID, "Heater Relay", 27, true, true);
    ioManager.addDigitalOutputToSettingsGroup(IO_HEATER_ID, "I/O", "Heater Relay", "Heater Relay", 2);
#endif

    ioManager.addDigitalInput(IO_RESET_ID, "Reset Button", 14, true, true, false, true);
    ioManager.addDigitalInputToSettingsGroup(IO_RESET_ID, "I/O", "Reset Button", "Reset Button", 10);

    ioManager.addDigitalInput(IO_AP_ID, "AP Mode Button", 13, true, true, false, true);
    ioManager.addDigitalInputToSettingsGroup(IO_AP_ID, "I/O", "AP Mode Button", "AP Mode Button", 11);

    cm::IOManager::DigitalInputEventOptions resetOptions;
    resetOptions.longClickMs = 3000;
    ioManager.configureDigitalInputEvents(
        IO_RESET_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []()
            {
                lmg.logTag(LL::Debug, "IO", "Reset button pressed -> show display");
                ShowDisplayOn(); },
            .onLongPressOnStartup = []()
            {
                lmg.logTag(LL::Warn, "IO", "Reset button pressed at startup -> restoring defaults");
                ConfigManager.clearAllFromPrefs();
                ConfigManager.saveAll();
                delay(500);
                ESP.restart(); },
        },
        resetOptions);

    cm::IOManager::DigitalInputEventOptions apOptions;
    apOptions.longClickMs = 1200;
    ioManager.configureDigitalInputEvents(
        IO_AP_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []()
            {
                lmg.logTag(LL::Debug, "IO", "AP button pressed -> show display");
                ShowDisplayOn(); },
            .onLongPressOnStartup = []()
            {
                lmg.logTag(LL::Warn, "IO", "AP button pressed at startup -> starting AP mode");
                ConfigManager.startAccessPoint(APMODE_SSID, APMODE_PASSWORD); },
        },
        apOptions);
}

static const char *resetReasonToText(esp_reset_reason_t reason)
{
    switch (reason)
    {
    case ESP_RST_UNKNOWN:
        return "UNKNOWN";
    case ESP_RST_POWERON:
        return "POWERON";
    case ESP_RST_EXT:
        return "EXT";
    case ESP_RST_SW:
        return "SW";
    case ESP_RST_PANIC:
        return "PANIC";
    case ESP_RST_INT_WDT:
        return "INT_WDT";
    case ESP_RST_TASK_WDT:
        return "TASK_WDT";
    case ESP_RST_WDT:
        return "WDT";
    case ESP_RST_DEEPSLEEP:
        return "DEEPSLEEP";
    case ESP_RST_BROWNOUT:
        return "BROWNOUT";
    case ESP_RST_SDIO:
        return "SDIO";
    default:
        return "OTHER";
    }
}

static void setupMqtt()
{
    // Tasmota SENSOR payloads can exceed PubSubClient default packet size (~256B).
    // Increase buffer to avoid silently dropped inbound messages.
    mqtt.setBufferSize(1024);

    mqtt.attach(ConfigManager);
    mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt");
    mqtt.addMqttSettingsToSettingsGroup(ConfigManager, "MQTT", "MQTT Settings", 40);

    // Receive: signed grid power W (positive import, negative export)
    mqtt.addTopicReceiveInt(
        "grid_import_w",
        "Grid Power",
        "tele/powerMeter/powerMeter/SENSOR",
        &currentGridImportW,
        "W",
        "E320.Power_in");

    mqtt.addTopicReceiveInt(
        "solar_power_w",
        "Solar power",
        "tele/tasmota_1DEE45/SENSOR",
        &solarPowerW,
        "W",
        "ENERGY.Power");

    mqtt.addTopicReceiveBool(
        "negative_price",
        "Negative Price",
        "SolarLimiter/Input/NegativePrice",
        &negativePriceActive,
        "none");

    mqtt.addTopicReceiveFloat(
        "electricity_price",
        "Electricity Price",
        "SolarLimiter/Input/ElectricityPrice",
        &electricityPriceEurKwh,
        "EUR/kWh",
        3,
        "none");

    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "grid_import_w", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 50);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "solar_power_w", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 51);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "negative_price", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 52);
    mqtt.addMqttTopicToSettingsGroup(ConfigManager, "electricity_price", "MQTT-Topics", "MQTT-Topics", "MQTT-Received", 53);

    // Optional: show receive topics in runtime UI
    // mqtt.addMqttTopicToLiveGroup(ConfigManager, "grid_import_w", "mqtt", "MQTT-Received", "MQTT-Received", 1);

    static bool mqttLogAdded = false;
    if (!mqttLogAdded)
    {
        auto mqttLog = std::make_unique<cm::MQTTLogOutput>(mqtt);
        mqttLog->setLevel(LL::Info);
        mqttLog->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
        lmg.addOutput(std::move(mqttLog));
        mqttLogAdded = true;
    }
}

static void registerProjectSettings()
{
    limiterSettings.attachTo(ConfigManager);
    tempSettings.attachTo(ConfigManager);
    i2cSettings.attachTo(ConfigManager);
#if FEATURE_FAN_ENABLED
    fanSettings.attachTo(ConfigManager);
#endif
#if FEATURE_HEATER_ENABLED
    heaterSettings.attachTo(ConfigManager);
#endif
    displaySettings.attachTo(ConfigManager);
    wifiRoamingSettings.attachTo(ConfigManager);
    rs485settings.attachTo(ConfigManager);
}

static void configureLimiterSettingsBehavior()
{
    limiterSettings.smoothingSize.showIfFunc = []()
    { return !limiterSettings.usePidSmoothing.get(); };

    limiterSettings.forceMinOnNegativePrice.setCallback([](bool enabled)
                                                        {
        if (normalizingNegativePriceSettings || !enabled)
        {
            return;
        }
        lastNegativePriceSettingPreference = NegativePriceSettingPreference::ForceMin;
        normalizeNegativePriceSettings(NegativePriceSettingPreference::ForceMin, true, true); });

    limiterSettings.setZeroOnNegativePrice.setCallback([](bool enabled)
                                                       {
        if (normalizingNegativePriceSettings || !enabled)
        {
            return;
        }
        lastNegativePriceSettingPreference = NegativePriceSettingPreference::SetZero;
        normalizeNegativePriceSettings(NegativePriceSettingPreference::SetZero, true, true); });
}

static void normalizeNegativePriceSettings(NegativePriceSettingPreference preferred, bool persist, bool logCorrection)
{
    if (!limiterSettings.forceMinOnNegativePrice.get() || !limiterSettings.setZeroOnNegativePrice.get())
    {
        return;
    }

    NegativePriceSettingPreference winner = preferred;
    if (winner == NegativePriceSettingPreference::None)
    {
        winner = lastNegativePriceSettingPreference;
    }
    if (winner == NegativePriceSettingPreference::None)
    {
        winner = NegativePriceSettingPreference::ForceMin;
    }

    normalizingNegativePriceSettings = true;
    if (winner == NegativePriceSettingPreference::SetZero)
    {
        limiterSettings.forceMinOnNegativePrice.set(false);
    }
    else
    {
        winner = NegativePriceSettingPreference::ForceMin;
        limiterSettings.setZeroOnNegativePrice.set(false);
    }
    normalizingNegativePriceSettings = false;
    lastNegativePriceSettingPreference = winner;

    if (logCorrection)
    {
        lmg.logTag(LL::Warn, "PRICE", "Corrected conflicting negative-price settings -> %s",
                   winner == NegativePriceSettingPreference::SetZero ? "zero" : "min");
    }
    if (persist)
    {
        ConfigManager.saveAll();
    }
}

static bool hasValidStationCredentials(const String &ssid, const String &password)
{
    if (ssid.isEmpty())
    {
        return false;
    }
    if (ssid.length() > 32)
    {
        return false;
    }
    if (password.length() > 63)
    {
        return false;
    }
    return true;
}

static void setupNetworkDefaults()
{
    const String currentSsid = wifiSettings.wifiSsid.get();
    const String currentPassword = wifiSettings.wifiPassword.get();
    const bool hasValidStoredWiFi = hasValidStationCredentials(currentSsid, currentPassword);

    if (!currentSsid.isEmpty() && currentSsid.length() > 32)
    {
        lmg.logTag(LL::Warn, "SETUP", "Stored WiFi SSID length invalid (%u > 32)", static_cast<unsigned>(currentSsid.length()));
    }
    if (currentPassword.length() > 63)
    {
        lmg.logTag(LL::Warn, "SETUP", "Stored WiFi password length invalid (%u > 63)", static_cast<unsigned>(currentPassword.length()));
    }

    // Apply secret defaults only if nothing is configured yet (after loading persisted settings).
    if (!hasValidStoredWiFi)
    {
#if CM_HAS_WIFI_SECRETS
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "SETUP: *** WiFi credentials missing/invalid, setting My values *** ");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
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
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Info, "Applied WiFi defaults from secrets");
        lmg.log(LL::Info, "Skipping forced reboot to avoid reset loops");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
#else
        lmg.log(LL::Warn, "SETUP: WiFi credentials missing/invalid but secret/secrets.h is missing; using UI/AP mode");
#endif
    }

    mqtt.attach(ConfigManager); // Re-attach to apply loaded values (attach() is idempotent)
    if (mqttSettings.server.get().isEmpty())
    {
#if CM_HAS_WIFI_SECRETS
#if defined(MY_MQTT_BROKER_IP) && defined(MY_MQTT_BROKER_PORT) && defined(MY_MQTT_ROOT)
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "SETUP: *** MQTT Broker is empty, setting My values *** ");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        mqttSettings.server.set(MY_MQTT_BROKER_IP);
        mqttSettings.port.set(MY_MQTT_BROKER_PORT);
#ifdef MY_MQTT_USERNAME
        mqttSettings.username.set(MY_MQTT_USERNAME);
#endif
#ifdef MY_MQTT_PASSWORD
        mqttSettings.password.set(MY_MQTT_PASSWORD);
#endif
        mqttSettings.publishTopicBase.set(MY_MQTT_ROOT);
        ConfigManager.saveAll();
        lmg.log(LL::Debug, "-------------------------------------------------------------");
#else
        lmg.log(LL::Info, "SETUP: MQTT server is empty; secret/secrets.h does not provide MQTT defaults for this example");
#endif
#else
        lmg.log(LL::Info, "SETUP: MQTT server is empty and secret/secrets.h is missing; leaving MQTT unconfigured");
#endif
    }

    if (systemSettings.otaPassword.get() != OTA_PASSWORD)
    {
        systemSettings.otaPassword.save(OTA_PASSWORD);
    }

#if defined(WIFI_FILTER_MAC_PRIORITY)
    if (wifiRoamingSettings.preferredApMac.get().isEmpty())
    {
        wifiRoamingSettings.preferredApMac.save(WIFI_FILTER_MAC_PRIORITY);
    }
#endif
}

static void attachSystemSettings()
{
    ConfigManager.setCategoryLayoutOverride(cm::CoreCategories::System, cm::CoreCategories::System, cm::CoreCategories::System, "System Settings", 20);
    ConfigManager.addSettingsPage(cm::CoreCategories::System, 20);
    ConfigManager.addSettingsGroup(cm::CoreCategories::System, cm::CoreCategories::System, "System Settings", 20);
    ConfigManager.addSetting(&systemSettings.allowOTA);
    ConfigManager.addSetting(&systemSettings.otaPassword);
}

static void syncStoredCodeVersion()
{
    ConfigManager.setVersion(VERSION);

    Preferences prefs;
    if (!prefs.begin("ConfigManager", false))
    {
        lmg.logTag(LL::Warn, "SETUP", "Version sync skipped: preferences unavailable");
        return;
    }

    const char *versionKey = systemSettings.version.getKey();
    const String codeVersion(VERSION);
    const String storedVersion = prefs.getString(versionKey, "");
    if (storedVersion != codeVersion)
    {
        const size_t bytesWritten = prefs.putString(versionKey, codeVersion);
        if (bytesWritten > 0)
        {
            lmg.logTag(LL::Info, "SETUP", "Stored version synced to %s", VERSION);
        }
        else
        {
            lmg.logTag(LL::Warn, "SETUP", "Stored version sync failed");
        }
    }
    prefs.end();
}

static bool isValidMacAddress(const String &value)
{
    if (value.length() != 17)
    {
        return false;
    }

    for (int i = 0; i < value.length(); ++i)
    {
        const char c = value.charAt(i);
        if ((i + 1) % 3 == 0)
        {
            if (c != ':')
            {
                return false;
            }
            continue;
        }

        const bool isDigit = (c >= '0' && c <= '9');
        const bool isUpperHex = (c >= 'A' && c <= 'F');
        const bool isLowerHex = (c >= 'a' && c <= 'f');
        if (!isDigit && !isUpperHex && !isLowerHex)
        {
            return false;
        }
    }

    return true;
}

static void applyAccessPointMacPriority()
{
    String preferredMac = wifiRoamingSettings.preferredApMac.get();
    preferredMac.trim();

#if defined(WIFI_FILTER_MAC_PRIORITY)
    if (preferredMac.isEmpty())
    {
        preferredMac = WIFI_FILTER_MAC_PRIORITY;
    }
#endif

    if (preferredMac.isEmpty())
    {
        lmg.logTag(LL::Debug, "WiFi", "AP MAC priority not configured");
        return;
    }

    if (!isValidMacAddress(preferredMac))
    {
        lmg.logTag(LL::Warn, "WiFi", "AP MAC priority invalid: %s", preferredMac.c_str());
        return;
    }

    ConfigManager.setAccessPointMacPriority(preferredMac.c_str());
    lmg.logTag(LL::Info, "WiFi", "AP MAC priority set: %s", preferredMac.c_str());
}

static void updateMqttTopics()
{
    String base = mqtt.settings().publishTopicBase.get();
    if (base.isEmpty())
    {
        base = mqtt.getMqttBaseTopic();
    }
    if (base.isEmpty())
    {
        const String cid = mqtt.settings().clientId.get();
        if (!cid.isEmpty())
        {
            base = cid;
        }
    }
    if (base.isEmpty())
    {
        const char *hn = WiFi.getHostname();
        if (hn && hn[0])
        {
            base = hn;
        }
    }
    if (base.isEmpty())
    {
        base = APP_NAME;
    }

    mqttBaseTopic = base;
    topicPublishSetValueW = mqttBaseTopic + "/SetValue";
    topicPublishCalculatedValueW = mqttBaseTopic + "/CalculatedValue";
    topicPublishGridImportW = mqttBaseTopic + "/GetValue";
    topicPublishTempC = mqttBaseTopic + "/Temperature";
    topicPublishHumidityPct = mqttBaseTopic + "/Humidity";
    topicPublishDewpointC = mqttBaseTopic + "/Dewpoint";
}

static void resetPidController()
{
    pidController.initialized = false;
    pidController.output = 0.0f;
    pidController.integral = 0.0f;
    pidController.previousError = 0.0f;
    pidController.lastUpdateMs = 0;
}

static int computePidStabilizedTarget(int baseTarget, int configuredMin, int configuredMax)
{
    const unsigned long nowMs = millis();
    float dtSeconds = limiterSettings.RS232PublishPeriod.get();

    if (pidController.initialized)
    {
        const unsigned long elapsedMs = nowMs - pidController.lastUpdateMs;
        if (elapsedMs > 0)
        {
            dtSeconds = static_cast<float>(elapsedMs) / 1000.0f;
        }
    }

    dtSeconds = constrain(dtSeconds, 0.05f, 10.0f);

    if (!pidController.initialized)
    {
        pidController.initialized = true;
        pidController.output = constrain(static_cast<float>(baseTarget), static_cast<float>(configuredMin), static_cast<float>(configuredMax));
        pidController.integral = 0.0f;
        pidController.previousError = 0.0f;
    }

    // PID setpoint is the desired inverter target. PID input is the last commanded target.
    const float error = static_cast<float>(baseTarget) - pidController.output;
    const float candidateIntegral = constrain(pidController.integral + error * dtSeconds, -5000.0f, 5000.0f);
    const float derivative = (error - pidController.previousError) / dtSeconds;
    const float delta = limiterSettings.pidKp.get() * error +
                        limiterSettings.pidKi.get() * candidateIntegral +
                        limiterSettings.pidKd.get() * derivative;

    const float candidateOutput = pidController.output + delta;
    const float clampedOutput = constrain(candidateOutput, static_cast<float>(configuredMin), static_cast<float>(configuredMax));

    if (candidateOutput == clampedOutput ||
        (candidateOutput > static_cast<float>(configuredMax) && error < 0.0f) ||
        (candidateOutput < static_cast<float>(configuredMin) && error > 0.0f))
    {
        pidController.integral = candidateIntegral;
    }

    pidController.output = clampedOutput;

    pidController.previousError = error;
    pidController.lastUpdateMs = nowMs;

    return static_cast<int>(roundf(pidController.output));
}

#if FEATURE_FAN_ENABLED
static void setFanRelay(bool on)
{
    if (!fanSettings.enabled.get())
    {
        on = false;
    }
    ioManager.setState(IO_FAN_ID, on);
}

static bool getFanRelay()
{
    return ioManager.getState(IO_FAN_ID);
}
#endif

#if FEATURE_HEATER_ENABLED
static void setHeaterRelay(bool on)
{
    if (!heaterSettings.enabled.get()
#if FEATURE_ANY_RELAY_OUTPUTS
        && !manualOverrideActive
#endif
    )
    {
        on = false;
    }
    ioManager.setState(IO_HEATER_ID, on);
}

static bool getHeaterRelay()
{
    return ioManager.getState(IO_HEATER_ID);
}
#endif

#if FEATURE_ANY_RELAY_OUTPUTS
static void setManualOverride(bool on)
{
    manualOverrideActive = on;
    if (!manualOverrideActive)
    {
#if FEATURE_FAN_ENABLED
        CheckVentilator(temperature);
#endif
#if FEATURE_HEATER_ENABLED
        EvaluateHeater(temperature);
#endif
    }
}
#endif

//----------------------------------------
// MQTT FUNCTIONS
//----------------------------------------

static void publishMqttNow()
{
    if (!mqtt.isConnected())
    {
        return;
    }

    updateMqttTopics();

    mqtt.publishExtraTopic("setvalue_w", topicPublishSetValueW.c_str(), String(inverterSetValue), false);
    mqtt.publishExtraTopic("calculated_w", topicPublishCalculatedValueW.c_str(), String(inverterCalculatedValue), false);
    mqtt.publishExtraTopic("grid_import_w", topicPublishGridImportW.c_str(), String(currentGridImportW), false);
    mqtt.publishExtraTopic("temperature_c", topicPublishTempC.c_str(), String(temperature), false);
    mqtt.publishExtraTopic("humidity_pct", topicPublishHumidityPct.c_str(), String(Humidity), false);
    mqtt.publishExtraTopic("dewpoint_c", topicPublishDewpointC.c_str(), String(Dewpoint), false);
}

static void updateStatusLED()
{
    static int lastMode = -1; // 1=AP, 2=CONNECTED, 3=CONNECTING

    const bool connected = ConfigManager.getWiFiManager().isConnected();
    const bool apMode = ConfigManager.getWiFiManager().isInAPMode();

    const int mode = apMode ? 1 : (connected ? 2 : 3);
    if (mode == lastMode)
    {
        return;
    }
    lastMode = mode;

    switch (mode)
    {
    case 1: // AP mode: fast blink
        buildinLED.setPulseRepeat(/*count*/ 1, /*periodMs*/ 200, /*gapMs*/ 0);
        break;
    case 2: // Connected: heartbeat
        buildinLED.setPulseRepeat(/*count*/ 1, /*periodMs*/ 100, /*gapMs*/ 1500);
        break;
    case 3: // Connecting/disconnected: double blink
        buildinLED.setPulseRepeat(/*count*/ 3, /*periodMs*/ 200, /*gapMs*/ 600);
        break;
    }
}

//----------------------------------------
// HELPER FUNCTIONS
//----------------------------------------

void cb_RS485Listener()
{
    int configuredMin = limiterSettings.minOutput.get();
    int configuredMax = limiterSettings.maxOutput.get();
    if (configuredMin > configuredMax)
    {
        const int tmp = configuredMin;
        configuredMin = configuredMax;
        configuredMax = tmp;
    }

    if (powerSmoother != nullptr)
    {
        powerSmoother->setBufferSize(limiterSettings.smoothingSize.get());
    }

    const bool usePidSmoothing = limiterSettings.usePidSmoothing.get();
    if (usePidSmoothing != lastLimiterModeWasPid)
    {
        resetPidController();
        if (powerSmoother != nullptr)
        {
            powerSmoother->fillBufferOnStart(currentGridImportW);
        }
        lastLimiterModeWasPid = usePidSmoothing;
    }

    const int offset = limiterSettings.inputCorrectionOffset.get();
    const int currentInverterOutputW = solarPowerW;
    const int signedGridPowerW = currentGridImportW;
    const int gridImportW = max(signedGridPowerW, 0);
    const int gridExportW = max(-signedGridPowerW, 0);
    const bool negativePriceForceMinEnabled = limiterSettings.forceMinOnNegativePrice.get();
    const bool negativePriceSetZeroEnabled = !negativePriceForceMinEnabled && limiterSettings.setZeroOnNegativePrice.get();
    const bool negativePriceMinActive = negativePriceActive && negativePriceForceMinEnabled;
    const bool negativePriceZeroActive = negativePriceActive && negativePriceSetZeroEnabled;
    const int negativePriceOverrideTarget = negativePriceZeroActive ? 0 : (negativePriceMinActive ? configuredMin : -1);
    if (negativePriceOverrideTarget != lastNegativePriceOverrideTarget)
    {
        if (negativePriceOverrideTarget >= 0)
        {
            lmg.logTag(LL::Info, "PRICE", "Negative price active (%.3f EUR/kWh) -> forcing %d W", electricityPriceEurKwh, negativePriceOverrideTarget);
        }
        else
        {
            lmg.logTag(LL::Info, "PRICE", "Negative price inactive (%.3f EUR/kWh) -> resuming normal output path", electricityPriceEurKwh);
        }
        lastNegativePriceOverrideTarget = negativePriceOverrideTarget;
    }
    int pidBaseTarget = 0;
    int pidClampedBaseTarget = 0;
    int pidInput = 0;

    if (negativePriceOverrideTarget >= 0)
    {
        resetPidController();
        inverterCalculatedValue = negativePriceOverrideTarget;
    }
    else if (usePidSmoothing)
    {
        pidBaseTarget = currentInverterOutputW + signedGridPowerW + offset;
        pidClampedBaseTarget = constrain(pidBaseTarget, configuredMin, configuredMax);
        pidInput = pidController.initialized ? static_cast<int>(roundf(pidController.output)) : pidClampedBaseTarget;
        inverterCalculatedValue = computePidStabilizedTarget(pidBaseTarget, configuredMin, configuredMax);
    }
    else if (powerSmoother != nullptr)
    {
        inverterCalculatedValue = powerSmoother->smooth(currentGridImportW);
    }
    else
    {
        inverterCalculatedValue = configuredMax;
        lmg.logTag(LL::Warn, "RS485", "Smoother not initialized -> fallback to MAX");
    }

    if (negativePriceOverrideTarget >= 0)
    {
        inverterSetValue = negativePriceOverrideTarget;
        sendToRS485(static_cast<uint16_t>(inverterSetValue));
    }
    else if (limiterSettings.enableController.get())
    {
        const int correctedValue = usePidSmoothing ? inverterCalculatedValue : inverterCalculatedValue + offset;
        inverterSetValue = constrain(correctedValue, configuredMin, configuredMax);
        sendToRS485(static_cast<uint16_t>(inverterSetValue));
        lmg.logTag(LL::Trace, "RS485", "Controller enabled -> set inverter to %d W (calc=%d, corr=%d)", inverterSetValue, inverterCalculatedValue, correctedValue);
        if (usePidSmoothing)
        {
            lmg.logTag(LL::Debug, "PID", "inv=%d gridSigned=%d gridIn=%d gridOut=%d off=%d base=%d clamp=%d in=%d out=%d min=%d max=%d set=%d",
                       currentInverterOutputW, signedGridPowerW, gridImportW, gridExportW, offset, pidBaseTarget, pidClampedBaseTarget,
                       pidInput, inverterCalculatedValue, configuredMin, configuredMax, inverterSetValue);
        }
    }
    else
    {
        inverterSetValue = configuredMax;
        resetPidController();
        lmg.logTag(LL::Info, "RS485", "Controller disabled -> using MAX output");
        sendToRS485(static_cast<uint16_t>(inverterSetValue));
    }
}

void testRS232()
{
    // test the RS232 connection
    lmg.logTag(LL::Info, "RS485", "Testing RS232 connection... shorting RX and TX pins");
    lmg.logTag(LL::Info, "RS485", "Baudrate: %d", rs485settings.baudRate.get());
    lmg.logTag(LL::Info, "RS485", "RX Pin: %d", rs485settings.rxPin.get());
    lmg.logTag(LL::Info, "RS485", "TX Pin: %d", rs485settings.txPin.get());
    lmg.logTag(LL::Info, "RS485", "DE Pin: %d", rs485settings.dePin.get());

    Serial2.begin(rs485settings.baudRate.get(), SERIAL_8N1, rs485settings.rxPin.get(), rs485settings.txPin.get());
    Serial2.println("Hello RS485");
    delay(300);
    if (Serial2.available())
    {
        lmg.logTag(LL::Debug, "RS485", "[MAIN] Received on Serial2");
    }
}

void SetupStartDisplay()
{
    Wire.begin(i2cSettings.sdaPin.get(), i2cSettings.sclPin.get());
    Wire.setClock(i2cSettings.busFreq.get());

    const uint8_t address = static_cast<uint8_t>(i2cSettings.displayAddr.get());
    if (!display.begin(SSD1306_SWITCHCAPVCC, address))
    {
        displayInitialized = false;
        displayActive = false;
        lmg.logTag(LL::Warn, "Display", "SSD1306 init failed (addr=0x%02X)", static_cast<unsigned int>(address));
        return;
    }

    displayInitialized = true;
    displayActive = true;

    display.clearDisplay();
    display.drawRect(0, 0, 128, 25, WHITE);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(10, 5);
    display.println("Starting!");
    display.display();
}

void SetupStartTemperatureMeasuring()
{
    // init BME280 for temperature and humidity sensor
    bme280.setAddress(BME280_ADDRESS, i2cSettings.sdaPin.get(), i2cSettings.sclPin.get());
    bool isStatus = bme280.begin(
        bme280.BME280_STANDBY_0_5,
        bme280.BME280_FILTER_16,
        bme280.BME280_SPI3_DISABLE,
        bme280.BME280_OVERSAMPLING_2,
        bme280.BME280_OVERSAMPLING_16,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_MODE_NORMAL);
    if (!isStatus)
    {
        bme280Initialized = false;
        lmg.logTag(LL::Error, "BME280", "BME280 init failed");
    }
    else
    {
        bme280Initialized = true;
        lmg.logTag(LL::Info, "BME280", "BME280 ready. Starting measurement ticker...");

        temperatureTicker.attach(tempSettings.readIntervalSec.get(), readBme280); // Attach the ticker to read BME280
        readBme280();                                                             // initial read
    }
}

void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    logNetworkIpInfo("onWiFiConnected");
    lmg.logTag(LL::Debug, "WiFi", "Station Mode: http://%s", WiFi.localIP().toString().c_str());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    lmg.logTag(LL::Warn, "WiFi", "Disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    logNetworkIpInfo("onWiFiAPMode");
    lmg.logTag(LL::Debug, "WiFi", "AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}

void readBme280()
{
    if (!bme280Initialized)
    {
        lmg.logTag(LL::Warn, "BME280", "Skip read: sensor not initialized");
        return;
    }

    // todo: add settings for correcting the values!!!
    //   set sea-level pressure
    bme280.setSeaLevelPressure(tempSettings.seaLevelPressure.get());

    bme280.read();

    const float measuredTemperature = bme280.data.temperature + tempSettings.tempCorrection.get();
    const float measuredHumidity = bme280.data.humidity + tempSettings.humidityCorrection.get();
    const float measuredPressure = bme280.data.pressure;

    const bool temperatureValid = isfinite(measuredTemperature) && measuredTemperature >= -20.0f && measuredTemperature <= 70.0f;
    const bool humidityValid = isfinite(measuredHumidity) && measuredHumidity >= 0.0f && measuredHumidity <= 100.0f;
    const bool pressureValid = isfinite(measuredPressure) && measuredPressure >= 300.0f && measuredPressure <= 1200.0f;

    if (!temperatureValid || !humidityValid || !pressureValid)
    {
        lmg.logTag(LL::Warn, "BME280", "Invalid read T=%.1f H=%.1f P=%.1f -> keep last", measuredTemperature, measuredHumidity, measuredPressure);
        return;
    }

    temperature = measuredTemperature;
    Humidity = measuredHumidity;
    Pressure = measuredPressure;
    Dewpoint = cm::helpers::computeDewPoint(temperature, Humidity);

    // output formatted values to serial console
    lmg.logTag(LL::Trace, "BME280", "-----------------------");
    lmg.logTag(LL::Trace, "BME280", "Temperature: %.1f C", temperature);
    lmg.logTag(LL::Trace, "BME280", "Humidity   : %.1f %%", Humidity);
    lmg.logTag(LL::Trace, "BME280", "Dewpoint   : %.1f C", Dewpoint);
    lmg.logTag(LL::Trace, "BME280", "Pressure   : %.0f hPa", Pressure);
    lmg.logTag(LL::Trace, "BME280", "Altitude   : %.2f m", bme280.data.altitude);
    lmg.logTag(LL::Trace, "BME280", "-----------------------");
}

// Limiter provider moved into setup() for clarity

void WriteToDisplay()
{
    if (!displayInitialized || !displayActive)
    {
        return; // exit the function if the display is not active
    }

    // display.clearDisplay();
    display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area
    display.drawRect(0, 0, 128, 24, WHITE);

    display.setTextSize(1);
    display.setTextColor(WHITE);

    // When running in AP mode, show connection info prominently.
    if (WiFi.getMode() == WIFI_AP && WiFi.status() != WL_CONNECTED)
    {
        const IPAddress apIp = WiFi.softAPIP();
        const String apSsid = WiFi.softAPSSID();

        display.setCursor(3, 3);
        display.printf("AP: %s", apIp.toString().c_str());
        display.setCursor(3, 13);
        display.printf("SSID: %s", apSsid.c_str());
        display.display();
        return;
    }

    display.setCursor(3, 3);
    if (temperature > 0)
    {
        display.printf("<- %d W|Temp: %2.1f", currentGridImportW, temperature);
    }
    else
    {
        display.printf("<- %d W", currentGridImportW);
    }

    display.setCursor(3, 13);
    if (Dewpoint != 0)
    {
        display.printf("-> %d W|DP-T: %2.1f", inverterSetValue, Dewpoint);
    }
    else
    {
        display.printf("-> %d W", inverterSetValue);
    }

    display.display();
}

#if FEATURE_FAN_ENABLED
void CheckVentilator(float currentTemperature)
{
#if FEATURE_ANY_RELAY_OUTPUTS
    if (manualOverrideActive)
    {
        return;
    }
#endif
    if (!fanSettings.enabled.get())
    {
        setFanRelay(false);
        return;
    }
    if (currentTemperature >= fanSettings.onThreshold.get())
    {
        setFanRelay(true);
    }
    else if (currentTemperature <= fanSettings.offThreshold.get())
    {
        setFanRelay(false);
    }
}
#endif

#if FEATURE_HEATER_ENABLED
void EvaluateHeater(float currentTemperature)
{

#if FEATURE_ANY_RELAY_OUTPUTS
    if (manualOverrideActive)
    {
        return;
    }
#endif

    if (dewpointRiskActive)
    {
        heaterLatchedState = true;
    }

    if (heaterSettings.enabled.get())
    {
        // Priority 4: Threshold hysteresis (normal mode)
        float onTh = heaterSettings.onTemp.get();
        float offTh = heaterSettings.offTemp.get();
        if (offTh <= onTh)
            offTh = onTh + 0.3f; // enforce separation

        if (currentTemperature < onTh)
        {
            heaterLatchedState = true;
        }
        if (currentTemperature > offTh)
        {
            heaterLatchedState = false;
        }
    }
    setHeaterRelay(heaterLatchedState);
}
#endif

void ShowDisplayOn()
{
    if (!displayInitialized)
        return;
    displayTicker.detach();                                                // Stop the ticker to prevent multiple calls
    display.ssd1306_command(SSD1306_DISPLAYON);                            // Turn on the display
    displayTicker.attach(displaySettings.onTimeSec.get(), ShowDisplayOff); // Reattach the ticker to turn off the display after the specified time
    displayActive = true;
}

void ShowDisplayOff()
{
    if (!displayInitialized)
        return;
    displayTicker.detach();                      // Stop the ticker to prevent multiple calls
    display.ssd1306_command(SSD1306_DISPLAYOFF); // Turn off the display
    // display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area

    if (displaySettings.turnDisplayOff.get())
    {
        displayActive = false;
    }
}
