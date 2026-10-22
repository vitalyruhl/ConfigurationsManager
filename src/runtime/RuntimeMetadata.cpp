#include "RuntimeManager.h"
#include "detail/RuntimeMetaJson.h"
#include "detail/CompactRuntimeMetaStore.h"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

struct RuntimeMetadataState {
  struct Ref {
    uint32_t legacyIndex = 0;
    RuntimeFieldId compactId;
  };
  cm::CompactRuntimeMetaStore store;
  cm::detail::CheckedVector<Ref> order;
};

namespace {
bool copyBase(RuntimeFieldBase& to, const cm::RuntimeMetaView& from) {
#define CM_COPY_BASE(n)                             \
  if (!cm::detail::copyRuntimeString(to.n, from.n)) \
  return false
  CM_COPY_BASE(key);
  CM_COPY_BASE(label);
  CM_COPY_BASE(group);
  CM_COPY_BASE(sourceGroup);
  CM_COPY_BASE(page);
  CM_COPY_BASE(card);
#undef CM_COPY_BASE
  to.order = from.order;
  return true;
}
bool normalizeBase(RuntimeFieldBase& base) {
  if (base.sourceGroup.isEmpty()) {
    if (!cm::detail::copyRuntimeString(base.sourceGroup, base.group))
      return false;
    if (base.page.isEmpty() && base.card.isEmpty())
      base.group = String();
  }
  return true;
}
template <class T>
bool copyDefinition(T& to, const T& from) {
  if (!copyBase(to.base, cm::RuntimeMetaView(from.base)))
    return false;
  using P = typename cm::detail::MetaTraits<T>::Payload;
  if constexpr (!std::is_same<P, cm::detail::EmptyPayload>::value && !std::is_same<P, cm::detail::StatePayload>::value)
    if (!cm::detail::copyRuntimeString(to.unit, from.unit))
      return false;
  if constexpr (std::is_same<P, cm::detail::FloatPayload>::value || std::is_same<P, cm::detail::FloatRangePayload>::value)
    to.precision = from.precision;
  if constexpr (std::is_same<P, cm::detail::StatePayload>::value)
    to.initialState = from.initialState;
  if constexpr (std::is_same<P, cm::detail::IntRangePayload>::value) {
    to.intMin = from.intMin;
    to.intMax = from.intMax;
    to.intInit = from.intInit;
  }
  if constexpr (std::is_same<P, cm::detail::FloatRangePayload>::value) {
    to.floatMin = from.floatMin;
    to.floatMax = from.floatMax;
    to.floatInit = from.floatInit;
  }
  return normalizeBase(to.base);
}
} // namespace

ConfigManagerRuntime::ConfigManagerRuntime()
    : configManager(nullptr)
#if CM_ENABLE_SYSTEM_PROVIDER
      ,
      builtinSystemProviderEnabled(false), builtinSystemProviderRegistered(false), loopWindowStart(0), loopSamples(0), loopAccumMs(0.0), loopAvgMs(0.0)
#endif
{
}
ConfigManagerRuntime::~ConfigManagerRuntime() {}

size_t ConfigManagerRuntime::runtimeMetaCountLocked() const {
#ifdef development
  if (runtimeMetaOverrideActive)
    return runtimeMetaOverride.size();
#endif
  return runtimeMetadataState ? runtimeMetadataState->order.size() : runtimeMeta.size();
}
cm::RuntimeMetaView ConfigManagerRuntime::runtimeMetaViewLocked(size_t index) const {
#ifdef development
  if (runtimeMetaOverrideActive)
    return cm::RuntimeMetaView(runtimeMetaOverride[index]);
#endif
  if (!runtimeMetadataState)
    return cm::RuntimeMetaView(runtimeMeta[index]);
  const auto& ref = runtimeMetadataState->order[index];
  if (!ref.compactId)
    return cm::RuntimeMetaView(runtimeMeta[ref.legacyIndex]);
  const auto promoted = runtimeMetadataState->store.promotedLegacyIndex(ref.compactId);
  if (promoted != std::numeric_limits<size_t>::max())
    return cm::RuntimeMetaView(runtimeMeta[promoted]);
  return runtimeMetadataState->store.view(ref.compactId);
}
void ConfigManagerRuntime::begin(ConfigManagerClass* cm) {
  configManager = cm;
  logRuntimeMetaEvent("Runtime manager initialized", 0);
  // Copy only one routing definition; callbacks may acquire layout locks.
  size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(runtimeDataMutex);
    count = runtimeMetadataState ? runtimeMetadataState->order.size() : runtimeMeta.size();
  }
  for (size_t i = 0; configManager && i < count; ++i) {
    RuntimeFieldBase placement;
    {
      std::lock_guard<std::mutex> lock(runtimeDataMutex);
      // Development overrides affect output only, as in the legacy path.
      if (runtimeMetadataState) {
        const auto& r = runtimeMetadataState->order[i];
        const auto promoted = r.compactId ? runtimeMetadataState->store.promotedLegacyIndex(r.compactId) : std::numeric_limits<size_t>::max();
        if (r.compactId && promoted == std::numeric_limits<size_t>::max()) {
          if (!copyBase(placement, runtimeMetadataState->store.view(r.compactId)))
            continue;
        } else if (!copyBase(placement, cm::RuntimeMetaView(runtimeMeta[r.compactId ? promoted : r.legacyIndex])))
          continue;
      } else if (!copyBase(placement, cm::RuntimeMetaView(runtimeMeta[i])))
        continue;
    }
    registerRuntimePlacement(placement);
  }
}

void ConfigManagerRuntime::addRuntimeMeta(const RuntimeFieldMeta& meta) {
  RuntimeFieldMeta normalized = meta;
  if (normalized.sourceGroup.isEmpty()) {
    normalized.sourceGroup = normalized.group;
    if (normalized.page.isEmpty() && normalized.card.isEmpty())
      normalized.group = String();
  }
  {
    std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
    std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
    if (runtimeMetadataState && (runtimeMeta.size() > UINT32_MAX || !runtimeMetadataState->order.ensureOne()))
      return;
    runtimeMeta.push_back(normalized);
    if (runtimeMetadataState)
      runtimeMetadataState->order.emplace_back(RuntimeMetadataState::Ref{static_cast<uint32_t>(runtimeMeta.size() - 1), {}});
    markRuntimeMetaJsonCacheDirtyLocked();
  }
  logRuntimeMetaAdded(normalized.sourceGroup.length() ? normalized.sourceGroup : normalized.group, normalized.key);
  if (configManager)
    registerRuntimePlacement(normalized);
}

template <class T>
RuntimeFieldId ConfigManagerRuntime::addCompactRuntimeMeta(const T& meta, bool zeroPrecision) {
  T normalized;
  if (!copyDefinition(normalized, meta))
    return {};
  RuntimeFieldId id;
  {
    std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
    std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
    std::unique_ptr<RuntimeMetadataState> pending;
    auto* state = runtimeMetadataState.get();
    if (!state) {
      if (runtimeMeta.size() > UINT32_MAX)
        return {};
      pending.reset(new (std::nothrow) RuntimeMetadataState());
      if (!pending || !pending->order.reserve(runtimeMeta.size() + 1))
        return {};
      for (size_t i = 0; i < runtimeMeta.size(); ++i)
        pending->order.emplace_back(RuntimeMetadataState::Ref{static_cast<uint32_t>(i), {}});
      state = pending.get();
    }
    if (!state->order.ensureOne())
      return {};
    id = state->store.add(normalized, zeroPrecision);
    if (!id)
      return {};
    state->order.emplace_back(RuntimeMetadataState::Ref{0, id});
    if (pending)
      runtimeMetadataState = std::move(pending);
    markRuntimeMetaJsonCacheDirtyLocked();
  }
  logRuntimeMetaAdded(normalized.base.sourceGroup.length() ? normalized.base.sourceGroup : normalized.base.group, normalized.base.key);
  if (configManager)
    registerRuntimePlacement(normalized.base);
  return id;
}
template <class T>
bool ConfigManagerRuntime::updateCompactRuntimeMeta(RuntimeFieldId id, const std::function<void(T&)>& updater) {
  if (!updater)
    return false;
  std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
  std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
  if (!runtimeMetadataState || !runtimeMetadataState->store.update(id, updater))
    return false;
  markRuntimeMetaJsonCacheDirtyLocked();
  return true;
}
#define CM_TYPED_API(T)                                                                                     \
  RuntimeFieldId ConfigManagerRuntime::addRuntimeMeta(const T& meta) {                                      \
    return addCompactRuntimeMeta(meta);                                                                     \
  }                                                                                                         \
  bool ConfigManagerRuntime::updateRuntimeMeta(RuntimeFieldId id, const std::function<void(T&)>& updater) { \
    return updateCompactRuntimeMeta(id, updater);                                                           \
  }
CM_TYPED_API(RuntimeFloatMeta)
CM_TYPED_API(RuntimeIntMeta)
CM_TYPED_API(RuntimeBoolMeta)
CM_TYPED_API(RuntimeTextMeta)
CM_TYPED_API(RuntimeDividerMeta)
CM_TYPED_API(RuntimeButtonMeta)
CM_TYPED_API(RuntimeCheckboxMeta)
CM_TYPED_API(RuntimeStateButtonMeta)
CM_TYPED_API(RuntimeMomentaryButtonMeta)
CM_TYPED_API(RuntimeIntSliderMeta)
CM_TYPED_API(RuntimeIntInputMeta)
CM_TYPED_API(RuntimeFloatSliderMeta)
CM_TYPED_API(RuntimeFloatInputMeta)
#undef CM_TYPED_API
#define CM_EXTENSION_API(method, type, storeMethod)                                   \
  bool ConfigManagerRuntime::method(RuntimeFieldId id, const type& value) {           \
    std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);                     \
    std::lock_guard<std::mutex> dataLock(runtimeDataMutex);                           \
    if (!runtimeMetadataState || !runtimeMetadataState->store.storeMethod(id, value)) \
      return false;                                                                   \
    markRuntimeMetaJsonCacheDirtyLocked();                                            \
    return true;                                                                      \
  }
CM_EXTENSION_API(setRuntimeNumericAlarm, RuntimeNumericAlarm, setNumericAlarm)
CM_EXTENSION_API(setRuntimeBoolAlarm, RuntimeBoolAlarm, setBoolAlarm)
CM_EXTENSION_API(setRuntimeStateLabels, RuntimeStateLabels, setStateLabels)
CM_EXTENSION_API(setRuntimeFieldStyle, RuntimeFieldStyle, setStyle)
CM_EXTENSION_API(setRuntimeStaticText, String, setStaticText)
#undef CM_EXTENSION_API

RuntimeFieldMeta* ConfigManagerRuntime::promoteRuntimeMetaLocked(RuntimeFieldId id, RuntimeFieldMeta&& meta) {
  if (runtimeMeta.size() > UINT32_MAX)
    return nullptr;
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
  try {
#endif
    runtimeMeta.push_back(std::move(meta));
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
#endif
  if (!runtimeMetadataState->store.promote(id, runtimeMeta.size() - 1)) {
    runtimeMeta.pop_back();
    return nullptr;
  }
  markRuntimeMetaJsonCacheDirtyLocked();
  return &runtimeMeta.back();
}
bool ConfigManagerRuntime::updateRuntimeMeta(const String& group, const String& key, const std::function<void(RuntimeFieldMeta&)>& updater) {
  if (!updater)
    return false;
  std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
  std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
  if (runtimeMetadataState) {
    for (const auto& ref : runtimeMetadataState->order) {
      const auto promoted = ref.compactId ? runtimeMetadataState->store.promotedLegacyIndex(ref.compactId) : std::numeric_limits<size_t>::max();
      if (!ref.compactId || promoted != std::numeric_limits<size_t>::max()) {
        auto& m = runtimeMeta[ref.compactId ? promoted : ref.legacyIndex];
        if (m.key == key && (m.group == group || m.sourceGroup == group)) {
          updater(m);
          markRuntimeMetaJsonCacheDirtyLocked();
          return true;
        }
      } else {
        const auto view = runtimeMetadataState->store.view(ref.compactId);
        if (view.key != key || (view.group != group && view.sourceGroup != group))
          continue;
        RuntimeFieldMeta m;
        if (!runtimeMetadataState->store.materialize(ref.compactId, m))
          return false;
        updater(m);
        if (runtimeMetadataState->store.canApplyLegacy(ref.compactId, m)) {
          if (!runtimeMetadataState->store.applyLegacy(ref.compactId, m))
            return false;
          markRuntimeMetaJsonCacheDirtyLocked();
          return true;
        }
        return promoteRuntimeMetaLocked(ref.compactId, std::move(m)) != nullptr;
      }
    }
  } else
    for (auto& m : runtimeMeta)
      if (m.key == key && (m.group == group || m.sourceGroup == group)) {
        updater(m);
        markRuntimeMetaJsonCacheDirtyLocked();
        return true;
      }
  return false;
}
RuntimeFieldMeta* ConfigManagerRuntime::findRuntimeMeta(const String& group, const String& key) {
  std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
  std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
  if (runtimeMetadataState) {
    for (const auto& ref : runtimeMetadataState->order) {
      const auto promoted = ref.compactId ? runtimeMetadataState->store.promotedLegacyIndex(ref.compactId) : std::numeric_limits<size_t>::max();
      if (!ref.compactId || promoted != std::numeric_limits<size_t>::max()) {
        auto& m = runtimeMeta[ref.compactId ? promoted : ref.legacyIndex];
        if (m.key == key && (m.group == group || m.sourceGroup == group))
          return &m;
      } else {
        const auto v = runtimeMetadataState->store.view(ref.compactId);
        if (v.key != key || (v.group != group && v.sourceGroup != group))
          continue;
        RuntimeFieldMeta m;
        if (!runtimeMetadataState->store.materialize(ref.compactId, m))
          return nullptr;
        return promoteRuntimeMetaLocked(ref.compactId, std::move(m));
      }
    }
  } else
    for (auto& m : runtimeMeta)
      if (m.key == key && (m.group == group || m.sourceGroup == group))
        return &m;
  return nullptr;
}

String ConfigManagerRuntime::buildRuntimeMetaJsonLocked(const std::function<bool(const RuntimeFieldMeta&)>& include, const std::function<bool(const cm::RuntimeMetaView&)>& viewInclude) {
#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
  if (runtimeMetaSerializationFailureForTest)
    return String();
#endif
  const auto less = [](const auto& a, const auto& b) {if(a.group==b.group){if(a.order==b.order)return a.label<b.label;return a.order<b.order;}return a.group<b.group; };
#ifdef development
  if (runtimeMetaOverrideActive)
    std::sort(runtimeMetaOverride.begin(), runtimeMetaOverride.end(), less);
  else
#endif
    if (runtimeMetadataState) {
    auto& refs = runtimeMetadataState->order;
    const auto refView = [&](const RuntimeMetadataState::Ref& r) {
      if (!r.compactId)
        return cm::RuntimeMetaView(runtimeMeta[r.legacyIndex]);
      const auto promoted = runtimeMetadataState->store.promotedLegacyIndex(r.compactId);
      return promoted != std::numeric_limits<size_t>::max() ? cm::RuntimeMetaView(runtimeMeta[promoted]) : runtimeMetadataState->store.view(r.compactId);
    };
    if (refs.size() > 1)
      std::sort(refs.begin(), refs.end(), [&](const auto& a, const auto& b) { return less(refView(a), refView(b)); });
  } else
    std::sort(runtimeMeta.begin(), runtimeMeta.end(), less);
  const auto emit = [&](auto& sink) {
    if (!sink.raw("["))
      return false;
    bool first = true;
    for (size_t i = 0; i < runtimeMetaCountLocked(); ++i) {
      const auto v = runtimeMetaViewLocked(i);
      if (viewInclude && !viewInclude(v))
        continue;
      if (include) {
#ifdef development
        if (runtimeMetaOverrideActive) {
          if (!include(runtimeMetaOverride[i]))
            continue;
        } else
#endif
          if (!runtimeMetadataState) {
          if (!include(runtimeMeta[i]))
            continue;
        } else {
          const auto& r = runtimeMetadataState->order[i];
          const auto promoted = r.compactId ? runtimeMetadataState->store.promotedLegacyIndex(r.compactId) : std::numeric_limits<size_t>::max();
          if (!r.compactId || promoted != std::numeric_limits<size_t>::max()) {
            if (!include(runtimeMeta[r.compactId ? promoted : r.legacyIndex]))
              continue;
          } else {
            RuntimeFieldMeta temporary;
            if (!runtimeMetadataState->store.materialize(r.compactId, temporary))
              return false;
            if (!include(temporary))
              continue;
          }
        }
      }
      if (!first && !sink.raw(","))
        return false;
      first = false;
      if (!cm::writeRuntimeMeta(sink, v))
        return false;
    }
    return sink.raw("]");
  };
  cm::JsonOutput<String> count(nullptr, std::numeric_limits<unsigned int>::max() - size_t(1));
  if (!emit(count) || !count.good()) {
    reportRuntimeMetaFailure("size", count.size(), runtimeMetaCountLocked());
    return String();
  }
  String out;
  if (!out.reserve(static_cast<unsigned int>(count.size()))) {
    reportRuntimeMetaFailure("reserve", count.size(), runtimeMetaCountLocked());
    return String();
  }
  cm::JsonOutput<String> writer(&out, count.size());
  if (!emit(writer) || !writer.good()) {
    reportRuntimeMetaFailure("serialize", count.size(), runtimeMetaCountLocked());
    return String();
  }
  if (writer.size() != count.size() || out.length() != count.size()) {
    reportRuntimeMetaFailure("length", count.size(), runtimeMetaCountLocked());
    return String();
  }
  return out;
}

void ConfigManagerRuntime::markRuntimeMetaJsonCacheDirtyLocked() {
  runtimeMetaJsonCacheDirty = true;
  ++runtimeMetaRevision;
}

std::shared_ptr<const String> ConfigManagerRuntime::runtimeMetaJsonPayload() {
  // A cache rebuild is single-flight. Other callers keep using the last valid
  // snapshot immediately instead of queueing behind a large serialization.
  {
    std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
    const auto cached = runtimeMetaJsonCache.lock();
    if (cached && !runtimeMetaJsonCacheDirty) {
      return cached;
    }
    if (runtimeMetaJsonCacheBuildInProgress) {
      return cached;
    }
    runtimeMetaJsonCacheBuildInProgress = true;
  }

  uint32_t builtRevision = 0;
  String candidate;
  {
    std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
    ++runtimeMetaSerializationBuildCount;
#endif
    candidate = buildRuntimeMetaJsonLocked();
    builtRevision = runtimeMetaRevision.load();
  }

  std::shared_ptr<const String> replacement;
  if (candidate.length() != 0) {
    replacement = std::make_shared<String>(std::move(candidate));
  }

  std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
  runtimeMetaJsonCacheBuildInProgress = false;
  auto cached = runtimeMetaJsonCache.lock();
  if (replacement && replacement->length() != 0) {
    if (runtimeMetaRevision.load() == builtRevision) {
      runtimeMetaJsonCache = replacement;
      runtimeMetaJsonCacheDirty = false;
      cached = replacement;
    } else if (!cached) {
      // The candidate is complete JSON for the most recent stable state
      // seen by this build. Keep it as a temporary fallback, but rebuild
      // again before treating it as current metadata.
      runtimeMetaJsonCache = replacement;
      cached = replacement;
    }
  }
  return cached;
}

bool ConfigManagerRuntime::hasRuntimeMetaView(const std::function<bool(const cm::RuntimeMetaView&)>& include) const {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  for (size_t i = 0; i < runtimeMetaCountLocked(); ++i)
    if (!include || include(runtimeMetaViewLocked(i)))
      return true;
  return false;
}
bool ConfigManagerRuntime::hasRuntimeMeta(const std::function<bool(const RuntimeFieldMeta&)>& include) const {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  for (size_t i = 0; i < runtimeMetaCountLocked(); ++i) {
    if (!include)
      return true;
#ifdef development
    if (runtimeMetaOverrideActive) {
      if (include(runtimeMetaOverride[i]))
        return true;
      continue;
    }
#endif
    if (!runtimeMetadataState) {
      if (include(runtimeMeta[i]))
        return true;
      continue;
    }
    const auto& r = runtimeMetadataState->order[i];
    const auto promoted = r.compactId ? runtimeMetadataState->store.promotedLegacyIndex(r.compactId) : std::numeric_limits<size_t>::max();
    if (!r.compactId || promoted != std::numeric_limits<size_t>::max()) {
      if (include(runtimeMeta[r.compactId ? promoted : r.legacyIndex]))
        return true;
    } else {
      RuntimeFieldMeta temporary;
      if (!runtimeMetadataState->store.materialize(r.compactId, temporary))
        return false;
      if (include(temporary))
        return true;
    }
  }
  return false;
}
String ConfigManagerRuntime::runtimeMetaToJSONView(const std::function<bool(const cm::RuntimeMetaView&)>& include) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  return buildRuntimeMetaJsonLocked({}, include);
}
String ConfigManagerRuntime::runtimeMetaToJSON(const std::function<bool(const RuntimeFieldMeta&)>& include) {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  return buildRuntimeMetaJsonLocked(include);
}
String ConfigManagerRuntime::runtimeMetaToJSON() {
  const auto payload = runtimeMetaJsonPayload();
  return payload ? *payload : String();
}

#ifdef development

void ConfigManagerRuntime::setRuntimeMetaOverride(const std::vector<RuntimeFieldMeta>& override) {
  std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
  std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
  runtimeMetaOverride = override;
  runtimeMetaOverrideActive = true;
  markRuntimeMetaJsonCacheDirtyLocked();
  logRuntimeMetaEvent("Meta override set", override.size());
}

void ConfigManagerRuntime::clearRuntimeMetaOverride() {
  std::lock_guard<std::mutex> cacheLock(runtimeMetaCacheMutex);
  std::lock_guard<std::mutex> dataLock(runtimeDataMutex);
  runtimeMetaOverride.clear();
  runtimeMetaOverrideActive = false;
  markRuntimeMetaJsonCacheDirtyLocked();
  logRuntimeMetaEvent("Meta override cleared", 0);
}

#endif

#ifdef CM_RUNTIME_META_TEST_INSTRUMENTATION
namespace {
struct StringCapacityAccess : String {
  static size_t ownedBytes(const String& s) {
    const auto small = &StringCapacityAccess::isSSO;
    const auto cap = &StringCapacityAccess::capacity;
    return (s.*small)() ? 0 : static_cast<size_t>((s.*cap)()) + 1;
  }
};
} // namespace
ConfigManagerRuntime::RuntimeMetaStatsForTest ConfigManagerRuntime::getRuntimeMetaStatsForTest() const {
  std::lock_guard<std::mutex> lock(runtimeDataMutex);
  RuntimeMetaStatsForTest result;
  result.legacyCapacityBytes = runtimeMeta.capacity() * sizeof(RuntimeFieldMeta);
  for (const auto& m : runtimeMeta) {
#define CM_OWNED(n) result.legacyOwnedBytes += StringCapacityAccess::ownedBytes(m.n)
    CM_OWNED(group);
    CM_OWNED(sourceGroup);
    CM_OWNED(page);
    CM_OWNED(key);
    CM_OWNED(label);
    CM_OWNED(unit);
    CM_OWNED(onLabel);
    CM_OWNED(offLabel);
    CM_OWNED(staticValue);
    CM_OWNED(card);
#undef CM_OWNED
    result.legacyOwnedBytes += m.style.rules.capacity() * sizeof(RuntimeStyleRule);
    for (const auto& r : m.style.rules) {
      result.legacyOwnedBytes += StringCapacityAccess::ownedBytes(r.target) + StringCapacityAccess::ownedBytes(r.className) + r.properties.capacity() * sizeof(RuntimeStyleProperty);
      for (const auto& p : r.properties)
        result.legacyOwnedBytes += StringCapacityAccess::ownedBytes(p.key) + StringCapacityAccess::ownedBytes(p.value);
    }
  }
  if (runtimeMetadataState) {
    result.compactStateAllocated = true;
    result.stateBytes = sizeof(RuntimeMetadataState);
    result.storeBytes = runtimeMetadataState->store.stats().retainedBytes;
    result.orderCapacityBytes = runtimeMetadataState->order.capacity() * sizeof(RuntimeMetadataState::Ref);
  }
  result.retainedBytes = result.legacyCapacityBytes + result.legacyOwnedBytes + result.stateBytes + result.orderCapacityBytes;
  if (runtimeMetadataState)
    result.retainedBytes += result.storeBytes - sizeof(cm::CompactRuntimeMetaStore);
  return result;
}
#endif
