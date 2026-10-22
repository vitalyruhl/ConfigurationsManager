#pragma once
#include "RuntimeSelection.h"

namespace cm {
template <typename Entry, typename Group>
std::vector<Entry> selectRuntimeEntries(const std::vector<Entry>& entries, const RuntimeSelection& selection, Group group) {
  std::vector<Entry> selected;
  for (const auto& entry : entries)
    if (selection.contains(group(entry)))
      selected.push_back(entry);
  return selected;
}

template <typename Provider>
void fillRuntimeProviders(JsonObject root, std::vector<Provider>& providers) {
  std::stable_sort(providers.begin(), providers.end(), [](const auto& a, const auto& b) { return a.order < b.order; });
  for (auto& provider : providers) {
    JsonObject slot = root[provider.name.c_str()].template is<JsonObject>()
                        ? root[provider.name.c_str()].template as<JsonObject>()
                        : root[provider.name.c_str()].template to<JsonObject>();
    if (provider.fill)
      provider.fill(slot);
  }
}

template <typename Control>
void fillRuntimeControls(JsonObject root, const std::vector<Control>& controls) {
  for (const auto& control : controls) {
    if (!control.getter)
      continue;
    JsonObject slot = root[control.group.c_str()].template is<JsonObject>()
                        ? root[control.group.c_str()].template as<JsonObject>()
                        : root[control.group.c_str()].template to<JsonObject>();
    slot[control.key.c_str()] = control.getter();
  }
}

inline void filterRuntimeSnapshot(JsonObject target, JsonObjectConst source, const RuntimeSelection& selection) {
  for (JsonPairConst entry : source) {
    const char* key = entry.key().c_str();
    if (selection.contains(key) || std::strcmp(key, "uptime") == 0)
      target[key] = entry.value();
  }
  // Safety state does not depend on dashboard visibility or system telemetry selection.
  for (const char* key : {"otaActive", "otaHasPassword"}) {
    if (!source["system"][key].isNull())
      target["system"][key] = source["system"][key];
  }
}
} // namespace cm
