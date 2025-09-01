#include <Arduino.h>
#include "ConfigManager.h"
#include <WebServer.h>
// #include <WiFiClientSecure.h>
#include <WebServer.h>

#define VERSION "V2.0.1" // remove throwing errors, becaus it let esp restart without showing the error message

#define BUTTON_PIN_AP_MODE 13

// ⚠️ Warning ⚠️
// ESP 32 has a limitation of 15 chars for the key name.
// The key name is build from the category and the key name. (<category>_<key>).
// The category is limited to 13 chars, the key name to 1 char.
// Since V2.0.0 the key will be truncated if it is to long, but you have now a userfriendly displayName to show in the web interface.

// Usage:
// Config<VarType> VarName (const char *name, const char *category, const char* displayName, T defaultValue, bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr)

//since V2.0.0 Thera is a way to upload the firmware over the air (OTA).
    //You can set the hostname and the password for the OTA in the setupOTA function.
    //If you leave the hostname empty, it will be set to "esp32-device".
    //If you leave the password empty, it will be set to "ota".
    //Usage: cfg.setupOTA("Ota-esp32-device", "ota1234"); //be sure you have a wifi connection before calling this function!
    //you can uplaod the firmware with the following command:
    //pio run --target upload --upload-port <IP_ADDRESS> optional:--upload-flags="
    //pio run --target upload --upload-port 192.168.2.126
    //or use web-interface http://<IP_ADDRESS>/ota_update

ConfigManagerClass cfg; // Create an instance of ConfigManager before using it in structures etc.
ConfigManagerClass::LogCallback ConfigManagerClass::logger = nullptr; // Initialize the logger to nullptr

WebServer server(80);
int cbTestValue = 0;

// here used global variables without struct eg Config is an helper class for the settings stored in the ConfigManager.h

// 2025.08.17 Breacking Changes in Interface --> add Prettyname 'displayName' for the web interface
Config<String> wifiSsid("ssid", "wifi", "WiFi SSID", "MyWiFi");
Config<String> wifiPassword("password", "wifi", "WiFi Password", "secretpass", true, true);
Config<bool> useDhcp("dhcp", "network", "Use DHCP", true);
Config<int> updateInterval("interval", "main", "Update Interval (seconds)", 30);


//--------------------------------------------------------------------
// predeclaration of functions
void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate);

#pragma region "Callback-Example"
void testCallback(int val); // Callback function for testCb here defined, later implemented...
Config<int> testCb("cbt", "main", "Test Callback", 0, true, false, testCallback); // since V2.0.0 use displayName for web interface
#pragma endregion
//--------------------------------------------------------------------

#pragma region "Structure-Example"
// General configuration (Structure example)
struct General_Settings {
    Config<bool> enableController;
    Config<int> maxOutput;
    Config<int> minOutput;
    Config<float> MQTTPublischPeriod;
    Config<String> Version;

    General_Settings() :
        enableController("enCtrl", "GS", "Enable Controller", true),
        maxOutput("MaxO", "GS", "Maximum Output", 1100),
        minOutput("MinO", "GS", "Minimum Output", 500),
        MQTTPublischPeriod("MQTTP", "GS", "MQTT Publish Period", 5.0),
        Version("Version", "GS", "Firmware Version", VERSION)
    {
        // Register settings
        cfg.addSetting(&enableController);
        cfg.addSetting(&maxOutput);
        cfg.addSetting(&minOutput);
        cfg.addSetting(&MQTTPublischPeriod);
        cfg.addSetting(&Version);
    }
};

General_Settings generalSettings; // Create an instance of General_Settings-Struct

// Example of a structure for WiFi with settings
struct WiFi_Settings {
    Config<String> Ssid;
    Config<String> Password;
    Config<bool> Dhcp;

    WiFi_Settings() :
        Ssid("ssid", "struct", "WiFi SSID", "MyWiFiStruct"),
        Password("password", "struct", "WiFi Password", "secretpassStruct", true, true),
        Dhcp("dhcp", "struct", "Use DHCP", false)
    {
        cfg.addSetting(&Ssid);
        cfg.addSetting(&Password);
        cfg.addSetting(&Dhcp);
    }
};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct
#pragma endregion

//--------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);

    // Register settings
    cfg.addSetting(&wifiSsid);
    cfg.addSetting(&wifiPassword);
    cfg.addSetting(&useDhcp);
    cfg.addSetting(&updateInterval);
    cfg.addSetting(&testCb);

    //-----------------------------------------------------------------
    // this is only to show, that you get an exception if the key is to long
    // but you cant catch it in the constructor!
    try
    {
        const char *key = wifiSsid.getKey();
    }
    catch (const KeyTooLongException &e)
    {
        Serial.printf("[ERROR] Config Error: %s\n", e.what());
    }
    catch (const KeyTruncatedWarning &e)
    {
        Serial.printf("[MAIN-Catch] Config Error: %s\n", e.what());
    }

    // test a to long, but truncatable key
    try
    {
        // Config<String> toLongKey("abcdefghijklmnop", "1234567890", "test to long, but truncatable key", true, false);
        Config<String> toLongKey("abcdefghijklmnop", "1234567890", "Test Key", "test to long, but truncatable key", true, false);
        const char *key = toLongKey.getKey();
        // Serial.printf("[WARNING] this Key was truncated: %s\n", key);
    }
    catch (const KeyTruncatedWarning &e)
    {
        Serial.printf("[MAIN-Catch-WARNING] Config Error: %s\n", e.what());
    }
    catch (const KeyTooLongException &e)
    {
        Serial.printf("[ERROR]  Config Error: %s\n", e.what());
    }
    //-----------------------------------------------------------------

    //-----------------------------------------------------------------
    // Set logger callback to log in your own way
    // void cbMyConfigLogger(const char *msg)
    // {
    //     Serial.println(msg);
    // }
    // ConfigManagerClass::setLogger(cbMyConfigLogger);

    // or as an lambda function...
    ConfigManagerClass::setLogger([](const char *msg)
                                  {
            Serial.print("[CFG] ");
            Serial.println(msg); });

    //-----------------------------------------------------------------

    cfg.loadAll(); // Load all settings from the preferences
    Serial.println("Loaded configuration:");

    generalSettings.Version.set(VERSION);// update version on device
    cfg.saveAll(); // Save all settings to the preferences

    SetupCheckForAPModeButton();

    delay(300);
    Serial.println("Configuration printout:");
    Serial.println(cfg.toJSON(false));

    // Test setting changes
    useDhcp.set(false);
    updateInterval.set(15);
    cfg.saveAll();
    delay(300);

    if (wifiSsid.get().length() == 0)
    {
        Serial.printf("⚠️ SETUP: SSID is empty! [%s]\n", wifiSsid.get().c_str());
        cfg.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        Serial.printf("🖥️  AP Mode! \n");
        return; // Skip webserver setup in AP mode
    }

    if (useDhcp.get())
    {
        Serial.println("DHCP enabled");
        cfg.startWebServer(wifiSsid.get(), wifiPassword.get());

    }
    else
    {
        Serial.println("DHCP disabled");
        cfg.startWebServer("192.168.2.126", "255.255.255.0", "192.168.0.250" , wifiSsid.get(), wifiPassword.get());

    }
    delay(1500);
    if (WiFi.status() == WL_CONNECTED) {
        cfg.setupOTA("Ota-esp32-device", "ota1234");
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

    cfg.handleClient();

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