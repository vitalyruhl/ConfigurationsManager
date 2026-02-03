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
    limiterSettings.create();
    tempSettings.create();
    i2cSettings.create();
    fanSettings.create();
    heaterSettings.create();
    displaySettings.create();
    rs485settings.create();
}
