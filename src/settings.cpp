#include "settings.h"

// Define all the settings instances that are declared as extern in settings.h
MQTT_Settings mqttSettings;
I2CSettings i2cSettings;
DisplaySettings displaySettings;
SystemSettings systemSettings;
ButtonSettings buttonSettings;
SigmaLogLevel logLevel;
WiFi_Settings wifiSettings;
BoilerSettings boilerSettings;

// Function to register all settings with ConfigManager
// This solves the static initialization order problem
void initializeAllSettings() {
    // Register settings with their dedicated registration methods
    wifiSettings.registerSettings();
    mqttSettings.registerSettings();

    // Register the other settings directly
    ConfigManager.addSetting(&i2cSettings.sdaPin);
    ConfigManager.addSetting(&i2cSettings.sclPin);
    ConfigManager.addSetting(&i2cSettings.busFreq);
    ConfigManager.addSetting(&i2cSettings.bmeFreq);
    ConfigManager.addSetting(&i2cSettings.displayAddr);

    ConfigManager.addSetting(&boilerSettings.enabled);
    ConfigManager.addSetting(&boilerSettings.onThreshold);
    ConfigManager.addSetting(&boilerSettings.offThreshold);
    ConfigManager.addSetting(&boilerSettings.relayPin);
    ConfigManager.addSetting(&boilerSettings.activeLow);
    ConfigManager.addSetting(&boilerSettings.boilerTimeMin);

    ConfigManager.addSetting(&displaySettings.turnDisplayOff);
    ConfigManager.addSetting(&displaySettings.onTimeSec);

    ConfigManager.addSetting(&systemSettings.allowOTA);
    ConfigManager.addSetting(&systemSettings.otaPassword);
    ConfigManager.addSetting(&systemSettings.wifiRebootTimeoutMin);
    ConfigManager.addSetting(&systemSettings.version);

    ConfigManager.addSetting(&buttonSettings.apModePin);
    ConfigManager.addSetting(&buttonSettings.resetDefaultsPin);
}