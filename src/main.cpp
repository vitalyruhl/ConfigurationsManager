#include <Arduino.h>
#include "ConfigManager.h"
#include <WebServer.h>
// #include <WiFiClientSecure.h>


#define VERSION "V2.1.0" // remove throwing errors, because it causes ESP to restart without showing the error message

#define BUTTON_PIN_AP_MODE 13

// ⚠️ Warning ⚠️
// ESP32 has a limitation of 15 characters for the key name.
// The key name is built from the category and the key name (<category>_<key>).
// The category is limited to 13 characters, the key name to 1 character.
// Since V2.0.0, the key will be truncated if it is too long, but you now have a user-friendly displayName to show in the web interface.

// Usage:
// Config<VarType> VarName (const char *name, const char *category, const char* displayName, T defaultValue, bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr)

// Since V2.0.0 there is a way to upload the firmware over the air (OTA).
// You can set the hostname and the password for OTA in the setupOTA function.
// If you leave the hostname empty, it will be set to "esp32-device".
// If you leave the password empty, it will be set to "ota".
// Usage: cfg.setupOTA("Ota-esp32-device", "ota1234"); // Make sure you have a WiFi connection before calling this function!
// You can upload the firmware with the following command:
// pio run --target upload --upload-port <IP_ADDRESS> optional:--upload-flags="
// pio run --target upload --upload-port 192.168.2.126
// or use the web interface http://<IP_ADDRESS>/ota_update

ConfigManagerClass cfg; // Create an instance of ConfigManager before using it in structures etc.
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr; // Initialize the logger to nullptr

WebServer server(80);
int cbTestValue = 0;

// Here, global variables are used without a struct, e.g., Config is a helper class for the settings stored in ConfigManager.h

// 2025.08.17 Breaking Changes in Interface --> add Prettyname 'displayName' for the web interface
Config<int> updateInterval("interval", "main", "Update Interval (seconds)", 30);
Config<bool> testBool("tbool", "main", "test bool", true);

// 2025.09.04 Test long category and key names

// Good old version with short category name:
// Config<float>  TempCorrectionOffset("TCO", "Temp","Temperature Correction", 0.1);
// Config<float>  HumidityCorrectionOffset("HYO", "Temp","Humidity Correction", 0.1);

// Improved version since V2.2.0 with pretty category name: e.g., ("keyname", "category", "web displayName", "web Pretty category", defaultValue)
Config<float>  TempCorrectionOffset("TCO", "Temp","Temperature Correction","Temperature Correction Settings", 0.1);
Config<float>  HumidityCorrectionOffset("HYO", "Temp","Humidity Correction","Temperature Correction Settings", 0.1);

// Now, these will be truncated and added if their truncated keys are unique:
Config<float>  VeryLongCategoryName("VlongC", "VeryLongCategoryName","category Correction long","key Correction", 0.1);
Config<float>  VeryLongKeyName("VeryLongKeyName", "Temp","key Correction long","key Correction", 0.1);


//--------------------------------------------------------------------
// Forward declarations of functions
void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate);

// Example: Callback usage
void testCallback(int val); // Callback function for testCb here defined, later implemented...
Config<int> testCb("cbt", "main", "Test Callback", 0, true, false, testCallback); // since V2.0.0 use displayName for web interface
#pragma endregion
//--------------------------------------------------------------------

// Example: Using structures for grouped settings
// General configuration (structure example)
struct General_Settings
{
    Config<bool> enableController;     // Set to false to disable the controller and use maximum power output
    Config<bool> enableMQTT;           // Set to false to disable the MQTT connection

    Config<bool> saveDisplay; // To turn off the display
    Config<int> displayShowTime; // Time in seconds to show the display after boot or button press (default is 60 seconds, 0 = 10s)

    Config<bool> allowOTA; // Allow OTA updates (default is true, set to false to disable OTA updates)
    Config<String> otaPassword; // Password for OTA updates (default is "ota1234", change it to a secure password)

    Config<String> Version;            // Save the current version of the software

    General_Settings() :enableController("enCtrl", "Limiter","Enable Limitation", true),
                        enableMQTT("enMQTT", "Limiter","Enable MQTT Propagation", true),

                        saveDisplay("Save", "Display", "Turn Display Off", true),
                        displayShowTime("Time", "Display", "Display On-Time in Sec", 60),

                        allowOTA("OTAEn", "System", "Allow OTA Updates", true),
                        otaPassword("OTAPass", "System", "OTA Password", "ota1234", true, true),

                        Version("Version", "System","Program Version", VERSION)
    {
    // Register settings with ConfigManager
        cfg.addSetting(&enableController);
        cfg.addSetting(&enableMQTT);

        cfg.addSetting(&saveDisplay);
        cfg.addSetting(&displayShowTime);

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
    WiFi_Settings() :

                      wifiSsid("ssid", "wifi", "WiFi SSID", "MyWiFi"),
                      wifiPassword("password", "wifi", "WiFi Password", "secretpass", true, true),
                      useDhcp("dhcp", "network", "Use DHCP", false)

    {
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
    }
};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct


// Declaration as a struct with callback function
struct MQTT_Settings {
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
    MQTT_Settings() :
    mqtt_port("Port", "MQTT", "Port", "MQTT-Section", 1883),
    mqtt_server("Server", "MQTT", "Server-IP", "MQTT-Section", String("192.168.2.3")),
    mqtt_username("User", "MQTT", "User", "MQTT-Section", String("housebattery")),
    mqtt_password("Pass", "MQTT", "Password", "MQTT-Section", String("mqttsecret"), true, true),
    mqtt_sensor_powerusage_topic("PUT", "MQTT", "Powerusage Topic", "MQTT-Section", String("emon/emonpi/power1")),
    Publish_Topic("MQTTT", "MQTT", "Publish-Topic", "MQTT-Section", String("SolarLimiter"))
    {
        cfg.addSetting(&mqtt_port);
        cfg.addSetting(&mqtt_server);
        cfg.addSetting(&mqtt_username);
        cfg.addSetting(&mqtt_password);
        cfg.addSetting(&mqtt_sensor_powerusage_topic);
        cfg.addSetting(&Publish_Topic);

    // Callback to update topics when Publish_Topic changes
        Publish_Topic.setCallback([this](String newValue) {
            this->updateTopics();
        });

    updateTopics(); // Make sure topics are initialized
    }

    void updateTopics() {
    String hostname = Publish_Topic.get(); // You can throw an error here if it's empty
        mqtt_publish_setvalue_topic = hostname + "/SetValue";
        mqtt_publish_getvalue_topic = hostname + "/GetValue";
        mqtt_publish_Temperature_topic = hostname + "/Temperature";
        mqtt_publish_Humidity_topic = hostname + "/Humidity";
        mqtt_publish_Dewpoint_topic = hostname + "/Dewpoint";
    }
};

MQTT_Settings mqttSettings; //mqttSettings

#pragma endregion

//--------------------------------------------------------------------

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
    ConfigManagerClass::setLogger([](const char *msg){
            Serial.print("[CFG] ");
            Serial.println(msg);
        });

    //-----------------------------------------------------------------



    // Register settings
    cfg.addSetting(&updateInterval);
    cfg.addSetting(&testCb);
    cfg.addSetting(&testBool);

    cfg.addSetting(&TempCorrectionOffset);
    cfg.addSetting(&HumidityCorrectionOffset);
    cfg.addSetting(&VeryLongCategoryName);
    cfg.addSetting(&VeryLongKeyName);



    // 2025.09.04 New function to check all settings for errors
    cfg.checkSettingsForErrors();


    try
    {
    cfg.loadAll(); // Load all settings from preferences
    }
    catch(const std::exception& e)
    {
         Serial.println(e.what());
    }

    Serial.println("Loaded configuration:");

    generalSettings.Version.set(VERSION); // Update version on device
    cfg.saveAll(); // Save all settings to preferences

    SetupCheckForAPModeButton();

    delay(300);
    Serial.println("Configuration printout:");
    Serial.println(cfg.toJSON(false));

    // Test setting changes
    testBool.set(false);
    updateInterval.set(15);
    cfg.saveAll();
    delay(300);

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
        cfg.startWebServer("192.168.2.126", "255.255.255.0", "192.168.0.250" , wifiSettings.wifiSsid.get(), wifiSettings.wifiPassword.get());

    }
    delay(1500);
    if (WiFi.status() == WL_CONNECTED && generalSettings.allowOTA.get()) {
        cfg.setupOTA("Ota-esp32-device", generalSettings.otaPassword.get().c_str());
    }
    Serial.printf("🖥️ Webserver running at: %s\n", WiFi.localIP().toString().c_str());
}

void loop()
{
    if (WiFi.getMode() == WIFI_AP)
    {
        blinkBuidInLED(3, 100);
    }
    else
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            Serial.println("❌ WiFi not connected!");
            cfg.reconnectWifi();
            delay(1000);
            return;
        }
        blinkBuidInLED(1, 100);
    }

    static unsigned long lastPrint = 0;
    int interval = max(updateInterval.get(), 1); // Prevent zero interval
    if (millis() - lastPrint > interval * 1000)
    {
        lastPrint = millis();
    // Serial.printf("Loop --> DHCP status: %s\n", useDhcp.get() ? "yes" : "no");
    // Serial.printf("Loop --> WiFi status: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
        cbTestValue++;
        testCb.set(cbTestValue);
        if (cbTestValue > 10)
        {
            cbTestValue = 0;
        }
    }

    cfg.handleClient();
    cfg.handleOTA();

    static unsigned long lastOTAmessage = 0;
    if (millis() - lastOTAmessage > 10000) {
        lastOTAmessage = millis();
        Serial.printf("OTA Status: %s\n", cfg.getOTAStatus().c_str());
    }

    delay(500);
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