#include <Arduino.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <Preferences.h>
#include <Ticker.h>
#include <WiFi.h>
#include <time.h>
#include <cmath>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// AsyncWebServer instance is provided by ConfigManager library (extern AsyncWebServer server)

#include "ConfigManager.h"
#include "runtime/CompactLiveMetadata.h"
#include "runtime/RuntimeDiagnostics.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "io/IOManager.h"
#include "alarm/AlarmManager.h"
#include "logging/LoggingManager.h"

#define CM_MQTT_NO_DEFAULT_HOOKS
#include "mqtt/MQTTManager.h"
#include "sbt/SbtService.h"
#include "control/PreparedControl.h"
#include "control/CurrentControl.h"
#include "control/OutputCorrection.h"
#include "control/TelemetryAge.h"
#include "control/NumericSafety.h"
#include "control/FuturePrices.h"
#include "control/PlanCaptureContext.h"
#include "control/PvForecast.h"
#include "control/ChargePlan.h"
#include "control/ModeSettingsRuntime.h"

#include "RS485Module/RS485Module.h"
#include "helpers/HelperModule.h"

// Card visibility is independent from sensor acquisition and MQTT.
#ifndef SIL_DEACTIVATE_BME280_CARD
#define SIL_DEACTIVATE_BME280_CARD 1
#endif
// Installed MPPT 150/35 hardware ceiling; owner confirmed no additional current limit.
#ifndef SIL_QUALIFIED_MPPT_MAX_CHARGE_A
#define SIL_QUALIFIED_MPPT_MAX_CHARGE_A 35.0f
#endif
static_assert(SIL_QUALIFIED_MPPT_MAX_CHARGE_A >= 0, "MPPT charge limit cannot be negative");
static constexpr int kI2cSda = 21, kI2cScl = 22;
static constexpr uint32_t kI2cHz = 400000;

// Feature flags
#ifndef FEATURE_BME280_ENABLED
#define FEATURE_BME280_ENABLED 1
#endif

#ifndef FEATURE_OLED_DISPLAY_ENABLED
#define FEATURE_OLED_DISPLAY_ENABLED 1
#endif

#ifndef FEATURE_FAN_ENABLED
#define FEATURE_FAN_ENABLED 0
#endif

#ifndef FEATURE_HEATER_ENABLED
#define FEATURE_HEATER_ENABLED 0
#endif

#ifndef FEATURE_MQTT_LOGGER_ENABLED
#define FEATURE_MQTT_LOGGER_ENABLED 0
#endif

#define FEATURE_ANY_I2C (FEATURE_OLED_DISPLAY_ENABLED || FEATURE_BME280_ENABLED)
#define FEATURE_ANY_RELAY_OUTPUTS (FEATURE_FAN_ENABLED || FEATURE_HEATER_ENABLED)

#if FEATURE_ANY_I2C
#include <Wire.h>
#endif

#if FEATURE_BME280_ENABLED
#include <BME280_I2C.h>
#endif

#if FEATURE_OLED_DISPLAY_ENABLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#endif

#if FEATURE_MQTT_LOGGER_ENABLED
#include "mqtt/MQTTLogOutput.h"
#endif

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
#define VERSION "5.6.0"
#endif

static sil::RuntimeDiagnostics runtimeDiagnostics;

enum class NegativePriceSettingPreference : uint8_t
{
    None,
    ForceMin,
    SetZero
};

// Forward declarations

// Startup and lifecycle
void cb_RS485Listener();
#if FEATURE_BME280_ENABLED
bool readBme280();
void SetupStartTemperatureMeasuring();
#endif
void setupGUI();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

// Setup helpers
static void logNetworkIpInfo(const char *context);
static void setupLogging();
static void setupMqtt();
static void setupNetworkDefaults();
static void attachSystemSettings();
static void syncStoredCodeVersion();
static void registerProjectSettings();
static void configureLimiterSettingsBehavior();
static void refreshPreparedSettings();
static void normalizeNegativePriceSettings(NegativePriceSettingPreference preferred, bool persist, bool logCorrection);
static void registerIOBindings();
static bool ensureNvsReady();
static bool isValidMacAddress(const String &value);
static void applyAccessPointMacPriority();

// MQTT and scheduler helpers
static const char *resetReasonToText(esp_reset_reason_t reason);
static void updateMqttTopics();
static void publishMqttNow();
static void handleRS485Scheduler();
#if FEATURE_BME280_ENABLED
static bool handleTemperatureScheduler();
#endif
static void updateStatusLED();

// RS485 and limiter helpers
void testRS232();
static void resetPidController();
static int computePidStabilizedTarget(int baseTarget, int configuredMin, int configuredMax);
static void processRS485Tick();
#if CM_ENABLE_LOGGING && CM_ENABLE_VERBOSE_LOGGING
static void verboseLogging();
#endif

// Display helpers
#if FEATURE_OLED_DISPLAY_ENABLED
void SetupStartDisplay();
void WriteToDisplay();
void ShowDisplayOn();
void ShowDisplayOff();
static void handleDisplayScheduler();
static void handleDisplayPowerScheduler();
#endif

// Relay automation helpers
#if FEATURE_ANY_RELAY_OUTPUTS
static void setManualOverride(bool on);
#endif

#if FEATURE_FAN_ENABLED
void CheckVentilator(float currentTemperature);
static void setFanRelay(bool on);
static bool getFanRelay();
#endif

#if FEATURE_HEATER_ENABLED
void EvaluateHeater(float currentTemperature);
static void setHeaterRelay(bool on);
static bool getHeaterRelay();
#endif

void ProjectConfig();
//--------------------------------------------------------------------------------------------------------------

#pragma region configuration variables

// Project settings (ConfigOptions API)
struct LimiterSettings
{
    Config<bool> enableController{ConfigOptions<bool>{.key = "LimiterEnable", .name = "Limiter Enabled", .category = "Limiter", .defaultValue = true, .sortOrder = 1}};
    Config<int> maxOutput{ConfigOptions<int>{.key = "LimiterMaxW", .name = "Max Output (W)", .category = "Limiter", .defaultValue = 1100, .sortOrder = 2}};
    Config<int> idleOutput{ConfigOptions<int>{.key = "LimiterIdleW", .name = "Idle Output (W)", .category = "Limiter", .defaultValue = 500, .sortOrder = 3}};
    Config<int> sbtTimeout{ConfigOptions<int>{.key = "sbtAgeS", .name = "SBT Data Timeout (s)", .category = "Limiter", .defaultValue = 180, .sortOrder = 19}};
    Config<int> minOutput{ConfigOptions<int>{.key = "LimiterMinW", .name = "Min Output (W)", .category = "Limiter", .defaultValue = 500, .sortOrder = 4}};
    Config<int> inputCorrectionOffset{ConfigOptions<int>{.key = "LimiterCorrW", .name = "Output Correction Offset (W)", .category = "Limiter", .defaultValue = 50, .sortOrder = 5}};
    Config<float> pidKp{ConfigOptions<float>{.key = "LimiterPIDKp", .name = "PID Kp", .category = "Limiter", .defaultValue = 0.35f, .sortOrder = 7}};
    Config<float> pidKi{ConfigOptions<float>{.key = "LimiterPIDKi", .name = "PID Ki", .category = "Limiter", .defaultValue = 0.05f, .sortOrder = 8}};
    Config<float> pidKd{ConfigOptions<float>{.key = "LimiterPIDKd", .name = "PID Kd", .category = "Limiter", .defaultValue = 0.02f, .sortOrder = 9}};
    Config<bool> forceMinOnNegativePrice{ConfigOptions<bool>{.key = "LimiterNegPrice", .name = "Force Min On Negative Price", .category = "Limiter", .defaultValue = false, .sortOrder = 10}};
    Config<bool> setZeroOnNegativePrice{ConfigOptions<bool>{.key = "LimNegZero", .name = "Set 0 On Negative Price", .category = "Limiter", .defaultValue = false, .sortOrder = 11}};
    Config<float> RS232PublishPeriod{ConfigOptions<float>{.key = "LimiterRS485P", .name = "RS485 Publish Period (s)", .category = "Limiter", .defaultValue = 2.0f, .sortOrder = 12}};

    Config<float> reserveOffSoc{ConfigOptions<float>{.key = "resOff", .name = "Reserve OFF SOC (%)", .category = "Limiter", .defaultValue = 2.0f, .sortOrder = 20}};
    Config<float> reserveOnSoc{ConfigOptions<float>{.key = "resOn", .name = "Reserve ON SOC (%)", .category = "Limiter", .defaultValue = 10.0f, .sortOrder = 21}};
    Config<bool> mosManualMode{ConfigOptions<bool>{.key = "mosMan", .name = "MOS Hand Mode", .category = "Limiter", .defaultValue = true, .sortOrder = 23}};
    Config<bool> inverterEnabled{ConfigOptions<bool>{.key = "invEn", .name = "Inverter Enabled", .category = "Limiter", .defaultValue = true, .sortOrder = 24}};
    Config<float> priceComfort{ConfigOptions<float>{.key = "priceC", .name = "Price Comfort (EUR/kWh)", .category = "Limiter", .defaultValue = 0.25f, .sortOrder = 25}};
    Config<float> priceDeadband{ConfigOptions<float>{.key = "priceDb", .name = "Price Deadband (EUR/kWh)", .category = "Limiter", .defaultValue = 0.06f, .sortOrder = 26}};

    Config<String> priceScheduleInputTopic{ConfigOptions<String>{.key = "fcTopic", .name = "Price Schedule Topic", .category = "MQTT-Topics", .defaultValue = String("homeassistant/sensor/electricity_price/schedule"), .sortOrder = 54}};
    Config<String> pvScheduleInputTopic{ConfigOptions<String>{.key = "pvTopic", .name = "PV Forecast Topic", .category = "MQTT-Topics", .defaultValue = String("homeassistant/sensor/solar_forecast/shed/schedule"), .sortOrder = 55}};
    Config<float> forecastPriceFactor{ConfigOptions<float>{.key = "fcPriceF", .name = "Forecast Price Factor", .category = "Limiter", .defaultValue = 0.93137255f, .sortOrder = 27}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&forecastPriceFactor);
        cfg.addSetting(&priceScheduleInputTopic);
        cfg.addSetting(&pvScheduleInputTopic);
        cfg.addSetting(&enableController);
        cfg.addSetting(&maxOutput);
        cfg.addSetting(&idleOutput);
        cfg.addSetting(&sbtTimeout);
        cfg.addSetting(&minOutput);
        cfg.addSetting(&inputCorrectionOffset);
        cfg.addSetting(&pidKp);
        cfg.addSetting(&pidKi);
        cfg.addSetting(&pidKd);
        cfg.addSetting(&forceMinOnNegativePrice);
        cfg.addSetting(&setZeroOnNegativePrice);
        cfg.addSetting(&RS232PublishPeriod);
        cfg.addSetting(&reserveOffSoc);
        cfg.addSetting(&reserveOnSoc);
        cfg.addSetting(&mosManualMode);
        cfg.addSetting(&inverterEnabled);
        cfg.addSetting(&priceComfort);
        cfg.addSetting(&priceDeadband);
    }
};

#if FEATURE_BME280_ENABLED
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
#endif

#if FEATURE_OLED_DISPLAY_ENABLED
struct I2CSettings
{
#if FEATURE_OLED_DISPLAY_ENABLED
    Config<int> displayAddr{ConfigOptions<int>{.key = "I2CDisp", .name = "Display Address", .category = "I2C", .defaultValue = 0x3C, .sortOrder = 4}};
#endif

    void attachTo(ConfigManagerClass &cfg)
    {
#if FEATURE_OLED_DISPLAY_ENABLED
        cfg.addSetting(&displayAddr);
#endif
    }
};
#endif

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

#if FEATURE_OLED_DISPLAY_ENABLED
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
#endif

struct WiFiRoamingSettings
{
    Config<String> preferredApMac{ConfigOptions<String>{.key = "WiFiMacPrio", .name = "Preferred AP MAC", .category = "WiFi", .defaultValue = "", .sortOrder = 90}};

    void attachTo(ConfigManagerClass &cfg)
    {
        cfg.addSetting(&preferredApMac);
    }
};

// Global settings/config instances
LimiterSettings limiterSettings;
#if FEATURE_BME280_ENABLED
TempSettings tempSettings;
#endif
#if FEATURE_OLED_DISPLAY_ENABLED
I2CSettings i2cSettings;
#endif
#if FEATURE_FAN_ENABLED
FanSettings fanSettings;
#endif
#if FEATURE_HEATER_ENABLED
HeaterSettings heaterSettings;
#endif
#if FEATURE_OLED_DISPLAY_ENABLED
DisplaySettings displaySettings;
#endif
WiFiRoamingSettings wifiRoamingSettings;
RS485_Settings rs485settings;

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

// Hardware objects
#if FEATURE_BME280_ENABLED
BME280_I2C bme280;
#endif
Ticker RS485Ticker;

#if FEATURE_OLED_DISPLAY_ENABLED
static constexpr int OLED_WIDTH = 128;
static constexpr int OLED_HEIGHT = 32;
static constexpr int OLED_RESET_PIN = 4; // keep legacy wiring default
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, OLED_RESET_PIN);
#endif


// MQTT and runtime state
int currentGridImportW = 0;        // signed grid power: positive import, negative export
int inverterCalculatedValue = 0;   // calculated controller output before final send decision
int inverterSetValue = 0;          // value sent to RS485 (with offset and min-max, or max when limiter off)
int solarPowerW = 0;               // current solar production
bool negativePriceActive = false;  // MQTT input: true forces minimum output when enabled
float electricityPriceEurKwh = 0.0f; // optional MQTT input for status/logging
#if FEATURE_BME280_ENABLED
float temperature = 0.0;           // current temperature in Celsius
float Dewpoint = 0.0;              // current dewpoint in Celsius
float Humidity = 0.0;              // current humidity in percent
float Pressure = 0.0;              // current pressure in hPa
#endif

static String mqttBaseTopic;
static String topicPublishSetValueW;
static String topicPublishCalculatedValueW;
static String topicPublishRuntimeDiagnostics;
static String topicPublishGridImportW;
static String topicPublishPlanningConfig;
static String topicPublishPlanningState;
static String topicPublishDischargeBudget;
static String topicPublishChargePlan;
static String topicModeState, topicModeCommand, topicModeStatus;
static String modeConfiguredRoot;
static bool modeSubscribed = false, modePublishDirty = true;
#if FEATURE_BME280_ENABLED
static String topicPublishTempC;
static String topicPublishHumidityPct;
static String topicPublishDewpointC;
#endif

// Scheduler/timing state
#if FEATURE_BME280_ENABLED
static bool bme280Initialized = false;
static volatile bool rs485TickDue = false;
static unsigned long nextTemperatureReadMs = 0;
static constexpr unsigned long MIN_TEMPERATURE_READ_INTERVAL_MS = 1000UL;
static constexpr unsigned long MAX_TEMPERATURE_READ_INTERVAL_MS = 3600000UL;
#else
static volatile bool rs485TickDue = false;
#endif

#if FEATURE_OLED_DISPLAY_ENABLED
// Display state
bool displayActive = true; // flag to indicate if the display is active
static bool displayInitialized = false;
static unsigned long nextDisplayRefreshMs = 0;
static unsigned long displayOffAtMs = 0;
static constexpr unsigned long DISPLAY_REFRESH_INTERVAL_MS = 1000UL;
static constexpr unsigned long MIN_DISPLAY_ON_MS = 1000UL;
static constexpr unsigned long MAX_DISPLAY_ON_MS = 86400000UL;
#endif

// Feature-specific globals
#if FEATURE_HEATER_ENABLED && FEATURE_BME280_ENABLED
static bool dewpointRiskActive = false;   // tracks dewpoint alarm state
#endif
#if FEATURE_HEATER_ENABLED
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
static sbt::Service sbtInputs;

#if FEATURE_FAN_ENABLED
static constexpr char IO_FAN_ID[] = "fan_relay";
#endif
#if FEATURE_HEATER_ENABLED
static constexpr char IO_HEATER_ID[] = "heater_relay";
#endif
static constexpr char IO_RESET_ID[] = "reset_btn";
static constexpr char IO_AP_ID[] = "ap_btn";

struct PidControllerState
{
    bool initialized = false;
    float output = 0.0f;
    float integral = 0.0f;
    float previousError = 0.0f;
    unsigned long lastUpdateMs = 0;
};

static PidControllerState pidController;
static control::PreparedSettings preparedSettings;
static control::RuntimeState preparedState;
static bool preparedSettingsLoaded = false;
static control::CurrentControl currentControl;
static control::ModeSettingsRuntime operatingModes;
static control::OperatingModePolicy modePolicy;
static control::ModeDecision modeDecision;
static control::CurrentDecision currentDecision;
static control::TelemetryAge telemetryAge;
static control::FuturePrices futurePrices;
static control::PvForecast pvForecast;
static String pvScheduleTopic;
static bool pvScheduleSubscribed = false;
static control::FuturePlan futurePlan;
static control::DischargeBudgetPublication dischargeBudget;
static control::ChargePlanPublication chargePlan;
static control::PlanCaptureContext planCaptureContext;
static uint32_t planContextEpoch = 0;
static bool planCaptureContextKnown = false;
static control::PlanCaptureFaults planCaptureFaults;
static String priceScheduleTopic;
static bool priceScheduleSubscribed = false;
static float forecastPriceFactor = 0.93f;
static bool currentDecisionAvailable = false;
static int lastIssuedCommandW = 0;
static int lastMockCommandW = 0;
// Regulator state stays in the pre-correction domain; wire/log values stay corrected.
static int lastIssuedBaseCommandW = 0;
static int lastMockBaseCommandW = 0;
static bool lastIssuedCommandKnown = false;
static int lastNegativePriceOverrideTarget = -1;
static NegativePriceSettingPreference lastNegativePriceSettingPreference = NegativePriceSettingPreference::None;
static bool normalizingNegativePriceSettings = false;

#if CM_ENABLE_LOGGING && CM_ENABLE_VERBOSE_LOGGING
struct ControlLogSnapshot
{
    control::CurrentInputs inputs;
    float gridW = 0, inverterW = 0;
    bool gridFresh = false, inverterFresh = false;
    int minW = 0, maxW = 0, offsetW = 0;
    uint32_t atMs = 0;
};
static ControlLogSnapshot controlLog;
#endif

#pragma endregion configurationn variables

//----------------------------------------
// MAIN FUNCTIONS
//----------------------------------------

void setup()
{
    Serial.begin(115200);
    runtimeDiagnostics.begin(VERSION, kRs485MockTransport);
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
    // Preserve system consumers without registering the former System live card.
    ConfigManager.getRuntime().addRuntimeProvider("system", [](JsonObject &data) {
        const uint32_t total = ESP.getHeapSize(), free = ESP.getFreeHeap(), largest = ESP.getMaxAllocHeap();
        const auto heap = sil::heapSnapshot(total, free, largest);
        data["freeHeap"] = heap.free; data["usedHeap"] = heap.used;
        data["totalHeap"] = total; data["heapFragmentation"] = heap.fragmentation;
        data["minFreeHeap"] = ESP.getMinFreeHeap(); data["largestBlock"] = largest;
        data["resetReason"] = static_cast<int>(esp_reset_reason());
        const bool connected = WiFi.status() == WL_CONNECTED;
        data["wifiConnected"] = connected;
        data["rssi"] = connected ? WiFi.RSSI() : 0;
        data["localIP"] = connected ? WiFi.localIP().toString() : String("0.0.0.0");
        data["gateway"] = connected ? WiFi.gatewayIP().toString() : String("0.0.0.0");
        data["routerMAC"] = connected ? WiFi.BSSIDstr() : String("00:00:00:00:00:00");
        data["channel"] = connected ? WiFi.channel() : 0;
        data["cpuFreqMHz"] = ESP.getCpuFreqMHz(); data["flashSize"] = ESP.getFlashChipSize();
        data["otaActive"] = ConfigManager.getOTAManager().isActive();
        data["otaHasPassword"] = ConfigManager.getOTAManager().hasPassword();
        data["mqttEnabled"] = mqttSettings.enableMQTT.get(); data["mqttConnected"] = mqtt.isConnected();
        data["mqttReconnects"] = mqtt.getReconnectCount();
        const time_t now = time(nullptr);
        if (now > 1600000000) {
            tm local; localtime_r(&now, &local); char text[24];
            strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
            data["dateTime"] = text;
        }
    }, 0);

    coreSettings.attachWiFi(ConfigManager);
    attachSystemSettings();
    coreSettings.attachNtp(ConfigManager);

    registerProjectSettings();
    configureLimiterSettingsBehavior();
    registerIOBindings();
    setupMqtt();

    ConfigManager.checkSettingsForErrors();
    ConfigManager.loadAll();
    operatingModes.begin();
    refreshPreparedSettings();
    normalizeNegativePriceSettings(NegativePriceSettingPreference::None, true, true);
    syncStoredCodeVersion();
    delay(100);
    setupNetworkDefaults();
    ioManager.begin();

    ConfigManager.startWebServer();

    // The synchronous smart-roaming scan can suspend control for up to ten seconds.
    ConfigManager.enableSmartRoaming(false);
    ConfigManager.setRoamingThreshold(-75);
    ConfigManager.setRoamingCooldown(30);
    ConfigManager.setRoamingImprovement(10);
    applyAccessPointMacPriority();

    updateMqttTopics();
    setupGUI();
#if FEATURE_OLED_DISPLAY_ENABLED
    SetupStartDisplay();
    ShowDisplayOn();
#endif

    cm::helpers::pulseWait(LED_BUILTIN, cm::helpers::PulseOutput::ActiveLevel::ActiveHigh, 3, 100);

    RS485begin();
#if FEATURE_BME280_ENABLED
    SetupStartTemperatureMeasuring();
#endif

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
    runtimeDiagnostics.loop();
    runtimeDiagnostics.enter(sil::RuntimeStage::Wifi);
#if defined(SIL_TEST_SINGLE_STACK_RESET) && SIL_TEST_SINGLE_STACK_RESET
    static_assert(kRs485MockTransport, "Recovery injection requires the hard mock transport");
    static bool recoveryAttempted = false;
    static uint32_t recoveryReadySince = 0;
    if (!recoveryAttempted)
    {
        const uint32_t now = millis();
        const uint32_t recoveryUtc = uint32_t(time(nullptr));
        if (!mqtt.isConnected() || !futurePlan.valid || !dischargeBudget.snapshot.valid || !chargePlan.snapshot.valid)
            recoveryReadySince = 0;
        else if (!recoveryReadySince)
            recoveryReadySince = now;
        else if (uint32_t(now - recoveryReadySince) >= 10000UL && recoveryUtc >= 1704067200UL &&
                 ((recoveryUtc % 900UL >= 270UL && recoveryUtc % 900UL < 295UL) ||
                  (recoveryUtc % 900UL >= 570UL && recoveryUtc % 900UL < 595UL)))
        {
            recoveryAttempted = true;
            Serial.printf("[I] [MOCKED!] STACK_RESET_TEST begin ms=%lu heap=%u block=%u\n",
                static_cast<unsigned long>(now), unsigned(ESP.getFreeHeap()), unsigned(ESP.getMaxAllocHeap()));
            ConfigManager.performStackReset();
            Serial.printf("[I] [MOCKED!] STACK_RESET_TEST end ms=%lu heap=%u block=%u\n",
                static_cast<unsigned long>(millis()), unsigned(ESP.getFreeHeap()), unsigned(ESP.getMaxAllocHeap()));
        }
    }
#endif
    ConfigManager.getWiFiManager().update();
    runtimeDiagnostics.enter(sil::RuntimeStage::Mqtt);
    mqtt.loop();
    if (mqttSettings.publishTopicBase.get() != modeConfiguredRoot) operatingModes.discardPendingMode();
    const char *previousModeStorage = operatingModes.storageReason();
    if (operatingModes.loop(uint32_t(time(nullptr)))) {
        resetPidController();
        currentControl = control::CurrentControl{};
        modePolicy = control::OperatingModePolicy{};
        planCaptureContextKnown = false;
        ++planContextEpoch;
        dischargeBudget.requestCapture(); chargePlan.requestCapture();
        modePublishDirty = true;
        rs485TickDue = true;
    }
    if (strcmp(previousModeStorage, operatingModes.storageReason())) modePublishDirty = true;
    runtimeDiagnostics.enter(sil::RuntimeStage::Sbt);
    sbtInputs.loop();
    runtimeDiagnostics.enter(sil::RuntimeStage::Publish);
    publishMqttNow();
    runtimeDiagnostics.enter(sil::RuntimeStage::Control);
    handleRS485Scheduler();
    runtimeDiagnostics.enter(sil::RuntimeStage::Sensors);
#if FEATURE_BME280_ENABLED
    const bool temperatureUpdated = handleTemperatureScheduler();
#else
    const bool temperatureUpdated = false;
#endif
    lmg.loop();
    ioManager.update();

    // Services managed by ConfigManager.
    runtimeDiagnostics.enter(sil::RuntimeStage::Config);
    ConfigManager.handleClient();
    alarmManager.update();

    updateStatusLED();
    cm::helpers::PulseOutput::loopAll();

#if FEATURE_OLED_DISPLAY_ENABLED
    handleDisplayScheduler();
    handleDisplayPowerScheduler();
#endif

#if FEATURE_ANY_RELAY_OUTPUTS
    if (temperatureUpdated && !manualOverrideActive)
    {
#if FEATURE_FAN_ENABLED
        CheckVentilator(temperature);
#endif
#if FEATURE_HEATER_ENABLED
        EvaluateHeater(temperature);
#endif
    }
#endif
    runtimeDiagnostics.report(mqtt.getBoundedIoStats(), futurePrices.revision(), pvForecast.revision());
    runtimeDiagnostics.enter(sil::RuntimeStage::Idle);
    delay(10);
}

void setupGUI()
{
    // Settings layout
    // coreSettings owns the WiFi/System/NTP pages now; MQTT module registers its own layout.
    ConfigManager.addSettingsPage("Limiter", 60);
    ConfigManager.addSettingsGroup("Limiter", "Limiter", "Limiter Settings", 60);
#if FEATURE_BME280_ENABLED
    ConfigManager.addSettingsPage("Temp", 70);
    ConfigManager.addSettingsGroup("Temp", "Temp", "Temp Settings", 70);
#endif
#if FEATURE_OLED_DISPLAY_ENABLED
    ConfigManager.addSettingsPage("I2C", 80);
    ConfigManager.addSettingsGroup("I2C", "I2C", "I2C Settings", 80);
#endif
#if FEATURE_FAN_ENABLED
    ConfigManager.addSettingsPage("Fan", 90);
    ConfigManager.addSettingsGroup("Fan", "Fan", "Fan Settings", 90);
#endif
#if FEATURE_HEATER_ENABLED
    ConfigManager.addSettingsPage("Heater", 100);
    ConfigManager.addSettingsGroup("Heater", "Heater", "Heater Settings", 100);
#endif
#if FEATURE_OLED_DISPLAY_ENABLED
    ConfigManager.addSettingsPage("Display", 110);
    ConfigManager.addSettingsGroup("Display", "Display", "Display Settings", 110);
#endif
    ConfigManager.addSettingsPage("RS485", 120);
    ConfigManager.addSettingsGroup("RS485", "RS485", "RS485 Settings", 120);
    ConfigManager.addSettingsPage("I/O", 130);
    ConfigManager.addSettingsGroup("I/O", "I/O", "I/O Settings", 130);

    // region sensor fields BME280
#if FEATURE_BME280_ENABLED && !SIL_DEACTIVATE_BME280_CARD
    ConfigManager.addLivePage("Temperature Sensor", 10);
    ConfigManager.addLiveCard("Temperature Sensor", "Sensors", 10);
    ConfigManager.addLiveGroup("Temperature Sensor", "Sensors", "Sensor Readings", 10);
    sil::CompactLiveGroup sensors(ConfigManager.getRuntime(), "sensors", "Temperature Sensor", "Sensors", "Sensor Readings");

    sensors.value<RuntimeFloatMeta>("temp", []()
                  { return roundf(temperature * 10.0f) / 10.0f; },
        "Temperature", "\xC2\xB0" "C", 1, 2);

    sensors.value<RuntimeFloatMeta>("hum", []()
                  { return roundf(Humidity * 10.0f) / 10.0f; },
        "Humidity", "%", 1, 11);

    sensors.value<RuntimeFloatMeta>("dew", []()
                  { return roundf(Dewpoint * 10.0f) / 10.0f; },
        "Dewpoint", "\xC2\xB0" "C", 1, 12);

    sensors.value<RuntimeFloatMeta>("pressure", []()
                  { return roundf(Pressure * 10.0f) / 10.0f; },
        "Pressure", "hPa", 1, 13);
#endif
#if FEATURE_BME280_ENABLED && !SIL_DEACTIVATE_BME280_CARD
    if(!sensors.valid()) lmg.logTag(LL::Error, "GUI", "Sensor fields unavailable");
#endif
    // endregion sensor fields

    // region Limiter
    ConfigManager.addLivePage("Limiter", 20);
    ConfigManager.addLiveCard("Limiter", "Limiter", 20);
    ConfigManager.addLiveGroup("Limiter", "Limiter", "Limiter Status", 20);
    sil::CompactLiveGroup limiter(ConfigManager.getRuntime(), "Limiter", "Limiter", "Limiter", "Limiter Status");

    if (!sil::registerHeapMetadata(ConfigManager.getRuntime()))
        lmg.logTag(LL::Error, "GUI", "Heap fields unavailable");
    ConfigManager.getRuntime().addRuntimeProvider("Limiter", [](JsonObject &data) {
        const uint32_t total = ESP.getHeapSize(), free = ESP.getFreeHeap(), largest = ESP.getMaxAllocHeap();
        const auto heap = sil::heapSnapshot(total, free, largest);
        data["usedHeap"] = heap.used; data["freeHeap"] = heap.free;
        data["heapFragmentation"] = heap.fragmentation;
    }, 40);

    ConfigManager.getRuntime().addRuntimeProvider("planning", [](JsonObject &data) {
        data["pv_revision"] = pvForecast.revision();
        data["pv_slots"] = pvForecast.slots();
        data["pv_valid"] = pvForecast.valid(uint32_t(time(nullptr)));
        data["pv_calibrated"] = pvForecast.calibrated();
        data["pv_charge_limit_qualified"] = SIL_QUALIFIED_MPPT_MAX_CHARGE_A > 0;
        data["pv_offered_wh"] = futurePlan.offeredPvWh;
        data["pv_usable_wh"] = futurePlan.usablePvWh;
    }, 50);

    limiter.value<RuntimeBoolMeta>("enabled", []()
                  { return limiterSettings.enableController.get(); },
        "Limiter Enabled", "", 2, 1);

    limiter.value<RuntimeFloatMeta>("gridIn", []()
                  { return currentGridImportW; },
        "Grid Import", "W", 0, 2);

    limiter.value<RuntimeIntMeta>("invSet", []()
                  { return inverterSetValue; },
        kRs485MockTransport ? "Mock Inverter Demand (no TX)" : "Requested Inverter Output (not ACK)", "W", 0, 3);

    limiter.value<RuntimeIntMeta>("invCalc", []()
                  { return inverterCalculatedValue; },
        "Calculated Setpoint", "W", 0, 4);

    limiter.value<RuntimeFloatMeta>("solar", []()
                  {
        float watts = 0;
        return telemetryAge.read(control::TelemetryAge::Inverter, millis(), control::kGridAcStaleMs, watts)
            ? double(watts) : NAN;
    },
        "Measured Inverter AC Output", "W", 0, 5);

    limiter.value<RuntimeBoolMeta>("negPrice", []()
                  { return negativePriceActive; },
        "Negative Price", "", 2, 6);

    limiter.value<RuntimeFloatMeta>("price", []()
                  { return electricityPriceEurKwh; },
        "Electricity Price", "EUR/kWh", 3, 7);
    limiter.value<RuntimeFloatMeta>("pvPower", []()
                  { return sbtInputs.liveMeasurement("victron/solar_power_w", millis()); },
        "PV Solar Power (Victron)", "W", 0, 8);

    limiter.value<RuntimeFloatMeta>("batteryPower", []()
                  { return sbtInputs.liveMeasurement("bms/power_w", millis()); },
        "Battery Power (+charge / -discharge)", "W", 0, 9);

    limiter.value<RuntimeTextMeta>("priceBand", []()
                  { return String(currentDecisionAvailable ? currentDecision.priceBand : "unknown"); },
        "Current Price Band", "", 2, 10);

    limiter.value<RuntimeBoolMeta>("forecastValid", []()
                  { return currentDecisionAvailable && futurePlan.valid; },
        "Future Price Plan Valid", "", 2, 11);

    limiter.value<RuntimeTextMeta>("forecastStatus", []()
                  { return String(currentDecisionAvailable ? futurePlan.reason : "startup"); },
        "Future Price Plan Status", "", 2, 12);

    limiter.value<RuntimeIntMeta>("forecastSlots", []()
                  { return futurePrices.slots(); },
        "Received Price Slots (15 min)", "", 0, 13);

    limiter.value<RuntimeTextMeta>("controlReason", []()
                  { return String(currentDecisionAvailable ? currentDecision.reason : "startup"); },
        "Output Decision Reason", "", 2, 14);

    limiter.value<RuntimeBoolMeta>("mockTransport", []()
                  { return kRs485MockTransport; },
        "Mock Transport (no hardware TX)", "", 2, 15);
    if(!limiter.valid()) lmg.logTag(LL::Error, "GUI", "Runtime fields unavailable");
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

#if FEATURE_HEATER_ENABLED && FEATURE_BME280_ENABLED
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
#elif FEATURE_HEATER_ENABLED
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
    lmg.addOutput(std::move(serialOut));

    // Keep normal UART diagnostics without debug or trace output.
    lmg.setGlobalLevel(LL::Info);
    lmg.attachToConfigManager(LL::Info, LL::Debug, "");

#if !CM_DISABLE_GUI_LOGGING
    auto guiOut = std::make_unique<cm::LoggingManager::GuiOutput>(ConfigManager, 20);
    guiOut->setLevel(LL::Error);
    lmg.addOutput(std::move(guiOut));
#endif
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
#if FEATURE_OLED_DISPLAY_ENABLED
                lmg.logTag(LL::Debug, "IO", "Reset button pressed -> show display");
                ShowDisplayOn();
#endif
            },
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
#if FEATURE_OLED_DISPLAY_ENABLED
                lmg.logTag(LL::Debug, "IO", "AP button pressed -> show display");
                ShowDisplayOn();
#endif
            },
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
    // Full diagnostic schedule plus topic/header; CM checks allocation and publish errors.
    mqtt.setBufferSize(control::DischargeBudget::kMqttPacketBytes);

    mqtt.attach(ConfigManager);
    // Keep MQTT settings/diagnostic values without the helper's implicit System card.
    ConfigManager.getRuntime().addRuntimeProvider("mqtt", [](JsonObject &data) {
        data["enabled"] = mqttSettings.enableMQTT.get();
        data["wifi"] = WiFi.isConnected(); data["connected"] = mqtt.isConnected();
        data["state"] = static_cast<int>(mqtt.getState());
        data["reconnects"] = mqtt.getReconnectCount(); data["retry"] = mqtt.getCurrentRetry();
        data["uptimeMs"] = mqtt.getUptime(); data["lastTopic"] = mqtt.getLastTopic();
        data["lastPayload"] = mqtt.getLastPayload(); data["lastMsgAgeMs"] = mqtt.getLastMessageAgeMs();
        data["grid_import_w"] = currentGridImportW; data["solar_power_w"] = solarPowerW;
        data["negative_price"] = negativePriceActive; data["electricity_price"] = electricityPriceEurKwh;
    }, 0);
    mqtt.addMqttSettingsToSettingsGroup(ConfigManager, "MQTT", "MQTT Settings", 40);
    sbtInputs.attach(ConfigManager, mqtt);

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

    telemetryAge.attach(ConfigManager);
    mqtt.onMessage([](const char *topic, const byte *payload, unsigned int length)
                   {
        if (topic && topicModeCommand == topic) {
            const String &configuredRoot = mqttSettings.publishTopicBase.get();
            if (configuredRoot != modeConfiguredRoot) return;
            control::ModeCommand command;
            if (control::parseModeCommand(reinterpret_cast<const char *>(payload), length, uint32_t(time(nullptr)), command))
                operatingModes.queueMode(command.mode, command.expiresAt);
        } else if (topic && priceScheduleTopic == topic) {
            const char *result = futurePrices.receive(payload, length, uint32_t(time(nullptr)), millis());
            if (strcmp(result, "pending") && strcmp(result, "duplicate"))
                lmg.logTag(!strcmp(result, "accepted") || !strcmp(result, "withdrawn") ? LL::Info : LL::Warn,
                    "FORECAST", "rx=%s rev=%lu slots=%u", result,
                    static_cast<unsigned long>(futurePrices.revision()), futurePrices.slots());
        } else if (topic && pvScheduleTopic == topic) {
            const char *result = pvForecast.receive(payload, length, uint32_t(time(nullptr)), millis());
            if (strcmp(result, "pending") && strcmp(result, "duplicate"))
                lmg.logTag(!strcmp(result, "accepted") ? LL::Info : LL::Warn,
                    "PV", "rx=%s rev=%lu slots=%u", result,
                    static_cast<unsigned long>(pvForecast.revision()), pvForecast.slots());
        } else telemetryAge.receive(topic, payload, length, millis());
    });
    mqtt.onDisconnected([]() {
        telemetryAge.reset(); priceScheduleSubscribed = false; pvScheduleSubscribed = false;
        modeSubscribed = false; modePublishDirty = true; operatingModes.discardPendingMode();
    });

    // Optional: show receive topics in runtime UI
    // mqtt.addMqttTopicToLiveGroup(ConfigManager, "grid_import_w", "mqtt", "MQTT-Received", "MQTT-Received", 1);

#if FEATURE_MQTT_LOGGER_ENABLED
    auto mqttLog = std::make_unique<cm::MQTTLogOutput>(mqtt);
    mqttLog->setLevel(LL::Info);
    mqttLog->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
    lmg.addOutput(std::move(mqttLog));
#endif
}

static void registerProjectSettings()
{
    limiterSettings.attachTo(ConfigManager);
    operatingModes.attachTo(ConfigManager);
#if FEATURE_BME280_ENABLED
    tempSettings.attachTo(ConfigManager);
#endif
#if FEATURE_OLED_DISPLAY_ENABLED
    i2cSettings.attachTo(ConfigManager);
#endif
#if FEATURE_FAN_ENABLED
    fanSettings.attachTo(ConfigManager);
#endif
#if FEATURE_HEATER_ENABLED
    heaterSettings.attachTo(ConfigManager);
#endif
#if FEATURE_OLED_DISPLAY_ENABLED
    displaySettings.attachTo(ConfigManager);
#endif
    wifiRoamingSettings.attachTo(ConfigManager);
    rs485settings.attachTo(ConfigManager);
}

// Called once after load and on setting changes, never from the control cycle.
// Invalid combinations remain visible as entered; future modules must require valid.
static void refreshPreparedSettings()
{
    forecastPriceFactor = limiterSettings.forecastPriceFactor.get();
    preparedSettings.idleOutputW = std::max(0, limiterSettings.idleOutput.get());
    const int timeout = limiterSettings.sbtTimeout.get();
    if (timeout < 1 || timeout > 9999) limiterSettings.sbtTimeout.set(180);
    sbtInputs.configureTimeout(unsigned(limiterSettings.sbtTimeout.get()));
    preparedSettings.reserveOffSocPercent = limiterSettings.reserveOffSoc.get();
    preparedSettings.reserveOnSocPercent = limiterSettings.reserveOnSoc.get();
    preparedSettings.mosManualMode = limiterSettings.mosManualMode.get();
    preparedSettings.inverterEnabled = limiterSettings.inverterEnabled.get();
    preparedSettings.priceComfortEurPerKwh = limiterSettings.priceComfort.get();
    preparedSettings.priceDeadbandEurPerKwh = limiterSettings.priceDeadband.get();
    preparedSettings.priceHysteresisEurPerKwh = preparedSettings.priceDeadbandEurPerKwh / 3.0f;
    preparedSettings.valid = std::isfinite(preparedSettings.reserveOffSocPercent) &&
        std::isfinite(preparedSettings.reserveOnSocPercent) &&
        preparedSettings.reserveOffSocPercent >= 0.0f &&
        preparedSettings.reserveOffSocPercent < preparedSettings.reserveOnSocPercent &&
        preparedSettings.reserveOnSocPercent <= 100.0f &&
        std::isfinite(preparedSettings.priceComfortEurPerKwh) &&
        std::isfinite(preparedSettings.priceDeadbandEurPerKwh) &&
        preparedSettings.priceDeadbandEurPerKwh >= 0.0f;
    preparedState.recalculationDirty = true;
    preparedSettingsLoaded = true;
}

static void configureLimiterSettingsBehavior()
{
    // Preparation only: callbacks cache settings; no planner or actuator is enabled.
    limiterSettings.idleOutput.setCallback([](int) { refreshPreparedSettings(); });
    limiterSettings.sbtTimeout.setCallback([](int) { refreshPreparedSettings(); });
    limiterSettings.reserveOffSoc.setCallback([](float) { refreshPreparedSettings(); });
    limiterSettings.reserveOnSoc.setCallback([](float) { refreshPreparedSettings(); });
    limiterSettings.mosManualMode.setCallback([](bool) { refreshPreparedSettings(); });
    limiterSettings.inverterEnabled.setCallback([](bool) { refreshPreparedSettings(); });
    limiterSettings.priceComfort.setCallback([](float) { refreshPreparedSettings(); });
    limiterSettings.priceDeadband.setCallback([](float) { refreshPreparedSettings(); });
    limiterSettings.forecastPriceFactor.setCallback([](float) { refreshPreparedSettings(); });

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

#if defined(SIL_MOCK_TRANSPORT) && SIL_MOCK_TRANSPORT
// Configure the isolated test broker locally; unset values keep the guard closed.
#ifndef SIL_MOCK_MQTT_EXPECTED_HOST
#define SIL_MOCK_MQTT_EXPECTED_HOST ""
#endif
#ifndef SIL_MOCK_MQTT_EXPECTED_PORT
#define SIL_MOCK_MQTT_EXPECTED_PORT 0
#endif
static bool mockMqttIsIsolated()
{
    const char *prefix = "SILHardening/COM8/in/";
    if (!SIL_MOCK_MQTT_EXPECTED_HOST[0] || SIL_MOCK_MQTT_EXPECTED_PORT < 1 || SIL_MOCK_MQTT_EXPECTED_PORT > 65535 ||
        mqttSettings.server.get() != SIL_MOCK_MQTT_EXPECTED_HOST || mqttSettings.port.get() != SIL_MOCK_MQTT_EXPECTED_PORT ||
        !limiterSettings.priceScheduleInputTopic.get().startsWith(prefix) ||
        !limiterSettings.pvScheduleInputTopic.get().startsWith(prefix)) return false;
    auto *sbtRoot = static_cast<Config<String> *>(ConfigManager.findSettingByKeyHint("MQTT-Topics", "SBTInputRoot"));
    if (!sbtRoot || !sbtRoot->get().startsWith(prefix)) return false;
    for (const char *id : {"grid_import_w", "solar_power_w", "negative_price", "electricity_price"}) {
        uint32_t hash = 2166136261u;
        for (const char *p = id; *p; ++p) { hash ^= uint8_t(*p); hash *= 16777619u; }
        char hint[16];
        snprintf(hint, sizeof(hint), "MQTTRT_%08X", unsigned(hash));
        auto *topic = static_cast<Config<String> *>(ConfigManager.findSettingByKeyHint("MQTT-Topics", hint));
        if (!topic || !topic->get().startsWith(prefix)) return false;
    }
    return true;
}
#endif

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

#if defined(SIL_MOCK_TRANSPORT) && SIL_MOCK_TRANSPORT
    // Never connect a newly flashed mock to production using inherited/default NVS.
    wifiSettings.useDhcp.set(true);
    mqttSettings.publishTopicBase.set("SILHardening/COM8/out");
    mqttSettings.clientId.set("SILHardening-COM8-device");
    if (!mockMqttIsIsolated()) {
        mqttSettings.enableMQTT.set(false);
        Serial.println("[W] [MOCKED!] MQTT disabled: fixture isolation incomplete");
    }
#endif

    // Test firmware keeps the existing device OTA credential.
#if !defined(SIL_MOCK_TRANSPORT) || !SIL_MOCK_TRANSPORT
    if (systemSettings.otaPassword.get() != OTA_PASSWORD)
    {
        systemSettings.otaPassword.save(OTA_PASSWORD);
    }
#endif

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

    modeConfiguredRoot = mqttSettings.publishTopicBase.get();
    if (base == mqttBaseTopic)
    {
        return;
    }

    if (modeSubscribed && !topicModeCommand.isEmpty()) mqtt.unsubscribe(topicModeCommand.c_str());
    modeSubscribed = false; modePublishDirty = true;
    operatingModes.discardPendingMode();
    mqttBaseTopic = base;
    topicModeState = mqttBaseTopic + "/Control/Mode";
    topicModeCommand = topicModeState + "/set";
    topicModeStatus = mqttBaseTopic + "/Control/ModeStatus";
    topicPublishSetValueW = mqttBaseTopic + "/SetValue";
    topicPublishCalculatedValueW = mqttBaseTopic + "/CalculatedValue";
    topicPublishRuntimeDiagnostics = mqttBaseTopic + "/Diagnostics/Runtime";
    topicPublishGridImportW = mqttBaseTopic + "/GetValue";
    topicPublishPlanningConfig = mqttBaseTopic + "/Planning/Config";
    topicPublishPlanningState = mqttBaseTopic + "/Planning/State";
    topicPublishDischargeBudget = mqttBaseTopic + "/Planning/DischargeBudget";
    topicPublishChargePlan = mqttBaseTopic + "/Planning/ChargePlan";
    dischargeBudget.topicChanged();
    chargePlan.topicChanged();
#if FEATURE_BME280_ENABLED
    topicPublishTempC = mqttBaseTopic + "/Temperature";
    topicPublishHumidityPct = mqttBaseTopic + "/Humidity";
    topicPublishDewpointC = mqttBaseTopic + "/Dewpoint";
#endif
}

struct JsonNumberText
{
    char text[16];
    JsonNumberText(float value, int decimals)
    {
        if (std::isfinite(value))
            snprintf(text, sizeof(text), "%.*f", decimals, value);
        else
            snprintf(text, sizeof(text), "null");
    }
};

// Payloads are formatted on the stack so each publish needs one heap allocation.
static String buildPlanningConfigPayload()
{
    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"v\":1,\"valid\":%s,\"resOff\":%s,\"resOn\":%s,\"mosMan\":%s,\"invEn\":%s,"
             "\"priceC\":%s,\"priceDb\":%s,\"hystDb\":%s,\"fcPriceF\":%s,\"maxOutputW\":%d}",
             preparedSettings.valid ? "true" : "false",
             JsonNumberText(preparedSettings.reserveOffSocPercent, 4).text,
             JsonNumberText(preparedSettings.reserveOnSocPercent, 4).text,
             preparedSettings.mosManualMode ? "true" : "false",
             preparedSettings.inverterEnabled ? "true" : "false",
             JsonNumberText(preparedSettings.priceComfortEurPerKwh, 4).text,
             JsonNumberText(preparedSettings.priceDeadbandEurPerKwh, 4).text,
             JsonNumberText(preparedSettings.priceHysteresisEurPerKwh, 4).text,
             JsonNumberText(forecastPriceFactor, 6).text, limiterSettings.maxOutput.get());
    return String(payload);
}

static String buildPlanningStatePayload()
{
    if (!currentDecisionAvailable)
        return "{\"v\":1,\"strategy\":null,\"reason\":null,\"limitW\":null,\"chargeW\":null,\"reserveOk\":null,\"allocWh\":null}";
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"v\":1,\"strategy\":\"%s\",\"reason\":\"%s\",\"limitW\":%d,\"chargeW\":%s,\"reserveOk\":%s%s,"
             "\"forecast\":%s,\"forecastReason\":\"%s\",\"revision\":%lu,\"allocWh\":%s,\"availableWh\":%s,"
             "\"shortfallWh\":%s,\"batteryMaxW\":%d,\"targetUTC\":%lu}",
             currentDecision.strategy, currentDecision.reason, currentDecision.ceilingW,
             currentDecision.chargeTargetValid ? JsonNumberText(currentDecision.chargeTargetW, 1).text : "null",
             currentDecision.reserveKnown ? (currentDecision.reserveAllowed ? "true" : "false") : "null",
             kRs485MockTransport ? ",\"mock\":true" : "",
             futurePlan.valid ? "true" : "false", futurePlan.reason,
             static_cast<unsigned long>(futurePrices.revision()),
             futurePlan.valid ? JsonNumberText(futurePlan.reservedWh, 1).text : "null",
             futurePlan.valid ? JsonNumberText(futurePlan.availableWh, 1).text : "null",
             futurePlan.valid ? JsonNumberText(futurePlan.shortfallWh, 1).text : "null",
             futurePlan.batteryCeilingW, static_cast<unsigned long>(futurePlan.targetAt));
    return String(payload);
}

static void publishMqttNow()
{
    dischargeBudget.connection(mqtt.isConnected());
    chargePlan.connection(mqtt.isConnected());
    if (!mqtt.isConnected())
    {
        return;
    }

    // CM allocates a key String per lazy publish check; avoid doing that on every loop pass.
    static uint32_t lastPublishCheckMs = 0;
    static uint32_t lastTopicCheckMs = 0;
    const uint32_t nowMs = millis();
    if (uint32_t(nowMs - lastPublishCheckMs) < 250UL)
    {
        return;
    }
    lastPublishCheckMs = nowMs;
    if (mqttBaseTopic.isEmpty() || uint32_t(nowMs - lastTopicCheckMs) >= 5000UL)
    {
        lastTopicCheckMs = nowMs;
        updateMqttTopics();
    }

    static uint32_t nextScheduleSubscribeMs = 0;
    static uint32_t nextModeAttemptMs = 0;
    if (static_cast<int32_t>(nowMs - nextModeAttemptMs) >= 0) {
        nextModeAttemptMs = nowMs + 5000UL;
        if (!modeSubscribed) modeSubscribed = mqtt.subscribe(topicModeCommand.c_str(), 1);
        {
            char status[256];
            static char publishedModeStatus[256] = "";
            const char *effective = !modeDecision.handled && currentDecisionAvailable && currentDecision.direct && currentDecision.ceilingW == 0
                ? "hold" : modeDecision.effective;
            const char *reason = !modeDecision.handled && currentDecisionAvailable ? currentDecision.reason : modeDecision.reason;
            snprintf(status, sizeof(status), "{\"configured\":\"%s\",\"effective\":\"%s\",\"reason\":\"%s\",\"storage\":\"%s\",\"charge_threshold\":%.2f,\"inverter_share_percent\":%d}",
                control::operatingModeName(operatingModes.confirmed().mode), effective,
                reason, operatingModes.storageReason(), operatingModes.confirmed().chargeThreshold,
                operatingModes.confirmed().inverterSharePercent);
            if (modePublishDirty || strcmp(status, publishedModeStatus)) {
                const bool stateSent = mqtt.publishRaw(topicModeState.c_str(), control::operatingModeName(operatingModes.confirmed().mode), true);
                const bool statusSent = mqtt.publishRaw(topicModeStatus.c_str(), status, true);
                modePublishDirty = !(stateSent && statusSent);
                if (!modePublishDirty) snprintf(publishedModeStatus, sizeof(publishedModeStatus), "%s", status);
            }
        }
    }
    const String &inputTopic = limiterSettings.priceScheduleInputTopic.get();
    if (priceScheduleTopic != inputTopic) {
        dischargeBudget.topicChanged(); chargePlan.topicChanged();
        if (priceScheduleSubscribed && !mqtt.unsubscribe(priceScheduleTopic.c_str()))
            lmg.logTag(LL::Warn, "FORECAST", "old topic unsubscribe failed");
        priceScheduleSubscribed = false;
        futurePrices.clear();
        futurePlan = control::FuturePlan{};
        priceScheduleTopic = inputTopic;
        nextScheduleSubscribeMs = 0;
        if (inputTopic.isEmpty() || inputTopic.indexOf('+') >= 0 || inputTopic.indexOf('#') >= 0)
            lmg.logTag(LL::Warn, "FORECAST", "invalid input topic; subscription disabled");
    }
    const bool scheduleTopicValid = !priceScheduleTopic.isEmpty() &&
        priceScheduleTopic.indexOf('+') < 0 && priceScheduleTopic.indexOf('#') < 0;
    if (scheduleTopicValid && !priceScheduleSubscribed && (nextScheduleSubscribeMs == 0 ||
        static_cast<int32_t>(nowMs - nextScheduleSubscribeMs) >= 0)) {
        nextScheduleSubscribeMs = nowMs + 5000UL;
        priceScheduleSubscribed = mqtt.subscribe(priceScheduleTopic.c_str(), 1);
        lmg.logTag(priceScheduleSubscribed ? LL::Info : LL::Warn, "FORECAST", "subscribe=%d topic=%s",
            priceScheduleSubscribed, priceScheduleTopic.c_str());
    }
    static uint32_t nextPvSubscribeMs = 0;
    const String &pvTopic = limiterSettings.pvScheduleInputTopic.get();
    if (pvScheduleTopic != pvTopic) {
        dischargeBudget.topicChanged(); chargePlan.topicChanged();
        if (pvScheduleSubscribed) mqtt.unsubscribe(pvScheduleTopic.c_str());
        pvScheduleSubscribed = false;
        pvForecast.clear();
        pvScheduleTopic = pvTopic;
        nextPvSubscribeMs = 0;
    }
    const bool pvTopicValid = !pvScheduleTopic.isEmpty() && pvScheduleTopic.length() <= 128 &&
        pvScheduleTopic.indexOf('+') < 0 && pvScheduleTopic.indexOf('#') < 0;
    if (pvTopicValid && !pvScheduleSubscribed && (!nextPvSubscribeMs ||
        static_cast<int32_t>(nowMs - nextPvSubscribeMs) >= 0)) {
        nextPvSubscribeMs = nowMs + 5000UL;
        pvScheduleSubscribed = mqtt.subscribe(pvScheduleTopic.c_str(), 1);
    }
    if (dischargeBudget.publishDue(nowMs))
    {
        const size_t bytes = dischargeBudget.snapshot.serialize(dischargeBudget.payload, sizeof(dischargeBudget.payload));
        const bool fits = control::DischargeBudget::packetFits(topicPublishDischargeBudget.length(), bytes);
        // A failed buffer allocation, serialization or send affects only this diagnostic.
        const bool sent = fits && mqtt.publishRaw(topicPublishDischargeBudget.c_str(), dischargeBudget.payload, false);
        dischargeBudget.published(sent, nowMs);
        if (!sent) lmg.logTag(LL::Warn, "FORECAST", "budget publish failed; retry 5s");
    }
    if (chargePlan.publishDue(nowMs))
    {
        const size_t bytes = chargePlan.snapshot.serialize(chargePlan.payload, sizeof(chargePlan.payload));
        const bool fits = control::ChargePlan::packetFits(topicPublishChargePlan.length(), bytes);
        const bool sent = fits && mqtt.publishRaw(topicPublishChargePlan.c_str(), chargePlan.payload, false);
        chargePlan.published(sent, nowMs);
        if (!sent) lmg.logTag(LL::Warn, "FORECAST", "charge plan publish failed; retry 5s");
    }
    mqtt.publishExtraTopicLazy("setvalue_w", topicPublishSetValueW.c_str(), []() { return String(inverterSetValue); }, false);
    mqtt.publishExtraTopicLazy("calculated_w", topicPublishCalculatedValueW.c_str(), []() { return String(inverterCalculatedValue); }, false);
    mqtt.publishExtraTopicLazy("runtime_diag", topicPublishRuntimeDiagnostics.c_str(), []() { return runtimeDiagnostics.mqttPayload(mqtt.getBoundedIoStats()); }, false);
    mqtt.publishExtraTopicLazy("grid_import_w", topicPublishGridImportW.c_str(), []() { return String(currentGridImportW); }, false);
    if (preparedSettingsLoaded)
    {
        mqtt.publishExtraTopicLazy("planning_config", topicPublishPlanningConfig.c_str(), []() { return buildPlanningConfigPayload(); }, false);
        mqtt.publishExtraTopicLazy("planning_state", topicPublishPlanningState.c_str(), []() { return buildPlanningStatePayload(); }, false);
    }
#if FEATURE_BME280_ENABLED
    mqtt.publishExtraTopicLazy("temperature_c", topicPublishTempC.c_str(), []() { return String(temperature); }, false);
    mqtt.publishExtraTopicLazy("humidity_pct", topicPublishHumidityPct.c_str(), []() { return String(Humidity); }, false);
    mqtt.publishExtraTopicLazy("dewpoint_c", topicPublishDewpointC.c_str(), []() { return String(Dewpoint); }, false);
#endif
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
    if (!std::isfinite(dtSeconds) || !std::isfinite(limiterSettings.pidKp.get()) ||
        !std::isfinite(limiterSettings.pidKi.get()) || !std::isfinite(limiterSettings.pidKd.get()))
    {
        resetPidController();
        return 0;
    }

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
    if (!std::isfinite(candidateOutput))
    {
        resetPidController();
        return 0;
    }
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

    int result = 0;
    if (!control::checkedFloatToInt(roundf(pidController.output), result))
    {
        resetPidController();
        return 0;
    }
    return result;
}

//----------------------------------------
// MQTT FUNCTIONS
//----------------------------------------

static unsigned long secondsToMsClamped(int seconds, unsigned long minMs, unsigned long maxMs)
{
    if (seconds <= 0)
    {
        return minMs;
    }
    const unsigned long secondsValue = static_cast<unsigned long>(seconds);
    if (secondsValue > maxMs / 1000UL)
    {
        return maxMs;
    }
    const unsigned long ms = secondsValue * 1000UL;
    if (ms < minMs)
    {
        return minMs;
    }
    if (ms > maxMs)
    {
        return maxMs;
    }
    return ms;
}

static bool timeReached(unsigned long now, unsigned long target)
{
    return target == 0 || static_cast<long>(now - target) >= 0;
}

static void handleRS485Scheduler()
{
    if (!rs485TickDue)
    {
        return;
    }

    rs485TickDue = false;
    processRS485Tick();
    runtimeDiagnostics.controlCompleted();
}

#if FEATURE_BME280_ENABLED
static bool handleTemperatureScheduler()
{
    if (!bme280Initialized)
    {
        return false;
    }

    const unsigned long now = millis();
    if (!timeReached(now, nextTemperatureReadMs))
    {
        return false;
    }

    const unsigned long intervalMs = secondsToMsClamped(tempSettings.readIntervalSec.get(),
                                                        MIN_TEMPERATURE_READ_INTERVAL_MS,
                                                        MAX_TEMPERATURE_READ_INTERVAL_MS);
    nextTemperatureReadMs = now + intervalMs;
    return readBme280();
}
#endif

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
    rs485TickDue = true;
}

static void issueInverterCommand(int watts, int maximumW, int outputOffsetW, int minimumW = 0)
{
    const int baseWatts = control::correctOutputCommand(watts, 0, maximumW);
    watts = control::correctOutputCommand(baseWatts, outputOffsetW, maximumW);
    minimumW = constrain(minimumW, 0, max(0, maximumW));
    if (minimumW > 0 && (currentDecision.ceilingW < minimumW || watts < minimumW))
    {
        currentDecision.strategy = "minimum";
        currentDecision.reason = "minimum_output";
        currentDecision.reserveAllowed = true;
        currentDecision.pvOnly = false;
        currentDecision.chargeTargetValid = false;
        currentDecision.chargeTargetW = 0;
    }
    watts = max(watts, minimumW);
    inverterSetValue = watts;
    if (kRs485MockTransport)
    {
        lastMockBaseCommandW = baseWatts;
        lastMockCommandW = watts;
        lmg.logTag(LL::Info, "MOCK", "[MOCKED!] demand=%dW reason=%s band=%s limit=%dW; no hardware TX",
            watts, currentDecision.reason, currentDecision.priceBand, currentDecision.ceilingW);
        return;
    }
    sendToRS485(static_cast<uint16_t>(watts));
    if (rs485settings.enableRS485.get())
    {
        lastIssuedBaseCommandW = baseWatts;
        lastIssuedCommandW = watts;
        lastIssuedCommandKnown = true;
    }
}

static void processRS485Tick()
{
    int configuredMin = limiterSettings.minOutput.get();
    int configuredMax = limiterSettings.maxOutput.get();
    if (configuredMin > configuredMax)
    {
        const int tmp = configuredMin;
        configuredMin = configuredMax;
        configuredMax = tmp;
    }

    const uint32_t nowMs = millis();
    float measuredGridW = 0, measuredInverterW = 0, measuredPrice = 0;
    int currentInverterOutputW = 0, signedGridPowerW = 0;
    const bool gridFresh = telemetryAge.read(control::TelemetryAge::Grid, nowMs, control::kGridAcStaleMs, measuredGridW) &&
        control::checkedFloatToInt(measuredGridW, signedGridPowerW);
    const bool inverterFresh = telemetryAge.read(control::TelemetryAge::Inverter, nowMs, control::kGridAcStaleMs, measuredInverterW) &&
        control::checkedFloatToInt(measuredInverterW, currentInverterOutputW);
    const bool priceFresh = telemetryAge.read(control::TelemetryAge::Price, nowMs, 1200000UL, measuredPrice);
    const int offset = limiterSettings.inputCorrectionOffset.get();
    const int gridImportW = max(signedGridPowerW, 0);
    const int gridExportW = control::nonnegativeExportMagnitude(signedGridPowerW);
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

    const auto source = sbtInputs.controlSnapshot(nowMs);
    control::CurrentInputs inputs;
    inputs.sourcesWithinGrace = source.sourcesWithinGrace;
    inputs.alarmRestricted = source.alarmRestricted;
    inputs.bmsValid = source.bmsValid;
    inputs.batteryDischargeIntentAvailable = source.batteryDischargeIntentAvailable;
    inputs.batteryChargeAvailable = source.batteryChargeAvailable;
    inputs.softwareMosLockOnly = source.softwareMosLockOnly;
    inputs.chargerFault = source.chargerFault;
    inputs.householdW = measuredInverterW + measuredGridW;
    inputs.chargerValid = source.chargerValid;
    inputs.chargerStandby = source.chargerStandby;
    inputs.metersValid = gridFresh && inverterFresh;
    inputs.priceValid = priceFresh;
    inputs.soc = source.soc;
    inputs.batteryW = source.batteryW;
    inputs.chargerW = source.chargerW;
    inputs.price = measuredPrice;
    inputs.bmsSequence = source.bmsSequence;
    inputs.chargerSequence = source.chargerSequence;
    control::ModeInputs modeInputs;
    modeInputs.settingsValid = preparedSettings.valid;
    modeInputs.metersValid = inputs.metersValid;
    modeInputs.bmsValid = inputs.bmsValid;
    modeInputs.dischargeAllowed = source.batteryDischargeIntentAvailable;
    modeInputs.chargeKnown = source.batteryChargeStateKnown;
    modeInputs.chargeAllowed = source.batteryChargeAvailable;
    modeInputs.physicalDischargeOff = source.bmsValid && source.batteryDischargeIntentAvailable && !source.batteryPathAvailable;
    modeInputs.genuineFault = source.alarmRestricted && !source.softwareMosLockOnly;
    modeInputs.chargerFault = source.chargerFault;
    modeInputs.chargerValid = source.chargerValid;
    modeInputs.chargerW = source.chargerW;
    modeInputs.inverterEnabled = preparedSettings.inverterEnabled;
    modeInputs.transportEnabled = kRs485MockTransport || rs485settings.enableRS485.get();
    modeInputs.householdW = inputs.householdW;
    modeInputs.soc = source.soc;
    modeInputs.reserveOff = preparedSettings.reserveOffSocPercent;
    modeInputs.reserveOn = preparedSettings.reserveOnSocPercent;
    modeInputs.correctionW = offset;
    modeInputs.maximumW = configuredMax;
    const auto nextModeDecision = modePolicy.update(operatingModes.confirmed(), modeInputs);
    if (strcmp(modeDecision.effective, nextModeDecision.effective) || strcmp(modeDecision.reason, nextModeDecision.reason))
        modePublishDirty = true;
    modeDecision = nextModeDecision;
    const uint32_t utc = uint32_t(time(nullptr));
    control::PlanCaptureContext captureContext;
    captureContext.settings = preparedSettings;
    captureContext.maximumW = configuredMax; captureContext.minimumW = configuredMin; captureContext.offsetW = offset;
    captureContext.sourceTimeoutS = limiterSettings.sbtTimeout.get();
    captureContext.priceFactor = forecastPriceFactor;
    captureContext.observePrice(priceFresh, measuredPrice, planCaptureContextKnown ? &planCaptureContext : nullptr);
    captureContext.prices = futurePrices.contentRevision(); captureContext.pv = pvForecast.contentRevision();
    captureContext.limiter = limiterSettings.enableController.get();
    captureContext.transport = kRs485MockTransport || rs485settings.enableRS485.get();
    captureContext.negativeMin = limiterSettings.forceMinOnNegativePrice.get();
    captureContext.negativeZero = limiterSettings.setZeroOnNegativePrice.get();
    captureContext.pvValid = pvForecast.valid(utc);
    if (!planCaptureContextKnown || !planCaptureContext.same(captureContext))
    {
        if (!planCaptureContextKnown || !planCaptureContext.sameHard(captureContext))
            ++planContextEpoch;
        planCaptureContext = captureContext; planCaptureContextKnown = true;
        dischargeBudget.requestCapture(); chargePlan.requestCapture();
    }
    float coveredPrice = 0;
    const bool forecastCoverage = utc <= UINT32_MAX - 900UL &&
        futurePrices.priceAt(utc, forecastPriceFactor, coveredPrice) &&
        futurePrices.priceAt(utc + 900UL, forecastPriceFactor, coveredPrice);
    planCaptureFaults.observe(source.chargerFaultKnown, source.chargerFault);
    const bool knownSourceFault = planCaptureFaults.blocked(source.genuineBmsFault);
    const bool manualMode = operatingModes.confirmed().mode != control::OperatingMode::Automatic;
    const bool projectionHardInvalid = captureContext.hardInvalid(knownSourceFault) || manualMode;
    const bool planSourceGrace = source.sourcesWithinGrace || source.bmsWithinGrace;
    dischargeBudget.observe(nowMs, utc, planContextEpoch, planSourceGrace, projectionHardInvalid, forecastCoverage);
    chargePlan.observe(nowMs, utc, planContextEpoch, planSourceGrace, projectionHardInvalid, forecastCoverage);
    const bool capturePlans = dischargeBudget.captureDue(nowMs) || chargePlan.captureDue(nowMs);
    control::DischargeBudget *budgetTrace = capturePlans ? &dischargeBudget.candidate : nullptr;
    const bool plannerExpensiveNow = currentControl.expensivePrice(preparedSettings, priceFresh, measuredPrice);
    const bool plannerCheapNow = currentControl.cheapPrice();
    // An unavailable MPPT supplies no measured PV. Fresh BMS voltage only bounds the installed
    // MPPT's possible future charging after sunlight restores its power supply.
    const bool chargerPlanningAvailable = source.chargerValid || source.bmsValid;
    const float planningVoltageV = source.chargerValid ? source.chargerVoltageV : source.batteryVoltageV;
    const float planningMaxChargeW = chargerPlanningAvailable && source.batteryChargeConfirmed && !knownSourceFault &&
        std::isfinite(planningVoltageV) && planningVoltageV > 0
        ? planningVoltageV * SIL_QUALIFIED_MPPT_MAX_CHARGE_A : 0.0f;
    futurePlan = futurePrices.plan(utc, nowMs, preparedSettings,
        source.bmsValid, source.soc, source.batteryW, source.fullCapacityAh,
        source.bmsSequence, priceFresh, measuredPrice,
        plannerExpensiveNow,
        forecastPriceFactor, configuredMax, &pvForecast,
        planningMaxChargeW, budgetTrace,
        source.chargerValid ? source.chargerW : 0.0f, plannerCheapNow);
    if (!budgetTrace && dischargeBudget.invalidationDue(futurePlan.valid))
    {
        dischargeBudget.candidate.reset(utc, futurePrices.revision(), pvForecast.revision());
        dischargeBudget.candidate.finish(false, futurePlan.reason);
        dischargeBudget.select(dischargeBudget.candidate, nowMs, planContextEpoch);
    }
    inputs.futureValid = futurePlan.valid;
    inputs.futureBatteryCeilingW = futurePlan.batteryCeilingW;
    inputs.futureChargeTargetW = futurePlan.chargeTargetW;
    inputs.futureReason = futurePlan.reason;
    currentDecision = currentControl.update(preparedSettings, inputs,
        limiterSettings.enableController.get(), kRs485MockTransport || rs485settings.enableRS485.get(),
        configuredMax, kRs485MockTransport ? lastMockBaseCommandW : lastIssuedBaseCommandW, nowMs);
    if (modeDecision.handled) {
        currentDecision = control::CurrentDecision{};
        currentDecision.direct = true;
        currentDecision.ceilingW = modeDecision.watts;
        currentDecision.strategy = modeDecision.effective;
        currentDecision.reason = modeDecision.reason;
        currentControl = control::CurrentControl{};
    }
    // This projection consumes the decision but never participates in PID, MOS or inverter commands.
    control::ChargePlanInputs projection;
    projection.controlEnabled = preparedSettings.inverterEnabled &&
        (kRs485MockTransport || rs485settings.enableRS485.get()) &&
        (limiterSettings.enableController.get() || !source.batteryDischargeIntentAvailable);
    projection.sourcesQualified = preparedSettings.valid && planSourceGrace &&
        inputs.bmsValid && chargerPlanningAvailable && inputs.priceValid && inputs.metersValid && source.batteryChargeStateKnown &&
        !inputs.chargerFault && (!inputs.alarmRestricted || inputs.softwareMosLockOnly) &&
        (!projection.controlEnabled || !currentDecision.direct || source.chargerStandby);
    projection.chargeAvailable = source.batteryChargeAvailable;
    projection.soc = source.soc;
    projection.fullCapacityAh = source.fullCapacityAh;
    projection.maxChargeW = planningMaxChargeW;
    projection.currentChargerW = source.chargerW;
    projection.priceFactor = forecastPriceFactor;
    dischargeBudget.qualification(futurePlan.valid && inputs.bmsValid && chargerPlanningAvailable &&
        inputs.priceValid && inputs.metersValid && source.batteryChargeStateKnown && !knownSourceFault);
    chargePlan.qualification(futurePlan.valid && projection.sourcesQualified && !knownSourceFault);
    if (capturePlans)
    {
        chargePlan.candidate.project(utc, preparedSettings, futurePlan, futurePrices, pvForecast,
            currentDecision, dischargeBudget.candidate, projection);
        if (knownSourceFault)
        {
            dischargeBudget.candidate.finish(false, "source_fault");
            chargePlan.candidate.finish(false, "source_fault");
        }
        if (manualMode) {
            dischargeBudget.candidate.finish(false, "manual_mode");
            chargePlan.candidate.finish(false, "manual_mode");
        }
        dischargeBudget.select(dischargeBudget.candidate, nowMs, planContextEpoch);
        chargePlan.select(chargePlan.candidate, nowMs, planContextEpoch);
    }
    else if (chargePlan.invalidationDue(futurePlan.valid && projection.sourcesQualified))
    {
        chargePlan.candidate.reset(utc, futurePrices.revision(), pvForecast.revision());
        chargePlan.candidate.finish(false, futurePlan.valid ? "sources_missing" : futurePlan.reason);
        chargePlan.select(chargePlan.candidate, nowMs, planContextEpoch);
    }
    if (modeDecision.handled) {
        // Policy demand already includes correction. Zero bias preserves STOP and
        // avoids applying the automatic endpoint rule or configured MIN to a mode.
        currentDecisionAvailable = modeDecision.handled;
        resetPidController();
        inverterCalculatedValue = modeDecision.watts;
        issueInverterCommand(modeDecision.watts, configuredMax, 0);
        return;
    }
    currentDecisionAvailable = true;
    // Only qualified discharge intent may lift economic/PV preservation limits.
    // Stops, reserve, source grace/recovery and discharge-unavailable paths retain precedence.
    const int outputMinimumW = currentDecision.minimumOverrideAllowed && !currentDecision.direct &&
        negativePriceOverrideTarget < 0 && limiterSettings.enableController.get()
        ? constrain(configuredMin, 0, max(0, configuredMax)) : 0;

    if (negativePriceOverrideTarget >= 0)
    {
        resetPidController();
        inverterCalculatedValue = min(negativePriceOverrideTarget, currentDecision.ceilingW);
        currentDecision.ceilingW = inverterCalculatedValue;
        currentDecision.strategy = "negative_price";
        currentDecision.reason = "negative_price_override";
        currentDecision.chargeTargetValid = false;
    }
    else if (currentDecision.direct)
    {
        resetPidController();
        inverterCalculatedValue = currentDecision.ceilingW;
    }
    else
    {
        pidBaseTarget = control::saturatingAdd(currentInverterOutputW, signedGridPowerW);
        const int effectiveCeiling = max(outputMinimumW, constrain(currentDecision.ceilingW, 0, configuredMax));
        const int effectiveMin = min(configuredMin, effectiveCeiling);
        pidClampedBaseTarget = constrain(pidBaseTarget, effectiveMin, effectiveCeiling);
        pidInput = pidClampedBaseTarget;
        if (pidController.initialized && !control::checkedFloatToInt(roundf(pidController.output), pidInput)) resetPidController();
        inverterCalculatedValue = computePidStabilizedTarget(pidBaseTarget, effectiveMin, effectiveCeiling);
        if (!pidController.initialized) {
            currentDecision.direct = true;
            currentDecision.minimumOverrideAllowed = false;
            currentDecision.chargeTargetValid = false;
            currentDecision.ceilingW = 0;
            currentDecision.reason = "pid_invalid";
        }
    }
    if (negativePriceOverrideTarget >= 0)
    {
        inverterSetValue = min(negativePriceOverrideTarget, currentDecision.ceilingW);
        issueInverterCommand(inverterSetValue, configuredMax, offset);
    }
    else if (currentDecision.direct)
    {
        inverterSetValue = constrain(currentDecision.ceilingW, 0, configuredMax);
        resetPidController();
        issueInverterCommand(inverterSetValue, configuredMax, offset);
    }
    else if (limiterSettings.enableController.get() || currentDecision.pvOnly)
    {
        const int correctedValue = inverterCalculatedValue;
        const int ceiling = max(outputMinimumW, constrain(currentDecision.ceilingW, 0, configuredMax));
        const int effectiveMin = min(configuredMin, ceiling);
        inverterSetValue = constrain(correctedValue, effectiveMin, ceiling);
        issueInverterCommand(inverterSetValue, configuredMax, offset, outputMinimumW);
        lmg.logTag(LL::Trace, "RS485", "Controller enabled -> set inverter to %d W (calc=%d, corr=%d)", inverterSetValue, inverterCalculatedValue, correctedValue);
        lmg.logTag(LL::Trace, "PID", "inv=%d gridSigned=%d gridIn=%d gridOut=%d off=%d base=%d clamp=%d in=%d out=%d min=%d max=%d set=%d",
                   currentInverterOutputW, signedGridPowerW, gridImportW, gridExportW, offset, pidBaseTarget, pidClampedBaseTarget,
                   pidInput, inverterCalculatedValue, configuredMin, configuredMax, inverterSetValue);
    }
    else
    {
        inverterSetValue = configuredMax;
        resetPidController();
        lmg.logTag(LL::Info, "RS485", "Controller disabled -> using MAX output");
        issueInverterCommand(inverterSetValue, configuredMax, offset);
    }

#if CM_ENABLE_LOGGING && CM_ENABLE_VERBOSE_LOGGING
    controlLog.inputs = inputs;
    controlLog.gridW = measuredGridW;
    controlLog.inverterW = measuredInverterW;
    controlLog.gridFresh = gridFresh;
    controlLog.inverterFresh = inverterFresh;
    controlLog.minW = configuredMin;
    controlLog.maxW = configuredMax;
    controlLog.offsetW = offset;
    controlLog.atMs = nowMs;
    verboseLogging();
#endif
}

#if CM_ENABLE_LOGGING && CM_ENABLE_VERBOSE_LOGGING
static void verboseLogging()
{
    const auto &inputs = controlLog.inputs;
    static const char *lastReason = nullptr, *lastBand = nullptr;
    static int lastSetW = -1;
    const bool boundaryChanged = inverterSetValue != lastSetW &&
        (inverterSetValue == 0 || inverterSetValue == controlLog.maxW);
    if (!lastReason || strcmp(lastReason, currentDecision.reason) != 0 ||
        strcmp(lastBand, currentDecision.priceBand) != 0 || boundaryChanged)
    {
        lmg.logTag(LL::Info, "CTRL-STEP", "set=%dW limit=%dW strategy=%s reason=%s band=%s",
            inverterSetValue, currentDecision.ceilingW, currentDecision.strategy,
            currentDecision.reason, currentDecision.priceBand);
        lmg.logTag(LL::Info, "CTRL-STEP", "valid grid/inv/bms/charger/price=%d/%d/%d/%d/%d mqtt=%d neg=%d",
            controlLog.gridFresh, controlLog.inverterFresh, inputs.bmsValid, inputs.chargerValid,
            inputs.priceValid, mqtt.isConnected(), negativePriceActive);
        lmg.logTag(LL::Info, "CTRL-STEP", "grid=%+.1fW invAC=%.1fW batt=%+.1fW charger=%.1fW calc=%dW maxAlloc=%lu",
            controlLog.gridFresh ? controlLog.gridW : NAN, controlLog.inverterFresh ? controlLog.inverterW : NAN,
            inputs.bmsValid ? inputs.batteryW : NAN, inputs.chargerValid ? inputs.chargerW : NAN,
            inverterCalculatedValue, static_cast<unsigned long>(ESP.getMaxAllocHeap()));
    }
    lastReason = currentDecision.reason;
    lastBand = currentDecision.priceBand;
    lastSetW = inverterSetValue;

    static bool logged = false;
    static uint32_t lastLogMs = 0;
    const float publishSec = mqttSettings.publishIntervalSec.get();
    // Match the MQTT period, but never flood logs in publish-on-change mode.
    const double intervalMs = std::isfinite(publishSec) && publishSec > 10.0f
        ? double(publishSec) * 1000.0 : 10000.0;
    if (logged && uint32_t(controlLog.atMs - lastLogMs) < intervalMs)
        return;
    logged = true;
    lastLogMs = controlLog.atMs;

    const auto diagnostics = sbtInputs.diagnosticSnapshot(controlLog.atMs);
    const auto sample = [&diagnostics](unsigned i) -> double {
        return i < sbt::channelCount && (diagnostics.validMask & (uint32_t(1) << i))
                   ? diagnostics.values[i] : NAN;
    };
    // Verbose-only diagnostics use the existing visible Info sink. Trace is
    // intentionally filtered by setupLogging, independently of these flags.
    lmg.logTag(LL::Info, "CTRL", "---------- snapshot ms=%lu mock=%d ----------",
        static_cast<unsigned long>(controlLog.atMs), kRs485MockTransport);
    lmg.logTag(LL::Info, "CTRL-IN", "grid(GetValue)=%+.1fW valid=%d invAC=%+.1fW valid=%d price=%.4f valid=%d neg=%d",
        controlLog.gridFresh ? controlLog.gridW : NAN, controlLog.gridFresh,
        controlLog.inverterFresh ? controlLog.inverterW : NAN, controlLog.inverterFresh,
        inputs.priceValid ? inputs.price : NAN, inputs.priceValid, negativePriceActive);
    lmg.logTag(LL::Info, "CTRL-CFG", "lim=%d inv=%d rs485=%d min/max=%d/%d offset=%d mosMan=%d",
        limiterSettings.enableController.get(), preparedSettings.inverterEnabled, rs485settings.enableRS485.get(),
        controlLog.minW, controlLog.maxW, controlLog.offsetW, preparedSettings.mosManualMode);
    lmg.logTag(LL::Info, "CTRL-CFG", "resOff/On=%g/%g priceC/db/hys=%g/%g/%g",
        preparedSettings.reserveOffSocPercent, preparedSettings.reserveOnSocPercent,
        preparedSettings.priceComfortEurPerKwh,
        preparedSettings.priceDeadbandEurPerKwh, preparedSettings.priceHysteresisEurPerKwh);
    lmg.logTag(LL::Info, "CTRL-PRICE", "band=%s cheap<=%.4f expensive>=%.4f hyst=%.4f EUR/kWh",
        currentDecision.priceBand, preparedSettings.priceComfortEurPerKwh - preparedSettings.priceDeadbandEurPerKwh,
        preparedSettings.priceComfortEurPerKwh + preparedSettings.priceDeadbandEurPerKwh,
        preparedSettings.priceHysteresisEurPerKwh);
    lmg.logTag(LL::Info, "FORECAST", "valid=%d reason=%s factor=%.6f rev=%lu slots=%u nextPrice=%.4f",
        futurePlan.valid, futurePlan.reason, forecastPriceFactor,
        static_cast<unsigned long>(futurePrices.revision()), futurePrices.slots(),
        futurePlan.valid ? futurePlan.nextPrice : NAN);
    lmg.logTag(LL::Info, "FORECAST", "avail/reserve/shortfall=%.1f/%.1f/%.1fWh batteryMax=%dW charge=%.1fW targetUTC=%lu",
        futurePlan.availableWh, futurePlan.reservedWh, futurePlan.shortfallWh,
        futurePlan.batteryCeilingW, futurePlan.chargeTargetW, static_cast<unsigned long>(futurePlan.targetAt));
    lmg.logTag(LL::Info, "CTRL-OUT", "strategy=%s reason=%s ceiling=%d chargeW=%.1f reserve=%s",
        currentDecision.strategy, currentDecision.reason, currentDecision.ceilingW,
        currentDecision.chargeTargetValid ? currentDecision.chargeTargetW : NAN,
        currentDecision.reserveKnown ? (currentDecision.reserveAllowed ? "allowed" : "blocked") : "unknown");
    lmg.logTag(LL::Info, "CTRL-CMD", "demand=%.1fW calc=%dW set=%dW lastAttempt=%.0fW (not ACK)",
        inputs.metersValid ? double(controlLog.inverterW) + controlLog.gridW : NAN,
        inverterCalculatedValue, inverterSetValue, lastIssuedCommandKnown ? double(lastIssuedCommandW) : NAN);
    lmg.logTag(LL::Info, "CTRL-CMD", "mockDemand=%.0fW bmsValid=%d chargerValid=%d mqtt=%d",
        kRs485MockTransport ? double(lastMockCommandW) : NAN,
        inputs.bmsValid, inputs.chargerValid, mqtt.isConnected());
    // maxAlloc is the largest contiguous block; web responses fail when it drops.
    lmg.logTag(LL::Info, "HEAP", "free=%lu min=%lu maxAlloc=%lu",
        static_cast<unsigned long>(ESP.getFreeHeap()), static_cast<unsigned long>(ESP.getMinFreeHeap()),
        static_cast<unsigned long>(ESP.getMaxAllocHeap()));
    lmg.logTag(LL::Info, "BMS", "----------");
    lmg.logTag(LL::Info, "BMS", "fresh=%d seq=%lu why=%s soc=%.1f%% p=%+.1fW v=%.2fV i=%+.2fA",
        diagnostics.fresh[0], static_cast<unsigned long>(diagnostics.sequence[0]), diagnostics.reason[0],
        sample(2), sample(5), sample(3), sample(4));
    lmg.logTag(LL::Info, "BMS", "charge=%g discharge=%g allowC=%g allowD=%g alarm=%.0f prot=%s",
        sample(0), sample(1), sample(8), sample(9), sample(14), diagnostics.protectionValid ? diagnostics.protection : "NA");
    lmg.logTag(LL::Info, "BMS", "tempMin/Max=%.1f/%.1f cellMin/Max=%.3f/%.3f delta=%.3f remain/fullAh=%.1f/%.1f cells/sensors=%.0f/%.0f",
        sample(25), sample(10), sample(11), sample(12), sample(13), sample(6), sample(7), sample(26), sample(27));
    lmg.logTag(LL::Info, "VICTRON", "----------");
    lmg.logTag(LL::Info, "VICTRON", "fresh=%d seq=%lu why=%s solar=%.1fW battV=%.2fV chargeI=%.2fA chargerDC=%.1fW",
        diagnostics.fresh[1], static_cast<unsigned long>(diagnostics.sequence[1]), diagnostics.reason[1],
        sample(15), sample(18), sample(19), inputs.chargerValid ? inputs.chargerW : NAN);
    lmg.logTag(LL::Info, "VICTRON", "loadI=%.2fA state=%s err=%.0f yield=%.1fWh pvV/pvA/battW=UNSUPPORTED",
        sample(21), diagnostics.chargeStateValid ? diagnostics.chargeState : "NA", sample(23), sample(24));
    lmg.logTag(LL::Info, "CTRL", "---------- end ----------");
}
#endif

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

#if FEATURE_BME280_ENABLED
void SetupStartTemperatureMeasuring()
{
    // init BME280 for temperature and humidity sensor
    bme280.setAddress(BME280_ADDRESS, kI2cSda, kI2cScl);
    bool isStatus = bme280.begin(
        bme280.BME280_STANDBY_0_5,
        bme280.BME280_FILTER_16,
        bme280.BME280_SPI3_DISABLE,
        bme280.BME280_OVERSAMPLING_2,
        bme280.BME280_OVERSAMPLING_16,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_MODE_NORMAL);
    Wire.setClock(kI2cHz);
    if (!isStatus)
    {
        bme280Initialized = false;
        lmg.logTag(LL::Error, "BME280", "BME280 init failed");
    }
    else
    {
        bme280Initialized = true;
        lmg.logTag(LL::Info, "BME280", "BME280 ready. Starting measurement scheduler...");

        readBme280();
        nextTemperatureReadMs = millis() + secondsToMsClamped(tempSettings.readIntervalSec.get(),
                                                              MIN_TEMPERATURE_READ_INTERVAL_MS,
                                                              MAX_TEMPERATURE_READ_INTERVAL_MS);
    }
}
#endif

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

#if FEATURE_BME280_ENABLED
bool readBme280()
{
    if (!bme280Initialized)
    {
        lmg.logTag(LL::Warn, "BME280", "Skip read: sensor not initialized");
        return false;
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
        return false;
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
    return true;
}
#endif

// Limiter provider moved into setup() for clarity

#if FEATURE_OLED_DISPLAY_ENABLED
static void handleDisplayScheduler()
{
    const unsigned long now = millis();
    if (!timeReached(now, nextDisplayRefreshMs))
    {
        return;
    }

    nextDisplayRefreshMs = now + DISPLAY_REFRESH_INTERVAL_MS;
    WriteToDisplay();
}

static void handleDisplayPowerScheduler()
{
    if (!displayActive || !displaySettings.turnDisplayOff.get() || displayOffAtMs == 0)
    {
        return;
    }

    const unsigned long now = millis();
    if (timeReached(now, displayOffAtMs))
    {
        ShowDisplayOff();
    }
}

void SetupStartDisplay()
{
    Wire.begin(kI2cSda, kI2cScl);
    Wire.setClock(kI2cHz);

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
#if FEATURE_BME280_ENABLED
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
#else
    display.printf("<- %d W", currentGridImportW);

    display.setCursor(3, 13);
    display.printf("-> %d W", inverterSetValue);
#endif

    display.display();
}

void ShowDisplayOn()
{
    if (!displayInitialized)
        return;
    display.ssd1306_command(SSD1306_DISPLAYON); // Turn on the display
    displayActive = true;
    displayOffAtMs = millis() + secondsToMsClamped(displaySettings.onTimeSec.get(),
                                                   MIN_DISPLAY_ON_MS,
                                                   MAX_DISPLAY_ON_MS);
    nextDisplayRefreshMs = 0;
}

void ShowDisplayOff()
{
    if (!displayInitialized)
        return;
    display.ssd1306_command(SSD1306_DISPLAYOFF); // Turn off the display
    // display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area

    if (displaySettings.turnDisplayOff.get())
    {
        displayActive = false;
    }
    displayOffAtMs = 0;
}
#endif

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

#if FEATURE_BME280_ENABLED
    if (dewpointRiskActive)
    {
        heaterLatchedState = true;
    }
#endif

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
