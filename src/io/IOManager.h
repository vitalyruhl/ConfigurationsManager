#pragma once

#include <Arduino.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "ConfigManager.h"

namespace cm {

class IOManager {
public:
    enum class RuntimeControlType {
        Button,
        Checkbox,
        StateButton,
    };

    struct DigitalOutputBinding {
        const char* id = nullptr;
        const char* name = nullptr;

        // Defaults used before the first load from Preferences.
        int defaultPin = -1;
        bool defaultActiveLow = true;
        bool defaultEnabled = true;

        // If false, IOManager will not register any ConfigManager settings for this output.
        // That means: no Settings tab entries, no persistence, and defaults are used at runtime.
        bool registerSettings = true;

        // Visibility controls (UI only). Hidden settings still exist and apply.
        bool showPinInWeb = true;
        bool showActiveLowInWeb = true;
    };

    void addDigitalOutput(const DigitalOutputBinding& binding);

    // Registers the IO item's settings into ConfigManager and groups them into a dedicated Settings card.
    // The persisted category token remains cm::CoreCategories::IO.
    void addIOtoGUI(const char* id, const char* cardName, int order);

    // Overload: also registers a runtime control.
    // - Use RuntimeControlType::Button with a single callback (momentary action)
    // - Use RuntimeControlType::{Checkbox,StateButton} with getter + setter (switch)
    void addIOtoGUI(const char* id, const char* cardName, int order, RuntimeControlType type,
                    std::function<void()> onPress,
                    const char* runtimeLabel = nullptr,
                    const char* runtimeGroup = "controls");

    void addIOtoGUI(const char* id, const char* cardName, int order, RuntimeControlType type,
                    std::function<bool()> getter,
                    std::function<void(bool)> setter,
                    const char* runtimeLabel = nullptr,
                    const char* runtimeGroup = "controls");

    void begin();
    void update();

    bool set(const char* id, bool on) { return setState(id, on); }
    bool getStatus(const char* id) const { return getState(id); }

    bool setState(const char* id, bool on);
    bool getState(const char* id) const;

    bool isConfigured(const char* id) const;

private:
    struct DigitalOutputEntry {
        String id;
        String name;

        uint8_t slot = 0;

        bool settingsRegistered = false;
        String cardKey;
        String cardPretty;
        int cardOrder = 100;

        String keyPin;
        String keyActiveLow;

        std::unique_ptr<Config<int>> pin;
        std::unique_ptr<Config<bool>> activeLow;

        int defaultPin = -1;
        bool defaultActiveLow = true;
        bool defaultEnabled = true;

        bool registerSettings = true;

        bool showPinInWeb = true;
        bool showActiveLowInWeb = true;

        bool desiredState = false;

        int lastPin = -1;
        bool lastActiveLow = true;
        bool hasLast = false;
    };

    std::vector<DigitalOutputEntry> digitalOutputs;

    uint8_t nextDigitalOutputSlot = 0;

    static bool isValidPin(int pin);

    int findIndex(const char* id) const;
    static bool isActiveLowNow(const DigitalOutputEntry& entry);
    static int getPinNow(const DigitalOutputEntry& entry);

    static String formatSlotKey(uint8_t slot, char suffix);

    void applyDesiredState(DigitalOutputEntry& entry);
    void reconfigureIfNeeded(DigitalOutputEntry& entry);

    static void writePinState(int pin, bool activeLow, bool on);
};

} // namespace cm
