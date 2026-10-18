#include <ArduinoJson.h>
#include "runtime/detail/RuntimeMetaJson.h"
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
// [MOCKED!] Host persistent metadata and Arduino String output interface.
// Reference serializer is frozen from the pre-change implementation.
// Match the ESP32 String equality behavior, including its NUL limitation.
struct KeyString : std::string {
  using std::string::string;
  using std::string::operator=;
  bool operator==(const KeyString& other) const {
    return length() == other.length() && std::strcmp(c_str(), other.c_str()) == 0;
  }
  bool operator==(const char* other) const { return std::strcmp(c_str(), other) == 0; }
};
struct Property { KeyString key; std::string value; };
struct Rule { KeyString target; std::vector<Property> properties; bool hasVisible=false, visible=true; std::string className; };
struct Style { std::vector<Rule> rules; bool empty() const { return rules.empty(); } };
struct Output {
  std::string value;
  size_t reserved=0, reserveCalls=0, failAfter=std::numeric_limits<size_t>::max();
  bool failReserve=false;
  void remove(unsigned) { value.clear(); }
  bool reserve(unsigned n) { ++reserveCalls; if(failReserve)return false; reserved=n;value.reserve(n);return true; }
  bool concat(char c) { if(value.size()>=reserved || value.size()>=failAfter)return false;value.push_back(c);return true; }
  size_t length() const { return value.size(); }
};
struct TrackingAllocator : ArduinoJson::Allocator {
  size_t calls=0, bytes=0;
  void* allocate(size_t n) override { ++calls;bytes+=n;return std::malloc(n); }
  void* reallocate(void* p,size_t n) override {++calls;bytes+=n;return std::realloc(p,n);}
  void deallocate(void* p) override {std::free(p);}
};
struct Meta {
  std::string group;
  std::string sourceGroup;
  std::string page;
  std::string key;
  std::string label;
  std::string unit;
  std::string onLabel;
  std::string offLabel;
  int precision = 2;
  int order = 100;
  bool isBool = false;
  bool isString = false;
  bool isDivider = false;
  bool isButton = false;
  bool isCheckbox = false;
  bool isStateButton = false;
  bool isMomentaryButton = false;
  bool isIntSlider = false;
  bool isFloatSlider = false;
  bool isIntInput = false;
  bool isFloatInput = false;
  bool hasAlarm = false;
  bool alarmWhenTrue = false;
  bool boolAlarmValue = false;
  float alarmMin = 0.0f;
  float alarmMax = 0.0f;
  float warnMin = 0.0f;
  float warnMax = 0.0f;
  // Slider specific properties
  int intMin = 0;
  int intMax = 100;
  int intInit = 0;
  float floatMin = 0.0f;
  float floatMax = 1.0f;
  float floatInit = 0.0f;
  int floatPrecision = 2;
  bool initialState = false;
  std::string staticValue;
  std::string card;
  Style style;
};

void referenceMeta(JsonObject o, const Meta& m) {
  o["group"] = m.group;
  if (m.sourceGroup.length()) {
    o["sourceGroup"] = m.sourceGroup;
  }
  if (m.page.length()) {
    o["page"] = m.page;
  }
  if (m.card.length()) {
    o["card"] = m.card;
  }
  o["key"] = m.key;
  o["label"] = m.label;
  if (m.unit.length()) {
    o["unit"] = m.unit;
  }
  o["precision"] = m.precision;
  if (m.isBool)
    o["isBool"] = true;
  if (m.isString)
    o["isString"] = true;
  if (m.isDivider)
    o["isDivider"] = true;
  if (m.isButton)
    o["isButton"] = true;
  if (m.isCheckbox)
    o["isCheckbox"] = true;
  if (m.isStateButton)
    o["isStateButton"] = true;
  if (m.isMomentaryButton)
    o["isMomentaryButton"] = true;
  if (m.isIntSlider) {
    o["isIntSlider"] = true;
    o["min"] = m.intMin;
    o["max"] = m.intMax;
    o["init"] = m.intInit;
  }
  if (m.isFloatSlider) {
    o["isFloatSlider"] = true;
    o["min"] = m.floatMin;
    o["max"] = m.floatMax;
    o["init"] = m.floatInit;
  }
  if (m.isIntInput) {
    o["isIntInput"] = true;
    o["min"] = m.intMin;
    o["max"] = m.intMax;
    o["init"] = m.intInit;
  }
  if (m.isFloatInput) {
    o["isFloatInput"] = true;
    o["min"] = m.floatMin;
    o["max"] = m.floatMax;
    o["init"] = m.floatInit;
  }
  if (m.hasAlarm)
    o["hasAlarm"] = true;
  if (m.alarmWhenTrue)
    o["alarmWhenTrue"] = true;
  if (m.boolAlarmValue) {
    o["boolAlarmValue"] = m.boolAlarmValue;
  }
  if (m.alarmMin != 0.0f || m.alarmMax != 0.0f) {
    o["alarmMin"] = m.alarmMin;
    o["alarmMax"] = m.alarmMax;
  }
  if (m.warnMin != 0.0f || m.warnMax != 0.0f) {
    o["warnMin"] = m.warnMin;
    o["warnMax"] = m.warnMax;
  }
  o["order"] = m.order;
  if (m.staticValue.length()) {
    o["staticValue"] = m.staticValue;
  }
  if (m.onLabel.length()) {
    o["onLabel"] = m.onLabel;
  }
  if (m.offLabel.length()) {
    o["offLabel"] = m.offLabel;
  }
  if (!m.style.empty()) {
    JsonObject styleObj = o.createNestedObject("style");
    for (const auto& rule : m.style.rules) {
      if (!rule.target.length()) {
        continue;
      }
      JsonObject ruleObj = styleObj.createNestedObject(rule.target);
      if (rule.hasVisible) {
        ruleObj["visible"] = rule.visible;
      }
      if (rule.className.length()) {
        ruleObj["className"] = rule.className;
      }
      for (const auto& prop : rule.properties) {
        if (prop.key.length()) {
          ruleObj[prop.key] = prop.value;
        }
      }
    }
  }
}

static void compare(const Meta& m) {
  JsonDocument reference; referenceMeta(reference.to<JsonObject>(),m);
  std::string expected;serializeJson(reference,expected);
  Output actual;
  assert(cm::buildJsonOutput([&](auto& sink){return cm::writeRuntimeMeta(sink,m);},actual));
  if(expected!=actual.value){std::cerr<<"Expected: "<<expected<<"\nActual: "<<actual.value<<"\n";std::exit(1);}
  assert(actual.reserveCalls==1);
  JsonDocument parsed; assert(!deserializeJson(parsed,actual.value));
}
int main() {
  Meta m;compare(m);
  m.group="g\"\\\n\t";m.sourceGroup="source";m.page="page";m.card="card";
  m.key="key";m.label="\xc3\xa4\xe6\xbc\xa2";m.unit="V";m.staticValue="value";m.onLabel="On";m.offLabel="Off";
  m.isBool=m.isString=m.isDivider=m.isButton=m.isCheckbox=m.isStateButton=m.isMomentaryButton=true;
  m.hasAlarm=m.alarmWhenTrue=m.boolAlarmValue=true;
  m.alarmMin=-3.25f;m.alarmMax=40.123456f;m.warnMin=0.00000012f;m.warnMax=10000000.0f;
  m.intMin=INT_MIN;m.intMax=INT_MAX;m.intInit=-1;
  m.floatMin=-0.123456789f;m.floatMax=123.125f;m.floatInit=-0.0f;
  for(unsigned mask=0;mask<16;++mask){m.isIntSlider=mask&1;m.isFloatSlider=mask&2;m.isIntInput=mask&4;m.isFloatInput=mask&8;compare(m);}
  Rule rule;rule.target="label\"";rule.hasVisible=true;rule.visible=false;rule.className="a";
  rule.properties={{"color","red"},{"visible","override"},{"className","override class"},{"color","blue"},{"","ignored"}};
  m.style.rules={rule,Rule{},rule};m.style.rules.back().className="last";compare(m);
  m.style.rules.back().properties={{"new","only"}};m.style.rules.back().hasVisible=false;m.style.rules.back().className.clear();compare(m);
  Rule nulRule; nulRule.target=KeyString("target\0a",8);
  nulRule.hasVisible=true; nulRule.className="base";
  nulRule.properties={{KeyString("visible\0x",9),"separate"},
      {KeyString("className\0x",11),"separate class"},
      {KeyString("key\0a",5),"first"},{KeyString("key\0b",5),"second"},
      {KeyString("key\0a",5),"replacement"}};
  Rule otherNulRule=nulRule; otherNulRule.target=KeyString("target\0b",8);
  otherNulRule.properties={{"distinct","target"}};
  assert(nulRule.target==otherNulRule.target); // Demonstrate the platform comparison trap.
  m.style.rules={nulRule,otherNulRule}; compare(m);
  m.style.rules={Rule{}};compare(m);
  for(float value:{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){m.floatMin=value;m.alarmMax=value;compare(m);}
  m.label=std::string("A\0\x01\x1fZ",5);
  Output escaped;assert(cm::buildJsonOutput([&](auto& sink){return cm::writeRuntimeMeta(sink,m);},escaped));
  JsonDocument parsed;assert(!deserializeJson(parsed,escaped.value));assert(parsed["label"].as<std::string>()==m.label);
  for(bool reserveFailure:{false,true}){Output failed;failed.failReserve=reserveFailure;if(!reserveFailure)failed.failAfter=17;
    assert(!cm::buildJsonOutput([&](auto& sink){return cm::writeRuntimeMeta(sink,m);},failed));assert(failed.value.empty());}
  TrackingAllocator alloc;
  for(unsigned i=0;i<64;++i)for(unsigned pass=0;pass<2;++pass){JsonDocument d(&alloc);referenceMeta(d.to<JsonObject>(),m);}
  std::cout<<"Runtime metadata passed: 16 flag combinations, duplicate styles/properties, escaping, nonfinite numbers, failure cleanup. Legacy 64-field two-pass document allocations="<<alloc.calls<<", requested bytes="<<alloc.bytes<<"; new serializer creates no documents.\n";
}
