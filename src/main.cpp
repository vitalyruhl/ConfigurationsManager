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
h3 { color: orange; text-decoration: underline; }
)CSS";

Helpers helpers;
MQTTManager mqttManager; // Global MQTT Manager instance

Ticker PublischMQTTTicker;
Ticker PublischMQTTTSettingsTicker;
Ticker ListenMQTTTicker;
Ticker displayTicker;

// globale helpers variables
float temperature = 70.0;    // current temperature in Celsius
int boilerTimeRemaining = 0; // remaining time for boiler in minutes
bool boilerState = false;    // current state of the heater (on/off)

bool tickerActive = false; // flag to indicate if the ticker is active
bool displayActive = true; // flag to indicate if the display is active

static bool globalAlarmState = false;// Global alarm state for temperature monitoring
static constexpr char TEMP_ALARM_ID[] = "temp_low";

static unsigned long lastDisplayUpdate = 0; // Non-blocking display update management
static const unsigned long displayUpdateInterval = 100; // Update display every 100ms
static const unsigned long resetHoldDurationMs = 3000; // Require 3s hold to factory reset

#pragma endregion configuration variables

//----------------------------------------
// MAIN FUNCTIONS
//----------------------------------------

void setup()
{

    LoggerSetupSerial(); // Initialize the serial logger
    currentLogLevel = SIGMALOG_DEBUG; //overwrite the default SIGMALOG_INFO level to debug to see all messages
    sl->Info("[SETUP] System setup start...");

    ConfigManager.setAppName(APP_NAME);                                                   // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1); // Register global CSS override
    ConfigManager.enableBuiltinSystemProvider();                                          // enable the builtin system provider (uptime, freeHeap, rssi etc.)

    sl->Info("[SETUP] Load configuration...");
    initializeAllSettings(); // Register all settings BEFORE loading
    ConfigManager.loadAll();

    // set wifi settings if not set yet from my secret folder
        if (wifiSettings.wifiSsid.get().isEmpty()){
            sl->Debug ("-------------------------------------------------------------");
            sl->Debug ("SETUP: *** SSID is empty, setting My values *** ");
            sl->Debug ("-------------------------------------------------------------");
            // ConfigManager.clearAllFromPrefs();
            wifiSettings.wifiSsid.set( MY_WIFI_SSID );
            wifiSettings.wifiPassword.set( MY_WIFI_PASSWORD );
            ConfigManager.saveAll();
            delay(1000); // Small delay

        }


    // Debug: Print some settings after loading
    sl->Debug ("[SETUP] === LOADED SETTINGS (Important) ===");
    sl->Printf("[SETUP] WiFi SSID: '%s' (length: %d)", wifiSettings.wifiSsid.get().c_str(), wifiSettings.wifiSsid.get().length()).Debug();
    sl->Printf("[SETUP] WiFi Password:  (length: %d)", wifiSettings.wifiPassword.get().length()).Debug();
    sl->Printf("[SETUP] WiFi Use DHCP: %s", wifiSettings.useDhcp.get() ? "true" : "false").Debug();
    sl->Printf("[SETUP] WiFi Static IP: '%s'", wifiSettings.staticIp.get().c_str()).Debug();
    sl->Printf("[SETUP] WiFi Gateway: '%s'", wifiSettings.gateway.get().c_str()).Debug();
    sl->Printf("[SETUP] WiFi Subnet: '%s'", wifiSettings.subnet.get().c_str()).Debug();
    sl->Printf("[SETUP] WiFi DNS1: '%s'", wifiSettings.dnsPrimary.get().c_str()).Debug();
    sl->Printf("[SETUP] WiFi DNS2: '%s'", wifiSettings.dnsSecondary.get().c_str()).Debug();
    sl->Debug ("[SETUP] === END SETTINGS ===");

    ConfigManager.checkSettingsForErrors(); // Check for any settings errors and report them in console

    // Serial.println(ConfigManager.toJSON(false)); // Print full configuration JSON to console

    PinSetup(); // Setup GPIO pins for buttons ToDO: move it to Relays and rename it in GPIOSetup()
    sl->Debug("[SETUP] Check for reset/AP button...");
    SetupCheckForResetButton();
    SetupCheckForAPModeButton();

    // init modules...
    sl->Info("[SETUP] init modules...");
    SetupStartDisplay();
    ShowDisplay();

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
    sll->Debug("[SETUP] AP mode - MQTT disabled");
    }

    setupGUI();
    ConfigManager.enableWebSocketPush(); // Enable WebSocket push for real-time updates
    //---------------------------------------------------------------------------------------------------
    sl->Info("[SETUP] System setup completed.");
    sll->Info("[SETUP] Setup completed.");
}

void loop()
{
    CheckButtons();
    boilerState = Relays::getBoiler(); // get Relay state

    ConfigManager.getWiFiManager().update(); // Update WiFi Manager - handles all WiFi logic

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

    ConfigManager.handleClient();
    ConfigManager.handleWebsocketPush();
    ConfigManager.getOTAManager().handle();
    ConfigManager.updateLoopTiming(); // Update internal loop timing metrics for system provider
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
    RuntimeFieldMeta systemAppMeta{};
    systemAppMeta.group = "system";
    systemAppMeta.key = "app_name";
    systemAppMeta.label = "application";
    systemAppMeta.isString = true;
    systemAppMeta.staticValue = String(APP_NAME);
    systemAppMeta.order = 0;
    CRM().addRuntimeMeta(systemAppMeta);

    RuntimeFieldMeta systemVersionMeta{};
    systemVersionMeta.group = "system";
    systemVersionMeta.key = "app_version";
    systemVersionMeta.label = "version";
    systemVersionMeta.isString = true;
    systemVersionMeta.staticValue = String(VERSION);
    systemVersionMeta.order = 1;
    CRM().addRuntimeMeta(systemVersionMeta);

    RuntimeFieldMeta systemBuildMeta{};
    systemBuildMeta.group = "system";
    systemBuildMeta.key = "build_date";
    systemBuildMeta.label = "build date";
    systemBuildMeta.isString = true;
    systemBuildMeta.staticValue = String(VERSION_DATE);
    systemBuildMeta.order = 2;
    CRM().addRuntimeMeta(systemBuildMeta);

    // Runtime live values provider
    CRM().addRuntimeProvider("Boiler",
        [](JsonObject &o)
        {
            o["Bo_EN_Set"] = boilerSettings.enabled.get();
            o["Bo_EN"] = Relays::getBoiler();
            o["Bo_Temp"] = temperature;
            o["Bo_SettedTime"] = boilerSettings.boilerTimeMin.get();
            o["Bo_TimeLeft"] = boilerTimeRemaining;
        });

    // Add metadata for Boiler provider fields
    // Show whether boiler control is enabled (setting) and actual relay state
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_EN_Set", .label = "enabled", .precision = 0, .order = 1, .isBool = true});
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_EN", .label = "relay on", .precision = 0, .order = 2, .isBool = true});
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_Temp", .label = "temperature", .unit = "°C", .precision = 1, .order = 10});
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_TimeLeft", .label = "time left", .unit = "min", .precision = 0, .order = 21});
    CRM().addRuntimeMeta({.group = "Boiler", .key = "Bo_SettedTime", .label = "time setted", .unit = "min", .precision = 0, .order = 22});

    // Add alarms provider for min Temperature monitoring with hysteresis
    CRM().registerRuntimeAlarm(TEMP_ALARM_ID);
    CRM().addRuntimeProvider("Alarms",
        [](JsonObject &o)
        {
            o["AL_Status"] = globalAlarmState;
            o["Current_Temp"] = temperature;
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

    // show some Info
    CRM().addRuntimeMeta({.group = "Alarms", .key = "Current_Temp", .label = "current temp", .unit = "°C", .precision = 1, .order = 100});
    CRM().addRuntimeMeta({.group = "Alarms", .key = "On_Threshold", .label = "on threshold", .unit = "°C", .precision = 1, .order = 101});
    CRM().addRuntimeMeta({.group = "Alarms", .key = "Off_Threshold", .label = "off threshold", .unit = "°C", .precision = 1, .order = 102});

    // Temperature slider for testing (initialize with current temperature value)
    CRM().addRuntimeProvider("Hand overrides", [](JsonObject &o) { }, 100);

    static float transientFloatVal = temperature; // Initialize with current temperature
    ConfigManager.defineRuntimeFloatSlider("Hand overrides", "f_adj", "Temperature Test", -10.0f, 100.0f, temperature, 1, []()
        { return transientFloatVal; }, [](float v)
        { transientFloatVal = v;
            temperature = v;
            sl->Printf("[MAIN] Temperature manually set to %.1f°C via slider", v).Debug();
        }, String("°C"));

    // State button to manually control the boiler relay
    static bool stateBtnState = false;
    ConfigManager.defineRuntimeStateButton("Hand overrides", "sb_mode", "Will Duschen", []()
        { return stateBtnState; }, [](bool v) { stateBtnState = v;  Relays::setBoiler(v); }, /*init*/ false);

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


        //todo: add check estimated time etc to handle Boiler
        //todo: check on/of temp to activate/deactivate boiler


        if (boilerSettings.enabled.get() || forceON)
        {
            if (boilerTimeRemaining > 0)
            {
                if (!Relays::getBoiler())
                {
                    Relays::setBoiler(true); // Turn on the boiler
                }
                boilerTimeRemaining--;
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
    }
}

void PinSetup()
{
    analogReadResolution(12); // Use full 12-bit resolution
    pinMode(buttonSettings.resetDefaultsPin.get(), INPUT_PULLUP);
    pinMode(buttonSettings.apModePin.get(), INPUT_PULLUP);
    Relays::initPins();
    Relays::setBoiler(false); // Force known OFF state
}

//----------------------------------------
// MQTT FUNCTIONS
//----------------------------------------
void setupMQTT()
{
    // -- Setup MQTT connection --
    sl->Printf("[MAIN] Starting MQTT! [%s]", mqttSettings.mqtt_server.get().c_str()).Info();
    sll->Printf("[MAIN] Starting MQTT! [%s]", mqttSettings.mqtt_server.get().c_str()).Info();

    mqttSettings.updateTopics();

    // Configure MQTT Manager
    mqttManager.setServer(mqttSettings.mqtt_server.get().c_str(), static_cast<uint16_t>(mqttSettings.mqtt_port.get()));
    mqttManager.setCredentials(mqttSettings.mqtt_username.get().c_str(), mqttSettings.mqtt_password.get().c_str());
    mqttManager.setClientId(("ESP32_" + String(WiFi.macAddress())).c_str());
    mqttManager.setMaxRetries(10);
    mqttManager.setRetryInterval(5000);

    // Set MQTT callbacks
    mqttManager.onConnected([]()
                            {
                                sl->Debug("[MAIN] Ready to subscribe to MQTT topics...");
                                mqttManager.subscribe(mqttSettings.mqtt_Settings_SetState_topic.get().c_str());
                                cb_publishToMQTT(); // Publish initial values
                            });

    mqttManager.onDisconnected([]() { sl->Warn("[MAIN] MQTT disconnected"); });

    mqttManager.onMessage([](char *topic, byte *payload, unsigned int length) { cb_MQTT_GotMessage(topic, payload, length); });

    mqttManager.begin();
}

void cb_publishToMQTT()
{
    if (mqttManager.isConnected())
    {
        // sl->Debug("[MAIN] cb_publishToMQTT: Publishing to MQTT...");
        mqttManager.publish(mqttSettings.mqtt_publish_AktualBoilerTemperature.c_str(), String(temperature));
        mqttManager.publish(mqttSettings.mqtt_publish_AktualTimeRemaining_topic.c_str(), String(boilerTimeRemaining));
        mqttManager.publish(mqttSettings.mqtt_publish_AktualState.c_str(), String(boilerState));
        buildinLED.repeat(/*count*/ 1, /*frequencyMs*/ 250, /*gapMs*/ 1500);
    }
}

void cb_MQTT_GotMessage(char *topic, byte *message, unsigned int length)
{
    String messageTemp((char *)message, length); // Convert byte array to String using constructor
    messageTemp.trim();                          // Remove leading and trailing whitespace

    sl->Printf("[MAIN] <-- MQTT: Topic[%s] <-- [%s]", topic, messageTemp.c_str()).Debug();
    if (strcmp(topic, mqttSettings.mqtt_Settings_SetState_topic.get().c_str()) == 0)
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
    sll->Internal("[MAIN] Reset button pressed!");
        ConfigManager.clearAllFromPrefs(); // Clear all settings from EEPROM
        ConfigManager.saveAll();           // Save the default settings to EEPROM

        // Show user feedback that reset is happening
    sll->Internal("[MAIN] Settings reset complete - restarting...");
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
    sll->Internal("[MAIN] AP mode button!");
    sll->Internal("[MAIN] -> starting AP mode...");
        ConfigManager.startAccessPoint(APName, ""); // Only SSID and password
    }
}


void CheckButtons()
{
    static bool lastResetButtonState = HIGH;
    static bool lastAPButtonState = HIGH;
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
            sll->Internal("[MAIN] Reset button -> restoring defaults");
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
    static int lastTimeRemaining = -1;
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
    if (abs(temperature - lastTemperature) > 0.1 ||
        boilerTimeRemaining != lastTimeRemaining ||
        boilerState != lastBoilerState)
    {
        needsUpdate = true;
        lastTemperature = temperature;
        lastTimeRemaining = boilerTimeRemaining;
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
        display.printf("Boiler: %s | T:%.1f ", boilerState ? "ON " : "OFF", temperature);
        display.write((uint8_t)248); // degree symbol in CP437
        display.print("C");
    }
    else
    {
        display.printf("Boiler: %s", boilerState ? "ON " : "OFF");
    }

    // Show remaining time
    display.setCursor(3, 13);
    if (boilerTimeRemaining > 0)
    {
        display.printf("Time left: %d min", boilerTimeRemaining);
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
    sll->Info("[MAIN] Starting Webserver...!");

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
    sll->Info("[MAIN] WiFi connected!");

    if (!tickerActive)
    {
        ShowDisplay(); // Show the display

        // Start MQTT tickers
        PublischMQTTTicker.attach(mqttSettings.MQTTPublischPeriod.get(), cb_publishToMQTT);
        ListenMQTTTicker.attach(mqttSettings.MQTTListenPeriod.get(), cb_MQTTListener);

        // Start OTA if enabled
        if (systemSettings.allowOTA.get())
        {
            sll->Debug("[MAIN] Start OTA-Module");
            ConfigManager.setupOTA(APP_NAME, systemSettings.otaPassword.get().c_str());
        }

        tickerActive = true;
    }
    sl->Printf("\n\n[MAIN] Webserver running at: %s\n", WiFi.localIP().toString().c_str()).Info();
    sll->Printf("[MAIN] IP: %s\n\n", WiFi.localIP().toString().c_str()).Info();
    sl->Printf("[MAIN] WLAN-Strength: %d dBm\n", WiFi.RSSI()).Info();
    sl->Printf("[MAIN] WLAN-Strength is: %s\n\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak")).Info();
    sll->Printf("[MAIN] WLAN: %s\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak")).Info();
}

void onWiFiDisconnected()
{
    sl->Debug("[MAIN] WiFi disconnected! Deactivating services...");
    sll->Warn("[MAIN] WiFi lost connection!");
    sll->Warn("[MAIN] deactivate mqtt ticker.");

    if (tickerActive)
    {
        ShowDisplay(); // Show the display to indicate WiFi is lost

        // Stop MQTT tickers
        PublischMQTTTicker.detach();
        ListenMQTTTicker.detach();

        // Stop OTA if it should be disabled
        if (systemSettings.allowOTA.get() == false && ConfigManager.isOTAInitialized())
        {
            sll->Debug("[MAIN] Stop OTA-Module");
            ConfigManager.stopOTA();
        }

        tickerActive = false;
    }
}

void onWiFiAPMode()
{
    sl->Warn("[MAIN] WiFi in AP mode");
    sll->Warn("[MAIN] Running in AP mode!");

    // Ensure services are stopped in AP mode
    if (tickerActive)
    {
        onWiFiDisconnected(); // Reuse disconnected logic
    }
}
