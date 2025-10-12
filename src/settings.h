#ifndef SETTINGS_H
#define SETTINGS_H

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <SigmaLoger.h>

#include "ConfigManager.h"

#define VERSION "0.0.1"           // version of the software (major.minor.patch)
#define VERSION_DATE "10.10.2025" // date of the version
#define APP_NAME "Boiler-Saver" // name of the application, used for SSID in AP mode and as a prefix for the hostname

// Watchdog timeout remains compile-time constant
#define WDT_TIMEOUT 60

extern ConfigManagerClass ConfigManager;// Use the global ConfigManager instance
//--------------------------------------------------------------------------------------------------------------

struct WiFi_Settings // wifiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;

    WiFi_Settings() : 
        wifiSsid(ConfigOptions<String>{.key = "WiFiSSID", .name = "WiFi SSID", .category = "WiFi Settings", .defaultValue = "MyWiFi", .sortOrder = 1}),
        wifiPassword(ConfigOptions<String>{.key = "WiFiPassword", .name = "WiFi Password", .category = "WiFi Settings", .defaultValue = "secretpass", .isPassword = true, .sortOrder = 2}),
        useDhcp(ConfigOptions<bool>{.key = "WiFiUseDHCP", .name = "Use DHCP", .category = "WiFi Settings", .defaultValue = false, .sortOrder = 3}),
        staticIp(ConfigOptions<String>{.key = "WiFiStaticIP", .name = "Static IP", .category = "WiFi Settings", .defaultValue = "192.168.2.126", .sortOrder = 4, .showIf = [this]() { return !useDhcp.get(); }}),
        gateway(ConfigOptions<String>{.key = "WiFiGateway", .name = "Gateway", .category = "WiFi Settings", .defaultValue = "192.168.2.250", .sortOrder = 5, .showIf = [this]() { return !useDhcp.get(); }}),
        subnet(ConfigOptions<String>{.key = "WiFiSubnet", .name = "Subnet Mask", .category = "WiFi Settings", .defaultValue = "255.255.255.0", .sortOrder = 6, .showIf = [this]() { return !useDhcp.get(); }})
    {
        // Settings registration moved to initializeAllSettings()
    }
    
    void registerSettings()
    {
        // Register settings with ConfigManager
        ConfigManager.addSetting(&wifiSsid);
        ConfigManager.addSetting(&wifiPassword);
        ConfigManager.addSetting(&useDhcp);
        ConfigManager.addSetting(&staticIp);
        ConfigManager.addSetting(&gateway);
        ConfigManager.addSetting(&subnet);
    }
};

// mqtt-Setup
struct MQTT_Settings
{
    Config<int> mqtt_port;
    Config<String> mqtt_server;
    Config<String> mqtt_username;
    Config<String> mqtt_password;
    Config<String> Publish_Topic;
    Config<String> mqtt_Settings_ShowerTime_topic;
    Config<String> mqtt_Settings_SetState_topic;
    Config<bool> mqtt_Settings_SetState; // payload to turn boiler on
    Config<int>mqtt_Settings_ShowerTime;
    Config<float> MQTTPublischPeriod;
    Config<float> MQTTListenPeriod;


    // For dynamic topics based on Publish_Topic
    String mqtt_publish_AktualState;
    String mqtt_publish_AktualBoilerTemperature;
    String mqtt_publish_AktualTimeRemaining_topic;

    // Now show extra pretty category name since V2.2.0: e.g., ("keyname", "category", "web displayName", "web Pretty category", defaultValue)
    MQTT_Settings() : mqtt_port(ConfigOptions<int>{.key = "MQTTTPort", .name = "Port", .category = "MQTT", .defaultValue = 1883}),
                      mqtt_server(ConfigOptions<String>{.key = "MQTTServer", .name = "Server-IP", .category = "MQTT", .defaultValue = String("192.168.2.3")}),
                      mqtt_username(ConfigOptions<String>{.key = "MQTTUser", .name = "User", .category = "MQTT", .defaultValue = String("housebattery")}),
                      mqtt_password(ConfigOptions<String>{.key = "MQTTPass", .name = "Password", .category = "MQTT", .defaultValue = String("mqttsecret"), .showInWeb = true, .isPassword = true}),
                      Publish_Topic(ConfigOptions<String>{.key = "MQTTTPT", .name = "Publish-Topic", .category = "MQTT", .defaultValue = String("BoilerSaver")}),
                      mqtt_Settings_ShowerTime_topic(ConfigOptions<String>{.key = "MQTTSTT", .name = "Shower-Time Topic", .category = "MQTT", .defaultValue = String("BoilerSaver/Settings/ShowerTime"), .showInWeb = true, .isPassword = false}),
                      mqtt_Settings_SetState_topic(ConfigOptions<String>{.key = "MQTTSTS", .name = "Set-Shower-Time Topic", .category = "MQTT", .defaultValue = String("BoilerSaver/Settings/SetShowerTime"), .showInWeb = true, .isPassword = false}),
                      MQTTPublischPeriod(ConfigOptions<float>{.key = "MQTTPP", .name = "Publish-Period (s)", .category = "MQTT", .defaultValue = 2.0f}),
                      MQTTListenPeriod(ConfigOptions<float>{.key = "MQTTLP", .name = "Listen-Period (s)", .category = "MQTT", .defaultValue = 0.5f}),
                      mqtt_Settings_SetState(ConfigOptions<bool>{.key = "SetSt", .name = "Set-State", .category = "MQTT", .defaultValue = false, .showInWeb = false, .isPassword = false}),
                      mqtt_Settings_ShowerTime(ConfigOptions<int>{.key = "ShwTm", .name = "Shower Time (min)", .category = "MQTT", .defaultValue = 90, .showInWeb = false, .isPassword = false})

    {
        // Settings registration moved to registerSettings()
        
        // Callback to update topics when Publish_Topic changes
        Publish_Topic.setCallback([this](String newValue){ this->updateTopics(); });

        updateTopics(); // Make sure topics are initialized
    }
    
    void registerSettings()
    {
        ConfigManager.addSetting(&mqtt_port);
        ConfigManager.addSetting(&mqtt_server);
        ConfigManager.addSetting(&mqtt_username);
        ConfigManager.addSetting(&mqtt_password);
        ConfigManager.addSetting(&Publish_Topic);
        ConfigManager.addSetting(&mqtt_Settings_ShowerTime_topic);
        ConfigManager.addSetting(&mqtt_Settings_SetState_topic);
        ConfigManager.addSetting(&MQTTPublischPeriod);
        ConfigManager.addSetting(&MQTTListenPeriod);
        ConfigManager.addSetting(&mqtt_Settings_SetState);
        ConfigManager.addSetting(&mqtt_Settings_ShowerTime);
    }

    void updateTopics()
    {
        String hostname = Publish_Topic.get();
        mqtt_publish_AktualState = hostname + "/AktualState"; //show State of Boiler Heating/Save-Mode
        mqtt_publish_AktualBoilerTemperature = hostname + "/TemperatureBoiler"; //show Temperature of Boiler
        mqtt_publish_AktualTimeRemaining_topic = hostname + "/TimeRemaining"; //show Time Remaining if Boiler is Heating
    }
};


struct I2CSettings {
    Config<int> sdaPin;
    Config<int> sclPin;
    Config<int> busFreq;
    Config<int> bmeFreq;
    Config<int> displayAddr;
    I2CSettings():
        sdaPin(ConfigOptions<int>{.key = "I2CSDA", .name = "I2C SDA Pin", .category = "I2C", .defaultValue = 21}),
        sclPin(ConfigOptions<int>{.key = "I2CSCL", .name = "I2C SCL Pin", .category = "I2C", .defaultValue = 22}),
        busFreq(ConfigOptions<int>{.key = "I2CFreq", .name = "I2C Bus Freq", .category = "I2C", .defaultValue = 400000}),
        bmeFreq(ConfigOptions<int>{.key = "BMEFreq", .name = "BME280 Bus Freq", .category = "I2C", .defaultValue = 400000}),
        displayAddr(ConfigOptions<int>{.key = "DispAddr", .name = "Display I2C Address", .category = "I2C", .defaultValue = 0x3C})
    {
        // Settings registration moved to initializeAllSettings()
    }
};

struct BoilerSettings {
    Config<bool>  enabled;// enable/disable boiler control
    Config<float> onThreshold;// temperature to turn boiler on
    Config<float> offThreshold;// temperature to turn boiler off
    Config<int>   relayPin;// GPIO for boiler relay
    Config<bool>  activeLow;// relay active low/high
    Config<int>   boilerTimeMin;// max time boiler is allowed to heat

    BoilerSettings():
        enabled(ConfigOptions<bool>{
            .key = "BoI_En",
            .name = "Enable Boiler Control",
            .category = "Boiler",
            .defaultValue = true
        }),
        onThreshold(ConfigOptions<float>{
            .key = "BoI_OnT",
            .name = "Boiler On Threshold",
            .category = "Boiler",
            .defaultValue = 55.0f,
            .showInWeb = true,
            .isPassword = false
        }),
        offThreshold(ConfigOptions<float>{
            .key = "BoI_OffT",
            .name = "Boiler Off Threshold",
            .category = "Boiler",
            .defaultValue = 80.0f,
            .showInWeb = true,
            .isPassword = false
        }),
        relayPin(ConfigOptions<int>{
            .key = "BoI_Pin",
            .name = "Boiler Relay GPIO",
            .category = "Boiler",
            .defaultValue = 23
        }),
        activeLow(ConfigOptions<bool>{
            .key = "BoI_Low",
            .name = "Boiler Relay LOW-Active",
            .category = "Boiler",
            .defaultValue = true
        }),
        boilerTimeMin(ConfigOptions<int>{
            .key = "BoI_Time",
            .name = "Boiler Max Heating Time (min)",
            .category = "Boiler",
            .defaultValue = 90,
            .showInWeb = true
        })
    {
        // Settings registration moved to initializeAllSettings()
    }
};

struct DisplaySettings {
    Config<bool> turnDisplayOff;
    Config<int>  onTimeSec;
    DisplaySettings():
        turnDisplayOff(ConfigOptions<bool>{.name = "Turn Display Off", .category = "Display", .defaultValue = true}),
        onTimeSec(ConfigOptions<int>{.name = "Display On-Time (s)", .category = "Display", .defaultValue = 60})
    {
        // Settings registration moved to initializeAllSettings()
    }
};

struct SystemSettings {
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<int> wifiRebootTimeoutMin;
    Config<String> version;
    SystemSettings():
        allowOTA(ConfigOptions<bool>{.key = "OTAEn", .name = "Allow OTA Updates", .category = "System", .defaultValue = true}),
        otaPassword(ConfigOptions<String>{.key = "OTAPass", .name = "OTA Password", .category = "System", .defaultValue = String("ota1234"), .showInWeb = true, .isPassword = true}),
        wifiRebootTimeoutMin(ConfigOptions<int>{
            .key = "WiFiRb",
            .name = "Reboot if WiFi lost (min)",
            .category = "System",
            .defaultValue = 15,
            .showInWeb = true
        }),
        version(ConfigOptions<String>{.key = "P_Version", .name = "Program Version", .category = "System", .defaultValue = String(VERSION)})
    {
        // Settings registration moved to initializeAllSettings()
    }
};

struct ButtonSettings {
    Config<int> apModePin;
    Config<int> resetDefaultsPin;
    ButtonSettings():
        apModePin(ConfigOptions<int>{.key = "BtnAP", .name = "AP Mode Button GPIO", .category = "Buttons", .defaultValue = 13}),
        resetDefaultsPin(ConfigOptions<int>{.key = "BtnRst", .name = "Reset Defaults Button GPIO", .category = "Buttons", .defaultValue = 15})
    {
        // Settings registration moved to initializeAllSettings()
    }
};

extern MQTT_Settings mqttSettings;
extern I2CSettings i2cSettings;
extern DisplaySettings displaySettings;
extern SystemSettings systemSettings;
extern ButtonSettings buttonSettings;
extern SigmaLogLevel logLevel;
extern WiFi_Settings wifiSettings;
extern BoilerSettings boilerSettings;

// Function to register all settings with ConfigManager
// This must be called after ConfigManager is properly initialized
void initializeAllSettings();

#endif // SETTINGS_H
