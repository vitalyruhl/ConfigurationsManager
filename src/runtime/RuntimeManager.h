#pragma once

#include <ArduinoJson.h>
#include <functional>
#include <mutex>
#include <vector>
#include "../ConfigManagerConfig.h"

// Forward declaration
class ConfigManagerClass;

// Runtime value provider structure
struct RuntimeValueProvider {
    String name;
    std::function<void(JsonObject&)> fill;
    int order = 100;

    RuntimeValueProvider(const String& n, std::function<void(JsonObject&)> f, int o = 100)
        : name(n), fill(f), order(o) {}
};

struct RuntimeStyleProperty {
    String key;
    String value;
};

struct RuntimeStyleRule {
    String target;
    std::vector<RuntimeStyleProperty> properties;
    bool hasVisible = false;
    bool visible = true;
    String className;

    RuntimeStyleRule& set(const char* property, const char* value) {
        if (!property || !property[0]) {
            return *this;
        }
        for (auto& prop : properties) {
            if (prop.key == property) {
                prop.value = value ? value : "";
                return *this;
            }
        }
        properties.push_back(RuntimeStyleProperty{String(property), value ? String(value) : String()});
        return *this;
    }

    RuntimeStyleRule& setVisible(bool value) {
        hasVisible = true;
        visible = value;
        return *this;
    }

    RuntimeStyleRule& addCSSClass(const char* cssClass) {
        if (!cssClass || !cssClass[0]) {
            return *this;
        }
        if (!className.isEmpty()) {
            className += " ";
        }
        className += cssClass;
        return *this;
    }
};

struct RuntimeFieldStyle {
    std::vector<RuntimeStyleRule> rules;

    RuntimeStyleRule& rule(const char* targetName) {
        String target = targetName ? String(targetName) : String();
        for (auto& rule : rules) {
            if (rule.target == target) {
                return rule;
            }
        }
        RuntimeStyleRule created;
        created.target = target;
        rules.push_back(std::move(created));
        return rules.back();
    }

    bool empty() const {
        return rules.empty();
    }
};

// Runtime field metadata structure
struct RuntimeFieldMeta {
    String group;
    String sourceGroup;
    String page;
    String key;
    String label;
    String unit;
    String onLabel;
    String offLabel;
    int precision = 2;
    int order = 100;
    bool isBool = false;
    bool isString = false;
    bool isDivider = false;
    bool isButton = false;
    bool isCheckbox = false;
    bool isStateButton = false;
    bool isMomentaryButton = false;
    bool isIntSlider = false;
    bool isFloatSlider = false;
    bool isIntInput = false;
    bool isFloatInput = false;
    bool hasAlarm = false;
    bool alarmWhenTrue = false;
    bool boolAlarmValue = false;
    float alarmMin = 0.0f;
    float alarmMax = 0.0f;
    float warnMin = 0.0f;
    float warnMax = 0.0f;
    // Slider specific properties
    int intMin = 0;
    int intMax = 100;
    int intInit = 0;
    float floatMin = 0.0f;
    float floatMax = 1.0f;
    float floatInit = 0.0f;
    int floatPrecision = 2;
    bool initialState = false;
    String staticValue;
    String card;
    RuntimeFieldStyle style;
};

// Interactive control structures
struct RuntimeButton {
    String group;
    String key;
    std::function<void()> onPress;

    RuntimeButton(const String& g, const String& k, std::function<void()> press)
        : group(g), key(k), onPress(press) {}
};

struct RuntimeCheckbox {
    String group;
    String key;
    std::function<bool()> getter;
    std::function<void(bool)> setter;

    RuntimeCheckbox(const String& g, const String& k, std::function<bool()> get, std::function<void(bool)> set)
        : group(g), key(k), getter(get), setter(set) {}
};

struct RuntimeStateButton {
    String group;
    String key;
    std::function<bool()> getter;
    std::function<void(bool)> setter;

    RuntimeStateButton(const String& g, const String& k, std::function<bool()> get, std::function<void(bool)> set)
        : group(g), key(k), getter(get), setter(set) {}
};

struct RuntimeIntSlider {
    String group;
    String key;
    std::function<int()> getter;
    std::function<void(int)> setter;
    int minV;
    int maxV;

    RuntimeIntSlider(const String& g, const String& k, std::function<int()> get, std::function<void(int)> set, int min, int max)
        : group(g), key(k), getter(get), setter(set), minV(min), maxV(max) {}
};

struct RuntimeFloatSlider {
    String group;
    String key;
    std::function<float()> getter;
    std::function<void(float)> setter;
    float minV;
    float maxV;

    RuntimeFloatSlider(const String& g, const String& k, std::function<float()> get, std::function<void(float)> set, float min, float max)
        : group(g), key(k), getter(get), setter(set), minV(min), maxV(max) {}
};

struct RuntimeIntInput {
    String group;
    String key;
    std::function<int()> getter;
    std::function<void(int)> setter;
    int minV;
    int maxV;

    RuntimeIntInput(const String& g, const String& k, std::function<int()> get, std::function<void(int)> set, int min, int max)
        : group(g), key(k), getter(get), setter(set), minV(min), maxV(max) {}
};

struct RuntimeFloatInput {
    String group;
    String key;
    std::function<float()> getter;
    std::function<void(float)> setter;
    float minV;
    float maxV;

    RuntimeFloatInput(const String& g, const String& k, std::function<float()> get, std::function<void(float)> set, float min, float max)
        : group(g), key(k), getter(get), setter(set), minV(min), maxV(max) {}
};

struct RuntimeAlarm {
    String name;
    bool active = false;
    bool manual = false;
    std::function<bool()> checkFunction = nullptr;
    std::function<void()> onTrigger = nullptr;
    std::function<void()> onClear = nullptr;
};

class ConfigManagerRuntime {
public:
    typedef std::function<void(const char*)> LogCallback;

private:
    ConfigManagerClass* configManager;
    LogCallback logCallback;
    // runtime-safe contract:
    // - registration/mutation methods lock this mutex
    // - JSON serializers copy snapshots under lock, then serialize unlocked
    mutable std::mutex runtimeDataMutex;

    // Runtime data
    std::vector<RuntimeValueProvider> runtimeProviders;
    std::vector<RuntimeFieldMeta> runtimeMeta;

    std::vector<RuntimeButton> runtimeButtons;

    std::vector<RuntimeCheckbox> runtimeCheckboxes;

    std::vector<RuntimeStateButton> runtimeStateButtons;

    std::vector<RuntimeIntSlider> runtimeIntSliders;

    std::vector<RuntimeFloatSlider> runtimeFloatSliders;

    std::vector<RuntimeIntInput> runtimeIntInputs;
    std::vector<RuntimeFloatInput> runtimeFloatInputs;

    std::vector<RuntimeAlarm> runtimeAlarms;

    // System provider data
#if CM_ENABLE_SYSTEM_PROVIDER
    bool builtinSystemProviderEnabled;
    bool builtinSystemProviderRegistered;
    unsigned long loopWindowStart;
    uint32_t loopSamples;
    double loopAccumMs;
    double loopAvgMs;
#endif

    // Helper methods
    void log(const char* format, ...) const;
    void sortProviders();
    void sortMeta();
    RuntimeAlarm* findAlarm(const String& name);
    const RuntimeAlarm* findAlarm(const String& name) const;

public:
    ConfigManagerRuntime();
    ~ConfigManagerRuntime();

    // Initialization
    void begin(ConfigManagerClass* cm);
    void setLogCallback(LogCallback logger);

    // Runtime value providers
    // Thread-safe registration: may be called while runtime JSON is generated.
    void addRuntimeProvider(const RuntimeValueProvider& provider);
    void addRuntimeProvider(const String& name, std::function<void(JsonObject&)> fillFunc, int order = 100);

    // Runtime metadata
    // Thread-safe registration: may be called while runtime JSON is generated.
    void addRuntimeMeta(const RuntimeFieldMeta& meta);
    bool updateRuntimeMeta(const String& group, const String& key, const std::function<void(RuntimeFieldMeta&)>& updater);
    // Note: returned pointer is only stable until the next runtime meta mutation.
    RuntimeFieldMeta* findRuntimeMeta(const String& group, const String& key);

    // JSON generation
    String runtimeValuesToJSON();
    String runtimeMetaToJSON();

    // System provider
#if CM_ENABLE_SYSTEM_PROVIDER
    void enableBuiltinSystemProvider();
    void updateLoopTiming();
    double getLoopAverage() const { return loopAvgMs; }
    // Refresh cached sketch metrics (size/free) to avoid repeated bootloader_mmap calls
    void refreshSketchInfoCache();
#else
    void enableBuiltinSystemProvider() {}
    void updateLoopTiming() {}
    double getLoopAverage() const { return 0.0; }
    void refreshSketchInfoCache() {}
#endif

    // Alarms
    void addRuntimeAlarm(const String& name, std::function<bool()> checkFunction);
    void addRuntimeAlarm(const String& name, std::function<bool()> checkFunction,
                         std::function<void()> onTrigger, std::function<void()> onClear = nullptr);
    void registerRuntimeAlarm(const String& name, std::function<void()> onTrigger = nullptr, std::function<void()> onClear = nullptr);
    void setRuntimeAlarmActive(const String& name, bool active, bool fireCallbacks = true);
    bool isRuntimeAlarmActive(const String& name) const;
    void updateAlarms();
    bool hasActiveAlarms() const;
    std::vector<String> getActiveAlarms() const;

    // Development support
#ifdef development
    std::vector<RuntimeFieldMeta> runtimeMetaOverride;
    bool runtimeMetaOverrideActive = false;
    void setRuntimeMetaOverride(const std::vector<RuntimeFieldMeta>& override);
    void clearRuntimeMetaOverride();
#endif

    // Interactive runtime controls
    void defineRuntimeButton(const String& group, const String& key, const String& label,
                           std::function<void()> onPress, const String& card = String(), int order = 100);
    void handleButtonPress(const String& group, const String& key);

    void defineRuntimeCheckbox(const String& group, const String& key, const String& label,
                             std::function<bool()> getter, std::function<void(bool)> setter,
                             const String& card = String(), int order = 100);
    void handleCheckboxChange(const String& group, const String& key, bool value);

    void defineRuntimeStateButton(const String& group, const String& key, const String& label,
                                std::function<bool()> getter, std::function<void(bool)> setter,
                                bool initState = false, const String& card = String(), int order = 100,
                                const String& onLabel = String(), const String& offLabel = String());
    void defineRuntimeMomentaryButton(const String& group, const String& key, const String& label,
                                      std::function<bool()> getter, std::function<void(bool)> setter,
                                      const String& card = String(), int order = 100,
                                      const String& onLabel = String(), const String& offLabel = String());
    void handleStateButtonToggle(const String& group, const String& key);
    void handleStateButtonSet(const String& group, const String& key, bool value);

    void defineRuntimeIntSlider(const String& group, const String& key, const String& label,
                              int minValue, int maxValue, int initValue,
                              std::function<int()> getter, std::function<void(int)> setter,
                              const String& unit = String(), const String& card = String(), int order = 100);
    void handleIntSliderChange(const String& group, const String& key, int value);

    void defineRuntimeFloatSlider(const String& group, const String& key, const String& label,
                                float minValue, float maxValue, float initValue, int precision,
                                std::function<float()> getter, std::function<void(float)> setter,
                                const String& unit = String(), const String& card = String(), int order = 100);
    void handleFloatSliderChange(const String& group, const String& key, float value);

    // Numeric value inputs (separate from sliders)
    void defineRuntimeIntValue(const String& group, const String& key, const String& label,
                               int minValue, int maxValue, int initValue,
                               std::function<int()> getter, std::function<void(int)> setter,
                               const String& unit = String(), const String& card = String(), int order = 100);
    void handleIntInputChange(const String& group, const String& key, int value);

    void defineRuntimeFloatValue(const String& group, const String& key, const String& label,
                                 float minValue, float maxValue, float initValue, int precision,
                                 std::function<float()> getter, std::function<void(float)> setter,
                                 const String& unit = String(), const String& card = String(), int order = 100);
    void handleFloatInputChange(const String& group, const String& key, float value);

    // Live runtime control registrations (meta handled externally)
    void registerRuntimeButton(const String& group, const String& key, std::function<void()> onPress);
    void registerRuntimeCheckbox(const String& group, const String& key,
                                 std::function<bool()> getter, std::function<void(bool)> setter);
    void registerRuntimeStateButton(const String& group, const String& key,
                                    std::function<bool()> getter, std::function<void(bool)> setter);
    void registerRuntimeMomentaryButton(const String& group, const String& key,
                                        std::function<bool()> getter, std::function<void(bool)> setter);
    void registerRuntimeIntSlider(const String& group, const String& key,
                                  std::function<int()> getter, std::function<void(int)> setter,
                                  int minValue, int maxValue);
    void registerRuntimeFloatSlider(const String& group, const String& key,
                                    std::function<float()> getter, std::function<void(float)> setter,
                                    float minValue, float maxValue);
    void registerRuntimeIntInput(const String& group, const String& key,
                                 std::function<int()> getter, std::function<void(int)> setter,
                                 int minValue, int maxValue);
    void registerRuntimeFloatInput(const String& group, const String& key,
                                   std::function<float()> getter, std::function<void(float)> setter,
                                   float minValue, float maxValue);
};
