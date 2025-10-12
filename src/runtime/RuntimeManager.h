#pragma once

#include <ArduinoJson.h>
#include <functional>
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

// Runtime field metadata structure
struct RuntimeFieldMeta {
    String group;
    String key;
    String label;
    String unit;
    int precision = 2;
    int order = 100;
    bool isBool = false;
    bool isButton = false;
    bool isCheckbox = false;
    bool isStateButton = false;
    bool isIntSlider = false;
    bool isFloatSlider = false;
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
    String card;
};

// Interactive control structures
#if CM_ENABLE_RUNTIME_BUTTONS
struct RuntimeButton {
    String group;
    String key;
    std::function<void()> onPress;
    
    RuntimeButton(const String& g, const String& k, std::function<void()> press)
        : group(g), key(k), onPress(press) {}
};
#endif

#if CM_ENABLE_RUNTIME_CHECKBOXES
struct RuntimeCheckbox {
    String group;
    String key;
    std::function<bool()> getter;
    std::function<void(bool)> setter;
    
    RuntimeCheckbox(const String& g, const String& k, std::function<bool()> get, std::function<void(bool)> set)
        : group(g), key(k), getter(get), setter(set) {}
};
#endif

#if CM_ENABLE_RUNTIME_STATE_BUTTONS
struct RuntimeStateButton {
    String group;
    String key;
    std::function<bool()> getter;
    std::function<void(bool)> setter;
    
    RuntimeStateButton(const String& g, const String& k, std::function<bool()> get, std::function<void(bool)> set)
        : group(g), key(k), getter(get), setter(set) {}
};
#endif

#if (CM_ENABLE_RUNTIME_INT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
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
#endif

#if (CM_ENABLE_RUNTIME_FLOAT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
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
#endif

#if CM_ENABLE_RUNTIME_ALARMS
struct RuntimeAlarm {
    String name;
    bool active = false;
    std::function<bool()> checkFunction;
    std::function<void()> onTrigger = nullptr;
    std::function<void()> onClear = nullptr;
    
    RuntimeAlarm(const String& n, std::function<bool()> check)
        : name(n), checkFunction(check) {}
        
    RuntimeAlarm(const String& n, std::function<bool()> check, 
                 std::function<void()> trigger, std::function<void()> clear)
        : name(n), checkFunction(check), onTrigger(trigger), onClear(clear) {}
};
#endif

class ConfigManagerRuntime {
public:
    typedef std::function<void(const char*)> LogCallback;

private:
    ConfigManagerClass* configManager;
    LogCallback logCallback;
    
    // Runtime data
    std::vector<RuntimeValueProvider> runtimeProviders;
    std::vector<RuntimeFieldMeta> runtimeMeta;
    
#if CM_ENABLE_RUNTIME_BUTTONS
    std::vector<RuntimeButton> runtimeButtons;
#endif

#if CM_ENABLE_RUNTIME_CHECKBOXES
    std::vector<RuntimeCheckbox> runtimeCheckboxes;
#endif

#if CM_ENABLE_RUNTIME_STATE_BUTTONS
    std::vector<RuntimeStateButton> runtimeStateButtons;
#endif

#if (CM_ENABLE_RUNTIME_INT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
    std::vector<RuntimeIntSlider> runtimeIntSliders;
#endif

#if (CM_ENABLE_RUNTIME_FLOAT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
    std::vector<RuntimeFloatSlider> runtimeFloatSliders;
#endif
    
#if CM_ENABLE_RUNTIME_ALARMS
    std::vector<RuntimeAlarm> runtimeAlarms;
#endif
    
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

public:
    ConfigManagerRuntime();
    ~ConfigManagerRuntime();
    
    // Initialization
    void begin(ConfigManagerClass* cm);
    void setLogCallback(LogCallback logger);
    
    // Runtime value providers
    void addRuntimeProvider(const RuntimeValueProvider& provider);
    void addRuntimeProvider(const String& name, std::function<void(JsonObject&)> fillFunc, int order = 100);
    
    // Runtime metadata
    void addRuntimeMeta(const RuntimeFieldMeta& meta);
    RuntimeFieldMeta* findRuntimeMeta(const String& group, const String& key);
    
    // JSON generation
    String runtimeValuesToJSON();
    String runtimeMetaToJSON();
    
    // System provider
#if CM_ENABLE_SYSTEM_PROVIDER
    void enableBuiltinSystemProvider();
    void updateLoopTiming();
    double getLoopAverage() const { return loopAvgMs; }
#else
    void enableBuiltinSystemProvider() {}
    void updateLoopTiming() {}
    double getLoopAverage() const { return 0.0; }
#endif
    
    // Alarms
#if CM_ENABLE_RUNTIME_ALARMS
    void addRuntimeAlarm(const String& name, std::function<bool()> checkFunction);
    void addRuntimeAlarm(const String& name, std::function<bool()> checkFunction, 
                         std::function<void()> onTrigger, std::function<void()> onClear = nullptr);
    void updateAlarms();
    bool hasActiveAlarms() const;
    std::vector<String> getActiveAlarms() const;
#else
    void addRuntimeAlarm(const String&, std::function<bool()>) {}
    void addRuntimeAlarm(const String&, std::function<bool()>, std::function<void()>, std::function<void()> = nullptr) {}
    void updateAlarms() {}
    bool hasActiveAlarms() const { return false; }
    std::vector<String> getActiveAlarms() const { return {}; }
#endif
    
    // Development support
#ifdef development
    std::vector<RuntimeFieldMeta> runtimeMetaOverride;
    bool runtimeMetaOverrideActive = false;
    void setRuntimeMetaOverride(const std::vector<RuntimeFieldMeta>& override);
    void clearRuntimeMetaOverride();
#endif

    // Interactive runtime controls
#if CM_ENABLE_RUNTIME_BUTTONS
    void defineRuntimeButton(const String& group, const String& key, const String& label, 
                           std::function<void()> onPress, const String& card = String(), int order = 100);
    void handleButtonPress(const String& group, const String& key);
#else
    void defineRuntimeButton(const String&, const String&, const String&, std::function<void()>, const String& = String(), int = 100) {}
    void handleButtonPress(const String&, const String&) {}
#endif

#if CM_ENABLE_RUNTIME_CHECKBOXES
    void defineRuntimeCheckbox(const String& group, const String& key, const String& label,
                             std::function<bool()> getter, std::function<void(bool)> setter,
                             const String& card = String(), int order = 100);
    void handleCheckboxChange(const String& group, const String& key, bool value);
#else
    void defineRuntimeCheckbox(const String&, const String&, const String&, std::function<bool()>, std::function<void(bool)>, const String& = String(), int = 100) {}
    void handleCheckboxChange(const String&, const String&, bool) {}
#endif

#if CM_ENABLE_RUNTIME_STATE_BUTTONS
    void defineRuntimeStateButton(const String& group, const String& key, const String& label,
                                std::function<bool()> getter, std::function<void(bool)> setter,
                                bool initState = false, const String& card = String(), int order = 100);
    void handleStateButtonToggle(const String& group, const String& key);
#else
    void defineRuntimeStateButton(const String&, const String&, const String&, std::function<bool()>, std::function<void(bool)>, bool = false, const String& = String(), int = 100) {}
    void handleStateButtonToggle(const String&, const String&) {}
#endif

#if (CM_ENABLE_RUNTIME_INT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
    void defineRuntimeIntSlider(const String& group, const String& key, const String& label,
                              int minValue, int maxValue, int initValue,
                              std::function<int()> getter, std::function<void(int)> setter,
                              const String& unit = String(), const String& card = String(), int order = 100);
    void handleIntSliderChange(const String& group, const String& key, int value);
#else
    void defineRuntimeIntSlider(const String&, const String&, const String&, int, int, int, std::function<int()>, std::function<void(int)>, const String& = String(), const String& = String(), int = 100) {}
    void handleIntSliderChange(const String&, const String&, int) {}
#endif

#if (CM_ENABLE_RUNTIME_FLOAT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
    void defineRuntimeFloatSlider(const String& group, const String& key, const String& label,
                                float minValue, float maxValue, float initValue, int precision,
                                std::function<float()> getter, std::function<void(float)> setter,
                                const String& unit = String(), const String& card = String(), int order = 100);
    void handleFloatSliderChange(const String& group, const String& key, float value);
#else
    void defineRuntimeFloatSlider(const String&, const String&, const String&, float, float, float, int, std::function<float()>, std::function<void(float)>, const String& = String(), const String& = String(), int = 100) {}
    void handleFloatSliderChange(const String&, const String&, float) {}
#endif
};