#include <Arduino.h>
#include "ConfigManager.h"
#include <WebServer.h>
// #include <WiFiClientSecure.h>
#include <WebServer.h>

#define VERSION "1.2.0" // Add additional Logging

#define BUTTON_PIN_AP_MODE 13
#define sl Serial // logger

// ⚠️ Warning ⚠️ settings will not be stored if length >14! Max length for prefs is 15 chars...
// Settings are stored in format: <category>_<key>

// Usage:
// Config<VarType> VarName (const char *name, const char category, T defaultValue, bool showInWeb = true, bool isPassword = false, void (cb)(T) = nullptr)

ConfigManagerClass cfg; // Create an instance of ConfigManager before using it in structures etc.
WebServer server(80);
int cbTestValue = 0;

// here used global variables without struct eg Config is an helper class for the settings stored in the ConfigManager.h
// todo: test for settings used in a void scope etc...
Config<String> wifiSsid("ssid", "wifi", "MyWiFi");
Config<String> wifiPassword("password", "wifi", "secretpass", true, true);
Config<bool> useDhcp("dhcp", "network", true);

Config<int> updateInterval("interval", "main", 30);

//--------------------------------------------------------------------
// predeclaration of functions
void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate);

#pragma region "Callback-Example"
void testCallback(int val);                                      // Callback function for testCb here defined, later implemented...
Config<int> testCb("cbt", "main", 0, true, false, testCallback); // define int variable with callback
#pragma endregion
//--------------------------------------------------------------------

#pragma region "Structure-Example"
// General configuration (Structure example)
struct General_Settings
{
    Config<bool> enableController;
    Config<int> maxOutput;
    Config<int> minOutput;
    Config<float> MQTTPublischPeriod;
    Config<String> Version;

    General_Settings() : enableController("enCtrl", "GS", true),
                         maxOutput("MaxO", "GS", 1100),
                         minOutput("MinO", "GS", 500),
                         MQTTPublischPeriod("MQTTP", "GS", 5.0),
                         Version("Version", "GS", VERSION)
    {
        // Register settings with ConfigManager
        cfg.addSetting(&enableController);
        cfg.addSetting(&maxOutput);
        cfg.addSetting(&minOutput);
        cfg.addSetting(&MQTTPublischPeriod);
        cfg.addSetting(&Version);
    }
};

General_Settings generalSettings; // Create an instance of General_Settings-Struct

// Example of a structure for WiFi with settings
struct WiFi_Settings
{
    Config<String> Ssid;
    Config<String> Password;
    Config<bool> Dhcp;
    WiFi_Settings() :

                      Ssid("ssid", "struct", "MyWiFiStruct"),
                      Password("password", "struct", "secretpassStruct", true, true),
                      Dhcp("dhcp", "struct", false)

    {
        cfg.addSetting(&Ssid);
        cfg.addSetting(&Password);
        cfg.addSetting(&Dhcp);
    }
};

WiFi_Settings wifiSettings; // Create an instance of WiFi_Settings-Struct
#pragma endregion

#pragma region "Logger-Callback"

void cbMyConfigLogger(const char* msg) {
    sl.println(msg);
}

#pragma endregion
//--------------------------------------------------------------------

void setup()
{
    sl.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);

    // Register settings
    cfg.addSetting(&wifiSsid);
    cfg.addSetting(&wifiPassword);
    cfg.addSetting(&useDhcp);
    cfg.addSetting(&updateInterval);
    cfg.addSetting(&testCb);

    // ConfigManagerClass::setLogger(cbMyConfigLogger); // Set logger callback to log in your own way

    ConfigManagerClass::setLogger([](const char* msg) {
            Serial.print("[CFG] ");
            Serial.println(msg);
    });

    cfg.loadAll();
    sl.println("Loaded configuration:");

    SetupCheckForAPModeButton();

    delay(300);
    sl.println("Configuration printout:");
    sl.println(cfg.toJSON(false));

    // Test setting changes
    useDhcp.set(false);
    updateInterval.set(15);
    cfg.saveAll();
    delay(300);

    if (wifiSsid.get().length() == 0)
    {
        sl.printf("⚠️ SETUP: SSID is empty! [%s]\n", wifiSsid.get().c_str());
        cfg.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        sl.printf("🖥️  AP Mode! ");
        return; // Skip webserver setup in AP mode
    }

    if (useDhcp.get())
    {
        sl.println("DHCP enabled");
        cfg.startWebServer(wifiSsid.get(), wifiPassword.get());
    }
    else
    {
        sl.println("DHCP disabled");
        cfg.startWebServer("192.168.2.122", "255.255.255.0", wifiSsid.get(), wifiPassword.get());
    }
    sl.printf("🖥️ Webserver running at: %s", WiFi.localIP().toString().c_str());
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
            sl.println("❌ WiFi not connected!");
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
        // sl.printf("Loop --> DHCP status: %s\n", useDhcp.get() ? "yes" : "no");
        // sl.printf("Loop --> WiFi status: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
        cbTestValue++;
        testCb.set(cbTestValue);
        if (cbTestValue > 10)
        {
            cbTestValue = 0;
        }
    }

    delay(500);
}

void testCallback(int val)
{
    sl.printf("Callback called with value: %d\n", val);
}

void SetupCheckForAPModeButton()
{
    String APName = "ESP32_Config";
    String pwd = "config1234"; // Default AP password

    sl.println("Checking AP mode button...");

    if (digitalRead(BUTTON_PIN_AP_MODE) == LOW)
    {
        sl.printf("AP mode button pressed -> Starting AP with\n --> SSID: %s \n --> Password: %s\n", APName.c_str(), pwd.c_str());

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