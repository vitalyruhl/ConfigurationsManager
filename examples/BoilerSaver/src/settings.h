#ifndef SETTINGS_H
#define SETTINGS_H

#pragma once

#include <Arduino.h>

#include "ConfigManager.h"

#define APP_VERSION "3.3.0"
#define VERSION_DATE "05.11.2025"
#define APP_NAME "Boiler-Saver"

extern ConfigManagerClass ConfigManager;

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
    Config<int>   boilerTimeMin;// max time boiler is allowed to heat
    Config<bool>  stopTimerOnTarget; // stop timer when off-threshold reached
    Config<bool>  onlyOncePerPeriod; // publish '1' only once per period

    BoilerSettings():
        enabled(ConfigOptions<bool>{
            .key = "BoI_En",
            .name = "Enable Boiler Control",
            .category = "Boiler",
            .defaultValue = true
        }),
        onThreshold(ConfigOptions<float>{
            .key = "BoI_OnT",
            .name = "Alarm Under Temperature",
            .category = "Boiler",
            .defaultValue = 55.0f,
            .showInWeb = true,
            .isPassword = false
        }),
        offThreshold(ConfigOptions<float>{
            .key = "BoI_OffT",
            .name = "You can shower now temperature",
            .category = "Boiler",
            .defaultValue = 80.0f,
            .showInWeb = true,
            .isPassword = false
        }),
        boilerTimeMin(ConfigOptions<int>{
            .key = "BoI_Time",
            .name = "Boiler Max Heating Time (min)",
            .category = "Boiler",
            .defaultValue = 90,
            .showInWeb = true
        }),
        stopTimerOnTarget(ConfigOptions<bool>{
            .key = "BoI_StopOnT",
            .name = "Stop timer when target reached",
            .category = "Boiler",
            .defaultValue = true,
            .showInWeb = true
        }),
        onlyOncePerPeriod(ConfigOptions<bool>{
            .key = "YSNOnce",
            .name = "Notify once per period",
            .category = "Boiler",
            .defaultValue = true,
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

struct TempSensorSettings {
    Config<int>   gpioPin;      // DS18B20 data pin
    Config<float> corrOffset;   // correction offset in °C
    Config<int>   readInterval; // seconds
    TempSensorSettings():
        gpioPin(ConfigOptions<int>{.key = "TsPin", .name = "GPIO Pin", .category = "Temp Sensor", .defaultValue = 18}),
        corrOffset(ConfigOptions<float>{.key = "TsOfs", .name = "Correction Offset", .category = "Temp Sensor", .defaultValue = 0.0f, .showInWeb = true}),
        readInterval(ConfigOptions<int>{.key = "TsInt", .name = "Read Interval (s)", .category = "Temp Sensor", .defaultValue = 30, .showInWeb = true})
    {}
};

extern I2CSettings i2cSettings;
extern DisplaySettings displaySettings;
extern TempSensorSettings tempSensorSettings;
extern BoilerSettings boilerSettings;

// Function to register all settings with ConfigManager
// This must be called after ConfigManager is properly initialized
void initializeAllSettings();

#endif // SETTINGS_H
