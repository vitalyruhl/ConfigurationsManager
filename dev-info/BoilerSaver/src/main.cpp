#include <Arduino.h>
#include <esp_task_wdt.h> // for watchdog timer
#include <Ticker.h>
#include "Wire.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "settings.h"
#include "logging/logging.h"
#include "helpers/helpers.h"
#include "helpers/relays.h"
#include "helpers/mqtt_manager.h"
// Time/NTP support
#include <time.h>
// DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
// New non-blocking blinker utility
#include "binking/Blinker.h"

 #include "secret/wifiSecret.h"

// predeclare the functions (prototypes)
void SetupStartDisplay();
void setupGUI();
void setupMQTT();
void cb_publishToMQTT();
void cb_MQTT_GotMessage(char *topic, byte *message, unsigned int length);
void cb_MQTTListener();
void WriteToDisplay();
void SetupCheckForResetButton();
void SetupCheckForAPModeButton();
bool SetupStartWebServer();
void CheckButtons();
void ShowDisplay();
void ShowDisplayOff();
void updateStatusLED();
void PinSetup();
void handeleBoilerState(bool forceON = false);
void UpdateBoilerAlarmState();
static void cb_readTempSensor();
static void setupTempSensor();
static void handleShowerRequest(bool requested);

// Shorthand helper for RuntimeManager access
static inline ConfigManagerRuntime& CRM() { return ConfigManager.getRuntimeManager(); }

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

Helpers helpers;
MQTTManager mqttManager; // Global MQTT Manager instance

Ticker PublischMQTTTicker;
Ticker PublischMQTTTSettingsTicker;
Ticker ListenMQTTTicker;
Ticker displayTicker;
Ticker TempReadTicker;
Ticker NtpSyncTicker;
// Ticker WillShowerResetTicker; // no longer used (WillShower acts as switch)

// globale helpers variables
float temperature = 70.0;    // current temperature in Celsius
int boilerTimeRemaining = 0; // remaining time for boiler in SECONDS
bool boilerState = false;    // current state of the heater (on/off)

bool tickerActive = false; // flag to indicate if the ticker is active
bool displayActive = true; // flag to indicate if the display is active

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

    LoggerSetupSerial(); // Initialize the serial logger
    // currentLogLevel = SIGMALOG_DEBUG; //overwrite the default SIGMALOG_INFO level to debug to see all messages
    sl->Info("[SETUP] System setup start...");

    ConfigManager.setAppName(APP_NAME);
    ConfigManager.setVersion(APP_VERSION);                                                  // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1); // Register global CSS override
    ConfigManager.enableBuiltinSystemProvider();                                          // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);

    sl->Info("[SETUP] Load configuration...");
    initializeAllSettings(); // Register all settings BEFORE loading

    // Setup MQTT publishing callbacks for boiler settings changes via web GUI
    boilerSettings.setupCallbacks();

    ConfigManager.loadAll();

    //----------------------------------------------------------------------------------------------------------------------------------
    // set wifi settings if not set yet from my secret folder
    if (wifiSettings.wifiSsid.get().isEmpty())
    {
        Serial.println("-------------------------------------------------------------");
        Serial.println("SETUP: *** SSID is empty, setting My values *** ");
        Serial.println("-------------------------------------------------------------");
        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);
        wifiSettings.staticIp.set(MY_WIFI_IP);
        wifiSettings.useDhcp.set(false);
        ConfigManager.saveAll();
        delay(1000); // Small delay
    }
    // Serial.println(ConfigManager.toJSON(false)); // Print full configuration JSON to console

    PinSetup(); // Setup GPIO pins for buttons ToDO: move it to Relays and rename it in GPIOSetup()
    sl->Debug("[SETUP] Check for reset/AP button...");
    SetupCheckForResetButton();
    SetupCheckForAPModeButton();

    
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
    ConfigManager.setWifiAPMacPriority("e0-08-55-92-55-ac");   // Prefer this AP, fallback to others


    // init modules...
    sl->Info("[SETUP] init modules...");
    SetupStartDisplay();
    ShowDisplay();

    // Initialize temperature sensor and start periodic reads
    setupTempSensor();

    //----------------------------------------

    bool startedInStationMode = SetupStartWebServer();
    sl->Printf("[SETUP] SetupStartWebServer returned: %s", startedInStationMode ? "true" : "false").Debug();
    sl->Debug("[SETUP] Station mode");
    // Skip MQTT and OTA setup in AP mode (for initial configuration only)
    if (startedInStationMode)
    {
        setupMQTT();
    }
    else
    {
    sl->Debug("[SETUP] Skipping MQTT setup in AP mode");
    sll->Debug("MQTT disabled");
    }

    setupGUI();
    // Enhanced WebSocket configuration
    ConfigManager.enableWebSocketPush(); // Enable WebSocket push for real-time updates
    ConfigManager.setWebSocketInterval(1000); // Faster updates - every 1 second
    ConfigManager.setPushOnConnect(true);     // Immediate data on client connect

    //---------------------------------------------------------------------------------------------------
    sl->Info("[SETUP] System setup completed.");
    sll->Info("Setup completed.");
}

void loop()
{
    CheckButtons();
    boilerState = Relays::getBoiler(); // get Relay state

    
    //-------------------------------------------------------------------------------------------------------------
    // for working with the ConfigManager nessesary in loop
    ConfigManager.updateLoopTiming(); // Update internal loop timing metrics for system provider
    ConfigManager.getWiFiManager().update(); // Update WiFi Manager - handles all WiFi logic
    ConfigManager.handleClient(); // Handle web server client requests
    ConfigManager.handleWebsocketPush(); // Handle WebSocket push updates
    ConfigManager.handleOTA();           // Handle OTA updates
    ConfigManager.handleRuntimeAlarms(); // Handle runtime alarms
    //-------------------------------------------------------------------------------------------------------------


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

    mqttManager.loop(); // Handle MQTT Manager loop

    // Monitor MQTT connection status and log periodically
    bool currentMqttState = mqttManager.isConnected();
    if (currentMqttState != lastMqttConnectedState) {
        // State changed - log immediately
        if (currentMqttState) {
            sl->Printf("[MAIN] MQTT reconnected - Uptime: %lu ms, Reconnect count: %d",
                       mqttManager.getUptime(), mqttManager.getReconnectCount()).Info();
        } else {
            sl->Printf("[MAIN] MQTT connection lost - State: %d, Retry: %d",
                       (int)mqttManager.getState(), mqttManager.getCurrentRetry()).Warn();
        }
        lastMqttConnectedState = currentMqttState;
        lastMqttStatusLog = millis();
    } else if (millis() - lastMqttStatusLog > 60000) { // Log status every 60 seconds
        if (currentMqttState) {
            sl->Printf("[MAIN] MQTT status: Connected, Uptime: %lu ms", mqttManager.getUptime()).Debug();
        } else {
            sl->Printf("[MAIN] MQTT status: Disconnected, State: %d, Retry: %d/%d",
                       (int)mqttManager.getState(), mqttManager.getCurrentRetry(), 15).Debug();
        }
        lastMqttStatusLog = millis();
    }

    // Advance boiler/timer logic once per second (function is self-throttled)
    handeleBoilerState(false);

    updateStatusLED();       // Schedule LED patterns if WiFi state changed
    Blinker::loopAll();      // Advance all blinker state machines
    delay(10); // Small delay
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
            o["Bo_EN"] = Relays::getBoiler();
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
            bool canShower = (temperature >= boilerSettings.offThreshold.get());
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
            sl->Printf("[MAIN] Temperature manually set to %.1f°C via slider", v).Debug();
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
        sl->Printf("[MAIN] [ALARM] Temperature %.1f°C -> %s", temperature, globalAlarmState ? "HEATER ON" : "HEATER OFF").Debug();
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
        const bool wasOn = Relays::getBoiler();
        const int prevTime = boilerTimeRemaining;

        // Temperature-based auto control: turn off when upper threshold reached, allow turn-on when below lower threshold
        if (Relays::getBoiler()) {
            if (temperature >= boilerSettings.offThreshold.get()) {
                Relays::setBoiler(false);
                if (stopOnTarget) {
                    boilerTimeRemaining = 0;
                    if (willShowerRequested) {
                        willShowerRequested = false;
                        if (mqttManager.isConnected()) {
                            mqttManager.publish(mqttSettings.topicWillShower.c_str(), "0", true);
                        }
                    }
                }
            }
        } else {
            if ((boilerSettings.enabled.get() || forceON) && (temperature <= boilerSettings.onThreshold.get()) && (boilerTimeRemaining > 0)) {
                Relays::setBoiler(true);
            }
        }


        if (boilerSettings.enabled.get() || forceON)
        {
            if (boilerTimeRemaining > 0)
            {
                if (!Relays::getBoiler())
                {
                    Relays::setBoiler(true); // Turn on the boiler
                }
                boilerTimeRemaining--; // count down in seconds
            }
            else
            {
                if (Relays::getBoiler())
                {
                    Relays::setBoiler(false); // Turn off the boiler
                }
            }
        }
        else
        {
            if (Relays::getBoiler())
            {
                Relays::setBoiler(false); // Turn off the boiler if disabled
            }
        }

        // Detect timer end transition to 0 -> clear WillShower and publish retained OFF
        if (prevTime > 0 && boilerTimeRemaining <= 0) {
            if (willShowerRequested) {
                willShowerRequested = false;
                if (mqttManager.isConnected()) {
                    mqttManager.publish(mqttSettings.topicWillShower.c_str(), "0", true);
                }
            }
            if (Relays::getBoiler()) {
                Relays::setBoiler(false);
            }
        }
    }
}

void PinSetup()
{
    analogReadResolution(12); // Use full 12-bit resolution
    pinMode(buttonSettings.resetDefaultsPin.get(), INPUT_PULLUP);
    pinMode(buttonSettings.apModePin.get(), INPUT_PULLUP);
    if (buttonSettings.showerRequestPin.get() > 0) {
        pinMode(buttonSettings.showerRequestPin.get(), INPUT_PULLUP);
    }
    Relays::initPins();
    Relays::setBoiler(false); // Force known OFF state
}


static void cb_readTempSensor() {
    if (!ds18) {
        sl->Warn("[TEMP] DS18B20 sensor not initialized");
        return;
    }
    ds18->requestTemperatures();
    float t = ds18->getTempCByIndex(0);
    sl->Printf("[TEMP] Raw sensor reading: %.2f°C", t).Debug();

    // Check for sensor fault (-127°C indicates sensor error)
    bool sensorError = (t <= -127.0f || t >= 85.0f); // DS18B20 valid range is -55°C to +125°C, but -127°C is error code

    if (sensorError) {
        if (!sensorFaultState) {
            sensorFaultState = true;
            CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, true, false);
            sl->Printf("[TEMP] SENSOR FAULT detected! Reading: %.2f°C", t).Error();
        }
        sl->Printf("[TEMP] Invalid temperature reading: %.2f°C (sensor fault)", t).Warn();
        // Try to check if sensor is still present
        uint8_t deviceCount = ds18->getDeviceCount();
        sl->Printf("[TEMP] Devices still found: %d", deviceCount).Debug();
    } else {
        // Clear sensor fault if it was set
        if (sensorFaultState) {
            sensorFaultState = false;
            CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, false, false);
            sl->Printf("[TEMP] Sensor fault cleared! Reading: %.2f°C", t).Info();
        }

        temperature = t + tempSensorSettings.corrOffset.get();
        sl->Printf("[TEMP] Temperature updated: %.2f°C (offset: %.2f°C)", temperature, tempSensorSettings.corrOffset.get()).Info();
        // Optionally: push alarms now
        // CRM().updateAlarms(); // cheap
    }
}

static void setupTempSensor() {
    int pin = tempSensorSettings.gpioPin.get();
    if (pin <= 0) {
        sl->Warn("[TEMP] DS18B20 GPIO pin not set or invalid -> skipping init");
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
    sl->Printf("[TEMP] OneWire devices found: %d", deviceCount).Info();

    if (deviceCount == 0) {
        sl->Info("[TEMP] No DS18B20 sensors found! Check:");
        sl->Info("[TEMP] 1. Pull-up resistor (4.7kΩ) between VCC and GPIO");
        sl->Info("[TEMP] 2. Wiring: VCC→3.3V, GND→GND, DATA→GPIO");
        sl->Info("[TEMP] 3. Sensor connection and power");

        // Set sensor fault alarm if no devices found
        sensorFaultState = true;
        CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, true, false);
        sl->Printf("[TEMP] Sensor fault alarm activated - no devices found").Warn();
    } else {
        sl->Printf("[TEMP] Found %d DS18B20 sensor(s) on GPIO %d", deviceCount, pin).Info();

        // Clear sensor fault alarm if devices are found
        sensorFaultState = false;
        CRM().setRuntimeAlarmActive(SENSOR_FAULT_ALARM_ID, false, false);

        // Check if sensor is using parasitic power
        bool parasitic = ds18->readPowerSupply(0);
        sl->Printf("[TEMP] Power mode: %s", parasitic ? "Normal (VCC connected)" : "Parasitic (VCC=GND)").Info();

        // Set resolution to 12-bit for better accuracy
        ds18->setResolution(12);
        sl->Printf("[TEMP] Resolution set to 12-bit").Info();
    }

    float intervalSec = (float)tempSensorSettings.readInterval.get();
    if (intervalSec < 1.0f) intervalSec = 30.0f;
    TempReadTicker.attach(intervalSec, cb_readTempSensor);
    sl->Printf("[TEMP] DS18B20 initialized on GPIO %d, interval %.1fs, offset %.2f°C", pin, intervalSec, tempSensorSettings.corrOffset.get()).Info();
}

//----------------------------------------
// MQTT PUBLISHING HELPER FOR SETTINGS CHANGES VIA WEB GUI
//----------------------------------------
void BoilerSettings::publishSettingToMQTT(const String& settingName, const String& value) {
    extern MQTTManager mqttManager;
    extern MQTT_Settings mqttSettings;
    extern SigmaLoger *sl;

    if (mqttManager.isConnected()) {
        String topic = mqttSettings.Publish_Topic.get() + "/Settings/" + settingName;
        sl->Printf("[MAIN] GUI Change: Publishing to topic [%s] (length: %d)", topic.c_str(), topic.length()).Debug();
        mqttManager.publish(topic.c_str(), value, true);
        sl->Printf("[MAIN] GUI Change: Published %s = %s to MQTT", settingName.c_str(), value.c_str()).Debug();
    }
}

//----------------------------------------
// MQTT FUNCTIONS
//----------------------------------------
void setupMQTT()
{
    // -- Setup MQTT connection --
    sl->Printf("[MAIN] Starting MQTT! [%s]", mqttSettings.mqtt_server.get().c_str()).Info();
    sll->Printf("Starting MQTT! [%s]", mqttSettings.mqtt_server.get().c_str()).Info();

    // Test network connectivity to MQTT broker before attempting connection
    String mqttHost = mqttSettings.mqtt_server.get();
    uint16_t mqttPort = static_cast<uint16_t>(mqttSettings.mqtt_port.get());

    sl->Printf("[MAIN] Testing connectivity to MQTT broker %s:%d", mqttHost.c_str(), mqttPort).Debug();

    WiFiClient testClient;
    bool canConnect = testClient.connect(mqttHost.c_str(), mqttPort);
    if (canConnect) {
        testClient.stop();
        sl->Printf("[MAIN] Network connectivity to MQTT broker: OK").Info();
    } else {
        sl->Printf("[MAIN] Network connectivity to MQTT broker: FAILED").Warn();
        sl->Printf("[MAIN] Check if MQTT broker is running and accessible").Warn();
    }

    mqttSettings.updateTopics();

    // Configure MQTT Manager with improved stability settings
    mqttManager.setServer(mqttSettings.mqtt_server.get().c_str(), static_cast<uint16_t>(mqttSettings.mqtt_port.get()));
    mqttManager.setCredentials(mqttSettings.mqtt_username.get().c_str(), mqttSettings.mqtt_password.get().c_str());

    // Create unique client ID with MAC address, timestamp and random number to avoid conflicts
    String macAddr = WiFi.macAddress();
    macAddr.replace(":", "");
    uint32_t chipId = ESP.getEfuseMac() & 0xFFFFFF;
    String clientId = "ESP32_" + macAddr + "_" + String(chipId, HEX) + "_" + String(millis());
    mqttManager.setClientId(clientId.c_str());

    // Improved connection parameters for stability
    mqttManager.setKeepAlive(90);        // Longer keep-alive to avoid timeouts
    mqttManager.setMaxRetries(15);       // More retries for flaky networks
    mqttManager.setRetryInterval(10000); // Longer interval between retries
    mqttManager.setBufferSize(512);      // Larger buffer for message handling

    sl->Printf("[MAIN] MQTT Client ID: %s", clientId.c_str()).Debug();
    sl->Printf("[MAIN] MQTT Credentials: User=%s, Pass=%s",
               mqttSettings.mqtt_username.get().c_str(),
               mqttSettings.mqtt_password.get().length() > 0 ? "***" : "none").Debug();

    // Set MQTT callbacks
    mqttManager.onConnected([]()
                            {
                                sl->Debug("[MAIN] MQTT Connected! Subscribing to command topics...");
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topicSetShowerTime.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topicSetShowerTime.c_str());
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topicWillShower.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topicWillShower.c_str());
                                // Subscribe to bidirectional boiler settings topics
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topic_BoilerEnabled.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topic_BoilerEnabled.c_str());
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topic_OnThreshold.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topic_OnThreshold.c_str());
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topic_OffThreshold.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topic_OffThreshold.c_str());

                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topic_StopTimerOnTarget.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topic_StopTimerOnTarget.c_str());
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topic_OncePerPeriod.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topic_OncePerPeriod.c_str());
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topic_YouCanShowerPeriodMin.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topic_YouCanShowerPeriodMin.c_str());
                                sl->Printf("[MAIN] Subscribe to: %s", mqttSettings.topicSave.c_str()).Debug();
                                mqttManager.subscribe(mqttSettings.topicSave.c_str());

                                // // Clean up any potentially corrupted retained messages from previous versions
                                // String baseSettings = mqttSettings.Publish_Topic.get() + "/Settings/";
                                // mqttManager.publish((baseSettings + "StopTimerOnTarg11").c_str(), "", true); // Clear corrupted topic
                                // sl->Info("[MAIN] Cleared potential corrupted retained messages");

                                // One-time retained propagation of all relevant topics (on cold start)
                                if (!didStartupMQTTPropagate) {
                                    // Compute derived status - only notify if boiler is/was actively heating
                                    youCanShowerNow = (temperature >= boilerSettings.offThreshold.get()) && Relays::getBoiler();

                                    // Publish current statuses retained
                                    mqttManager.publish(mqttSettings.mqtt_publish_AktualBoilerTemperature.c_str(), String(temperature), /*retained*/ true);
                                    {
                                        int total = max(0, boilerTimeRemaining);
                                        int h = total / 3600;
                                        int m = (total % 3600) / 60;
                                        int s = total % 60;
                                        char buf[12];
                                        snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
                                        mqttManager.publish(mqttSettings.mqtt_publish_AktualTimeRemaining_topic.c_str(), String(buf), /*retained*/ true);
                                    }
                                    mqttManager.publish(mqttSettings.mqtt_publish_AktualState.c_str(), String(Relays::getBoiler()), /*retained*/ true);
                                    mqttManager.publish(mqttSettings.mqtt_publish_YouCanShowerNow_topic.c_str(), youCanShowerNow ? "1" : "0", /*retained*/ true);
                                    // Publish current Boiler settings retained (state reflection)
                                    mqttManager.publish(mqttSettings.topic_BoilerEnabled.c_str(), boilerSettings.enabled.get() ? "1" : "0", true);
                                    mqttManager.publish(mqttSettings.topic_OnThreshold.c_str(), String(boilerSettings.onThreshold.get()), true);
                                    mqttManager.publish(mqttSettings.topic_OffThreshold.c_str(), String(boilerSettings.offThreshold.get()), true);

                                    mqttManager.publish(mqttSettings.topic_StopTimerOnTarget.c_str(), boilerSettings.stopTimerOnTarget.get() ? "1" : "0", true);
                                    mqttManager.publish(mqttSettings.topic_OncePerPeriod.c_str(), boilerSettings.onlyOncePerPeriod.get() ? "1" : "0", true);

                                    didStartupMQTTPropagate = true;
                                    sl->Info("[MAIN] Published retained MQTT startup state");
                                }
                                cb_publishToMQTT(); // Publish initial values
                            });

    mqttManager.onDisconnected([]() {
        sl->Printf("[MAIN] MQTT disconnected - Retry count: %d, Uptime was: %lu ms",
                   mqttManager.getCurrentRetry(), mqttManager.getUptime()).Warn();
        sll->Printf("MQTT disconnected - Will retry connection").Warn();
    });

    mqttManager.onMessage([](char *topic, byte *payload, unsigned int length) { cb_MQTT_GotMessage(topic, payload, length); });

    mqttManager.begin();
}

// Compute current period ID for once-per-period gating
static long getCurrentPeriodId()
{
    const long periodMin = max(1, boilerSettings.boilerTimeMin.get());
    const long periodSec = periodMin * 60L;
    // Prefer NTP time if available (epoch > 1 Jan 1971 makes it likely)
    time_t now = time(nullptr);
    if (now > 24 * 60 * 60) {
        return now / periodSec;
    }
    // Fallback: millis-based coarse period
    return (long)((millis() / 1000UL) / periodSec);
}

void cb_publishToMQTT()
{
    if (mqttManager.isConnected())
    {
        // sl->Debug("[MAIN] cb_publishToMQTT: Publishing to MQTT...");
        mqttManager.publish(mqttSettings.mqtt_publish_AktualBoilerTemperature.c_str(), String(temperature));
    int total = max(0, boilerTimeRemaining);
    int h = total / 3600;
    int m = (total % 3600) / 60;
    int s = total % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%d:%02d:%02d", h, m, s);
    mqttManager.publish(mqttSettings.mqtt_publish_AktualTimeRemaining_topic.c_str(), String(buf));
        mqttManager.publish(mqttSettings.mqtt_publish_AktualState.c_str(), String(Relays::getBoiler()));
        // Publish 'You can shower now' status based on off-threshold AND relay state
        // Only notify when temp is reached AND boiler was actually heating (relay on)
        const bool canShower = (temperature >= boilerSettings.offThreshold.get()) && Relays::getBoiler();
        youCanShowerNow = canShower; // keep in-sync for UI/runtime
        if (!boilerSettings.onlyOncePerPeriod.get()) {
            // legacy behavior: always publish current state
            mqttManager.publish(mqttSettings.mqtt_publish_YouCanShowerNow_topic.c_str(), canShower ? "1" : "0");
            lastPublishedYouCanShower = canShower;
        } else {
            const long pid = getCurrentPeriodId();
            if (canShower) {
                if (pid != lastYouCanShower1PeriodId) {
                    // First time this period -> publish '1' and remember period
                    mqttManager.publish(mqttSettings.mqtt_publish_YouCanShowerNow_topic.c_str(), "1", true);
                    lastYouCanShower1PeriodId = pid;
                    lastPublishedYouCanShower = true;
                }
                // else: already sent '1' this period -> suppress repeat
            } else {
                // Optionally publish '0' so dashboards can reset
                if (lastPublishedYouCanShower != false) {
                    mqttManager.publish(mqttSettings.mqtt_publish_YouCanShowerNow_topic.c_str(), "0", true);
                    lastPublishedYouCanShower = false;
                }
            }
        }
        // Note: Settings are only published on startup or when explicitly changed via MQTT/GUI
        // No periodic republishing to avoid confusion between ESP32 GUI and HA
        buildinLED.repeat(/*count*/ 1, /*frequencyMs*/ 100, /*gapMs*/ 1500);
    }
}

void cb_MQTT_GotMessage(char *topic, byte *message, unsigned int length)
{
    if (topic == nullptr || message == nullptr) {
        sl->Warn("[MAIN] MQTT callback with null pointer - ignored");
        return;
    }

    // Additional safety check for empty or invalid length
    if (length == 0) {
        sl->Warn("[MAIN] MQTT callback with zero length message - ignored");
        return;
    }

    String messageTemp((char *)message, length); // Convert byte array to String using constructor
    messageTemp.trim();                          // Remove leading and trailing whitespace

    sl->Printf("[MAIN] <-- MQTT: Topic[%s] <-- [%s]", topic, messageTemp.c_str()).Debug();

    // Debug: Print all registered topics
    sl->Printf("[MAIN] DEBUG: Comparing with SetShowerTime: %s", mqttSettings.topicSetShowerTime.c_str()).Debug();
    sl->Printf("[MAIN] DEBUG: Comparing with WillShower: %s", mqttSettings.topicWillShower.c_str()).Debug();
    sl->Printf("[MAIN] DEBUG: Comparing with BoilerEnabled: %s", mqttSettings.topic_BoilerEnabled.c_str()).Debug();

    if (strcmp(topic, mqttSettings.topicSetShowerTime.c_str()) == 0)
    {
        // check if it is a number, if not set it to 0
        if (messageTemp.equalsIgnoreCase("null") ||
            messageTemp.equalsIgnoreCase("undefined") ||
            messageTemp.equalsIgnoreCase("NaN") ||
            messageTemp.equalsIgnoreCase("Infinity") ||
            messageTemp.equalsIgnoreCase("-Infinity"))
        {
            sl->Printf("[MAIN] Received invalid value from MQTT: %s", messageTemp.c_str()).Warn();
            messageTemp = "0";
        }
        // Interpret payload as minutes to keep boiler ON
        int mins = messageTemp.toInt();
        if (mins > 0) {
            boilerTimeRemaining = mins * 60;
            willShowerRequested = true;
            if (!Relays::getBoiler()) {
                Relays::setBoiler(true);
            }
            ShowDisplay();
            sl->Printf("[MAIN] MQTT set shower time: %d min (relay ON)", mins).Debug();
            if (mqttManager.isConnected()) {
                mqttManager.publish(mqttSettings.topicWillShower.c_str(), "1", true);
            }
        }
    }
    else if (strcmp(topic, mqttSettings.topicWillShower.c_str()) == 0)
    {
        // Boolean-like arming: 'I will shower' -> start timer with configured minutes
        bool willShower = messageTemp.equalsIgnoreCase("1") || messageTemp.equalsIgnoreCase("true") || messageTemp.equalsIgnoreCase("on");
        if (willShower == willShowerRequested) {
            // No state change -> ignore to avoid echo loops
            return;
        }
        if (willShower) {
            int mins = boilerSettings.boilerTimeMin.get();
            if (mins <= 0) mins = 60;
            if (boilerTimeRemaining <= 0) {
                boilerTimeRemaining = mins * 60;
            }
            willShowerRequested = true;
            if (!Relays::getBoiler()) {
                Relays::setBoiler(true);
            }
            ShowDisplay();
            sl->Printf("[MAIN] HA request: will shower -> set %d min (relay ON)", mins).Debug();
        } else {
            willShowerRequested = false;
            boilerTimeRemaining = 0;
            if (Relays::getBoiler()) {
                Relays::setBoiler(false);
            }
            sl->Debug("[MAIN] HA request: will shower = false -> timer cleared, relay OFF");
        }
    }
    // Boiler settings updates via MQTT
    else if (strcmp(topic, mqttSettings.topic_BoilerEnabled.c_str()) == 0) {
        bool v = messageTemp.equalsIgnoreCase("1") || messageTemp.equalsIgnoreCase("true") || messageTemp.equalsIgnoreCase("on");
        boilerSettings.enabled.set(v);
        sl->Printf("[MAIN] MQTT: BoilerEnabled set to %s", v ? "true" : "false").Debug();
    }
    else if (strcmp(topic, mqttSettings.topic_OnThreshold.c_str()) == 0) {
        float v = messageTemp.toFloat();
        if (v > 0) {
            boilerSettings.onThreshold.set(v);
            sl->Printf("[MAIN] MQTT: OnThreshold set to %.1f", v).Debug();
        }
    }
    else if (strcmp(topic, mqttSettings.topic_OffThreshold.c_str()) == 0) {
        float v = messageTemp.toFloat();
        if (v > 0) {
            boilerSettings.offThreshold.set(v);
            sl->Printf("[MAIN] MQTT: OffThreshold set to %.1f", v).Debug();
        }
    }
    else if (strcmp(topic, mqttSettings.topic_BoilerTimeMin.c_str()) == 0) {
        int v = messageTemp.toInt();
        if (v >= 0) {
            boilerSettings.boilerTimeMin.set(v);
            sl->Printf("[MAIN] MQTT: BoilerTimeMin set to %d", v).Debug();
            lastYouCanShower1PeriodId = -1; lastPublishedYouCanShower = false;
        }
    }
    else if (strcmp(topic, mqttSettings.topic_StopTimerOnTarget.c_str()) == 0) {
        bool v = messageTemp.equalsIgnoreCase("1") || messageTemp.equalsIgnoreCase("true") || messageTemp.equalsIgnoreCase("on");
        boilerSettings.stopTimerOnTarget.set(v);
        sl->Printf("[MAIN] MQTT: StopTimerOnTarget set to %s", v ? "true" : "false").Debug();

    }
    else if (strcmp(topic, mqttSettings.topic_OncePerPeriod.c_str()) == 0) {
        bool v = messageTemp.equalsIgnoreCase("1") || messageTemp.equalsIgnoreCase("true") || messageTemp.equalsIgnoreCase("on");
        boilerSettings.onlyOncePerPeriod.set(v);
        sl->Printf("[MAIN] MQTT: OncePerPeriod set to %s", v ? "true" : "false").Debug();
        lastYouCanShower1PeriodId = -1; lastPublishedYouCanShower = false;
    }
    else if (strcmp(topic, mqttSettings.topic_YouCanShowerPeriodMin.c_str()) == 0) {
        // Map incoming period to Boiler Max Heating Time for compatibility
        int v = messageTemp.toInt();
        if (v <= 0) v = 45; // default
        boilerSettings.boilerTimeMin.set(v);
        sl->Printf("[MAIN] MQTT: YouCanShowerPeriodMin mapped to BoilerTimeMin = %d", v).Debug();
        lastYouCanShower1PeriodId = -1; lastPublishedYouCanShower = false;

    }
    else if (strcmp(topic, mqttSettings.topicSave.c_str()) == 0) {
        // Persist all current settings
        ConfigManager.saveAll();
        if (mqttManager.isConnected()) mqttManager.publish(mqttSettings.topicSave.c_str(), "OK", false);
        sl->Info("[MAIN] Settings saved via MQTT");
    }
    else {
        sl->Printf("[MAIN] MQTT: Topic [%s] not recognized - ignored", topic).Warn();
    }
}

void cb_MQTTListener()
{
    mqttManager.loop(); // process MQTT connection and incoming messages
}

//----------------------------------------
// HELPER FUNCTIONS
//----------------------------------------

void SetupCheckForResetButton()
{
    // check for pressed reset button
    if (digitalRead(buttonSettings.resetDefaultsPin.get()) == LOW)
    {
    sl->Internal("[MAIN] Reset button pressed -> Reset all settings...");
    sll->Internal("Reset!");
        ConfigManager.clearAllFromPrefs(); // Clear all settings from EEPROM
        ConfigManager.saveAll();           // Save the default settings to EEPROM

        // Show user feedback that reset is happening
    sll->Internal("restarting...");
        //ToDo: add non blocking delay to show message on display before restart
        ESP.restart(); // Restart the ESP32
    }
}

void SetupCheckForAPModeButton()
{
    String APName = "ESP32_Config";
    String pwd = "config1234"; // Default AP password

    if (wifiSettings.wifiSsid.get().length() == 0)
    {
    sl->Printf("[MAIN] WiFi SSID is empty [%s] (fresh/unconfigured)", wifiSettings.wifiSsid.get().c_str()).Error();
        ConfigManager.startAccessPoint(APName, ""); // Only SSID and password
    }

    // check for pressed AP mode button

    if (digitalRead(buttonSettings.apModePin.get()) == LOW)
    {
    sl->Internal("[MAIN] AP mode button pressed -> starting AP mode...");
    sll->Internal("AP mode button!");
        ConfigManager.startAccessPoint(APName, ""); // Only SSID and password
    }
}

void CheckButtons()
{
    static bool lastResetButtonState = HIGH;
    static bool lastAPButtonState = HIGH;
    static bool lastShowerButtonState = HIGH;
    static unsigned long lastButtonCheck = 0;
    static unsigned long resetPressStart = 0;
    static bool resetHandled = false;

    // Debounce: only check buttons every 50ms
    if (millis() - lastButtonCheck < 50)
    {
        return;
    }
    lastButtonCheck = millis();

    bool currentResetState = digitalRead(buttonSettings.resetDefaultsPin.get());
    bool currentAPState = digitalRead(buttonSettings.apModePin.get());
    bool currentShowerState = HIGH;
    if (buttonSettings.showerRequestPin.get() > 0) {
        currentShowerState = digitalRead(buttonSettings.showerRequestPin.get());
    }

    // Check for button press (transition from HIGH to LOW)
    if (lastResetButtonState == HIGH && currentResetState == LOW)
    {
        sl->Debug("[MAIN] Reset-Button pressed -> Start Display Ticker...");
        ShowDisplay();
    }

    if (lastAPButtonState == HIGH && currentAPState == LOW)
    {
        sl->Debug("[MAIN] AP-Mode-Button pressed -> Start Display Ticker...");
        ShowDisplay();
    }

    // Shower request press (toggle on/off)
    if (buttonSettings.showerRequestPin.get() > 0) {
        if (lastShowerButtonState == HIGH && currentShowerState == LOW) {
            // Toggle the shower request state
            bool newState = !willShowerRequested;
            sl->Printf("[MAIN] Shower button pressed -> toggling shower request to %s", newState ? "ON" : "OFF").Debug();
            ShowDisplay();
            handleShowerRequest(newState);
        }
        lastShowerButtonState = currentShowerState;
    }

    lastResetButtonState = currentResetState;
    lastAPButtonState = currentAPState;

    // Detect long press on reset button to restore defaults at runtime
    if (currentResetState == LOW)
    {
        if (resetPressStart == 0)
        {
            resetPressStart = millis();
        }
        else if (!resetHandled && millis() - resetPressStart >= resetHoldDurationMs)
        {
            resetHandled = true;
            sl->Internal("[MAIN] Reset button long-press detected -> restoring defaults");
            sll->Internal("restoring defaults");
            ConfigManager.clearAllFromPrefs();
            ConfigManager.saveAll();
            delay(3000); // Small delay to allow message to be seen
            ESP.restart();
        }
    }
    else
    {
        resetPressStart = 0;
        resetHandled = false;
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
    sl->Info("[MAIN] Starting Webserver...!");
    sll->Info("Starting Webserver...!");

    if (WiFi.getMode() == WIFI_AP)
    {
        return false; // Skip webserver setup in AP mode
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        if (wifiSettings.useDhcp.get())
        {
            sl->Debug("[MAIN] startWebServer: DHCP enabled");
            ConfigManager.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
        }
        else
        {
            sl->Debug("[MAIN] startWebServer: DHCP disabled - using static IP");
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
        }
    }

    return true; // Webserver setup completed
}

void onWiFiConnected()
{
    sl->Info("[MAIN] WiFi connected! Activating services...");
    sll->Info("WiFi connected!");

    if (!tickerActive)
    {
        ShowDisplay(); // Show the display

        // Start MQTT tickers
        PublischMQTTTicker.attach(mqttSettings.MQTTPublischPeriod.get(), cb_publishToMQTT);
        ListenMQTTTicker.attach(mqttSettings.MQTTListenPeriod.get(), cb_MQTTListener);

        // Start OTA if enabled
        if (systemSettings.allowOTA.get())
        {
            sll->Debug("Start OTA-Module");
            ConfigManager.setupOTA(APP_NAME, systemSettings.otaPassword.get().c_str());
        }

        tickerActive = true;
    }
    sl->Printf("\n\n[MAIN] Webserver running at: %s\n", WiFi.localIP().toString().c_str()).Info();
    sll->Printf("IP: %s\n\n", WiFi.localIP().toString().c_str()).Info();
    sl->Printf("[MAIN] WLAN-Strength: %d dBm\n", WiFi.RSSI()).Info();
    sl->Printf("[MAIN] WLAN-Strength is: %s\n\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak")).Info();
    sll->Printf("WLAN: %s\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak")).Info();

    // Start NTP sync now and schedule periodic resyncs
    auto doNtpSync = [](){
        // Use TZ-aware sync for correct local time (Berlin: CET/CEST)
        configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str());
    };
    doNtpSync();
    NtpSyncTicker.detach();
    int ntpInt = ntpSettings.frequencySec.get();
    if (ntpInt < 60) ntpInt = 3600; // default to 1 hour
    NtpSyncTicker.attach(ntpInt, +[](){
        configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str());
    });
}

void onWiFiDisconnected()
{
    sl->Debug("[MAIN] WiFi disconnected! Deactivating services...");
    sll->Warn("WiFi lost connection!");

    if (tickerActive)
    {
        ShowDisplay(); // Show the display to indicate WiFi is lost

        // Stop MQTT tickers
        PublischMQTTTicker.detach();
        ListenMQTTTicker.detach();

        // Stop OTA if it should be disabled
        if (systemSettings.allowOTA.get() == false && ConfigManager.isOTAInitialized())
        {
            sll->Debug("Stop OTA-Module");
            ConfigManager.stopOTA();
        }

        tickerActive = false;
    }
}

void onWiFiAPMode()
{
    sl->Warn("[MAIN] WiFi in AP mode");
    sll->Warn("AP mode!");

    // Ensure services are stopped in AP mode
    if (tickerActive)
    {
        onWiFiDisconnected(); // Reuse disconnected logic
    }
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
        Relays::setBoiler(true);
        ShowDisplay();
        if (mqttManager.isConnected()) {
            mqttManager.publish(mqttSettings.topicWillShower.c_str(), "1", true);
        }
    } else {
        // user canceled
        boilerTimeRemaining = 0;
        Relays::setBoiler(false);
        if (mqttManager.isConnected()) {
            mqttManager.publish(mqttSettings.topicWillShower.c_str(), "0", true);
        }
    }
}
