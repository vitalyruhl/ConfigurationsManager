#ifndef SETTINGS_H
#define SETTINGS_H

#pragma once

#include <Arduino.h>

#include "ConfigManager.h"

#define APP_VERSION "4.0.0"
#define VERSION_DATE "05.11.2025"
#ifndef APP_NAME
#define APP_NAME "Boiler-Saver"
#endif

extern ConfigManagerClass ConfigManager;

struct I2CSettings {
    Config<int> *sdaPin = nullptr;
    Config<int> *sclPin = nullptr;
    Config<int> *busFreq = nullptr;
    Config<int> *displayAddr = nullptr;

    void create()
    {
        sdaPin = &ConfigManager.addSettingInt("I2CSDA")
                      .name("I2C SDA Pin")
                      .category("I2C")
                      .defaultValue(21)
                      .build();
        sclPin = &ConfigManager.addSettingInt("I2CSCL")
                      .name("I2C SCL Pin")
                      .category("I2C")
                      .defaultValue(22)
                      .build();
        busFreq = &ConfigManager.addSettingInt("I2CFreq")
                      .name("I2C Bus Freq")
                      .category("I2C")
                      .defaultValue(400000)
                      .build();
        displayAddr = &ConfigManager.addSettingInt("DispAddr")
                          .name("Display I2C Address")
                          .category("I2C")
                          .defaultValue(0x3C)
                          .build();
    }
};

struct BoilerSettings {
    Config<bool> *enabled = nullptr;       // enable/disable boiler control
    Config<float> *onThreshold = nullptr;  // temperature to turn boiler on
    Config<float> *offThreshold = nullptr; // temperature to turn boiler off
    Config<int> *boilerTimeMin = nullptr;  // max time boiler is allowed to heat
    Config<bool> *stopTimerOnTarget = nullptr; // stop timer when off-threshold reached
    Config<bool> *onlyOncePerPeriod = nullptr; // publish '1' only once per period

    void create()
    {
        enabled = &ConfigManager.addSettingBool("BoI_En")
                       .name("Enable Boiler Control")
                       .category("Boiler")
                       .defaultValue(true)
                       .build();
        onThreshold = &ConfigManager.addSettingFloat("BoI_OnT")
                          .name("Alarm Under Temperature")
                          .category("Boiler")
                          .defaultValue(55.0f)
                          .build();
        offThreshold = &ConfigManager.addSettingFloat("BoI_OffT")
                           .name("You can shower now temperature")
                           .category("Boiler")
                           .defaultValue(80.0f)
                           .build();
        boilerTimeMin = &ConfigManager.addSettingInt("BoI_Time")
                             .name("Boiler Max Heating Time (min)")
                             .category("Boiler")
                             .defaultValue(90)
                             .build();
        stopTimerOnTarget = &ConfigManager.addSettingBool("BoI_StopOnT")
                                 .name("Stop timer when target reached")
                                 .category("Boiler")
                                 .defaultValue(true)
                                 .build();
        onlyOncePerPeriod = &ConfigManager.addSettingBool("YSNOnce")
                                 .name("Notify once per period")
                                 .category("Boiler")
                                 .defaultValue(true)
                                 .build();
    }
};

struct DisplaySettings {
    Config<bool> *turnDisplayOff = nullptr;
    Config<int> *onTimeSec = nullptr;

    void create()
    {
        turnDisplayOff = &ConfigManager.addSettingBool()
                               .name("Turn Display Off")
                               .category("Display")
                               .defaultValue(true)
                               .build();
        onTimeSec = &ConfigManager.addSettingInt()
                         .name("Display On-Time (s)")
                         .category("Display")
                         .defaultValue(60)
                         .build();
    }
};

struct TempSensorSettings {
    Config<int> *gpioPin = nullptr;      // DS18B20 data pin
    Config<float> *corrOffset = nullptr;   // correction offset in °C
    Config<int> *readInterval = nullptr; // seconds

    void create()
    {
        gpioPin = &ConfigManager.addSettingInt("TsPin")
                       .name("GPIO Pin")
                       .category("Temp Sensor")
                       .defaultValue(18)
                       .build();
        corrOffset = &ConfigManager.addSettingFloat("TsOfs")
                          .name("Correction Offset")
                          .category("Temp Sensor")
                          .defaultValue(0.0f)
                          .build();
        readInterval = &ConfigManager.addSettingInt("TsInt")
                             .name("Read Interval (s)")
                             .category("Temp Sensor")
                             .defaultValue(30)
                             .build();
    }
};

extern I2CSettings i2cSettings;
extern DisplaySettings displaySettings;
extern TempSensorSettings tempSensorSettings;
extern BoilerSettings boilerSettings;

// Function to register all settings with ConfigManager
// This must be called after ConfigManager is properly initialized
void initializeAllSettings();

#endif // SETTINGS_H
