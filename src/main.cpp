#include <Arduino.h>
#include "ConfigManager.h"
#include <WebServer.h>
// #include <WiFiClientSecure.h>


#define VERSION "V2.3.0"

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


/*
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

ConfigManagerClass cfg; // Create an instance of ConfigManager before using it in structures etc.
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr; // Initialize the logger to nullptr

WebServer server(80);
int cbTestValue = 0;

// Here, global variables are used without a struct, e.g., Config is a helper class for the settings stored in ConfigManager.h
// 2025.08.17 Breaking Changes in Interface --> now with struct initialization


// minimal Init
Config<bool> testBool(ConfigOptions<bool>{
    .keyName = "tbool",
    .category = "main",
    .defaultValue = true
});


// extended version with UI-friendly prettyName and prettyCategory
// Improved version since V2.0.0
Config<float> TempCorrectionOffset(ConfigOptions<float>{
    .keyName = "TCO",
    .category = "Temp",
    .defaultValue = 0.1f,
    .prettyName = "Temperature Correction",
    .prettyCat = "Temperature Correction Settings"
});
Config<float> HumidityCorrectionOffset(ConfigOptions<float>{
    .keyName = "HYO",
    .category = "Temp",
    .defaultValue = 0.1f,
    .prettyName = "Humidity Correction",
    .prettyCat = "Temperature Correction Settings"
});

Config<int> updateInterval(ConfigOptions<int>{
    .keyName = "interval",
    .category = "main",
    .defaultValue = 30,
    .prettyName = "Update Interval (seconds)"
});

// Now, these will be truncated and added if their truncated keys are unique:
Config<float> VeryLongCategoryName(ConfigOptions<float>{
    .keyName = "VlongC",
    .category = "VeryLongCategoryName",
    .defaultValue = 0.1f,
    .prettyName = "category Correction long",
    .prettyCat = "key Correction"
});
Config<float> VeryLongKeyName(ConfigOptions<float>{
    .keyName = "VeryLongKeyName",
    .category = "Temp",
    .defaultValue = 0.1f,
    .prettyName = "key Correction long",
    .prettyCat = "key Correction"
});


//--------------------------------------------------------------------
// Forward declarations of functions
void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate);

// Example: Callback usage
void testCallback(int val); // Callback function for testCb here defined, later implemented...
Config<int> testCb(ConfigOptions<int>{
    .keyName = "cbt",
    .category = "main",
    .defaultValue = 0,
    .prettyName = "Test Callback",
    .showInWeb = true,
    .isPassword = false,
    .cb = testCallback
});
#pragma endregion
//--------------------------------------------------------------------

// Example: Using structures for grouped settings
// General configuration (structure example)
struct General_Settings
{
    Config<bool> enableController;
    Config<bool> enableMQTT;
    Config<bool> saveDisplay;
    Config<int> displayShowTime;
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<String> Version;

    General_Settings() :
    enableController(ConfigOptions<bool>{ .keyName = "enCtrl", .category = "Limiter", .defaultValue = true, .prettyName = "Enable Limitation" }),
    enableMQTT(ConfigOptions<bool>{ .keyName = "enMQTT", .category = "Limiter", .defaultValue = true, .prettyName = "Enable MQTT Propagation" }),

    saveDisplay(ConfigOptions<bool>{ .keyName = "Save", .category = "Display", .defaultValue = true, .prettyName = "Turn Display Off" }),
    displayShowTime(ConfigOptions<int>{ .keyName = "Time", .category = "Display", .defaultValue = 60, .prettyName = "Display On-Time in Sec" }),

    allowOTA(ConfigOptions<bool>{ .keyName = "OTAEn", .category = "System", .defaultValue = true, .prettyName = "Allow OTA Updates" }),
    otaPassword(ConfigOptions<String>{ .keyName = "OTAPass", .category = "System", .defaultValue = "ota1234", .prettyName = "OTA Password", .showInWeb = true, .isPassword = true }),
    Version(ConfigOptions<String>{ .keyName = "Version", .category = "System", .defaultValue = String(VERSION), .prettyName = "Program Version" })

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
struct WiFi_Settings //wifiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;

    WiFi_Settings() :
    wifiSsid(ConfigOptions<String>{ .keyName = "ssid", .category = "wifi", .defaultValue = "MyWiFi", .prettyName = "WiFi SSID", .prettyCat = "Network Settings" }),
    wifiPassword(ConfigOptions<String>{ .keyName = "password", .category = "wifi", .defaultValue = "secretpass", .prettyName = "WiFi Password", .prettyCat = "Network Settings", .showInWeb = true, .isPassword = true }),
    useDhcp(ConfigOptions<bool>{ .keyName = "dhcp", .category = "network", .defaultValue = false, .prettyName = "Use DHCP", .prettyCat = "Network Settings" }),
    staticIp(ConfigOptions<String>{ .keyName = "sIP", .category = "network", .defaultValue = "192.168.2.126", .prettyName = "Static IP", .prettyCat = "Network Settings" }),
    subnet(ConfigOptions<String>{ .keyName = "subnet", .category = "network", .defaultValue = "255.255.255.0", .prettyName = "Subnet-Mask", .prettyCat = "Network Settings" }),
    gateway(ConfigOptions<String>{ .keyName = "GW", .category = "network", .defaultValue = "192.168.2.250", .prettyName = "Gateway", .prettyCat = "Network Settings" })

    {
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
    mqtt_port(ConfigOptions<int>{ .keyName = "Port", .category = "MQTT", .defaultValue = 1883, .prettyName = "Port", .prettyCat = "MQTT-Section" }),
    mqtt_server(ConfigOptions<String>{ .keyName = "Server", .category = "MQTT", .defaultValue = String("192.168.2.3"), .prettyName = "Server-IP", .prettyCat = "MQTT-Section" }),
    mqtt_username(ConfigOptions<String>{ .keyName = "User", .category = "MQTT", .defaultValue = String("housebattery"), .prettyName = "User", .prettyCat = "MQTT-Section" }),
    mqtt_password(ConfigOptions<String>{ .keyName = "Pass", .category = "MQTT", .defaultValue = String("mqttsecret"), .prettyName = "Password", .prettyCat = "MQTT-Section", .showInWeb = true, .isPassword = true }),
    mqtt_sensor_powerusage_topic(ConfigOptions<String>{ .keyName = "PUT", .category = "MQTT", .defaultValue = String("emon/emonpi/power1"), .prettyName = "Powerusage Topic", .prettyCat = "MQTT-Section" }),
    Publish_Topic(ConfigOptions<String>{ .keyName = "MQTTT", .category = "MQTT", .defaultValue = String("SolarLimiter"), .prettyName = "Publish-Topic", .prettyCat = "MQTT-Section" })
    
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