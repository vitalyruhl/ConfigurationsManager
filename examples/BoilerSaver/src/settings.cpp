#include "settings.h"

// Define all the settings instances that are declared as extern in settings.h
I2CSettings i2cSettings;
DisplaySettings displaySettings;
BoilerSettings boilerSettings;
TempSensorSettings tempSensorSettings;

// Function to register all settings with ConfigManager
// This solves the static initialization order problem
void initializeAllSettings() {
    // Register settings directly
    ConfigManager.addSetting(&i2cSettings.sdaPin);
    ConfigManager.addSetting(&i2cSettings.sclPin);
    ConfigManager.addSetting(&i2cSettings.busFreq);
    ConfigManager.addSetting(&i2cSettings.displayAddr);

    ConfigManager.addSetting(&boilerSettings.enabled);
    ConfigManager.addSetting(&boilerSettings.onThreshold);
    ConfigManager.addSetting(&boilerSettings.offThreshold);
    ConfigManager.addSetting(&boilerSettings.boilerTimeMin);
    ConfigManager.addSetting(&boilerSettings.stopTimerOnTarget);
    ConfigManager.addSetting(&boilerSettings.onlyOncePerPeriod);

    ConfigManager.addSetting(&displaySettings.turnDisplayOff);
    ConfigManager.addSetting(&displaySettings.onTimeSec);

    // Temp sensor settings
    ConfigManager.addSetting(&tempSensorSettings.gpioPin);
    ConfigManager.addSetting(&tempSensorSettings.corrOffset);
    ConfigManager.addSetting(&tempSensorSettings.readInterval);
}
