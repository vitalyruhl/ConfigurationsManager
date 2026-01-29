#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>

class Helpers {
public:
    // convert a float value from one range to another
    // Input 0-1023 to output 0-100
    // Example: mapFloat(512, 0, 1023, 0, 100) = 50.0
    static float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);
    void checkVersion(String currentVersion, String currentVersionDate); //todo: add this Feature to the configManager
};

#endif // HELPERS_H
