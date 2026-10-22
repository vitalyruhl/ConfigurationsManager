#include <ArduinoJson.h>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#if __has_include("web/AppInfoJson.h")
#include "web/AppInfoJson.h"

// [MOCKED!] Arduino String storage and allocation-failure boundaries only.
struct Output {
  std::string text = "old output";
  unsigned reserveCalls = 0;
  bool failReserve = false;
  size_t failAfter = std::numeric_limits<size_t>::max();
  void remove(unsigned) {
    text.clear();
  }
  bool reserve(unsigned size) {
    ++reserveCalls;
    if (failReserve)
      return false;
    text.reserve(size);
    return true;
  }
  bool concat(char byte) {
    if (text.size() >= failAfter)
      return false;
    text.push_back(byte);
    return true;
  }
  size_t length() const {
    return text.length();
  }
};

struct DocumentAllocator : ArduinoJson::Allocator {
  size_t bytes = 0;
  void* allocate(size_t size) override {
    bytes += size;
    return std::malloc(size);
  }
  void deallocate(void* value) override {
    std::free(value);
  }
  void* reallocate(void* value, size_t size) override {
    return std::realloc(value, size);
  }
};

static void legacyGolden(const std::string& name, const std::string& title, const std::string& version, bool guiLogging) {
  DocumentAllocator allocator;
  JsonDocument reference(&allocator);
  reference["appName"] = name;
  reference["appTitle"] = title;
  reference["version"] = version;
  reference["guiLogging"] = guiLogging;
  std::string expected;
  serializeJson(reference, expected);
  Output actual;
  assert(cm::buildAppInfoJson(name, title, version, guiLogging, actual));
  assert(actual.text == expected);
  assert(actual.reserveCalls == 1);
  assert(allocator.bytes >= 4096);
}

int main() {
  legacyGolden("SolarBatteryTelemetry", "Battery \"Monitor\"", "1.0.0", true);
  legacyGolden("", "", "", false);
  legacyGolden("\xc3\xa4\xe2\x82\xac", "\\\b\f\n\r\t", "quoted\"version", false);

  std::string controls;
  for (unsigned i = 0; i < 32; ++i)
    controls.push_back(static_cast<char>(i));
  Output escaped;
  assert(cm::buildAppInfoJson(controls, controls, controls, true, escaped));
  JsonDocument parsed;
  assert(!deserializeJson(parsed, escaped.text));
  for (const char* key : {"appName", "appTitle", "version"}) {
    const auto value = parsed[key].as<JsonString>();
    assert(value.size() == controls.size());
    assert(std::string(value.c_str(), value.size()) == controls);
  }
  assert(parsed["guiLogging"].is<bool>());
  assert(parsed["guiLogging"].as<bool>());
  assert(parsed.size() == 4);

  Output reserveFailure;
  reserveFailure.failReserve = true;
  assert(!cm::buildAppInfoJson(controls, controls, controls, false, reserveFailure));
  assert(reserveFailure.text.empty());
  Output writeFailure;
  writeFailure.failAfter = 20;
  assert(!cm::buildAppInfoJson(controls, controls, controls, false, writeFailure));
  assert(writeFailure.text.empty());
  std::cout << "App info JSON goldens, controls, schema and failure cleanup passed.\n";
}
#else
int main() {
  std::cerr << "Missing production AppInfoJson helper.\n";
  return 1;
}
#endif
