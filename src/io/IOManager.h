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

    struct LiveControlHandleBool {
        std::function<void(bool)>* onChange = nullptr;
        std::function<void()>* onClick = nullptr;

        LiveControlHandleBool& onChangeCallback(std::function<void(bool)> cb) {
            if (onChange) {
                *onChange = std::move(cb);
            }
            return *this;
        }

        LiveControlHandleBool& onClickCallback(std::function<void()> cb) {
            if (onClick) {
                *onClick = std::move(cb);
            }
            return *this;
        }
    };

    struct LiveControlHandleFloat {
        std::function<void(float)>* onChange = nullptr;

        LiveControlHandleFloat& onChangeCallback(std::function<void(float)> cb) {
            if (onChange) {
                *onChange = std::move(cb);
            }
            return *this;
        }
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

    struct AnalogInputBinding {
        const char* id = nullptr;
        const char* name = nullptr;

        // Defaults used before the first load from Preferences.
        int defaultPin = -1;
        bool defaultEnabled = true;

        // Mapping defaults. If you keep out range equal to raw range, the mapping becomes identity (raw == out).
        int defaultRawMin = 0;
        int defaultRawMax = 4095;
        float defaultOutMin = 0.0f;
        float defaultOutMax = 4095.0f;

        // UI / runtime display
        const char* defaultUnit = "";
        int defaultPrecision = 2;

        // Event configuration (used for onChange triggering)
        float defaultDeadband = 0.01f;
        uint32_t defaultMinEventMs = 10000;

        bool registerSettings = true;
        bool showPinInWeb = true;
        bool showMappingInWeb = true;
        bool showUnitInWeb = true;
        bool showDeadbandInWeb = true;
        bool showMinEventInWeb = true;
    };

    struct AnalogOutputBinding {
        const char* id = nullptr;
        const char* name = nullptr;

        // Defaults used before the first load from Preferences.
        int defaultPin = -1;
        bool defaultEnabled = true;

        // Value mapping: setValue/getValue operate in this range.
        // Internally, values are mapped to a raw voltage output range (0..3.3V).
        float valueMin = 0.0f;
        float valueMax = 100.0f;
        bool reverse = false;

        bool registerSettings = true;
        bool showPinInWeb = true;
    };

    struct AnalogAlarmCallbacks {
        // Fired on each alarm state transition (false->true or true->false)
        // The parameter is the new alarm state.
        std::function<void(bool)> onStateChanged;

        // Convenience callbacks (also fired only on transitions)
        std::function<void()> onEnter;
        std::function<void()> onExit;

        // Min/Max specific transitions (true when below min / above max)
        std::function<void(bool)> onMinStateChanged;
        std::function<void()> onMinEnter;
        std::function<void()> onMinExit;

        std::function<void(bool)> onMaxStateChanged;
        std::function<void()> onMaxEnter;
        std::function<void()> onMaxExit;
    };

    void addDigitalOutput(const DigitalOutputBinding& binding);
    void addDigitalInput(const DigitalInputBinding& binding);
    void addAnalogInput(const AnalogInputBinding& binding);

    // New parameter-list API (preferred)
    void addDigitalInput(const char* id,
                         const char* name,
                         int gpioPin,
                         bool activeLow,
                         bool pullup,
                         bool pulldown,
                         bool persistSettings);

    void addDigitalOutput(const char* id,
                          const char* name,
                          int gpioPin,
                          bool activeLow,
                          bool persistSettings);

    void addAnalogInput(const char* id,
                        const char* name,
                        int adcPin,
                        bool persistSettings,
                        int rawMin = 0,
                        int rawMax = 4095,
                        float outMin = 0.0f,
                        float outMax = 4095.0f,
                        const char* unit = "",
                        int precision = 2,
                        float deadband = 0.01f,
                        uint32_t minEventMs = 10000);

    // Analog output: value mapping (valueMin..valueMax) -> raw voltage (0..3.3V).
    // Note: initial implementation uses ESP32 DAC pins (GPIO25/26). PWM/LEDC is planned as a follow-up.
    void addAnalogOutput(const AnalogOutputBinding& binding);
    void addAnalogOutput(const char* id,
                         const char* name,
                         int dacOrPwmPin,
                         bool persistSettings,
                         float valueMin = 0.0f,
                         float valueMax = 100.0f,
                         bool reverse = false);

    // Optional: enable non-blocking button-like events for a digital input.
    // Works independently from the GUI.
    void configureDigitalInputEvents(const char* id,
                                    DigitalInputEventCallbacks callbacks);

    void configureDigitalInputEvents(const char* id,
                                    DigitalInputEventCallbacks callbacks,
                                    DigitalInputEventOptions options);

    // Settings placement (persisted IOs only)
    void addDigitalInputToSettings(const char* id, const char* pageName, int order);
    void addDigitalInputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
    void addDigitalInputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);

    void addDigitalOutputToSettings(const char* id, const char* pageName, int order);
    void addDigitalOutputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
    void addDigitalOutputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);

    void addAnalogInputToSettings(const char* id, const char* pageName, int order);
    void addAnalogInputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
    void addAnalogInputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);

    void addAnalogOutputToSettings(const char* id, const char* pageName, int order);
    void addAnalogOutputToSettingsGroup(const char* id, const char* pageName, const char* groupName, int order);
    void addAnalogOutputToSettingsGroup(const char* id, const char* pageName, const char* cardName, const char* groupName, int order);

    // Live placement (returns handle for callbacks)
    LiveControlHandleBool addDigitalInputToLive(const char* id, int order,
                                                const char* pageName = "Live",
                                                const char* cardName = "Live Values",
                                                const char* groupName = nullptr,
                                                const char* labelOverride = nullptr,
                                                bool alarmWhenActive = true);

    LiveControlHandleBool addDigitalOutputToLive(RuntimeControlType type,
                                                 const char* id,
                                                 int order,
                                                 const char* pageName = "Live",
                                                 const char* cardName = "Live Values",
                                                 const char* groupName = nullptr,
                                                 const char* labelOverride = nullptr,
                                                 const char* onLabel = nullptr,
                                                 const char* offLabel = nullptr);

    LiveControlHandleFloat addAnalogOutputToLive(const char* id,
                                                 int order,
                                                 float sliderMin,
                                                 float sliderMax,
                                                 int sliderPrecision,
                                                 const char* pageName = "Live",
                                                 const char* cardName = "Live Values",
                                                 const char* groupName = nullptr,
                                                 const char* labelOverride = nullptr,
                                                 const char* unit = nullptr);

    void addAnalogInputToLive(const char* id, int order,
                              const char* pageName = "Live",
                              const char* cardName = "Live Values",
                              const char* groupName = nullptr,
                              const char* labelOverride = nullptr,
                              bool showRaw = false);

    void addAnalogInputToLiveWithAlarm(const char* id, int order,
                                       float alarmMin,
                                       float alarmMax,
                                       AnalogAlarmCallbacks callbacks = {},
                                       const char* pageName = "Live",
                                       const char* cardName = "Live Values",
                                       const char* groupName = nullptr,
                                       const char* labelOverride = nullptr);

    // Legacy combined helpers (kept for now)
    void addIOtoGUI(const char* id, const char* cardName, int order);
    void addInputToGUI(const char* id, const char* cardName, int order,
                       const char* runtimeLabel = nullptr,
                       const char* runtimeGroup = "inputs",
                       bool alarmWhenActive = true);
    void addInputSettingsToGUI(const char* id, const char* cardName, int order);
    void addInputRuntimeToGUI(const char* id, int order,
                              const char* runtimeLabel = nullptr,
                              const char* runtimeGroup = "inputs",
                              bool alarmWhenActive = true);

    // Analog input settings + runtime values.
    // Shows either scaled OR raw value (depending on showRaw).
    // Call this function multiple times with different runtimeGroup values to show the same input in multiple cards.
    void addAnalogInputToGUI(const char* id, const char* cardName, int order,
                             const char* runtimeLabel = nullptr,
                             const char* runtimeGroup = "analog",
                             bool showRaw = false);

    // Registers the scaled value with alarm thresholds.
    // Alarm triggers when value < alarmMin OR value > alarmMax.
    void addAnalogInputToGUIWithAlarm(const char* id, const char* cardName, int order,
                                      float alarmMin, float alarmMax,
                                      const char* runtimeLabel = nullptr,
                                      const char* runtimeGroup = "analog");

    // Overload: same as above, but also sets callbacks for alarm transitions.
    void addAnalogInputToGUIWithAlarm(const char* id, const char* cardName, int order,
                                      float alarmMin, float alarmMax,
                                      AnalogAlarmCallbacks callbacks,
                                      const char* runtimeLabel = nullptr,
                                      const char* runtimeGroup = "analog");

    // Configure analog alarm thresholds + callbacks.
    // Use NAN for alarmMin and/or alarmMax to disable that boundary.
    // Alarm is evaluated on the scaled value (getAnalogValue / runtime scaled field).
    void configureAnalogInputAlarm(const char* id,
                                   float alarmMin,
                                   float alarmMax,
                                   AnalogAlarmCallbacks callbacks = {});

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

    // Analog output runtime slider (float).
    // Uses setValue/getValue internally.
    void addIOtoGUI(const char* id, const char* cardName, int order,
                    float sliderMin,
                    float sliderMax,
                    float sliderStep,
                    int sliderPrecision,
                    const char* runtimeLabel = nullptr,
                    const char* runtimeGroup = "controls",
                    const char* unit = nullptr);

    // Analog output helpers (naming aligned with AnalogInput helpers)
    void addAnalogOutputSliderToGUI(const char* id, const char* cardName, int order,
                                    float sliderMin,
                                    float sliderMax,
                                    float sliderStep,
                                    int sliderPrecision,
                                    const char* runtimeLabel = nullptr,
                                    const char* runtimeGroup = "controls",
                                    const char* unit = nullptr);

    // Read-only analog output values in runtime dashboard.
    // These do NOT change the output; they only display current values.
    void addAnalogOutputValueToGUI(const char* id, const char* cardName, int order,
                                   const char* runtimeLabel = nullptr,
                                   const char* runtimeGroup = "controls",
                                   const char* unit = nullptr,
                                   int precision = 2);

    // Shows the raw DAC value (0..255).
    void addAnalogOutputValueRawToGUI(const char* id, const char* cardName, int order,
                                      const char* runtimeLabel = nullptr,
                                      const char* runtimeGroup = "controls");

    // Shows the current output voltage (0..3.3V).
    void addAnalogOutputValueVoltToGUI(const char* id, const char* cardName, int order,
                                       const char* runtimeLabel = nullptr,
                                       const char* runtimeGroup = "controls",
                                       int precision = 3);

    void begin();
    void update();

    bool set(const char* id, bool on) { return setState(id, on); }
    bool getStatus(const char* id) const { return getState(id); }

    bool setState(const char* id, bool on);
    bool getState(const char* id) const;

    bool getInputState(const char* id) const;

    int getAnalogRawValue(const char* id) const;
    float getAnalogValue(const char* id) const;

    // Analog output API
    bool setValue(const char* id, float value);
    float getValue(const char* id) const;

    bool setRawValue(const char* id, float rawVolts);
    float getRawValue(const char* id) const;

    bool setDACValue(const char* id, int dacValue);
    int getDACValue(const char* id) const;

    bool isConfigured(const char* id) const;

private:
    struct AnalogRuntimeField {
        String id;
        bool showRaw = false;
    };

    struct AnalogRuntimeGroup {
        String group;
        std::vector<AnalogRuntimeField> fields;
    };

    enum class AnalogOutputRuntimeKind {
        ScaledValue,
        RawDac,
        Volts,
    };

    struct AnalogOutputRuntimeField {
        String id;
        String key;
        AnalogOutputRuntimeKind kind = AnalogOutputRuntimeKind::ScaledValue;
    };

    struct AnalogOutputRuntimeGroup {
        String group;
        std::vector<AnalogOutputRuntimeField> fields;
    };

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

        Config<int> *pin = nullptr;
        Config<bool> *activeLow = nullptr;

        int defaultPin = -1;
        bool defaultActiveLow = true;
        bool defaultEnabled = true;

        bool registerSettings = true;

        bool showPinInWeb = true;
        bool showActiveLowInWeb = true;

        bool desiredState = false;

        std::function<void(bool)> onChangeCallback;
        std::function<void()> onClickCallback;

        String runtimeGroup;

        int lastPin = -1;
        bool lastActiveLow = true;
        bool hasLast = false;
    };

    struct DigitalInputEntry {
        String id;
        String name;

        uint8_t slot = 0;

        bool settingsRegistered = false;
        bool runtimeRegistered = false;
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

        Config<int> *pin = nullptr;
        Config<bool> *activeLow = nullptr;
        Config<bool> *pullup = nullptr;
        Config<bool> *pulldown = nullptr;

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
        bool lastStateForCallback = false;
        bool hasLastStateForCallback = false;
        std::function<void(bool)> onChangeCallback;

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

    struct AnalogInputEntry {
        String id;
        String name;

        uint8_t slot = 0;

        bool settingsRegistered = false;
        String settingsCategory;
        String cardKey;
        String cardPretty;
        int cardOrder = 100;

        String keyPin;
        String keyRawMin;
        String keyRawMax;
        String keyOutMin;
        String keyOutMax;
        String keyUnit;
        String keyDeadband;
        String keyMinEventMs;
        String keyAlarmMin;
        String keyAlarmMax;

        std::shared_ptr<std::string> cardKeyStable;
        std::shared_ptr<std::string> cardPrettyStable;
        std::shared_ptr<std::string> keyPinStable;
        std::shared_ptr<std::string> keyRawMinStable;
        std::shared_ptr<std::string> keyRawMaxStable;
        std::shared_ptr<std::string> keyOutMinStable;
        std::shared_ptr<std::string> keyOutMaxStable;
        std::shared_ptr<std::string> keyUnitStable;
        std::shared_ptr<std::string> keyDeadbandStable;
        std::shared_ptr<std::string> keyMinEventMsStable;
        std::shared_ptr<std::string> keyAlarmMinStable;
        std::shared_ptr<std::string> keyAlarmMaxStable;

        Config<int> *pin = nullptr;
        Config<int> *rawMin = nullptr;
        Config<int> *rawMax = nullptr;
        Config<float> *outMin = nullptr;
        Config<float> *outMax = nullptr;
        Config<String> *unit = nullptr;
        Config<float> *deadband = nullptr;
        Config<int> *minEventMs = nullptr;
        Config<float> *alarmMinSetting = nullptr;
        Config<float> *alarmMaxSetting = nullptr;

        int defaultPin = -1;
        bool defaultEnabled = true;
        int defaultRawMin = 0;
        int defaultRawMax = 4095;
        float defaultOutMin = 0.0f;
        float defaultOutMax = 4095.0f;
        String defaultUnit;
        int defaultPrecision = 2;
        float defaultDeadband = 0.01f;
        uint32_t defaultMinEventMs = 10000;

        bool registerSettings = true;
        bool showPinInWeb = true;
        bool showMappingInWeb = true;
        bool showUnitInWeb = true;
        bool showDeadbandInWeb = true;
        bool showMinEventInWeb = true;

        String runtimeGroup;
        String runtimeLabel;
        int runtimeOrder = 100;
        bool runtimeShowRaw = true;
        bool runtimeShowScaled = true;

        int rawValue = -1;
        float value = NAN;

        float alarmMin = NAN;
        float alarmMax = NAN;
        AnalogAlarmCallbacks alarmCallbacks;
        bool alarmState = false;
        bool alarmMinState = false;
        bool alarmMaxState = false;
        bool alarmStateInitialized = false;

        int lastRawValue = -1;
        float lastValue = NAN;
        uint32_t lastEventMs = 0;
        bool warningLoggedInvalidPin = false;
    };

    struct AnalogOutputEntry {
        String id;
        String name;

        uint8_t slot = 0;

        bool settingsRegistered = false;
        String cardKey;
        String cardPretty;
        int cardOrder = 100;

        String keyPin;

        std::shared_ptr<std::string> cardKeyStable;
        std::shared_ptr<std::string> cardPrettyStable;
        std::shared_ptr<std::string> keyPinStable;

        Config<int> *pin = nullptr;

        int defaultPin = -1;
        bool defaultEnabled = true;

        float valueMin = 0.0f;
        float valueMax = 100.0f;
        bool reverse = false;

        bool registerSettings = true;
        bool showPinInWeb = true;

        float desiredRawVolts = 0.0f;
        float rawVolts = 0.0f;

        float desiredValue = 0.0f;
        float value = 0.0f;

        std::function<void(float)> onChangeCallback;

        int lastPin = -1;
        bool hasLast = false;
        bool warningLoggedInvalidPin = false;
    };

    std::vector<DigitalOutputEntry> digitalOutputs;
    std::vector<DigitalInputEntry> digitalInputs;
    std::vector<AnalogInputEntry> analogInputs;
    std::vector<AnalogOutputEntry> analogOutputs;

    std::vector<AnalogRuntimeGroup> analogRuntimeGroups;
    std::vector<AnalogOutputRuntimeGroup> analogOutputRuntimeGroups;

    uint32_t startupLongPressWindowEndsMs = 0;
    static constexpr uint32_t STARTUP_LONG_PRESS_WINDOW_MS = 10000;

    uint8_t nextDigitalOutputSlot = 0;
    uint8_t nextDigitalInputSlot = 0;
    uint8_t nextAnalogInputSlot = 0;
    uint8_t nextAnalogOutputSlot = 0;

    static bool isValidPin(int pin);

    int findIndex(const char* id) const;
    int findInputIndex(const char* id) const;
    int findAnalogInputIndex(const char* id) const;
    int findAnalogOutputIndex(const char* id) const;
    static bool isActiveLowNow(const DigitalOutputEntry& entry);
    static int getPinNow(const DigitalOutputEntry& entry);

    static bool isInputActiveLowNow(const DigitalInputEntry& entry);
    static bool isInputPullupNow(const DigitalInputEntry& entry);
    static bool isInputPulldownNow(const DigitalInputEntry& entry);
    static int getInputPinNow(const DigitalInputEntry& entry);
    static String formatAnalogSlotKey(uint8_t slot, char suffix);
    static String formatAnalogOutputSlotKey(uint8_t slot, char suffix);

    static bool isValidAnalogPin(int pin);
    static bool isValidAnalogOutputPin(int pin);

    static int getAnalogPinNow(const AnalogInputEntry& entry);
    static int getAnalogRawMinNow(const AnalogInputEntry& entry);
    static int getAnalogRawMaxNow(const AnalogInputEntry& entry);
    static float getAnalogOutMinNow(const AnalogInputEntry& entry);
    static float getAnalogOutMaxNow(const AnalogInputEntry& entry);
    static float getAnalogDeadbandNow(const AnalogInputEntry& entry);
    static uint32_t getAnalogMinEventMsNow(const AnalogInputEntry& entry);

    static float mapAnalogValue(int raw, int rawMin, int rawMax, float outMin, float outMax);
    void reconfigureIfNeeded(AnalogInputEntry& entry);
    void readAnalogInput(AnalogInputEntry& entry);
    void processAnalogEvents(AnalogInputEntry& entry, uint32_t nowMs);
    void processAnalogAlarm(AnalogInputEntry& entry);
    static void ensureAnalogAlarmSettings(AnalogInputEntry& entry, float alarmMin, float alarmMax);
    void ensureAnalogRuntimeProvider(const String& group);
    void registerAnalogRuntimeField(const String& group, const String& id, bool showRaw);

    void ensureAnalogOutputRuntimeProvider(const String& group);
    void registerAnalogOutputRuntimeField(const String& group, const String& id, const String& key, AnalogOutputRuntimeKind kind);

    static String formatSlotKey(uint8_t slot, char suffix);
    static String formatInputSlotKey(uint8_t slot, char suffix);

    void applyDesiredState(DigitalOutputEntry& entry);
    void reconfigureIfNeeded(DigitalOutputEntry& entry);

    void reconfigureIfNeeded(AnalogOutputEntry& entry);
    void applyDesiredAnalogOutput(AnalogOutputEntry& entry);

    static float clampFloat(float value, float minValue, float maxValue);
    static float mapFloat(float value, float inMin, float inMax, float outMin, float outMax);

    void reconfigureIfNeeded(DigitalInputEntry& entry);
    void readInputState(DigitalInputEntry& entry);
    void processInputEvents(DigitalInputEntry& entry, uint32_t nowMs);
    void ensureOutputRuntimeProvider(const String& group);
    void ensureInputRuntimeProvider(const String& group);

    bool isStartupLongPressWindowActive(uint32_t nowMs) const;

    static void writePinState(int pin, bool activeLow, bool on);
};

} // namespace cm
