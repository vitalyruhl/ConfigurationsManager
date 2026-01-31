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
    Config<bool> enableController;
    Config<int> maxOutput;
    Config<int> minOutput;
    Config<int> inputCorrectionOffset;
    Config<int> smoothingSize;
    Config<float> RS232PublishPeriod;

    LimiterSettings()
        : enableController(ConfigOptions<bool>{.key = "LimiterEnable", .name = "Limiter Enabled", .category = "Limiter", .defaultValue = true})
        , maxOutput(ConfigOptions<int>{.key = "LimiterMaxW", .name = "Max Output (W)", .category = "Limiter", .defaultValue = 1100})
        , minOutput(ConfigOptions<int>{.key = "LimiterMinW", .name = "Min Output (W)", .category = "Limiter", .defaultValue = 500})
        , inputCorrectionOffset(ConfigOptions<int>{.key = "LimiterCorrW", .name = "Input Correction Offset (W)", .category = "Limiter", .defaultValue = 50})
        , smoothingSize(ConfigOptions<int>{.key = "LimiterSmooth", .name = "Smoothing Level", .category = "Limiter", .defaultValue = 10})
        , RS232PublishPeriod(ConfigOptions<float>{.key = "LimiterRS485P", .name = "RS485 Publish Period (s)", .category = "Limiter", .defaultValue = 2.0f})
    {
    }
};

// BME280 settings
struct TempSettings
{
    Config<float> tempCorrection;
    Config<float> humidityCorrection;
    Config<int> seaLevelPressure;
    Config<int> readIntervalSec;
    Config<float> dewpointRiskWindow;

    TempSettings()
        : tempCorrection(ConfigOptions<float>{.key = "TCO", .name = "Temperature Correction", .category = "Temp", .defaultValue = 0.1f})
        , humidityCorrection(ConfigOptions<float>{.key = "HYO", .name = "Humidity Correction", .category = "Temp", .defaultValue = 0.1f})
        , seaLevelPressure(ConfigOptions<int>{.key = "SLP", .name = "Sea Level Pressure (hPa)", .category = "Temp", .defaultValue = 1013})
        , readIntervalSec(ConfigOptions<int>{.key = "ReadTemp", .name = "Read Temp/Humidity every (s)", .category = "Temp", .defaultValue = 30})
        , dewpointRiskWindow(ConfigOptions<float>{.key = "DPWin", .name = "Dewpoint Risk Window (C)", .category = "Temp", .defaultValue = 1.5f})
    {
    }
};

struct I2CSettings
{
    Config<int> sdaPin;
    Config<int> sclPin;
    Config<int> busFreq;
    Config<int> displayAddr;

    I2CSettings()
        : sdaPin(ConfigOptions<int>{.key = "I2CSDA", .name = "SDA Pin", .category = "I2C", .defaultValue = 21})
        , sclPin(ConfigOptions<int>{.key = "I2CSCL", .name = "SCL Pin", .category = "I2C", .defaultValue = 22})
        , busFreq(ConfigOptions<int>{.key = "I2CFreq", .name = "Bus Frequency (Hz)", .category = "I2C", .defaultValue = 400000})
        , displayAddr(ConfigOptions<int>{.key = "I2CDisp", .name = "Display Address", .category = "I2C", .defaultValue = 0x3C})
    {
    }
};

struct FanSettings
{
    Config<bool> enabled;
    Config<float> onThreshold;
    Config<float> offThreshold;

    FanSettings()
        : enabled(ConfigOptions<bool>{.key = "FanEnable", .name = "Enable Fan Control", .category = "Fan", .defaultValue = true})
        , onThreshold(ConfigOptions<float>{.key = "FanOn", .name = "Fan ON above (C)", .category = "Fan", .defaultValue = 30.0f})
        , offThreshold(ConfigOptions<float>{.key = "FanOff", .name = "Fan OFF below (C)", .category = "Fan", .defaultValue = 27.0f})
    {
    }
};

struct HeaterSettings
{
    Config<bool> enabled;
    Config<float> onTemp;
    Config<float> offTemp;

    HeaterSettings()
        : enabled(ConfigOptions<bool>{.key = "HeatEnable", .name = "Enable Heater Control", .category = "Heater", .defaultValue = false})
        , onTemp(ConfigOptions<float>{.key = "HeatOn", .name = "Heater ON below (C)", .category = "Heater", .defaultValue = 0.0f})
        , offTemp(ConfigOptions<float>{.key = "HeatOff", .name = "Heater OFF above (C)", .category = "Heater", .defaultValue = 0.5f})
    {
    }
};

struct DisplaySettings
{
    Config<bool> turnDisplayOff;
    Config<int> onTimeSec;

    DisplaySettings()
        : turnDisplayOff(ConfigOptions<bool>{.key = "DispSleep", .name = "Turn Display Off", .category = "Display", .defaultValue = true})
        , onTimeSec(ConfigOptions<int>{.key = "DispOnS", .name = "On Time (s)", .category = "Display", .defaultValue = 60})
    {
    }
};

struct RS485_Settings
{
    // Serial2 is used for RS485 communication
    static constexpr bool useExtraSerial = true;

    Config<bool> enableRS485;
    Config<int> baudRate;
    Config<int> rxPin;
    Config<int> txPin;
    Config<int> dePin;

    RS485_Settings()
        : enableRS485(ConfigOptions<bool>{.key = "RS485En", .name = "Enable RS485", .category = "RS485", .defaultValue = true})
        , baudRate(ConfigOptions<int>{.key = "RS485Baud", .name = "Baud Rate", .category = "RS485", .defaultValue = 4800})
        , rxPin(ConfigOptions<int>{.key = "RS485Rx", .name = "RX Pin", .category = "RS485", .defaultValue = 18})
        , txPin(ConfigOptions<int>{.key = "RS485Tx", .name = "TX Pin", .category = "RS485", .defaultValue = 19})
        , dePin(ConfigOptions<int>{.key = "RS485DE", .name = "DE Pin", .category = "RS485", .defaultValue = 4})
    {
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

