#include <Arduino.h>
#include "ConfigManager.h"
#include <WebServer.h>
// #include <WiFiClientSecure.h>
#include <WebServer.h>

#define BUTTON_PIN_AP_MODE 13

#define sl Serial // Dummy-Logger

// ⚠️ Atention ⚠️ settings will not be stored if its lenht >14!, because the max length for prefs is 15 chars...
//  The settings wil be stored in the format: <category>_<key>

// Usage:
// Config<VarType> VarName (const char *name, const char *category, T defaultValue, bool showInWeb = true, bool isPassword = false, void (*cb)(T) = nullptr)

ConfigManagerClass configManager;
Config<String> wifiSsid("ssid", "wifi", "MyWiFi");
Config<String> wifiPassword("password", "wifi", "secretpass", true, true);
Config<bool> useDhcp("dhcp", "network", true);
Config<int> updateInterval("interval", "main", 30);

WebServer server(80);

void SetupCheckForAPModeButton();
void blinkBuidInLED(int BlinkCount, int blinkRate);

void setup()
{
    sl.begin(115200);
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);

    // register settings
    configManager.addSetting(&wifiSsid);
    configManager.addSetting(&wifiPassword);
    configManager.addSetting(&useDhcp);
    configManager.addSetting(&updateInterval);

    configManager.loadAll();
    sl.println("Config loaded:");

    SetupCheckForAPModeButton();

    delay(300);
    sl.println("Printout the Config:");
    sl.println(configManager.toJSON(false));

    // Test change of settings
    useDhcp.set(false);
    updateInterval.set(15);
    configManager.saveAll();
    delay(300);

    if (wifiSsid.get().length() == 0)
    {
        sl.printf("⚠️ SETUP: ssid is empty! [%s]\n", wifiSsid.get().c_str());
        configManager.startAccessPoint();
    }

    if (WiFi.getMode() == WIFI_AP)
    {
        sl.printf("🖥️  AP-Mode! ");
        return; // we dont want tu set ap-mode of on starting webserver...
    }

    if (useDhcp.get())
    {
        sl.println("DHCP aktiviert");
        configManager.startWebServer(wifiSsid.get(), wifiPassword.get());
    }
    else
    {
        sl.println("DHCP deaktiviert");
        configManager.startWebServer("192.168.2.122", "255.255.255.0", wifiSsid.get(), wifiPassword.get());
    }
    sl.printf("🖥️ Webserver läuft unter:%s", WiFi.localIP().toString().c_str());
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
            configManager.reconnectWifi();
            delay(1000);
            return;
        }
        blinkBuidInLED(1, 100);
    }

    configManager.handleClient();

    static unsigned long lastPrint = 0;
    int interval = max(updateInterval.get(), 1); // prevent a zero interval
    if (millis() - lastPrint > interval * 1000)
    {
        lastPrint = millis();
        sl.printf("Loop --> get DHCP using bit : %s\n", useDhcp.get() ? "jop" : "nop");
        // sl.printf("Loop --> Wifistatus: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
    }

    delay(500);
}

void SetupCheckForAPModeButton()
{
    String APName = "ESP32_Config";//this is the default name for the AP-Mode
    String pwd = "config1234"; //this is the default password for the AP-Mode

    sl.println("Check for AP-Mode-Button...");

    if (digitalRead(BUTTON_PIN_AP_MODE) == LOW)
    {
        sl.printf("AP-Mode-Button pressed... -> Start AP-Mode with\n --> SSID: %s \nand \n --->  pwd: %s\n", APName.c_str(), pwd.c_str());

        //uncomment your prefered AP-Mode
        // configManager.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", "config1234");
        // configManager.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", pwd);
        // configManager.startAccessPoint("192.168.4.1", "255.255.255.0", APName, pwd);
        configManager.startAccessPoint("192.168.4.1", "255.255.255.0", APName, "");
        // configManager.startAccessPoint();
    }
}

void blinkBuidInLED(int BlinkCount, int blinkRate)
{
    // BlinkCount = 3; // number of blinks
    // blinkRate = 1000; // blink rate in ms

    for (int i = 0; i < BlinkCount; i++)
    {
        digitalWrite(LED_BUILTIN, HIGH); // turn the LED on
        delay(blinkRate);                // wait
        digitalWrite(LED_BUILTIN, LOW);  // turn the LED off
        delay(blinkRate);                // wait
    }
}
