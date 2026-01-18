#include "RuntimeManager.h"
#include "../ConfigManager.h"
#include <algorithm>
#include <utility>
#include <time.h>

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
    RUNTIME_LOG("[RT] Runtime manager initialized");
}

void ConfigManagerRuntime::setLogCallback(LogCallback logger) {
    logCallback = logger;
}

void ConfigManagerRuntime::addRuntimeProvider(const RuntimeValueProvider& provider) {
    runtimeProviders.push_back(provider);
    RUNTIME_LOG("[RT] Added provider: %s (order: %d)", provider.name.c_str(), provider.order);
}

void ConfigManagerRuntime::addRuntimeProvider(const String& name, std::function<void(JsonObject&)> fillFunc, int order) {
    addRuntimeProvider(RuntimeValueProvider(name, fillFunc, order));
}

void ConfigManagerRuntime::addRuntimeMeta(const RuntimeFieldMeta& meta) {
    runtimeMeta.push_back(meta);
    RUNTIME_LOG("[RT] Added meta: %s.%s", meta.group.c_str(), meta.key.c_str());
}

RuntimeFieldMeta* ConfigManagerRuntime::findRuntimeMeta(const String& group, const String& key) {
    for (auto& meta : runtimeMeta) {
        if (meta.group == group && meta.key == key) {
            return &meta;
        }
    }
    return nullptr;
}

#if CM_ENABLE_RUNTIME_ALARMS
RuntimeAlarm* ConfigManagerRuntime::findAlarm(const String& name) {
    for (auto& alarm : runtimeAlarms) {
        if (alarm.name == name) {
            return &alarm;
        }
    }
    return nullptr;
}

const RuntimeAlarm* ConfigManagerRuntime::findAlarm(const String& name) const {
    for (const auto& alarm : runtimeAlarms) {
        if (alarm.name == name) {
            return &alarm;
        }
    }
    return nullptr;
}
#endif

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
        JsonObject slot;
        JsonVariant existing = root[prov.name];
        if (existing.isNull()) {
            slot = root.createNestedObject(prov.name);
        } else if (existing.is<JsonObject>()) {
            slot = existing.as<JsonObject>();
        } else {
            root.remove(prov.name);
            slot = root.createNestedObject(prov.name);
        }
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

#if CM_ENABLE_RUNTIME_ANALOG_SLIDERS
        for (auto& slider : runtimeIntSliders) {
            if (slider.group == prov.name && slider.getter) {
                slot[slider.key] = slider.getter();
            }
        }
#endif

#if CM_ENABLE_RUNTIME_ANALOG_SLIDERS
        for (auto& slider : runtimeFloatSliders) {
            if (slider.group == prov.name && slider.getter) {
                slot[slider.key] = slider.getter();
            }
        }
#endif

#if CM_ENABLE_RUNTIME_NUMBER_INPUTS
        for (auto& input : runtimeIntInputs) {
            if (input.group == prov.name && input.getter) {
                slot[input.key] = input.getter();
            }
        }

        for (auto& input : runtimeFloatInputs) {
            if (input.group == prov.name && input.getter) {
                slot[input.key] = input.getter();
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
        if (m.isString) o["isString"] = true;
        if (m.isDivider) o["isDivider"] = true;
        if (m.isButton) o["isButton"] = true;
        if (m.isCheckbox) o["isCheckbox"] = true;
        if (m.isStateButton) o["isStateButton"] = true;
        if (m.isMomentaryButton) o["isMomentaryButton"] = true;
        if (m.isIntSlider) {
            o["isIntSlider"] = true;
            o["min"] = m.intMin;
            o["max"] = m.intMax;
            o["init"] = m.intInit;
        }
        if (m.isFloatSlider) {
            o["isFloatSlider"] = true;
            o["min"] = m.floatMin;
            o["max"] = m.floatMax;
            o["init"] = m.floatInit;
        }
        if (m.isIntInput) {
            o["isIntInput"] = true;
            o["min"] = m.intMin;
            o["max"] = m.intMax;
            o["init"] = m.intInit;
        }
        if (m.isFloatInput) {
            o["isFloatInput"] = true;
            o["min"] = m.floatMin;
            o["max"] = m.floatMax;
            o["init"] = m.floatInit;
        }
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
        if (m.staticValue.length()) {
            o["staticValue"] = m.staticValue;
        }
        if (m.onLabel.length()) {
            o["onLabel"] = m.onLabel;
        }
        if (m.offLabel.length()) {
            o["offLabel"] = m.offLabel;
        }
    }

    String out;
    serializeJson(d, out);
    return out;
#endif
}

#if CM_ENABLE_RUNTIME_NUMBER_INPUTS
void ConfigManagerRuntime::defineRuntimeIntValue(const String& group, const String& key, const String& label,
                                                 int minValue, int maxValue, int initValue,
                                                 std::function<int()> getter, std::function<void(int)> setter,
                                                 const String& unit, const String& card, int order) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isIntInput = true;
    meta.intMin = minValue;
    meta.intMax = maxValue;
    meta.intInit = initValue;
    meta.unit = unit;
    meta.order = order;
    meta.card = card;
    addRuntimeMeta(meta);

    runtimeIntInputs.emplace_back(group, key, getter, setter, minValue, maxValue);
    RUNTIME_LOG("[RT] Added int input: %s.%s [%d-%d]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleIntInputChange(const String& group, const String& key, int value) {
    for (auto& input : runtimeIntInputs) {
        if (input.group == group && input.key == key) {
            if (input.setter) {
                int clampedValue = max(input.minV, min(input.maxV, value));
                input.setter(clampedValue);
                RUNTIME_LOG("[RT] Int input changed: %s.%s = %d", group.c_str(), key.c_str(), clampedValue);
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] Int input not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeFloatValue(const String& group, const String& key, const String& label,
                                                   float minValue, float maxValue, float initValue, int precision,
                                                   std::function<float()> getter, std::function<void(float)> setter,
                                                   const String& unit, const String& card, int order) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isFloatInput = true;
    meta.floatMin = minValue;
    meta.floatMax = maxValue;
    meta.floatInit = initValue;
    meta.precision = precision;
    meta.floatPrecision = precision;
    meta.unit = unit;
    meta.order = order;
    meta.card = card;
    addRuntimeMeta(meta);

    runtimeFloatInputs.emplace_back(group, key, getter, setter, minValue, maxValue);
    RUNTIME_LOG("[RT] Added float input: %s.%s [%.2f-%.2f]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleFloatInputChange(const String& group, const String& key, float value) {
    for (auto& input : runtimeFloatInputs) {
        if (input.group == group && input.key == key) {
            if (input.setter) {
                float clampedValue = max(input.minV, min(input.maxV, value));
                input.setter(clampedValue);
                RUNTIME_LOG("[RT] Float input changed: %s.%s = %.2f", group.c_str(), key.c_str(), clampedValue);
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] Float input not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if CM_ENABLE_SYSTEM_PROVIDER

// File-scope cache to avoid repeated bootloader_mmap calls
size_t _cm_cachedSketchSize = 0;
size_t _cm_cachedFreeSketch = 0;
bool _cm_sketchInfoInitialized = false;

// Simple RSSI-to-text quality mapper
static const char* rssiQualityText(int rssi) {
    // Typical RSSI range: -30 (excellent) to -90 (very weak)
    if (rssi >= -50) return "excellent";   // Excellent
    if (rssi >= -60) return "good";        // Good
    if (rssi >= -67) return "ok";          // Fair/OK
    if (rssi >= -75) return "weak";        // Weak
    return "very weak";                     // Very weak / none
}

void ConfigManagerRuntime::enableBuiltinSystemProvider() {
    if (builtinSystemProviderRegistered) return;

    addRuntimeProvider("system", [this](JsonObject& obj) {
        obj["freeHeap"] = ESP.getFreeHeap();
        obj["totalHeap"] = ESP.getHeapSize();
        obj["usedHeap"] = ESP.getHeapSize() - ESP.getFreeHeap();
        obj["heapFragmentation"] = 100 - (ESP.getMaxAllocHeap() * 100) / ESP.getFreeHeap();

        if (WiFi.status() == WL_CONNECTED) {
            int rssi = WiFi.RSSI();
            obj["rssi"] = rssi;
            obj["rssiTxt"] = rssiQualityText(rssi);
            obj["wifiConnected"] = true;
            obj["localIP"] = WiFi.localIP().toString();
            obj["gateway"] = WiFi.gatewayIP().toString();
            obj["routerMAC"] = WiFi.BSSIDstr();
            obj["channel"] = WiFi.channel();
        } else {
            // When disconnected, expose neutral values
            obj["rssi"] = 0;
            obj["rssiTxt"] = rssiQualityText(-100);
            obj["wifiConnected"] = false;
            obj["localIP"] = "0.0.0.0";
            obj["gateway"] = "0.0.0.0";
            obj["routerMAC"] = "00:00:00:00:00:00";
            obj["channel"] = 0;
        }

        obj["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        obj["flashSize"] = ESP.getFlashChipSize();

        // Avoid repeated bootloader_mmap calls (not reentrant) by caching sketch metrics
        extern size_t _cm_cachedSketchSize;
        extern size_t _cm_cachedFreeSketch;
        extern bool _cm_sketchInfoInitialized;
        if (!_cm_sketchInfoInitialized) {
            _cm_cachedSketchSize = ESP.getSketchSize();
            _cm_cachedFreeSketch = ESP.getFreeSketchSpace();
            _cm_sketchInfoInitialized = true;
        }
        obj["sketchSize"] = _cm_cachedSketchSize;
        obj["freeSketchSpace"] = _cm_cachedFreeSketch;

        if (loopSamples > 0) {
            obj["loopAvg"] = loopAvgMs;
        }

#if CM_ENABLE_SYSTEM_TIME
        // Provide current local date-time in ISO 8601 without timezone suffix
        time_t now = time(nullptr);
        if (now > 1600000000) { // sanity check (~2020-09-13)
            struct tm tmnow;
            localtime_r(&now, &tmnow);
            char buf[24];
            // Format: YYYY-MM-DDTHH:MM:SS
            strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmnow);
            obj["dateTime"] = buf;
        }
#endif

        // OTA status exposed to GUI
        if (configManager) {
            auto &ota = configManager->getOTAManager();
            // obj["allowOTA"] = ota.isEnabled();
            obj["otaActive"] = ota.isActive();
        }
    }, 0);

    // Provide basic meta so UI can display the RSSI and its quality text
    {
        // Helper to upsert meta with a specific order
        auto upsertMeta = [this](const String& key, const String& label, const String& unit, int order,
                                 bool isBool = false, bool isString = false, int precision = 0) {
            RuntimeFieldMeta* existing = findRuntimeMeta("system", key);
            if (existing) {
                existing->order = order;
                if (label.length()) existing->label = label;
                if (unit.length() && existing->unit.length() == 0) existing->unit = unit;
                if (isBool) existing->isBool = true;
                if (isString) existing->isString = true;
                if (precision >= 0) existing->precision = precision;
                return;
            }
            RuntimeFieldMeta m;
            m.group = "system";
            m.key = key;
            m.label = label.length() ? label : key;
            m.unit = unit;
            m.order = order;
            m.isBool = isBool;
            m.isString = isString;
            m.precision = precision;
            addRuntimeMeta(m);
        };

        // Orders 0-2 are used by app_name/app_version/build_date defined in main.cpp
        upsertMeta("wifiConnected", "Wifi Connected", "", 3, true);
        upsertMeta("rssi", "WiFi RSSI", "dBm", 4, false, false, 0);
        upsertMeta("rssiTxt", "Signal", "", 5, false, true, 0);
        upsertMeta("localIP", "Local IP", "", 6, false, true, 0);
        upsertMeta("gateway", "Gateway", "", 7, false, true, 0);
        upsertMeta("routerMAC", "Router MAC", "", 8, false, true, 0);
        upsertMeta("channel", "WiFi Channel", "", 9, false, false, 0);

        // Connectivity and OTA state (booleans)
        upsertMeta("allowOTA", "AllowOTA", "", 10, true);
        upsertMeta("otaActive", "OtaActive", "", 11, true);

#if CM_ENABLE_SYSTEM_TIME
        upsertMeta("dateTime", "Date/Time", "", 12, false, true, 0);
#endif

        // System numeric stats
        // todo: make heap settable to show or not....
        upsertMeta("cpuFreqMHz", "CpuFreqMHz", "", 20, false, false, 0);
        upsertMeta("flashSize", "FlashSize", "", 21, false, false, 0);
        upsertMeta("sketchSize", "SketchSize", "", 22, false, false, 0);
        upsertMeta("freeSketchSpace", "FreeSketchSpace", "", 23, false, false, 0);
        upsertMeta("heapFragmentation", "HeapFragmentation", "", 24, false, false, 0);
        upsertMeta("totalHeap", "TotalHeap", "", 30, false, false, 0);
        upsertMeta("usedHeap", "UsedHeap", "", 31, false, false, 0);
        // Keep unit empty to avoid conflicting with pre-existing meta; value is in bytes currently
        upsertMeta("freeHeap", "FreeHeap", "", 32, false, false, 0);
    }

    builtinSystemProviderRegistered = true;
    builtinSystemProviderEnabled = true;
    RUNTIME_LOG("[RT] Built-in system provider enabled");
}

void ConfigManagerRuntime::refreshSketchInfoCache() {
#if CM_ENABLE_SYSTEM_PROVIDER
    _cm_cachedSketchSize = ESP.getSketchSize();
    _cm_cachedFreeSketch = ESP.getFreeSketchSpace();
    _cm_sketchInfoInitialized = true;
#endif
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
    RuntimeAlarm alarm;
    alarm.name = name;
    alarm.checkFunction = checkFunction;
    alarm.manual = false;
    runtimeAlarms.push_back(std::move(alarm));
    RUNTIME_LOG("[RT] Added alarm: %s", name.c_str());
}

void ConfigManagerRuntime::addRuntimeAlarm(const String& name, std::function<bool()> checkFunction,
                                          std::function<void()> onTrigger, std::function<void()> onClear) {
    RuntimeAlarm alarm;
    alarm.name = name;
    alarm.checkFunction = checkFunction;
    alarm.onTrigger = onTrigger;
    alarm.onClear = onClear;
    alarm.manual = false;
    runtimeAlarms.push_back(std::move(alarm));
    RUNTIME_LOG("[RT] Added alarm with triggers: %s", name.c_str());
}

void ConfigManagerRuntime::registerRuntimeAlarm(const String& name, std::function<void()> onTrigger, std::function<void()> onClear) {
    RuntimeAlarm* alarm = findAlarm(name);
    if (alarm) {
        alarm->manual = true;
        alarm->checkFunction = nullptr;
        if (onTrigger) alarm->onTrigger = onTrigger;
        if (onClear) alarm->onClear = onClear;
        RUNTIME_LOG("[RT] Updated manual alarm registration: %s", name.c_str());
        return;
    }

    RuntimeAlarm newAlarm;
    newAlarm.name = name;
    newAlarm.manual = true;
    newAlarm.onTrigger = onTrigger;
    newAlarm.onClear = onClear;
    runtimeAlarms.push_back(std::move(newAlarm));
    RUNTIME_LOG("[RT] Registered manual alarm: %s", name.c_str());
}

void ConfigManagerRuntime::setRuntimeAlarmActive(const String& name, bool active, bool fireCallbacks) {
    RuntimeAlarm* alarm = findAlarm(name);
    if (!alarm) {
        RuntimeAlarm newAlarm;
        newAlarm.name = name;
        newAlarm.manual = true;
        newAlarm.active = active;
        runtimeAlarms.push_back(std::move(newAlarm));
        alarm = &runtimeAlarms.back();
        RUNTIME_LOG("[RT] Lazily created manual alarm entry: %s", name.c_str());

        if (fireCallbacks && alarm->active && alarm->onTrigger) {
            RUNTIME_LOG("[RT] Manual trigger callback for alarm: %s", name.c_str());
            alarm->onTrigger();
        }
        return;
    }

    alarm->manual = true;

    if (alarm->active == active) {
        return;
    }

    alarm->active = active;
    RUNTIME_LOG("[RT] Alarm %s manually set to %s", name.c_str(), active ? "ACTIVE" : "cleared");

    if (!fireCallbacks) {
        return;
    }

    if (active) {
        if (alarm->onTrigger) {
            RUNTIME_LOG("[RT] Manual trigger callback for alarm: %s", name.c_str());
            alarm->onTrigger();
        }
    } else {
        if (alarm->onClear) {
            RUNTIME_LOG("[RT] Manual clear callback for alarm: %s", name.c_str());
            alarm->onClear();
        }
    }
}

bool ConfigManagerRuntime::isRuntimeAlarmActive(const String& name) const {
    const RuntimeAlarm* alarm = findAlarm(name);
    return alarm ? alarm->active : false;
}

void ConfigManagerRuntime::updateAlarms() {
    for (auto& alarm : runtimeAlarms) {
        if (alarm.manual) {
            continue;
        }

        if (alarm.checkFunction) {
            bool newState = alarm.checkFunction();
            if (newState != alarm.active) {
                alarm.active = newState;
                RUNTIME_LOG("[RT] Alarm %s: %s", alarm.name.c_str(), newState ? "ACTIVE" : "cleared");

                // Call trigger callbacks
                if (newState && alarm.onTrigger) {
                    RUNTIME_LOG("[RT] Calling onTrigger for alarm: %s", alarm.name.c_str());
                    alarm.onTrigger();
                } else if (!newState && alarm.onClear) {
                    RUNTIME_LOG("[RT] Calling onClear for alarm: %s", alarm.name.c_str());
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
    RUNTIME_LOG("[RT] Meta override set (%d entries)", override.size());
}

void ConfigManagerRuntime::clearRuntimeMetaOverride() {
    runtimeMetaOverride.clear();
    runtimeMetaOverrideActive = false;
    RUNTIME_LOG("[RT] Meta override cleared");
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
    RUNTIME_LOG("[RT] Added button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleButtonPress(const String& group, const String& key) {
    for (auto& button : runtimeButtons) {
        if (button.group == group && button.key == key) {
            if (button.onPress) {
                button.onPress();
                RUNTIME_LOG("[RT] Button pressed: %s.%s", group.c_str(), key.c_str());
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] Button not found: %s.%s", group.c_str(), key.c_str());
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
    RUNTIME_LOG("[RT] Added checkbox: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleCheckboxChange(const String& group, const String& key, bool value) {
    for (auto& checkbox : runtimeCheckboxes) {
        if (checkbox.group == group && checkbox.key == key) {
            if (checkbox.setter) {
                checkbox.setter(value);
                RUNTIME_LOG("[RT] Checkbox changed: %s.%s = %s", group.c_str(), key.c_str(), value ? "true" : "false");
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] Checkbox not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if CM_ENABLE_RUNTIME_STATE_BUTTONS
void ConfigManagerRuntime::defineRuntimeStateButton(const String& group, const String& key, const String& label,
                                                   std::function<bool()> getter, std::function<void(bool)> setter,
                                                   bool initState, const String& card, int order,
                                                   const String& onLabel, const String& offLabel) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isStateButton = true;
    meta.initialState = initState;
    meta.order = order;
    meta.card = card;
    meta.onLabel = onLabel;
    meta.offLabel = offLabel;
    addRuntimeMeta(meta);

    runtimeStateButtons.emplace_back(group, key, getter, setter);
    RUNTIME_LOG("[RT] Added state button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeMomentaryButton(const String& group, const String& key, const String& label,
                                                        std::function<bool()> getter, std::function<void(bool)> setter,
                                                        const String& card, int order,
                                                        const String& onLabel, const String& offLabel) {
    RuntimeFieldMeta meta;
    meta.group = group;
    meta.key = key;
    meta.label = label;
    meta.isMomentaryButton = true;
    meta.order = order;
    meta.card = card;
    meta.onLabel = onLabel;
    meta.offLabel = offLabel;
    addRuntimeMeta(meta);

    runtimeStateButtons.emplace_back(group, key, getter, setter);
    RUNTIME_LOG("[RT] Added momentary button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleStateButtonToggle(const String& group, const String& key) {
    for (auto& button : runtimeStateButtons) {
        if (button.group == group && button.key == key) {
            if (button.getter && button.setter) {
                bool currentState = button.getter();
                bool newState = !currentState;
                button.setter(newState);
                RUNTIME_LOG("[RT] State button toggled: %s.%s = %s", group.c_str(), key.c_str(), newState ? "true" : "false");
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] State button not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleStateButtonSet(const String& group, const String& key, bool value) {
    for (auto& button : runtimeStateButtons) {
        if (button.group == group && button.key == key) {
            if (button.setter) {
                button.setter(value);
                RUNTIME_LOG("[RT] State button set: %s.%s = %s", group.c_str(), key.c_str(), value ? "true" : "false");
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] State button not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if CM_ENABLE_RUNTIME_ANALOG_SLIDERS
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
    RUNTIME_LOG("[RT] Added int slider: %s.%s [%d-%d]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleIntSliderChange(const String& group, const String& key, int value) {
    for (auto& slider : runtimeIntSliders) {
        if (slider.group == group && slider.key == key) {
            if (slider.setter) {
                // Clamp value to range
                int clampedValue = max(slider.minV, min(slider.maxV, value));
                slider.setter(clampedValue);
                RUNTIME_LOG("[RT] Int slider changed: %s.%s = %d", group.c_str(), key.c_str(), clampedValue);
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] Int slider not found: %s.%s", group.c_str(), key.c_str());
}
#endif

#if CM_ENABLE_RUNTIME_ANALOG_SLIDERS
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
    RUNTIME_LOG("[RT] Added float slider: %s.%s [%.2f-%.2f]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleFloatSliderChange(const String& group, const String& key, float value) {
    for (auto& slider : runtimeFloatSliders) {
        if (slider.group == group && slider.key == key) {
            if (slider.setter) {
                // Clamp value to range
                float clampedValue = max(slider.minV, min(slider.maxV, value));
                slider.setter(clampedValue);
                RUNTIME_LOG("[RT] Float slider changed: %s.%s = %.2f", group.c_str(), key.c_str(), clampedValue);
            }
            return;
        }
    }
    RUNTIME_LOG("[RT] Float slider not found: %s.%s", group.c_str(), key.c_str());
}
#endif