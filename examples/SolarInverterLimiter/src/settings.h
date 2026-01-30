#ifndef SETTINGS_H
#define SETTINGS_H

#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <SigmaLoger.h>

#include "ConfigManager.h"

// Make secrets available for defaults used in settings constructors.
// This header is guarded with pragma once.
#if __has_include("secret/wifiSecret.h")
#include "secret/wifiSecret.h"
#endif

// Provide safe fallback if OTA_PASSWORD is not defined by secrets.
#ifndef OTA_PASSWORD
#define OTA_PASSWORD "otasecret"
#endif

#define APP_NAME "Solarinverter - Limiter" // name of the application
#define VERSION "3.0.0"           // version of the software (major.minor.patch)
#define VERSION_DATE "11.01.2026" // date of the version

//--------------------------------------------------------------------------------------------------------------
// set the I2C address for the BME280 sensor for temperature and humidity
// (defaults now moved into I2CSettings as Config defaults)

//--------------------------------------------------------------------------------------------------------------
// set the I2C address for the display (SSD1306)
// Display address moved into settings (I2CSettings)
//--------------------------------------------------------------------------------------------------------------

// Watchdog timeout remains compile-time constant
#define WDT_TIMEOUT 60


extern ConfigManagerClass &cfg; // alias to global ConfigManager instance (defined by library)

//--------------------------------------------------------------------------------------------------------------
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

    WiFi_Settings() : wifiSsid(ConfigOptions<String>{.key = "WiFiSSID", .name = "WiFi SSID", .category = "WiFi", .defaultValue = "", .showInWeb = true, .sortOrder = 1}),
                      wifiPassword(ConfigOptions<String>{.key = "WiFiPassword", .name = "WiFi Password", .category = "WiFi", .defaultValue = "secretpass", .showInWeb = true, .isPassword = true, .sortOrder = 2}),
                      useDhcp(ConfigOptions<bool>{.key = "WiFiUseDHCP", .name = "Use DHCP", .category = "WiFi", .defaultValue = true, .showInWeb = true, .sortOrder = 3}),
                      staticIp(ConfigOptions<String>{.key = "WiFiStaticIP", .name = "Static IP", .category = "WiFi", .defaultValue = "192.168.2.131", .sortOrder = 4, .showIf = [this](){ return !useDhcp.get(); }}),
                      gateway(ConfigOptions<String>{.key = "WiFiGateway", .name = "Gateway", .category = "WiFi", .defaultValue = "192.168.2.250", .sortOrder = 5, .showIf = [this](){ return !useDhcp.get(); }}),
                      subnet(ConfigOptions<String>{.key = "WiFiSubnet", .name = "Subnet Mask", .category = "WiFi", .defaultValue = "255.255.255.0", .sortOrder = 6, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsPrimary(ConfigOptions<String>{.key = "WiFiDNS1", .name = "Primary DNS", .category = "WiFi", .defaultValue = "192.168.2.250", .sortOrder = 7, .showIf = [this](){ return !useDhcp.get(); }}),
                      dnsSecondary(ConfigOptions<String>{.key = "WiFiDNS2", .name = "Secondary DNS", .category = "WiFi", .defaultValue = "8.8.8.8", .sortOrder = 8, .showIf = [this](){ return !useDhcp.get(); }})
    {
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        cfg.addSetting(&wifiSsid);
        cfg.addSetting(&wifiPassword);
        cfg.addSetting(&useDhcp);
        cfg.addSetting(&staticIp);
        cfg.addSetting(&gateway);
        cfg.addSetting(&subnet);
        cfg.addSetting(&dnsPrimary);
        cfg.addSetting(&dnsSecondary);
    }

};


// mqtt-Setup
struct MQTT_Settings {
    Config<int>    mqtt_port;
    Config<String> mqtt_server;
    Config<String> mqtt_username;
    Config<String> mqtt_password;
    Config<String> mqtt_sensor_powerusage_topic;
    Config<String> mqtt_sensor_powerusage_json_keypath;
    Config<String> publishTopicBase;
    Config<float> mqttPublishPeriodSec;
    Config<float> mqttListenPeriodSec;
    Config<bool>  enableMQTT; // moved from LimiterSettings

    // dynamic topics (derived)
    String mqtt_publish_setvalue_topic;
    String mqtt_publish_getvalue_topic;
    String mqtt_publish_Temperature_topic;
    String mqtt_publish_Humidity_topic;
    String mqtt_publish_Dewpoint_topic;

    MQTT_Settings() :
        mqttPublishPeriodSec(ConfigOptions<float>{
            .key = "MQTTPubPer",
            .name = "Publish Period (s)",
            .category = "MQTT",
            .defaultValue = 5.0f,
            .categoryPretty = "MQTT"
        }),
        mqttListenPeriodSec(ConfigOptions<float>{
            .key = "MQTTSubPer",
            .name = "Listen Period (s)",
            .category = "MQTT",
            .defaultValue = 0.5f,
            .categoryPretty = "MQTT"
        }),
        mqtt_port(ConfigOptions<int>{
            .key = "MQTTPort",
            .name = "Port",
            .category = "MQTT",
            .defaultValue = 1883,
            .categoryPretty = "MQTT"
        }),
        mqtt_server(ConfigOptions<String>{
            .key = "MQTTHost",
            .name = "Server",
            .category = "MQTT",
            .defaultValue = String("192.168.2.3"),
            .categoryPretty = "MQTT"
        }),
        mqtt_username(ConfigOptions<String>{
            .key = "MQTTUser",
            .name = "Username",
            .category = "MQTT",
            .defaultValue = String(""),
            .categoryPretty = "MQTT"
        }),
        mqtt_password(ConfigOptions<String>{
            .key = "MQTTPass",
            .name = "Password",
            .category = "MQTT",
            .defaultValue = String(""),
            .isPassword = true,
            .categoryPretty = "MQTT"
        }),
        mqtt_sensor_powerusage_topic(ConfigOptions<String>{
            .key = "MQTTPwrTopic",
            .name = "Power Usage Topic",
            .category = "MQTT",
            .defaultValue = String("emon/emonpi/power1"),
            .categoryPretty = "MQTT"
        }),
        mqtt_sensor_powerusage_json_keypath(ConfigOptions<String>{
            .key = "MQTTPwrKey",
            .name = "Power Usage JSON Key",
            .category = "MQTT",
            .defaultValue = String("none"),
            .categoryPretty = "MQTT"
        }),
        publishTopicBase(ConfigOptions<String>{
            .key = "MQTTBaseTopic",
            .name = "Base Topic",
            .category = "MQTT",
            .defaultValue = String("SolarLimiter2"),
            .categoryPretty = "MQTT"
        }),
        enableMQTT(ConfigOptions<bool>{
            .key = "MQTTEnable",
            .name = "Enable MQTT",
            .category = "MQTT",
            .defaultValue = true,
            .categoryPretty = "MQTT"
        })
    {
        cfg.addSetting(&mqtt_port);
        cfg.addSetting(&mqtt_server);
        cfg.addSetting(&mqtt_username);
        cfg.addSetting(&mqtt_password);
        cfg.addSetting(&mqtt_sensor_powerusage_topic);
        cfg.addSetting(&mqtt_sensor_powerusage_json_keypath);
        cfg.addSetting(&publishTopicBase);
        cfg.addSetting(&mqttPublishPeriodSec);
        cfg.addSetting(&mqttListenPeriodSec);
        cfg.addSetting(&enableMQTT);

        // capturing lambda requires std::function path -> use setCallback after construction
        publishTopicBase.setCallback([this](String){ this->updateTopics(); });
        updateTopics();
    }

    void updateTopics() {
        String hostname = publishTopicBase.get();
        mqtt_publish_setvalue_topic      = hostname + "/SetValue";
        mqtt_publish_getvalue_topic      = hostname + "/GetValue";
        mqtt_publish_Temperature_topic   = hostname + "/Temperature";
        mqtt_publish_Humidity_topic      = hostname + "/Humidity";
        mqtt_publish_Dewpoint_topic      = hostname + "/Dewpoint";
    }
};


// General configuration (default Settings)
struct LimiterSettings {
    Config<bool> enableController;
    Config<int>  maxOutput;
    Config<int>  minOutput;
    Config<int>  inputCorrectionOffset;
    Config<int>  smoothingSize;
    Config<float> RS232PublishPeriod; // keep here for now
    LimiterSettings() :
        enableController(ConfigOptions<bool>{
            .key = "LimiterEnable",
            .name = "Limiter Enabled",
            .category = "Limiter",
            .defaultValue = true,
            .categoryPretty = "Limiter"
        }),
        maxOutput(ConfigOptions<int>{
            .key = "LimiterMaxW",
            .name = "Max Output (W)",
            .category = "Limiter",
            .defaultValue = 1100,
            .categoryPretty = "Limiter"
        }),
        minOutput(ConfigOptions<int>{
            .key = "LimiterMinW",
            .name = "Min Output (W)",
            .category = "Limiter",
            .defaultValue = 500,
            .categoryPretty = "Limiter"
        }),
        inputCorrectionOffset(ConfigOptions<int>{
            .key = "LimiterCorrW",
            .name = "Input Correction Offset (W)",
            .category = "Limiter",
            .defaultValue = 50,
            .categoryPretty = "Limiter"
        }),
        smoothingSize(ConfigOptions<int>{
            .key = "LimiterSmooth",
            .name = "Smoothing Level",
            .category = "Limiter",
            .defaultValue = 10,
            .categoryPretty = "Limiter"
        }),
        RS232PublishPeriod(ConfigOptions<float>{
            .key = "LimiterRS485P",
            .name = "RS485 Publish Period (s)",
            .category = "Limiter",
            .defaultValue = 2.0f,
            .categoryPretty = "Limiter"
        })
    {
        cfg.addSetting(&enableController);
        cfg.addSetting(&maxOutput);
        cfg.addSetting(&minOutput);
        cfg.addSetting(&inputCorrectionOffset);
        cfg.addSetting(&smoothingSize);
        cfg.addSetting(&RS232PublishPeriod);
    }
};

struct TempSettings // BME280 Settings
{
    Config<float> tempCorrection;
    Config<float> humidityCorrection;
    Config<int> seaLevelPressure;
    Config<int> readIntervalSec;
    Config<float> dewpointRiskWindow; // ΔT (°C) über Taupunkt, ab der Risiko-Alarm auslöst

    TempSettings() : tempCorrection(ConfigOptions<float>{.key = "TCO", .name = "Temperature Correction", .category = "Temp", .defaultValue = 0.1f}),
                     humidityCorrection(ConfigOptions<float>{.key = "HYO", .name = "Humidity Correction", .category = "Temp", .defaultValue = 0.1f}),
                     seaLevelPressure(ConfigOptions<int>{.key = "SLP", .name = "Sea Level Pressure", .category = "Temp", .defaultValue = 1013}),
                     readIntervalSec(ConfigOptions<int>{.key = "ReadTemp", .name = "Read Temp/Humidity every (s)", .category = "Temp", .defaultValue = 30}),
                     dewpointRiskWindow(ConfigOptions<float>{.key = "DPWin", .name = "Dewpoint Risk Window (°C)", .category = "Temp", .defaultValue = 1.5f})
    {
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        cfg.addSetting(&tempCorrection);
        cfg.addSetting(&humidityCorrection);
        cfg.addSetting(&seaLevelPressure);
        cfg.addSetting(&readIntervalSec);
        cfg.addSetting(&dewpointRiskWindow);
    }
};

struct I2CSettings {
    Config<int> sdaPin;
    Config<int> sclPin;
    Config<int> busFreq;
    Config<int> bmeFreq;
    Config<int> displayAddr;
    I2CSettings():
        sdaPin(ConfigOptions<int>{
            .key = "I2CSDA",
            .name = "SDA Pin",
            .category = "I2C",
            .defaultValue = 21,
            .categoryPretty = "I2C"
        }),
        sclPin(ConfigOptions<int>{
            .key = "I2CSCL",
            .name = "SCL Pin",
            .category = "I2C",
            .defaultValue = 22,
            .categoryPretty = "I2C"
        }),
        busFreq(ConfigOptions<int>{
            .key = "I2CFreq",
            .name = "Bus Frequency (Hz)",
            .category = "I2C",
            .defaultValue = 400000,
            .categoryPretty = "I2C"
        }),
        bmeFreq(ConfigOptions<int>{
            .key = "I2CBmeHz",
            .name = "BME280 Frequency (Hz)",
            .category = "I2C",
            .defaultValue = 400000,
            .categoryPretty = "I2C"
        }),
        displayAddr(ConfigOptions<int>{
            .key = "I2CDisp",
            .name = "Display Address",
            .category = "I2C",
            .defaultValue = 0x3C,
            .categoryPretty = "I2C"
        })
    {
        cfg.addSetting(&sdaPin);
        cfg.addSetting(&sclPin);
        cfg.addSetting(&busFreq);
        cfg.addSetting(&bmeFreq);
        cfg.addSetting(&displayAddr);
    }
};

struct FanSettings {
    Config<bool>  enabled;
    Config<float> onThreshold;
    Config<float> offThreshold;
    Config<int>   relayPin;
    Config<bool>  activeLow;
    FanSettings():
        enabled(ConfigOptions<bool>{
            .key = "FanEnable",
            .name = "Enable Fan Control",
            .category = "Fan",
            .defaultValue = true,
            .categoryPretty = "Fan"
        }),
        onThreshold(ConfigOptions<float>{
            .key = "FanOn",
            .name = "Fan ON above (°C)",
            .category = "Fan",
            .defaultValue = 30.0f,
            .showIf = [this](){return this->enabled.get();},
            .categoryPretty = "Fan"
        }),
        offThreshold(ConfigOptions<float>{
            .key = "FanOff",
            .name = "Fan OFF below (°C)",
            .category = "Fan",
            .defaultValue = 27.0f,
            .showIf = [this](){return this->enabled.get();},
            .categoryPretty = "Fan"
        }),
        relayPin(ConfigOptions<int>{
            .key = "FanPin",
            .name = "Relay GPIO",
            .category = "Fan",
            .defaultValue = 23,
            .categoryPretty = "Fan"
        }),
        activeLow(ConfigOptions<bool>{
            .key = "FanLow",
            .name = "Active LOW",
            .category = "Fan",
            .defaultValue = true,
            .categoryPretty = "Fan"
        })
    {
        cfg.addSetting(&enabled);
        cfg.addSetting(&onThreshold);
        cfg.addSetting(&offThreshold);
        cfg.addSetting(&relayPin);
        cfg.addSetting(&activeLow);
    }
};

struct HeaterSettings {
    Config<bool>  enabled;
    Config<float> onTemp;
    Config<float> offTemp;
    Config<int>   relayPin;
    Config<bool>  activeLow;
    HeaterSettings():
        enabled(ConfigOptions<bool>{
            .key = "HeatEnable",
            .name = "Enable Heater Control",
            .category = "Heater",
            .defaultValue = false,
            .categoryPretty = "Heater"
        }),
        onTemp(ConfigOptions<float>{
            .key = "HeatOn",
            .name = "Heater ON below (°C)",
            .category = "Heater",
            .defaultValue = 0.0f,
            .showIf = [this](){return this->enabled.get();},
            .categoryPretty = "Heater"
        }),
        offTemp(ConfigOptions<float>{
            .key = "HeatOff",
            .name = "Heater OFF above (°C)",
            .category = "Heater",
            .defaultValue = 0.5f,
            .showIf = [this](){return this->enabled.get();},
            .categoryPretty = "Heater"
        }),
        relayPin(ConfigOptions<int>{
            .key = "HeatPin",
            .name = "Relay GPIO",
            .category = "Heater",
            .defaultValue = 33,
            .showIf = [this](){return this->enabled.get();},
            .categoryPretty = "Heater"
        }),
        activeLow(ConfigOptions<bool>{
            .key = "HeatLow",
            .name = "Active LOW",
            .category = "Heater",
            .defaultValue = true,
            .showIf = [this](){return this->enabled.get();},
            .categoryPretty = "Heater"
        })
    {
        cfg.addSetting(&enabled);
        cfg.addSetting(&onTemp);
        cfg.addSetting(&offTemp);
        cfg.addSetting(&relayPin);
        cfg.addSetting(&activeLow);
    }
};

struct DisplaySettings {
    Config<bool> turnDisplayOff;
    Config<int>  onTimeSec;
    DisplaySettings():
        turnDisplayOff(ConfigOptions<bool>{
            .key = "DispSleep",
            .name = "Turn Display Off",
            .category = "Display",
            .defaultValue = true,
            .categoryPretty = "Display"
        }),
        onTimeSec(ConfigOptions<int>{
            .key = "DispOnS",
            .name = "On Time (s)",
            .category = "Display",
            .defaultValue = 60,
            .categoryPretty = "Display"
        })
    {
        cfg.addSetting(&turnDisplayOff);
        cfg.addSetting(&onTimeSec);
    }
};

struct SystemSettings
{
    Config<bool> allowOTA;
    Config<String> otaPassword;
    Config<int> wifiRebootTimeoutMin;
    Config<String> version;
    SystemSettings() : allowOTA(ConfigOptions<bool>{.key = "OTAEn", .name = "Allow OTA Updates", .category = "System", .defaultValue = true}),
                       otaPassword(ConfigOptions<String>{.key = "OTAPass", .name = "OTA Password", .category = "System", .defaultValue = String(OTA_PASSWORD), .showInWeb = true, .isPassword = true}),
                       wifiRebootTimeoutMin(ConfigOptions<int>{
                           .key = "WiFiRb",
                           .name = "Reboot if WiFi lost (min)",
                           .category = "WiFi",
                           .defaultValue = 5,
                           .showInWeb = true}),
                       version(ConfigOptions<String>{.key = "P_Version", .name = "Program Version", .category = "System", .defaultValue = String(VERSION)})
    {
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        cfg.addSetting(&allowOTA);
        cfg.addSetting(&otaPassword);
        cfg.addSetting(&wifiRebootTimeoutMin);
        cfg.addSetting(&version);
    }
};

struct ButtonSettings
{
    Config<int> apModePin;
    Config<int> resetDefaultsPin;
    ButtonSettings() : apModePin(ConfigOptions<int>{.key = "BtnAP", .name = "AP Mode Button GPIO", .category = "Buttons", .defaultValue = 13}),
                       resetDefaultsPin(ConfigOptions<int>{.key = "BtnRst", .name = "Reset Defaults Button GPIO", .category = "Buttons", .defaultValue = 15})
    {
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        cfg.addSetting(&apModePin);
        cfg.addSetting(&resetDefaultsPin);
    }
};

struct NTPSettings
{
    Config<int> frequencySec; // Sync frequency (seconds)
    Config<String> server1;   // Primary NTP server
    Config<String> server2;   // Secondary NTP server
    Config<String> tz;        // POSIX/TZ string for local time
    NTPSettings() : frequencySec(ConfigOptions<int>{.key = "NTPFrq", .name = "NTP Sync Interval (s)", .category = "NTP", .defaultValue = 3600, .showInWeb = true}),
                    server1(ConfigOptions<String>{.key = "NTP1", .name = "NTP Server 1", .category = "NTP", .defaultValue = String("192.168.2.250"), .showInWeb = true}),
                    server2(ConfigOptions<String>{.key = "NTP2", .name = "NTP Server 2", .category = "NTP", .defaultValue = String("pool.ntp.org"), .showInWeb = true}),
                    tz(ConfigOptions<String>{.key = "NTPTZ", .name = "Time Zone (POSIX)", .category = "NTP", .defaultValue = String("CET-1CEST,M3.5.0/02,M10.5.0/03"), .showInWeb = true})
    {
        // Constructor - do not register here due to static initialization order
    }

    void init() {
        // Register settings with ConfigManager after ConfigManager is ready
        cfg.addSetting(&frequencySec);
        cfg.addSetting(&server1);
        cfg.addSetting(&server2);
        cfg.addSetting(&tz);
    }
};

// RS485 configuration (default Settings)
struct RS485_Settings
{
    bool useExtraSerial = true; // set to true to use Serial2 for RS485 communication
    int baudRate = 4800;
    int rxPin = 18;          // only for Serial2, not used for Serial
    int txPin = 19;          // only for Serial2, not used for Serial
    int dePin = 4;           // DE pin for RS485 communication (direction control)
    bool enableRS485 = true; // set to false to disable RS485 communication
    // todo: add settings for Inverter eg, headder, checksum, etc.
};

extern MQTT_Settings mqttSettings;
extern LimiterSettings limiterSettings;
extern TempSettings tempSettings;
extern I2CSettings i2cSettings;
extern FanSettings fanSettings;
extern HeaterSettings heaterSettings;
extern DisplaySettings displaySettings;
extern SystemSettings systemSettings;
extern ButtonSettings buttonSettings;
extern RS485_Settings rs485settings;
extern SigmaLogLevel logLevel;
extern WiFi_Settings wifiSettings;
extern NTPSettings ntpSettings;
// extern LDR_Settings ldrSettings;

#endif // SETTINGS_H
