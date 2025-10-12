#include "RuntimeManager.h"
#include "../ConfigManager.h"
#include <algorithm>

// Logging support
#if CM_ENABLE_LOGGING
extern std::function<void(const char*)> ConfigManagerClass_logger;
#define RUNTIME_LOG(...) if(ConfigManagerClass_logger) { char buf[256]; snprintf(buf, sizeof(buf), __VA_ARGS__); ConfigManagerClass_logger(buf); }
#else
#define RUNTIME_LOG(...)
#endif

ConfigManagerRuntime::ConfigManagerRuntime()
    : configManager(nullptr)
#if CM_ENABLE_SYSTEM_PROVIDER
    , builtinSystemProviderEnabled(false)
    , builtinSystemProviderRegistered(false)
    , loopWindowStart(0)
    , loopSamples(0)
    , loopAccumMs(0.0)
    , loopAvgMs(0.0)
#endif
{
}

ConfigManagerRuntime::~ConfigManagerRuntime() {
}

void ConfigManagerRuntime::begin(ConfigManagerClass* cm) {
    configManager = cm;
    RUNTIME_LOG("[Runtime] Runtime manager initialized");
}

void ConfigManagerRuntime::setLogCallback(LogCallback logger) {
    logCallback = logger;
}

void ConfigManagerRuntime::addRuntimeProvider(const RuntimeValueProvider& provider) {
    runtimeProviders.push_back(provider);
    RUNTIME_LOG("[Runtime] Added provider: %s (order: %d)", provider.name.c_str(), provider.order);
}

void ConfigManagerRuntime::addRuntimeProvider(const String& name, std::function<void(JsonObject&)> fillFunc, int order) {
    addRuntimeProvider(RuntimeValueProvider(name, fillFunc, order));
}

void ConfigManagerRuntime::addRuntimeMeta(const RuntimeFieldMeta& meta) {
    runtimeMeta.push_back(meta);
    RUNTIME_LOG("[Runtime] Added meta: %s.%s", meta.group.c_str(), meta.key.c_str());
}

RuntimeFieldMeta* ConfigManagerRuntime::findRuntimeMeta(const String& group, const String& key) {
    for (auto& meta : runtimeMeta) {
        if (meta.group == group && meta.key == key) {
            return &meta;
        }
    }
    return nullptr;
}

void ConfigManagerRuntime::sortProviders() {
    std::sort(runtimeProviders.begin(), runtimeProviders.end(), 
        [](const RuntimeValueProvider& a, const RuntimeValueProvider& b) {
            return a.order < b.order;
        });
}

void ConfigManagerRuntime::sortMeta() {
    std::sort(runtimeMeta.begin(), runtimeMeta.end(), 
        [](const RuntimeFieldMeta& a, const RuntimeFieldMeta& b) {
            if (a.group == b.group) {
                if (a.order == b.order) return a.label < b.label;
                return a.order < b.order;
            }
            return a.group < b.group;
        });
}

String ConfigManagerRuntime::runtimeValuesToJSON() {
    DynamicJsonDocument d(2048);
    JsonObject root = d.to<JsonObject>();
    root["uptime"] = millis();
    
    // Sort providers by order
    sortProviders();
    
    for (auto& prov : runtimeProviders) {
        JsonObject slot = root.createNestedObject(prov.name);
        if (prov.fill) {
            prov.fill(slot);
        }
        
        // Add interactive control states for this provider/group
#if CM_ENABLE_RUNTIME_CHECKBOXES
        for (auto& checkbox : runtimeCheckboxes) {
            if (checkbox.group == prov.name && checkbox.getter) {
                slot[checkbox.key] = checkbox.getter();
            }
        }
#endif

#if CM_ENABLE_RUNTIME_STATE_BUTTONS
        for (auto& button : runtimeStateButtons) {
            if (button.group == prov.name && button.getter) {
                slot[button.key] = button.getter();
            }
        }
#endif

#if (CM_ENABLE_RUNTIME_INT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
        for (auto& slider : runtimeIntSliders) {
            if (slider.group == prov.name && slider.getter) {
                slot[slider.key] = slider.getter();
            }
        }
#endif

#if (CM_ENABLE_RUNTIME_FLOAT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
        for (auto& slider : runtimeFloatSliders) {
            if (slider.group == prov.name && slider.getter) {
                slot[slider.key] = slider.getter();
            }
        }
#endif
    }
    
#if CM_ENABLE_RUNTIME_ALARMS
    if (!runtimeAlarms.empty()) {
        JsonObject alarms = root.createNestedObject("alarms");
        for (auto& a : runtimeAlarms) {
            alarms[a.name] = a.active;
        }
    }
#endif
    
    String out;
    serializeJson(d, out);
    return out;
}

String ConfigManagerRuntime::runtimeMetaToJSON() {
#if !CM_ENABLE_RUNTIME_META
    return "[]";
#else
    DynamicJsonDocument d(4096);
    JsonArray arr = d.to<JsonArray>();
    
    // Sort meta by group, then order, then label
    std::vector<RuntimeFieldMeta> metaSorted;
#ifdef development
    if (runtimeMetaOverrideActive) {
        metaSorted = runtimeMetaOverride;
    } else {
        metaSorted = runtimeMeta;
    }
#else
    metaSorted = runtimeMeta;
#endif
    
    std::sort(metaSorted.begin(), metaSorted.end(), 
        [](const RuntimeFieldMeta& a, const RuntimeFieldMeta& b) {
            if (a.group == b.group) {
                if (a.order == b.order) return a.label < b.label;
                return a.order < b.order;
            }
            return a.group < b.group;
        });
    
    for (auto& m : metaSorted) {
        JsonObject o = arr.createNestedObject();
        o["group"] = m.group;
        o["key"] = m.key;
        o["label"] = m.label;
        if (m.unit.length()) {
            o["unit"] = m.unit;
        }
        o["precision"] = m.precision;
        if (m.isBool) o["isBool"] = true;
        if (m.isButton) o["isButton"] = true;
        if (m.isCheckbox) o["isCheckbox"] = true;
        if (m.isStateButton) o["isStateButton"] = true;
        if (m.isIntSlider) o["isIntSlider"] = true;
        if (m.isFloatSlider) o["isFloatSlider"] = true;
        if (m.hasAlarm) o["hasAlarm"] = true;
        if (m.alarmWhenTrue) o["alarmWhenTrue"] = true;
        if (m.boolAlarmValue != false) {  // Only include if not default false
            o["boolAlarmValue"] = m.boolAlarmValue;
        }
        if (m.alarmMin != 0.0f || m.alarmMax != 0.0f) {
            o["alarmMin"] = m.alarmMin;
            o["alarmMax"] = m.alarmMax;
        }
        if (m.warnMin != 0.0f || m.warnMax != 0.0f) {
            o["warnMin"] = m.warnMin;
            o["warnMax"] = m.warnMax;
        }
        o["order"] = m.order;
    }
    
    String out;
    serializeJson(d, out);
    return out;
#endif
}

#if CM_ENABLE_SYSTEM_PROVIDER

void ConfigManagerRuntime::enableBuiltinSystemProvider() {
    if (builtinSystemProviderRegistered) return;
    
    addRuntimeProvider("system", [this](JsonObject& obj) {
        obj["freeHeap"] = ESP.getFreeHeap();
        obj["totalHeap"] = ESP.getHeapSize();
        obj["usedHeap"] = ESP.getHeapSize() - ESP.getFreeHeap();
        obj["heapFragmentation"] = 100 - (ESP.getMaxAllocHeap() * 100) / ESP.getFreeHeap();
        
        if (WiFi.status() == WL_CONNECTED) {
            obj["rssi"] = WiFi.RSSI();
            obj["wifiConnected"] = true;
        } else {
            obj["wifiConnected"] = false;
        }
        
        obj["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        obj["flashSize"] = ESP.getFlashChipSize();
        obj["sketchSize"] = ESP.getSketchSize();
        obj["freeSketchSpace"] = ESP.getFreeSketchSpace();
        
        if (loopSamples > 0) {
            obj["loopAvg"] = loopAvgMs;
        }
        
        // OTA status would need to be injected from OTA manager
        // obj["otaActive"] = false; // This should come from OTA manager
    }, 0);
    
    builtinSystemProviderRegistered = true;
    builtinSystemProviderEnabled = true;
    RUNTIME_LOG("[Runtime] Built-in system provider enabled");
}

void ConfigManagerRuntime::updateLoopTiming() {
    static unsigned long lastLoopTime = 0;
    unsigned long now = millis();
    
    if (lastLoopTime > 0) {
        double deltaMs = (now - lastLoopTime);
        loopAccumMs += deltaMs;
        loopSamples++;
        
        // Reset window every 10 seconds
        if (loopWindowStart == 0) {
            loopWindowStart = now;
        } else if (now - loopWindowStart >= 10000) {
            if (loopSamples > 0) {
                loopAvgMs = loopAccumMs / loopSamples;
            }
            loopAccumMs = 0.0;
            loopSamples = 0;
            loopWindowStart = now;
        }
    }
    
    lastLoopTime = now;
}

#endif // CM_ENABLE_SYSTEM_PROVIDER

#if CM_ENABLE_RUNTIME_ALARMS

void ConfigManagerRuntime::addRuntimeAlarm(const String& name, std::function<bool()> checkFunction) {
    runtimeAlarms.emplace_back(name, checkFunction);
    RUNTIME_LOG("[Runtime] Added alarm: %s", name.c_str());
}

void ConfigManagerRuntime::addRuntimeAlarm(const String& name, std::function<bool()> checkFunction, 
                                          std::function<void()> onTrigger, std::function<void()> onClear) {
    runtimeAlarms.emplace_back(name, checkFunction, onTrigger, onClear);
    RUNTIME_LOG("[Runtime] Added alarm with triggers: %s", name.c_str());
}

void ConfigManagerRuntime::updateAlarms() {
    for (auto& alarm : runtimeAlarms) {
        if (alarm.checkFunction) {
            bool newState = alarm.checkFunction();
            if (newState != alarm.active) {
                alarm.active = newState;
                RUNTIME_LOG("[Runtime] Alarm %s: %s", alarm.name.c_str(), newState ? "ACTIVE" : "cleared");
                
                // Call trigger callbacks
                if (newState && alarm.onTrigger) {
                    RUNTIME_LOG("[Runtime] Calling onTrigger for alarm: %s", alarm.name.c_str());
                    alarm.onTrigger();
                } else if (!newState && alarm.onClear) {
                    RUNTIME_LOG("[Runtime] Calling onClear for alarm: %s", alarm.name.c_str());
                    alarm.onClear();
                }
            }
        }
    }
}

bool ConfigManagerRuntime::hasActiveAlarms() const {
    for (const auto& alarm : runtimeAlarms) {
        if (alarm.active) return true;
    }
    return false;
}

std::vector<String> ConfigManagerRuntime::getActiveAlarms() const {
    std::vector<String> active;
    for (const auto& alarm : runtimeAlarms) {
        if (alarm.active) {
            active.push_back(alarm.name);
        }
    }
    return active;
}

#endif // CM_ENABLE_RUNTIME_ALARMS

#ifdef development

void ConfigManagerRuntime::setRuntimeMetaOverride(const std::vector<RuntimeFieldMeta>& override) {
    runtimeMetaOverride = override;
    runtimeMetaOverrideActive = true;
    RUNTIME_LOG("[Runtime] Meta override set (%d entries)", override.size());
}

void ConfigManagerRuntime::clearRuntimeMetaOverride() {
    runtimeMetaOverride.clear();
    runtimeMetaOverrideActive = false;
    RUNTIME_LOG("[Runtime] Meta override cleared");
}

#endif

void ConfigManagerRuntime::log(const char* format, ...) const {
#if CM_ENABLE_LOGGING
    if (logCallback) {
        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        logCallback(buffer);
    }
#endif
}

// Interactive runtime control implementations

#if CM_ENABLE_RUNTIME_BUTTONS
void ConfigManagerRuntime::defineRuntimeButton(const String& group, const String& key, const String& label, 
                                              std::function<void()> onPress, const String& card, int order) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isButton = true;
    meta.order = order;
    meta.card = card;
    addRuntimeMeta(meta);
    
    runtimeButtons.emplace_back(group, key, onPress);
    RUNTIME_LOG("[Runtime] Added button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleButtonPress(const String& group, const String& key) {
    for (auto& button : runtimeButtons) {
        if (button.group == group && button.key == key) {
            if (button.onPress) {
                button.onPress();
                RUNTIME_LOG("[Runtime] Button pressed: %s.%s", group.c_str(), key.c_str());
            }
            return;
        }
    }
    RUNTIME_LOG("[Runtime] Button not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if CM_ENABLE_RUNTIME_CHECKBOXES
void ConfigManagerRuntime::defineRuntimeCheckbox(const String& group, const String& key, const String& label,
                                                std::function<bool()> getter, std::function<void(bool)> setter,
                                                const String& card, int order) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isCheckbox = true;
    meta.order = order;
    meta.card = card;
    addRuntimeMeta(meta);
    
    runtimeCheckboxes.emplace_back(group, key, getter, setter);
    RUNTIME_LOG("[Runtime] Added checkbox: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleCheckboxChange(const String& group, const String& key, bool value) {
    for (auto& checkbox : runtimeCheckboxes) {
        if (checkbox.group == group && checkbox.key == key) {
            if (checkbox.setter) {
                checkbox.setter(value);
                RUNTIME_LOG("[Runtime] Checkbox changed: %s.%s = %s", group.c_str(), key.c_str(), value ? "true" : "false");
            }
            return;
        }
    }
    RUNTIME_LOG("[Runtime] Checkbox not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if CM_ENABLE_RUNTIME_STATE_BUTTONS
void ConfigManagerRuntime::defineRuntimeStateButton(const String& group, const String& key, const String& label,
                                                   std::function<bool()> getter, std::function<void(bool)> setter,
                                                   bool initState, const String& card, int order) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isStateButton = true;
    meta.initialState = initState;
    meta.order = order;
    meta.card = card;
    addRuntimeMeta(meta);
    
    runtimeStateButtons.emplace_back(group, key, getter, setter);
    RUNTIME_LOG("[Runtime] Added state button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleStateButtonToggle(const String& group, const String& key) {
    for (auto& button : runtimeStateButtons) {
        if (button.group == group && button.key == key) {
            if (button.getter && button.setter) {
                bool currentState = button.getter();
                bool newState = !currentState;
                button.setter(newState);
                RUNTIME_LOG("[Runtime] State button toggled: %s.%s = %s", group.c_str(), key.c_str(), newState ? "true" : "false");
            }
            return;
        }
    }
    RUNTIME_LOG("[Runtime] State button not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if (CM_ENABLE_RUNTIME_INT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
void ConfigManagerRuntime::defineRuntimeIntSlider(const String& group, const String& key, const String& label,
                                                 int minValue, int maxValue, int initValue,
                                                 std::function<int()> getter, std::function<void(int)> setter,
                                                 const String& unit, const String& card, int order) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isIntSlider = true;
    meta.intMin = minValue;
    meta.intMax = maxValue;
    meta.intInit = initValue;
    meta.unit = unit;
    meta.order = order;
    meta.card = card;
    addRuntimeMeta(meta);
    
    runtimeIntSliders.emplace_back(group, key, getter, setter, minValue, maxValue);
    RUNTIME_LOG("[Runtime] Added int slider: %s.%s [%d-%d]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleIntSliderChange(const String& group, const String& key, int value) {
    for (auto& slider : runtimeIntSliders) {
        if (slider.group == group && slider.key == key) {
            if (slider.setter) {
                // Clamp value to range
                int clampedValue = max(slider.minV, min(slider.maxV, value));
                slider.setter(clampedValue);
                RUNTIME_LOG("[Runtime] Int slider changed: %s.%s = %d", group.c_str(), key.c_str(), clampedValue);
            }
            return;
        }
    }
    RUNTIME_LOG("[Runtime] Int slider not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if (CM_ENABLE_RUNTIME_FLOAT_SLIDERS || CM_ENABLE_RUNTIME_ANALOG_SLIDERS)
void ConfigManagerRuntime::defineRuntimeFloatSlider(const String& group, const String& key, const String& label,
                                                   float minValue, float maxValue, float initValue, int precision,
                                                   std::function<float()> getter, std::function<void(float)> setter,
                                                   const String& unit, const String& card, int order) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isFloatSlider = true;
    meta.floatMin = minValue;
    meta.floatMax = maxValue;
    meta.floatInit = initValue;
    meta.precision = precision;
    meta.floatPrecision = precision;
    meta.unit = unit;
    meta.order = order;
    meta.card = card;
    addRuntimeMeta(meta);
    
    runtimeFloatSliders.emplace_back(group, key, getter, setter, minValue, maxValue);
    RUNTIME_LOG("[Runtime] Added float slider: %s.%s [%.2f-%.2f]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleFloatSliderChange(const String& group, const String& key, float value) {
    for (auto& slider : runtimeFloatSliders) {
        if (slider.group == group && slider.key == key) {
            if (slider.setter) {
                // Clamp value to range
                float clampedValue = max(slider.minV, min(slider.maxV, value));
                slider.setter(clampedValue);
                RUNTIME_LOG("[Runtime] Float slider changed: %s.%s = %.2f", group.c_str(), key.c_str(), clampedValue);
            }
            return;
        }
    }
    RUNTIME_LOG("[Runtime] Float slider not found: %s.%s", group.c_str(), key.c_str());
}
#endif