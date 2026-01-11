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
//     -DCM_ENABLE_RUNTIME_ANALOG_SLIDERS=1
// - You can turn features off by setting =0. The extra build script will also prune the Web UI accordingly.
//
// See docs/FEATURE_FLAGS.md for the complete list and examples. For convenience, this project enables
// most features by default in platformio.ini so you can test and then disable what you don’t need.
// ---------------------------------------------------------------------------------------------------------------------
#include "ConfigManager.h"
// ---------------------------------------------------------------------------------------------------------------------

// Demo defaults (do not store real credentials in repo)
static const char SETTINGS_PASSWORD[] = "cm";
static const char OTA_PASSWORD[] = "ota";

#include <WiFi.h>
#include <esp_wifi.h>
#include <Ticker.h>     // for reading temperature periodically
#include <BME280_I2C.h> // Include BME280 library Temperature and Humidity sensor
#include "Wire.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-BME280-Full-GUI-Demo"
#define BUTTON_PIN_AP_MODE 13

// [WARNING] Warning
// ESP32 has a limitation of 15 characters for the key name.
// The key name is built from the category and the key name (<category>_<key>).
// The category is limited to 13 characters, the key name to 1 character.
// Since V2.0.0, the key will be truncated if it is too long, but you now have a user-friendly displayName to show in the web interface.


extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

//--------------------------------------------------------------------
// Forward declarations of functions
void SetupCheckForAPModeButton();
void SetupCheckForResetButton();
void updateStatusLED(); // new non-blocking status LED handler
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
void setHeaterState(bool on);
void cbTestButton();

// -------------------------------------------------------------------
// Global theme override test: make all h3 headings orange without underline
// Served via /user_theme.css and auto-injected by the frontend if present.
// NOTE: We only have setCustomCss() (no _P variant yet) so we pass the PROGMEM string pointer directly.


static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
.card h3 { color: orange; text-decoration: underline; font-weight: 900 !Important; font-size: 1.2rem !Important; }
.rw[data-group="sensors"][data-key="temp"] .rw{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="sensors"][data-key="temp"] .val{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
.rw[data-group="sensors"][data-key="temp"] .un{ color:rgba(16, 23, 198, 1);font-weight:900;font-size: 1.2rem;}
)CSS";

// #region Examples
// minimal Init
Config<bool> testBool(ConfigOptions<bool>{
    .key = "tbool",
    .category = "Example Settings",
    .defaultValue = true});

//---------------------------------------------------------------------------------------------------

Config<int> updateInterval(ConfigOptions<int>{
    .key = "interval",
    .name = "Update Interval (seconds)",
    .category = "Example Settings",
    .defaultValue = 30});

// Now, these will be truncated and added if their truncated keys are unique:
Config<float> VeryLongCategoryName(ConfigOptions<float>{
    .key = "VlongC",
    .name = "category Correction long",
    .category = "VeryLongCategoryName",
    .defaultValue = 0.1f,
    .categoryPretty = "Category correction long - Example"});

Config<float> VeryLongKeyName(ConfigOptions<float>{
    .key = "VeryLongKeyName",
    .name = "Key correction long",
    .category = "VeryLongCategoryName",
    .defaultValue = 0.1f});

//---------------------------------------------------------------------------------------------------
// ---- Temporary dynamic visibility example ----

Config<bool> tempBoolToggle(ConfigOptions<bool>{
    .key = "toggle",
    .name = "Temp Toggle",
    .category = "Dynamic visibility example",
    .defaultValue = true});

Config<String> tempSettingAktiveOnTrue(ConfigOptions<String>{
    .key = "trueS",
    .name = "Visible When True",
    .category = "Dynamic visibility example",
    .defaultValue = String("Shown if toggle = true"),
    .showIf = []()
    { return tempBoolToggle.get(); }});

Config<String> tempSettingAktiveOnFalse(ConfigOptions<String>{
    .key = "falseS",
    .name = "Visible When False",
    .category = "Dynamic visibility example",
    .defaultValue = String("Shown if toggle = false"),
    .showIf = []()
    { return !tempBoolToggle.get(); }});
// ---- End temporary dynamic visibility example ----

//--------------------------------------------------------------------------------------------------------------
// Example: Using structures for grouped settings
// SystemSettings configuration (structure example)
// #endregion Examples

// #region Structured Settings (System/WiFi/NTP)

struct SystemSettings
{
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<int> wifiRebootTimeoutMin;
    Config<String> version;
    SystemSettings() : allowOTA(ConfigOptions<bool>{.key = "OTAEn", .name = "Allow OTA Updates", .category = "System", .defaultValue = true}),
                       otaPassword(ConfigOptions<String>{.key = "OTAPass", .name = "OTA Password", .category = "System", .defaultValue = String(OTA_PASSWORD), .showInWeb = true, .isPassword = true}),
                       wifiRebootTimeoutMin(ConfigOptions<int>{
                           .key = "WiFiRb",
                           .name = "Reboot if WiFi lost (min)",
                           .category = "System",
                           .defaultValue = 5,
                           .showInWeb = true}),
                       version(ConfigOptions<String>{.key = "P_Version", .name = "Program Version", .category = "System", .defaultValue = String(VERSION)})
    {
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
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
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        ConfigManager.addSetting(&apModePin);
        ConfigManager.addSetting(&resetDefaultsPin);
        ConfigManager.addSetting(&showerRequestPin);
    }
};

SystemSettings systemSettings; // Create an instance of SystemSettings-Struct
ButtonSettings buttonSettings; // Create an instance of ButtonSettings-Struct

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
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        ConfigManager.addSetting(&wifiSsid);
        ConfigManager.addSetting(&wifiPassword);
        ConfigManager.addSetting(&useDhcp);
        ConfigManager.addSetting(&staticIp);
        ConfigManager.addSetting(&gateway);
        ConfigManager.addSetting(&subnet);
        ConfigManager.addSetting(&dnsPrimary);
        ConfigManager.addSetting(&dnsSecondary);
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
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        ConfigManager.addSetting(&frequencySec);
        ConfigManager.addSetting(&server1);
        ConfigManager.addSetting(&server2);
        ConfigManager.addSetting(&tz);
    }
};

NTPSettings ntpSettings; // ntpSettings

// #endregion Structured Settings (System/WiFi/NTP)

// #region Temperature Measurement

//--------------------------------------------------------------------------------------------------------------
// implement read temperature function and variables to show how to use live values
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
static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); } // Shorthand helper for RuntimeManager access

float temperature = 0.0; // current temperature in Celsius
float Dewpoint = 0.0;    // current dewpoint in Celsius
float Humidity = 0.0;    // current humidity in percent
float Pressure = 0.0;    // current pressure in hPa

struct TempSettings // BME280 Settings
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
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        ConfigManager.addSetting(&tempCorrection);
        ConfigManager.addSetting(&humidityCorrection);
        ConfigManager.addSetting(&seaLevelPressure);
        ConfigManager.addSetting(&readIntervalSec);
        ConfigManager.addSetting(&dewpointRiskWindow);
    }
};
TempSettings tempSettings;
//-------------------------------------------------------------------------------------------------------------

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
    bme280.setSeaLevelPressure(tempSettings.seaLevelPressure.get());
    bme280.read();

    temperature = bme280.data.temperature + tempSettings.tempCorrection.get();
    Humidity = bme280.data.humidity + tempSettings.humidityCorrection.get();
    Pressure = bme280.data.pressure;

    Dewpoint = computeDewPoint(temperature, Humidity);
}

void SetupStartTemperatureMeasuring()
{
    Serial.println("[TEMP] Initializing BME280 sensor...");

    // init BME280 for temperature and humidity sensor
    bme280.setAddress(BME280_ADDRESS, I2C_SDA, I2C_SCL);

    // Add timeout protection for BME280 initialization
    unsigned long startTime = millis();
    const unsigned long timeout = 5000; // 5 second timeout

    Serial.println("[TEMP] Starting BME280.begin()...");
    bool isStatus = false;

    isStatus = bme280.begin(
        bme280.BME280_STANDBY_0_5,
        bme280.BME280_FILTER_OFF,
        bme280.BME280_SPI3_DISABLE,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_OVERSAMPLING_1,
        bme280.BME280_MODE_NORMAL);

    if (!isStatus)
    {
        Serial.println("[TEMP] BME280 not initialized - continuing without temperature sensor");
    }
    else
    {
        Serial.println("[TEMP] BME280 ready! Starting temperature ticker...");
        int iv = tempSettings.readIntervalSec.get();
        if (iv < 2){iv = 2;}
        temperatureTicker.attach((float)iv, readBme280); // Attach ticker with configured interval
        readBme280();                                    // Read once at startup
    }

    Serial.println("[TEMP] Temperature setup completed");
}

// #endregion Temperature Measurement

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
            Serial.println(msg);
        });

    //-----------------------------------------------------------------
    ConfigManager.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setVersion(VERSION); // Set the application version for web UI display
    // Optional demo: global CSS override
    ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1); // Register global CSS override
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD); // Set the settings password from wifiSecret.h
    ConfigManager.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    //----------------------------------------------------------------------------------------------------------------------------------

    //----------------------------------------------------------------------------------------------------------------------------------
    // Register individual settings (non-structured)
    ConfigManager.addSetting(&updateInterval);
    ConfigManager.addSetting(&testBool);
    ConfigManager.addSetting(&VeryLongCategoryName);
    ConfigManager.addSetting(&VeryLongKeyName);

    // Register temporary dynamic test settings
    ConfigManager.addSetting(&tempBoolToggle); // show, we have used it in gui, but register it here - no problem, its only showing
    ConfigManager.addSetting(&tempSettingAktiveOnTrue);
    ConfigManager.addSetting(&tempSettingAktiveOnFalse);

    // Initialize structured settings using Delayed Initialization Pattern
    // This avoids static initialization order problems - see docs/SETTINGS_STRUCTURE_PATTERN.md
    systemSettings.init();    // System settings (OTA, version, etc.)
    buttonSettings.init();    // GPIO button configuration
    tempSettings.init();      // BME280 temperature sensor settings
    ntpSettings.init();       // NTP time synchronization settings
    wifiSettings.init();      // WiFi connection settings

    //----------------------------------------------------------------------------------------------------------------------------------

    ConfigManager.checkSettingsForErrors(); // 2025.09.04 New function to check all settings for errors (e.g., duplicate keys after truncation etc.)

    ConfigManager.loadAll(); // Load all settings from preferences, is necessary before using the settings!

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
    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");   // Prefer this AP, fallback to others

    SetupCheckForResetButton();
    
    SetupCheckForAPModeButton(); // check for AP mode button AFTER setting WiFi credentials

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

    // Enhanced WebSocket configuration
    ConfigManager.enableWebSocketPush(); // Enable WebSocket push for real-time updates
    ConfigManager.setWebSocketInterval(1000); // Faster updates - every 1 second
    ConfigManager.setPushOnConnect(true);     // Immediate data on client connect

    SetupStartTemperatureMeasuring();
    //----------------------------------------------------------------------------------------------------------------------------------

    Serial.println("Loaded configuration:");

    // Show correct IP address depending on WiFi mode
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        Serial.printf("[INFO] Webserver running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[INFO] Webserver running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[INFO] Webserver running (IP not available)");
    }

    Serial.println("Configuration printout:");
    Serial.println(ConfigManager.toJSON(true)); // Show ALL settings, not just web-visible ones

    Serial.println("\nSetup completed successfully!");

    // NOTE: Avoid auto-modifying and persisting settings in examples.

    Serial.println("\n[MAIN] Setup completed successfully! Starting main loop...");
    Serial.println("=================================================================");
}

void loop()
{

    //-------------------------------------------------------------------------------------------------------------
    // for working with the ConfigManager nessesary in loop
    ConfigManager.updateLoopTiming(); // Update internal loop timing metrics for system provider
    ConfigManager.getWiFiManager().update(); // Update WiFi Manager - handles all WiFi logic
    ConfigManager.handleClient(); // Handle web server client requests
    ConfigManager.handleWebsocketPush(); // Handle WebSocket push updates
    ConfigManager.handleOTA();           // Handle OTA updates
    ConfigManager.handleRuntimeAlarms(); // Handle runtime alarms
    //-------------------------------------------------------------------------------------------------------------


    static unsigned long lastLoopLog = 0;
    if (millis() - lastLoopLog > 60000) { // Every 60 seconds
        lastLoopLog = millis();
        Serial.printf("[MAIN] Loop running, WiFi status: %d, heap: %d\n", WiFi.status(), ESP.getFreeHeap());
    }


    // Evaluate cross-field runtime alarms periodically (cheap doc build ~ small JSON)
    static unsigned long lastAlarmEval = 0;
    if (millis() - lastAlarmEval > 1500)
    {
        lastAlarmEval = millis();
        CRM().updateAlarms(); // shows how to use shortcut helper CRM() instead of CRM()
    }


    updateStatusLED();
    delay(10);
}

//----------------------------------------
// GUI SETUP
//----------------------------------------

void setupGUI()
{
    Serial.println("[GUI] setupGUI() start");
    // #region BME280 Sensor Display with Runtime Providers

        // Register sensor runtime provider for BME280 data
        Serial.println("[GUI] Adding runtime provider: sensors");
        CRM().addRuntimeProvider("sensors", [](JsonObject &data)
        {
            // Apply precision to sensor values to reduce JSON size
            data["temp"] = roundf(temperature * 10.0f) / 10.0f;     // 1 decimal place
            data["hum"] = roundf(Humidity * 10.0f) / 10.0f;        // 1 decimal place
            data["dew"] = roundf(Dewpoint * 10.0f) / 10.0f;        // 1 decimal place
            data["pressure"] = roundf(Pressure * 10.0f) / 10.0f;   // 1 decimal place
        },2);

        // Define sensor display fields using addRuntimeMeta
        Serial.println("[GUI] Adding meta: sensors.temp");
        RuntimeFieldMeta tempMeta;
        tempMeta.group = "sensors";
        tempMeta.key = "temp";
        tempMeta.label = "Temperature";
        tempMeta.unit = "°C";
        tempMeta.precision = 1;
        tempMeta.order = 10;
        CRM().addRuntimeMeta(tempMeta);

        Serial.println("[GUI] Adding meta: sensors.hum");
        RuntimeFieldMeta humMeta;
        humMeta.group = "sensors";
        humMeta.key = "hum";
        humMeta.label = "Humidity";
        humMeta.unit = "%";
        humMeta.precision = 1;
        humMeta.order = 11;
        CRM().addRuntimeMeta(humMeta);

        Serial.println("[GUI] Adding meta: sensors.dew");
        RuntimeFieldMeta dewMeta;
        dewMeta.group = "sensors";
        dewMeta.key = "dew";
        dewMeta.label = "Dewpoint";
        dewMeta.unit = "°C";
        dewMeta.precision = 1;
        dewMeta.order = 12;
        CRM().addRuntimeMeta(dewMeta);

        Serial.println("[GUI] Adding meta: sensors.pressure");
        RuntimeFieldMeta pressureMeta;
        pressureMeta.group = "sensors";
        pressureMeta.key = "pressure";
        pressureMeta.label = "Pressure";
        pressureMeta.unit = "hPa";
        pressureMeta.precision = 1;
        pressureMeta.order = 13;
        CRM().addRuntimeMeta(pressureMeta);

        // Add runtime provider for sensor range field
        Serial.println("[GUI] Adding meta: sensors.range");
        RuntimeFieldMeta rangeMeta;
        rangeMeta.group = "sensors";
        rangeMeta.key = "range";
        rangeMeta.label = "Sensor Range";
        rangeMeta.unit = "V";
        rangeMeta.precision = 1;
        rangeMeta.order = 14;
        CRM().addRuntimeMeta(rangeMeta);
    // #endregion BME280 Sensor Display with Runtime Providers

    // #region Controls Card with Buttons, Toggles, and Sliders
        // Add interactive controls provider
        Serial.println("[GUI] Adding runtime provider: controls");
        CRM().addRuntimeProvider("controls", [](JsonObject &data)
        {
            // Optionally expose control states
        },3);

        // Example button
        Serial.println("[GUI] Defining runtime button: controls.testBtn");
        ConfigManager.defineRuntimeButton("controls", "testBtn", "Test Button", []()
        {
            cbTestButton();
        }, "", 20);

        // Example toggle slider
        static bool heaterState = false;
        Serial.println("[GUI] Defining runtime checkbox: controls.heater");
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
        Serial.println("[GUI] Defining runtime state button: controls.fan");
        ConfigManager.defineRuntimeStateButton("controls", "fan", "Fan", []()
        {
            return fanState;
        }, [](bool state)
        {
            fanState = state;
            setFanState(state);
            Serial.printf("[FAN] State: %s\n", state ? "ON" : "OFF");
        }, false, "", 22);

        // Divider between discrete controls (buttons/toggles) and analog controls
        Serial.println("[GUI] Adding meta divider: controls.analogDivider");
        RuntimeFieldMeta analogDividerMeta;
        analogDividerMeta.group = "controls";
        analogDividerMeta.key = "analogDivider";
        analogDividerMeta.label = "Analog";
        analogDividerMeta.isDivider = true;
        analogDividerMeta.order = 23;
        CRM().addRuntimeMeta(analogDividerMeta);

        // Integer slider for adjustments (Note this is no persistent setting)
        static int adjustValue = 0;
        auto getAdjustValue = []() -> int { return adjustValue; };
        auto setAdjustValue = [](int value) {
            adjustValue = value;
            Serial.printf("[ADJUST] Value: %d\n", value);
        };


        Serial.println("[GUI] Defining runtime int value: controls.adjustValue");
        ConfigManager.defineRuntimeIntValue(
            "controls",
            "adjustValue",
            "Adjustment Value",
            -10,
            10,
            0,
            getAdjustValue,
            setAdjustValue,
            "Unit",
            "steps",
            24
        );

        // Divider between analog value and slider controls
        Serial.println("[GUI] Adding meta divider: controls.analogDivider2");
        RuntimeFieldMeta analogDividerMeta2;
        analogDividerMeta2.group = "controls";
        analogDividerMeta2.key = "analogDivider2";
        analogDividerMeta2.label = "Analog";
        analogDividerMeta2.isDivider = true;
        analogDividerMeta2.order = 24;
        CRM().addRuntimeMeta(analogDividerMeta2);

        Serial.println("[GUI] Defining runtime int slider: controls.adjust");
        ConfigManager.defineRuntimeIntSlider(
            "controls",
            "adjust",
            "Adjustment",
            -10,
            10,
            0,
            getAdjustValue,
            setAdjustValue,
            "UNIT",
            "steps",
            25
        );

        // Float slider synchronized with the Temp setting (Temp.TCO)
        Serial.println("[GUI] Defining runtime float slider: controls.tempOffset");
        ConfigManager.defineRuntimeFloatSlider(
            "controls",
            "tempOffset",
            "Temperature Offset",
            -5.0f,
            5.0f,
            tempSettings.tempCorrection.get(),
            2,
            []() {
                return tempSettings.tempCorrection.get();
            },
            [](float value) {
                tempSettings.tempCorrection.set(value);
                Serial.printf("[TEMP_OFFSET] Value: %.2f°C\n", value);
            },
            "°C",
            "",
            26
        );

    // #endregion Controls Card with Buttons, Toggles, and Sliders


    // #region Alarms
   
        // GUI Boolean alarm example (registered in runtime alarm system)
        Serial.println("[GUI] Defining runtime alarm: alerts.overheat");
        ConfigManager.defineRuntimeAlarm("alerts", "overheat", "Overheat Warning", []() {
            return temperature > 40.0; // Trigger at 40°C for demo
        });

        // Alert status display using addRuntimeMeta for boolean values (separate from runtime alarm system)
        Serial.println("[GUI] Adding runtime provider: alerts");
        CRM().addRuntimeProvider("alerts", [](JsonObject &data)
        {
            // Connection status (also shown in Alerts card)
            data["connected"] = WiFi.status() == WL_CONNECTED;

            // Runtime alarm status (registered via defineRuntimeAlarm)
            data["overheat"] = CRM().isRuntimeAlarmActive("alerts.overheat");

            // Dewpoint risk alarm: temperature is within risk window of dewpoint
            bool dewpointRisk = false;
            if (!isnan(temperature) && !isnan(Dewpoint)) {
                float riskWindow = tempSettings.dewpointRiskWindow.get(); // Default 1.5°C
                float tempDelta = temperature - Dewpoint;
                dewpointRisk = (tempDelta <= riskWindow) && (tempDelta >= 0);
            }

            // Low temperature alarm: temperature below 10°C
            bool tempLow = false;
            if (!isnan(temperature)) {
                tempLow = temperature < 10.0f;
            }

            data["dewpoint_risk"] = dewpointRisk;
            data["temp_low"] = tempLow;
        },1);

        Serial.println("[GUI] Adding meta: alerts.connected");
        RuntimeFieldMeta connectedMeta;
        connectedMeta.group = "alerts";
        connectedMeta.key = "connected";
        connectedMeta.label = "Connected";
        connectedMeta.order = 29;
        connectedMeta.isBool = true;
        CRM().addRuntimeMeta(connectedMeta);

        Serial.println("[GUI] Adding meta: alerts.overheat");
        RuntimeFieldMeta overheatMeta;
        overheatMeta.group = "alerts";
        overheatMeta.key = "overheat";
        overheatMeta.label = "Overheat Warning";
        overheatMeta.order = 28;
        overheatMeta.isBool = true;
        overheatMeta.hasAlarm = true;
        overheatMeta.alarmWhenTrue = true;
        overheatMeta.boolAlarmValue = true;
        CRM().addRuntimeMeta(overheatMeta);

        Serial.println("[GUI] Adding meta: alerts.dewpoint_risk");
        RuntimeFieldMeta dewpointRiskMeta;
        dewpointRiskMeta.group = "alerts";
        dewpointRiskMeta.key = "dewpoint_risk";
        dewpointRiskMeta.label = "Condensation Risk";
        dewpointRiskMeta.order = 30;
        dewpointRiskMeta.isBool = true;
        dewpointRiskMeta.hasAlarm = true;
        dewpointRiskMeta.alarmWhenTrue = true;
        dewpointRiskMeta.boolAlarmValue = true; // highlight when true
        CRM().addRuntimeMeta(dewpointRiskMeta);

        Serial.println("[GUI] Adding meta: alerts.temp_low");
        RuntimeFieldMeta tempLowMeta;
        tempLowMeta.group = "alerts";
        tempLowMeta.key = "temp_low";
        tempLowMeta.label = "Low Temperature Alert";
        tempLowMeta.order = 31;
        tempLowMeta.isBool = true;
        tempLowMeta.hasAlarm = true;
        tempLowMeta.alarmWhenTrue = true;
        tempLowMeta.boolAlarmValue = true; // highlight when true
        CRM().addRuntimeMeta(tempLowMeta);
    // #endregion Alarms


    // #region test injection example (do not override built-in system provider)
    // NOTE: Do NOT register a custom runtime provider named "system".
    // It will override the built-in System provider (enabled via ConfigManager.enableBuiltinSystemProvider())
    // and you will lose the default fields (RSSI/IP/Gateway/DateTime/etc.).
    //
    // If you want to add test data, use a separate group/card like "system_test".
    // If you really need to inject into the System card, the runtime engine needs provider chaining/merging.
    //
    // Serial.println("[GUI] Adding runtime provider: system_test");
    // CRM().addRuntimeProvider("system_test", [](JsonObject &data)
    // {
    //     data["testValue"] = 42;
    // }, 99);
    //
    // Serial.println("[GUI] Adding meta: system_test.testValue");
    // RuntimeFieldMeta testValueMeta;
    // testValueMeta.group = "system_test";
    // testValueMeta.key = "testValue";
    // testValueMeta.label = "Test Value";
    // testValueMeta.order = 99;
    // CRM().addRuntimeMeta(testValueMeta);
    // #endregion test injection example


    Serial.println("[GUI] setupGUI() end");
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
            ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());
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
            ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)systemSettings.wifiRebootTimeoutMin.get());
        }
    }

    return true; // Webserver setup completed
}

void onWiFiConnected()
{
    Serial.println("[MAIN] WiFi connected! Activating services...");

    if (!tickerActive)
    {
        // Ensure ArduinoOTA is initialized once WiFi is connected and OTA is allowed
        // This runs on every (re)connection to guarantee espota responds
        if (systemSettings.allowOTA.get() && !ConfigManager.getOTAManager().isInitialized())
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

