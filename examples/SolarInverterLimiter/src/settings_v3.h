#pragma once

#include <Arduino.h>

#include "ConfigManager.h"

// Example metadata (shown in Web UI)
#define APP_NAME "SolarInverterLimiter"
#define VERSION "3.3.0"
#define VERSION_DATE "2026-01-30"

// Limiter configuration
struct LimiterSettings
{
    Config<bool> *enableController = nullptr;
    Config<int> *maxOutput = nullptr;
    Config<int> *minOutput = nullptr;
    Config<int> *inputCorrectionOffset = nullptr;
    Config<int> *smoothingSize = nullptr;
    Config<float> *RS232PublishPeriod = nullptr;

    void create()
    {
        enableController = &ConfigManager.addSettingBool("LimiterEnable")
                                 .name("Limiter Enabled")
                                 .category("Limiter")
                                 .defaultValue(true)
                                 .build();
        maxOutput = &ConfigManager.addSettingInt("LimiterMaxW")
                           .name("Max Output (W)")
                           .category("Limiter")
                           .defaultValue(1100)
                           .build();
        minOutput = &ConfigManager.addSettingInt("LimiterMinW")
                           .name("Min Output (W)")
                           .category("Limiter")
                           .defaultValue(500)
                           .build();
        inputCorrectionOffset = &ConfigManager.addSettingInt("LimiterCorrW")
                                     .name("Input Correction Offset (W)")
                                     .category("Limiter")
                                     .defaultValue(50)
                                     .build();
        smoothingSize = &ConfigManager.addSettingInt("LimiterSmooth")
                              .name("Smoothing Level")
                              .category("Limiter")
                              .defaultValue(10)
                              .build();
        RS232PublishPeriod = &ConfigManager.addSettingFloat("LimiterRS485P")
                                   .name("RS485 Publish Period (s)")
                                   .category("Limiter")
                                   .defaultValue(2.0f)
                                   .build();
    }
};

// BME280 settings
struct TempSettings
{
    Config<float> *tempCorrection = nullptr;
    Config<float> *humidityCorrection = nullptr;
    Config<int> *seaLevelPressure = nullptr;
    Config<int> *readIntervalSec = nullptr;
    Config<float> *dewpointRiskWindow = nullptr;

    void create()
    {
        tempCorrection = &ConfigManager.addSettingFloat("TCO")
                            .name("Temperature Correction")
                            .category("Temp")
                            .defaultValue(0.1f)
                            .build();
        humidityCorrection = &ConfigManager.addSettingFloat("HYO")
                                 .name("Humidity Correction")
                                 .category("Temp")
                                 .defaultValue(0.1f)
                                 .build();
        seaLevelPressure = &ConfigManager.addSettingInt("SLP")
                                .name("Sea Level Pressure (hPa)")
                                .category("Temp")
                                .defaultValue(1013)
                                .build();
        readIntervalSec = &ConfigManager.addSettingInt("ReadTemp")
                               .name("Read Temp/Humidity every (s)")
                               .category("Temp")
                               .defaultValue(30)
                               .build();
        dewpointRiskWindow = &ConfigManager.addSettingFloat("DPWin")
                                   .name("Dewpoint Risk Window (C)")
                                   .category("Temp")
                                   .defaultValue(1.5f)
                                   .build();
    }
};

struct I2CSettings
{
    Config<int> *sdaPin = nullptr;
    Config<int> *sclPin = nullptr;
    Config<int> *busFreq = nullptr;
    Config<int> *displayAddr = nullptr;

    void create()
    {
        sdaPin = &ConfigManager.addSettingInt("I2CSDA")
                      .name("SDA Pin")
                      .category("I2C")
                      .defaultValue(21)
                      .build();
        sclPin = &ConfigManager.addSettingInt("I2CSCL")
                      .name("SCL Pin")
                      .category("I2C")
                      .defaultValue(22)
                      .build();
        busFreq = &ConfigManager.addSettingInt("I2CFreq")
                      .name("Bus Frequency (Hz)")
                      .category("I2C")
                      .defaultValue(400000)
                      .build();
        displayAddr = &ConfigManager.addSettingInt("I2CDisp")
                          .name("Display Address")
                          .category("I2C")
                          .defaultValue(0x3C)
                          .build();
    }
};

struct FanSettings
{
    Config<bool> *enabled = nullptr;
    Config<float> *onThreshold = nullptr;
    Config<float> *offThreshold = nullptr;

    void create()
    {
        enabled = &ConfigManager.addSettingBool("FanEnable")
                       .name("Enable Fan Control")
                       .category("Fan")
                       .defaultValue(true)
                       .build();
        onThreshold = &ConfigManager.addSettingFloat("FanOn")
                          .name("Fan ON above (C)")
                          .category("Fan")
                          .defaultValue(30.0f)
                          .build();
        offThreshold = &ConfigManager.addSettingFloat("FanOff")
                           .name("Fan OFF below (C)")
                           .category("Fan")
                           .defaultValue(27.0f)
                           .build();
    }
};

struct HeaterSettings
{
    Config<bool> *enabled = nullptr;
    Config<float> *onTemp = nullptr;
    Config<float> *offTemp = nullptr;

    void create()
    {
        enabled = &ConfigManager.addSettingBool("HeatEnable")
                       .name("Enable Heater Control")
                       .category("Heater")
                       .defaultValue(false)
                       .build();
        onTemp = &ConfigManager.addSettingFloat("HeatOn")
                      .name("Heater ON below (C)")
                      .category("Heater")
                      .defaultValue(0.0f)
                      .build();
        offTemp = &ConfigManager.addSettingFloat("HeatOff")
                       .name("Heater OFF above (C)")
                       .category("Heater")
                       .defaultValue(0.5f)
                       .build();
    }
};

struct DisplaySettings
{
    Config<bool> *turnDisplayOff = nullptr;
    Config<int> *onTimeSec = nullptr;

    void create()
    {
        turnDisplayOff = &ConfigManager.addSettingBool("DispSleep")
                               .name("Turn Display Off")
                               .category("Display")
                               .defaultValue(true)
                               .build();
        onTimeSec = &ConfigManager.addSettingInt("DispOnS")
                         .name("On Time (s)")
                         .category("Display")
                         .defaultValue(60)
                         .build();
    }
};

struct RS485_Settings
{
    // Serial2 is used for RS485 communication
    static constexpr bool useExtraSerial = true;

    Config<bool> *enableRS485 = nullptr;
    Config<int> *baudRate = nullptr;
    Config<int> *rxPin = nullptr;
    Config<int> *txPin = nullptr;
    Config<int> *dePin = nullptr;

    void create()
    {
        enableRS485 = &ConfigManager.addSettingBool("RS485En")
                              .name("Enable RS485")
                              .category("RS485")
                              .defaultValue(true)
                              .build();
        baudRate = &ConfigManager.addSettingInt("RS485Baud")
                       .name("Baud Rate")
                       .category("RS485")
                       .defaultValue(4800)
                       .build();
        rxPin = &ConfigManager.addSettingInt("RS485Rx")
                     .name("RX Pin")
                     .category("RS485")
                     .defaultValue(18)
                     .build();
        txPin = &ConfigManager.addSettingInt("RS485Tx")
                     .name("TX Pin")
                     .category("RS485")
                     .defaultValue(19)
                     .build();
        dePin = &ConfigManager.addSettingInt("RS485DE")
                     .name("DE Pin")
                     .category("RS485")
                     .defaultValue(4)
                     .build();
    }
};

extern LimiterSettings limiterSettings;
extern TempSettings tempSettings;
extern I2CSettings i2cSettings;
extern FanSettings fanSettings;
extern HeaterSettings heaterSettings;
extern DisplaySettings displaySettings;
extern RS485_Settings rs485settings;

void initializeAllSettings();

