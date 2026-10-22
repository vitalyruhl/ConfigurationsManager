#include "CompactHostString.h"
#include "runtime/detail/CompactRuntimeMetaStore.h"
#include "runtime/detail/RuntimeMetaJson.h"
#include <cassert>
#include <cstdio>
#include <iostream>
#include <type_traits>

struct FailingHeap {
  inline static bool fail = false;
  static void* allocate(size_t n) {
    return fail ? nullptr : std::malloc(n);
  }
  static void deallocate(void* p) {
    std::free(p);
  }
};
static void checkedGrowthFailure() {
  cm::detail::CheckedVector<String, FailingHeap> buffer;
  assert(buffer.emplace_back("retained"));
  FailingHeap::fail = true;
  assert(!buffer.reserve(100));
  assert(buffer.size() == 1 && buffer[0] == "retained");
  FailingHeap::fail = false;
  assert(buffer.reserve(100));
  assert(buffer[0] == "retained");
}
template <class T>
static void checkKind(cm::CompactRuntimeMetaStore& store, RuntimeFieldKind kind) {
  T def;
  def.base.key = "typed";
  auto id = store.add(def);
  assert(id && store.kind(id) == kind);
  auto view = store.view(id);
  String json;
  assert(cm::buildJsonOutput([&](auto& sink) { return cm::writeRuntimeMeta(sink, view); }, json));
  RuntimeFieldMeta legacy;
  assert(store.materialize(id, legacy));
  String oldJson;
  assert(cm::buildJsonOutput([&](auto& sink) { return cm::writeRuntimeMeta(sink, legacy); }, oldJson));
  assert(std::string(json.c_str(), json.length()) == std::string(oldJson.c_str(), oldJson.length()));
}
static void allKindsAndByteText() {
  cm::CompactRuntimeMetaStore store;
  checkKind<RuntimeFloatMeta>(store, RuntimeFieldKind::Float);
  checkKind<RuntimeIntMeta>(store, RuntimeFieldKind::Int);
  checkKind<RuntimeBoolMeta>(store, RuntimeFieldKind::Bool);
  checkKind<RuntimeTextMeta>(store, RuntimeFieldKind::Text);
  checkKind<RuntimeDividerMeta>(store, RuntimeFieldKind::Divider);
  checkKind<RuntimeButtonMeta>(store, RuntimeFieldKind::Button);
  checkKind<RuntimeCheckboxMeta>(store, RuntimeFieldKind::Checkbox);
  checkKind<RuntimeStateButtonMeta>(store, RuntimeFieldKind::StateButton);
  checkKind<RuntimeMomentaryButtonMeta>(store, RuntimeFieldKind::MomentaryButton);
  checkKind<RuntimeIntSliderMeta>(store, RuntimeFieldKind::IntSlider);
  checkKind<RuntimeIntInputMeta>(store, RuntimeFieldKind::IntInput);
  checkKind<RuntimeFloatSliderMeta>(store, RuntimeFieldKind::FloatSlider);
  checkKind<RuntimeFloatInputMeta>(store, RuntimeFieldKind::FloatInput);
  assert(store.view(RuntimeFieldId{2}).precision == 0);
  assert(store.view(RuntimeFieldId{3}).isBool && !store.view(RuntimeFieldId{3}).unit.length());
  RuntimeFloatMeta first;
  first.base.key = "nul";
  first.base.group = String("a\0b", 3);
  first.base.label = "Long UTF-8 \xc3\xa4 label to require allocation";
  auto a = store.add(first);
  first.base.group = String("a\0c", 3);
  auto b = store.add(first);
  assert(a && b && store.textCount() == 2);
  assert(!std::memcmp(store.view(a).group.c_str(), "a\0b", 3));
  assert(!std::memcmp(store.view(b).group.c_str(), "a\0c", 3));
  String output;
  assert(cm::buildJsonOutput([&](auto& sink) { return cm::writeRuntimeMeta(sink, store.view(a)); }, output));
  assert(std::string(output.c_str()).find("a\\u0000b") != std::string::npos);
  RuntimeStateLabels labels;
  labels.onLabel = "Enabled";
  labels.offLabel = "Disabled";
  assert(!store.setStateLabels(a, labels));
  assert(store.setStateLabels(RuntimeFieldId{3}, labels));
  RuntimeBoolAlarm alarm;
  alarm.hasAlarm = true;
  alarm.boolAlarmValue = true;
  assert(store.setBoolAlarm(RuntimeFieldId{3}, alarm));
  assert(store.view(RuntimeFieldId{3}).boolAlarmValue);
  assert(!store.setNumericAlarm(RuntimeFieldId{3}, RuntimeNumericAlarm{}));
}
static size_t legacyBytes(const std::vector<RuntimeFieldMeta>& fields) {
  size_t total = sizeof(fields) + fields.capacity() * sizeof(RuntimeFieldMeta);
  for (const auto& m : fields) {
    for (const auto* s : {&m.group, &m.sourceGroup, &m.page, &m.key, &m.label, &m.unit, &m.onLabel, &m.offLabel, &m.staticValue, &m.card})
      total += s->allocatedBytes();
    total += m.style.rules.capacity() * sizeof(RuntimeStyleRule);
    for (const auto& r : m.style.rules) {
      total += r.target.allocatedBytes() + r.className.allocatedBytes() + r.properties.capacity() * sizeof(RuntimeStyleProperty);
      for (const auto& p : r.properties)
        total += p.key.allocatedBytes() + p.value.allocatedBytes();
    }
  }
  return total;
}
static void allocationFixtures() {
  for (int fixture = 0; fixture < 3; ++fixture) {
    cm::CompactRuntimeMetaStore compact;
    std::vector<RuntimeFieldMeta> legacy;
    for (int i = 0; i < 64; ++i) {
      char key[12], label[12];
      std::snprintf(key, sizeof(key), "value%02d", i);
      std::snprintf(label, sizeof(label), "Value %02d", i);
      RuntimeFieldBase base;
      base.key = key;
      base.label = label;
      base.group = "telemetry";
      base.sourceGroup = "sensors";
      base.page = "Overview";
      base.card = "Values";
      base.order = i;
      RuntimeFieldMeta old;
      old.key = key;
      old.label = label;
      old.group = base.group;
      old.sourceGroup = base.sourceGroup;
      old.page = base.page;
      old.card = base.card;
      old.order = i;
      RuntimeFieldId id;
      const int kind = fixture == 0 ? 0 : fixture == 1 ? 2
                                                       : i % 13;
      switch (kind) {
        case 0: {
          RuntimeFloatMeta d;
          d.base = base;
          d.unit = "W";
          id = compact.add(d);
          old.unit = "W";
          break;
        }
        case 1: {
          RuntimeIntMeta d;
          d.base = base;
          d.unit = "count";
          id = compact.add(d);
          old.unit = "count";
          old.precision = 0;
          break;
        }
        case 2: {
          RuntimeBoolMeta d;
          d.base = base;
          id = compact.add(d);
          old.isBool = true;
          break;
        }
        case 3: {
          RuntimeTextMeta d;
          d.base = base;
          id = compact.add(d);
          old.isString = true;
          break;
        }
        case 4: {
          RuntimeDividerMeta d;
          d.base = base;
          id = compact.add(d);
          old.isDivider = true;
          break;
        }
        case 5: {
          RuntimeButtonMeta d;
          d.base = base;
          id = compact.add(d);
          old.isButton = true;
          break;
        }
        case 6: {
          RuntimeCheckboxMeta d;
          d.base = base;
          id = compact.add(d);
          old.isCheckbox = true;
          break;
        }
        case 7: {
          RuntimeStateButtonMeta d;
          d.base = base;
          d.initialState = true;
          id = compact.add(d);
          old.isStateButton = true;
          old.initialState = true;
          break;
        }
        case 8: {
          RuntimeMomentaryButtonMeta d;
          d.base = base;
          id = compact.add(d);
          old.isMomentaryButton = true;
          break;
        }
        case 9: {
          RuntimeIntSliderMeta d;
          d.base = base;
          d.unit = "A";
          id = compact.add(d);
          old.unit = "A";
          old.isIntSlider = true;
          old.precision = 0;
          break;
        }
        case 10: {
          RuntimeIntInputMeta d;
          d.base = base;
          d.unit = "A";
          id = compact.add(d);
          old.unit = "A";
          old.isIntInput = true;
          old.precision = 0;
          break;
        }
        case 11: {
          RuntimeFloatSliderMeta d;
          d.base = base;
          d.unit = "V";
          id = compact.add(d);
          old.unit = "V";
          old.isFloatSlider = true;
          break;
        }
        case 12: {
          RuntimeFloatInputMeta d;
          d.base = base;
          d.unit = "V";
          id = compact.add(d);
          old.unit = "V";
          old.isFloatInput = true;
          break;
        }
      }
      assert(id);
      if (fixture == 2 && i % 8 == 0) {
        old.style.rule("value").set("color", "orange");
        assert(compact.setStyle(id, old.style));
      }
      if (fixture == 2 && kind == 0) {
        RuntimeNumericAlarm alarm;
        alarm.hasAlarm = true;
        alarm.alarmMax = 100;
        assert(compact.setNumericAlarm(id, alarm));
        old.hasAlarm = true;
        old.alarmMax = 100;
      }
      if (fixture == 2 && kind == 2) {
        RuntimeStateLabels labels;
        labels.onLabel = "On";
        labels.offLabel = "Off";
        assert(compact.setStateLabels(id, labels));
        old.onLabel = "On";
        old.offLabel = "Off";
      }
      legacy.push_back(std::move(old));
    }
    const auto s = compact.stats();
    const auto before = legacyBytes(legacy);
    std::cout << "[I] Host requested retained capacity " << (fixture == 0 ? "64-Float" : fixture == 1 ? "64-Bool"
                                                                                                      : "64-Mixed")
              << ": legacy=" << before << ", compact=" << s.retainedBytes << ", pools=" << s.poolCapacityBytes << ", text=" << s.textCapacityBytes << ", handles=" << s.handleCapacityBytes << ", sparse=" << s.sparseCapacityBytes << ", store=" << sizeof(compact) << ", saving=" << (100.0 * (before - s.retainedBytes) / before) << "%. Excludes allocator bookkeeping and manager order refs.\n";
    if (fixture == 0)
      assert(s.retainedBytes * 4 <= before * 3);
  }
}
int main() {
  allocationFixtures();
  {
    cm::CompactRuntimeMetaStore promoted;
    RuntimeFloatMeta def;
    def.base.key = "An unusually long owned field key";
    def.base.label = "An unusually long owned label";
    def.base.group = "An unusually long owned routing group";
    auto id = promoted.add(def);
    assert(id);
    assert(promoted.promote(id, 0));
    assert(promoted.stats().poolCapacityBytes == 4 * sizeof(cm::detail::StoredRecord<cm::detail::FloatPayload>));
    assert(promoted.textCount() == 0);
  }
  checkedGrowthFailure();
  allKindsAndByteText();
  cm::CompactRuntimeMetaStore store;
  RuntimeFieldId first;
  {
    RuntimeFloatMeta def;
    def.base.key = "power";
    def.base.label = "Power";
    def.base.group = "live";
    def.base.page = "dashboard";
    def.unit = "W";
    first = store.add(def);
  }
  assert(first);
  auto v = store.view(first);
  assert(v.valid && v.key == "power" && v.group == "live" && v.unit == "W" && v.precision == 2);
  for (int i = 0; i < 80; ++i) {
    RuntimeBoolMeta d;
    d.base.key = "switch";
    d.base.group = "live";
    assert(store.add(d));
  }
  assert(store.view(first).key == "power");
  assert(store.textCount() == 3);
  assert(!store.update<RuntimeBoolMeta>(first, [](RuntimeBoolMeta&) {}));
  assert(!store.setBoolAlarm(first, RuntimeBoolAlarm{}));
  RuntimeNumericAlarm alarm;
  alarm.hasAlarm = true;
  alarm.alarmMax = 250;
  assert(store.setNumericAlarm(first, alarm));
  assert(store.view(first).alarmMax == 250);
  assert(store.update<RuntimeFloatMeta>(first, [](RuntimeFloatMeta& m) { m.unit = "kW"; m.precision = 3; }));
  assert(store.view(first).unit == "kW" && store.view(first).precision == 3);
  String::failAfter = 0;
  assert(!store.update<RuntimeFloatMeta>(first, [](RuntimeFloatMeta& m) { m.base.label = "A changed field label"; }));
  String::failAfter = -1;
  assert(store.view(first).label == "Power");
  RuntimeFieldStyle style;
  style.rule("value").set("color", "red");
  assert(store.setStyle(first, style));
  assert(store.view(first).style.rules[0].properties[0].value == "red");
  assert(store.setStyle(first, RuntimeFieldStyle{}));
  assert(store.view(first).style.empty());
  assert(store.setStaticText(first, String("a\0b", 3)));
  assert(store.view(first).staticValue.length() == 3);
  RuntimeFieldMeta legacy;
  assert(store.materialize(first, legacy));
  assert(legacy.unit == "kW" && legacy.alarmMax == 250);
  assert(store.applyLegacy(first, legacy));
  legacy.isCheckbox = true;
  assert(!store.applyLegacy(first, legacy));
  assert(store.promote(first, 7));
  assert(store.promotedLegacyIndex(first) == 7);
  assert(!store.view(first).valid && !store.update<RuntimeFloatMeta>(first, [](RuntimeFloatMeta&) {}));
  RuntimeFloatMeta replacement;
  replacement.base.key = "different";
  replacement.base.label = "Different long label";
  replacement.base.group = "new group";
  replacement.base.sourceGroup = "new source";
  replacement.base.page = "new page";
  replacement.base.card = "new card";
  replacement.unit = "new unit";
  for (int fail = 0; fail < 7; ++fail) {
    cm::CompactRuntimeMetaStore atomic;
    RuntimeFloatMeta original;
    original.base.key = "before";
    original.base.group = "old group";
    const auto id = atomic.add(original);
    assert(id);
    String::failAfter = fail;
    assert(!atomic.replace(id, replacement));
    String::failAfter = -1;
    const auto unchanged = atomic.view(id);
    assert(unchanged.key == "before" && unchanged.group == "old group" && unchanged.label.length() == 0);
    assert(atomic.textCount() == 1);
  }
  cm::CompactRuntimeMetaStore legacyAtomic;
  RuntimeBoolMeta flag;
  flag.base.key = "flag";
  auto flagId = legacyAtomic.add(flag);
  assert(flagId);
  RuntimeFieldMeta changed;
  assert(legacyAtomic.materialize(flagId, changed));
  changed.label = "A long new bool label";
  changed.onLabel = "Enabled";
  changed.offLabel = "Disabled";
  changed.style.rule("value").set("color", "green");
  changed.staticValue = "Static long value";
  assert(legacyAtomic.canApplyLegacy(flagId, changed));
  for (int fail = 0; fail < 8; ++fail) {
    String::failAfter = fail;
    assert(!legacyAtomic.applyLegacy(flagId, changed));
    String::failAfter = -1;
    auto unchanged = legacyAtomic.view(flagId);
    assert(unchanged.label.length() == 0 && unchanged.onLabel.length() == 0 && unchanged.style.empty());
  }
  assert(legacyAtomic.applyLegacy(flagId, changed));
  auto applied = legacyAtomic.view(flagId);
  assert(applied.label == changed.label && applied.onLabel == "Enabled");
  String result;
  assert(cm::buildJsonOutput([&](auto& sink) { return cm::writeRuntimeMeta(sink, applied); }, result));
  assert(std::string(result.c_str()).find("\"style\":{\"value\":{\"color\":\"green\"}}") != std::string::npos);
  RuntimeIntSliderMeta slider;
  auto sliderId = legacyAtomic.add(slider);
  RuntimeFieldMeta invalidSlider;
  assert(legacyAtomic.materialize(sliderId, invalidSlider));
  invalidSlider.intInit = 101;
  assert(!legacyAtomic.canApplyLegacy(sliderId, invalidSlider));
  RuntimeFieldMeta copied;
  assert(legacyAtomic.materialize(flagId, copied));
  assert(copied.style.rules[0].properties[0].value == "green");
  const auto beforePromote = legacyAtomic.retainedBytes();
  assert(legacyAtomic.promote(flagId, 4));
  assert(legacyAtomic.retainedBytes() < beforePromote);
  assert(!legacyAtomic.materialize(flagId, copied));
  assert(!legacyAtomic.setStyle(RuntimeFieldId{}, RuntimeFieldStyle{}));
  std::cout << "[I] Compact storage ownership, growth, updates, sparse extensions and promotion passed.\n";
}
