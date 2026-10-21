#pragma once

#include <ArduinoJson.h>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include "../ConfigManagerConfig.h"
#include "detail/RuntimeSnapshot.h"
#include "RuntimeFieldTypes.h"
#include "detail/RuntimeMetaView.h"

struct RuntimeMetadataState;
class ConfigManagerRuntime;
namespace cm { namespace detail {
void registerBuiltinSystemMetadata(ConfigManagerRuntime& runtime, bool withTime);
} }

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
  friend void cm::detail::registerBuiltinSystemMetadata(ConfigManagerRuntime&, bool);
public:
  typedef std::function<void(const char*)> LogCallback;

private:
  ConfigManagerClass* configManager;
  LogCallback logCallback;
  // Metadata mutations take cacheMutex before this mutex; cache rebuilds only
  // hold this mutex while traversing mutable metadata.
  mutable std::mutex runtimeDataMutex;
  std::recursive_mutex runtimeGenerationMutex;
  mutable std::mutex runtimeMetaCacheMutex;

  // Reuse metadata only while a response/caller still owns the complete snapshot.
  std::weak_ptr<const String> runtimeMetaJsonCache;
  bool runtimeMetaJsonCacheDirty = true;
  bool runtimeMetaJsonCacheBuildInProgress = false;
  std::atomic<uint32_t> runtimeMetaRevision{0};

#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
  bool runtimeMetaSerializationFailureForTest = false;
  std::atomic<size_t> runtimeMetaSerializationBuildCount{0};
#endif

  // Runtime data
  std::vector<RuntimeValueProvider> runtimeProviders;
  std::vector<RuntimeFieldMeta> runtimeMeta;
  std::unique_ptr<RuntimeMetadataState> runtimeMetadataState;

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
  void registerRuntimePlacement(const RuntimeFieldMeta& meta);
  void registerRuntimePlacement(const RuntimeFieldBase& meta);
  template<class T> RuntimeFieldId addCompactRuntimeMeta(const T& meta, bool zeroPrecision = false);
  template<class T> bool updateCompactRuntimeMeta(RuntimeFieldId id, const std::function<void(T&)>& updater);
  RuntimeFieldMeta* promoteRuntimeMetaLocked(RuntimeFieldId id, RuntimeFieldMeta&& meta);
  cm::RuntimeMetaView runtimeMetaViewLocked(size_t index) const;
  size_t runtimeMetaCountLocked() const;
  void reportRuntimeMetaFailure(const char* reason, size_t bytes, size_t fields) const;
  void logRuntimeMetaAdded(const String& group, const String& key) const;
  void logRuntimeMetaEvent(const char* message, size_t count) const;
  String buildRuntimeMetaJsonLocked(const std::function<bool(const RuntimeFieldMeta&)>& include = {}, const std::function<bool(const cm::RuntimeMetaView&)>& viewInclude = {});
  void markRuntimeMetaJsonCacheDirtyLocked();
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
  RuntimeFieldId addRuntimeMeta(const RuntimeFloatMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeFloatMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeIntMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeIntMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeBoolMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeBoolMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeTextMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeTextMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeDividerMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeDividerMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeButtonMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeButtonMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeCheckboxMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeCheckboxMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeStateButtonMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeStateButtonMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeMomentaryButtonMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeMomentaryButtonMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeIntSliderMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeIntSliderMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeIntInputMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeIntInputMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeFloatSliderMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeFloatSliderMeta&)>& updater);
  RuntimeFieldId addRuntimeMeta(const RuntimeFloatInputMeta& meta);
  bool updateRuntimeMeta(RuntimeFieldId id, const std::function<void(RuntimeFloatInputMeta&)>& updater);
  bool setRuntimeNumericAlarm(RuntimeFieldId id, const RuntimeNumericAlarm& alarm);
  bool setRuntimeBoolAlarm(RuntimeFieldId id, const RuntimeBoolAlarm& alarm);
  bool setRuntimeStateLabels(RuntimeFieldId id, const RuntimeStateLabels& labels);
  bool setRuntimeFieldStyle(RuntimeFieldId id, const RuntimeFieldStyle& style);
  bool setRuntimeStaticText(RuntimeFieldId id, const String& text);
  bool updateRuntimeMeta(const String& group, const String& key, const std::function<void(RuntimeFieldMeta&)>& updater);
  // Note: returned pointer is only stable until the next runtime meta mutation.
  RuntimeFieldMeta* findRuntimeMeta(const String& group, const String& key);

  // JSON generation
  // HTTP waits only for generation; main-loop WS ticks use a nonblocking claim.
  std::unique_lock<std::recursive_mutex> acquireRuntimeGeneration(bool wait = true) {
    std::unique_lock<std::recursive_mutex> lock(runtimeGenerationMutex, std::defer_lock);
    if (wait) lock.lock();
    else (void)lock.try_lock();
    return lock;
  }
  String runtimeValuesToJSON();
  String runtimeValuesToJSON(const cm::RuntimeSelection& selection);
  void fillRuntimeSnapshot(JsonObject root, const cm::RuntimeSelection& selection);
  String runtimeMetaToJSON();
  String runtimeMetaToJSON(const std::function<bool(const RuntimeFieldMeta&)>& include);
  // Internal layout predicates borrow the locked metadata without materialization.
  String runtimeMetaToJSONView(const std::function<bool(const cm::RuntimeMetaView&)>& include);
  bool hasRuntimeMetaView(const std::function<bool(const cm::RuntimeMetaView&)>& include = {}) const;
  bool hasRuntimeMeta(const std::function<bool(const RuntimeFieldMeta&)>& include = {}) const;
  std::shared_ptr<const String> runtimeMetaJsonPayload();

#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
  struct RuntimeMetaStatsForTest {
    size_t retainedBytes=0, legacyCapacityBytes=0, legacyOwnedBytes=0;
    size_t stateBytes=0, storeBytes=0, orderCapacityBytes=0;
    bool compactStateAllocated=false;
  };
  RuntimeMetaStatsForTest getRuntimeMetaStatsForTest() const;

  void setRuntimeMetaSerializationFailureForTest(bool fail) {
    std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
    std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
    runtimeMetaSerializationFailureForTest = fail;
    markRuntimeMetaJsonCacheDirtyLocked();
  }

  size_t getRuntimeMetaSerializationBuildCountForTest() const {
    std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
    return runtimeMetaSerializationBuildCount.load();
  }
#endif

  // System provider
#if CM_ENABLE_SYSTEM_PROVIDER
  void enableBuiltinSystemProvider();
  void updateLoopTiming();
  double getLoopAverage() const {
    return loopAvgMs;
  }
  // Refresh cached sketch metrics (size/free) to avoid repeated bootloader_mmap calls
  void refreshSketchInfoCache();
#else
  void enableBuiltinSystemProvider() {}
  void updateLoopTiming() {}
  double getLoopAverage() const {
    return 0.0;
  }
  void refreshSketchInfoCache() {}
#endif

  // Alarms
  void addRuntimeAlarm(const String& name, std::function<bool()> checkFunction);
  void addRuntimeAlarm(const String& name, std::function<bool()> checkFunction, std::function<void()> onTrigger, std::function<void()> onClear = nullptr);
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
  void defineRuntimeButton(const String& group, const String& key, const String& label, std::function<void()> onPress, const String& card = String(), int order = 100);
  void handleButtonPress(const String& group, const String& key);

  void defineRuntimeCheckbox(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, const String& card = String(), int order = 100);
  void handleCheckboxChange(const String& group, const String& key, bool value);

  void defineRuntimeStateButton(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, bool initState = false, const String& card = String(), int order = 100, const String& onLabel = String(), const String& offLabel = String());
  void defineRuntimeMomentaryButton(const String& group, const String& key, const String& label, std::function<bool()> getter, std::function<void(bool)> setter, const String& card = String(), int order = 100, const String& onLabel = String(), const String& offLabel = String());
  void handleStateButtonToggle(const String& group, const String& key);
  void handleStateButtonSet(const String& group, const String& key, bool value);

  void defineRuntimeIntSlider(const String& group, const String& key, const String& label, int minValue, int maxValue, int initValue, std::function<int()> getter, std::function<void(int)> setter, const String& unit = String(), const String& card = String(), int order = 100);
  void handleIntSliderChange(const String& group, const String& key, int value);

  void defineRuntimeFloatSlider(const String& group, const String& key, const String& label, float minValue, float maxValue, float initValue, int precision, std::function<float()> getter, std::function<void(float)> setter, const String& unit = String(), const String& card = String(), int order = 100);
  void handleFloatSliderChange(const String& group, const String& key, float value);

  // Numeric value inputs (separate from sliders)
  void defineRuntimeIntValue(const String& group, const String& key, const String& label, int minValue, int maxValue, int initValue, std::function<int()> getter, std::function<void(int)> setter, const String& unit = String(), const String& card = String(), int order = 100);
  void handleIntInputChange(const String& group, const String& key, int value);

  void defineRuntimeFloatValue(const String& group, const String& key, const String& label, float minValue, float maxValue, float initValue, int precision, std::function<float()> getter, std::function<void(float)> setter, const String& unit = String(), const String& card = String(), int order = 100);
  void handleFloatInputChange(const String& group, const String& key, float value);

  // Live runtime control registrations (meta handled externally)
  void registerRuntimeButton(const String& group, const String& key, std::function<void()> onPress);
  void registerRuntimeCheckbox(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter);
  void registerRuntimeStateButton(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter);
  void registerRuntimeMomentaryButton(const String& group, const String& key, std::function<bool()> getter, std::function<void(bool)> setter);
  void registerRuntimeIntSlider(const String& group, const String& key, std::function<int()> getter, std::function<void(int)> setter, int minValue, int maxValue);
  void registerRuntimeFloatSlider(const String& group, const String& key, std::function<float()> getter, std::function<void(float)> setter, float minValue, float maxValue);
  void registerRuntimeIntInput(const String& group, const String& key, std::function<int()> getter, std::function<void(int)> setter, int minValue, int maxValue);
  void registerRuntimeFloatInput(const String& group, const String& key, std::function<float()> getter, std::function<void(float)> setter, float minValue, float maxValue);
};
