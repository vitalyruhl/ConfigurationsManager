#include <Arduino.h>

// ---------------------------------------------------------------------------------------------------------------------
// ConfigManager compile-time feature toggles (moved to platformio.ini)
//
// To keep the sketch clean and allow per-environment tuning, all feature switches are now defined via
// PlatformIO build flags instead of #defines in this file.
//
// How to use:
// - Open platformio.ini and add -D flags under each [env] in the build_flags section, e.g.:
//     -DCM_ENABLE_OTA=1
//     -DCM_ENABLE_WS_PUSH=1
//     -DCM_ENABLE_RUNTIME_INT_SLIDERS=1
// - You can turn features off by setting =0. The extra build script will also prune the Web UI accordingly.
//
// See docs/FEATURE_FLAGS.md for the complete list and examples. For convenience, this project enables
// most features by default in platformio.ini so you can test and then disable what you don’t need.
// ---------------------------------------------------------------------------------------------------------------------
#include "ConfigManager.h"
// ---------------------------------------------------------------------------------------------------------------------

#include "secret/wifiSecret.h"

#include <Ticker.h>     // for read temperature periodically
#include <BME280_I2C.h> // Include BME280 library Temperature and Humidity sensor
#include "Wire.h"
// Always async web server now
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
// #include <WiFiClientSecure.h>

#define VERSION "V2.6.1" // 2025.10.08
#define APP_NAME "CM-BME280-Demo"
#define BUTTON_PIN_AP_MODE 13

// ⚠️ Warning ⚠️
// ESP32 has a limitation of 15 characters for the key name.
// The key name is built from the category and the key name (<category>_<key>).
// The category is limited to 13 characters, the key name to 1 character.
// Since V2.0.0, the key will be truncated if it is too long, but you now have a user-friendly displayName to show in the web interface.

// Since V2.0.0 there is a way to upload the firmware over the air (OTA).
// You can set the hostname and the password for OTA in the setupOTA function.

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

//--------------------------------------------------------------------
// Forward declarations of functions
void SetupCheckForAPModeButton();
void SetupCheckForResetButton();
void updateStatusLED();                             // new non-blocking status LED handler
void setHeaterState(bool on);
void setFanState(bool on);
void cbTestButton();
void setupGUI();
bool SetupStartWebServer();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
void SetupStartTemperatureMeasuring();
void readBme280();
static float computeDewPoint(float temperatureC, float relHumidityPct);
void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate); // legacy (blocking) blink – will be phased out
void updateStatusLED();                             // new non-blocking status LED handler
void setHeaterState(bool on);
void cbTestButton();

// -------------------------------------------------------------------
// Global theme override test: make all h1 headings orange without underline
// Served via /user_theme.css and auto-injected by the frontend if present.
// NOTE: We only have setCustomCss() (no _P variant yet) so we pass the PROGMEM string pointer directly.
static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
h3 { color: orange; text-decoration: underline;}
.rw[data-group="sensors"][data-key="temp"] .lab{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="sensors"][data-key="temp"] .val{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="sensors"][data-key="temp"] .un{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
)CSS";

// minimal Init
Config<bool> testBool(ConfigOptions<bool>{
    .key = "tbool",
    .category = "main",
    .defaultValue = true});

//---------------------------------------------------------------------------------------------------
// extended version with UI-friendly prettyName and prettyCategory
// Improved version since V2.0.0

Config<int> updateInterval(ConfigOptions<int>{
    .key = "interval",
    .name = "Update Interval (seconds)",
    .category = "main",
    .defaultValue = 30});

// Now, these will be truncated and added if their truncated keys are unique:
Config<float> VeryLongCategoryName(ConfigOptions<float>{
    .key = "VlongC",
    .name = "category Correction long",
    .category = "VeryLongCategoryName",
    .defaultValue = 0.1f});

Config<float> VeryLongKeyName(ConfigOptions<float>{
    .key = "VeryLongKeyName",
    .name = "key Correction long",
    .category = "Temp",
    .defaultValue = 0.1f});

//---------------------------------------------------------------------------------------------------
// ---- Temporary dynamic visibility test settings ----
// Category kept short ("DynTest") to avoid key truncation when combined with key names.
Config<bool> tempBoolToggle(ConfigOptions<bool>{
    .key = "toggle",
    .name = "Temp Toggle",
    .category = "DynTest",
    .defaultValue = true});

Config<String> tempSettingAktiveOnTrue(ConfigOptions<String>{
    .key = "trueS",
    .name = "Visible When True",
    .category = "DynTest",
    .defaultValue = String("Shown if toggle = true"),
    .showIf = []()
    { return tempBoolToggle.get(); }});

Config<String> tempSettingAktiveOnFalse(ConfigOptions<String>{
    .key = "falseS",
    .name = "Visible When False",
    .category = "DynTest",
    .defaultValue = String("Shown if toggle = false"),
    .showIf = []()
    { return !tempBoolToggle.get(); }});
// ---- End temporary dynamic visibility test settings ----

//--------------------------------------------------------------------------------------------------------------
// Example: Using structures for grouped settings
// SystemSettings configuration (structure example)
struct SystemSettings
{
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<int> wifiRebootTimeoutMin;
    Config<String> version;
    SystemSettings() : allowOTA(ConfigOptions<bool>{.key = "OTAEn", .name = "Allow OTA Updates", .category = "System", .defaultValue = true}),
                       otaPassword(ConfigOptions<String>{.key = "OTAPass", .name = "OTA Password", .category = "System", .defaultValue = String("ota1234"), .showInWeb = true, .isPassword = true}),
                       wifiRebootTimeoutMin(ConfigOptions<int>{
                           .key = "WiFiRb",
                           .name = "Reboot if WiFi lost (min)",
                           .category = "System",
                           .defaultValue = 5,
                           .showInWeb = true}),
                       version(ConfigOptions<String>{.key = "P_Version", .name = "Program Version", .category = "System", .defaultValue = String(VERSION)})
    {
        // Register settings with ConfigManager
        ConfigManager.addSetting(&allowOTA);
        ConfigManager.addSetting(&otaPassword);
        ConfigManager.addSetting(&wifiRebootTimeoutMin);
        ConfigManager.addSetting(&version);
    }
};

struct ButtonSettings
{
    Config<int> apModePin;
    Config<int> resetDefaultsPin;
    Config<int> showerRequestPin;
    ButtonSettings() : apModePin(ConfigOptions<int>{.key = "BtnAP", .name = "AP Mode Button GPIO", .category = "Buttons", .defaultValue = 13}),
                       resetDefaultsPin(ConfigOptions<int>{.key = "BtnRst", .name = "Reset Defaults Button GPIO", .category = "Buttons", .defaultValue = 15}),
                       showerRequestPin(ConfigOptions<int>{.key = "BtnShower", .name = "Shower Request Button GPIO", .category = "Buttons", .defaultValue = 19, .showInWeb = true})
    {
        // Register settings with ConfigManager
        ConfigManager.addSetting(&apModePin);
        ConfigManager.addSetting(&resetDefaultsPin);
        ConfigManager.addSetting(&showerRequestPin);
    }
};

SystemSettings systemSettings; // Create an instance of SystemSettings-Struct
ButtonSettings buttonSettings; // Create an instance of ButtonSettings-Struct

//--------------------------------------------------------------------------------------------------------------
// Example of a structure for WiFi settings
struct WiFi_Settings // wifiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;
    Config<String> dnsPrimary;
    Config<String> dnsSecondary;

    WiFi_Settings() : wifiSsid(ConfigOptions<String>{.key = "WiFiSSID", .name = "WiFi SSID", .category = "WiFi", .defaultValue = "", .showInWeb = true, .sortOrder = 1}),
                      wifiPassword(ConfigOptions<String>{.key = "WiFiPassword", .name = "WiFi Password", .category = "WiFi", .defaultValue = "secretpass", .showInWeb = true, .isPassword = true, .sortOrder = 2}),
                      useDhcp(ConfigOptions<bool>{.key = "WiFiUseDHCP", .name = "Use DHCP", .category = "WiFi", .defaultValue = true, .showInWeb = true, .sortOrder = 3}),
                      staticIp(ConfigOptions<String>{.key = "WiFiStaticIP", .name = "Static IP", .category = "WiFi", .defaultValue = "192.168.2.131", .sortOrder = 4, .showIf = [this](){ return !useDhcp.get(); }}),
                      gateway(ConfigOptions<String>{.key = "WiFiGateway", .name = "Gateway", .category = "WiFi", .defaultValue = "192.168.2.250", .sortOrder = 5, .showIf = [this](){ return !useDhcp.get(); }}),
                      subnet(ConfigOptions<String>{.key = "WiFiSubnet", .name = "Subnet Mask", .category = "WiFi", .defaultValue = "255.255.255.0", .sortOrder = 6, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsPrimary(ConfigOptions<String>{.key = "WiFiDNS1", .name = "Primary DNS", .category = "WiFi", .defaultValue = "192.168.2.250", .sortOrder = 7, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsSecondary(ConfigOptions<String>{.key = "WiFiDNS2", .name = "Secondary DNS", .category = "WiFi", .defaultValue = "8.8.8.8", .sortOrder = 8, .showIf = [this](){ return !useDhcp.get(); }})
    {
        // Settings will be registered manually in setup() to avoid constructor timing issues
    }

};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct

//--------------------------------------------------------------------------------------------------------------
// NTP Settings
struct NTPSettings
{
    Config<int> frequencySec; // Sync frequency (seconds)
    Config<String> server1;   // Primary NTP server
    Config<String> server2;   // Secondary NTP server
    Config<String> tz;        // POSIX/TZ string for local time
    NTPSettings() : frequencySec(ConfigOptions<int>{.key = "NTPFrq", .name = "NTP Sync Interval (s)", .category = "NTP", .defaultValue = 3600, .showInWeb = true}),
                    server1(ConfigOptions<String>{.key = "NTP1", .name = "NTP Server 1", .category = "NTP", .defaultValue = String("192.168.2.250"), .showInWeb = true}),
                    server2(ConfigOptions<String>{.key = "NTP2", .name = "NTP Server 2", .category = "NTP", .defaultValue = String("pool.ntp.org"), .showInWeb = true}),
                    tz(ConfigOptions<String>{.key = "NTPTZ", .name = "Time Zone (POSIX)", .category = "NTP", .defaultValue = String("CET-1CEST,M3.5.0/02,M10.5.0/03"), .showInWeb = true})
    {
        ConfigManager.addSetting(&frequencySec);
        ConfigManager.addSetting(&server1);
        ConfigManager.addSetting(&server2);
        ConfigManager.addSetting(&tz);
    }
};

NTPSettings ntpSettings; // ntpSettings

//--------------------------------------------------------------------------------------------------------------
// MQTT Settings
// Declaration as a struct with callback function
struct MQTT_Settings
{
    Config<int> mqtt_port;
    Config<String> mqtt_server;
    Config<String> mqtt_username;
    Config<String> mqtt_password;
    Config<String> Publish_Topic;
    // Derived MQTT topics (no longer persisted): see updateTopics()
    String topicSetShowerTime;                 // <base>/Settings/SetShowerTime
    String topicWillShower;                    // <base>/Settings/WillShower
    Config<bool> mqtt_Settings_SetState;       // payload to turn boiler on
    String mqtt_publish_YouCanShowerNow_topic; // <base>/YouCanShowerNow (publish)
    String topic_BoilerEnabled;                // <base>/Settings/BoilerEnabled
    String topic_OnThreshold;                  // <base>/Settings/OnThreshold
    String topic_OffThreshold;                 // <base>/Settings/OffThreshold
    String topic_BoilerTimeMin;                // <base>/Settings/BoilerTimeMin
    String topic_StopTimerOnTarget;            // <base>/Settings/StopTimerOnTarget
    String topic_OncePerPeriod;                // <base>/Settings/YouCanShowerOncePerPeriod
    String topic_YouCanShowerPeriodMin;        // <base>/Settings/YouCanShowerPeriodMin
    String topicSave;                          // <base>/Settings/Save (subscribe)
    Config<float> MQTTPublischPeriod;
    Config<float> MQTTListenPeriod;

    // For dynamic topics based on Publish_Topic
    String mqtt_publish_AktualState;
    String mqtt_publish_AktualBoilerTemperature;
    String mqtt_publish_AktualTimeRemaining_topic;

    MQTT_Settings() : mqtt_port(ConfigOptions<int>{.key = "MQTTTPort", .name = "Port", .category = "MQTT", .defaultValue = 1883}),
                      mqtt_server(ConfigOptions<String>{.key = "MQTTServer", .name = "Server-IP", .category = "MQTT", .defaultValue = String("192.168.2.3")}),
                      mqtt_username(ConfigOptions<String>{.key = "MQTTUser", .name = "User", .category = "MQTT", .defaultValue = String("housebattery")}),
                      mqtt_password(ConfigOptions<String>{.key = "MQTTPass", .name = "Password", .category = "MQTT", .defaultValue = String("mqttsecret"), .showInWeb = true, .isPassword = true}),
                      Publish_Topic(ConfigOptions<String>{.key = "MQTTTPT", .name = "Publish-Topic", .category = "MQTT", .defaultValue = String("BoilerSaver")}),
                      MQTTPublischPeriod(ConfigOptions<float>{.key = "MQTTPP", .name = "Publish-Period (s)", .category = "MQTT", .defaultValue = 2.0f}),
                      MQTTListenPeriod(ConfigOptions<float>{.key = "MQTTLP", .name = "Listen-Period (s)", .category = "MQTT", .defaultValue = 0.5f}),
                      mqtt_Settings_SetState(ConfigOptions<bool>{.key = "SetSt", .name = "Set-State", .category = "MQTT", .defaultValue = false, .showInWeb = false, .isPassword = false})
    {

        Publish_Topic.setCallback([this](String newValue)
                                  { this->updateTopics(); });

        updateTopics(); // Make sure topics are initialized
    }

    void registerSettings()
    {
        ConfigManager.addSetting(&mqtt_port);
        ConfigManager.addSetting(&mqtt_server);
        ConfigManager.addSetting(&mqtt_username);
        ConfigManager.addSetting(&mqtt_password);
        ConfigManager.addSetting(&Publish_Topic);
        ConfigManager.addSetting(&MQTTPublischPeriod);
        ConfigManager.addSetting(&MQTTListenPeriod);
        ConfigManager.addSetting(&mqtt_Settings_SetState);
    }

    void updateTopics()
    {
        String hostname = Publish_Topic.get();
        mqtt_publish_AktualState = hostname + "/AktualState";                   // show State of Boiler Heating/Save-Mode
        mqtt_publish_AktualBoilerTemperature = hostname + "/TemperatureBoiler"; // show Temperature of Boiler
        mqtt_publish_AktualTimeRemaining_topic = hostname + "/TimeRemaining";   // show Time Remaining if Boiler is Heating
        mqtt_publish_YouCanShowerNow_topic = hostname + "/YouCanShowerNow";     // for notifying that user can shower now
        // Settings/state topics
        String sp = hostname + "/Settings";
        topicWillShower = sp + "/WillShower";
        topicSetShowerTime = sp + "/SetShowerTime";
        topicSave = sp + "/Save";
        topic_BoilerEnabled = sp + "/BoilerEnabled";
        topic_OnThreshold = sp + "/OnThreshold";
        topic_OffThreshold = sp + "/OffThreshold";
        topic_BoilerTimeMin = sp + "/BoilerTimeMin";
        topic_StopTimerOnTarget = sp + "/StopTimerOnTarget";
        topic_OncePerPeriod = sp + "/OncePerPeriod";
        topic_YouCanShowerPeriodMin = sp + "/YouCanShowerPeriodMin";

        // Debug: Print topic lengths to detect potential issues
        Serial.printf("[MQTT] StopTimerOnTarget topic: [%s] (length: %d)\n", topic_StopTimerOnTarget.c_str(), topic_StopTimerOnTarget.length());
    }
};
MQTT_Settings mqttSettings; // mqttSettings

//--------------------------------------------------------------------------------------------------------------
// implement read temperature function and variables to show how to unse live values
//--------------------------------------------------------------------------------------------------------------
// set the I2C address for the BME280 sensor for temperature and humidity
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_FREQUENCY 400000
#define BME280_FREQUENCY 400000
// #define BME280_ADDRESS 0x76 // I2C address for the BME280 sensor (default is 0x76) redefine, if needed

#define ReadTemperatureTicker 10.0 // time in seconds to read the temperature and humidity

BME280_I2C bme280;
Ticker temperatureTicker;
bool tickerActive = false; // flag to indicate if the ticker is active
Ticker NtpSyncTicker;

// forward declarations
void readBme280(); // read the values from the BME280 (Temperature, Humidity) and calculate the dewpoint
void SetupStartTemperatureMeasuring(); // setup the BME280 temperature and humidity sensor
static float computeDewPoint(float temperatureC, float relHumidityPct); // compute the dewpoint from temperature and humidity
static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntimeManager(); } // Shorthand helper for RuntimeManager access

float temperature = 0.0; // current temperature in Celsius
float Dewpoint = 0.0;    // current dewpoint in Celsius
float Humidity = 0.0;    // current humidity in percent
float Pressure = 0.0;    // current pressure in hPa

struct TempSettings
{
    Config<float> tempCorrection;
    Config<float> humidityCorrection;
    Config<int> seaLevelPressure;
    Config<int> readIntervalSec;
    Config<float> dewpointRiskWindow; // ΔT (°C) über Taupunkt, ab der Risiko-Alarm auslöst

    TempSettings() : tempCorrection(ConfigOptions<float>{.key = "TCO", .name = "Temperature Correction", .category = "Temp", .defaultValue = 0.1f}),
                     humidityCorrection(ConfigOptions<float>{.key = "HYO", .name = "Humidity Correction", .category = "Temp", .defaultValue = 0.1f}),
                     seaLevelPressure(ConfigOptions<int>{.key = "SLP", .name = "Sea Level Pressure", .category = "Temp", .defaultValue = 1013}),
                     readIntervalSec(ConfigOptions<int>{.key = "ReadTemp", .name = "Read Temp/Humidity every (s)", .category = "Temp", .defaultValue = 30}),
                     dewpointRiskWindow(ConfigOptions<float>{.key = "DPWin", .name = "Dewpoint Risk Window (°C)", .category = "Temp", .defaultValue = 1.5f})
    {
        ConfigManager.addSetting(&tempCorrection);
        ConfigManager.addSetting(&humidityCorrection);
        ConfigManager.addSetting(&seaLevelPressure);
        ConfigManager.addSetting(&readIntervalSec);
        ConfigManager.addSetting(&dewpointRiskWindow);
    }
};
TempSettings tempSettings;
//-------------------------------------------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);

    //-----------------------------------------------------------------
    // Set logger callback to log in your own way, but do this before using the ConfigManager object!
    // Example 1: use Serial for logging
    // void cbMyConfigLogger(const char *msg){ Serial.println(msg);}
    // ConfigManagerClass::setLogger(cbMyConfigLogger);

    // Or as a lambda function...
    ConfigManagerClass::setLogger([](const char *msg)
                                  {
            Serial.print("[ConfigManager] ");
            Serial.println(msg); });

    //-----------------------------------------------------------------
    ConfigManager.setAppName(APP_NAME);                                                   // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setVersion(VERSION);                                                   // Set the application version for web UI display
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1); // Register global CSS override
    ConfigManager.enableBuiltinSystemProvider();                                          // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    //----------------------------------------------------------------------------------------------------------------------------------

    //----------------------------------------------------------------------------------------------------------------------------------
    // Register other settings
    ConfigManager.addSetting(&updateInterval);
    ConfigManager.addSetting(&testBool);
    ConfigManager.addSetting(&VeryLongCategoryName);
    ConfigManager.addSetting(&VeryLongKeyName);

    // Register temporary dynamic test settings
    ConfigManager.addSetting(&tempBoolToggle); // show, we have used it in gui, but register it here - no problem, its only showing
    ConfigManager.addSetting(&tempSettingAktiveOnTrue);
    ConfigManager.addSetting(&tempSettingAktiveOnFalse);

    ConfigManager.addSetting(&tempSettings.readIntervalSec);
    
    // IMPORTANT: Register WiFi settings manually (constructor registration seems to fail)
    ConfigManager.addSetting(&wifiSettings.wifiSsid);
    ConfigManager.addSetting(&wifiSettings.wifiPassword);
    ConfigManager.addSetting(&wifiSettings.useDhcp);
    ConfigManager.addSetting(&wifiSettings.staticIp);
    ConfigManager.addSetting(&wifiSettings.gateway);
    ConfigManager.addSetting(&wifiSettings.subnet);
    ConfigManager.addSetting(&wifiSettings.dnsPrimary);
    ConfigManager.addSetting(&wifiSettings.dnsSecondary);
    //----------------------------------------------------------------------------------------------------------------------------------

    ConfigManager.checkSettingsForErrors(); // 2025.09.04 New function to check all settings for errors (e.g., duplicate keys after truncation etc.)

    try
    {
        ConfigManager.loadAll(); // Load all settings from preferences, is nessesary before using the settings!
    }
    catch (const std::exception &e)
    {
        Serial.println(e.what());
    }

    //----------------------------------------------------------------------------------------------------------------------------------
    // check for reset button on startup (but not AP mode button yet)
    SetupCheckForResetButton();

    //----------------------------------------------------------------------------------------------------------------------------------
    // set wifi settings if not set yet from my secret folder
    Serial.printf("[DEBUG] SSID check: '%s' (isEmpty: %s, length: %d)\n", 
                  wifiSettings.wifiSsid.get().c_str(), 
                  wifiSettings.wifiSsid.get().isEmpty() ? "true" : "false",
                  wifiSettings.wifiSsid.get().length());
    
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

    //----------------------------------------------------------------------------------------------------------------------------------
    // check for AP mode button AFTER setting WiFi credentials
    SetupCheckForAPModeButton();

    // perform the wifi connection
    bool startedInStationMode = SetupStartWebServer();
    if (startedInStationMode)
    {
        // setupMQTT();
    }
    else
    {
        Serial.println("[SETUP] we are in AP mode");
    }

    setupGUI();
    ConfigManager.enableWebSocketPush(); // Enable WebSocket push for real-time updates
    
    // Enhanced WebSocket configuration
    ConfigManager.setWebSocketInterval(1000); // Faster updates - every 1 second
    ConfigManager.setPushOnConnect(true);     // Immediate data on client connect
    
    SetupStartTemperatureMeasuring();
    //----------------------------------------------------------------------------------------------------------------------------------

    Serial.println("Loaded configuration:");

    // Show correct IP address depending on WiFi mode
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        Serial.printf("🖥️ Webserver running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("🖥️ Webserver running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("🖥️ Webserver running (IP not available)");
    }

    Serial.println("Configuration printout:");
    Serial.println(ConfigManager.toJSON(true)); // Show ALL settings, not just web-visible ones
    
    // Note: debugPrintSettings() removed due to potential crashes during startup
    Serial.println("\nSetup completed successfully!");

    // Test setting changes
    systemSettings.version.set(VERSION); // Update version on device
    testBool.set(false);
    updateInterval.set(15);
    ConfigManager.saveAll();
    delay(300);
}

void loop()
{
    ConfigManager.updateLoopTiming();        // Update internal loop timing metrics for system provider
    ConfigManager.getWiFiManager().update(); // Update WiFi Manager - handles all WiFi logic

    // WiFi status monitoring for debugging
    static unsigned long lastWiFiCheck = 0;
    if (millis() - lastWiFiCheck > 30000) // Check every 30 seconds
    {
        lastWiFiCheck = millis();
        bool wifiStatus = ConfigManager.getWiFiStatus();
        Serial.printf("[WiFi] Status: %s\n", wifiStatus ? "Connected" : "Disconnected");
        if (wifiStatus) {
            int currentRSSI = WiFi.RSSI();
            String bssid = WiFi.BSSIDstr();
            String accessPoint = "Unknown";
            
            // Identify access point by BSSID
            if (bssid.indexOf("66:b5:8d:4c:e1:d5") >= 0 || bssid.indexOf("66-b5-8d-4c-e1-d5") >= 0) {
                accessPoint = "FRITZ!Repeater 1200 AX";
            } else {
                accessPoint = "Main FRITZ!Box (or other AP)";
            }
            
            Serial.printf("[WiFi] Current RSSI: %d dBm (%s)\n", currentRSSI, 
                currentRSSI > -50 ? "excellent" : (currentRSSI > -60 ? "good" : (currentRSSI > -67 ? "ok" : (currentRSSI > -75 ? "weak" : "very weak"))));
            Serial.printf("[WiFi] Connected to: %s\n", accessPoint.c_str());
            Serial.printf("[WiFi] BSSID: %s (Channel: %d)\n", bssid.c_str(), WiFi.channel());
        }
    }

    // Evaluate cross-field runtime alarms periodically (cheap doc build ~ small JSON)
    static unsigned long lastAlarmEval = 0;
    if (millis() - lastAlarmEval > 1500)
    {
        lastAlarmEval = millis();
        CRM().updateAlarms();
    }

    ConfigManager.handleClient();
    ConfigManager.handleWebsocketPush();
    ConfigManager.handleOTA();           // Handle OTA updates
    ConfigManager.handleRuntimeAlarms(); // Handle runtime alarms
    ConfigManager.updateLoopTiming(); // Update internal loop timing metrics for system provider

    delay(10);
}



//----------------------------------------
// GUI SETUP
//----------------------------------------

void setupGUI()
{
    //-----------------------------------------------------------------
    // BME280 Sensor Display with Runtime Providers
    //-----------------------------------------------------------------

    // Register sensor runtime provider for BME280 data
    ConfigManager.getRuntimeManager().addRuntimeProvider("sensors", [](JsonObject &data)
    {
        // Apply precision to sensor values to reduce JSON size
        data["temp"] = roundf(temperature * 10.0f) / 10.0f;     // 1 decimal place
        data["hum"] = roundf(Humidity * 10.0f) / 10.0f;        // 1 decimal place  
        data["dew"] = roundf(Dewpoint * 10.0f) / 10.0f;        // 1 decimal place
        data["pressure"] = roundf(Pressure * 10.0f) / 10.0f;   // 1 decimal place
    });

    // Define sensor display fields using addRuntimeMeta
    RuntimeFieldMeta tempMeta;
    tempMeta.group = "sensors";
    tempMeta.key = "temp";
    tempMeta.label = "Temperature";
    tempMeta.unit = "°C";
    tempMeta.precision = 1;
    tempMeta.order = 10;
    ConfigManager.getRuntimeManager().addRuntimeMeta(tempMeta);

    RuntimeFieldMeta humMeta;
    humMeta.group = "sensors";
    humMeta.key = "hum";
    humMeta.label = "Humidity";
    humMeta.unit = "%";
    humMeta.precision = 1;
    humMeta.order = 11;
    ConfigManager.getRuntimeManager().addRuntimeMeta(humMeta);

    RuntimeFieldMeta dewMeta;
    dewMeta.group = "sensors";
    dewMeta.key = "dew";
    dewMeta.label = "Dewpoint";
    dewMeta.unit = "°C";
    dewMeta.precision = 1;
    dewMeta.order = 12;
    ConfigManager.getRuntimeManager().addRuntimeMeta(dewMeta);

    RuntimeFieldMeta pressureMeta;
    pressureMeta.group = "sensors";
    pressureMeta.key = "pressure";
    pressureMeta.label = "Pressure";
    pressureMeta.unit = "hPa";
    pressureMeta.precision = 1;
    pressureMeta.order = 13;
    ConfigManager.getRuntimeManager().addRuntimeMeta(pressureMeta);

    // Add runtime provider for sensor range field
    RuntimeFieldMeta rangeMeta;
    rangeMeta.group = "sensors";
    rangeMeta.key = "range";
    rangeMeta.label = "Sensor Range";
    rangeMeta.unit = "V";
    rangeMeta.precision = 1;
    rangeMeta.order = 14;
    ConfigManager.getRuntimeManager().addRuntimeMeta(rangeMeta);

    // Add status provider for connection status
    ConfigManager.getRuntimeManager().addRuntimeProvider("status", [](JsonObject &data)
    {
        data["connected"] = WiFi.status() == WL_CONNECTED;
    });

    // Add interactive controls provider
    ConfigManager.getRuntimeManager().addRuntimeProvider("controls", [](JsonObject &data)
    {
        // Optionally expose control states
    });

    // Test button
    ConfigManager.defineRuntimeButton("controls", "testBtn", "Test Button", []()
    {
        cbTestButton();
    }, "", 20);

    // Example toggle for heater simulation
    static bool heaterState = false;
    ConfigManager.defineRuntimeCheckbox("controls", "heater", "Heater", []()
    {
        return heaterState;
    }, [](bool state)
    {
        heaterState = state;
        setHeaterState(state);
    }, "", 21);

    // Example state button (toggle with visual feedback)
    static bool fanState = false;
    ConfigManager.defineRuntimeStateButton("controls", "fan", "Fan", []()
    {
        return fanState;
    }, [](bool state)
    {
        fanState = state;
        setFanState(state);
        Serial.printf("[FAN] State: %s\n", state ? "ON" : "OFF");
    }, false, "", 22);

    // Integer slider for adjustments
    static int adjustValue = 0;
    ConfigManager.defineRuntimeIntSlider("controls", "adjust", "Adjustment", -10, 10, 0, []()
    {
        return adjustValue;
    }, [](int value)
    {
        adjustValue = value;
        Serial.printf("[ADJUST] Value: %d\n", value);
    }, "", "steps", 23);

    // Float slider for temperature offset
    static float tempOffset = 0.0f;
    ConfigManager.defineRuntimeFloatSlider("controls", "tempOffset", "Temp Offset", -5.0f, 5.0f, 0.0f, 2, []()
    {
        return tempOffset;
    }, [](float value)
    {
        tempOffset = value;
        Serial.printf("[TEMP_OFFSET] Value: %.2f°C\n", value);
    }, "", "°C", 24);

    // Additional runtime fields as recommended
    // Sensor range field for demonstration
    static float sensorRange = 3.3f;
    ConfigManager.defineRuntimeField("sensors", "range", "Sensor Range", "V", 0.0, 5.0);
    
    // Connection status boolean
    static bool connectionStatus = false;
    ConfigManager.defineRuntimeBool("status", "connected", "Connection Status", false, 1);
    
    // Overheat alarm 
    ConfigManager.defineRuntimeAlarm("alarms", "overheat", "Overheat Warning", []() { 
        return temperature > 40.0; // Trigger at 40°C for demo
    });

    // Alarm status display using addRuntimeMeta for boolean values
    ConfigManager.getRuntimeManager().addRuntimeProvider("alarms", [](JsonObject &data)
    {
        data["dewpoint_risk"] = false; // Will be updated by alarm logic
        data["temp_low"] = false;      // Will be updated by alarm logic
    });

    RuntimeFieldMeta dewpointRiskMeta;
    dewpointRiskMeta.group = "alarms";
    dewpointRiskMeta.key = "dewpoint_risk";
    dewpointRiskMeta.label = "Dewpoint Risk";
    dewpointRiskMeta.order = 30;
    dewpointRiskMeta.isBool = true;
    ConfigManager.getRuntimeManager().addRuntimeMeta(dewpointRiskMeta);

    RuntimeFieldMeta tempLowMeta;
    tempLowMeta.group = "alarms";
    tempLowMeta.key = "temp_low";
    tempLowMeta.label = "Low Temperature";
    tempLowMeta.order = 31;
    tempLowMeta.isBool = true;
    ConfigManager.getRuntimeManager().addRuntimeMeta(tempLowMeta);
}

//----------------------------------------
// HELPER FUNCTIONS
//----------------------------------------

void SetupCheckForResetButton()
{
    // check for pressed reset button
    if (digitalRead(buttonSettings.resetDefaultsPin.get()) == LOW)
    {
        Serial.println("[MAIN] Reset button pressed -> Reset all settings...");
        ConfigManager.clearAllFromPrefs(); // Clear all settings from EEPROM
        ConfigManager.saveAll();           // Save the default settings to EEPROM

        // Show user feedback that reset is happening
        Serial.println("[MAIN] restarting...");
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
        Serial.println("[MAIN] WiFi SSID is empty (fresh/unconfigured)");
        ConfigManager.startAccessPoint(APName, ""); // Only SSID and password
    }

    // check for pressed AP mode button

    if (digitalRead(buttonSettings.apModePin.get()) == LOW)
    {
        Serial.println("[MAIN] AP mode button pressed -> starting AP mode...");
        ConfigManager.startAccessPoint(APName, ""); // Only SSID and password
    }
}

//----------------------------------------
// WIFI MANAGER CALLBACK FUNCTIONS
//----------------------------------------

bool SetupStartWebServer()
{
    Serial.println("[MAIN] Starting Webserver...!");

    if (WiFi.getMode() == WIFI_AP)
    {
        return false; // Skip webserver setup in AP mode
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        if (wifiSettings.useDhcp.get())
        {
            Serial.println("[MAIN] startWebServer: DHCP enabled");
            ConfigManager.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
        }
        else
        {
            Serial.println("[MAIN] startWebServer: DHCP disabled - using static IP");
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
    Serial.println("[MAIN] WiFi connected! Activating services...");

    if (!tickerActive)
    {
        // Start OTA if enabled
        if (systemSettings.allowOTA.get())
        {
            ConfigManager.setupOTA(APP_NAME, systemSettings.otaPassword.get().c_str());
        }

        tickerActive = true;
    }

    // Show correct IP address when connected
    Serial.printf("\n\n[MAIN] Webserver running at: %s (Connected)\n", WiFi.localIP().toString().c_str());
    Serial.printf("[MAIN] WLAN-Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[MAIN] WLAN-Strength is: %s\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak"));
    
    String bssid = WiFi.BSSIDstr();
    String accessPoint = "Unknown";
    if (bssid.indexOf("66:b5:8d:4c:e1:d5") >= 0 || bssid.indexOf("66-b5-8d-4c-e1-d5") >= 0) {
        accessPoint = "FRITZ!Repeater 1200 AX";
    } else {
        accessPoint = "Main FRITZ!Box (or other AP)";
    }
    
    Serial.printf("[MAIN] Connected to: %s\n", accessPoint.c_str());
    Serial.printf("[MAIN] BSSID: %s (Channel: %d)\n", bssid.c_str(), WiFi.channel());
    Serial.printf("[MAIN] Local MAC: %s\n\n", WiFi.macAddress().c_str());

    // Start NTP sync now and schedule periodic resyncs
    auto doNtpSync = []()
    {
        // Use TZ-aware sync for correct local time (Berlin: CET/CEST)
        configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str());
    };
    doNtpSync();
    NtpSyncTicker.detach();
    int ntpInt = ntpSettings.frequencySec.get();
    if (ntpInt < 60)
        ntpInt = 3600; // default to 1 hour
    NtpSyncTicker.attach(ntpInt, +[]()
                                 { configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str()); });
}

void onWiFiDisconnected()
{
    Serial.println("[MAIN] WiFi disconnected! Deactivating services...");

    if (tickerActive)
    {
        // Stop OTA if it should be disabled
        if (systemSettings.allowOTA.get() == false && ConfigManager.getOTAManager().isInitialized())
        {
            // ConfigManager.stopOTA(); // Not needed in new API
        }

        tickerActive = false;
    }
}

void onWiFiAPMode()
{
    Serial.println("[MAIN] WiFi in AP mode");

    // Ensure services are stopped in AP mode
    if (tickerActive)
    {
        onWiFiDisconnected(); // Reuse disconnected logic
    }
}

//----------------------------------------
// Other FUNCTIONS
//----------------------------------------

void SetupStartTemperatureMeasuring()
{
    // init BME280 for temperature and humidity sensor
    bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);
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
        Serial.println("can NOT initialize for using BME280.");
    }
    else
    {
        Serial.println("ready to using BME280. Sart Ticker...");
        int iv = tempSettings.readIntervalSec.get();
        if (iv < 2)
            iv = 2;
        temperatureTicker.attach((float)iv, readBme280); // Attach ticker with configured interval
        readBme280();                                    // Read once at startup
    }
}

static float computeDewPoint(float temperatureC, float relHumidityPct)
{
    if (isnan(temperatureC) || isnan(relHumidityPct))
        return NAN;
    if (relHumidityPct <= 0.0f)
        relHumidityPct = 0.1f; // Unterlauf abfangen
    if (relHumidityPct > 100.0f)
        relHumidityPct = 100.0f; // Clamp
    const float a = 17.62f;
    const float b = 243.12f;
    float rh = relHumidityPct / 100.0f;
    float gamma = (a * temperatureC) / (b + temperatureC) + log(rh);
    float dew = (b * gamma) / (a - gamma);
    return dew;
}

void readBme280()
{
    //   set sea-level pressure
    bme280.setSeaLevelPressure(tempSettings.seaLevelPressure.get());

    bme280.read();

    temperature = bme280.data.temperature + tempSettings.tempCorrection.get();
    Humidity = bme280.data.humidity + tempSettings.humidityCorrection.get();
    Pressure = bme280.data.pressure;

    Dewpoint = computeDewPoint(temperature, Humidity);

    // Note: Serial output removed from ticker interrupt context to prevent blocking
    // Temperature readings are available via runtime providers in web interface
}


#define HEATER_PIN 23       // Example pin for heater relay
#define FAN_PIN 25          // Example pin for fan relay
#define LowActiveRelay true // Set to true if relay is active LOW, false if active HIGH
void setHeaterState(bool on)
{
    pinMode(HEATER_PIN, OUTPUT); // Example pin for heater relay
    if (on)
    {
        Serial.println("Heater ON");
        digitalWrite(HEATER_PIN, LowActiveRelay ? LOW : HIGH); // Example: turn on heater relay
    }
    else
    {
        Serial.println("Heater OFF");
        digitalWrite(HEATER_PIN, LowActiveRelay ? HIGH : LOW); // Example: turn off heater relay
    }
}

void setFanState(bool on)
{
    pinMode(FAN_PIN, OUTPUT); // Example pin for fan relay
    if (on)
    {
        Serial.println("Fan ON");
        digitalWrite(FAN_PIN, LowActiveRelay ? LOW : HIGH); // Example: turn on fan relay
    }
    else
    {
        Serial.println("Fan OFF");
        digitalWrite(FAN_PIN, LowActiveRelay ? HIGH : LOW); // Example: turn off fan relay
    }
}

void cbTestButton()
{
    Serial.println("Test Button pressed!");
}

// ------------------------------------------------------------------
// Non-blocking status LED pattern
//  States / patterns:
//   - AP mode: fast blink (100ms on / 100ms off)
//   - Connected STA: slow heartbeat (on 60ms every 2s)
//   - Connecting / disconnected: double blink (2 quick pulses every 1s)
// ------------------------------------------------------------------
void updateStatusLED()
{
    static unsigned long lastChange = 0;
    static uint8_t phase = 0;
    unsigned long now = millis();

    bool apMode = WiFi.getMode() == WIFI_AP;
    bool connected = !apMode && WiFi.status() == WL_CONNECTED;

    if (apMode)
    {
        // simple fast blink 5Hz (100/100)
        if (now - lastChange >= 100)
        {
            lastChange = now;
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
        }
        return;
    }

    if (connected)
    {
        // heartbeat: brief flash every 2s
        switch (phase)
        {
        case 0: // LED off idle
            if (now - lastChange >= 2000)
            {
                phase = 1;
                lastChange = now;
                digitalWrite(LED_BUILTIN, HIGH);
            }
            break;
        case 1: // LED on briefly
            if (now - lastChange >= 60)
            {
                phase = 0;
                lastChange = now;
                digitalWrite(LED_BUILTIN, LOW);
            }
            break;
        }
        return;
    }

    // disconnected / connecting: double blink every ~1s
    switch (phase)
    {
    case 0: // idle off
        if (now - lastChange >= 1000)
        {
            phase = 1;
            lastChange = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
        break;
    case 1: // first on
        if (now - lastChange >= 80)
        {
            phase = 2;
            lastChange = now;
            digitalWrite(LED_BUILTIN, LOW);
        }
        break;
    case 2: // gap
        if (now - lastChange >= 120)
        {
            phase = 3;
            lastChange = now;
            digitalWrite(LED_BUILTIN, HIGH);
        }
        break;
    case 3: // second on
        if (now - lastChange >= 80)
        {
            phase = 4;
            lastChange = now;
            digitalWrite(LED_BUILTIN, LOW);
        }
        break;
    case 4: // tail gap back to idle
        if (now - lastChange >= 200)
        {
            phase = 0;
            lastChange = now;
        }
        break;
    }
}

