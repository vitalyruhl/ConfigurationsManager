#pragma once

#include "../RuntimeManager.h"
#include <type_traits>

namespace cm {
namespace detail {
// Registration is independent of hardware sampling and preserves existing overrides.
inline void registerBuiltinSystemMetadata(ConfigManagerRuntime& runtime, bool withTime) {
  auto upsertMeta = [&runtime](auto meta, const char* key, const char* label, const char* unit, int order) {
    using T = decltype(meta);
    const bool updated = runtime.updateRuntimeMeta("system", key, [&](RuntimeFieldMeta& existing) {
      existing.order = order;
      existing.label = label;
      if (unit[0] && existing.unit.isEmpty())
        existing.unit = unit;
      if constexpr (std::is_same<T, RuntimeBoolMeta>::value)
        existing.isBool = true;
      if constexpr (std::is_same<T, RuntimeTextMeta>::value)
        existing.isString = true;
      existing.precision = 0;
    });
    // A rejected checked update must not turn an existing field into a duplicate.
    if (updated || runtime.hasRuntimeMetaView([&](const RuntimeMetaView& field) {
          return field.key == key && (field.group == "system" || field.sourceGroup == "system");
        }))
      return;

    meta.base.group = "system";
    meta.base.key = key;
    meta.base.label = label;
    meta.base.order = order;
    if constexpr (std::is_same<T, RuntimeIntMeta>::value)
      meta.unit = unit;
    // Publish historical precision atomically with the compact field. The flag
    // occupies existing handle padding; no public Bool/Text member is needed.
    runtime.addCompactRuntimeMeta(meta, !std::is_same<T, RuntimeIntMeta>::value);
  };

  // Orders 0-2 remain available to application identity fields.
  upsertMeta(RuntimeBoolMeta{}, "wifiConnected", "Wifi Connected", "", 10);
  upsertMeta(RuntimeIntMeta{}, "channel", "WiFi Channel", "", 11);
  upsertMeta(RuntimeTextMeta{}, "rssiTxt", "Signal", "", 12);
  upsertMeta(RuntimeIntMeta{}, "rssi", "WiFi RSSI", "dBm", 13);
  upsertMeta(RuntimeTextMeta{}, "localIP", "Local IP", "", 14);
  upsertMeta(RuntimeTextMeta{}, "gateway", "Gateway", "", 15);
  upsertMeta(RuntimeTextMeta{}, "routerMAC", "Router MAC", "", 16);
  upsertMeta(RuntimeBoolMeta{}, "allowOTA", "AllowOTA", "", 20);
  upsertMeta(RuntimeBoolMeta{}, "otaActive", "OtaActive", "", 21);
  if (withTime)
    upsertMeta(RuntimeTextMeta{}, "dateTime", "Date/Time", "", 22);

  upsertMeta(RuntimeIntMeta{}, "cpuFreqMHz", "CpuFreqMHz", "", 30);
  upsertMeta(RuntimeIntMeta{}, "flashSize", "FlashSize", "", 31);
  upsertMeta(RuntimeIntMeta{}, "sketchSize", "SketchSize", "", 32);
  upsertMeta(RuntimeIntMeta{}, "freeSketchSpace", "FreeSketchSpace", "", 33);
  upsertMeta(RuntimeIntMeta{}, "heapFragmentation", "HeapFragmentation", "", 34);
  upsertMeta(RuntimeIntMeta{}, "totalHeap", "TotalHeap", "", 35);
  upsertMeta(RuntimeIntMeta{}, "usedHeap", "UsedHeap", "", 36);
  // Preserve an existing unit override; the sampled value remains bytes.
  upsertMeta(RuntimeIntMeta{}, "freeHeap", "FreeHeap", "", 37);
}
} // namespace detail
} // namespace cm
