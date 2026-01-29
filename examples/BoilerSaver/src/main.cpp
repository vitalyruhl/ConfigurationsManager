#include <Arduino.h>
#include <Ticker.h>
#include <Wire.h>

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>

#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "ConfigManager.h"
#include "settings.h"
#include "binking/Blinker.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "io/IOManager.h"
#include "logging/LoggingManager.h"

#define CM_MQTT_NO_DEFAULT_HOOKS
#include "mqtt/MQTTManager.h"
#include "mqtt/MQTTLogOutput.h"

// predeclare the functions (prototypes)
static void setupLogging();
static void setupGUI();
static void setupMQTT();
static void updateMqttTopics();
static void setupMqttCallbacks();
static void handleMqttMessage(const char* topic, const char* payload, unsigned int length);
static void publishMqttState(bool retained);
static void publishMqttStateIfNeeded();
static void ensureMqttDefaults(bool enableMissing, bool baseMissing, bool publishMissing);
static void registerIOBindings();
static void SetupStartDisplay();
static void WriteToDisplay();
static bool SetupStartWebServer();
static void ShowDisplay();
static void ShowDisplayOff();
static void updateStatusLED();
static void handeleBoilerState(bool forceON = false);
static void UpdateBoilerAlarmState();
static void setBoilerState(bool on);
static bool getBoilerState();
static void cb_readTempSensor();
static void setupTempSensor();
static void handleShowerRequest(bool requested);

// Shorthand helper for RuntimeManager access
static inline ConfigManagerRuntime& CRM() { return ConfigManager.getRuntime(); }

// Global blinkers: built-in LED and optional buzzer
static Blinker buildinLED(LED_BUILTIN, Blinker::HIGH_ACTIVE);

// WiFi Manager callback functions
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

//--------------------------------------------------------------------------------------------------------------

#pragma region configuration variables

static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.rw[data-group="Boiler"][data-key="Bo_Temp"]  .lab{ color:rgba(150, 2, 10, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="Boiler"][data-key="Bo_Temp"] .val{ color:rgba(150, 2, 10, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="Boiler"][data-key="Bo_Temp"] .un{ color:rgba(150, 2, 10, 1);font-weight:900;font-size: 1.2rem;}
)CSS";

static const char SETTINGS_PASSWORD[] = "";

static cm::LoggingManager &lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level;
static cm::MQTTManager &mqtt = cm::MQTTManager::instance();
static cm::IOManager ioManager;

static cm::CoreSettings &coreSettings = cm::CoreSettings::instance();
static cm::CoreSystemSettings &systemSettings = coreSettings.system;
static cm::CoreNtpSettings &ntpSettings = coreSettings.ntp;
static cm::CoreWiFiServices wifiServices;

static Adafruit_SSD1306 display(4);

static constexpr char IO_BOILER_ID[] = "boiler";
static constexpr char IO_RESET_ID[] = "reset_btn";
static constexpr char IO_AP_ID[] = "ap_btn";
static constexpr char IO_SHOWER_ID[] = "shower_btn";

static String mqttBaseTopic;
static String topicSetShowerTime;
static String topicWillShower;
static String topicSave;
static String topicBoilerEnabled;
static String topicOnThreshold;
static String topicOffThreshold;
static String topicBoilerTimeMin;
static String topicStopTimerOnTarget;
static String topicOncePerPeriod;
static String topicYouCanShowerPeriodMin;
static String topicActualState;
static String topicActualBoilerTemp;
static String topicActualTimeRemaining;
static String topicYouCanShowerNow;

static unsigned long lastMqttPublishMs = 0;

static Ticker displayTicker;
static Ticker TempReadTicker;

// globale helpers variables
float temperature = 70.0;    // current temperature in Celsius
int boilerTimeRemaining = 0; // remaining time for boiler in SECONDS
bool boilerState = false;    // current state of the heater (on/off)

static bool displayActive = true; // flag to indicate if the display is active

static bool globalAlarmState = false;// Global alarm state for temperature monitoring
static constexpr char TEMP_ALARM_ID[] = "temp_low";
static constexpr char SENSOR_FAULT_ALARM_ID[] = "sensor_fault";
static bool sensorFaultState = false; // Global alarm state for sensor fault monitoring

static unsigned long lastDisplayUpdate = 0; // Non-blocking display update management
static const unsigned long displayUpdateInterval = 100; // Update display every 100ms
static const unsigned long resetHoldDurationMs = 3000; // Require 3s hold to factory reset
// DS18B20 globals
static OneWire* oneWireBus = nullptr;
static DallasTemperature* ds18 = nullptr;
static bool youCanShowerNow = false; // derived status for MQTT/UI
static bool willShowerRequested = false; // unified flag for UI+MQTT 'I will shower'
static bool didStartupMQTTPropagate = false; // ensure one-time retained propagation
// static bool suppressNextWillShowerFalse = false; // no longer needed
// Gating for 'You can shower now' once-per-period behavior
static long lastYouCanShower1PeriodId = -1; // period id when we last published a '1'
static bool lastPublishedYouCanShower = false; // track last published state to allow publishing 0 transitions
// MQTT status monitoring
static unsigned long lastMqttStatusLog = 0;
static bool lastMqttConnectedState = false;

#pragma endregion configuration variables

//----------------------------------------
// MAIN FUNCTIONS
//----------------------------------------

void setup()
{

    setupLogging();
    lmg.log(LL::Info, "[SETUP] System setup start...");

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(APP_VERSION);
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.enableBuiltinSystemProvider();

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);

    systemSettings.allowOTA.setCallback([](bool enabled) {
        lmg.log(LL::Info, "[OTA] Setting changed to: %s", enabled ? "enabled" : "disabled");
        ConfigManager.getOTAManager().enable(enabled);
    });

    initializeAllSettings();
    registerIOBindings();

    bool mqttEnableMissing = false;
    bool mqttBaseMissing = false;
    bool mqttPublishMissing = false;
    {
        Preferences prefs;
        if (prefs.begin("ConfigManager", true)) {
            mqttEnableMissing = !prefs.isKey("MQTTEnable");
            mqttBaseMissing = !prefs.isKey("MQTTBaseTopic");
            mqttPublishMissing = !prefs.isKey("MQTTPubPer");
            prefs.end();
        }
    }

    setupMQTT();

    ConfigManager.loadAll();

    ConfigManager.getOTAManager().enable(systemSettings.allowOTA.get());

    // Re-attach to apply loaded values (attach() is idempotent)
    mqtt.attach(ConfigManager);

    ioManager.begin();
    setBoilerState(false);

    ensureMqttDefaults(mqttEnableMissing, mqttBaseMissing, mqttPublishMissing);
    updateMqttTopics();
    setupMqttCallbacks();

    setupGUI();
    ConfigManager.enableWebSocketPush();
    ConfigManager.setWebSocketInterval(1000);
    ConfigManager.setPushOnConnect(true);

    ConfigManager.enableSmartRoaming(true);
    ConfigManager.setRoamingThreshold(-75);
    ConfigManager.setRoamingCooldown(30);
    ConfigManager.setRoamingImprovement(10);
    lmg.log(LL::Info, "[MAIN] Smart WiFi Roaming enabled with WiFi stack fix");

    // Configure WiFi AP MAC filtering/priority (example - customize as needed)
    // ConfigManager.setWifiAPMacFilter("60:B5:8D:4C:E1:D5"); // Only connect to this specific AP
    ConfigManager.setWifiAPMacPriority("e0-08-55-92-55-ac");

    SetupStartDisplay();
    ShowDisplay();
    setupTempSensor();

    const bool startedInStationMode = SetupStartWebServer();
    lmg.log(LL::Debug, "[SETUP] SetupStartWebServer returned: %s", startedInStationMode ? "true" : "false");

    lmg.log(LL::Info, "[SETUP] System setup completed.");
}

void loop()
{
    boilerState = getBoilerState();

    ConfigManager.updateLoopTiming();
    ConfigManager.getWiFiManager().update();
    ioManager.update();
    ConfigManager.handleClient();
    ConfigManager.handleWebsocketPush();
    ConfigManager.handleOTA();
    ConfigManager.handleRuntimeAlarms();

    // Non-blocking display updates
    if (millis() - lastDisplayUpdate > displayUpdateInterval)
    {
        lastDisplayUpdate = millis();
        WriteToDisplay();
    }

    // Evaluate cross-field runtime alarms periodically (cheap doc build ~ small JSON)
    static unsigned long lastAlarmEval = 0;
    if (millis() - lastAlarmEval > 1500)
    {
        lastAlarmEval = millis();
        UpdateBoilerAlarmState();
        CRM().updateAlarms();
    }

    mqtt.loop();
    lmg.loop();

    publishMqttStateIfNeeded();

    // Monitor MQTT connection status and log periodically
    const bool currentMqttState = mqtt.isConnected();
    if (currentMqttState != lastMqttConnectedState) {
        if (currentMqttState) {
            lmg.log(LL::Info, "[MAIN] MQTT reconnected - Uptime: %lu ms, Reconnect count: %u",
                    mqtt.getUptime(), mqtt.getReconnectCount());
        } else {
            lmg.log(LL::Warn, "[MAIN] MQTT connection lost - State: %s, Retry: %u",
                    cm::MQTTManager::mqttStateToString(mqtt.getState()), mqtt.getCurrentRetry());
        }
        lastMqttConnectedState = currentMqttState;
        lastMqttStatusLog = millis();
    } else if (millis() - lastMqttStatusLog > 60000UL) {
        if (currentMqttState) {
            lmg.log(LL::Debug, "[MAIN] MQTT status: Connected, Uptime: %lu ms", mqtt.getUptime());
        } else {
            lmg.log(LL::Debug, "[MAIN] MQTT status: Disconnected, State: %s, Retry: %u/%u",
                    cm::MQTTManager::mqttStateToString(mqtt.getState()), mqtt.getCurrentRetry(), 15u);
        }
        lastMqttStatusLog = millis();
    }

    // Advance boiler/timer logic once per second (function is self-throttled)
    handeleBoilerState(false);

    updateStatusLED();
    Blinker::loopAll();
    delay(10);
}

//----------------------------------------
// PROJECT FUNCTIONS
//----------------------------------------

void setupGUI()
{
    // Ensure the dashboard shows basic firmware identity even before runtime data merges
        // RuntimeFieldMeta systemAppMeta{};
        // systemAppMeta.group = "system";
        // systemAppMeta.key = "app_name";
        // systemAppMeta.label = "application";
        // systemAppMeta.isString = true;
        // systemAppMeta.staticValue = String(APP_NAME);
        // systemAppMeta.order = 0;
        // CRM().addRuntimeMeta(systemAppMeta);

        // RuntimeFieldMeta systemVersionMeta{};
        // systemVersionMeta.group = "system";
        // systemVersionMeta.key = "app_version";
        // systemVersionMeta.label = "version";
        // systemVersionMeta.isString = true;
        // systemVersionMeta.staticValue = String(VERSION);
        // systemVersionMeta.order = 1;
        // CRM().addRuntimeMeta(systemVersionMeta);

        // RuntimeFieldMeta systemBuildMeta{};
        // systemBuildMeta.group = "system";
        // systemBuildMeta.key = "build_date";
        // systemBuildMeta.label = "build date";
        // systemBuildMeta.isString = true;
        // systemBuildMeta.staticValue = String(VERSION_DATE);
        // systemBuildMeta.order = 2;
        // CRM().addRuntimeMeta(systemBuildMeta);

    // Runtime live values provider
    CRM().addRuntimeProvider("Boiler",
        [](JsonObject &o)
        {
            o["Bo_EN_Set"] = boilerSettings.enabled.get();
            o["Bo_EN"] = getBoilerState();
            o["Bo_Temp"] = temperature;
            o["Bo_SettedTime"] = boilerSettings.boilerTimeMin.get();
            // Expose time left both in seconds and formatted HH:MM:SS
            o["Bo_TimeLeft"] = boilerTimeRemaining; // raw seconds for API consumers
            {
                int total = max(0, boilerTimeRemaining);
                int h = total / 3600;
                int m = (total % 3600) / 60;
                int s = total % 60;
                char buf[12];
                snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
                o["Bo_TimeLeftFmt"] = String(buf);
            }
            // Derived readiness: can shower when current temp >= off threshold
            bool canShower = (temperature >= boilerSettings.offThreshold.get()) && getBoilerState();
            o["Bo_CanShower"] = canShower;
            youCanShowerNow = canShower; // keep MQTT status aligned
        });

    // Add metadata for Boiler provider fields
    // Show whether boiler control is enabled (setting) and actual relay state
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_EN_Set", .label = "Enabled", .precision = 0, .order = 1, .isBool = true});
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_EN", .label = "Relay On", .precision = 0, .order = 2, .isBool = true});
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_CanShower", .label = "You can shower now", .precision = 0, .order = 5, .isBool = true});
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_Temp", .label = "Temperature", .unit = "°C", .precision = 1, .order = 10});
    // Show formatted time remaining as HH:MM:SS
    {
        RuntimeFieldMeta timeFmtMeta{};
        timeFmtMeta.group = "Boiler";
        timeFmtMeta.key = "Bo_TimeLeftFmt";
        timeFmtMeta.label = "Time remaining";
        timeFmtMeta.order = 21;
        timeFmtMeta.isString = true;
        CRM().addRuntimeMeta(timeFmtMeta);
    }
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_SettedTime", .label = "Time Set", .unit = "min", .precision = 0, .order = 22});

    // Add alarms provider for min Temperature monitoring with hysteresis
    CRM().registerRuntimeAlarm(TEMP_ALARM_ID);
    CRM().registerRuntimeAlarm(SENSOR_FAULT_ALARM_ID);
    CRM().addRuntimeProvider("Alarms",
        [](JsonObject &o)
        {
            o["AL_Status"] = globalAlarmState;
            o["SF_Status"] = sensorFaultState;
            o["On_Threshold"] = boilerSettings.onThreshold.get();
            o["Off_Threshold"] = boilerSettings.offThreshold.get();
        });

    // Define alarm metadata fields
    RuntimeFieldMeta alarmMeta{};
    alarmMeta.group = "Alarms";
    alarmMeta.key = "AL_Status";
    alarmMeta.label = "Under Temperature Alarm (Boiler Error?)";
    alarmMeta.precision = 0;
    alarmMeta.order = 1;
    alarmMeta.isBool = true;
    alarmMeta.boolAlarmValue = true;
    alarmMeta.alarmWhenTrue = true;
    alarmMeta.hasAlarm = true;
    CRM().addRuntimeMeta(alarmMeta);

    // Define sensor fault alarm metadata
    RuntimeFieldMeta sensorFaultMeta{};
    sensorFaultMeta.group = "Alarms";
    sensorFaultMeta.key = "SF_Status";
    sensorFaultMeta.label = "Temperature Sensor Fault";
    sensorFaultMeta.precision = 0;
    sensorFaultMeta.order = 2;
    sensorFaultMeta.isBool = true;
    sensorFaultMeta.boolAlarmValue = true;
    sensorFaultMeta.alarmWhenTrue = true;
    sensorFaultMeta.hasAlarm = true;
    CRM().addRuntimeMeta(sensorFaultMeta);

    // show some Info
    CRM().addRuntimeMeta({.group = "Alarms", .key = "On_Threshold", .label = "Alarm Under Temperature", .unit = "°C", .precision = 1, .order = 101});
    CRM().addRuntimeMeta({.group = "Alarms", .key = "Off_Threshold", .label = "You can shower now temperature", .unit = "°C", .precision = 1, .order = 102});

#ifdef ENABLE_TEMP_TEST_SLIDER
    // Temperature slider for testing (initialize with current temperature value)
    CRM().addRuntimeProvider("Hand overrides", [](JsonObject &o) { }, 100);

    static float transientFloatVal = temperature; // Initialize with current temperature
    ConfigManager.defineRuntimeFloatSlider("Hand overrides", "f_adj", "Temperature Test", -10.0f, 100.0f, temperature, 1, []()
        { return transientFloatVal; }, [](float v)
            { transientFloatVal = v;
                temperature = v;
                lmg.log(LL::Debug, "[MAIN] Temperature manually set to %.1f°C via slider", v);
    }, String("°C"));
#endif

    // Ensure interactive control is placed under Boiler group (project convention)

    // State button to manually control the boiler relay (under Boiler card)
    // Note: Provide helpText and sortOrder for predictable placement.
    ConfigManager.defineRuntimeStateButton(
        "Boiler",              // group (Boiler section)
        "sb_mode",             // key (short, URL-safe)
        "Will Shower",         // label shown in the UI
        []() { return willShowerRequested; },   // getter
        [](bool v) { handleShowerRequest(v); }, // setter
        /*defaultState*/ false,
        /*helpText*/ "Request hot water now; toggles boiler for a shower",
        /*sortOrder*/ 90
    );
    CRM().setRuntimeAlarmActive(TEMP_ALARM_ID, globalAlarmState, false);
}

void UpdateBoilerAlarmState()
{
    bool previousState = globalAlarmState;

    if (globalAlarmState)
    {
        if (temperature >= boilerSettings.onThreshold.get() + 2.0f)
        {
            globalAlarmState = false;
        }
    }
    else
    {
        if (temperature <= boilerSettings.onThreshold.get())
        {
            globalAlarmState = true;
        }
    }

    if (globalAlarmState != previousState)
    {
        lmg.log(LL::Debug, "[MAIN] [ALARM] Temperature %.1f°C -> %s",
                temperature, globalAlarmState ? "HEATER ON" : "HEATER OFF");
        CRM().setRuntimeAlarmActive(TEMP_ALARM_ID, globalAlarmState, false);
        handeleBoilerState(true); // Force boiler if the temperature is too low
    }
}

void handeleBoilerState(bool forceON)
{
    static unsigned long lastBoilerCheck = 0;
    unsigned long now = millis();

    if (now - lastBoilerCheck >= 1000) // Check every second
    {
        lastBoilerCheck = now;
        const bool stopOnTarget = boilerSettings.stopTimerOnTarget.get();
        const int prevTime = boilerTimeRemaining;

        // Temperature-based auto control: turn off when upper threshold reached, allow turn-on when below lower threshold
        if (getBoilerState()) {
            if (temperature >= boilerSettings.offThreshold.get()) {
                setBoilerState(false);
                if (stopOnTarget) {
                    boilerTimeRemaining = 0;
                    if (willShowerRequested) {
                        willShowerRequested = false;
                        if (mqtt.isConnected() && !topicWillShower.isEmpty()) {
                            mqtt.publish(topicWillShower.c_str(), "0", true);
                        }
                    }
                }
            }
        } else {
            if ((boilerSettings.enabled.get() || forceON) && (temperature <= boilerSettings.onThreshold.get()) && (boilerTimeRemaining > 0)) {
                setBoilerState(true);
            }
        }


        if (boilerSettings.enabled.get() || forceON)
        {
            if (boilerTimeRemaining > 0)
            {
                if (!getBoilerState())
                {
                    setBoilerState(true); // Turn on the boiler
                }
                boilerTimeRemaining--; // count down in seconds
            }
            else
            {
                if (getBoilerState())
                {
                    setBoilerState(false); // Turn off the boiler
                }
            }
        }
        else
        {
            if (getBoilerState())
            {
                setBoilerState(false); // Turn off the boiler if disabled
            }
        }

        // Detect timer end transition to 0 -> clear WillShower and publish retained OFF
        if (prevTime > 0 && boilerTimeRemaining <= 0) {
            if (willShowerRequested) {
                willShowerRequested = false;
                if (mqtt.isConnected() && !topicWillShower.isEmpty()) {
                    mqtt.publish(topicWillShower.c_str(), "0", true);
                }
            }
            if (getBoilerState()) {
                setBoilerState(false);
            }
        }
    }
}


static void cb_readTempSensor() {
    if (!ds18) {
        lmg.log(LL::Warn, "[TEMP] DS18B20 sensor not initialized");
        return;
    }
    ds18->requestTemperatures();
    float t = ds18->getTempCByIndex(0);
    lmg.log(LL::Debug, "[TEMP] Raw sensor reading: %.2f°C", t);

    // Check for sensor fault (-127°C indicates sensor error)
    bool sensorError = (t <= -127.0f || t >= 85.0f); // DS18B20 valid range is -55°C to +125°C, but -127°C is error code

    if (sensorError) {
        if (!sensorFaultState) {
            sensorFaultState = true;
            CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, true, false);
            lmg.log(LL::Error, "[TEMP] SENSOR FAULT detected! Reading: %.2f°C", t);
        }
        lmg.log(LL::Warn, "[TEMP] Invalid temperature reading: %.2f°C (sensor fault)", t);
        // Try to check if sensor is still present
        uint8_t deviceCount = ds18->getDeviceCount();
        lmg.log(LL::Debug, "[TEMP] Devices still found: %d", deviceCount);
    } else {
        // Clear sensor fault if it was set
        if (sensorFaultState) {
            sensorFaultState = false;
            CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, false, false);
            lmg.log(LL::Info, "[TEMP] Sensor fault cleared! Reading: %.2f°C", t);
        }

        temperature = t + tempSensorSettings.corrOffset.get();
        lmg.log(LL::Info, "[TEMP] Temperature updated: %.2f°C (offset: %.2f°C)", temperature, tempSensorSettings.corrOffset.get());
        // Optionally: push alarms now
        // CRM().updateAlarms(); // cheap
    }
}

static void setupTempSensor() {
    int pin = tempSensorSettings.gpioPin.get();
    if (pin <= 0) {
        lmg.log(LL::Warn, "[TEMP] DS18B20 GPIO pin not set or invalid -> skipping init");
        return;
    }
    oneWireBus = new OneWire((uint8_t)pin);
    ds18 = new DallasTemperature(oneWireBus);
    ds18->begin();

    // Configure for better compatibility
    ds18->setWaitForConversion(true);  // Wait for conversion to complete
    ds18->setCheckForConversion(true); // Check if conversion is done

    // Extended diagnostics
    uint8_t deviceCount = ds18->getDeviceCount();
    lmg.log(LL::Info, "[TEMP] OneWire devices found: %d", deviceCount);

    if (deviceCount == 0) {
        lmg.log(LL::Info, "[TEMP] No DS18B20 sensors found! Check:");
        lmg.log(LL::Info, "[TEMP] 1. Pull-up resistor (4.7kΩ) between VCC and GPIO");
        lmg.log(LL::Info, "[TEMP] 2. Wiring: VCC->3.3V, GND->GND, DATA->GPIO");
        lmg.log(LL::Info, "[TEMP] 3. Sensor connection and power");

        // Set sensor fault alarm if no devices found
        sensorFaultState = true;
        CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, true, false);
        lmg.log(LL::Warn, "[TEMP] Sensor fault alarm activated - no devices found");
    } else {
        lmg.log(LL::Info, "[TEMP] Found %d DS18B20 sensor(s) on GPIO %d", deviceCount, pin);

        // Clear sensor fault alarm if devices are found
        sensorFaultState = false;
        CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, false, false);

        // Check if sensor is using parasitic power
        bool parasitic = ds18->readPowerSupply(0);
        lmg.log(LL::Info, "[TEMP] Power mode: %s", parasitic ? "Normal (VCC connected)" : "Parasitic (VCC=GND)");

        // Set resolution to 12-bit for better accuracy
        ds18->setResolution(12);
        lmg.log(LL::Info, "[TEMP] Resolution set to 12-bit");
    }

    float intervalSec = (float)tempSensorSettings.readInterval.get();
    if (intervalSec < 1.0f) intervalSec = 30.0f;
    TempReadTicker.attach(intervalSec, cb_readTempSensor);
    lmg.log(LL::Info, "[TEMP] DS18B20 initialized on GPIO %d, interval %.1fs, offset %.2f°C",
            pin, intervalSec, tempSensorSettings.corrOffset.get());
}

//----------------------------------------
// LOGGING / IO / MQTT HELPERS
//----------------------------------------
static void setupLogging()
{
    Serial.begin(115200);

    auto serialOut = std::make_unique<cm::LoggingManager::SerialOutput>(Serial);
    serialOut->setLevel(LL::Trace);
    serialOut->addTimestamp(cm::LoggingManager::Output::TimestampMode::Millis);
    serialOut->setRateLimitMs(2);
    lmg.addOutput(std::move(serialOut));

    lmg.setGlobalLevel(LL::Trace);
    lmg.attachToConfigManager(LL::Info, LL::Trace, "CM");
}

static void registerIOBindings()
{
    analogReadResolution(12);

    ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
        .id = IO_BOILER_ID,
        .name = "Boiler Relay",
        .defaultPin = 23,
        .defaultActiveLow = true,
        .defaultEnabled = true,
    });
    ioManager.addIOtoGUI(IO_BOILER_ID, "Boiler IO", 1);

    ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
        .id = IO_RESET_ID,
        .name = "Reset Button",
        .defaultPin = 15,
        .defaultActiveLow = true,
        .defaultPullup = true,
        .defaultPulldown = false,
        .defaultEnabled = true,
    });

    ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
        .id = IO_AP_ID,
        .name = "AP Mode Button",
        .defaultPin = 13,
        .defaultActiveLow = true,
        .defaultPullup = true,
        .defaultPulldown = false,
        .defaultEnabled = true,
    });

    ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
        .id = IO_SHOWER_ID,
        .name = "Shower Request Button",
        .defaultPin = 19,
        .defaultActiveLow = true,
        .defaultPullup = true,
        .defaultPulldown = false,
        .defaultEnabled = true,
    });

    ioManager.addInputToGUI(IO_AP_ID, nullptr, 8, "AP Mode", "inputs", false);
    ioManager.addInputToGUI(IO_RESET_ID, nullptr, 9, "Reset", "inputs", false);
    ioManager.addInputToGUI(IO_SHOWER_ID, nullptr, 10, "Shower Button", "inputs", false);

    cm::IOManager::DigitalInputEventOptions resetOptions;
    resetOptions.longClickMs = resetHoldDurationMs;
    ioManager.configureDigitalInputEvents(
        IO_RESET_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []() {
                lmg.log(LL::Debug, "[MAIN] Reset button pressed -> show display");
                ShowDisplay();
            },
            .onLongPressOnStartup = []() {
                lmg.log(LL::Trace, "[MAIN] Reset button pressed at startup -> restoring defaults");
                ConfigManager.clearAllFromPrefs();
                ConfigManager.saveAll();
                delay(3000);
                ESP.restart();
            },
            .onLongClick = []() {
                lmg.log(LL::Trace, "[MAIN] Reset button long-press detected -> restoring defaults");
                ConfigManager.clearAllFromPrefs();
                ConfigManager.saveAll();
                delay(3000);
                ESP.restart();
            },
        },
        resetOptions);

    cm::IOManager::DigitalInputEventOptions apOptions;
    apOptions.longClickMs = 1200;
    ioManager.configureDigitalInputEvents(
        IO_AP_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []() {
                lmg.log(LL::Debug, "[MAIN] AP button pressed -> show display");
                ShowDisplay();
            },
            .onLongPressOnStartup = []() {
                lmg.log(LL::Trace, "[MAIN] AP button pressed at startup -> starting AP mode");
                ConfigManager.startAccessPoint("ESP32_Config", "");
            },
            .onLongClick = []() {
                lmg.log(LL::Trace, "[MAIN] AP button long-press -> starting AP mode");
                ConfigManager.startAccessPoint("ESP32_Config", "");
            },
        },
        apOptions);

    ioManager.configureDigitalInputEvents(
        IO_SHOWER_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onClick = []() {
                const bool newState = !willShowerRequested;
                lmg.log(LL::Debug, "[MAIN] Shower button pressed -> toggling shower request to %s",
                        newState ? "ON" : "OFF");
                ShowDisplay();
                handleShowerRequest(newState);
            },
        });
}

static void setBoilerState(bool on)
{
    ioManager.setState(IO_BOILER_ID, on);
}

static bool getBoilerState()
{
    return ioManager.getState(IO_BOILER_ID);
}

static void setupMQTT()
{
    mqtt.attach(ConfigManager);
    mqtt.addMQTTRuntimeProviderToGUI(ConfigManager, "mqtt", 2, 10);

    static bool mqttLogAdded = false;
    if (!mqttLogAdded) {
        auto mqttLog = std::make_unique<cm::MQTTLogOutput>(mqtt);
        mqttLog->setLevel(LL::Debug);
        mqttLog->addTimestamp(cm::LoggingManager::Output::TimestampMode::DateTime);
        lmg.addOutput(std::move(mqttLog));
        mqttLogAdded = true;
    }
}

static void ensureMqttDefaults(bool enableMissing, bool baseMissing, bool publishMissing)
{
    bool changed = false;
    if (enableMissing) {
        mqtt.settings().enableMQTT.set(true);
        changed = true;
    }
    if (baseMissing || mqtt.settings().publishTopicBase.get().isEmpty()) {
        mqtt.settings().publishTopicBase.set(String(APP_NAME));
        changed = true;
    }
    if (publishMissing) {
        mqtt.settings().publishIntervalSec.set(2.0f);
        changed = true;
    }

    if (changed) {
        ConfigManager.saveAll();
        mqtt.attach(ConfigManager);
    }
}

static void updateMqttTopics()
{
    String base = mqtt.settings().publishTopicBase.get();
    if (base.isEmpty()) {
        base = mqtt.getMqttBaseTopic();
    }
    if (base.isEmpty()) {
        base = String(APP_NAME);
    }

    if (base != mqttBaseTopic) {
        mqttBaseTopic = base;
        didStartupMQTTPropagate = false;
    }

    topicActualState = mqttBaseTopic + "/AktualState";
    topicActualBoilerTemp = mqttBaseTopic + "/TemperatureBoiler";
    topicActualTimeRemaining = mqttBaseTopic + "/TimeRemaining";
    topicYouCanShowerNow = mqttBaseTopic + "/YouCanShowerNow";

    const String sp = mqttBaseTopic + "/Settings";
    topicSetShowerTime = sp + "/SetShowerTime";
    topicWillShower = sp + "/WillShower";
    topicSave = sp + "/Save";
    topicBoilerEnabled = sp + "/BoilerEnabled";
    topicOnThreshold = sp + "/OnThreshold";
    topicOffThreshold = sp + "/OffThreshold";
    topicBoilerTimeMin = sp + "/BoilerTimeMin";
    topicStopTimerOnTarget = sp + "/StopTimerOnTarget";
    topicOncePerPeriod = sp + "/OncePerPeriod";
    topicYouCanShowerPeriodMin = sp + "/YouCanShowerPeriodMin";
}

static void setupMqttCallbacks()
{
    boilerSettings.enabled.setCallback([](bool v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicBoilerEnabled.c_str(), v ? "1" : "0", true);
        }
    });

    boilerSettings.onThreshold.setCallback([](float v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicOnThreshold.c_str(), String(v), true);
        }
    });

    boilerSettings.offThreshold.setCallback([](float v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicOffThreshold.c_str(), String(v), true);
        }
    });

    boilerSettings.boilerTimeMin.setCallback([](int v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicBoilerTimeMin.c_str(), String(v), true);
            mqtt.publish(topicYouCanShowerPeriodMin.c_str(), String(v), true);
        }
        lastYouCanShower1PeriodId = -1;
        lastPublishedYouCanShower = false;
    });

    boilerSettings.stopTimerOnTarget.setCallback([](bool v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicStopTimerOnTarget.c_str(), v ? "1" : "0", true);
        }
    });

    boilerSettings.onlyOncePerPeriod.setCallback([](bool v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicOncePerPeriod.c_str(), v ? "1" : "0", true);
        }
        lastYouCanShower1PeriodId = -1;
        lastPublishedYouCanShower = false;
    });
}

// Compute current period ID for once-per-period gating
static long getCurrentPeriodId()
{
    const long periodMin = max(1, boilerSettings.boilerTimeMin.get());
    const long periodSec = periodMin * 60L;
    time_t now = time(nullptr);
    if (now > 24 * 60 * 60) {
        return now / periodSec;
    }
    return (long)((millis() / 1000UL) / periodSec);
}

static void publishMqttState(bool retained)
{
    if (!mqtt.isConnected() || mqttBaseTopic.isEmpty()) {
        return;
    }

    mqtt.publish(topicActualBoilerTemp.c_str(), String(temperature), retained);

    int total = max(0, boilerTimeRemaining);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    mqtt.publish(topicActualTimeRemaining.c_str(), String(buf), retained);

    mqtt.publish(topicActualState.c_str(), getBoilerState() ? "1" : "0", retained);

    const bool canShower = (temperature >= boilerSettings.offThreshold.get()) && getBoilerState();
    youCanShowerNow = canShower;
    if (!boilerSettings.onlyOncePerPeriod.get()) {
        mqtt.publish(topicYouCanShowerNow.c_str(), canShower ? "1" : "0", retained);
        lastPublishedYouCanShower = canShower;
    } else {
        const long pid = getCurrentPeriodId();
        if (canShower) {
            if (pid != lastYouCanShower1PeriodId) {
                mqtt.publish(topicYouCanShowerNow.c_str(), "1", true);
                lastYouCanShower1PeriodId = pid;
                lastPublishedYouCanShower = true;
            }
        } else {
            if (lastPublishedYouCanShower) {
                mqtt.publish(topicYouCanShowerNow.c_str(), "0", true);
                lastPublishedYouCanShower = false;
            }
        }
    }

    buildinLED.repeat(/*count*/ 1, /*frequencyMs*/ 100, /*gapMs*/ 1500);
}

static void publishMqttStateIfNeeded()
{
    const float intervalSec = mqtt.settings().publishIntervalSec.get();
    if (intervalSec <= 0.0f) {
        return;
    }

    const unsigned long intervalMs = static_cast<unsigned long>(intervalSec * 1000.0f);
    if (intervalMs == 0) {
        return;
    }

    const unsigned long now = millis();
    if (lastMqttPublishMs == 0 || (now - lastMqttPublishMs >= intervalMs)) {
        lastMqttPublishMs = now;
        publishMqttState(false);
    }
}

static void handleMqttMessage(const char* topic, const char* payload, unsigned int length)
{
    if (!topic || !payload || length == 0) {
        lmg.log(LL::Warn, "[MAIN] MQTT callback with invalid payload - ignored");
        return;
    }

    String messageTemp(payload, length);
    messageTemp.trim();

    lmg.log(LL::Debug, "[MAIN] <-- MQTT: Topic[%s] <-- [%s]", topic, messageTemp.c_str());

    if (strcmp(topic, topicSetShowerTime.c_str()) == 0) {
        if (messageTemp.equalsIgnoreCase("null") ||
            messageTemp.equalsIgnoreCase("undefined") ||
            messageTemp.equalsIgnoreCase("NaN") ||
            messageTemp.equalsIgnoreCase("Infinity") ||
            messageTemp.equalsIgnoreCase("-Infinity")) {
            lmg.log(LL::Warn, "[MAIN] Received invalid value from MQTT: %s", messageTemp.c_str());
            messageTemp = "0";
        }
        const int mins = messageTemp.toInt();
        if (mins > 0) {
            boilerTimeRemaining = mins * 60;
            willShowerRequested = true;
            if (!getBoilerState()) {
                setBoilerState(true);
            }
            ShowDisplay();
            lmg.log(LL::Debug, "[MAIN] MQTT set shower time: %d min (relay ON)", mins);
            if (mqtt.isConnected()) {
                mqtt.publish(topicWillShower.c_str(), "1", true);
            }
        }
        return;
    }

    if (strcmp(topic, topicWillShower.c_str()) == 0) {
        const bool willShower = messageTemp.equalsIgnoreCase("1") ||
                                messageTemp.equalsIgnoreCase("true") ||
                                messageTemp.equalsIgnoreCase("on");
        if (willShower == willShowerRequested) {
            return;
        }
        if (willShower) {
            int mins = boilerSettings.boilerTimeMin.get();
            if (mins <= 0) mins = 60;
            if (boilerTimeRemaining <= 0) {
                boilerTimeRemaining = mins * 60;
            }
            willShowerRequested = true;
            if (!getBoilerState()) {
                setBoilerState(true);
            }
            ShowDisplay();
            lmg.log(LL::Debug, "[MAIN] HA request: will shower -> set %d min (relay ON)", mins);
        } else {
            willShowerRequested = false;
            boilerTimeRemaining = 0;
            if (getBoilerState()) {
                setBoilerState(false);
            }
            lmg.log(LL::Debug, "[MAIN] HA request: will shower = false -> timer cleared, relay OFF");
        }
        return;
    }

    if (strcmp(topic, topicBoilerEnabled.c_str()) == 0) {
        const bool v = messageTemp.equalsIgnoreCase("1") ||
                       messageTemp.equalsIgnoreCase("true") ||
                       messageTemp.equalsIgnoreCase("on");
        boilerSettings.enabled.set(v);
        lmg.log(LL::Debug, "[MAIN] MQTT: BoilerEnabled set to %s", v ? "true" : "false");
        return;
    }

    if (strcmp(topic, topicOnThreshold.c_str()) == 0) {
        const float v = messageTemp.toFloat();
        if (v > 0) {
            boilerSettings.onThreshold.set(v);
            lmg.log(LL::Debug, "[MAIN] MQTT: OnThreshold set to %.1f", v);
        }
        return;
    }

    if (strcmp(topic, topicOffThreshold.c_str()) == 0) {
        const float v = messageTemp.toFloat();
        if (v > 0) {
            boilerSettings.offThreshold.set(v);
            lmg.log(LL::Debug, "[MAIN] MQTT: OffThreshold set to %.1f", v);
        }
        return;
    }

    if (strcmp(topic, topicBoilerTimeMin.c_str()) == 0) {
        const int v = messageTemp.toInt();
        if (v >= 0) {
            boilerSettings.boilerTimeMin.set(v);
            lmg.log(LL::Debug, "[MAIN] MQTT: BoilerTimeMin set to %d", v);
            lastYouCanShower1PeriodId = -1;
            lastPublishedYouCanShower = false;
        }
        return;
    }

    if (strcmp(topic, topicStopTimerOnTarget.c_str()) == 0) {
        const bool v = messageTemp.equalsIgnoreCase("1") ||
                       messageTemp.equalsIgnoreCase("true") ||
                       messageTemp.equalsIgnoreCase("on");
        boilerSettings.stopTimerOnTarget.set(v);
        lmg.log(LL::Debug, "[MAIN] MQTT: StopTimerOnTarget set to %s", v ? "true" : "false");
        return;
    }

    if (strcmp(topic, topicOncePerPeriod.c_str()) == 0) {
        const bool v = messageTemp.equalsIgnoreCase("1") ||
                       messageTemp.equalsIgnoreCase("true") ||
                       messageTemp.equalsIgnoreCase("on");
        boilerSettings.onlyOncePerPeriod.set(v);
        lmg.log(LL::Debug, "[MAIN] MQTT: OncePerPeriod set to %s", v ? "true" : "false");
        lastYouCanShower1PeriodId = -1;
        lastPublishedYouCanShower = false;
        return;
    }

    if (strcmp(topic, topicYouCanShowerPeriodMin.c_str()) == 0) {
        int v = messageTemp.toInt();
        if (v <= 0) v = 45;
        boilerSettings.boilerTimeMin.set(v);
        lmg.log(LL::Debug, "[MAIN] MQTT: YouCanShowerPeriodMin mapped to BoilerTimeMin = %d", v);
        lastYouCanShower1PeriodId = -1;
        lastPublishedYouCanShower = false;
        return;
    }

    if (strcmp(topic, topicSave.c_str()) == 0) {
        ConfigManager.saveAll();
        if (mqtt.isConnected()) {
            mqtt.publish(topicSave.c_str(), "OK", false);
        }
        lmg.log(LL::Info, "[MAIN] Settings saved via MQTT");
        return;
    }

    lmg.log(LL::Warn, "[MAIN] MQTT: Topic [%s] not recognized - ignored", topic);
}

namespace cm
{
    void onMQTTConnected()
    {
        updateMqttTopics();
        lmg.log(LL::Info, "[MQTT] Connected");

        if (!topicSetShowerTime.isEmpty()) mqtt.subscribe(topicSetShowerTime.c_str());
        if (!topicWillShower.isEmpty()) mqtt.subscribe(topicWillShower.c_str());
        if (!topicBoilerEnabled.isEmpty()) mqtt.subscribe(topicBoilerEnabled.c_str());
        if (!topicOnThreshold.isEmpty()) mqtt.subscribe(topicOnThreshold.c_str());
        if (!topicOffThreshold.isEmpty()) mqtt.subscribe(topicOffThreshold.c_str());
        if (!topicStopTimerOnTarget.isEmpty()) mqtt.subscribe(topicStopTimerOnTarget.c_str());
        if (!topicOncePerPeriod.isEmpty()) mqtt.subscribe(topicOncePerPeriod.c_str());
        if (!topicYouCanShowerPeriodMin.isEmpty()) mqtt.subscribe(topicYouCanShowerPeriodMin.c_str());
        if (!topicSave.isEmpty()) mqtt.subscribe(topicSave.c_str());

        if (!didStartupMQTTPropagate) {
            publishMqttState(true);
            didStartupMQTTPropagate = true;
        }
        publishMqttState(false);
    }

    void onMQTTDisconnected()
    {
        lmg.log(LL::Warn, "[MQTT] Disconnected");
    }

    void onNewMQTTMessage(const char* topic, const char* payload, unsigned int length)
    {
        handleMqttMessage(topic, payload, length);
    }
}

//----------------------------------------
// DISPLAY FUNCTIONS
//----------------------------------------

void WriteToDisplay()
{
    // Static variables to track last displayed values
    static float lastTemperature = -999.0;
    static int lastTimeRemainingSec = -1;
    static bool lastBoilerState = false;
    static bool lastDisplayActive = true;

    if (displayActive == false)
    {
        // If display was just turned off, clear it once
        if (lastDisplayActive == true)
        {
            display.clearDisplay();
            display.display();
            lastDisplayActive = false;
        }
        return; // exit the function if the display is not active
    }

    bool wasInactive = !lastDisplayActive;
    lastDisplayActive = true;

    // Only update display if values have changed
    bool needsUpdate = wasInactive; // Force refresh on wake
    int timeLeftSec = boilerTimeRemaining;
    if (abs(temperature - lastTemperature) > 0.1 ||
        timeLeftSec != lastTimeRemainingSec ||
        boilerState != lastBoilerState)
    {
        needsUpdate = true;
        lastTemperature = temperature;
        lastTimeRemainingSec = timeLeftSec;
        lastBoilerState = boilerState;
    }

    if (!needsUpdate)
    {
        return; // No changes, skip display update
    }

    display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area
    display.drawRect(0, 0, 128, 24, WHITE);

    display.setTextSize(1);
    display.setTextColor(WHITE);

    display.cp437(true);// Use CP437 for extended glyphs (e.g., degree symbol 248)

    // Show boiler status and temperature
    display.setCursor(3, 3);
    if (temperature > 0)
    {
        display.printf("Relay: %s | T:%.1f ", boilerState ? "1" : "0", temperature);
        display.write((uint8_t)248); // degree symbol in CP437
        display.print("C");
    }
    else
    {
        display.printf("Relay: %s", boilerState ? "On " : "Off");
    }

    // Show remaining time
    display.setCursor(3, 13);
    if (timeLeftSec > 0)
    {
        int h = timeLeftSec / 3600;
        int mm = (timeLeftSec % 3600) / 60;
        int ss = timeLeftSec % 60;
        display.printf("Time R: %d:%02d:%02d", h, mm, ss);
    }
    else
    {
        display.printf("");
    }

    display.display();
}

void ShowDisplay()
{
    displayTicker.detach();                                                // Stop the ticker to prevent multiple calls
    display.ssd1306_command(SSD1306_DISPLAYON);                            // Turn on the display
    displayTicker.attach(displaySettings.onTimeSec.get(), ShowDisplayOff); // Reattach the ticker to turn off the display after the specified time
    displayActive = true;
}

void ShowDisplayOff()
{
    displayTicker.detach();                      // Stop the ticker to prevent multiple calls
    display.ssd1306_command(SSD1306_DISPLAYOFF); // Turn off the display
    // display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area

    if (displaySettings.turnDisplayOff.get())
    {
        displayActive = false;
    }
}

void updateStatusLED(){
    // ------------------------------------------------------------------
    // Status LED using Blinker: select patterns based on WiFi state
    //  - AP mode: fast blink (100ms on / 100ms off)
    //  - Connected: heartbeat (60ms on every 2s)
    //  - Connecting/disconnected: double blink every ~1s
    // Patterns are scheduled on state change; timing is handled by Blinker::loopAll().
    // ------------------------------------------------------------------

    static int lastMode = -1; // 1=AP, 2=CONNECTED, 3=CONNECTING

    const bool connected = ConfigManager.getWiFiManager().isConnected();
    const bool apMode = ConfigManager.getWiFiManager().isInAPMode();

    const int mode = apMode ? 1 : (connected ? 2 : 3);
    if (mode == lastMode) return; // no change
    lastMode = mode;

    switch (mode)
    {
    case 1: // AP mode: 100/100 continuous
        buildinLED.repeat(/*count*/ 1, /*frequencyMs*/ 200, /*gapMs*/ 0);
        break;
    case 2: // Connected: 60ms ON heartbeat every 2s -> 120ms pulse + 1880ms gap
        //mooved into send mqtt function to have heartbeat with mqtt messages
        // buildinLED.repeat(/*count*/ 1, /*frequencyMs*/ 120, /*gapMs*/ 1880);
        break;
    case 3: // Connecting: double blink every ~1s -> two 200ms pulses + 600ms gap
        buildinLED.repeat(/*count*/ 3, /*frequencyMs*/ 200, /*gapMs*/ 600);
        break;
    }
}

//----------------------------------------
// WIFI MANAGER CALLBACK FUNCTIONS
//----------------------------------------

bool SetupStartWebServer()
{
    lmg.log(LL::Info, "[MAIN] Starting Webserver...");

    ConfigManager.startWebServer();
    ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());

    return !ConfigManager.getWiFiManager().isInAPMode();
}

void onWiFiConnected()
{
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    ShowDisplay();

    lmg.log(LL::Info, "[MAIN] WiFi connected");
    lmg.log(LL::Info, "[MAIN] Station Mode: http://%s", WiFi.localIP().toString().c_str());
    lmg.log(LL::Info, "[MAIN] WLAN strength: %d dBm", WiFi.RSSI());
}

void onWiFiDisconnected()
{
    wifiServices.onDisconnected();
    ShowDisplay();
    lmg.log(LL::Warn, "[MAIN] WiFi disconnected");
}

void onWiFiAPMode()
{
    wifiServices.onAPMode();
    ShowDisplay();
    lmg.log(LL::Warn, "[MAIN] AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}

//----------------------------------------
// Shower request handler (UI/MQTT helper)
//----------------------------------------
static void handleShowerRequest(bool v)
{
    willShowerRequested = v;
    if (v) {
        if (boilerTimeRemaining <= 0) {
            int mins = boilerSettings.boilerTimeMin.get();
            if (mins <= 0) mins = 60;
            boilerTimeRemaining = mins * 60;
        }
        setBoilerState(true);
        ShowDisplay();
        if (mqtt.isConnected() && !topicWillShower.isEmpty()) {
            mqtt.publish(topicWillShower.c_str(), "1", true);
        }
    } else {
        // user canceled
        boilerTimeRemaining = 0;
        setBoilerState(false);
        if (mqtt.isConnected() && !topicWillShower.isEmpty()) {
            mqtt.publish(topicWillShower.c_str(), "0", true);
        }
    }
}
