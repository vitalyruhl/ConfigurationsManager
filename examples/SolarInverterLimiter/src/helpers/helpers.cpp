#include "helpers.h"
#include <Preferences.h>
#include "logging/LoggingManager.h"

static cm::LoggingManager &lmg = cm::LoggingManager::instance();
using LL = cm::LoggingManager::Level;

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


/** info
 * @param BlinkCount int -> number of blinks
 * @param blinkRate int -> blink rate in ms
 * @brief blinkBuidInLED() blinks the built-in LED a specified number of times at a specified rate.
 * @example: blinkBuidInLED(3, 1000) // Blink 3 times with a 1000ms delay
 */
void  Helpers::blinkBuidInLED(int BlinkCount, int blinkRate)
{
  // BlinkCount = 3; // number of blinks
  // blinkRate = 1000; // blink rate in ms

  for (int i = 0; i < BlinkCount; i++)
  {
    digitalWrite(LED_BUILTIN, HIGH); // turn the LED on (HIGH is the voltage level)
    delay(blinkRate);                // wait for a second
    digitalWrite(LED_BUILTIN, LOW);  // turn the LED off (LOW is the voltage level)
    delay(blinkRate);                // wait for a second
  }
}

/** info
 * @brief blinkBuidInLEDsetpinMode() initializes the built-in LED pin mode as an output.
 * @example: blinkBuidInLEDsetpinMode() // Initialize the built-in LED pin mode
 */
void  Helpers::blinkBuidInLEDsetpinMode() {
  pinMode(LED_BUILTIN, OUTPUT); // initialize GPIO pin 2 LED_BUILTIN as an output.
}


void Helpers::checkVersion(String currentVersion, String currentVersionDate)
{
    int currentMajor = 0, currentMinor = 0, currentPatch = 0;
    int major = 0, minor = 0, patch = 0;

    Preferences prefs;
    if (!prefs.begin("SolarInverterLimiter", true))
    {
        lmg.logTag(LL::Warn, "Helpers", "Failed to open Preferences namespace for version check");
        return;
    }

    String version = prefs.getString("version", "0.0.0");
    prefs.end();

    if (version == "0.0.0") // there is no version saved, set the default version
    {
       // todo: save version using ConfigManager settings storage
        return;
    }

    lmg.logTag(LL::Debug, "Helpers", "Current version: %s", currentVersion.c_str());
    lmg.logTag(LL::Debug, "Helpers", "Current version date: %s", currentVersionDate.c_str());
    lmg.logTag(LL::Debug, "Helpers", "Saved version: %s", version.c_str());

    sscanf(version.c_str(), "%d.%d.%d", &major, &minor, &patch);
    sscanf(currentVersion.c_str(), "%d.%d.%d", &currentMajor, &currentMinor, &currentPatch);

    if (currentMajor != major || currentMinor != minor)
    {
        lmg.logTag(LL::Warn, "Helpers", "Version changed (%d.%d -> %d.%d); settings migration is not implemented yet",
                   major, minor, currentMajor, currentMinor);
        // todo: implement a migration strategy and/or "remove all settings" helper
    }

}
