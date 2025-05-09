#include <Arduino.h>
#include "ConfigManager.h"
#include <WebServer.h>
// #include <WiFiClientSecure.h>
#include <WebServer.h>

#define BUTTON_PIN_AP_MODE 13

#define sl Serial // Dummy-Logger

ConfigManagerClass configManager;
Config<String> wifiSsid("ssid", "wifi", "MyWiFi");
Config<String> wifiPassword("password", "wifi", "secretpass", true, true);
Config<bool> useDhcp("dhcp", "network", true);

// this will be not stored, because max length is 14 chars...
// Config<int> updateInterval("interval", "network", 30);
Config<int> updateInterval("interval", "main", 30);

WebServer server(80);
// WiFiServerSecure httpsServer(443);
// extern WiFiServerSecure httpsServer;

void SetupCheckForAPModeButton();

void setup()
{
    sl.begin(115200);
    delay(500);

    // register settings
    configManager.addSetting(&wifiSsid);
    configManager.addSetting(&wifiPassword);
    configManager.addSetting(&useDhcp);
    configManager.addSetting(&updateInterval);

    configManager.loadAll();

    SetupCheckForAPModeButton();

    sl.println("Config:");
    sl.println(configManager.toJSON(false));

    // Test change of settings
    useDhcp.set(true);
    wifiPassword.set("VivilWLANPasswort");
    updateInterval.set(15);
    configManager.saveAll();

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
}

void loop()
{
    configManager.handleClient();
    static unsigned long lastPrint = 0;
    int interval = max(updateInterval.get(), 1); // prevent a zero interval
    if (millis() - lastPrint > interval * 1000)
    {
        lastPrint = millis();
        sl.printf("Loop --> DHCP: %s\n", useDhcp.get() ? "jop" : "nop");
        // sl.printf("Loop --> Wifistatus: %s\n", WiFi.status() == WL_CONNECTED ? "connected" : "not connected");
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        sl.println("❌ WiFi not connected!");
        configManager.reconnectWifi();
    }
}

void SetupCheckForAPModeButton()
{
    if (wifiSsid.get().length() == 0)
    {
        sl.printf("⚠️ SETUP: config.wifi_config.ssid is empty! [%s]\n", wifiSsid.get().c_str());
        // configManager.startAccessPoint();
        return;
    }

    pinMode(BUTTON_PIN_AP_MODE, INPUT_PULLUP);
    if (digitalRead(BUTTON_PIN_AP_MODE) == LOW)
    {
        sl.println("AP-Mode-Button pressed... -> Start AP-Mode...");
        configManager.startAccessPoint();
    }
}
