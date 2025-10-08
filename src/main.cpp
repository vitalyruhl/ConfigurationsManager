#include <Arduino.h>

// ---------------------------------------------------------------------------------------------------------------------
// ConfigManager compile-time feature toggles
//
// All switches inside ConfigManagerConfig.h.  Define them here before including the header to turn them on/off.
// Example:
//   #define CM_ENABLE_RUNTIME_CONTROLS 0
//   #include "ConfigManager.h"
// is not needed to shave off flash & RAM without touching the library sources.  Leave a flag commented out to
// inherit the upstream default.  This example keeps every feature on to showcase the full demo surface.
// ---------------------------------------------------------------------------------------------------------------------

#define CM_EMBED_WEBUI 1 // Enable embedded web UI (HTML/CSS/JS) in binary //default: enabled
#define CM_ALARM_GREEN_ON_FALSE 1 // Enable showing alarm state green on false for boolean runtime fields //default: enabled
#define CM_ENABLE_RUNTIME_CONTROLS 1 // Enable runtime controls (sensors and alarms only) //default: enabled
#define CM_ENABLE_RUNTIME_BUTTONS 1 // Enable runtime buttons //default: disabled
#define CM_ENABLE_RUNTIME_CHECKBOXES 1 // Enable runtime checkboxes (two-state toggles styled as android switches) //default: disabled
#define CM_ENABLE_RUNTIME_STATE_BUTTONS 1 // Enable runtime state buttons (ON/OFF with 2 states) //default: disabled
#define CM_ENABLE_RUNTIME_INT_SLIDERS 1  // Enable runtime integer sliders //default: disabled
#define CM_ENABLE_RUNTIME_FLOAT_SLIDERS 1 // Enable runtime float sliders //default: disabled
#define CM_ENABLE_RUNTIME_ALARMS 1 // Enable runtime alarms (thresholds, color coding) //default: disabled
#define CM_ENABLE_SYSTEM_PROVIDER 1  // Enable system info runtime provider card (heap, uptime, etc.) //default: disabled
#define CM_ENABLE_WS_PUSH 1 // Enable WebSocket push of runtime JSON updates (if runtime controls or alarms are enabled, this is auto-enabled) //default: disabled
#define CM_ENABLE_THEMING 1 //disable both theming and user CSS for simplicity //default: disabled
#define CM_ENABLE_STYLE_RULES 1//only style rules over .set("background", "#000") etc. //default: disabled
#define CM_ENABLE_USER_CSS 1 // Enable user CSS support (you can overload all CSS served by the frontend via cfg.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);) //default: disabled
#define CM_ENABLE_DYNAMIC_VISIBILITY 0 // Enable dynamic visibility of settings based on other settings (showIf callbacks) //default: disabled
#define CM_ENABLE_OTA 0 // Enable OTA update functionality //default: disabled
#define CM_ENABLE_LOGGING 0 // Enable logging via callback function (setLogger) //default: disabled
#define CM_ENABLE_VERBOSE_LOGGING 0 // Enable verbose logging (more detailed messages) //default: disabled


#include "ConfigManager.h"
#include <Ticker.h>     // for read temperature periodically
#include <BME280_I2C.h> // Include BME280 library Temperature and Humidity sensor
#include "Wire.h"
// Always async web server now
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
AsyncWebServer server(80);
// #include <WiFiClientSecure.h>

#define VERSION "V2.6.1" //2025.10.08
#define APP_NAME "CM-BME280-Demo"
#define BUTTON_PIN_AP_MODE 13

// ⚠️ Warning ⚠️
// ESP32 has a limitation of 15 characters for the key name.
// The key name is built from the category and the key name (<category>_<key>).
// The category is limited to 13 characters, the key name to 1 character.
// Since V2.0.0, the key will be truncated if it is too long, but you now have a user-friendly displayName to show in the web interface.

// Since V2.0.0 there is a way to upload the firmware over the air (OTA).
// You can set the hostname and the password for OTA in the setupOTA function.

/* Please note:
        struct ConfigOptions {
            const char* keyName;
            const char* category;
            T defaultValue;
            const char* prettyName = nullptr;
            const char* prettyCat = nullptr;
            bool showInWeb = true;
            bool isPassword = false;
            void (*cb)(T) = nullptr;
            std::function<bool()> showIf = nullptr;
        };
*/

ConfigManagerClass cfg;                                               // Create an instance of ConfigManager before using it in structures etc.
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr; // Initialize the logger to nullptr

//--------------------------------------------------------------------
// Forward declarations of functions
void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate); // legacy (blocking) blink – will be phased out
void updateStatusLED();                              // new non-blocking status LED handler
void setHeaterState(bool on);
void cbTestButton();

// -------------------------------------------------------------------
// Global theme override test: make all h1 headings orange without underline
// Served via /user_theme.css and auto-injected by the frontend if present.
// NOTE: We only have setCustomCss() (no _P variant yet) so we pass the PROGMEM string pointer directly.
static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
h3 { color: orange; text-decoration: underline; }
.rw[data-group="sensors"][data-key="temp"] .lab { color:rgba(16, 23, 198, 1); font-weight:900; }
.rw[data-group="sensors"][data-key="temp"] .val { color:rgba(16, 23, 198, 1); font-weight:900; }
.rw[data-group="sensors"][data-key="temp"] .un  { color:rgba(16, 23, 198, 1); font-weight:900; }
)CSS";

// (Server instance moved into conditional above)
int cbTestValue = 0;

// minimal Init
Config<bool> testBool(ConfigOptions<bool>{
    .keyName = "tbool",
    .category = "main",
    .defaultValue = true});


//---------------------------------------------------------------------------------------------------
// extended version with UI-friendly prettyName and prettyCategory
// Improved version since V2.0.0

Config<int> updateInterval(ConfigOptions<int>{
    .keyName = "interval",
    .category = "main",
    .defaultValue = 30,
    .prettyName = "Update Interval (seconds)"});

// Now, these will be truncated and added if their truncated keys are unique:
Config<float> VeryLongCategoryName(ConfigOptions<float>{
    .keyName = "VlongC",
    .category = "VeryLongCategoryName",
    .defaultValue = 0.1f,
    .prettyName = "category Correction long",
    .prettyCat = "key Correction"});

Config<float> VeryLongKeyName(ConfigOptions<float>{
    .keyName = "VeryLongKeyName",
    .category = "Temp",
    .defaultValue = 0.1f,
    .prettyName = "key Correction long",
    .prettyCat = "key Correction"});



//---------------------------------------------------------------------------------------------------
// ---- Temporary dynamic visibility test settings ----
// Category kept short ("DynTest") to avoid key truncation when combined with key names.
Config<bool> tempBoolToggle(ConfigOptions<bool>{
    .keyName = "toggle",
    .category = "DynTest",
    .defaultValue = true,
    .prettyName = "Temp Toggle",
    .prettyCat = "Dynamic Test"});

Config<String> tempSettingAktiveOnTrue(ConfigOptions<String>{
    .keyName = "trueS",
    .category = "DynTest",
    .defaultValue = String("Shown if toggle = true"),
    .prettyName = "Visible When True",
    .prettyCat = "Dynamic Test",
    .showIf = []()
    { return tempBoolToggle.get(); }});

Config<String> tempSettingAktiveOnFalse(ConfigOptions<String>{
    .keyName = "falseS",
    .category = "DynTest",
    .defaultValue = String("Shown if toggle = false"),
    .prettyName = "Visible When False",
    .prettyCat = "Dynamic Test",
    .showIf = []()
    { return !tempBoolToggle.get(); }});
// ---- End temporary dynamic visibility test settings ----


//---------------------------------------------------------------------------------------------------
// Example: Callback usage
void testCallback(int val); // Callback function for testCb here defined, later implemented...
Config<int> testCb(ConfigOptions<int>{
    .keyName = "cbt",
    .category = "main",
    .defaultValue = 0,
    .prettyName = "Test Callback",
    .showInWeb = true,
    .isPassword = false,
    .cb = testCallback});



//---------------------------------------------------------------------------------------------------
// Example: Using structures for grouped settings
// General configuration (structure example)
struct General_Settings
{
    Config<bool> enableController;
    Config<bool> enableMQTT;

    Config<bool> allowOTA;
    Config<String> otaPassword;

    Config<String> Version;

    General_Settings() :

                         enableController(ConfigOptions<bool>{.keyName = "enCtrl", .category = "Limiter", .defaultValue = true, .prettyName = "Enable Limitation"}),
                         enableMQTT(ConfigOptions<bool>{.keyName = "enMQTT", .category = "Limiter", .defaultValue = true, .prettyName = "Enable MQTT Propagation"}),

                         allowOTA(ConfigOptions<bool>{.keyName = "OTAEn", .category = "System", .defaultValue = true, .prettyName = "Allow OTA Updates"}),
                         otaPassword(ConfigOptions<String>{.keyName = "OTAPass", .category = "System", .defaultValue = "ota1234", .prettyName = "OTA Password", .showInWeb = true, .isPassword = true}),
                         Version(ConfigOptions<String>{.keyName = "Version", .category = "System", .defaultValue = String(VERSION), .prettyName = "Program Version"})

    {
        // Register settings with ConfigManager
        cfg.addSetting(&enableController);
        cfg.addSetting(&enableMQTT);

        cfg.addSetting(&allowOTA);
        cfg.addSetting(&otaPassword);

        cfg.addSetting(&Version);
    }
};

General_Settings generalSettings; // Create an instance of General_Settings-Struct

// Example of a structure for WiFi settings
struct WiFi_Settings // wifiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;

    // Shared OptionGroup constants to avoid repetition
    static constexpr OptionGroup WIFI_GROUP{.category ="wifi", .prettyCat = "WiFi Settings"};

    WiFi_Settings() : // Use OptionGroup helpers with shared constexpr instances
                      wifiSsid(WIFI_GROUP.opt<String>("ssid", "MyWiFi", "WiFi SSID")),
                      wifiPassword(WIFI_GROUP.opt<String>("password", "secretpass", "WiFi Password", true, true)),
                      useDhcp(WIFI_GROUP.opt<bool>("dhcp", false, "Use DHCP")),
                      staticIp(WIFI_GROUP.opt<String>("sIP", "192.168.2.126", "Static IP", true, false, nullptr, showIfFalse(useDhcp))),
                      gateway(WIFI_GROUP.opt<String>("GW", "192.168.2.250", "Gateway", true, false, nullptr, showIfFalse(useDhcp))),
                      subnet(WIFI_GROUP.opt<String>("subnet", "255.255.255.0", "Subnet-Mask", true, false, nullptr, showIfFalse(useDhcp)))
    {
        // Register settings with ConfigManager
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
        cfg.addSetting(&staticIp);
        cfg.addSetting(&gateway);
        cfg.addSetting(&subnet);
    }
};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct

// Declaration as a struct with callback function
struct MQTT_Settings
{
    Config<int> mqtt_port;
    Config<String> mqtt_server;
    Config<String> mqtt_username;
    Config<String> mqtt_password;
    Config<String> mqtt_sensor_powerusage_topic;
    Config<String> Publish_Topic;

    // For dynamic topics based on Publish_Topic
    String mqtt_publish_setvalue_topic;
    String mqtt_publish_getvalue_topic;
    String mqtt_publish_Temperature_topic;
    String mqtt_publish_Humidity_topic;
    String mqtt_publish_Dewpoint_topic;

    // Now show extra pretty category name since V2.2.0: e.g., ("keyname", "category", "web displayName", "web Pretty category", defaultValue)
    MQTT_Settings() : mqtt_port(ConfigOptions<int>{.keyName = "Port", .category = "MQTT", .defaultValue = 1883, .prettyName = "Port", }),
                      mqtt_server(ConfigOptions<String>{.keyName = "Server", .category = "MQTT", .defaultValue = String("192.168.2.3"), .prettyName = "Server-IP", }),
                      mqtt_username(ConfigOptions<String>{.keyName = "User", .category = "MQTT", .defaultValue = String("housebattery"), .prettyName = "User", }),
                      mqtt_password(ConfigOptions<String>{.keyName = "Pass", .category = "MQTT", .defaultValue = String("mqttsecret"), .prettyName = "Password", .showInWeb = true, .isPassword = true}),
                      mqtt_sensor_powerusage_topic(ConfigOptions<String>{.keyName = "PUT", .category = "MQTT", .defaultValue = String("emon/emonpi/power1"), .prettyName = "Powerusage Topic", }),
                      Publish_Topic(ConfigOptions<String>{.keyName = "MQTTT", .category = "MQTT", .defaultValue = String("SolarLimiter"), .prettyName = "Publish-Topic", })

    {
        cfg.addSetting(&mqtt_port);
        cfg.addSetting(&mqtt_server);
        cfg.addSetting(&mqtt_username);
        cfg.addSetting(&mqtt_password);
        cfg.addSetting(&mqtt_sensor_powerusage_topic);
        cfg.addSetting(&Publish_Topic);

        // Callback to update topics when Publish_Topic changes
        Publish_Topic.setCallback([this](String newValue)
                                  { this->updateTopics(); });

        updateTopics(); // Make sure topics are initialized
    }

    void updateTopics()
    {
        String hostname = Publish_Topic.get(); // You can throw an error here if it's empty
        mqtt_publish_setvalue_topic = hostname + "/SetValue";
        mqtt_publish_getvalue_topic = hostname + "/GetValue";
        mqtt_publish_Temperature_topic = hostname + "/Temperature";
        mqtt_publish_Humidity_topic = hostname + "/Humidity";
        mqtt_publish_Dewpoint_topic = hostname + "/Dewpoint";
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

// forward declarations
void readBme280();                                                      // read the values from the BME280 (Temperature, Humidity) and calculate the dewpoint
void SetupStartTemperatureMeasuring();                                  // setup the BME280 temperature and humidity sensor
static float computeDewPoint(float temperatureC, float relHumidityPct); // compute the dewpoint from temperature and humidity

float temperature = 0.0; // current temperature in Celsius
float Dewpoint = 0.0;    // current dewpoint in Celsius
float Humidity = 0.0;    // current humidity in percent
float Pressure = 0.0;    // current pressure in hPa

struct TempSettings {
    Config<float> tempCorrection;
    Config<float> humidityCorrection;
    Config<int>   seaLevelPressure;
    Config<int>   readIntervalSec;
    Config<float> dewpointRiskWindow; // ΔT (°C) über Taupunkt, ab der Risiko-Alarm auslöst
    // Shared OptionGroup constants to avoid repetition
    static constexpr OptionGroup TG{.category ="Temp", .prettyCat = "Temperature Settings"};
    TempSettings():
        tempCorrection(TG.opt<float>("TCO", 0.1f, "Temperature Correction")),
        humidityCorrection(TG.opt<float>("HYO", 0.1f, "Humidity Correction")),
        seaLevelPressure(TG.opt<int>("SLP", 1013, "Sea Level Pressure")),
        readIntervalSec(TG.opt<int>("ReadTemp", 30, "Read Temp/Humidity every (s)")),
        dewpointRiskWindow(TG.opt<float>("DPWin", 1.5f, "Dewpoint Risk Window (°C)"))
    {
        cfg.addSetting(&tempCorrection);
        cfg.addSetting(&humidityCorrection);
        cfg.addSetting(&seaLevelPressure);
        cfg.addSetting(&readIntervalSec);
        cfg.addSetting(&dewpointRiskWindow);
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
    // Set logger callback to log in your own way, but do this before using the cfg object!
    // Example 1: use Serial for logging
    // void cbMyConfigLogger(const char *msg){ Serial.println(msg);}
    // ConfigManagerClass::setLogger(cbMyConfigLogger);

    // Or as a lambda function...
    ConfigManagerClass::setLogger([](const char *msg)
                                  {
            Serial.print("[CFG] ");
            Serial.println(msg); });

    //-----------------------------------------------------------------
    cfg.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    cfg.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1);// Register global CSS override
    cfg.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    //----------------------------------------------------------------------------------------------------------------------------------
    // temperature - Sensor settings (BME280) to show how to use settings in your own code


    //-----------------------------------------------------------------
    //example for dynamic settings visibility
    //-----------------------------------------------------------------
    // Register example runtime provider with divider and additional info lines
    //addRuntimeProvider make an section in gui


    //then add the fields to show in gui.
        //Existing Fields:
            //defineRuntimeField (show Value),
            //defineRuntimeString (Show a Static String, with Static Value),
            //defineRuntimeBool (show a boolean Value green on true, white on false, red+blink on alarm),
            //defineRuntimeDivider (show a divider line </hr>)
            //defineRuntimeFieldThresholds (show Value with thresholds for warn and alarm, Warn = yellow and red = Alarm)

    // example for temperature and humidity sensor, with thresholds and alarms
    cfg.addRuntimeProvider({.name = "sensors",
                            .fill = [](JsonObject &o)
                            {
                                // Primary short keys expected by frontend
                                o["temp"] = temperature;
                                o["hum"] = Humidity;
                                o["dew"] = Dewpoint;
                                o["Pressure"] = Pressure;
                            }});

    // Runtime field metadata for dynamic UI
    // With thresholds: warn (yellow) and alarm (red). Example ranges; adjust as needed.
    cfg.defineRuntimeFieldThresholds("sensors", "temp", "Temperature", "°C", 1,
                                     1.0f, 30.0f, // warnMin / warnMax
                                     0.0f, 32.0f, // alarmMin / alarmMax
                                     true, true, true, true, 10);

    cfg.defineRuntimeFieldThresholds("sensors", "hum", "Humidity", "%", 1,
                                     30.0f, 70.0f,
                                     15.0f, 90.0f,
                                     true, false, true, true, 11);

    // only basic field, no thresholds
    cfg.defineRuntimeField("sensors", "dew", "Dewpoint", "°C", 1, 12);
    cfg.defineRuntimeField("sensors", "Pressure", "Pressure", "hPa", 1, 13);




    // Add interactive controls (demo): test button + heater toggle on system group
    cfg.addRuntimeProvider({
        .name = "Hand overrides",
        .fill = [](JsonObject &o){ /* optionally expose current override states later */ }
    });
    // Local state for heater override
    static bool heaterState = false;
    // Action button
    cfg.defineRuntimeButton("Hand overrides", "testBtn", "Test 1", [](){ cbTestButton(); }, 82);
    // Action toggle
    cfg.defineRuntimeCheckbox("Hand overrides", "heater", "Heater", [](){ return heaterState; }, [](bool v){ heaterState = v; setHeaterState(v); }, 83);

    // cfg.defineRuntimeDivider("Hand overrides", "More Controls", 88); // another divider (order 91)
    // Stateful button (acts like on/off toggle with dynamic label states handled client-side) order 92
    static bool stateBtnState = false;
    cfg.defineRuntimeStateButton("Hand overrides", "sb_mode", "Mode Button", [](){ return stateBtnState; }, [](bool v){ stateBtnState = v; Serial.printf("[STATE_BUTTON] -> %s\n", v?"ON":"OFF"); }, /*init*/ false, 91);
    // Int slider (-10..10) order 93
    static int transientIntVal = 0;
    cfg.defineRuntimeIntSlider("Hand overrides", "i_adj", "Int", -10, 10, 0, [](){ return transientIntVal; }, [](int v){ transientIntVal = v; Serial.printf("[INT_SLIDER] -> %d\n", v); }, 92);
    // Float slider (-10..10) with precision 2 order 94
    static float transientFloatVal = 0.0f;
    cfg.defineRuntimeFloatSlider("Hand overrides", "f_adj", "Float", -10.0f, 10.0f, 0.0f, 2, [](){ return transientFloatVal; }, [](float v){ transientFloatVal = v; Serial.printf("[FLOAT_SLIDER] -> %.2f\n", v); }, 93);

    // Example for runtime alarms based on multiple fields, of course you can also use global variables too.
    // Cross-field alarm: temperature within 1.0°C above dewpoint (risk of condensation)
        cfg.defineRuntimeAlarm(
            "dewpoint_risk",
            [](const JsonObject &root)
            {
                float dewpointRiskWindow = tempSettings.dewpointRiskWindow.get();// User-configurable window above dewpoint at which risk alarm triggers
                if (!root.containsKey("sensors"))
                    return false;
                const JsonObject sensors = root["sensors"].as<JsonObject>();
                if (!sensors.containsKey("temp") || !sensors.containsKey("dew"))
                    return false;
                float t = sensors["temp"].as<float>();
                float d = sensors["dew"].as<float>();
                return (t - d) <= dewpointRiskWindow; // risk window
            },
            []()
            { Serial.println("[ALARM] Dewpoint ENTER"); }, // here you could also trigger a relay or similar
            []()
            { Serial.println("[ALARM] Dewpoint EXIT"); });

        // Temperature MIN alarm -> Heater relay ON when temperature below alarmMin (0.5°C) and OFF when recovered.
        // Uses a little hysteresis (enter < 0.0, exit > 0.5) to avoid fast toggling.
        cfg.defineRuntimeAlarm(
            "temp_low",
            [](const JsonObject &root)
            {
                static bool lastState = false; // remember last state for hysteresis
                // Hysteresis: once active keep it on until t > 0.5
                if (lastState)
                { // currently active -> wait until we are clearly above release threshold
                    lastState = (temperature > 0.5f);
                }
                else
                { // currently inactive -> trigger when below entry threshold
                    lastState = (temperature < 0.0f);
                }
                return lastState;
            },
            []()
            {
                Serial.println("[ALARM] -> HEATER ON");
                // digitalWrite(RELAY_HEATER_PIN, HIGH);
            },
            []()
            {
                Serial.println("[ALARM] -> HEATER OFF");
                // digitalWrite(RELAY_HEATER_PIN, LOW);
            });

        cfg.defineRuntimeBool("alarms", "dp_risk", "Dewpoint Risk", true, /*order*/ 100);

        {
            // Custom styling for the too-low-temperature alarm (yellow, no blink, instead of red standard)
            // please note this css derectives will applyed to the element-style, so it cannot be overwritten by themes etc.
            // use GLOBAL_THEME_OVERRIDE for global css changes
            auto tooLowTempStyleOverride = ConfigManagerClass::defaultBoolStyle(true);
            tooLowTempStyleOverride.rule("stateDotOnTrue")
                .set("background", "#f1c40f")
                .set("border", "none")
                .set("boxShadow", "0 0 4px rgba(241,196,15,0.7)")
                .set("animation", "none");
            tooLowTempStyleOverride.rule("stateDotOnAlarm")
                .set("background", "#f1c40f")
                .set("border", "none")
                .set("boxShadow", "0 0 4px rgba(241,196,15,0.7)")
                .set("animation", "none");
            cfg.defineRuntimeBool("alarms", "temp_low", "too low temperature", true, /*order*/ 100, tooLowTempStyleOverride);
        }

    SetupStartTemperatureMeasuring();
    //----------------------------------------------------------------------------------------------------------------------------------

    //----------------------------------------------------------------------------------------------------------------------------------
    // Register other settings
    cfg.addSetting(&updateInterval);
    cfg.addSetting(&testCb);
    cfg.addSetting(&testBool);
    cfg.addSetting(&VeryLongCategoryName);
    cfg.addSetting(&VeryLongKeyName);

    // Register temporary dynamic test settings
    cfg.addSetting(&tempBoolToggle); //show, we have used it in gui, but register it here - no problem, its only showing
    cfg.addSetting(&tempSettingAktiveOnTrue);
    cfg.addSetting(&tempSettingAktiveOnFalse);

    cfg.addSetting(&tempSettings.readIntervalSec);
    //----------------------------------------------------------------------------------------------------------------------------------


    cfg.checkSettingsForErrors();// 2025.09.04 New function to check all settings for errors (e.g., duplicate keys after truncation etc.)

    try
    {
        cfg.loadAll(); // Load all settings from preferences, is nessesary before using the settings!
    }
    catch (const std::exception &e)
    {
        Serial.println(e.what());
    }

    Serial.println("Loaded configuration:");

    generalSettings.Version.set(VERSION); // Update version on device
    cfg.saveAll();                        // Save all settings to preferences

    SetupCheckForAPModeButton();

    delay(300);
    Serial.println("Configuration printout:");
    Serial.println(cfg.toJSON(false));

    // Test setting changes
    testBool.set(false);
    updateInterval.set(15);
    cfg.saveAll();
    delay(300);

    //nesseasary settings for webserver and wifi
    if (wifiSettings.wifiSsid.get().length() == 0)
    {
        Serial.printf("⚠️ SETUP: SSID is empty! [%s]\n", wifiSettings.wifiSsid.get().c_str());
        cfg.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        Serial.printf("🖥️  AP Mode! \n");
        return; // Skip webserver setup in AP mode
    }

    if (wifiSettings.useDhcp.get())
    {
        Serial.println("DHCP enabled");
        cfg.startWebServer(wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }
    else
    {
        Serial.println("DHCP disabled");
        cfg.startWebServer(wifiSettings.staticIp.get(), wifiSettings.gateway.get(), wifiSettings.subnet.get(), wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());
    }

    delay(500); //wait for wifi connection a bit

    cfg.enableWebSocketPush(2000); // 2s Interval for push updates to clients - if not set, ui will use polling


    // if you want to use OTA, set it up here
    if (WiFi.status() == WL_CONNECTED && generalSettings.allowOTA.get())
    {
        cfg.setupOTA("esp32", generalSettings.otaPassword.get().c_str());
    }
    Serial.printf("🖥️ Webserver running at: %s\n", WiFi.localIP().toString().c_str());
}

void loop()
{
    cfg.updateLoopTiming(); // Update internal loop timing metrics for system provider

    //check if wifi is connected, if not try to reconnect
    // Non-blocking status LED update replaces prior blocking blinkBuidInLED calls
    if (WiFi.getMode() != WIFI_AP && WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi not connected!");
        cfg.reconnectWifi();
        // still update LED pattern to 'disconnected'
    }
    updateStatusLED();

    // Example periodic update of testCb value
    static unsigned long lastPrint = 0;
    int interval = max(updateInterval.get(), 1); // Prevent zero interval
    if (millis() - lastPrint > interval * 1000)
    {
        lastPrint = millis();
        // Serial.printf("Loop --> DHCP status: %s\n", useDhcp.get() ? "yes" : "no");
        // Serial.printf("Loop --> WiFi status: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
        cbTestValue++;
        testCb.set(cbTestValue);
        if (cbTestValue > 100)
        {
            cbTestValue = 0;
        }
    }

    //nessesary for webserver and wifi to handle clients and websocket
    cfg.handleClient();
    cfg.handleWebsocketPush();
    cfg.handleOTA();

    // Evaluate cross-field runtime alarms periodically (cheap doc build ~ small JSON)
    static unsigned long lastAlarmEval = 0;
    if (millis() - lastAlarmEval > 1500)
    {
        lastAlarmEval = millis();
        cfg.handleRuntimeAlarms();
    }

    // Small yield without hard delay to keep loop fast; remove large blocking delays elsewhere
    delay(1);
}

void testCallback(int val)
{
    Serial.printf("Callback called with value: %d\n", val);
}

void SetupCheckForAPModeButton()
{
    String APName = "ESP32_Config";
    String pwd = "config1234"; // Default AP password

    Serial.println("Checking AP mode button...");

    if (digitalRead(BUTTON_PIN_AP_MODE) == LOW)
    {
        Serial.printf("AP mode button pressed -> Starting AP with\n --> SSID: %s \n --> Password: %s\n", APName.c_str(), pwd.c_str());

        // Choose preferred AP mode:
        // cfg.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", "config1234");
        // cfg.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", pwd);
        // cfg.startAccessPoint("192.168.4.1", "255.255.255.0", APName, pwd);
        cfg.startAccessPoint("192.168.4.1", "255.255.255.0", APName, "");
        // cfg.startAccessPoint();
    }
}

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

    // output formatted values to serial console
    Serial.println("-----------------------");
    Serial.printf("\r\nTemperature: %2.1lf °C | offset: %2.1lf K", temperature, tempSettings.tempCorrection.get());
    Serial.printf("\r\nHumidity   : %2.1lf %rH | offset: %2.1lf %rH", Humidity, tempSettings.humidityCorrection.get());
    Serial.printf("\r\nDewpoint   : %2.1lf °C", Dewpoint);
    Serial.printf("\r\nPressure   : %4.0lf hPa", Pressure);
    Serial.printf("\r\nAltitude   : %4.2lf m", bme280.data.altitude);
    Serial.println("-----------------------");
}

void blinkBuidInLED(int BlinkCount, int blinkRate)
{
    for (int i = 0; i < BlinkCount; i++)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(blinkRate);
        digitalWrite(LED_BUILTIN, LOW);
        delay(blinkRate);
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

    bool apMode = WiFi.getMode() == WIFI_AP;
    bool connected = !apMode && WiFi.status() == WL_CONNECTED;

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

#define HEATER_PIN 23 // Example pin for heater relay
#define LowActiveRelay true // Set to true if relay is active LOW, false if active HIGH
void setHeaterState(bool on){
    pinMode(HEATER_PIN, OUTPUT); // Example pin for heater relay
    if(on){
        Serial.println("Heater ON");
        digitalWrite(HEATER_PIN, LowActiveRelay ? LOW : HIGH); // Example: turn on heater relay
    } else {
        Serial.println("Heater OFF");
        digitalWrite(HEATER_PIN, LowActiveRelay ? HIGH : LOW); // Example: turn off heater relay
    }
}

void cbTestButton(){
    Serial.println("Test Button pressed!");
}
