#include <ArduinoJson.h>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <string>

#if __has_include("web/JsonOutput.h")
#include "web/JsonOutput.h"

static size_t allocationCount = 0;
void* operator new(size_t size) {
  ++allocationCount;
  if (void* p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete[](void* p) noexcept { ::operator delete(p); }

// [MOCKED!] Arduino String capacity/failure boundary, not JSON serialization.
struct Output {
  std::string text;
  size_t capacity = 0;
  unsigned reserveCalls = 0;
  bool failReserve = false;
  size_t failAfter = std::numeric_limits<size_t>::max();
  bool lieAboutWrite = false;
  void remove(unsigned) { text.clear(); }
  bool reserve(unsigned size) {
    ++reserveCalls;
    if (failReserve) return false;
    capacity = size;
    text.reserve(size);
    return true;
  }
  bool concat(char c) {
    if (text.size() >= failAfter || text.size() >= capacity) return false;
    if (!lieAboutWrite) text.push_back(c);
    return true;
  }
  bool concat(const char*, unsigned) = delete;
  size_t length() const { return text.length(); }
};

struct DocumentAllocator : ArduinoJson::Allocator {
  size_t calls = 0;
  void* allocate(size_t n) override { ++calls; return std::malloc(n); }
  void deallocate(void* p) override { std::free(p); }
  void* reallocate(void* p, size_t n) override { ++calls; return std::realloc(p, n); }
};

template<class T> static void scalar(T input) {
  JsonDocument reference;
  reference.set(input);
  std::string expected;
  serializeJson(reference, expected);
  Output actual;
  assert(cm::buildJsonOutput([&](auto& sink) { return sink.value(input); }, actual));
  assert(actual.text == expected);
  assert(actual.reserveCalls == 1);
}

static void goldenAndEscaping() {
  const std::string name = "Solar \"A\"\\\n\t\r\b\f \xc3\xa4";
  JsonDocument reference;
  reference["na\"me"] = name;
  reference["enabled"] = true;
  reference["number"] = -42;
  reference["voltage"] = 12.125;
  reference["missing"] = nullptr;
  std::string expected;
  serializeJson(reference, expected);
  Output actual;
  assert(cm::buildJsonOutput([&](auto& sink) {
    bool first = true;
    return sink.raw("{") && sink.member(first, "na\"me", name) &&
        sink.member(first, "enabled", true) && sink.member(first, "number", -42) &&
        sink.member(first, std::string("voltage"), 12.125) &&
        sink.member(first, "missing", nullptr) && sink.raw("}");
  }, actual));
  assert(actual.text == expected);
  const char controls[] = {0, 1, 8, 9, 10, 11, 12, 13, 31, '"', '\\'};
  assert(cm::buildJsonOutput([&](auto& sink) { return sink.string(controls, sizeof(controls)); }, actual));
  assert(actual.text == "\"\\u0000\\u0001\\b\\t\\n\\u000b\\f\\r\\u001f\\\"\\\\\"");
  JsonDocument parsed;
  assert(!deserializeJson(parsed, actual.text));
  auto decoded = parsed.as<JsonString>();
  assert(decoded.size() == sizeof(controls));
  assert(std::memcmp(decoded.c_str(), controls, sizeof(controls)) == 0);
  char allControls[32];
  for (unsigned i = 0; i < sizeof(allControls); ++i) allControls[i] = static_cast<char>(i);
  assert(cm::buildJsonOutput([&](auto& sink) { return sink.string(allControls, sizeof(allControls)); }, actual));
  assert(actual.text == "\"\\u0000\\u0001\\u0002\\u0003\\u0004\\u0005\\u0006\\u0007"
      "\\b\\t\\n\\u000b\\f\\r\\u000e\\u000f\\u0010\\u0011\\u0012\\u0013"
      "\\u0014\\u0015\\u0016\\u0017\\u0018\\u0019\\u001a\\u001b\\u001c\\u001d\\u001e\\u001f\"");
  assert(!deserializeJson(parsed, actual.text));
  assert(parsed.as<JsonString>().size() == sizeof(allControls));
  assert(std::memcmp(parsed.as<JsonString>().c_str(), allControls, sizeof(allControls)) == 0);
  std::string embedded("a\0b", 3);
  assert(cm::buildJsonOutput([&](auto& sink) { return sink.value(embedded); }, actual));
  assert(actual.text == "\"a\\u0000b\"");
  assert(cm::buildJsonOutput([](auto& sink) { return sink.string(""); }, actual));
  assert(actual.text == "\"\"");
  assert(cm::buildJsonOutput([](auto& sink) { return sink.value(static_cast<const char*>(nullptr)); }, actual));
  assert(actual.text == "null");
}

static void numericCompatibility() {
  scalar(true); scalar(false); scalar(nullptr); scalar("literal");
  scalar(std::numeric_limits<int64_t>::min()); scalar(std::numeric_limits<int64_t>::max());
  scalar(std::numeric_limits<uint64_t>::max()); scalar(int8_t(-128)); scalar(uint8_t(255));
  scalar(int16_t(-32768)); scalar(uint16_t(65535)); scalar(-1L); scalar(0u);
  const double values[] = {0, -0.0, 1, -1, 12.125, 1.23456789, 100000.125,
    1e-10, 1e-5, 1e7, 1e20, 1e100, std::numeric_limits<double>::max(),
    std::numeric_limits<double>::min(), std::numeric_limits<double>::denorm_min(),
    std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
    std::numeric_limits<double>::quiet_NaN()};
  for (double value : values) { scalar(value); scalar(static_cast<float>(value)); }
  // Exact float-as-double storage takes the 6-place float path in ArduinoJson.
  scalar(static_cast<double>(1.234567f));
  for (int exponent = -30; exponent <= 30; ++exponent) {
    scalar(1.234567890123 * std::pow(10.0, exponent));
    scalar(static_cast<float>(-1.234567890123 * std::pow(10.0, exponent)));
  }
}

static void failuresAreStickyAndAtomic() {
  cm::JsonOutput<Output> limited(nullptr, 3);
  assert(limited.raw("abc"));
  assert(!limited.raw("d") && !limited.good());
  assert(!limited.raw("") && !limited.value(false) && limited.size() == 3);
  cm::JsonOutput<Output> escaped(nullptr, 3);
  assert(!escaped.string("\n") && !escaped.good());
  cm::JsonOutput<Output> invalid;
  assert(!invalid.string(nullptr, 1) && !invalid.good());
  cm::JsonOutput<Output> invalidRaw;
  assert(!invalidRaw.raw(nullptr) && !invalidRaw.good());
  cm::JsonOutput<Output> huge(nullptr, 4);
  assert(!huge.string("x", std::numeric_limits<size_t>::max()) && !huge.good());
  Output reserveFail; reserveFail.text = "stale"; reserveFail.failReserve = true;
  assert(!cm::buildJsonOutput([](auto& sink) { return sink.raw("{}"); }, reserveFail));
  assert(reserveFail.text.empty() && reserveFail.reserveCalls == 1);
  for (size_t cut = 0; cut < 12; ++cut) {
    Output shortWrite; shortWrite.failAfter = cut;
    assert(!cm::buildJsonOutput([](auto& sink) { return sink.raw("{\"name\":123}"); }, shortWrite));
    assert(shortWrite.text.empty());
  }
  for (bool grows : {false, true}) {
    unsigned pass = 0; Output changed;
    assert(!cm::buildJsonOutput([&](auto& sink) {
      ++pass; return sink.raw((pass == 1) == grows ? "0" : "123");
    }, changed));
    assert(changed.text.empty());
  }
  for (unsigned failedPass : {1u, 2u}) {
    unsigned pass = 0; Output failed;
    assert(!cm::buildJsonOutput([&](auto& sink) {
      return sink.raw("{}") && ++pass != failedPass;
    }, failed));
    assert(failed.text.empty());
  }
  Output ignoredError;
  assert(!cm::buildJsonOutput([](auto& sink) { sink.raw(nullptr); return true; }, ignoredError));
  assert(ignoredError.text.empty() && ignoredError.reserveCalls == 0);
  Output dishonest; dishonest.lieAboutWrite = true;
  assert(!cm::buildJsonOutput([](auto& sink) { return sink.raw("{}"); }, dishonest));
  assert(dishonest.text.empty());
}

static void documentFreeAllocationEvidence() {
  const std::string owned(100, 'x');
  DocumentAllocator allocator;
  { JsonDocument old(&allocator); old["name"] = owned; old["value"] = 12.34567; }
  assert(allocator.calls > 0);
  Output output; output.text.reserve(2048);
  const size_t before = allocationCount;
  assert(cm::buildJsonOutput([&](auto& sink) {
    bool first = true;
    return sink.raw("{") && sink.member(first, "name", owned) &&
        sink.member(first, "value", 12.34567) && sink.raw("}");
  }, output));
  assert(allocationCount == before);
  assert(output.reserveCalls == 1);
  std::cout << "Document allocator calls in reference: " << allocator.calls
            << "; C++ allocations in direct count/write with preallocated output: 0\n";
}

int main() {
  goldenAndEscaping(); numericCompatibility(); failuresAreStickyAndAtomic();
  documentFreeAllocationEvidence();
  std::cout << "JSON output passed: scalar goldens, all control bytes, UTF-8, numeric bounds, float compatibility, bounded failure and atomic output\n";
}
#else
int main() {
  std::cerr << "FAIL: document-free JSON output serializer is not implemented\n";
  return 1;
}
#endif
