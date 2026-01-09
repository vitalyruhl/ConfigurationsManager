#include <Arduino.h>
#include "ConfigManager.h"
#include <WiFi.h>

#define VERSION CONFIGMANAGER_VERSION
#define APP_NAME "CM-Minimal-Demo"


bool SetupStartWebServer();
void onWiFiConnected();
void onWiFiDisconnected();
void onWiFiAPMode();

extern ConfigManagerClass ConfigManager;  // Use extern to reference the instance from ConfigManager.cpp

// Minimal skeleton: do not hardcode WiFi credentials in code.
// Leave SSID empty to start in AP mode and configure via Web UI.
static const char SETTINGS_PASSWORD[] = "cm";

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

    ConfigManagerClass::setLogger([](const char *msg)
        {
            Serial.print("[ConfigManager] ");
            Serial.println(msg);
        });

    ConfigManager.setAppName(APP_NAME); // Set an application name, used for SSID in AP mode and as a prefix for the hostname
    ConfigManager.setVersion(VERSION); // Set the application version for web UI display
    ConfigManager.enableBuiltinSystemProvider();
    ConfigManager.setSettingsPassword(SETTINGS_PASSWORD);

    wifiSettings.init();
    ConfigManager.loadAll();

    SetupStartWebServer();
}

void loop()
{
    ConfigManager.updateLoopTiming();
    ConfigManager.getWiFiManager().update();
    ConfigManager.handleClient();
}

//----------------------------------------
// GUI SETUP
//----------------------------------------

bool SetupStartWebServer()
{
    const String ssid = wifiSettings.wifiSsid.get();
    const String password = wifiSettings.wifiPassword.get();

    if (ssid.isEmpty())
    {
        ConfigManager.startAccessPoint();
        Serial.printf("[INFO] AP Mode: http://%s\n", WiFi.softAPIP().toString().c_str());
        return false;
    }

    if (wifiSettings.useDhcp.get())
    {
        ConfigManager.startWebServer(ssid, password);
    }
    else
    {
        IPAddress staticIP, gateway, subnet, dns1, dns2;
        if (!staticIP.fromString(wifiSettings.staticIp.get()))
        {
            Serial.println("[ERROR] Invalid static IP");
            ConfigManager.startWebServer(ssid, password);
            return true;
        }
        if (!gateway.fromString(wifiSettings.gateway.get()))
        {
            Serial.println("[ERROR] Invalid gateway");
            ConfigManager.startWebServer(ssid, password);
            return true;
        }
        if (!subnet.fromString(wifiSettings.subnet.get()))
        {
            Serial.println("[ERROR] Invalid subnet");
            ConfigManager.startWebServer(ssid, password);
            return true;
        }

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

        ConfigManager.startWebServer(staticIP, gateway, subnet, ssid, password, dns1, dns2);
    }

    ConfigManager.getWiFiManager().setAutoRebootTimeout((unsigned long)wifiSettings.wifiRebootTimeoutMin.get());
    return true;
}

void onWiFiConnected()
{
    Serial.printf("[INFO] Station Mode: http://%s\n", WiFi.localIP().toString().c_str());
}


//folowing functions are optional, only if you want to do something special on these events

void onWiFiDisconnected()
{
    Serial.println("[ERROR] WiFi disconnected");
}

void onWiFiAPMode()
{
    Serial.printf("[INFO] AP Mode: http://%s\n", WiFi.softAPIP().toString().c_str());
}
