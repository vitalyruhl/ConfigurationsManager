#pragma once

#include <Arduino.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "ConfigManager.h"

namespace cm {

class IOManager {
public:
    enum class RuntimeControlType {
        Button,
        MomentaryButton,
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

    struct DigitalInputBinding {
        const char* id = nullptr;
        const char* name = nullptr;

        // Defaults used before the first load from Preferences.
        int defaultPin = -1;
        bool defaultActiveLow = true;
        bool defaultPullup = true;
        bool defaultPulldown = false;
        bool defaultEnabled = true;

        bool registerSettings = true;

        // Visibility controls (UI only). Hidden settings still exist and apply.
        bool showPinInWeb = true;
        bool showActiveLowInWeb = true;
        bool showPullupInWeb = true;
        bool showPulldownInWeb = true;
    };

    struct DigitalInputEventCallbacks {
        std::function<void()> onPress;
        std::function<void()> onRelease;
        std::function<void()> onClick;
        std::function<void()> onDoubleClick;
        std::function<void()> onLongClick;
        std::function<void()> onLongPressOnStartup;
    };

    struct DigitalInputEventOptions {
        uint32_t debounceMs = 40;
        uint32_t doubleClickMs = 350;
        uint32_t longClickMs = 700;
    };

    void addDigitalOutput(const DigitalOutputBinding& binding);
    void addDigitalInput(const DigitalInputBinding& binding);

    // Optional: enable non-blocking button-like events for a digital input.
    // Works independently from the GUI.
    void configureDigitalInputEvents(const char* id,
                                    DigitalInputEventCallbacks callbacks);

    void configureDigitalInputEvents(const char* id,
                                    DigitalInputEventCallbacks callbacks,
                                    DigitalInputEventOptions options);

    // Registers the IO item's settings into ConfigManager and groups them into a dedicated Settings card.
    // The persisted category token remains cm::CoreCategories::IO.
    void addIOtoGUI(const char* id, const char* cardName, int order);

    // Digital input settings + runtime indicator (bool dot).
    // By default it sets boolAlarmValue=true (alarm when input is logically active).
    void addInputToGUI(const char* id, const char* cardName, int order,
                       const char* runtimeLabel = nullptr,
                       const char* runtimeGroup = "inputs",
                       bool alarmWhenActive = true);

    // Overload: also registers a runtime control.
    // - Use RuntimeControlType::Button with a single callback (momentary action)
    // - Use RuntimeControlType::MomentaryButton with getter + setter (button UI; sets true on press, false on release)
    // - Use RuntimeControlType::{Checkbox,StateButton} with getter + setter (switch)
    void addIOtoGUI(const char* id, const char* cardName, int order, RuntimeControlType type,
                    std::function<void()> onPress,
                    const char* runtimeLabel = nullptr,
                    const char* runtimeGroup = "controls");

    void addIOtoGUI(const char* id, const char* cardName, int order, RuntimeControlType type,
                    std::function<bool()> getter,
                    std::function<void(bool)> setter,
                    const char* runtimeLabel = nullptr,
                    const char* runtimeGroup = "controls",
                    const char* runtimeOnLabel = nullptr,
                    const char* runtimeOffLabel = nullptr);

    void begin();
    void update();

    bool set(const char* id, bool on) { return setState(id, on); }
    bool getStatus(const char* id) const { return getState(id); }

    bool setState(const char* id, bool on);
    bool getState(const char* id) const;

    bool getInputState(const char* id) const;

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

        // Stable backing storage for ConfigOptions pointers (vector moves must not corrupt strings).
        std::shared_ptr<std::string> cardKeyStable;
        std::shared_ptr<std::string> cardPrettyStable;
        std::shared_ptr<std::string> keyPinStable;
        std::shared_ptr<std::string> keyActiveLowStable;

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

    struct DigitalInputEntry {
        String id;
        String name;

        uint8_t slot = 0;

        bool settingsRegistered = false;
        String cardKey;
        String cardPretty;
        int cardOrder = 100;

        String keyPin;
        String keyActiveLow;
        String keyPullup;
        String keyPulldown;

        // Stable backing storage for ConfigOptions pointers (vector moves must not corrupt strings).
        std::shared_ptr<std::string> cardKeyStable;
        std::shared_ptr<std::string> cardPrettyStable;
        std::shared_ptr<std::string> keyPinStable;
        std::shared_ptr<std::string> keyActiveLowStable;
        std::shared_ptr<std::string> keyPullupStable;
        std::shared_ptr<std::string> keyPulldownStable;

        std::unique_ptr<Config<int>> pin;
        std::unique_ptr<Config<bool>> activeLow;
        std::unique_ptr<Config<bool>> pullup;
        std::unique_ptr<Config<bool>> pulldown;

        int defaultPin = -1;
        bool defaultActiveLow = true;
        bool defaultPullup = true;
        bool defaultPulldown = false;
        bool defaultEnabled = true;

        bool registerSettings = true;

        bool showPinInWeb = true;
        bool showActiveLowInWeb = true;
        bool showPullupInWeb = true;
        bool showPulldownInWeb = true;

        bool state = false;

        int lastPin = -1;
        bool lastActiveLow = true;
        bool lastPullup = true;
        bool lastPulldown = false;
        bool hasLast = false;

        String runtimeGroup;
        String runtimeLabel;
        int runtimeOrder = 100;
        bool alarmWhenActive = true;

        // Event detection (optional)
        DigitalInputEventCallbacks callbacks;
        DigitalInputEventOptions eventOptions;
        bool eventsEnabled = false;

        bool rawState = false;
        bool debouncedState = false;
        uint32_t lastRawChangeMs = 0;

        uint32_t pressStartMs = 0;
        bool longFired = false;
        uint8_t clickCount = 0;
        uint32_t lastReleaseMs = 0;
    };

    std::vector<DigitalOutputEntry> digitalOutputs;
    std::vector<DigitalInputEntry> digitalInputs;

    uint32_t startupLongPressWindowEndsMs = 0;
    static constexpr uint32_t STARTUP_LONG_PRESS_WINDOW_MS = 10000;

    uint8_t nextDigitalOutputSlot = 0;
    uint8_t nextDigitalInputSlot = 0;

    static bool isValidPin(int pin);

    int findIndex(const char* id) const;
    int findInputIndex(const char* id) const;
    static bool isActiveLowNow(const DigitalOutputEntry& entry);
    static int getPinNow(const DigitalOutputEntry& entry);

    static bool isInputActiveLowNow(const DigitalInputEntry& entry);
    static bool isInputPullupNow(const DigitalInputEntry& entry);
    static bool isInputPulldownNow(const DigitalInputEntry& entry);
    static int getInputPinNow(const DigitalInputEntry& entry);

    static String formatSlotKey(uint8_t slot, char suffix);
    static String formatInputSlotKey(uint8_t slot, char suffix);

    void applyDesiredState(DigitalOutputEntry& entry);
    void reconfigureIfNeeded(DigitalOutputEntry& entry);

    void reconfigureIfNeeded(DigitalInputEntry& entry);
    void readInputState(DigitalInputEntry& entry);
    void processInputEvents(DigitalInputEntry& entry, uint32_t nowMs);
    void ensureInputRuntimeProvider(const String& group);

    bool isStartupLongPressWindowActive(uint32_t nowMs) const;

    static void writePinState(int pin, bool activeLow, bool on);
};

} // namespace cm
