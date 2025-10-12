#include <Arduino.h>
#include <esp_task_wdt.h> // for watchdog timer
#include <Ticker.h>
#include "Wire.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);

#include "settings.h"
#include "logging/logging.h"
#include "helpers/helpers.h"
#include "helpers/relays.h"
#include "helpers/mqtt_manager.h"

// predeclare the functions (prototypes)
void SetupStartDisplay();
void publishToMQTT();
void cb_MQTT(char *topic, byte *message, unsigned int length);
void publishToMQTT();
void cb_PublishToMQTT();
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
WiFiManager wifiManager; // Global WiFi Manager instance
MQTTManager mqttManager;     // Global MQTT Manager instance

Ticker PublischMQTTTicker;
Ticker PublischMQTTTSettingsTicker;
Ticker ListenMQTTTicker;
Ticker displayTicker;

// globale helpers variables
float temperature = 70.0;      // current temperature in Celsius
int boilerTimeRemaining = 0; // remaining time for boiler in minutes
bool boilerState = false;    // current state of the heater (on/off)

bool tickerActive = false;    // flag to indicate if the ticker is active
bool displayActive = true;   // flag to indicate if the display is active

// Global alarm state for temperature monitoring
static bool globalAlarmState = false;

// Non-blocking MQTT reconnection state management
// Non-blocking display update management
static unsigned long lastDisplayUpdate = 0;
static const unsigned long displayUpdateInterval = 100; // Update display every 100ms

#pragma endregion configuration variables

//----------------------------------------
// MAIN FUNCTIONS
//----------------------------------------

void setup()
{

  LoggerSetupSerial(); // Initialize the serial logger

  sl->Printf("System setup start...").Debug();

  cfg.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
  cfg.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);// Register global CSS override
  cfg.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)


  PinSetup();
  sl->Printf("Check for reset/AP button...").Debug();
  SetupCheckForResetButton();
  SetupCheckForAPModeButton();

  // sl->Printf("Clear Settings...").Debug();
  // cfg.clearAllFromPrefs();

  sl->Printf("Load configuration...").Debug();
  cfg.loadAll();

  cfg.checkSettingsForErrors();

  // Debug: Print boiler threshold settings
  sl->Printf("Boiler Settings Debug:").Debug();
  sl->Printf("  onThreshold: %.1f°C", boilerSettings.onThreshold.get()).Debug();
  sl->Printf("  offThreshold: %.1f°C", boilerSettings.offThreshold.get()).Debug();
  sl->Printf("  enabled: %s", boilerSettings.enabled.get() ? "true" : "false").Debug();

  // Re-apply relay pin modes with loaded settings (pins/polarity may differ from defaults)
  Relays::initPins();

  mqttSettings.updateTopics();

  // init modules...
  sl->Printf("init modules...").Debug();
  SetupStartDisplay();
  ShowDisplay();

  helpers.blinkBuidInLEDsetpinMode(); // Initialize the built-in LED pin mode

  sl->Printf("Configuration printout:").Debug();
  Serial.println(cfg.toJSON(false)); // Print the configuration to the serial monitor
  //----------------------------------------

  bool isStartedAsAP = SetupStartWebServer();

  //----------------------------------------
  // -- Setup MQTT connection --
  sl->Printf("⚠️ SETUP: Starting MQTT! [%s]", mqttSettings.mqtt_server.get().c_str()).Debug();
  sll->Printf("Starting MQTT! [%s]", mqttSettings.mqtt_server.get().c_str()).Debug();
  
  // Configure MQTT Manager
  mqttManager.setServer(mqttSettings.mqtt_server.get().c_str(), static_cast<uint16_t>(mqttSettings.mqtt_port.get()));
  mqttManager.setCredentials(mqttSettings.mqtt_username.get().c_str(), mqttSettings.mqtt_password.get().c_str());
  mqttManager.setClientId(("ESP32_" + String(WiFi.macAddress())).c_str());
  mqttManager.setMaxRetries(10);
  mqttManager.setRetryInterval(5000);
  
  // Set MQTT callbacks
  mqttManager.onConnected([]() {
    sl->Printf("Ready to subscribe to MQTT topics...").Debug();
    sl->Printf("Propagate initial boiler settings to MQTT...").Debug();
    // Subscribe to topics
    mqttManager.subscribe(mqttSettings.mqtt_Settings_SetState_topic.get().c_str());
    // Publish initial values
    publishToMQTT();
  });
  
  mqttManager.onDisconnected([]() {
    sl->Printf("MQTT disconnected callback triggered").Debug();
  });
  
  mqttManager.onMessage([](char* topic, byte* payload, unsigned int length) {
    cb_MQTT(topic, payload, length);
  });
  
  mqttManager.begin();

  sl->Debug("System setup completed.");
  sll->Debug("Setup completed.");

  // Initialize WiFi Manager
  wifiManager.begin(10000, systemSettings.wifiRebootTimeoutMin.get()); // 10s reconnect interval, auto-reboot timeout from settings
  wifiManager.setCallbacks(onWiFiConnected, onWiFiDisconnected, onWiFiAPMode);

  //---------------------------------------------------------------------------------------------------
  // Runtime live values provider for relay outputs
  cfg.addRuntimeProvider({
    .name = String("Boiler"),
    .fill = [] (JsonObject &o){
        o["Bo_EN_Set"] = boilerSettings.enabled.get();
        o["Bo_EN"] = Relays::getBoiler();
        o["Bo_SettedTime"] = boilerSettings.boilerTimeMin.get();
        o["Bo_TimeLeft"] = boilerTimeRemaining;
        o["Bo_Temp"] = temperature;

        // Show alarm status (check if temperature is below threshold)
        static bool alarmActive = false;
        if (temperature < 60.0f) {
            alarmActive = true;
        } else if (temperature > 65.0f) {
            alarmActive = false;
        }
        o["Bo_AlarmActive"] = alarmActive;
    }
  });

  cfg.defineRuntimeField("Boiler", "Bo_Temp", "temperature", "°C", 1, 10);
  cfg.defineRuntimeField("Boiler", "Bo_TimeLeft", "time left", "min", 1, 60);

  // Add alarm status to Boiler provider for better visibility
  cfg.defineRuntimeField("Boiler", "Bo_AlarmActive", "alarm active", "", 0, 1);


  // Add interactive controls Set-Boiler
  cfg.addRuntimeProvider({
      .name = "Hand overrides",
      .fill = [](JsonObject &o){ /* optionally expose current override states later */ }
  });

  static bool stateBtnState = false;
  cfg.defineRuntimeStateButton("Hand overrides", "sb_mode", "Will Duschen", [](){ return stateBtnState; }, [](bool v){
    stateBtnState = v;  Relays::setBoiler(v);}, /*init*/ false, 91);

  // Add alarms provider BEFORE defining alarms
  cfg.addRuntimeProvider({
      .name = "Alarms",
      .fill = [](JsonObject &o){
          // Same hysteresis logic as in the alarm
          if (globalAlarmState) {
              // Currently heating -> turn OFF when temp reaches offThreshold
              if (temperature >= boilerSettings.offThreshold.get()) {
                  globalAlarmState = false;
              }
          } else {
              // Currently not heating -> turn ON when temp falls below onThreshold
              if (temperature <= boilerSettings.onThreshold.get()) {
                  globalAlarmState = true;
              }
          }

          o["AL_Status"] = globalAlarmState;
          o["AL_LT"] = globalAlarmState;  // Set boolean control too
          o["Current_Temp"] = temperature;
          o["On_Threshold"] = boilerSettings.onThreshold.get();
          o["Off_Threshold"] = boilerSettings.offThreshold.get();
      }
  });

  // Define alarm controls and status fields
  cfg.defineRuntimeBool("Alarms", "AL_LT", "Temperature Low Alarm", false, /*order*/ 90);
  cfg.defineRuntimeField("Alarms", "AL_Status", "alarm triggered", "", 0, 1);
  cfg.defineRuntimeField("Alarms", "Current_Temp", "current temp", "°C", 1, 100);
  cfg.defineRuntimeField("Alarms", "On_Threshold", "on threshold", "°C", 1, 101);
  cfg.defineRuntimeField("Alarms", "Off_Threshold", "off threshold", "°C", 1, 102);

  // Define a runtime alarm to control the boiler based on temperature with hysteresis
  cfg.defineRuntimeAlarm(
              "temp_low",
              [](const JsonObject &root)
              {
                  // Alarm is always enabled - just return the global state
                  return globalAlarmState;
              },
              []()
              {
                  Serial.println("[ALARM] -> HEATER ON");
                  sl->Printf("[ALARM] Temperature %.1f°C -> HEATER ON", temperature).Info();
                  handeleBoilerState(true);
              },
              []()
              {
                  Serial.println("[ALARM] -> HEATER OFF");
                  sl->Printf("[ALARM] Temperature %.1f°C -> HEATER OFF", temperature).Info();
                  handeleBoilerState(false);
              });

  // Temperature slider for testing (initialize with current temperature value)
  static float transientFloatVal = temperature; // Initialize with current temperature
  cfg.defineRuntimeFloatSlider("Hand overrides", "f_adj", "Temperature Test", -10.0f, 100.0f, temperature, 1, [](){
        return transientFloatVal; }, [](float v){
            transientFloatVal = v;
            temperature = v;
            sl->Printf("Temperature manually set to %.1f°C via slider", v).Debug();
        }, 93, String("°C"));

  //---------------------------------------------------------------------------------------------------
}

void loop()
{
  CheckButtons();
  boilerState = Relays::getBoiler();

  // Update WiFi Manager - handles all WiFi logic
  wifiManager.update();

  // Non-blocking display updates
  if (millis() - lastDisplayUpdate > displayUpdateInterval) {
    lastDisplayUpdate = millis();
    WriteToDisplay();
  }

  // Evaluate cross-field runtime alarms periodically (cheap doc build ~ small JSON)
  static unsigned long lastAlarmEval = 0;
  if (millis() - lastAlarmEval > 1500)
  {
      lastAlarmEval = millis();
      cfg.handleRuntimeAlarms();
  }

  // Handle MQTT Manager loop
  mqttManager.loop();

  updateStatusLED();
  cfg.handleClient();
  cfg.handleWebsocketPush();
  cfg.handleOTA();
  cfg.updateLoopTiming(); // Update internal loop timing metrics for system provider
}

//----------------------------------------
// MQTT FUNCTIONS
//----------------------------------------

void publishToMQTT()
{
  if (mqttManager.isConnected())
  {
    sl->Debug("publishToMQTT: Publishing to MQTT...");
    sll->Debug("Publishing to MQTT...");
    mqttManager.publish(mqttSettings.mqtt_publish_AktualBoilerTemperature.c_str(), String(temperature));
    mqttManager.publish(mqttSettings.mqtt_publish_AktualTimeRemaining_topic.c_str(), String(boilerTimeRemaining));
    mqttManager.publish(mqttSettings.mqtt_publish_AktualState.c_str(), String(boilerState));
  }
  else
  {
    sl->Warn("publishToMQTT: MQTT not connected!");
  }
}

void cb_MQTT(char *topic, byte *message, unsigned int length)
{
  String messageTemp((char *)message, length); // Convert byte array to String using constructor
  messageTemp.trim();                          // Remove leading and trailing whitespace

  sl->Printf("<-- MQTT: Topic[%s] <-- [%s]", topic, messageTemp.c_str()).Debug();
  //ToDO: set new Blinker for: helpers.blinkBuidInLED(1, 100); // blink the LED once to indicate that the loop is running
  if (strcmp(topic, mqttSettings.mqtt_Settings_SetState_topic.get().c_str()) == 0)
  {
    // check if it is a number, if not set it to 0
    if (messageTemp.equalsIgnoreCase("null") ||
        messageTemp.equalsIgnoreCase("undefined") ||
        messageTemp.equalsIgnoreCase("NaN") ||
        messageTemp.equalsIgnoreCase("Infinity") ||
        messageTemp.equalsIgnoreCase("-Infinity"))
    {
      sl->Printf("Received invalid value from MQTT: %s", messageTemp.c_str());
      messageTemp = "0";
    }

  }
}

void cb_PublishToMQTT()
{
  publishToMQTT(); // send to Mqtt
}

void cb_MQTTListener()
{
  mqttManager.loop(); // process MQTT connection and incoming messages
}

//----------------------------------------
// HELPER FUNCTIONS
//----------------------------------------

void handeleBoilerState(bool forceON)
{
  static unsigned long lastBoilerCheck = 0;
  unsigned long now = millis();

  if (now - lastBoilerCheck >= 1000) // Check every second
  {
    lastBoilerCheck = now;

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


void SetupCheckForResetButton()
{
  // check for pressed reset button
  if (digitalRead(buttonSettings.resetDefaultsPin.get()) == LOW)
  {
    sl->Internal("Reset button pressed -> Reset all settings...");
    sll->Internal("Reset button pressed!");
    sll->Internal("Reset all settings!");
    cfg.clearAllFromPrefs(); // Clear all settings from EEPROM
    cfg.saveAll();           // Save the default settings to EEPROM

    // Show user feedback that reset is happening
    sll->Internal("Settings reset complete - restarting...");

    ESP.restart();           // Restart the ESP32
  }
}

void SetupCheckForAPModeButton()
{
  String APName = "ESP32_Config";
  String pwd = "config1234"; // Default AP password

  // if (wifiSettings.wifiSsid.get().length() == 0 || systemSettings.unconfigured.get())
  if (wifiSettings.wifiSsid.get().length() == 0 )
  {
  sl->Printf("⚠️ SETUP: WiFi SSID is empty [%s] (fresh/unconfigured)", wifiSettings.wifiSsid.get().c_str()).Error();
    cfg.startAccessPoint("192.168.4.1", "255.255.255.0", APName, "");
  }

  // check for pressed AP mode button

  if (digitalRead(buttonSettings.apModePin.get()) == LOW)
  {
  sl->Internal("AP mode button pressed -> starting AP mode...");
  sll->Internal("AP mode button!");
  sll->Internal("-> starting AP mode...");
    cfg.startAccessPoint("192.168.4.1", "255.255.255.0", APName, "");
  }
}

bool SetupStartWebServer()
{
  sl->Printf("⚠️ SETUP: Starting Webserver...!").Debug();
  sll->Printf("Starting Webserver...!").Debug();

  if (wifiSettings.wifiSsid.get().length() == 0)
  {
    sl->Printf("No SSID! --> Start AP!").Debug();
    sll->Printf("No SSID!").Debug();
    sll->Printf("Start AP!").Debug();
    cfg.startAccessPoint();
    // Removed blocking delay(1000);
    return true; // Skip webserver setup if no SSID is set
  }

  if (WiFi.getMode() == WIFI_AP) {
    sl->Printf("🖥️ Run in AP Mode! ");
    sll->Printf("Run in AP Mode! ");
    return false; // Skip webserver setup in AP mode
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiSettings.useDhcp.get())
    {
      sl->Printf("startWebServer: DHCP enabled\n");
      cfg.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }
    else
    {
      sl->Printf("startWebServer: DHCP disabled\n");
      cfg.startWebServer(wifiSettings.staticIp.get(), wifiSettings.gateway.get(), wifiSettings.subnet.get(),
                  wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }
    // cfg.reconnectWifi();
    WiFi.setSleep(false);
    // Removed blocking delay(1000);
  }
  sl->Printf("\n\nWebserver running at: %s\n", WiFi.localIP().toString().c_str());
  sll->Printf("Web: %s\n\n", WiFi.localIP().toString().c_str());
  sl->Printf("WLAN-Strength: %d dBm\n", WiFi.RSSI());
  sl->Printf("WLAN-Strength is: %s\n\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak"));
  sll->Printf("WLAN: %s\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak"));

  return true; // Webserver setup completed
}

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
    if (lastDisplayActive == true) {
      display.clearDisplay();
      display.display();
      lastDisplayActive = false;
    }
    return; // exit the function if the display is not active
  }

  lastDisplayActive = true;

  // Only update display if values have changed
  bool needsUpdate = false;
  if (abs(temperature - lastTemperature) > 0.1 ||
      boilerTimeRemaining != lastTimeRemaining ||
      boilerState != lastBoilerState) {
    needsUpdate = true;
    lastTemperature = temperature;
    lastTimeRemaining = boilerTimeRemaining;
    lastBoilerState = boilerState;
  }

  if (!needsUpdate) {
    return; // No changes, skip display update
  }

  display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area
  display.drawRect(0, 0, 128, 24, WHITE);

  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Show boiler status and temperature
  display.setCursor(3, 3);
  if (temperature > 0) {
    display.printf("Boiler: %s | T:%.1f°C", boilerState ? "ON " : "OFF", temperature);
  } else {
    display.printf("Boiler: %s", boilerState ? "ON " : "OFF");
  }

  // Show remaining time
  display.setCursor(3, 13);
  if (boilerTimeRemaining > 0) {
    display.printf("Time left: %d min", boilerTimeRemaining);
  } else {
    display.printf("Ready");
  }

  display.display();
}

void PinSetup()
{
  analogReadResolution(12);  // Use full 12-bit resolution
  pinMode(buttonSettings.resetDefaultsPin.get(), INPUT_PULLUP);
  pinMode(buttonSettings.apModePin.get(), INPUT_PULLUP);
  Relays::initPins();
  // Force known OFF state
  Relays::setBoiler(false);
}

void CheckButtons()
{
  static bool lastResetButtonState = HIGH;
  static bool lastAPButtonState = HIGH;
  static unsigned long lastButtonCheck = 0;

  // Debounce: only check buttons every 50ms
  if (millis() - lastButtonCheck < 50) {
    return;
  }
  lastButtonCheck = millis();

  bool currentResetState = digitalRead(buttonSettings.resetDefaultsPin.get());
  bool currentAPState = digitalRead(buttonSettings.apModePin.get());

  // Check for button press (transition from HIGH to LOW)
  if (lastResetButtonState == HIGH && currentResetState == LOW)
  {
    sl->Internal("Reset-Button pressed -> Start Display Ticker...");
    ShowDisplay();
  }

  if (lastAPButtonState == HIGH && currentAPState == LOW)
  {
    sl->Internal("AP-Mode-Button pressed -> Start Display Ticker...");
    ShowDisplay();
  }

  lastResetButtonState = currentResetState;
  lastAPButtonState = currentAPState;
}

void ShowDisplay()
{
  displayTicker.detach(); // Stop the ticker to prevent multiple calls
  display.ssd1306_command(SSD1306_DISPLAYON); // Turn on the display
  displayTicker.attach(displaySettings.onTimeSec.get(), ShowDisplayOff); // Reattach the ticker to turn off the display after the specified time
  displayActive = true;
}

void ShowDisplayOff()
{
  displayTicker.detach(); // Stop the ticker to prevent multiple calls
  display.ssd1306_command(SSD1306_DISPLAYOFF); // Turn off the display
  // display.fillRect(0, 0, 128, 24, BLACK); // Clear the previous message area

  if (displaySettings.turnDisplayOff.get()){
    displayActive = false;
  }
}

// ------------------------------------------------------------------
// Non-blocking status LED pattern
//  States / patterns:
//   - AP mode: fast blink (100ms on / 100ms off)
//   - Connected STA: slow heartbeat (on 60ms every 2s)
//   - Connecting / disconnected: double blink (2 quick pulses every 1s)
// ------------------------------------------------------------------
void updateStatusLED() {
    static unsigned long lastChange = 0;
    static uint8_t phase = 0;
    unsigned long now = millis();

    bool connected = wifiManager.isConnected();
    bool apMode = wifiManager.isInAPMode();

    if (apMode) {
        // simple fast blink 5Hz (100/100)
        if (now - lastChange >= 100) {
            lastChange = now;
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        }
        return;
    }

    if (connected) {
        // heartbeat: brief flash every 2s
        switch (phase) {
            case 0: // LED off idle
                if (now - lastChange >= 2000) { phase = 1; lastChange = now; digitalWrite(LED_BUILTIN, HIGH); }
                break;
            case 1: // LED on briefly
                if (now - lastChange >= 60) { phase = 0; lastChange = now; digitalWrite(LED_BUILTIN, LOW); }
                break;
        }
        return;
    }

    // disconnected / connecting: double blink every ~1s
    switch (phase) {
        case 0: // idle off
            if (now - lastChange >= 1000) { phase = 1; lastChange = now; digitalWrite(LED_BUILTIN, HIGH); }
            break;
        case 1: // first on
            if (now - lastChange >= 80) { phase = 2; lastChange = now; digitalWrite(LED_BUILTIN, LOW); }
            break;
        case 2: // gap
            if (now - lastChange >= 120) { phase = 3; lastChange = now; digitalWrite(LED_BUILTIN, HIGH); }
            break;
        case 3: // second on
            if (now - lastChange >= 80) { phase = 4; lastChange = now; digitalWrite(LED_BUILTIN, LOW); }
            break;
        case 4: // tail gap back to idle
            if (now - lastChange >= 200) { phase = 0; lastChange = now; }
            break;
    }
}

//----------------------------------------
// WIFI MANAGER CALLBACK FUNCTIONS
//----------------------------------------

void onWiFiConnected() {
  sl->Debug("WiFi connected! Activating services...");
  sll->Debug("WiFi reconnected!");
  sll->Debug("Reattach ticker.");

  if (!tickerActive) {
    ShowDisplay(); // Show the display

    // Start MQTT tickers
    PublischMQTTTicker.attach(mqttSettings.MQTTPublischPeriod.get(), cb_PublishToMQTT);
    ListenMQTTTicker.attach(mqttSettings.MQTTListenPeriod.get(), cb_MQTTListener);

    // Start OTA if enabled
    if(systemSettings.allowOTA.get()){
        sll->Debug("Start OTA-Module");
        cfg.setupOTA(APP_NAME, systemSettings.otaPassword.get().c_str());
    }

    tickerActive = true;
  }
}

void onWiFiDisconnected() {
  sl->Debug("WiFi disconnected! Deactivating services...");
  sll->Debug("WiFi lost connection!");
  sll->Debug("deactivate mqtt ticker.");

  if (tickerActive) {
    ShowDisplay(); // Show the display to indicate WiFi is lost

    // Stop MQTT tickers
    PublischMQTTTicker.detach();
    ListenMQTTTicker.detach();

    // Stop OTA if it should be disabled
    if (systemSettings.allowOTA.get() == false && cfg.isOTAInitialized()) {
      sll->Debug("Stop OTA-Module");
      cfg.stopOTA();
    }

    tickerActive = false;
  }
}

void onWiFiAPMode() {
  sl->Debug("WiFi in AP mode");
  sll->Debug("Running in AP mode!");

  // Ensure services are stopped in AP mode
  if (tickerActive) {
    onWiFiDisconnected(); // Reuse disconnected logic
  }
}

