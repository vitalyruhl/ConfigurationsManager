#include <Arduino.h>
#include "ConfigManager.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#define VERSION "V2.7.0" // 2025.11.02
#define APP_NAME "CM-Minimal-Demo"


//--------------------------------------------------------------------
// Forward declarations of functions
bool SetupStartWebServer();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

// Global objects and variables
extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

//--------------------------------------------------------------------------------------------------------------
// nessesary Settings, you dont need to make them, but they are needed for the ConfigManager to work properly

// You can put your wifi settings here in defaultValues, if project is not public. Or you can put it in your secret folder, like me.
#include "secret/wifiSecret.h" // Include your WiFi credentials here

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
                      staticIp(ConfigOptions<String>{.key = "WiFiStaticIP", .name = "Static IP", .category = "WiFi", .defaultValue = "192.168.0.10", .sortOrder = 4, .showIf = [this](){ return !useDhcp.get(); }}),
                      gateway(ConfigOptions<String>{.key = "WiFiGateway", .name = "Gateway", .category = "WiFi", .defaultValue = "192.168.0.1", .sortOrder = 5, .showIf = [this](){ return !useDhcp.get(); }}),
                      subnet(ConfigOptions<String>{.key = "WiFiSubnet", .name = "Subnet Mask", .category = "WiFi", .defaultValue = "255.255.255.0", .sortOrder = 6, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsPrimary(ConfigOptions<String>{.key = "WiFiDNS1", .name = "Primary DNS", .category = "WiFi", .defaultValue = "192.168.0.1", .sortOrder = 7, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsSecondary(ConfigOptions<String>{.key = "WiFiDNS2", .name = "Secondary DNS", .category = "WiFi", .defaultValue = "8.8.8.8", .sortOrder = 8, .showIf = [this](){ return !useDhcp.get(); }}),
                      wifiRebootTimeoutMin(ConfigOptions<int>{
                           .key = "WiFiRb",
                           .name = "Reboot if WiFi lost (min)",
                           .category = "WiFi",
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
    // ConfigManager.enableBuiltinSystemProvider(); // enable the builtin system provider (uptime, freeHeap, rssi etc.)
    // ConfigManager.setSettingsPassword(SETTINGS_PASSWORD); // Set the settings password from wifiSecret.h
    //----------------------------------------------------------------------------------------------------------------------------------
    ConfigManager.setWifiAPMacPriority("60:B5:8D:4C:E1:D5");   // Prefer this AP, fallback to others, i need it for testing
    //----------------------------------------------------------------------------------------------------------------------------------
    //register your Settings here
    wifiSettings.init();      // WiFi connection settings
    //----------------------------------------------------------------------------------------------------------------------------------

    ConfigManager.loadAll(); // Load all settings from preferences, is necessary before using the settings!

    //----------------------------------------------------------------------------------------------------------------------------------
    // Configure Smart WiFi Roaming with default values (can be customized in setup if needed)
    ConfigManager.enableSmartRoaming(true);            // Re-enabled now that WiFi stack is fixed
    ConfigManager.setRoamingImprovement(10);           // Require 10 dBm improvement

    //----------------------------------------------------------------------------------------------------------------------------------
    // set wifi settings if not set yet from my secret folder (its a behavior to easy testing, but you can set it also in AP-Mode)
    if (wifiSettings.wifiSsid.get().isEmpty())
    {
        Serial.println("-------------------------------------------------------------");
        Serial.println("SETUP: *** SSID is empty, setting My values *** ");
        Serial.println("-------------------------------------------------------------");
        wifiSettings.wifiSsid.set(MY_WIFI_SSID);
        wifiSettings.wifiPassword.set(MY_WIFI_PASSWORD);
        ConfigManager.saveAll();
        delay(1000); // Small delay
    }

    // perform the wifi connection
    SetupStartWebServer();

   // Enhanced WebSocket configuration
    ConfigManager.enableWebSocketPush(); // Enable WebSocket push for real-time updates
    ConfigManager.setWebSocketInterval(1000); // Faster updates - every 1 second
    ConfigManager.setPushOnConnect(true);     // Immediate data on client connect

    // Show correct IP address depending on WiFi mode
    if (WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_AP_STA) {
        Serial.printf("🖥️ Webserver running at: %s (AP Mode)\n", WiFi.softAPIP().toString().c_str());
    } else if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("🖥️ Webserver running at: %s (Station Mode)\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("🖥️ Webserver running (IP not available)");
    }
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

bool SetupStartWebServer()
{
    Serial.println("[MAIN] Starting Webserver...!");

    if (WiFi.getMode() == WIFI_AP)
    {
        return false; // Skip in AP mode
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

    // Ensure ArduinoOTA is initialized once WiFi is connected and OTA is allowed
    // This runs on every (re)connection to guarantee espota responds
    if (!ConfigManager.getOTAManager().isInitialized())
    {
        ConfigManager.setupOTA(APP_NAME, "ota"); // Use default password for simplicity
    }

    // Show correct IP address when connected
    Serial.printf("\n\n[MAIN] Webserver running at: %s (Connected)\n", WiFi.localIP().toString().c_str());
    Serial.printf("[MAIN] WLAN-Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[MAIN] WLAN-Strength is: %s\n", WiFi.RSSI() > -70 ? "good" : (WiFi.RSSI() > -80 ? "ok" : "weak"));

    String bssid = WiFi.BSSIDstr();
    Serial.printf("[MAIN] BSSID: %s (Channel: %d)\n", bssid.c_str(), WiFi.channel());
    Serial.printf("[MAIN] Local MAC: %s\n\n", WiFi.macAddress().c_str());
}


//folowing functions are optional, only if you want to do something special on these events

void onWiFiDisconnected()
{
    Serial.println("[MAIN] WiFi disconnected!");
}

void onWiFiAPMode()
{
    Serial.println("[MAIN] WiFi in AP mode");
}
