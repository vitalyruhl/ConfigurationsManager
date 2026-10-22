#include "web/CheckedJson.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

// [MOCKED!] Arduino String destination failures; the JSON document and serializer are real.
struct Destination {
  std::string value;
  bool failReserve = false;
  size_t failAfter = static_cast<size_t>(-1);
  unsigned reserves = 0;
  void remove(unsigned) {
    value.clear();
  }
  bool reserve(unsigned size) {
    ++reserves;
    if (failReserve)
      return false;
    value.reserve(size);
    return true;
  }
  // Deliberately forbid the pointer overload: pinned Arduino String copies
  // size+1 bytes there, even when the serializer provides an unterminated slice.
  bool concat(const char*, unsigned) = delete;
  bool concat(char byte) {
    if (value.size() + 1 > failAfter)
      return false;
    value.push_back(byte);
    return true;
  }
  size_t length() const {
    return value.size();
  }
};
struct FailingAllocator : ArduinoJson::Allocator {
  void* allocate(size_t) override {
    return nullptr;
  }
  void deallocate(void* ptr) override {
    std::free(ptr);
  }
  void* reallocate(void*, size_t) override {
    return nullptr;
  }
};
int main() {
  Destination slice;
  cm::CheckedJsonWriter<Destination> sliceWriter(slice);
  const uint8_t unterminated[] = {'4', '0'};
  assert(sliceWriter.write(unterminated, sizeof(unterminated)) == 2);
  assert(slice.value == "40");
  JsonDocument layout;
  auto pages = layout["pages"].to<JsonArray>();
  auto page = pages.add<JsonObject>();
  page["name"] = "Bluetooth";
  page["title"] = "A \"quoted\" title\\path";
  page["order"] = 40;
  page["cards"].to<JsonArray>().add<JsonObject>()["name"] = "Power";
  layout["placements"]["source.value"]["page"] = "Bluetooth";
  layout["defaultPage"] = "Bluetooth";
  Destination good;
  cm::CheckedJsonResult result;
  assert(cm::serializeJsonChecked(layout, good, &result));
  assert(result.reason == cm::CheckedJsonReason::Ok);
  assert(result.expected == good.length() && result.written == good.length() && result.stored == good.length());
  assert(good.reserves == 1 && good.length() == measureJson(layout));
  JsonDocument decoded;
  assert(!deserializeJson(decoded, good.value));
  assert(decoded["pages"][0]["title"].as<std::string>() == "A \"quoted\" title\\path");
  Destination reserveFailure;
  reserveFailure.failReserve = true;
  assert(!cm::serializeJsonChecked(layout, reserveFailure, &result));
  assert(result.reason == cm::CheckedJsonReason::Reserve && result.expected == measureJson(layout));
  assert(reserveFailure.value.empty());
  Destination appendFailure;
  appendFailure.failAfter = 62;
  assert(!cm::serializeJsonChecked(layout, appendFailure, &result));
  assert(result.reason == cm::CheckedJsonReason::Write && result.stored == 62 && result.written == 62);
  assert(appendFailure.value.empty());
  FailingAllocator allocator;
  JsonDocument overflow(&allocator);
  overflow["pages"].to<JsonArray>().add("Bluetooth");
  assert(overflow.overflowed());
  Destination rejected;
  assert(!cm::serializeJsonChecked(overflow, rejected, &result));
  assert(result.reason == cm::CheckedJsonReason::Overflow && result.expected == 0);
  assert(rejected.value.empty() && rejected.reserves == 0);
  std::cout << "Checked JSON passed: real ArduinoJson generation/overflow, reserve failure, concat failure and escaped round-trip\n";
}
