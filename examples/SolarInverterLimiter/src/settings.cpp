#include "settings_v3.h"

LimiterSettings limiterSettings;
TempSettings tempSettings;
I2CSettings i2cSettings;
FanSettings fanSettings;
HeaterSettings heaterSettings;
DisplaySettings displaySettings;
RS485_Settings rs485settings;

void initializeAllSettings()
{
    ConfigManager.addSetting(&limiterSettings.enableController);
    ConfigManager.addSetting(&limiterSettings.maxOutput);
    ConfigManager.addSetting(&limiterSettings.minOutput);
    ConfigManager.addSetting(&limiterSettings.inputCorrectionOffset);
    ConfigManager.addSetting(&limiterSettings.smoothingSize);
    ConfigManager.addSetting(&limiterSettings.RS232PublishPeriod);

    ConfigManager.addSetting(&tempSettings.readIntervalSec);
    ConfigManager.addSetting(&tempSettings.tempCorrection);
    ConfigManager.addSetting(&tempSettings.humidityCorrection);
    ConfigManager.addSetting(&tempSettings.seaLevelPressure);
    ConfigManager.addSetting(&tempSettings.dewpointRiskWindow);

    ConfigManager.addSetting(&i2cSettings.sdaPin);
    ConfigManager.addSetting(&i2cSettings.sclPin);
    ConfigManager.addSetting(&i2cSettings.busFreq);
    ConfigManager.addSetting(&i2cSettings.displayAddr);

    ConfigManager.addSetting(&fanSettings.enabled);
    ConfigManager.addSetting(&fanSettings.onThreshold);
    ConfigManager.addSetting(&fanSettings.offThreshold);

    ConfigManager.addSetting(&heaterSettings.enabled);
    ConfigManager.addSetting(&heaterSettings.onTemp);
    ConfigManager.addSetting(&heaterSettings.offTemp);

    ConfigManager.addSetting(&displaySettings.turnDisplayOff);
    ConfigManager.addSetting(&displaySettings.onTimeSec);

    ConfigManager.addSetting(&rs485settings.enableRS485);
    ConfigManager.addSetting(&rs485settings.baudRate);
    ConfigManager.addSetting(&rs485settings.rxPin);
    ConfigManager.addSetting(&rs485settings.txPin);
    ConfigManager.addSetting(&rs485settings.dePin);
}
