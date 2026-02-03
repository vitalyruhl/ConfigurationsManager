#include "settings.h"

// Define all the settings instances that are declared as extern in settings.h
I2CSettings i2cSettings;
DisplaySettings displaySettings;
BoilerSettings boilerSettings;
TempSensorSettings tempSensorSettings;

// Function to register all settings with ConfigManager
// This solves the static initialization order problem
void initializeAllSettings() {
    i2cSettings.create();
    boilerSettings.create();
    displaySettings.create();
    tempSensorSettings.create();
}
