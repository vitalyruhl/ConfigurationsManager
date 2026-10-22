#pragma once
#include <ArduinoJson.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace cm {
// Exact source-group names; explicit empty is distinct from the legacy full snapshot.
struct RuntimeSelection {
  static constexpr size_t maxGroups = 32;
  static constexpr size_t maxGroupBytes = 64;
  static constexpr size_t maxPayloadBytes = 2048;
  static constexpr size_t maxClients = 8;
  bool all = false;
  std::vector<std::string> groups;

  static RuntimeSelection everything() {
    RuntimeSelection result;
    result.all = true;
    return result;
  }
  bool contains(const char* name) const {
    return all || std::find(groups.begin(), groups.end(), name) != groups.end();
  }
  void merge(const RuntimeSelection& other) {
    if (all)
      return;
    if (other.all) {
      all = true;
      groups.clear();
      return;
    }
    for (const auto& group : other.groups) {
      if (!contains(group.c_str()) && groups.size() < maxClients * maxGroups)
        groups.push_back(group);
    }
  }
};

inline bool parseRuntimeSelection(JsonVariantConst value, RuntimeSelection& target) {
  if (!value.is<JsonArrayConst>())
    return false;
  const auto array = value.as<JsonArrayConst>();
  if (array.size() > RuntimeSelection::maxGroups)
    return false;
  RuntimeSelection parsed;
  for (JsonVariantConst item : array) {
    if (!item.is<const char*>())
      return false;
    const auto text = item.as<JsonString>();
    const size_t length = text.size();
    if (!length || length > RuntimeSelection::maxGroupBytes || std::strlen(text.c_str()) != length)
      return false;
    for (size_t i = 0; i < length; ++i)
      if (static_cast<unsigned char>(text.c_str()[i]) < 32)
        return false;
    if (!parsed.contains(text.c_str()))
      parsed.groups.emplace_back(text.c_str(), length);
  }
  target = std::move(parsed);
  return true;
}

inline bool parseRuntimeSelection(const std::string& json, RuntimeSelection& target) {
  if (json.size() > RuntimeSelection::maxPayloadBytes)
    return false;
  JsonDocument document;
  if (deserializeJson(document, json, DeserializationOption::NestingLimit(2)))
    return false;
  return parseRuntimeSelection(document.as<JsonVariantConst>(), target);
}
inline bool parseRuntimeSelection(const char* json, RuntimeSelection& target) {
  if (!json || std::strlen(json) > RuntimeSelection::maxPayloadBytes)
    return false;
  return parseRuntimeSelection(std::string(json), target);
}
} // namespace cm
