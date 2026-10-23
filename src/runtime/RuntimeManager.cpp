#include "RuntimeManager.h"
#if CM_ENABLE_LIVE_UI
#include "detail/RuntimeMetaJson.h"
#include "detail/BuiltinSystemMetadata.h"
#include "../ConfigManager.h"
#include <algorithm>
#include <esp_heap_caps.h>
#include <limits>
#include <utility>
#include <time.h>

// Logging support
#define RUNTIME_LOG(...) CM_LOG("[Runtime] " __VA_ARGS__)
#elif CM_ENABLE_LOGGING
// Alarm diagnostics remain operational even when the Live web area is absent.
#include "../ConfigManager.h"
#define RUNTIME_LOG(...) CM_LOG("[Runtime] " __VA_ARGS__)
#else
#define RUNTIME_LOG(...) ((void)0)
#endif

namespace {
#if CM_ENABLE_LIVE_UI

void logRuntimeMetaSerializationFailure(const char* reason, size_t requiredBytes, size_t fieldCount) {
  RUNTIME_LOG("[W] Runtime meta unavailable: %s need=%u free=%u min=%u largest=%u fields=%u",
              reason,
              static_cast<unsigned>(requiredBytes),
              static_cast<unsigned>(ESP.getFreeHeap()),
              static_cast<unsigned>(ESP.getMinFreeHeap()),
              static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
              static_cast<unsigned>(fieldCount));
}

#endif
} // namespace

void ConfigManagerRuntime::setLogCallback(LogCallback logger) {
  logCallback = logger;
}

#if CM_ENABLE_LIVE_UI
void ConfigManagerRuntime::addRuntimeProvider(const RuntimeValueProvider& provider) {
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeProviders.push_back(provider);
  }
  RUNTIME_LOG("Added provider: %s (order: %d)", provider.name.c_str(), provider.order);
}

void ConfigManagerRuntime::addRuntimeProvider(const String& name, std::function<void(JsonObject&)> fillFunc, int order) {
  addRuntimeProvider(RuntimeValueProvider(name, fillFunc, order));
}

#else
void ConfigManagerRuntime::addRuntimeProvider(const RuntimeValueProvider&) {}
void ConfigManagerRuntime::addRuntimeProvider(const String&, std::function<void(JsonObject&)>, int) {}
#endif

RuntimeAlarm* ConfigManagerRuntime::findAlarm(const String& name) {
  for (auto& alarm : runtimeAlarms) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (alarm.name == name) {
      return &alarm;
    }
  }
  return nullptr;
}

const RuntimeAlarm* ConfigManagerRuntime::findAlarm(const String& name) const {
  for (const auto& alarm : runtimeAlarms) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (alarm.name == name) {
      return &alarm;
    }
  }
  return nullptr;
}

#if CM_ENABLE_LIVE_UI
String ConfigManagerRuntime::runtimeValuesToJSON() {
  return runtimeValuesToJSON(cm::RuntimeSelection::everything());
}

String ConfigManagerRuntime::runtimeValuesToJSON(const cm::RuntimeSelection& selection) {
  auto generation = acquireRuntimeGeneration();
  JsonDocument document;
  fillRuntimeSnapshot(document.to<JsonObject>(), selection);
  if (document.overflowed())
    return String();
  String out;
  serializeJson(document, out);
  if (out.length() != measureJson(document))
    return String();
  return out;
}

void ConfigManagerRuntime::fillRuntimeSnapshot(JsonObject root, const cm::RuntimeSelection& selection) {
  root["uptime"] = millis();
  std::vector<RuntimeValueProvider> providers;
  std::vector<RuntimeCheckbox> checkboxes;
  std::vector<RuntimeStateButton> buttons;
  std::vector<RuntimeIntSlider> intSliders;
  std::vector<RuntimeFloatSlider> floatSliders;
  std::vector<RuntimeIntInput> intInputs;
  std::vector<RuntimeFloatInput> floatInputs;
  std::vector<RuntimeAlarm> alarms;
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    const auto group = [](const auto& control) { return control.group.c_str(); };
    providers = cm::selectRuntimeEntries(runtimeProviders, selection, [](const auto& provider) { return provider.name.c_str(); });
    checkboxes = cm::selectRuntimeEntries(runtimeCheckboxes, selection, group);
    buttons = cm::selectRuntimeEntries(runtimeStateButtons, selection, group);
    intSliders = cm::selectRuntimeEntries(runtimeIntSliders, selection, group);
    floatSliders = cm::selectRuntimeEntries(runtimeFloatSliders, selection, group);
    intInputs = cm::selectRuntimeEntries(runtimeIntInputs, selection, group);
    floatInputs = cm::selectRuntimeEntries(runtimeFloatInputs, selection, group);
    if (selection.contains("alarms"))
      alarms = runtimeAlarms;
  }
  // User callbacks run outside the registry mutex and only for selected groups.
  cm::fillRuntimeProviders(root, providers);
  cm::fillRuntimeControls(root, checkboxes);
  cm::fillRuntimeControls(root, buttons);
  cm::fillRuntimeControls(root, intSliders);
  cm::fillRuntimeControls(root, floatSliders);
  cm::fillRuntimeControls(root, intInputs);
  cm::fillRuntimeControls(root, floatInputs);
  if (!alarms.empty()) {
    JsonObject slot = root["alarms"].to<JsonObject>();
    for (const auto& alarm : alarms)
      slot[alarm.name] = alarm.active;
  }
  if (configManager) {
    const auto& ota = configManager->getOTAManager();
    root["system"]["otaActive"] = ota.isActive();
    root["system"]["otaHasPassword"] = ota.hasPassword();
  }
}

void ConfigManagerRuntime::defineRuntimeIntValue(const String& group, const String& key, const String& label, int minValue, int maxValue, int initValue, std::function<int()> getter, std::function<void(int)> setter, const String& unit, const String& card, int order) {
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

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeIntInputs.emplace_back(group, key, getter, setter, minValue, maxValue);
  }
  RUNTIME_LOG("Added int input: %s.%s [%d-%d]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleIntInputChange(const String& group, const String& key, int value) {
  for (auto& input : runtimeIntInputs) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (input.group == group && input.key == key) {
      if (input.setter) {
        int clampedValue = max(input.minV, min(input.maxV, value));
        input.setter(clampedValue);
        RUNTIME_LOG("Int input changed: %s.%s = %d", group.c_str(), key.c_str(), clampedValue);
      }
      return;
    }
  }
  RUNTIME_LOG("Int input not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeFloatValue(const String& group, const String& key, const String& label, float minValue, float maxValue, float initValue, int precision, std::function<float()> getter, std::function<void(float)> setter, const String& unit, const String& card, int order) {
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

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeFloatInputs.emplace_back(group, key, getter, setter, minValue, maxValue);
  }
  RUNTIME_LOG("Added float input: %s.%s [%.2f-%.2f]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleFloatInputChange(const String& group, const String& key, float value) {
  for (auto& input : runtimeFloatInputs) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (input.group == group && input.key == key) {
      if (input.setter) {
        float clampedValue = max(input.minV, min(input.maxV, value));
        input.setter(clampedValue);
        RUNTIME_LOG("Float input changed: %s.%s = %.2f", group.c_str(), key.c_str(), clampedValue);
      }
      return;
    }
  }
  RUNTIME_LOG("Float input not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::registerRuntimeButton(const String& group, const String& key, std::function<void()> onPress) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeButtons.emplace_back(group, key, onPress);
  RUNTIME_LOG("Added button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::registerRuntimeCheckbox(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeCheckboxes.emplace_back(group, key, getter, setter);
  RUNTIME_LOG("Added checkbox: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::registerRuntimeStateButton(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeStateButtons.emplace_back(group, key, getter, setter);
  RUNTIME_LOG("Added state button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::registerRuntimeMomentaryButton(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeStateButtons.emplace_back(group, key, getter, setter);
  RUNTIME_LOG("Added momentary button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::registerRuntimeIntSlider(const String& group, const String& key, std::function<int()> getter, std::function<void(int)> setter, int minValue, int maxValue) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeIntSliders.emplace_back(group, key, getter, setter, minValue, maxValue);
  RUNTIME_LOG("Added int slider: %s.%s [%d-%d]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::registerRuntimeFloatSlider(const String& group, const String& key, std::function<float()> getter, std::function<void(float)> setter, float minValue, float maxValue) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeFloatSliders.emplace_back(group, key, getter, setter, minValue, maxValue);
  RUNTIME_LOG("Added float slider: %s.%s [%.2f-%.2f]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::registerRuntimeIntInput(const String& group, const String& key, std::function<int()> getter, std::function<void(int)> setter, int minValue, int maxValue) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeIntInputs.emplace_back(group, key, getter, setter, minValue, maxValue);
  RUNTIME_LOG("Added int input: %s.%s [%d-%d]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::registerRuntimeFloatInput(const String& group, const String& key, std::function<float()> getter, std::function<void(float)> setter, float minValue, float maxValue) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  runtimeFloatInputs.emplace_back(group, key, getter, setter, minValue, maxValue);
  RUNTIME_LOG("Added float input: %s.%s [%.2f-%.2f]", group.c_str(), key.c_str(), minValue, maxValue);
}

#if CM_ENABLE_SYSTEM_PROVIDER

// File-scope cache to avoid repeated bootloader_mmap calls
size_t _cm_cachedSketchSize = 0;
size_t _cm_cachedFreeSketch = 0;
bool _cm_sketchInfoInitialized = false;

// Simple RSSI-to-text quality mapper
static const char* rssiQualityText(int rssi) {
  // Typical RSSI range: -30 (excellent) to -90 (very weak)
  if (rssi >= -50)
    return "excellent"; // Excellent
  if (rssi >= -60)
    return "good"; // Good
  if (rssi >= -67)
    return "ok"; // Fair/OK
  if (rssi >= -75)
    return "weak";    // Weak
  return "very weak"; // Very weak / none
}

void ConfigManagerRuntime::enableBuiltinSystemProvider() {
  if (builtinSystemProviderRegistered)
    return;

  addRuntimeProvider("system", [this](JsonObject& obj) {
        obj["freeHeap"] = ESP.getFreeHeap();
        obj["totalHeap"] = ESP.getHeapSize();
        obj["usedHeap"] = ESP.getHeapSize() - ESP.getFreeHeap();
        obj["heapFragmentation"] = 100 - (ESP.getMaxAllocHeap() * 100) / ESP.getFreeHeap();

#if CM_ENABLE_WIFI
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
#else
        obj["rssi"] = 0;
        obj["rssiTxt"] = rssiQualityText(-100);
        obj["wifiConnected"] = false;
        obj["localIP"] = "0.0.0.0";
        obj["gateway"] = "0.0.0.0";
        obj["routerMAC"] = "00:00:00:00:00:00";
        obj["channel"] = 0;
#endif

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
            // Cppcheck rationale: Avoid changing mutable callback and integration handles without a call-site audit.
            // cppcheck-suppress constVariableReference
            auto &ota = configManager->getOTAManager();
            // obj["allowOTA"] = ota.isEnabled();
            obj["otaActive"] = ota.isActive();
            obj["otaHasPassword"] = ota.hasPassword();
        } }, 0);

  cm::detail::registerBuiltinSystemMetadata(*this, CM_ENABLE_SYSTEM_TIME != 0);

  builtinSystemProviderRegistered = true;
  builtinSystemProviderEnabled = true;
  RUNTIME_LOG("Built-in system provider enabled");
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

#else
String ConfigManagerRuntime::runtimeValuesToJSON() { return {}; }
String ConfigManagerRuntime::runtimeValuesToJSON(const cm::RuntimeSelection&) { return {}; }
void ConfigManagerRuntime::fillRuntimeSnapshot(JsonObject, const cm::RuntimeSelection&) {}
#endif

void ConfigManagerRuntime::addRuntimeAlarm(const String& name, std::function<bool()> checkFunction) {
  RuntimeAlarm alarm;
  alarm.name = name;
  alarm.checkFunction = checkFunction;
  alarm.manual = false;
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeAlarms.push_back(std::move(alarm));
  }
  RUNTIME_LOG("Added alarm: %s", name.c_str());
}

void ConfigManagerRuntime::addRuntimeAlarm(const String& name, std::function<bool()> checkFunction, std::function<void()> onTrigger, std::function<void()> onClear) {
  RuntimeAlarm alarm;
  alarm.name = name;
  alarm.checkFunction = checkFunction;
  alarm.onTrigger = onTrigger;
  alarm.onClear = onClear;
  alarm.manual = false;
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeAlarms.push_back(std::move(alarm));
  }
  RUNTIME_LOG("Added alarm with triggers: %s", name.c_str());
}

void ConfigManagerRuntime::registerRuntimeAlarm(const String& name, std::function<void()> onTrigger, std::function<void()> onClear) {
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    RuntimeAlarm* alarm = findAlarm(name);
    if (alarm) {
      alarm->manual = true;
      alarm->checkFunction = nullptr;
      if (onTrigger)
        alarm->onTrigger = onTrigger;
      if (onClear)
        alarm->onClear = onClear;
      RUNTIME_LOG("Updated manual alarm registration: %s", name.c_str());
      return;
    }

    RuntimeAlarm newAlarm;
    newAlarm.name = name;
    newAlarm.manual = true;
    newAlarm.onTrigger = onTrigger;
    newAlarm.onClear = onClear;
    runtimeAlarms.push_back(std::move(newAlarm));
  }
  RUNTIME_LOG("Registered manual alarm: %s", name.c_str());
}

void ConfigManagerRuntime::setRuntimeAlarmActive(const String& name, bool active, bool fireCallbacks) {
  std::function<void()> callbackToInvoke;
  String callbackAlarmName;
  // Cppcheck rationale: Preserve buffer and state lifetime across callback branches.
  // cppcheck-suppress variableScope
  // Cppcheck rationale: The value is consumed by a compile-time logging macro in enabled builds.
  // cppcheck-suppress unreadVariable
  bool callbackIsTrigger = false;
  bool shouldLogStateChange = false;
  bool wasCreated = false;
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    RuntimeAlarm* alarm = findAlarm(name);
    if (!alarm) {
      RuntimeAlarm newAlarm;
      newAlarm.name = name;
      newAlarm.manual = true;
      newAlarm.active = active;
      runtimeAlarms.push_back(std::move(newAlarm));
      alarm = &runtimeAlarms.back();
      wasCreated = true;
    }

    alarm->manual = true;

    if (alarm->active == active && !wasCreated) {
      return;
    }

    alarm->active = active;
    shouldLogStateChange = !wasCreated;

    if (fireCallbacks) {
      if (active && alarm->onTrigger) {
        callbackToInvoke = alarm->onTrigger;
        callbackAlarmName = alarm->name;
        // Cppcheck rationale: The value is consumed by a compile-time logging macro in enabled builds.
        // cppcheck-suppress unreadVariable
        callbackIsTrigger = true;
      } else if (!active && alarm->onClear) {
        callbackToInvoke = alarm->onClear;
        callbackAlarmName = alarm->name;
        // Cppcheck rationale: The value is consumed by a compile-time logging macro in enabled builds.
        // cppcheck-suppress unreadVariable
        callbackIsTrigger = false;
      }
    }
  }

  if (wasCreated) {
    RUNTIME_LOG("Lazily created manual alarm entry: %s", name.c_str());
  }
  if (shouldLogStateChange) {
    RUNTIME_LOG("Alarm %s manually set to %s", name.c_str(), active ? "ACTIVE" : "cleared");
  }
  if (callbackToInvoke) {
    RUNTIME_LOG("Manual %s callback for alarm: %s",
                callbackIsTrigger ? "trigger" : "clear",
                callbackAlarmName.c_str());
    callbackToInvoke();
  }
}

bool ConfigManagerRuntime::isRuntimeAlarmActive(const String& name) const {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  const RuntimeAlarm* alarm = findAlarm(name);
  return alarm ? alarm->active : false;
}

void ConfigManagerRuntime::updateAlarms() {
  struct PendingAlarmCallback {
    String name;
    bool newState = false;
    bool hasCallback = false;
    std::function<void()> callback;
  };

  std::vector<RuntimeAlarm> snapshot;
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    snapshot = runtimeAlarms;
  }

  std::vector<PendingAlarmCallback> pending;
  pending.reserve(snapshot.size());
  for (const auto& alarm : snapshot) {
    if (alarm.manual || !alarm.checkFunction) {
      continue;
    }

    const bool newState = alarm.checkFunction();
    if (newState == alarm.active) {
      continue;
    }

    PendingAlarmCallback entry;
    entry.name = alarm.name;
    entry.newState = newState;
    if (newState && alarm.onTrigger) {
      entry.hasCallback = true;
      entry.callback = alarm.onTrigger;
    } else if (!newState && alarm.onClear) {
      entry.hasCallback = true;
      entry.callback = alarm.onClear;
    }
    pending.push_back(std::move(entry));
  }

  if (pending.empty()) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    for (const auto& change : pending) {
      RuntimeAlarm* liveAlarm = findAlarm(change.name);
      if (liveAlarm) {
        liveAlarm->active = change.newState;
      }
    }
  }

  for (const auto& change : pending) {
    RUNTIME_LOG("Alarm %s: %s", change.name.c_str(), change.newState ? "ACTIVE" : "cleared");
    if (change.hasCallback && change.callback) {
      RUNTIME_LOG("Calling %s for alarm: %s",
                  change.newState ? "onTrigger" : "onClear",
                  change.name.c_str());
      change.callback();
    }
  }
}

bool ConfigManagerRuntime::hasActiveAlarms() const {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
  // cppcheck-suppress useStlAlgorithm
  for (const auto& alarm : runtimeAlarms) {
    if (alarm.active)
      return true;
  }
  return false;
}

std::vector<String> ConfigManagerRuntime::getActiveAlarms() const {
  std::vector<String> active;
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  for (const auto& alarm : runtimeAlarms) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (alarm.active) {
      active.push_back(alarm.name);
    }
  }
  return active;
}

// Interactive runtime control implementations

#if CM_ENABLE_LIVE_UI
void ConfigManagerRuntime::defineRuntimeButton(const String& group, const String& key, const String& label, std::function<void()> onPress, const String& card, int order) {
  RuntimeFieldMeta meta;
  meta.group = group;
  meta.key = key;
  meta.label = label;
  meta.isButton = true;
  meta.order = order;
  meta.card = card;
  addRuntimeMeta(meta);

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeButtons.emplace_back(group, key, onPress);
  }
  RUNTIME_LOG("Added button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleButtonPress(const String& group, const String& key) {
  for (auto& button : runtimeButtons) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (button.group == group && button.key == key) {
      if (button.onPress) {
        button.onPress();
        RUNTIME_LOG("Button pressed: %s.%s", group.c_str(), key.c_str());
      }
      return;
    }
  }
  RUNTIME_LOG("Button not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeCheckbox(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, const String& card, int order) {
  RuntimeFieldMeta meta;
  meta.group = group;
  meta.key = key;
  meta.label = label;
  meta.isCheckbox = true;
  meta.order = order;
  meta.card = card;
  addRuntimeMeta(meta);

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeCheckboxes.emplace_back(group, key, getter, setter);
  }
  RUNTIME_LOG("Added checkbox: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleCheckboxChange(const String& group, const String& key, bool value) {
  for (auto& checkbox : runtimeCheckboxes) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (checkbox.group == group && checkbox.key == key) {
      if (checkbox.setter) {
        checkbox.setter(value);
        RUNTIME_LOG("Checkbox changed: %s.%s = %s", group.c_str(), key.c_str(), value ? "true" : "false");
      }
      return;
    }
  }
  RUNTIME_LOG("Checkbox not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeStateButton(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, bool initState, const String& card, int order, const String& onLabel, const String& offLabel) {
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

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeStateButtons.emplace_back(group, key, getter, setter);
  }
  RUNTIME_LOG("Added state button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeMomentaryButton(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, const String& card, int order, const String& onLabel, const String& offLabel) {
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

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeStateButtons.emplace_back(group, key, getter, setter);
  }
  RUNTIME_LOG("Added momentary button: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleStateButtonToggle(const String& group, const String& key) {
  for (auto& button : runtimeStateButtons) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (button.group == group && button.key == key) {
      if (button.getter && button.setter) {
        bool currentState = button.getter();
        bool newState = !currentState;
        button.setter(newState);
        RUNTIME_LOG("State button toggled: %s.%s = %s", group.c_str(), key.c_str(), newState ? "true" : "false");
      }
      return;
    }
  }
  RUNTIME_LOG("State button not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::handleStateButtonSet(const String& group, const String& key, bool value) {
  for (auto& button : runtimeStateButtons) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (button.group == group && button.key == key) {
      if (button.setter) {
        button.setter(value);
        RUNTIME_LOG("State button set: %s.%s = %s", group.c_str(), key.c_str(), value ? "true" : "false");
      }
      return;
    }
  }
  RUNTIME_LOG("State button not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeIntSlider(const String& group, const String& key, const String& label, int minValue, int maxValue, int initValue, std::function<int()> getter, std::function<void(int)> setter, const String& unit, const String& card, int order) {
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

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeIntSliders.emplace_back(group, key, getter, setter, minValue, maxValue);
  }
  RUNTIME_LOG("Added int slider: %s.%s [%d-%d]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleIntSliderChange(const String& group, const String& key, int value) {
  for (auto& slider : runtimeIntSliders) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (slider.group == group && slider.key == key) {
      if (slider.setter) {
        // Clamp value to range
        int clampedValue = max(slider.minV, min(slider.maxV, value));
        slider.setter(clampedValue);
        RUNTIME_LOG("Int slider changed: %s.%s = %d", group.c_str(), key.c_str(), clampedValue);
      }
      return;
    }
  }
  RUNTIME_LOG("Int slider not found: %s.%s", group.c_str(), key.c_str());
}

void ConfigManagerRuntime::defineRuntimeFloatSlider(const String& group, const String& key, const String& label, float minValue, float maxValue, float initValue, int precision, std::function<float()> getter, std::function<void(float)> setter, const String& unit, const String& card, int order) {
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

  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    runtimeFloatSliders.emplace_back(group, key, getter, setter, minValue, maxValue);
  }
  RUNTIME_LOG("Added float slider: %s.%s [%.2f-%.2f]", group.c_str(), key.c_str(), minValue, maxValue);
}

void ConfigManagerRuntime::handleFloatSliderChange(const String& group, const String& key, float value) {
  for (auto& slider : runtimeFloatSliders) {
    // Cppcheck rationale: Keep the allocation-free, early-exit loop on the embedded target.
    // cppcheck-suppress useStlAlgorithm
    if (slider.group == group && slider.key == key) {
      if (slider.setter) {
        // Clamp value to range
        float clampedValue = max(slider.minV, min(slider.maxV, value));
        slider.setter(clampedValue);
        RUNTIME_LOG("Float slider changed: %s.%s = %.2f", group.c_str(), key.c_str(), clampedValue);
      }
      return;
    }
  }
  RUNTIME_LOG("Float slider not found: %s.%s", group.c_str(), key.c_str());
}

// Metadata core hooks keep networking and target diagnostics out of the registry.
void ConfigManagerRuntime::registerRuntimePlacement(const RuntimeFieldMeta& meta) {
  if (configManager)
    configManager->registerLivePlacement(meta);
}
void ConfigManagerRuntime::registerRuntimePlacement(const RuntimeFieldBase& meta) {
  if (configManager)
    configManager->registerLivePlacement(cm::RuntimeMetaView(meta));
}
void ConfigManagerRuntime::reportRuntimeMetaFailure(const char* reason, size_t bytes, size_t fields) const {
  logRuntimeMetaSerializationFailure(reason, bytes, fields);
}
void ConfigManagerRuntime::logRuntimeMetaAdded(const String& group, const String& key) const {
  RUNTIME_LOG("Added meta: %s.%s", group.c_str(), key.c_str());
}
void ConfigManagerRuntime::logRuntimeMetaEvent(const char* message, size_t count) const {
  RUNTIME_LOG("%s (%u)", message, static_cast<unsigned>(count));
}

#endif

#if !CM_ENABLE_LIVE_UI
void ConfigManagerRuntime::defineRuntimeButton(const String& group, const String& key, const String& label, std::function<void()> onPress, const String& card, int order) {}
void ConfigManagerRuntime::handleButtonPress(const String& group, const String& key) {}
void ConfigManagerRuntime::defineRuntimeCheckbox(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, const String& card, int order) {}
void ConfigManagerRuntime::handleCheckboxChange(const String& group, const String& key, bool value) {}
void ConfigManagerRuntime::defineRuntimeStateButton(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, bool initState, const String& card, int order, const String& onLabel, const String& offLabel) {}
void ConfigManagerRuntime::defineRuntimeMomentaryButton(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, const String& card, int order, const String& onLabel, const String& offLabel) {}
void ConfigManagerRuntime::handleStateButtonToggle(const String& group, const String& key) {}
void ConfigManagerRuntime::handleStateButtonSet(const String& group, const String& key, bool value) {}
void ConfigManagerRuntime::defineRuntimeIntSlider(const String& group, const String& key, const String& label, int minValue, int maxValue, int initValue, std::function<int()> getter, std::function<void(int)> setter, const String& unit, const String& card, int order) {}
void ConfigManagerRuntime::handleIntSliderChange(const String& group, const String& key, int value) {}
void ConfigManagerRuntime::defineRuntimeFloatSlider(const String& group, const String& key, const String& label, float minValue, float maxValue, float initValue, int precision, std::function<float()> getter, std::function<void(float)> setter, const String& unit, const String& card, int order) {}
void ConfigManagerRuntime::handleFloatSliderChange(const String& group, const String& key, float value) {}
void ConfigManagerRuntime::defineRuntimeIntValue(const String& group, const String& key, const String& label, int minValue, int maxValue, int initValue, std::function<int()> getter, std::function<void(int)> setter, const String& unit, const String& card, int order) {}
void ConfigManagerRuntime::handleIntInputChange(const String& group, const String& key, int value) {}
void ConfigManagerRuntime::defineRuntimeFloatValue(const String& group, const String& key, const String& label, float minValue, float maxValue, float initValue, int precision, std::function<float()> getter, std::function<void(float)> setter, const String& unit, const String& card, int order) {}
void ConfigManagerRuntime::handleFloatInputChange(const String& group, const String& key, float value) {}
void ConfigManagerRuntime::registerRuntimeButton(const String& group, const String& key, std::function<void()> onPress) {}
void ConfigManagerRuntime::registerRuntimeCheckbox(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter) {}
void ConfigManagerRuntime::registerRuntimeStateButton(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter) {}
void ConfigManagerRuntime::registerRuntimeMomentaryButton(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter) {}
void ConfigManagerRuntime::registerRuntimeIntSlider(const String& group, const String& key, std::function<int()> getter, std::function<void(int)> setter, int minValue, int maxValue) {}
void ConfigManagerRuntime::registerRuntimeFloatSlider(const String& group, const String& key, std::function<float()> getter, std::function<void(float)> setter, float minValue, float maxValue) {}
void ConfigManagerRuntime::registerRuntimeIntInput(const String& group, const String& key, std::function<int()> getter, std::function<void(int)> setter, int minValue, int maxValue) {}
void ConfigManagerRuntime::registerRuntimeFloatInput(const String& group, const String& key, std::function<float()> getter, std::function<void(float)> setter, float minValue, float maxValue) {}
#endif
