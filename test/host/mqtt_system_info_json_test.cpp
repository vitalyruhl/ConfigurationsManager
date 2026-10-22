#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

// [MOCKED!] Host String adapter; the firmware build validates real Arduino String.
struct HostString : std::string {
  static size_t live;
  using std::string::append;
  HostString& append(const char* s) {
    std::string::append(s);
    return *this;
  }
  HostString& operator=(const HostString&) = default;
  HostString& operator=(HostString&&) = default;
  HostString() {
    ++live;
  }
  HostString(const char* s)
      : std::string(s) {
    ++live;
  }
  HostString(const std::string& s)
      : std::string(s) {
    ++live;
  }
  HostString(const HostString& s)
      : std::string(s) {
    ++live;
  }
  HostString(HostString&& s) noexcept
      : std::string(std::move(s)) {
    ++live;
  }
  ~HostString() {
    --live;
  }
};
size_t HostString::live = 0;

namespace allocation {
std::map<void*, size_t> blocks;
size_t live = 0, peak = 0, calls = 0;
size_t maxStringsAtAllocation = 0;
void* allocate(size_t n) {
  void* p = std::malloc(n);
  if (p) {
    blocks[p] = n;
    live += n;
    peak = std::max(peak, live);
    ++calls;
  }
  maxStringsAtAllocation = std::max(maxStringsAtAllocation, HostString::live);
  return p;
}
void release(void* p) {
  if (p) {
    live -= blocks.at(p);
    blocks.erase(p);
  }
  std::free(p);
}
void* resize(void* p, size_t n) {
  if (!p)
    return allocate(n);
  const size_t old = blocks.at(p);
  void* q = std::realloc(p, n);
  if (q) {
    blocks.erase(p);
    blocks[q] = n;
    live = live - old + n;
    peak = std::max(peak, live);
  }
  return q;
}
void reset() {
  peak = calls = maxStringsAtAllocation = 0;
}
} // namespace allocation

// Instrument only ArduinoJson's default allocator, without a firmware-only hook.
#define malloc allocation::allocate
#define realloc allocation::resize
#define free allocation::release
#include <ArduinoJson.h>
#undef malloc
#undef realloc
#undef free
#include "mqtt/detail/SystemInfoJson.h"

// [MOCKED!] Complete SystemInfo boundary fixture; no network/device access.
struct Info {
  uint32_t uptimeMs = 4294967295U, freeHeap = 123456, minFreeHeap = 123,
           maxAllocHeap = 456, flashSizeBytes = 4194304, cpuFreqMHz = 240;
  HostString chipModel = "ESP\"32\\chip", sdkVersion = "sdk\nline";
  uint32_t chipRevision = 3;
  HostString hostname = "host\tname", ssid = "WiFi\"\\\n", ip = "192.0.2.1",
             mac = "AA:BB:CC:DD:EE:FF";
  int32_t rssi = -127;
};
int failures = 0;
void check(bool condition, const char* name) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << name << '\n';
  }
}

void run(bool espResult, bool wifiResult, bool optional, bool connectedValue, bool longStrings = false) {
  Info info;
  if (!optional)
    info.minFreeHeap = info.maxAllocHeap = 0;
  if (longStrings)
    info.ssid = HostString(std::string(600, 'x'));
  HostString base = "device/System-Info", uptime = "49d 17h 2m 47s";
  const size_t baselineStrings = HostString::live;
  allocation::reset();
  size_t publications = 0, connectedCalls = 0;
  std::vector<std::string> payloads;
  const bool result = cm::detail::publishSystemInfoPayloads(
    info, base, uptime, [&] { ++connectedCalls; check(publications == 1, "WiFi status sampled after ESP publication"); return connectedValue; }, [&](const HostString& topic, const HostString& payload) {
        check(allocation::live == 0, "document allocations released before publication");
        check(HostString::live == baselineStrings + 2, "only current payload and topic live at publication");
        check(topic == (publications ? "device/System-Info/WiFi" : "device/System-Info/ESP"), "topic/order contract");
        payloads.emplace_back(payload);
        return ++publications == 1 ? espResult : wifiResult; });
  check(publications == 2, "both publications attempted after ESP failure");
  check(connectedCalls == 1, "WiFi status sampled once");
  check(result == (espResult && wifiResult), "AND publication result");
  check(allocation::live == 0 && allocation::blocks.empty(), "all document allocations released after call");
  check(HostString::live == baselineStrings, "payloads and topics released after call");
  check(allocation::maxStringsAtAllocation <= baselineStrings + 1,
        "first payload/topic released before second document allocates");
  std::string expectedEsp = "{\"uptimeMs\":4294967295,\"uptimeHuman\":\"49d 17h 2m 47s\",\"freeHeap\":123456";
  if (optional)
    expectedEsp += ",\"minFreeHeap\":123,\"maxAllocHeap\":456";
  expectedEsp += ",\"flashSizeBytes\":4194304,\"cpuFreqMHz\":240,\"chipModel\":\"ESP\\\"32\\\\chip\",\"chipRevision\":3,\"sdkVersion\":\"sdk\\nline\"}";
  check(payloads[0] == expectedEsp, "ESP exact JSON schema/order/numbers/escaping/optional fields");
  const std::string ssid = longStrings ? std::string(600, 'x') : "WiFi\\\"\\\\\\n";
  const std::string expectedWifi = "{\"uptimeMs\":4294967295,\"uptimeHuman\":\"49d 17h 2m 47s\",\"hostname\":\"host\\tname\",\"ssid\":\"" + ssid + "\",\"rssi\":-127,\"ip\":\"192.0.2.1\",\"mac\":\"AA:BB:CC:DD:EE:FF\",\"connected\":" + (connectedValue ? "true}" : "false}");
  check(payloads[1] == expectedWifi, "WiFi exact JSON schema/order/numbers/escaping/connection state");
  std::cout << "host-only ArduinoJson allocations: peak=" << allocation::peak
            << " calls=" << allocation::calls << " optional=" << optional
            << " longStrings=" << longStrings << '\n';
}
int main() {
  run(true, true, true, true);
  run(false, true, false, false);
  run(true, false, true, false);
  run(false, false, false, true);
  run(true, true, true, true, true);
  if (failures) {
    std::cerr << failures << " failed checks\n";
    return 1;
  }
  std::cout << "MQTT System-Info JSON host tests passed\n";
}
