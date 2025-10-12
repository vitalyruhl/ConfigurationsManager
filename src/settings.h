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

extern ConfigManagerClass cfg;// store it globaly before using it in the settings
//--------------------------------------------------------------------------------------------------------------

struct WiFi_Settings // wifiSettings
{
    Config<String> wifiSsid;
    Config<String> wifiPassword;
    Config<bool> useDhcp;
    Config<String> staticIp;
    Config<String> gateway;
    Config<String> subnet;

    // Shared OptionGroup constants to avoid repetition
    static constexpr OptionGroup WIFI_GROUP{.category ="wifi", .prettyCat = "WiFi Settings"};

    WiFi_Settings() : // Use OptionGroup helpers with shared constexpr instances
                      wifiSsid(WIFI_GROUP.opt<String>("ssid", "MyWiFi", "WiFi SSID")),
                      wifiPassword(WIFI_GROUP.opt<String>("password", "secretpass", "WiFi Password", true, true)),
                      useDhcp(WIFI_GROUP.opt<bool>("dhcp", false, "Use DHCP")),
                      staticIp(WIFI_GROUP.opt<String>("sIP", "192.168.2.126", "Static IP", true, false, nullptr, showIfFalse(useDhcp))),
                      gateway(WIFI_GROUP.opt<String>("GW", "192.168.2.250", "Gateway", true, false, nullptr, showIfFalse(useDhcp))),
                      subnet(WIFI_GROUP.opt<String>("subnet", "255.255.255.0", "Subnet-Mask", true, false, nullptr, showIfFalse(useDhcp)))
    {
        // Register settings with ConfigManager
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
        cfg.addSetting(&staticIp);
        cfg.addSetting(&gateway);
        cfg.addSetting(&subnet);
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
    MQTT_Settings() : mqtt_port(ConfigOptions<int>{.keyName = "Port", .category = "MQTT", .defaultValue = 1883, .prettyName = "Port", }),
                      mqtt_server(ConfigOptions<String>{.keyName = "Server", .category = "MQTT", .defaultValue = String("192.168.2.3"), .prettyName = "Server-IP", }),
                      mqtt_username(ConfigOptions<String>{.keyName = "User", .category = "MQTT", .defaultValue = String("housebattery"), .prettyName = "User", }),
                      mqtt_password(ConfigOptions<String>{.keyName = "Pass", .category = "MQTT", .defaultValue = String("mqttsecret"), .prettyName = "Password", .showInWeb = true, .isPassword = true}),
                      Publish_Topic(ConfigOptions<String>{.keyName = "MQTTT", .category = "MQTT", .defaultValue = String("BoilerSaver"), .prettyName = "Publish-Topic", }),
                      mqtt_Settings_ShowerTime_topic(ConfigOptions<String>{.keyName = "ShowerT", .category = "MQTT", .defaultValue = String("BoilerSaver/Settings/ShowerTime"), .prettyName = "Shower-Time Topic", .showInWeb = true, .isPassword = false}),
                      mqtt_Settings_SetState_topic(ConfigOptions<String>{.keyName = "SetShower", .category = "MQTT", .defaultValue = String("BoilerSaver/Settings/SetShowerTime"), .prettyName = "Set-Shower-Time Topic", .showInWeb = true, .isPassword = false}),
                      MQTTPublischPeriod(ConfigOptions<float>{.keyName = "PubPrd", .category = "MQTT", .defaultValue = 2.0f, .prettyName = "Publish-Period (s)", }),
                      MQTTListenPeriod(ConfigOptions<float>{.keyName = "LisPrd", .category = "MQTT", .defaultValue = 0.5f, .prettyName = "Listen-Period (s)", }),
                      mqtt_Settings_SetState(ConfigOptions<bool>{.keyName = "SetSt", .category = "MQTT", .defaultValue = false, .prettyName = "Set-State", .showInWeb = false, .isPassword = false}),
                      mqtt_Settings_ShowerTime(ConfigOptions<int>{.keyName = "ShwTm", .category = "MQTT", .defaultValue = 90, .prettyName = "Shower Time (min)", .showInWeb = false, .isPassword = false})

    {
        cfg.addSetting(&mqtt_port);
        cfg.addSetting(&mqtt_server);
        cfg.addSetting(&mqtt_username);
        cfg.addSetting(&mqtt_password);
        cfg.addSetting(&Publish_Topic);
        cfg.addSetting(&mqtt_Settings_ShowerTime_topic);
        cfg.addSetting(&mqtt_Settings_SetState_topic);
        cfg.addSetting(&MQTTPublischPeriod);
        cfg.addSetting(&MQTTListenPeriod);
        cfg.addSetting(&mqtt_Settings_SetState);
        cfg.addSetting(&mqtt_Settings_ShowerTime);

        // Callback to update topics when Publish_Topic changes
        Publish_Topic.setCallback([this](String newValue){ this->updateTopics(); });

        updateTopics(); // Make sure topics are initialized
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
        sdaPin({"I2CSDA","I2C",21,"I2C SDA Pin","I2C"}),
        sclPin({"I2CSCL","I2C",22,"I2C SCL Pin","I2C"}),
        busFreq({"I2CFreq","I2C",400000,"I2C Bus Freq","I2C"}),
        bmeFreq({"BMEFreq","I2C",400000,"BME280 Bus Freq","I2C"}),
        displayAddr({"DispAddr","I2C",0x3C,"Display I2C Address","I2C"})
    {
        cfg.addSetting(&sdaPin);
        cfg.addSetting(&sclPin);
        cfg.addSetting(&busFreq);
        cfg.addSetting(&bmeFreq);
        cfg.addSetting(&displayAddr);
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
            .keyName = "En",
            .category = "Boiler",
            .defaultValue = true,
            .prettyName = "Enable Boiler Control",
            .prettyCat = "Boiler Control"
        }),
        onThreshold(ConfigOptions<float>{
            .keyName = "OnT",
            .category = "Boiler",
            .defaultValue = 55.0f,
            .prettyName = "Boiler On Threshold",
            .prettyCat = "Boiler Control",
            .showInWeb = true,
            .isPassword = false
        }),
        offThreshold(ConfigOptions<float>{
            .keyName = "OffT",
            .category = "Boiler",
            .defaultValue = 80.0f,
            .prettyName = "Boiler Off Threshold",
            .prettyCat = "Boiler Control",
            .showInWeb = true,
            .isPassword = false
        }),
        relayPin(ConfigOptions<int>{
            .keyName = "Pin",
            .category = "Boiler",
            .defaultValue = 23,
            .prettyName = "Boiler Relay GPIO",
            .prettyCat = "Boiler Control"
        }),
        activeLow(ConfigOptions<bool>{
            .keyName = "Low",
            .category = "Boiler",
            .defaultValue = true,
            .prettyName = "Boiler Relay LOW-Active",
            .prettyCat = "Boiler Control"
        }),
        boilerTimeMin(ConfigOptions<int>{
            .keyName = "Time",
            .category = "Boiler",
            .defaultValue = 90,
            .prettyName = "Boiler Max Heating Time (min)",
            .prettyCat = "Boiler Control",
            .showInWeb = true
        })
    {
        cfg.addSetting(&enabled);
        cfg.addSetting(&onThreshold);
        cfg.addSetting(&offThreshold);
        cfg.addSetting(&relayPin);
        cfg.addSetting(&activeLow);
        cfg.addSetting(&boilerTimeMin);
    }
};

struct DisplaySettings {
    Config<bool> turnDisplayOff;
    Config<int>  onTimeSec;
    DisplaySettings():
        turnDisplayOff({"Save","Display",true,"Turn Display Off","Display Settings"}),
        onTimeSec({"Time","Display",60,"Display On-Time (s)","Display Settings"})
    {
        cfg.addSetting(&turnDisplayOff);
        cfg.addSetting(&onTimeSec);
    }
};

struct SystemSettings {
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<int> wifiRebootTimeoutMin;
    Config<String> version;
    SystemSettings():
        allowOTA({"OTAEn","System",true,"Allow OTA Updates"}),
        otaPassword({"OTAPass","System",String("ota1234"),"OTA Password","System",true,true}),
        wifiRebootTimeoutMin(ConfigOptions<int>{
            .keyName = "WiFiRb",
            .category = "System",
            .defaultValue = 15,
            .prettyName = "Reboot if WiFi lost (min)",
            .prettyCat = "System",
            .showInWeb = true
        }),
        version({"Version","System",String(VERSION),"Program Version"})
    {
        cfg.addSetting(&allowOTA);
        cfg.addSetting(&otaPassword);
        cfg.addSetting(&wifiRebootTimeoutMin);
        cfg.addSetting(&version);
    }
};

struct ButtonSettings {
    Config<int> apModePin;
    Config<int> resetDefaultsPin;
    ButtonSettings():
        apModePin({"BtnAP","Buttons",13,"AP Mode Button GPIO","Buttons"}),
        resetDefaultsPin({"BtnRst","Buttons",15,"Reset Defaults Button GPIO","Buttons"})
    {
        cfg.addSetting(&apModePin);
        cfg.addSetting(&resetDefaultsPin);
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

#endif // SETTINGS_H
