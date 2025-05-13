#include <Arduino.h>
#include "ConfigManager.h"
#include <WebServer.h>
// #include <WiFiClientSecure.h>
#include <WebServer.h>

#define BUTTON_PIN_AP_MODE 13

#define sl Serial // logger

// ⚠️ Warning ⚠️ settings will not be stored if length >14! Max length for prefs is 15 chars...
// Settings are stored in format: <category>_<key>

// Usage:
// Config<VarType> VarName (const char *name, const char category, T defaultValue, bool showInWeb = true, bool isPassword = false, void (cb)(T) = nullptr)

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

// Register settings
configManager.addSetting(&wifiSsid);
configManager.addSetting(&wifiPassword);
configManager.addSetting(&useDhcp);
configManager.addSetting(&updateInterval);

configManager.loadAll();
sl.println("Loaded configuration:");

SetupCheckForAPModeButton();

delay(300);
sl.println("Configuration printout:");
sl.println(configManager.toJSON(false));

// Test setting changes
useDhcp.set(false);
updateInterval.set(15);
configManager.saveAll();
delay(300);

if (wifiSsid.get().length() == 0)
{
    sl.printf("⚠️ SETUP: SSID is empty! [%s]\n", wifiSsid.get().c_str());
    configManager.startAccessPoint();
}

if (WiFi.getMode() == WIFI_AP)
{
    sl.printf("🖥️  AP Mode! ");
    return; // Skip webserver setup in AP mode
}

if (useDhcp.get())
{
    sl.println("DHCP enabled");
    configManager.startWebServer(wifiSsid.get(), wifiPassword.get());
}
else
{
    sl.println("DHCP disabled");
    configManager.startWebServer("192.168.2.122", "255.255.255.0", wifiSsid.get(), wifiPassword.get());
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
        configManager.reconnectWifi();
        delay(1000);
        return;
    }
    blinkBuidInLED(1, 100);
}

configManager.handleClient();

static unsigned long lastPrint = 0;
int interval = max(updateInterval.get(), 1); // Prevent zero interval
if (millis() - lastPrint > interval * 1000)
{
    lastPrint = millis();
    sl.printf("Loop --> DHCP status: %s\n", useDhcp.get() ? "yes" : "no");
    // sl.printf("Loop --> WiFi status: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
}

delay(500);

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
    // configManager.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", "config1234");
    // configManager.startAccessPoint("192.168.4.1", "255.255.255.0", "ESP32_Config", pwd);
    // configManager.startAccessPoint("192.168.4.1", "255.255.255.0", APName, pwd);
    configManager.startAccessPoint("192.168.4.1", "255.255.255.0", APName, "");
    // configManager.startAccessPoint();
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