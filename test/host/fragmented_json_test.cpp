#include "web/FragmentedJson.h"
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <string>

struct TrackingAllocator : ArduinoJson::Allocator {
  std::map<void*, size_t> blocks;
  bool fail = false;
  void* allocate(size_t size) override {
    if (fail)
      return nullptr;
    void* p = std::malloc(size);
    if (p)
      blocks[p] = size;
    return p;
  }
  void deallocate(void* p) override {
    blocks.erase(p);
    std::free(p);
  }
  void* reallocate(void* p, size_t size) override {
    if (fail)
      return nullptr;
    void* q = std::realloc(p, size);
    if (q) {
      blocks.erase(p);
      blocks[q] = size;
    }
    return q;
  }
};
// [MOCKED!] String capacity/failure interface; real ArduinoJson documents/allocator.
struct Output {
  std::string value;
  size_t reserved = 0;
  bool failReserve = false;
  size_t failAfter = static_cast<size_t>(-1);
  std::function<void()> onReserve;
  void remove(unsigned) {
    value.clear();
  }
  bool reserve(unsigned n) {
    if (n <= reserved)
      return true;
    if (onReserve)
      onReserve();
    if (failReserve)
      return false;
    reserved = n;
    value.reserve(n);
    return true;
  }
  bool concat(char c) {
    if (value.size() >= failAfter || value.size() >= reserved)
      return false;
    value.push_back(c);
    return true;
  }
  bool concat(const char*, unsigned) = delete;
  size_t length() const {
    return value.size();
  }
};
static void page(JsonObject object, int i) {
  object["name"] = std::string("Page \"") + std::to_string(i) + "\\";
  object["order"] = 40 - i;
  auto card = object["cards"].to<JsonArray>().add<JsonObject>();
  card["name"] = "A";
  card["items"].to<JsonArray>().add("source.value");
  auto group = card["groups"].to<JsonArray>().add<JsonObject>();
  group["name"] = "G";
  group["items"].to<JsonArray>().add("source.other");
}
static void placement(JsonObject item, int i) {
  item["page"] = "Page";
  item["card"] = "A";
  item["group"] = "G";
  item["order"] = i;
}
static std::string key(int i) {
  return i == 61 ? "key\"0\\" : "key\"" + std::to_string(i) + "\\";
}
int main() {
  std::string golden;
  {
    JsonDocument baseline;
    auto pages = baseline["pages"].to<JsonArray>();
    for (int i = 0; i < 3; ++i)
      page(pages.add<JsonObject>(), i);
    for (int i = 0; i < 62; ++i)
      placement(baseline["placements"][key(i)].to<JsonObject>(), i);
    baseline["defaultPage"] = "Page";
    serializeJson(baseline, golden);
  }
  TrackingAllocator allocator;
  unsigned calls = 0;
  unsigned failPass = 0;
  auto fragments = [&](auto& sink) {
    allocator.fail = ++calls == failPass;
    if (!sink.raw("{\"pages\":["))
      return false;
    for (int i = 0; i < 3; ++i) {
      if (i && !sink.raw(","))
        return false;
      JsonDocument fragment(&allocator);
      page(fragment.to<JsonObject>(), i);
      if (!sink.json(fragment))
        return false;
    }
    if (!sink.raw("],\"placements\":{"))
      return false;
    for (int i = 0; i < 61; ++i) {
      if (i && !sink.raw(","))
        return false;
      {
        JsonDocument name(&allocator);
        name.set(key(i));
        if (!sink.json(name))
          return false;
      }
      if (!sink.raw(":"))
        return false;
      JsonDocument fragment(&allocator);
      placement(fragment.to<JsonObject>(), i == 0 ? 61 : i);
      if (!sink.json(fragment))
        return false;
    }
    if (!sink.raw("},\"defaultPage\":"))
      return false;
    {
      JsonDocument name(&allocator);
      name.set("Page");
      if (!sink.json(name))
        return false;
    }
    return sink.raw("}");
  };
  Output output;
  output.onReserve = [&]() { assert(calls == 1 && allocator.blocks.empty()); };
  assert(cm::buildFragmentedJson(fragments, output));
  assert(calls == 2 && allocator.blocks.empty() && output.value == golden);
  JsonDocument parsed;
  assert(!deserializeJson(parsed, output.value));
  assert(parsed["pages"][0]["order"] == 40 && parsed["placements"].size() == 61);
  assert(parsed["placements"][key(0)]["order"] == 61);
  for (unsigned failurePass : {1u, 2u}) {
    Output failed;
    calls = 0;
    failPass = failurePass;
    assert(!cm::buildFragmentedJson(fragments, failed));
    assert(failed.value.empty() && allocator.blocks.empty());
  }
  failPass = 0;
  calls = 0;
  allocator.fail = false;
  Output reserveFailure;
  reserveFailure.failReserve = true;
  assert(!cm::buildFragmentedJson(fragments, reserveFailure));
  assert(calls == 1 && reserveFailure.value.empty());
  Output changed;
  unsigned changes = 0;
  assert(!cm::buildFragmentedJson([&](auto& sink) { return sink.raw(++changes == 1 ? "{}" : "{\"x\":1}"); }, changed));
  assert(changed.value.empty());
  Output writeFailure;
  writeFailure.failAfter = 62;
  calls = 0;
  assert(!cm::buildFragmentedJson(fragments, writeFailure));
  assert(writeFailure.value.empty());
  Output empty;
  assert(cm::buildFragmentedJson([](auto& sink) { return sink.raw("{\"pages\":[],\"placements\":{},\"defaultPage\":\"\"}"); }, empty));
  assert(empty.value == "{\"pages\":[],\"placements\":{},\"defaultPage\":\"\"}");
  std::cout << "Fragmented JSON passed: golden " << golden.size() << " bytes, escaped keys, last-value duplicate semantics, fragment release, both-pass OOM, reserve/change/write failures\n";
}
