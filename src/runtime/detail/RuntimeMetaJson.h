#pragma once
#include "../../web/JsonOutput.h"
#include <cstring>

namespace cm {
// JSON keys include their full byte length; Arduino String equality uses strcmp.
template <typename Left, typename Right>
bool runtimeJsonKeyEquals(const Left& left, const Right& right) {
  return left.length() == right.length() &&
         (!left.length() || std::memcmp(left.c_str(), right.c_str(), left.length()) == 0);
}
template <typename Left>
bool runtimeJsonKeyEquals(const Left& left, const char* right) {
  const size_t length = std::strlen(right);
  return left.length() == length &&
         (!length || std::memcmp(left.c_str(), right, length) == 0);
}

// Emit an existing rule without copying strings or building a JSON object.
// Duplicate properties retain their first position and last assigned value.
template <typename Sink, typename Rule>
bool writeRuntimeStyleRule(Sink& sink, const Rule& rule) {
  if (!sink.raw("{"))
    return false;
  bool first = true;
  const auto latestProperty = [&](const char* key) {
    size_t found = rule.properties.size();
    for (size_t i = 0; i < rule.properties.size(); ++i) {
      if (runtimeJsonKeyEquals(rule.properties[i].key, key))
        found = i;
    }
    return found;
  };
  if (rule.hasVisible) {
    const size_t replacement = latestProperty("visible");
    if (replacement != rule.properties.size()) {
      if (!sink.member(first, "visible", rule.properties[replacement].value))
        return false;
    } else if (!sink.member(first, "visible", rule.visible))
      return false;
  }
  if (rule.className.length()) {
    const size_t replacement = latestProperty("className");
    if (replacement != rule.properties.size()) {
      if (!sink.member(first, "className", rule.properties[replacement].value))
        return false;
    } else if (!sink.member(first, "className", rule.className))
      return false;
  }
  for (size_t i = 0; i < rule.properties.size(); ++i) {
    const auto& property = rule.properties[i];
    if (!property.key.length() || (rule.hasVisible && runtimeJsonKeyEquals(property.key, "visible")) ||
        (rule.className.length() && runtimeJsonKeyEquals(property.key, "className")))
      continue;
    bool earlier = false;
    for (size_t j = 0; j < i; ++j) {
      if (runtimeJsonKeyEquals(rule.properties[j].key, property.key)) {
        earlier = true;
        break;
      }
    }
    if (earlier)
      continue;
    size_t last = i;
    for (size_t j = i + 1; j < rule.properties.size(); ++j) {
      if (runtimeJsonKeyEquals(rule.properties[j].key, property.key))
        last = j;
    }
    if (!sink.member(first, property.key, rule.properties[last].value))
      return false;
  }
  return sink.raw("}");
}

template <typename Sink, typename Style>
bool writeRuntimeStyle(Sink& sink, const Style& style) {
  if (!sink.raw("{"))
    return false;
  bool first = true;
  for (size_t i = 0; i < style.rules.size(); ++i) {
    const auto& rule = style.rules[i];
    if (!rule.target.length())
      continue;
    bool earlier = false;
    for (size_t j = 0; j < i; ++j) {
      if (runtimeJsonKeyEquals(style.rules[j].target, rule.target)) {
        earlier = true;
        break;
      }
    }
    if (earlier)
      continue;
    size_t last = i;
    for (size_t j = i + 1; j < style.rules.size(); ++j) {
      if (runtimeJsonKeyEquals(style.rules[j].target, rule.target))
        last = j;
    }
    // ArduinoJson's to<JsonObject>() replaces the complete repeated target.
    if ((!first && !sink.raw(",")) || !sink.string(rule.target) || !sink.raw(":"))
      return false;
    first = false;
    if (!writeRuntimeStyleRule(sink, style.rules[last]))
      return false;
  }
  return sink.raw("}");
}

// The caller holds runtimeDataMutex for the complete count/write operation.
// Keep omission rules and first-insertion ordering identical to the old tree.
template <typename Sink, typename Meta>
bool writeRuntimeMeta(Sink& sink, const Meta& m) {
  if (!sink.raw("{"))
    return false;
  bool first = true;
  if (!sink.member(first, "group", m.group))
    return false;
  if (m.sourceGroup.length() && !sink.member(first, "sourceGroup", m.sourceGroup))
    return false;
  if (m.page.length() && !sink.member(first, "page", m.page))
    return false;
  if (m.card.length() && !sink.member(first, "card", m.card))
    return false;
  if (!sink.member(first, "key", m.key) || !sink.member(first, "label", m.label))
    return false;
  if (m.unit.length() && !sink.member(first, "unit", m.unit))
    return false;
  if (!sink.member(first, "precision", m.precision))
    return false;
  if (m.isBool && !sink.member(first, "isBool", true))
    return false;
  if (m.isString && !sink.member(first, "isString", true))
    return false;
  if (m.isDivider && !sink.member(first, "isDivider", true))
    return false;
  if (m.isButton && !sink.member(first, "isButton", true))
    return false;
  if (m.isCheckbox && !sink.member(first, "isCheckbox", true))
    return false;
  if (m.isStateButton && !sink.member(first, "isStateButton", true))
    return false;
  if (m.isMomentaryButton && !sink.member(first, "isMomentaryButton", true))
    return false;
  bool rangeWritten = false;
  const auto writeRange = [&]() {
    if (rangeWritten)
      return true;
    rangeWritten = true;
    // Later flag groups overwrite the values without moving their first keys.
    const bool floating = m.isFloatInput || (!m.isIntInput && m.isFloatSlider);
    if (floating)
      return sink.member(first, "min", m.floatMin) &&
             sink.member(first, "max", m.floatMax) && sink.member(first, "init", m.floatInit);
    return sink.member(first, "min", m.intMin) &&
           sink.member(first, "max", m.intMax) && sink.member(first, "init", m.intInit);
  };
  if (m.isIntSlider && (!sink.member(first, "isIntSlider", true) || !writeRange()))
    return false;
  if (m.isFloatSlider && (!sink.member(first, "isFloatSlider", true) || !writeRange()))
    return false;
  if (m.isIntInput && (!sink.member(first, "isIntInput", true) || !writeRange()))
    return false;
  if (m.isFloatInput && (!sink.member(first, "isFloatInput", true) || !writeRange()))
    return false;
  if (m.hasAlarm && !sink.member(first, "hasAlarm", m.hasAlarm))
    return false;
  if (m.alarmWhenTrue && !sink.member(first, "alarmWhenTrue", m.alarmWhenTrue))
    return false;
  if (m.boolAlarmValue && !sink.member(first, "boolAlarmValue", m.boolAlarmValue))
    return false;
  if ((m.alarmMin != 0.0f || m.alarmMax != 0.0f) &&
      (!sink.member(first, "alarmMin", m.alarmMin) || !sink.member(first, "alarmMax", m.alarmMax)))
    return false;
  if ((m.warnMin != 0.0f || m.warnMax != 0.0f) &&
      (!sink.member(first, "warnMin", m.warnMin) || !sink.member(first, "warnMax", m.warnMax)))
    return false;
  if (!sink.member(first, "order", m.order))
    return false;
  if (m.staticValue.length() && !sink.member(first, "staticValue", m.staticValue))
    return false;
  if (m.onLabel.length() && !sink.member(first, "onLabel", m.onLabel))
    return false;
  if (m.offLabel.length() && !sink.member(first, "offLabel", m.offLabel))
    return false;
  if (!m.style.empty()) {
    if ((!first && !sink.raw(",")) || !sink.raw("\"style\":"))
      return false;
    if (!writeRuntimeStyle(sink, m.style))
      return false;
  }
  return sink.raw("}");
}
} // namespace cm
