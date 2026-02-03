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
#include "helpers/HelperModule.h"

#include "core/CoreSettings.h"
#include "core/CoreWiFiServices.h"
#include "io/IOManager.h"
#include "logging/LoggingManager.h"

#define CM_MQTT_NO_DEFAULT_HOOKS
#include "mqtt/MQTTManager.h"
#include "mqtt/MQTTLogOutput.h"

#if __has_include("secret/wifiSecret.h")
  #include "secret/wifiSecret.h"
  #define CM_HAS_WIFI_SECRETS 1
#else
  #define CM_HAS_WIFI_SECRETS 0
#endif

// predeclare the functions (prototypes)
static void setupLogging();
static void setupGUI();
static void setupMQTT();
static void updateMqttTopics();
static void setupMqttCallbacks();
static void handleMqttMessage(const char* topic, const char* payload, unsigned int length);
static void publishMqttState(bool retained);
static void publishMqttStateIfNeeded();
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

// Global pulse helper: built-in LED
static cm::helpers::PulseOutput buildinLED(LED_BUILTIN, cm::helpers::PulseOutput::ActiveLevel::ActiveHigh);

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
static cm::CoreWiFiSettings &wifiSettings = coreSettings.wifi;
static cm::MQTTManager::Settings &mqttSettings = mqtt.settings();
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
    lmg.scopedTag("SETUP");
    lmg.log("System setup start...");

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setAppTitle(APP_NAME);
    ConfigManager.setVersion(APP_VERSION);
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);
    ConfigManager.enableBuiltinSystemProvider();

    // Layout hints keep the Settings tab organized; WiFi/System/NTP are handled by coreSettings.
    ConfigManager.addSettingsPage("I2C", 40);
    ConfigManager.addSettingsGroup("I2C", "I2C", "I2C Bus", 40);
    ConfigManager.addSettingsPage("Boiler", 50);
    ConfigManager.addSettingsGroup("Boiler", "Boiler", "Boiler Control", 50);
    ConfigManager.addSettingsPage("Display", 60);
    ConfigManager.addSettingsGroup("Display", "Display", "Display Options", 60);
    ConfigManager.addSettingsPage("Temp Sensor", 70);
    ConfigManager.addSettingsGroup("Temp Sensor", "Temp Sensor", "Temperature Sensor", 70);
    ConfigManager.addSettingsPage(cm::CoreCategories::IO, 80);

    coreSettings.attachWiFi(ConfigManager);
    coreSettings.attachSystem(ConfigManager);
    coreSettings.attachNtp(ConfigManager);

    systemSettings.allowOTA.setCallback([](bool enabled) {
        lmg.log(LL::Info, "Setting changed to: %s", enabled ? "enabled" : "disabled");
        ConfigManager.getOTAManager().enable(enabled);
    });

    initializeAllSettings();
    registerIOBindings();

    setupMQTT();

    ConfigManager.loadAll();
    delay(100); // Small delay


    //add my wifi credentials if not set
    if (wifiSettings.wifiSsid.get().isEmpty())
    {
#if CM_HAS_WIFI_SECRETS
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "SETUP: *** SSID is empty, setting My values *** ");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);
        wifiSettings.useDhcp.set(MY_USE_DHCP);
        wifiSettings.staticIp.set(MY_WIFI_IP);
        wifiSettings.gateway.set(MY_GATEWAY_IP);
        wifiSettings.subnet.set(MY_SUBNET_MASK);
        wifiSettings.dnsPrimary.set(MY_DNS_IP);
        ConfigManager.saveAll();
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "Restarting ESP, after auto setting WiFi credentials");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        delay(500); // Small delay - get time to flush log
        ESP.restart();
#else
        lmg.log(LL::Warn, "SETUP: SSID is empty but secret/wifiSecret.h is missing; configure WiFi via UI/AP mode");
#endif
    }

    
    // Re-attach to apply loaded values (attach() is idempotent)
    mqtt.attach(ConfigManager);

    //add my mqtt credentials if not set
    if (mqttSettings.server.get().isEmpty())
    {
#if CM_HAS_WIFI_SECRETS
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        lmg.log(LL::Debug, "SETUP: *** MQTT Broker is empty, setting My values *** ");
        lmg.log(LL::Debug, "-------------------------------------------------------------");
        mqttSettings.server.set(MY_MQTT_BROKER_IP);
        mqttSettings.port.set(MY_MQTT_BROKER_PORT);
        mqttSettings.username.set(MY_MQTT_USERNAME);
        mqttSettings.password.set(MY_MQTT_PASSWORD);
        mqttSettings.publishTopicBase.set(MY_MQTT_ROOT);
        ConfigManager.saveAll();
        lmg.log(LL::Debug, "-------------------------------------------------------------");
#else
        lmg.log(LL::Info, "SETUP: MQTT server is empty and secret/wifiSecret.h is missing; leaving MQTT unconfigured");
#endif
    }

    ConfigManager.getOTAManager().enable(systemSettings.allowOTA.get());

    ioManager.begin();
    
    updateMqttTopics();
    setupMqttCallbacks();
    setBoilerState(false);

    ConfigManager.addLivePage("Boiler", 10);
    ConfigManager.addLiveGroup("Boiler", "Live Values", "Boiler", 10);
    ConfigManager.addLivePage("Alarms", 20);
    ConfigManager.addLiveGroup("Alarms", "Live Values", "Alarms", 20);
    ConfigManager.addLivePage("mqtt", 30);
    ConfigManager.addLiveGroup("mqtt", "Live Values", "MQTT", 30);
    ConfigManager.addLivePage("system", 40);
    ConfigManager.addLiveGroup("system", "Live Values", "System", 40);

    setupGUI();

    ConfigManager.enableWebSocketPush();
    ConfigManager.setWebSocketInterval(1000);
    ConfigManager.setPushOnConnect(true);

    ConfigManager.enableSmartRoaming(true);
    ConfigManager.setRoamingThreshold(-75);
    ConfigManager.setRoamingCooldown(30);
    ConfigManager.setRoamingImprovement(10);

    // ConfigManager.setWifiAPMacPriority("e0-08-55-92-55-ac"); //boiler
    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5"); //office

    SetupStartDisplay();
    ShowDisplay();
    setupTempSensor();

    const bool startedInStationMode = SetupStartWebServer();

    lmg.log("System setup completed.");
}

void loop()
{
    lmg.scopedTag("loop");
    ConfigManager.updateLoopTiming();
    
    ConfigManager.getWiFiManager().update();
    boilerState = getBoilerState();
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
   
    handeleBoilerState(false);

    updateStatusLED();

    cm::helpers::PulseOutput::loopAll();

    delay(10);

}

//----------------------------------------
// PROJECT FUNCTIONS
//----------------------------------------

void setupGUI()
{
    lmg.scopedTag("setupGUI");
    CRM().addRuntimeProvider("Boiler",
        [](JsonObject &o)
        {
            o["Bo_EN_Set"] = boilerSettings.enabled->get();
            o["Bo_EN"] = getBoilerState();
            o["Bo_Temp"] = temperature;
            o["Bo_SettedTime"] = boilerSettings.boilerTimeMin->get();
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
            bool canShower = (temperature >= boilerSettings.offThreshold->get()) && getBoilerState();
            o["Bo_CanShower"] = canShower;
            youCanShowerNow = canShower; // keep MQTT status aligned
        });

    // Add metadata for Boiler provider fields
    // Show whether boiler control is enabled (setting) and actual relay state
    {
        RuntimeFieldMeta meta{};
        meta.group = "Boiler";
        meta.key = "Bo_EN_Set";
        meta.label = "Enabled";
        meta.precision = 0;
        meta.order = 1;
        meta.isBool = true;
        CRM().addRuntimeMeta(meta);
    }
    {
        RuntimeFieldMeta meta{};
        meta.group = "Boiler";
        meta.key = "Bo_EN";
        meta.label = "Relay On";
        meta.precision = 0;
        meta.order = 2;
        meta.isBool = true;
        CRM().addRuntimeMeta(meta);
    }
    {
        RuntimeFieldMeta meta{};
        meta.group = "Boiler";
        meta.key = "Bo_CanShower";
        meta.label = "You can shower now";
        meta.precision = 0;
        meta.order = 5;
        meta.isBool = true;
        CRM().addRuntimeMeta(meta);
    }
    {
        RuntimeFieldMeta meta{};
        meta.group = "Boiler";
        meta.key = "Bo_Temp";
        meta.label = "Temperature";
        meta.unit = "°C";
        meta.precision = 1;
        meta.order = 10;
        CRM().addRuntimeMeta(meta);
    }
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
    {
        RuntimeFieldMeta meta{};
        meta.group = "Boiler";
        meta.key = "Bo_SettedTime";
        meta.label = "Time Set";
        meta.unit = "min";
        meta.precision = 0;
        meta.order = 22;
        CRM().addRuntimeMeta(meta);
    }

    // Add alarms provider for min Temperature monitoring with hysteresis
    CRM().registerRuntimeAlarm(TEMP_ALARM_ID);
    CRM().registerRuntimeAlarm(SENSOR_FAULT_ALARM_ID);
    CRM().addRuntimeProvider("Alarms",
        [](JsonObject &o)
        {
            o["AL_Status"] = globalAlarmState;
            o["SF_Status"] = sensorFaultState;
            o["On_Threshold"] = boilerSettings.onThreshold->get();
            o["Off_Threshold"] = boilerSettings.offThreshold->get();
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
    {
        RuntimeFieldMeta meta{};
        meta.group = "Alarms";
        meta.key = "On_Threshold";
        meta.label = "Alarm Under Temperature";
        meta.unit = "°C";
        meta.precision = 1;
        meta.order = 101;
        CRM().addRuntimeMeta(meta);
    }
    {
        RuntimeFieldMeta meta{};
        meta.group = "Alarms";
        meta.key = "Off_Threshold";
        meta.label = "You can shower now temperature";
        meta.unit = "°C";
        meta.precision = 1;
        meta.order = 102;
        CRM().addRuntimeMeta(meta);
    }
    // Ensure interactive control is placed under Boiler group (project convention)

    // State button to manually control the boiler relay (under Boiler card)
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
    lmg.scopedTag("UpdateBoilerAlarmState");
    bool previousState = globalAlarmState;

    if (globalAlarmState)
    {
        if (temperature >= boilerSettings.onThreshold->get() + 2.0f)
        {
            globalAlarmState = false;
        }
    }
    else
    {
        if (temperature <= boilerSettings.onThreshold->get())
        {
            globalAlarmState = true;
        }
    }

    if (globalAlarmState != previousState)
    {
        //todo -a add a new LL:state:Alarm for alarms
        lmg.log(LL::Error, "Temperature %.1f°C -> %s",
                temperature, globalAlarmState ? "HEATER ON" : "HEATER OFF");
        CRM().setRuntimeAlarmActive(TEMP_ALARM_ID, globalAlarmState, false);
        handeleBoilerState(true); // Force boiler if the temperature is too low
    }
}

void handeleBoilerState(bool forceON)
{
    lmg.scopedTag("handeleBoilerState");
    static unsigned long lastBoilerCheck = 0;
    unsigned long now = millis();

    if (now - lastBoilerCheck >= 1000) // Check every second
    {
        lastBoilerCheck = now;
        const bool stopOnTarget = boilerSettings.stopTimerOnTarget->get();
        const int prevTime = boilerTimeRemaining;

        // When we force-enable the boiler (e.g. due to under-temperature alarm),
        // ensure we actually have a non-zero timer so the existing control logic can turn the relay on.
        if (forceON && boilerTimeRemaining <= 0)
        {
            int mins = boilerSettings.boilerTimeMin->get();
            if (mins <= 0) {
                mins = 1;
            }
            boilerTimeRemaining = mins * 60;
            lmg.log(LL::Warn, "Under-temperature alarm active -> starting heating timer: %d min", mins);
        }

        // Temperature-based auto control: turn off when upper threshold reached, allow turn-on when below lower threshold
        if (getBoilerState()) {
            if (temperature >= boilerSettings.offThreshold->get()) {
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
            if ((boilerSettings.enabled->get() || forceON) && (temperature <= boilerSettings.onThreshold->get()) && (boilerTimeRemaining > 0)) {
                setBoilerState(true);
            }
        }


        if (boilerSettings.enabled->get() || forceON)
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
    lmg.scopedTag("TEMP");
    if (!ds18) {
        lmg.log(LL::Warn, "DS18B20 sensor not initialized");
        return;
    }
    ds18->requestTemperatures();
    float t = ds18->getTempCByIndex(0);
    lmg.log(LL::Debug, "Raw sensor reading: %.2f°C", t);

    // Check for sensor fault (-127°C indicates sensor error)
    bool sensorError = (t <= -127.0f || t >= 85.0f); // DS18B20 valid range is -55°C to +125°C, but -127°C is error code

    if (sensorError) {
        if (!sensorFaultState) {
            sensorFaultState = true;
            CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, true, false);
            lmg.log(LL::Error, "SENSOR FAULT detected! Reading: %.2f°C", t);
        }
        lmg.log(LL::Warn, "Invalid temperature reading: %.2f°C (sensor fault)", t);
        // Try to check if sensor is still present
        uint8_t deviceCount = ds18->getDeviceCount();
        lmg.log(LL::Debug, "Devices still found: %d", deviceCount);
    } else {
        // Clear sensor fault if it was set
        if (sensorFaultState) {
            sensorFaultState = false;
            CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, false, false);
            lmg.log(LL::Debug,"Sensor fault cleared! Reading: %.2f°C", t);
        }

        temperature = t + tempSensorSettings.corrOffset->get();
        lmg.log(LL::Trace, "Temperature updated: %.2f°C (offset: %.2f°C)", temperature, tempSensorSettings.corrOffset->get());
        // Optionally: push alarms now
        // CRM().updateAlarms(); // cheap
    }
}

static void setupTempSensor() {
    lmg.scopedTag("SETUP/TEMP");
    int pin = tempSensorSettings.gpioPin->get();
    if (pin <= 0) {
        lmg.log(LL::Error, "DS18B20 GPIO pin not set or invalid -> skipping init");
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
    lmg.log(LL::Debug, "OneWire devices found: %d", deviceCount);

    if (deviceCount == 0) {
        lmg.log(LL::Debug, "No DS18B20 sensors found! Check:");
        lmg.log(LL::Debug, "1. Pull-up resistor (4.7kΩ) between VCC and GPIO");
        lmg.log(LL::Debug, "2. Wiring: VCC->3.3V, GND->GND, DATA->GPIO");
        lmg.log(LL::Debug, "3. Sensor connection and power");

        // Set sensor fault alarm if no devices found
        sensorFaultState = true;
        CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, true, false);
        lmg.log(LL::Warn, "Sensor fault alarm activated - no devices found");
    } else {
        lmg.log(LL::Info, "Found %d DS18B20 sensor(s) on GPIO %d", deviceCount, pin);

        // Clear sensor fault alarm if devices are found
        sensorFaultState = false;
        CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, false, false);

        // Check if sensor is using parasitic power
        bool parasitic = ds18->readPowerSupply(0);
        lmg.log(LL::Info, "Power mode: %s", parasitic ? "Normal (VCC connected)" : "Parasitic [4,7KΩ] (VCC=GND)");

        // Set resolution to 12-bit for better accuracy
        ds18->setResolution(12);
        lmg.log(LL::Info, "Resolution set to 12-bit");
    }

    float intervalSec = (float)tempSensorSettings.readInterval->get();
    if (intervalSec < 1.0f) intervalSec = 30.0f;
    TempReadTicker.attach(intervalSec, cb_readTempSensor);
    lmg.log(LL::Debug, "DS18B20 initialized on GPIO %d, interval %.1fs, offset %.2f°C",
            pin, intervalSec, tempSensorSettings.corrOffset->get());
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
    lmg.attachToConfigManager(LL::Info, LL::Trace, "");
}

static void registerIOBindings()
{
    lmg.scopedTag("IO");
    analogReadResolution(12);

    ioManager.addDigitalOutput(cm::IOManager::DigitalOutputBinding{
        .id = IO_BOILER_ID,
        .name = "Boiler Relay",
        .defaultPin = 23,
        .defaultActiveLow = true,
        .defaultEnabled = true,
    });
    ioManager.addIOtoGUI(IO_BOILER_ID, "Boiler Relay", 1);

    ioManager.addDigitalInput(cm::IOManager::DigitalInputBinding{
        .id = IO_RESET_ID,
        .name = "Reset Button",
        .defaultPin = 14,
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

    ioManager.addInputToGUI(IO_SHOWER_ID, nullptr, 100, "Shower HW-Btn", "Boiler", false);

    cm::IOManager::DigitalInputEventOptions resetOptions;
    resetOptions.longClickMs = resetHoldDurationMs;
    ioManager.configureDigitalInputEvents(
        IO_RESET_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []() {
                lmg.logTag(LL::Debug,"IO", "Reset button pressed -> show display");
                ShowDisplay();
            },
            .onLongPressOnStartup = []() {
                lmg.logTag(LL::Trace,"IO", "Reset button pressed at startup -> restoring defaults");
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
                lmg.logTag(LL::Debug,"IO", "AP button pressed -> show display");
                ShowDisplay();
            },
            .onLongPressOnStartup = []() {
                lmg.logTag(LL::Trace,"IO", "AP button pressed at startup -> starting AP mode");
                ConfigManager.startAccessPoint("ESP32_Config", "");
            },
        },
        apOptions);

    ioManager.configureDigitalInputEvents(
        IO_SHOWER_ID,
        cm::IOManager::DigitalInputEventCallbacks{
            .onPress = []() {
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

static void updateMqttTopics()
{
     lmg.scopedTag("updateMqttTopics");
    String base = mqtt.settings().publishTopicBase.get();
    if (base.isEmpty()) {
        base = mqtt.getMqttBaseTopic();
    }
    if (base.isEmpty()) {
        base = String(APP_NAME);//Base-Fallback
    }

    if (base != mqttBaseTopic) {
        mqttBaseTopic = base;
        didStartupMQTTPropagate = false;
    }

    topicActualState = mqttBaseTopic + "/ActualState";
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
    lmg.scopedTag("setupMqttCallbacks");
    boilerSettings.enabled->setCallback([](bool v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicBoilerEnabled.c_str(), v ? "1" : "0", true);
        }
    });

    boilerSettings.onThreshold->setCallback([](float v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicOnThreshold.c_str(), String(v), true);
        }
    });

    boilerSettings.offThreshold->setCallback([](float v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicOffThreshold.c_str(), String(v), true);
        }
    });

    boilerSettings.boilerTimeMin->setCallback([](int v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicBoilerTimeMin.c_str(), String(v), true);
            mqtt.publish(topicYouCanShowerPeriodMin.c_str(), String(v), true);
        }
        lastYouCanShower1PeriodId = -1;
        lastPublishedYouCanShower = false;
    });

    boilerSettings.stopTimerOnTarget->setCallback([](bool v) {
        if (mqtt.isConnected()) {
            mqtt.publish(topicStopTimerOnTarget.c_str(), v ? "1" : "0", true);
        }
    });

    boilerSettings.onlyOncePerPeriod->setCallback([](bool v) {
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
    const long periodMin = max(1, boilerSettings.boilerTimeMin->get());
    const long periodSec = periodMin * 60L;
    time_t now = time(nullptr);
    if (now > 24 * 60 * 60) {
        return now / periodSec;
    }
    return (long)((millis() / 1000UL) / periodSec);
}

static void publishMqttState(bool retained)
{
    lmg.scopedTag("publishMqttState");
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

    const bool canShower = (temperature >= boilerSettings.offThreshold->get()) && getBoilerState();
    youCanShowerNow = canShower;
    if (!boilerSettings.onlyOncePerPeriod->get()) {
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

    //TODO: add into IO-Manager Blinker support
    buildinLED.setPulseRepeat(/*count*/ 1, /*periodMs*/ 100, /*gapMs*/ 1500);
}

static void publishMqttStateIfNeeded()
{
    lmg.scopedTag("publishMqttStateIfNeeded");
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
    lmg.scopedTag("MQTT");
    if (!topic || !payload || length == 0) {
        lmg.log(LL::Warn, "Callback with invalid payload - ignored");
        return;
    }

    String messageTemp(payload, length);
    messageTemp.trim();

    lmg.log(LL::Debug, "Topic[%s] <-- [%s]", topic, messageTemp.c_str());

    if (strcmp(topic, topicSetShowerTime.c_str()) == 0) {
        if (messageTemp.equalsIgnoreCase("null") ||
            messageTemp.equalsIgnoreCase("undefined") ||
            messageTemp.equalsIgnoreCase("NaN") ||
            messageTemp.equalsIgnoreCase("Infinity") ||
            messageTemp.equalsIgnoreCase("-Infinity")) {
            lmg.log(LL::Warn, "Received invalid value from MQTT: %s", messageTemp.c_str());
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
            lmg.log(LL::Debug, "MQTT set shower time: %d min (relay ON)", mins);
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
            int mins = boilerSettings.boilerTimeMin->get();
            if (mins <= 0) mins = 60;
            if (boilerTimeRemaining <= 0) {
                boilerTimeRemaining = mins * 60;
            }
            willShowerRequested = true;
            if (!getBoilerState()) {
                setBoilerState(true);
            }
            ShowDisplay();
            lmg.log(LL::Debug, "HA request: will shower -> set %d min (relay ON)", mins);
        } else {
            willShowerRequested = false;
            boilerTimeRemaining = 0;
            if (getBoilerState()) {
                setBoilerState(false);
            }
            lmg.log(LL::Debug, "HA request: will shower = false -> timer cleared, relay OFF");
        }
        return;
    }

    if (strcmp(topic, topicBoilerEnabled.c_str()) == 0) {
        const bool v = messageTemp.equalsIgnoreCase("1") ||
                       messageTemp.equalsIgnoreCase("true") ||
                       messageTemp.equalsIgnoreCase("on");
        boilerSettings.enabled->set(v);
        lmg.log(LL::Debug, "BoilerEnabled set to %s", v ? "true" : "false");
        return;
    }

    if (strcmp(topic, topicOnThreshold.c_str()) == 0) {
        const float v = messageTemp.toFloat();
        if (v > 0) {
            boilerSettings.onThreshold->set(v);
            lmg.log(LL::Debug, "OnThreshold set to %.1f", v);
        }
        return;
    }

    if (strcmp(topic, topicOffThreshold.c_str()) == 0) {
        const float v = messageTemp.toFloat();
        if (v > 0) {
            boilerSettings.offThreshold->set(v);
            lmg.log(LL::Debug, "OffThreshold set to %.1f", v);
        }
        return;
    }

    if (strcmp(topic, topicBoilerTimeMin.c_str()) == 0) {
        const int v = messageTemp.toInt();
        if (v >= 0) {
            boilerSettings.boilerTimeMin->set(v);
            lmg.log(LL::Debug, "BoilerTimeMin set to %d", v);
            lastYouCanShower1PeriodId = -1;
            lastPublishedYouCanShower = false;
        }
        return;
    }

    if (strcmp(topic, topicStopTimerOnTarget.c_str()) == 0) {
        const bool v = messageTemp.equalsIgnoreCase("1") ||
                       messageTemp.equalsIgnoreCase("true") ||
                       messageTemp.equalsIgnoreCase("on");
        boilerSettings.stopTimerOnTarget->set(v);
        lmg.log(LL::Debug, "StopTimerOnTarget set to %s", v ? "true" : "false");
        return;
    }

    if (strcmp(topic, topicOncePerPeriod.c_str()) == 0) {
        const bool v = messageTemp.equalsIgnoreCase("1") ||
                       messageTemp.equalsIgnoreCase("true") ||
                       messageTemp.equalsIgnoreCase("on");
        boilerSettings.onlyOncePerPeriod->set(v);
        lmg.log(LL::Debug, "OncePerPeriod set to %s", v ? "true" : "false");
        lastYouCanShower1PeriodId = -1;
        lastPublishedYouCanShower = false;
        return;
    }

    if (strcmp(topic, topicYouCanShowerPeriodMin.c_str()) == 0) {
        int v = messageTemp.toInt();
        if (v <= 0) v = 45;
        boilerSettings.boilerTimeMin->set(v);
        lmg.log(LL::Debug, "YouCanShowerPeriodMin mapped to BoilerTimeMin = %d", v);
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

    lmg.log(LL::Warn, "Topic [%s] not recognized - ignored", topic);
}

namespace cm
{
    
    void onMQTTConnected()
    {
        lmg.scopedTag("MQTT");
        updateMqttTopics();
        lmg.log(LL::Info, "Connected");

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
        lmg.log(LL::Warn, "Disconnected");
    }

    void onMQTTStateChanged(int state)
    {
        auto mqttState = static_cast<MQTTManager::ConnectionState>(state);
        lmg.log(LL::Info, "State changed: %s", MQTTManager::mqttStateToString(mqttState));
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
    lmg.scopedTag("DISPLAY");
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

static void SetupStartDisplay()
{
    Wire.begin(i2cSettings.sdaPin->get(), i2cSettings.sclPin->get());
    Wire.setClock(static_cast<uint32_t>(i2cSettings.busFreq->get()));

    display.begin(SSD1306_SWITCHCAPVCC, i2cSettings.displayAddr->get());
    display.clearDisplay();
    display.drawRect(0, 0, 128, 24, WHITE);
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(10, 4);
    display.println("Start");
    display.display();
}

void ShowDisplay()
{
    displayTicker.detach();                                                // Stop the ticker to prevent multiple calls
    display.ssd1306_command(SSD1306_DISPLAYON);                            // Turn on the display
    displayTicker.attach(displaySettings.onTimeSec->get(), ShowDisplayOff); // Reattach the ticker to turn off the display after the specified time
    displayActive = true;
}

void ShowDisplayOff()
{
    displayTicker.detach();                      // Stop the ticker to prevent multiple calls
    display.ssd1306_command(SSD1306_DISPLAYOFF); // Turn off the display
    // display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area

    if (displaySettings.turnDisplayOff->get())
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
        buildinLED.setPulseRepeat(/*count*/ 1, /*periodMs*/ 200, /*gapMs*/ 0);
        break;
    case 2: // Connected: 60ms ON heartbeat every 2s -> 120ms pulse + 1880ms gap
        //mooved into send mqtt function to have heartbeat with mqtt messages
        // buildinLED.setPulseRepeat(/*count*/ 1, /*periodMs*/ 120, /*gapMs*/ 1880);
        break;
    case 3: // Connecting: double blink every ~1s -> two 200ms pulses + 600ms gap
        buildinLED.setPulseRepeat(/*count*/ 3, /*periodMs*/ 200, /*gapMs*/ 600);
        break;
    }
}

//----------------------------------------
// WIFI MANAGER CALLBACK FUNCTIONS
//----------------------------------------

bool SetupStartWebServer()
{
    lmg.scopedTag("MAIN/WIFI");
    lmg.log(LL::Info, "Starting Webserver...");

    ConfigManager.startWebServer();
    // Auto reboot timeout is configured via WiFiRb (handled inside ConfigManager.startWebServer()).

    return !ConfigManager.getWiFiManager().isInAPMode();
}

void onWiFiConnected()
{
    lmg.scopedTag("MAIN/WIFI");
    wifiServices.onConnected(ConfigManager, APP_NAME, systemSettings, ntpSettings);
    ShowDisplay();

    lmg.log(LL::Info, "WiFi connected");
    lmg.log(LL::Info, "Station Mode: http://%s", WiFi.localIP().toString().c_str());
    lmg.log(LL::Info, "WLAN strength: %d dBm", WiFi.RSSI());
}

void onWiFiDisconnected()
{
    lmg.scopedTag("MAIN/WIFI");
    wifiServices.onDisconnected();
    ShowDisplay();
    lmg.log(LL::Warn, "WiFi disconnected");
}

void onWiFiAPMode()
{
    lmg.scopedTag("MAIN/WIFI");
    wifiServices.onAPMode();
    ShowDisplay();
    lmg.log(LL::Warn, "AP Mode: http://%s", WiFi.softAPIP().toString().c_str());
}

//----------------------------------------
// Shower request handler (UI/MQTT helper)
//----------------------------------------
static void handleShowerRequest(bool v)
{
    lmg.scopedTag("handleShowerRequest");
    willShowerRequested = v;
    if (v) {
        if (boilerTimeRemaining <= 0) {
            int mins = boilerSettings.boilerTimeMin->get();
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
