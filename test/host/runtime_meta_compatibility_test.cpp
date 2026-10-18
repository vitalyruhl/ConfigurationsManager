#include "CompactHostString.h"
#include "runtime/RuntimeManager.h"
#include <ArduinoJson.h>
#include <cassert>
#include <iostream>
#include <thread>
#include <atomic>

// [MOCKED!] Hardware integration hooks only; all registry/cache code is production.
class ConfigManagerClass {};
static size_t placements = 0;
static ConfigManagerRuntime* placementRuntime = nullptr;
void ConfigManagerRuntime::registerRuntimePlacement(const RuntimeFieldMeta&) {
  ++placements;
  if (placementRuntime)
    assert(placementRuntime->hasRuntimeMeta());
}
void ConfigManagerRuntime::registerRuntimePlacement(const RuntimeFieldBase&) {
  ++placements;
  if (placementRuntime)
    assert(placementRuntime->hasRuntimeMeta());
}
void ConfigManagerRuntime::reportRuntimeMetaFailure(const char*, size_t, size_t) const {}
void ConfigManagerRuntime::logRuntimeMetaAdded(const String&, const String&) const {}
void ConfigManagerRuntime::logRuntimeMetaEvent(const char*, size_t) const {}

static RuntimeFieldMeta field(const char* label, int order) {
  RuntimeFieldMeta m;
  m.group = "sensors";
  m.page = "Sensors";
  m.card = "Values";
  m.key = "value";
  m.label = label;
  m.order = order;
  m.unit = "C";
  return m;
}
static void legacyLookupAndCache() {
  ConfigManagerRuntime rt;
  rt.addRuntimeMeta(field("Z", 50));
  rt.addRuntimeMeta(field("A", 10));
  assert(rt.findRuntimeMeta("sensors", "value")->label == "Z");
  auto first = rt.runtimeMetaJsonPayload();
  assert(first && first->length());
  assert(rt.findRuntimeMeta("sensors", "value")->label == "A");
  assert(rt.runtimeMetaJsonPayload() == first);
  assert(rt.updateRuntimeMeta("sensors", "value", [](RuntimeFieldMeta& m) { m.label = "Changed"; }));
  auto second = rt.runtimeMetaJsonPayload();
  assert(second && first != second);
  assert(std::string(first->c_str()).find("Changed") == std::string::npos);
  assert(std::string(second->c_str()).find("Changed") != std::string::npos);
  JsonDocument json;
  assert(!deserializeJson(json, second->c_str()));
  assert(json.size() == 2);
  assert(json[0]["sourceGroup"] == "sensors");
  assert(json[0]["label"] == "Changed");
  auto filtered = rt.runtimeMetaToJSON([](const RuntimeFieldMeta& m) { return m.order == 50; });
  assert(!deserializeJson(json, filtered.c_str()));
  assert(json.size() == 1);
  assert(json[0]["label"] == "Z");
  RuntimeFieldMeta alias;
  alias.group = "alias";
  alias.key = "key";
  alias.label = "Alias";
  rt.addRuntimeMeta(alias);
  assert(rt.findRuntimeMeta("alias", "key"));
  assert(rt.findRuntimeMeta("alias", "key")->group.isEmpty());
  assert(!rt.findRuntimeMeta("missing", "key"));
}
static void fieldsAndOverrides() {
  ConfigManagerRuntime rt;
  for (int kind = 0; kind < 13; ++kind) {
    RuntimeFieldMeta m = field("Escaped \"\\\n\xc3\xa4", kind);
    m.isBool = kind == 0;
    m.isString = kind == 1;
    m.isDivider = kind == 2;
    m.isButton = kind == 3;
    m.isCheckbox = kind == 4;
    m.isStateButton = kind == 5;
    m.isMomentaryButton = kind == 6;
    m.isIntSlider = kind == 7;
    m.isFloatSlider = kind == 8;
    m.isIntInput = kind == 9;
    m.isFloatInput = kind == 10;
    m.hasAlarm = kind == 11;
    m.alarmMax = kind == 11 ? 100 : 0;
    m.alarmWhenTrue = kind == 12;
    m.onLabel = "On";
    m.offLabel = "Off";
    m.staticValue = "Static";
    m.style.rule("values").set("color", "red");
    rt.addRuntimeMeta(m);
  }
  JsonDocument json;
  auto text = rt.runtimeMetaToJSON();
  assert(!deserializeJson(json, text.c_str()));
  assert(json.size() == 13);
  assert(json[0]["isBool"] == true);
  assert(json[7]["isIntSlider"] == true);
  assert(json[8]["isFloatSlider"] == true);
  assert(json[11]["alarmMax"] == 100);
  assert(json[12]["alarmWhenTrue"] == true);
  assert(json[0]["style"]["values"]["color"] == "red");
#ifdef development
  std::vector<RuntimeFieldMeta> replacement{field("Override", 1)};
  rt.setRuntimeMetaOverride(replacement);
  text = rt.runtimeMetaToJSON();
  assert(!deserializeJson(json, text.c_str()));
  assert(json.size() == 1);
  assert(json[0]["label"] == "Override");
  rt.clearRuntimeMetaOverride();
  text = rt.runtimeMetaToJSON();
  assert(!deserializeJson(json, text.c_str()));
  assert(json.size() == 13);
#endif
}
static void concurrentMutation() {
  ConfigManagerRuntime rt;
  rt.addRuntimeMeta(field("Initial", 0));
  std::atomic<bool> good{true};
  std::thread mutate([&] {for(int i=0;i<100;++i) rt.updateRuntimeMeta("sensors","value",[&](RuntimeFieldMeta& m){m.order=i;}); });
  for (int i = 0; i < 100; ++i) {
    auto text = rt.runtimeMetaToJSON();
    JsonDocument parsed;
    if (deserializeJson(parsed, text.c_str()) || parsed.size() != 1)
      good = false;
  }
  mutate.join();
  assert(good);
}
template <class T>
static void assertEquivalentKind(T typed, RuntimeFieldMeta legacy) {
  ConfigManagerRuntime compact, old;
  typed.base.key = "key";
  typed.base.label = "Escaped \"\\\n\xc3\xa4";
  typed.base.group = "group";
  typed.base.page = "Page";
  typed.base.card = "Card";
  typed.base.order = 17;
  legacy.key = typed.base.key;
  legacy.label = typed.base.label;
  legacy.group = typed.base.group;
  legacy.page = typed.base.page;
  legacy.card = typed.base.card;
  legacy.order = typed.base.order;
  assert(compact.addRuntimeMeta(typed));
  old.addRuntimeMeta(legacy);
  assert(compact.runtimeMetaToJSON() == old.runtimeMetaToJSON());
}
static void allKindWireCompatibility() {
  assertEquivalentKind(RuntimeFloatMeta{}, RuntimeFieldMeta{});
  RuntimeFieldMeta legacy;
  legacy.precision = 0;
  assertEquivalentKind(RuntimeIntMeta{}, legacy);
#define CM_EQ_KIND(type, flag) \
  legacy = RuntimeFieldMeta{}; \
  legacy.flag = true;          \
  assertEquivalentKind(type{}, legacy)
  CM_EQ_KIND(RuntimeBoolMeta, isBool);
  CM_EQ_KIND(RuntimeTextMeta, isString);
  CM_EQ_KIND(RuntimeDividerMeta, isDivider);
  CM_EQ_KIND(RuntimeButtonMeta, isButton);
  CM_EQ_KIND(RuntimeCheckboxMeta, isCheckbox);
  CM_EQ_KIND(RuntimeStateButtonMeta, isStateButton);
  CM_EQ_KIND(RuntimeMomentaryButtonMeta, isMomentaryButton);
#undef CM_EQ_KIND
  legacy = RuntimeFieldMeta{};
  legacy.isIntSlider = true;
  legacy.precision = 0;
  assertEquivalentKind(RuntimeIntSliderMeta{}, legacy);
  legacy = RuntimeFieldMeta{};
  legacy.isIntInput = true;
  legacy.precision = 0;
  assertEquivalentKind(RuntimeIntInputMeta{}, legacy);
  legacy = RuntimeFieldMeta{};
  legacy.isFloatSlider = true;
  assertEquivalentKind(RuntimeFloatSliderMeta{}, legacy);
  legacy = RuntimeFieldMeta{};
  legacy.isFloatInput = true;
  assertEquivalentKind(RuntimeFloatInputMeta{}, legacy);
}
static void failedChangesRemainAtomic() {
  ConfigManagerRuntime rt;
  RuntimeFloatMeta f;
  f.base.key = "key";
  f.base.group = "group";
  f.base.label = "Before";
  String::failAfter = 0;
  assert(!rt.addRuntimeMeta(f));
  String::failAfter = -1;
  assert(!rt.hasRuntimeMeta());
  assert(!rt.getRuntimeMetaStatsForTest().compactStateAllocated);
  const auto id = rt.addRuntimeMeta(f);
  assert(id);
  auto before = rt.runtimeMetaJsonPayload();
  assert(before);
  String::failAfter = 0;
  assert(!rt.updateRuntimeMeta(id, std::function<void(RuntimeFloatMeta&)>([](RuntimeFloatMeta& m) { m.base.label = "After"; })));
  assert(!rt.setRuntimeStaticText(id, "failed text"));
  String::failAfter = -1;
  assert(rt.runtimeMetaJsonPayload() == before);
  String::failAfter = 0;
  assert(!rt.findRuntimeMeta("group", "key"));
  String::failAfter = -1;
  assert(rt.runtimeMetaJsonPayload() == before);
  assert(rt.updateRuntimeMeta(id, std::function<void(RuntimeFloatMeta&)>([](RuntimeFloatMeta&) {})));
}
static void placementCallbacksBorrowOneBase() {
  ConfigManagerRuntime rt;
  ConfigManagerClass hardware;
  auto legacy = field("Legacy", 0);
  legacy.style.rule("values").set("color", "red");
  rt.addRuntimeMeta(legacy);
  RuntimeBoolMeta compact;
  compact.base.key = "compact";
  assert(rt.addRuntimeMeta(compact));
  placements = 0;
  placementRuntime = &rt;
  RuntimeStyleRule::resetCopyCount();
  rt.begin(&hardware);
  assert(placements == 2);
  assert(RuntimeStyleRule::getCopyCount() == 0);
  compact.base.key = "later";
  assert(rt.addRuntimeMeta(compact));
  assert(placements == 3);
  placementRuntime = nullptr;
}
static void managerAccounting(int mode) {
  ConfigManagerRuntime legacy, compact;
  assert(!legacy.getRuntimeMetaStatsForTest().compactStateAllocated);
  for (int i = 0; i < 64; ++i) {
    RuntimeFloatMeta f;
    f.base.key = String((std::string("temperature_sensor_") + std::to_string(i)).c_str());
    f.base.label = "Room temperature sensor";
    f.base.group = "Measurements";
    f.base.sourceGroup = "telemetry";
    f.base.page = "Environment";
    f.base.card = "Measured values";
    f.unit = "degrees Celsius";
    f.precision = 1;
    const bool boolean = mode == 1 || (mode == 2 && i % 2);
    if (boolean) {
      RuntimeBoolMeta b;
      b.base = f.base;
      assert(compact.addRuntimeMeta(b));
    } else {
      assert(compact.addRuntimeMeta(f));
    }
    RuntimeFieldMeta m;
    m.key = f.base.key;
    m.label = f.base.label;
    m.group = f.base.group;
    m.sourceGroup = f.base.sourceGroup;
    m.page = f.base.page;
    m.card = f.base.card;
    m.unit = f.unit;
    m.precision = f.precision;
    if (boolean) {
      m.isBool = true;
      m.unit = "";
      m.precision = 2;
    }
    legacy.addRuntimeMeta(m);
  }
  const auto a = legacy.getRuntimeMetaStatsForTest(), b = compact.getRuntimeMetaStatsForTest();
  assert(!a.compactStateAllocated);
  assert(b.compactStateAllocated);
  assert(b.retainedBytes < a.retainedBytes * 3 / 4);
  const char* kind = mode == 0 ? "Float" : mode == 1 ? "Bool" : "mixed Float/Bool";
  std::cout << "[I] Actual manager 64-" << kind << " host retained bytes: legacy=" << a.retainedBytes << ", compact=" << b.retainedBytes << ", order=" << b.orderCapacityBytes << ", state=" << b.stateBytes << ". Host ABI only.\n";
}
static void compactCompatibility() {
  ConfigManagerRuntime rt;
  RuntimeFloatMeta f;
  f.base.key = "value";
  f.base.label = "Compact Z";
  f.base.group = "sensors";
  f.base.page = "Sensors";
  f.base.card = "Values";
  f.base.order = 50;
  f.unit = "C";
  const auto id = rt.addRuntimeMeta(f);
  assert(id);
  rt.addRuntimeMeta(field("Legacy A", 10));
  // Legacy callbacks preserve common registration order until serialization sorts it.
  assert(rt.updateRuntimeMeta("sensors", "value", [](RuntimeFieldMeta& m) {assert(m.label=="Compact Z");m.label="Compact Changed";m.hasAlarm=true;m.alarmMax=80; }));
  assert(rt.updateRuntimeMeta(id, std::function<void(RuntimeFloatMeta&)>([](RuntimeFloatMeta& m) {assert(m.base.label=="Compact Changed");m.precision=1; })));
  RuntimeBoolMeta b;
  b.base = f.base;
  b.base.key = "enabled";
  b.base.label = "Bool";
  const auto bid = rt.addRuntimeMeta(b);
  assert(bid);
  assert(!rt.setRuntimeNumericAlarm(bid, {}));
  assert(rt.setRuntimeBoolAlarm(bid, {true, true, true}));
  assert(rt.setRuntimeStateLabels(bid, {"Enabled", "Disabled"}));
  RuntimeFieldStyle style;
  style.rule("values").set("color", "red");
  assert(rt.setRuntimeFieldStyle(id, style));
  String dynamic("temporary text");
  assert(rt.setRuntimeStaticText(id, dynamic));
  dynamic = "changed";
  RuntimeStyleRule::resetCopyCount();
  auto payload = rt.runtimeMetaJsonPayload();
  assert(payload);
  assert(RuntimeStyleRule::getCopyCount() == 0);
  assert(rt.runtimeMetaJsonPayload() == payload);
  JsonDocument json;
  assert(!deserializeJson(json, payload->c_str()));
  assert(json.size() == 3);
  assert(json[0]["label"] == "Legacy A");
  assert(json[1]["key"] == "enabled");
  assert(json[2]["precision"] == 1);
  assert(json[2]["alarmMax"] == 80);
  assert(json[2]["staticValue"] == "temporary text");
  assert(json[1]["isBool"] == true);
  assert(json[1]["onLabel"] == "Enabled");
  assert(rt.findRuntimeMeta("sensors", "value")->label == "Legacy A");
  RuntimeStyleRule::resetCopyCount();
  auto selected = rt.runtimeMetaToJSONView([](const cm::RuntimeMetaView& m) { return m.key == "value"; });
  assert(!deserializeJson(json, selected.c_str()));
  assert(json.size() == 2);
  assert(RuntimeStyleRule::getCopyCount() == 0);
  assert(rt.hasRuntimeMetaView([](const cm::RuntimeMetaView& m) { return m.page == "Sensors"; }));
  // Unsupported legacy combinations promote exactly this compact handle.
  assert(rt.updateRuntimeMeta("sensors", "enabled", [](RuntimeFieldMeta& m) { m.unit = "legacy-only"; }));
  assert(!rt.updateRuntimeMeta(bid, std::function<void(RuntimeBoolMeta&)>([](RuntimeBoolMeta&) {})));
  auto* promoted = rt.findRuntimeMeta("sensors", "enabled");
  assert(promoted && promoted->unit == "legacy-only");
  assert(rt.findRuntimeMeta("sensors", "enabled") == promoted);
  // Mutable lookup promotes a compact field; future growth cannot alias its handle.
  RuntimeTextMeta t;
  t.base.key = "text";
  t.base.group = "other";
  const auto tid = rt.addRuntimeMeta(t);
  assert(tid);
  assert(rt.findRuntimeMeta("other", "text"));
  assert(!rt.updateRuntimeMeta(tid, std::function<void(RuntimeTextMeta&)>([](RuntimeTextMeta&) {})));
  for (int i = 0; i < 80; ++i) {
    RuntimeIntMeta n;
    n.base.key = String((std::string("number") + std::to_string(i)).c_str());
    assert(rt.addRuntimeMeta(n));
  }
  assert(!rt.updateRuntimeMeta(tid, std::function<void(RuntimeTextMeta&)>([](RuntimeTextMeta&) {})));
  assert(rt.updateRuntimeMeta(id, std::function<void(RuntimeFloatMeta&)>([](RuntimeFloatMeta& m) { m.base.label = "New"; })));
  auto newer = rt.runtimeMetaJsonPayload();
  assert(newer != payload);
  assert(std::string(payload->c_str()).find("Compact Changed") != std::string::npos);
#ifdef development
  rt.setRuntimeMetaOverride({field("Only override", 1)});
  auto overridden = rt.runtimeMetaToJSON();
  assert(!deserializeJson(json, overridden.c_str()));
  assert(json.size() == 1);
  rt.clearRuntimeMetaOverride();
#endif
  std::atomic<bool> good{true};
  std::thread update([&] {for(int i=0;i<100;++i)if(!rt.updateRuntimeMeta(id,std::function<void(RuntimeFloatMeta&)>([&](RuntimeFloatMeta&m){m.base.order=i;})))good=false; });
  for (int i = 0; i < 100; ++i) {
    auto text = rt.runtimeMetaToJSON();
    if (deserializeJson(json, text.c_str()))
      good = false;
  }
  update.join();
  assert(good);
  ConfigManagerRuntime weak;
  weak.addRuntimeMeta(f);
  {
    auto value = weak.runtimeMetaJsonPayload();
    assert(value);
  }
  auto count = weak.getRuntimeMetaSerializationBuildCountForTest();
  assert(weak.runtimeMetaJsonPayload());
  assert(weak.getRuntimeMetaSerializationBuildCountForTest() == count + 1);
}
int main() {
  placementCallbacksBorrowOneBase();
  allKindWireCompatibility();
  failedChangesRemainAtomic();
  managerAccounting(0);
  managerAccounting(1);
  managerAccounting(2);
  compactCompatibility();
  legacyLookupAndCache();
  fieldsAndOverrides();
  concurrentMutation();
  std::cout << "[I] Actual runtime metadata legacy lookup, normalization, controls, cache, predicates, overrides and concurrent updates passed.\n";
}
