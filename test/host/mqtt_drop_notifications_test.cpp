// [MOCKED!] Callback delivery uses cumulative transport counters.
#include "mqtt/detail/DroppedPublishNotifications.h"
#include <cassert>
#include <cstdint>

int main() {
  cm::detail::DroppedPublishNotifications notifications;
  uint32_t calls = 0;
  uint32_t messages = 0;
  uint32_t bytes = 0;
  notifications.setCallback([&](uint32_t count, uint32_t size) {
    ++calls;
    messages += count;
    bytes += size;
    notifications.dispatch(15, 150); // Reentrant dispatch must wait for the next loop.
  }, 10, 100);
  notifications.dispatch(10, 100);
  assert(calls == 0);
  notifications.dispatch(12, 120);
  assert(calls == 1 && messages == 2 && bytes == 20);
  notifications.dispatch(15, 150);
  assert(calls == 2 && messages == 5 && bytes == 50);
  notifications.setCallback([&](uint32_t count, uint32_t size) {
    messages = count;
    bytes = size;
  }, UINT32_MAX, UINT32_MAX - 1);
  notifications.dispatch(1, 2);
  assert(messages == 2 && bytes == 4);
  notifications.setCallback([&](uint32_t, uint32_t) {
    ++calls;
    notifications.setCallback({}, 2, 4);
  }, 1, 2);
  notifications.dispatch(2, 4);
  notifications.dispatch(3, 6);
  assert(calls == 3);
  notifications.setCallback([&](uint32_t count, uint32_t size) {
    messages = count;
    bytes = size;
  }, 3, 6);
  notifications.dispatch(4, 9);
  assert(messages == 1 && bytes == 3);
}
