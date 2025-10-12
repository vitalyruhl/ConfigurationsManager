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
};

#if CM_ENABLE_RUNTIME_ALARMS
struct RuntimeAlarm {
    String name;
    bool active = false;
    std::function<bool()> checkFunction;
    
    RuntimeAlarm(const String& n, std::function<bool()> check)
        : name(n), checkFunction(check) {}
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
    void updateAlarms();
    bool hasActiveAlarms() const;
    std::vector<String> getActiveAlarms() const;
#else
    void addRuntimeAlarm(const String&, std::function<bool()>) {}
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
};