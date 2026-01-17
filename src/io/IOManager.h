#pragma once

#include <Arduino.h>
#include <vector>

#include "ConfigManager.h"

namespace cm {

class IOManager {
public:
    struct DigitalOutputBinding {
        const char* id = nullptr;
        Config<int>* pin = nullptr;
        Config<bool>* activeLow = nullptr;
        Config<bool>* enabled = nullptr; // Optional. If nullptr, relay is always enabled.
    };

    void addDigitalOutput(const DigitalOutputBinding& binding);

    void begin();
    void update();

    bool setState(const char* id, bool on);
    bool getState(const char* id) const;

    bool isConfigured(const char* id) const;

private:
    struct DigitalOutputEntry {
        String id;
        Config<int>* pin = nullptr;
        Config<bool>* activeLow = nullptr;
        Config<bool>* enabled = nullptr;

        bool desiredState = false;

        int lastPin = -1;
        bool lastActiveLow = true;
        bool lastEnabled = true;
        bool hasLast = false;
    };

    std::vector<DigitalOutputEntry> digitalOutputs;

    static bool isValidPin(int pin);

    int findIndex(const char* id) const;

    static bool isEnabledNow(const DigitalOutputEntry& entry);
    static bool isActiveLowNow(const DigitalOutputEntry& entry);
    static int getPinNow(const DigitalOutputEntry& entry);

    void applyDesiredState(DigitalOutputEntry& entry);
    void reconfigureIfNeeded(DigitalOutputEntry& entry);

    static void writePinState(int pin, bool activeLow, bool on);
};

} // namespace cm
