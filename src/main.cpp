#include <Arduino.h>
#include "ConfigManager.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <Ticker.h>     // for reading temperature periodically
#include <BME280_I2C.h> // Include BME280 library Temperature and Humidity sensor
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define VERSION "V2.7.0" // 2025.11.02
#define APP_NAME "CM-BME280-Demo"


//--------------------------------------------------------------------------------------------------------------
// set the I2C address for the BME280 sensor for temperature and humidity
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_FREQUENCY 400000
#define BME280_FREQUENCY 400000
// #define BME280_ADDRESS 0x76 // I2C address for the BME280 sensor (default is 0x76) redefine, if needed
//-------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------
// Forward declarations of functions
void setupGUI();
bool SetupStartWebServer();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();
void SetupStartTemperatureMeasuring();
void readBme280();
void readBme280(); // read the values from the BME280 (Temperature, Humidity) and calculate the dewpoint
void SetupStartTemperatureMeasuring(); // setup the BME280 temperature and humidity sensor

// Global objects and variables
#include "secret/wifiSecret.h" // Include your WiFi credentials here
extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp
BME280_I2C bme280;
Ticker temperatureTicker;
bool tickerActive = false; // flag to indicate if the ticker is active
Ticker NtpSyncTicker;

static float computeDewPoint(float temperatureC, float relHumidityPct); // compute the dewpoint from temperature and humidity
static inline ConfigManagerRuntime &CRM() { return ConfigManager.getRuntime(); } // Shorthand helper for RuntimeManager access

float temperature = 0.0; // current temperature in Celsius
float Dewpoint = 0.0;    // current dewpoint in Celsius
float Humidity = 0.0;    // current humidity in percent
float Pressure = 0.0;    // current pressure in hPa


// -------------------------------------------------------------------
// Global theme override test: make all h3 headings orange without underline
static const char GLOBAL_THEME_OVERRIDE[] PROGMEM = R"CSS(
    h3 {color:orange;text-decoration:underline;}
)CSS";

//--------------------------------------------------------------------------------------------------------------
// nessesary Settings, you dont need to make them, but they are needed for the ConfigManager to work properly
struct SystemSettings
{
    Config<bool> allowOTA;
    Config<String> otaPassword;

    Config<String> version;
    SystemSettings() : allowOTA(ConfigOptions<bool>{.key = "OTAEn", .name = "Allow OTA Updates", .category = "System", .defaultValue = true}),
                       otaPassword(ConfigOptions<String>{.key = "OTAPass", .name = "OTA Password", .category = "System", .defaultValue = String(OTA_PASSWORD), .showInWeb = true, .isPassword = true}),
                       version(ConfigOptions<String>{.key = "P_Version", .name = "Program Version", .category = "System", .defaultValue = String(VERSION)})
                       {}
    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        ConfigManager.addSetting(&allowOTA);
        ConfigManager.addSetting(&otaPassword);
        ConfigManager.addSetting(&version);
    }
};

SystemSettings systemSettings; // Create an instance of SystemSettings-Struct

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
    Config<int> wifiRebootTimeoutMin;

    WiFi_Settings() : wifiSsid(ConfigOptions<String>{.key = "WiFiSSID", .name = "WiFi SSID", .category = "WiFi", .defaultValue = "", .showInWeb = true, .sortOrder = 1}),
                      wifiPassword(ConfigOptions<String>{.key = "WiFiPassword", .name = "WiFi Password", .category = "WiFi", .defaultValue = "secretpass", .showInWeb = true, .isPassword = true, .sortOrder = 2}),
                      useDhcp(ConfigOptions<bool>{.key = "WiFiUseDHCP", .name = "Use DHCP", .category = "WiFi", .defaultValue = true, .showInWeb = true, .sortOrder = 3}),
                      staticIp(ConfigOptions<String>{.key = "WiFiStaticIP", .name = "Static IP", .category = "WiFi", .defaultValue = "192.168.2.131", .sortOrder = 4, .showIf = [this](){ return !useDhcp.get(); }}),
                      gateway(ConfigOptions<String>{.key = "WiFiGateway", .name = "Gateway", .category = "WiFi", .defaultValue = "192.168.2.250", .sortOrder = 5, .showIf = [this](){ return !useDhcp.get(); }}),
                      subnet(ConfigOptions<String>{.key = "WiFiSubnet", .name = "Subnet Mask", .category = "WiFi", .defaultValue = "255.255.255.0", .sortOrder = 6, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsPrimary(ConfigOptions<String>{.key = "WiFiDNS1", .name = "Primary DNS", .category = "WiFi", .defaultValue = "192.168.2.250", .sortOrder = 7, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsSecondary(ConfigOptions<String>{.key = "WiFiDNS2", .name = "Secondary DNS", .category = "WiFi", .defaultValue = "8.8.8.8", .sortOrder = 8, .showIf = [this](){ return !useDhcp.get(); }}),
                      wifiRebootTimeoutMin(ConfigOptions<int>{
                           .key = "WiFiRb",
                           .name = "Reboot if WiFi lost (min)",
                           .category = "System",
                           .defaultValue = 5,
                           .showInWeb = true})
                    {}
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
        ConfigManager.addSetting(&wifiRebootTimeoutMin);
    }

};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct

struct TempSettings
{
    Config<float> tempCorrection;
    Config<float> humidityCorrection;
    Config<int> seaLevelPressure;
    Config<int> readIntervalSec;

    TempSettings() : tempCorrection(ConfigOptions<float>{.key = "TCO", .name = "Temperature Correction", .category = "Temp", .defaultValue = 0.1f}),
                     humidityCorrection(ConfigOptions<float>{.key = "HYO", .name = "Humidity Correction", .category = "Temp", .defaultValue = 0.1f}),
                     seaLevelPressure(ConfigOptions<int>{.key = "SLP", .name = "Sea Level Pressure", .category = "Temp", .defaultValue = 1013}),
                     readIntervalSec(ConfigOptions<int>{.key = "ReadTemp", .name = "Read Temp/Humidity every (s)", .category = "Temp", .defaultValue = 30})
                     {}

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        ConfigManager.addSetting(&tempCorrection);
        ConfigManager.addSetting(&humidityCorrection);
        ConfigManager.addSetting(&seaLevelPressure);
        ConfigManager.addSetting(&readIntervalSec);
    }
};
TempSettings tempSettings;

// //--------------------------------------------------------------------------------------------------------------
// // NTP Settings
// struct NTPSettings
// {
//     Config<int> frequencySec; // Sync frequency (seconds)
//     Config<String> server1;   // Primary NTP server
//     Config<String> server2;   // Secondary NTP server
//     Config<String> tz;        // POSIX/TZ string for local time
//     NTPSettings() : frequencySec(ConfigOptions<int>{.key = "NTPFrq", .name = "NTP Sync Interval (s)", .category = "NTP", .defaultValue = 3600, .showInWeb = true}),
//                     server1(ConfigOptions<String>{.key = "NTP1", .name = "NTP Server 1", .category = "NTP", .defaultValue = String("192.168.2.250"), .showInWeb = true}),
//                     server2(ConfigOptions<String>{.key = "NTP2", .name = "NTP Server 2", .category = "NTP", .defaultValue = String("pool.ntp.org"), .showInWeb = true}),
//                     tz(ConfigOptions<String>{.key = "NTPTZ", .name = "Time Zone (POSIX)", .category = "NTP", .defaultValue = String("CET-1CEST,M3.5.0/02,M10.5.0/03"), .showInWeb = true})
//     {
//         // Constructor - do not register here due to static initialization order
//     }

//     void init() {
//         // Register settings with ConfigManager after ConfigManager is ready
//         ConfigManager.addSetting(&frequencySec);
//         ConfigManager.addSetting(&server1);
//         ConfigManager.addSetting(&server2);
//         ConfigManager.addSetting(&tz);
//     }
// };

// NTPSettings ntpSettings; // ntpSettings



void setup()
{
    Serial.begin(115200);

    //-----------------------------------------------------------------
    // Set logger callback
    ConfigManagerClass::setLogger([](const char *msg)
        {
            Serial.print("[ConfigManager] ");
            Serial.println(msg);
        });

    //-----------------------------------------------------------------
    // Set APP information and global CSS override
    ConfigManager.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setVersion(VERSION); // Set the application version for web UI display
    // ConfigManager.setCustomCss(GLOBAL_THEME_OVERRIDE, sizeof(GLOBAL_THEME_OVERRIDE) - 1); // Register global CSS override
    // ConfigManager.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    // ConfigManager.setSettingsPassword(SETTINGS_PASSWORD); // Set the settings password from wifiSecret.h
    //----------------------------------------------------------------------------------------------------------------------------------

    //----------------------------------------------------------------------------------------------------------------------------------
    //register your Settings here
    systemSettings.init();    // System settings (OTA, version, etc.)
    wifiSettings.init();      // WiFi connection settings
    tempSettings.init();      // BME280 temperature sensor settings
    // ntpSettings.init();       // NTP time synchronization settings
    //----------------------------------------------------------------------------------------------------------------------------------

    ConfigManager.loadAll(); // Load all settings from preferences, is necessary before using the settings!

    //----------------------------------------------------------------------------------------------------------------------------------
    // Configure Smart WiFi Roaming with default values (can be customized in setup if needed)
    ConfigManager.enableSmartRoaming(true);            // Re-enabled now that WiFi stack is fixed
    ConfigManager.setRoamingImprovement(10);           // Require 10 dBm improvement

    //----------------------------------------------------------------------------------------------------------------------------------
    // Configure WiFi AP MAC filtering/priority (example - customize as needed)
    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");   // Prefer this AP, fallback to others

    //----------------------------------------------------------------------------------------------------------------------------------
    // set wifi settings if not set yet from my secret folder (its a behavior to easy testing, but you can set it in AP-Mode)
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

    // perform the wifi connection
    SetupStartWebServer();

    setupGUI();

    // Enhanced WebSocket configuration
    ConfigManager.enableWebSocketPush(); // Enable WebSocket push for real-time updates
    ConfigManager.setWebSocketInterval(1000); // Faster updates - every 1 second
    ConfigManager.setPushOnConnect(true);     // Immediate data on client connect

    SetupStartTemperatureMeasuring();

    // Show correct IP address depending on WiFi mode
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        Serial.printf("🖥️ Webserver running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("🖥️ Webserver running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("🖥️ Webserver running (IP not available)");
    }

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

}



//----------------------------------------
// GUI SETUP
//----------------------------------------

void setupGUI()
{
    Serial.println("[GUI] setupGUI() start");
    //-----------------------------------------------------------------
    // BME280 Sensor Display with Runtime Providers
    //-----------------------------------------------------------------

    // Register sensor runtime provider for BME280 data
    CRM().addRuntimeProvider("sensors", [](JsonObject &data)
    {
        data["temp"] = temperature;
        data["hum"] = Humidity;
        data["dew"] = Dewpoint;
        data["pressure"] = Pressure;
    });

    // Define sensor display fields using addRuntimeMeta
    RuntimeFieldMeta tempMeta;
    tempMeta.group = "sensors";
    tempMeta.key = "temp";
    tempMeta.label = "Temperature";
    tempMeta.unit = "°C";
    tempMeta.precision = 1;
    tempMeta.order = 10;
    CRM().addRuntimeMeta(tempMeta);

    RuntimeFieldMeta humMeta;
    humMeta.group = "sensors";
    humMeta.key = "hum";
    humMeta.label = "Humidity";
    humMeta.unit = "%";
    humMeta.precision = 1;
    humMeta.order = 11;
    CRM().addRuntimeMeta(humMeta);

    RuntimeFieldMeta dewMeta;
    dewMeta.group = "sensors";
    dewMeta.key = "dew";
    dewMeta.label = "Dewpoint";
    dewMeta.unit = "°C";
    dewMeta.precision = 1;
    dewMeta.order = 12;
    CRM().addRuntimeMeta(dewMeta);

    RuntimeFieldMeta pressureMeta;
    pressureMeta.group = "sensors";
    pressureMeta.key = "pressure";
    pressureMeta.label = "Pressure";
    pressureMeta.unit = "hPa";
    pressureMeta.precision = 1;
    pressureMeta.order = 13;
    CRM().addRuntimeMeta(pressureMeta);

    RuntimeFieldMeta rangeMeta;
    rangeMeta.group = "sensors";
    rangeMeta.key = "range";
    rangeMeta.label = "Sensor Range";
    rangeMeta.unit = "V";
    rangeMeta.precision = 1;
    rangeMeta.order = 14;
    CRM().addRuntimeMeta(rangeMeta);

}

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
    Serial.printf("[MAIN] BSSID: %s (Channel: %d)\n", bssid.c_str(), WiFi.channel());
    Serial.printf("[MAIN] Local MAC: %s\n\n", WiFi.macAddress().c_str());

    // // Start NTP sync now and schedule periodic resyncs
    // auto doNtpSync = []()
    // {
    //     // Use TZ-aware sync for correct local time (Berlin: CET/CEST)
    //     configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str());
    // };
    // doNtpSync();
    // NtpSyncTicker.detach();
    // int ntpInt = ntpSettings.frequencySec.get();
    // if (ntpInt < 60)
    //     ntpInt = 3600; // default to 1 hour
    // NtpSyncTicker.attach(ntpInt, +[]()
    //                              { configTzTime(ntpSettings.tz.get().c_str(), ntpSettings.server1.get().c_str(), ntpSettings.server2.get().c_str()); });
}

void onWiFiDisconnected()
{
    Serial.println("[MAIN] WiFi disconnected! Deactivating services...");

    if (tickerActive)
    {
        // Stop OTA if it should be disabled
        if (systemSettings.allowOTA.get() == false && ConfigManager.getOTAManager().isInitialized())
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

