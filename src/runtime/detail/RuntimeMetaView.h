#pragma once
#include "../RuntimeFieldTypes.h"
#include <cstddef>

namespace cm {
struct RuntimePropertyView {
  const String& key;
  const String& value;
};
struct RuntimePropertiesView {
  const void* source = nullptr;
  size_t (*count)(const void*) = nullptr;
  RuntimePropertyView (*item)(const void*, size_t) = nullptr;
  size_t size() const {
    return source ? count(source) : 0;
  }
  RuntimePropertyView operator[](size_t i) const {
    return item(source, i);
  }
  template <class P>
  static RuntimePropertiesView from(const P& p) {
    return {&p, [](const void* s) { return static_cast<const P*>(s)->size(); }, [](const void* s, size_t i) { const auto& p = (*static_cast<const P*>(s))[i]; return RuntimePropertyView{p.key, p.value}; }};
  }
};
struct RuntimeRuleView {
  const String& target;
  RuntimePropertiesView properties;
  bool hasVisible, visible;
  const String& className;
};
struct RuntimeRulesView {
  const void* source = nullptr;
  size_t (*count)(const void*) = nullptr;
  RuntimeRuleView (*item)(const void*, size_t) = nullptr;
  size_t size() const {
    return source ? count(source) : 0;
  }
  RuntimeRuleView operator[](size_t i) const {
    return item(source, i);
  }
  template <class R>
  static RuntimeRulesView from(const R& r) {
    return {&r, [](const void* s) { return static_cast<const R*>(s)->size(); }, [](const void* s, size_t i) { const auto& r = (*static_cast<const R*>(s))[i]; return RuntimeRuleView{r.target, RuntimePropertiesView::from(r.properties), r.hasVisible, r.visible, r.className}; }};
  }
};
struct RuntimeStyleView {
  RuntimeRulesView rules;
  bool empty() const {
    return rules.size() == 0;
  }
  template <class S>
  static RuntimeStyleView from(const S& s) {
    return {RuntimeRulesView::from(s.rules)};
  }
};
// Views borrow locked metadata and never retain a String copy or JSON tree.
struct RuntimeMetaView {
  const String &group, &sourceGroup, &page, &key, &label, &unit, &onLabel, &offLabel, &staticValue, &card;
  RuntimeStyleView style;
  bool valid = false;
  int precision = 2, order = 100;
  bool isBool = false, isString = false, isDivider = false, isButton = false;
  bool isCheckbox = false, isStateButton = false, isMomentaryButton = false;
  bool isIntSlider = false, isFloatSlider = false, isIntInput = false, isFloatInput = false;
  bool hasAlarm = false, alarmWhenTrue = false, boolAlarmValue = false;
  float alarmMin = 0, alarmMax = 0, warnMin = 0, warnMax = 0;
  int intMin = 0, intMax = 100, intInit = 0;
  float floatMin = 0, floatMax = 1, floatInit = 0;
  int floatPrecision = 2;
  bool initialState = false;
  static const String& emptyString() {
    static const String value;
    return value;
  }
  RuntimeMetaView()
      : group(emptyString()), sourceGroup(emptyString()), page(emptyString()), key(emptyString()), label(emptyString()), unit(emptyString()), onLabel(emptyString()), offLabel(emptyString()), staticValue(emptyString()), card(emptyString()) {}
  RuntimeMetaView(const String& g, const String& s, const String& p, const String& k, const String& l, const String& u, const String& on, const String& off, const String& text, const String& c)
      : group(g), sourceGroup(s), page(p), key(k), label(l), unit(u), onLabel(on), offLabel(off), staticValue(text), card(c), valid(true) {}
  explicit RuntimeMetaView(const RuntimeFieldBase& b)
      : RuntimeMetaView(b.group, b.sourceGroup, b.page, b.key, b.label, emptyString(), emptyString(), emptyString(), emptyString(), b.card) {
    order = b.order;
  }
  explicit RuntimeMetaView(const RuntimeFieldMeta& m)
      : RuntimeMetaView(m.group, m.sourceGroup, m.page, m.key, m.label, m.unit, m.onLabel, m.offLabel, m.staticValue, m.card) {
    style = RuntimeStyleView::from(m.style);
#define CM_VIEW_COPY(n) n = m.n
    CM_VIEW_COPY(precision);
    CM_VIEW_COPY(order);
    CM_VIEW_COPY(isBool);
    CM_VIEW_COPY(isString);
    CM_VIEW_COPY(isDivider);
    CM_VIEW_COPY(isButton);
    CM_VIEW_COPY(isCheckbox);
    CM_VIEW_COPY(isStateButton);
    CM_VIEW_COPY(isMomentaryButton);
    CM_VIEW_COPY(isIntSlider);
    CM_VIEW_COPY(isFloatSlider);
    CM_VIEW_COPY(isIntInput);
    CM_VIEW_COPY(isFloatInput);
    CM_VIEW_COPY(hasAlarm);
    CM_VIEW_COPY(alarmWhenTrue);
    CM_VIEW_COPY(boolAlarmValue);
    CM_VIEW_COPY(alarmMin);
    CM_VIEW_COPY(alarmMax);
    CM_VIEW_COPY(warnMin);
    CM_VIEW_COPY(warnMax);
    CM_VIEW_COPY(intMin);
    CM_VIEW_COPY(intMax);
    CM_VIEW_COPY(intInit);
    CM_VIEW_COPY(floatMin);
    CM_VIEW_COPY(floatMax);
    CM_VIEW_COPY(floatInit);
    CM_VIEW_COPY(floatPrecision);
    CM_VIEW_COPY(initialState);
#undef CM_VIEW_COPY
  }
};
} // namespace cm
