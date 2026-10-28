#include "ota/detail/HttpOtaRebootRequest.h"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main() {
  cm::detail::HttpOtaRebootRequest first;
  assert(!first.takeIfDue(100000));
  assert(first.request(1000));
  assert(!first.request(1400));
  assert(!first.takeIfDue(999)); // An older loop timestamp cannot consume a concurrent request.
  assert(!first.takeIfDue(1499));
  assert(first.takeIfDue(1500));
  assert(!first.takeIfDue(2000) && !first.request(3000));
  cm::detail::HttpOtaRebootRequest wrap;
  assert(wrap.request(0xffffff00u));
  assert(!wrap.takeIfDue(0x000000f3u));
  assert(wrap.takeIfDue(0x000000f4u));
  cm::detail::HttpOtaRebootRequest zero;
  assert(zero.request(0));
  assert(!zero.takeIfDue(499) && zero.takeIfDue(500));
  // Competing callbacks cannot rewrite the winning timestamp or rearm the mailbox.
  for (unsigned round = 0; round < 64; ++round) {
    cm::detail::HttpOtaRebootRequest concurrent;
    std::atomic<unsigned> requests{0}, consumed{0};
    std::atomic<bool> start{false};
    std::vector<std::thread> callbacks;
    for (unsigned i = 0; i < 4; ++i)
      callbacks.emplace_back([&, i] {
        while (!start.load(std::memory_order_acquire))
          std::this_thread::yield();
        if (concurrent.request(1000 + i))
          ++requests;
      });
    start.store(true, std::memory_order_release);
    for (auto& thread : callbacks)
      thread.join();
    assert(requests == 1 && !concurrent.takeIfDue(1499));
    std::thread a([&] {if(concurrent.takeIfDue(1503)) ++consumed; });
    std::thread b([&] {if(concurrent.takeIfDue(1503)) ++consumed; });
    a.join();
    b.join();
    assert(consumed == 1 && !concurrent.request(9999));
  }
  // Acquire observes the timestamp only after the winning callback publishes it.
  for (unsigned round = 0; round < 64; ++round) {
    cm::detail::HttpOtaRebootRequest ordered;
    std::thread producer([&] { assert(ordered.request(0xfffffff0u)); });
    bool taken = false;
    while (!taken) {
      taken = ordered.takeIfDue(0x000001e4u);
      std::this_thread::yield();
    }
    producer.join();
    assert(!ordered.takeIfDue(0x000001e5u));
  }
  std::cout << "[MOCKED!] HTTP reboot one-shot timing/wrap/concurrent publication passed\n";
}
