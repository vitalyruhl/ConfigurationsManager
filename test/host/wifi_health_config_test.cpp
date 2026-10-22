#include "wifi/detail/WiFiHealthConfig.h"
#include <cassert>
#include <cstring>
#include <thread>
#include <iostream>
int main() {
  cm::detail::WiFiHealthConfigMailbox box;
  cm::detail::WiFiHealthConfig update;
  assert(!box.take(update));
  assert(box.setTarget("gateway"));
  assert(box.setTimeoutMinutes(1));
  assert(box.take(update));
  assert(update.targetChanged && update.timeoutChanged);
  assert(std::strcmp(update.target.data(), "gateway") == 0 && update.timeoutMinutes == 1);
  assert(!box.take(update));
  assert(!box.setTarget("1234567890123456"));
  assert(!box.setTimeoutMinutes(10000));
  assert(!box.take(update));
  assert(box.setTimeoutMinutes(0));
  assert(box.take(update) && update.timeoutChanged && !update.targetChanged && update.timeoutMinutes == 0);
  assert(box.setTarget("192.168.255.254"));
  assert(box.setTarget("off"));
  assert(box.take(update) && !update.timeoutChanged && std::strcmp(update.target.data(), "off") == 0);
  std::thread producer([&] { for (unsigned i=0;i<1000;++i) { assert(box.setTarget(i%2 ? "gateway" : "192.168.2.1")); assert(box.setTimeoutMinutes(i)); } });
  for (unsigned i = 0; i < 1000; ++i)
    if (box.take(update) && update.targetChanged)
      assert(std::strcmp(update.target.data(), "gateway") == 0 || std::strcmp(update.target.data(), "192.168.2.1") == 0);
  producer.join();
  std::cout << "WiFi health configuration handoff checks passed\n";
}
