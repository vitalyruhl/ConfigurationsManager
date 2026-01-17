#include "helpers.h"
#include <Preferences.h>
#include "logging/logging.h"

/** info
 * @param x float -> the value to be converted
 * @param in_min the minimum value of the input range
 * @param in_max the maximum value of the input range
 * @param out_min the minimum value of the output range
 * @param out_max the maximum value of the output range
 * @return the converted value
 * @brief mapFloat() converts a float value from one range to another.
 * @example: mapFloat(512, 0, 1023, 0, 100) = 50.0 // Input 0-1023 to output 0-100
 */
float Helpers::mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


void Helpers::checkVersion(String currentVersion, String currentVersionDate)
{
    int currentMajor = 0, currentMinor = 0, currentPatch = 0;
    int major = 0, minor = 0, patch = 0;

    String version = Preferences().getString("version", "0.0.0");

    if (version == "0.0.0") // there is no version saved, set the default version
    {
       //todo:save Version to EEPROM from new ConfigurationMananger Library
        return;
    }

    sl->Printf("Current version: %s", currentVersion).Debug();
    sll->Printf("Cur. Version: %s", currentVersion).Debug();
    sl->Printf("Current Version_Date: %s", currentVersionDate).Debug();
    sll->Printf("from: %s", currentVersionDate).Debug();
    sl->Printf("Saved version: %s", version.c_str()).Debug();

    sscanf(version.c_str(), "%d.%d.%d", &major, &minor, &patch);
    sscanf(currentVersion.c_str(), "%d.%d.%d", &currentMajor, &currentMinor, &currentPatch);

    if (currentMajor != major || currentMinor != minor)
    {
        sl->Printf("Version changed, removing all settings...").Debug();
        sll->Printf("Version changed, removing all settings...").Debug();
        //todo: implement a function to remove all settings from new ConfigurationMananger Library
        
        sll->Printf("restarting...").Debug();
        //deactivate until the new version is implemented
        // delay(10000);  // Wait for 10 seconds to avoid multiple resets

        // ESP.restart(); // Restart the ESP32
    }

}
