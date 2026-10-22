#include "core/detail/LoopWatchdogState.h"
#include "core/LoopWatchdog.h"
#include <esp_timer.h>
#include <esp_system.h>
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>

// [MOCKED!] Only ESP timer, clock, reset reason and ROM logger are replaced.
// Production state and the complete production adapter are compiled unchanged.
struct FakeTimer {
  esp_timer_create_args_t args;
  bool active = false;
  bool deleted = false;
};
static std::vector<std::unique_ptr<FakeTimer>> timers;
static int64_t clockUs = 0;
static unsigned creates = 0, starts = 0, stops = 0, deletes = 0, restarts = 0, logs = 0;
static bool failCreate = false, failStart = false, callbackDuringSdk = false;
static esp_reset_reason_t resetReason = ESP_RST_POWERON;
int64_t esp_timer_get_time() {
  return clockUs;
}
esp_reset_reason_t esp_reset_reason() {
  return resetReason;
}
void esp_restart() {
  ++restarts;
  resetReason = ESP_RST_SW;
}
int ets_printf(const char*, ...) {
  ++logs;
  return 0;
}
static void inlineCallback(FakeTimer* timer) {
  if (callbackDuringSdk)
    timer->args.callback(timer->args.arg);
}
int esp_timer_create(const esp_timer_create_args_t* args, esp_timer_handle_t* handle) {
  ++creates;
  if (failCreate)
    return -1;
  assert(args->dispatch_method == ESP_TIMER_TASK && args->skip_unhandled_events);
  timers.emplace_back(new FakeTimer{*args});
  *handle = timers.back().get();
  inlineCallback(*handle);
  return ESP_OK;
}
int esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t period) {
  ++starts;
  assert(period == 1000000);
  inlineCallback(timer);
  if (failStart || timer->active)
    return -1;
  timer->active = true;
  return ESP_OK;
}
int esp_timer_stop(esp_timer_handle_t timer) {
  ++stops;
  inlineCallback(timer);
  timer->active = false;
  return ESP_OK;
}
int esp_timer_delete(esp_timer_handle_t timer) {
  ++deletes;
  inlineCallback(timer);
  assert(!timer->active);
  timer->deleted = true;
  return ESP_OK;
}
static void at(uint64_t ms) {
  clockUs = static_cast<int64_t>(ms * 1000);
}
static void poll(FakeTimer* timer) {
  assert(!timer->deleted);
  timer->args.callback(timer->args.arg);
}
static void portableState() {
  cm::detail::LoopWatchdogState state;
  assert(!state.expired(1000000));
  assert(state.configure(1000));
  assert(!state.expired(1000000));
  assert(state.configure(30000));
  state.heartbeat(100);
  assert(!state.expired(30099));
  assert(state.expired(30100));
  state.heartbeat(30100);
  assert(!state.expired(60099));
  assert(state.expired(60100));
  assert(!state.configure(999));
  assert(!state.configure(3600001));
  assert(state.timeoutMs() == 30000);
  assert(state.configure(3600000));
  assert(!state.expired(3630099));
  assert(state.expired(3630100));
  assert(state.configure(0));
  state.heartbeat(4000000);
  assert(!state.expired(8000000));
  assert(state.configure(1000));
  assert(!state.expired(8000000));
  state.heartbeat(8000000);
  assert(!state.expired(8000999));
  assert(state.expired(8001000));
  state.heartbeat(0xffffff00u);
  assert(!state.expired(0x2e7u));
  assert(state.expired(0x2e8u));
  state.heartbeat(1234);
  assert(!state.expired(1233));
  assert(!state.expired(2233));
  assert(state.expired(2234));
  std::puts("PASS: state startup, bounds, heartbeat, disable/rearm, rollover and stale observer clock");
}
static void startupExpiry() {
  const auto before = creates;
  const auto resets = restarts;
  {
    cm::LoopWatchdog watch;
    assert(!watch.isRunning());
    at(500000);
    assert(creates == before);
    assert(watch.configure(30000));
    assert(!watch.isRunning());
    assert(creates == before);
    assert(watch.heartbeat());
    assert(watch.isRunning());
    assert(creates == before + 1);
    auto* timer = timers.back().get();
    at(529999);
    poll(timer);
    assert(restarts == resets);
    at(530000);
    poll(timer);
    assert(restarts == resets + 1);
    assert(!watch.isRunning());
    assert(cm::LoopWatchdog::consumeRestartMarker());
    assert(!cm::LoopWatchdog::consumeRestartMarker());
  }
  assert(timers.back()->deleted);
  std::puts("PASS: adapter arms on first beat, independent exact timeout and one-shot reset evidence");
}
static void disableAndFailure() {
  cm::LoopWatchdog watch;
  at(0);
  assert(watch.heartbeat());
  auto* timer = timers.back().get();
  assert(!watch.configure(999));
  at(29999);
  poll(timer);
  const auto resets = restarts;
  assert(watch.configure(0));
  assert(!watch.isRunning());
  at(999999);
  poll(timer);
  assert(restarts == resets);
  const auto attempts = starts;
  assert(watch.heartbeat());
  assert(starts == attempts);
  assert(watch.configure(1000));
  assert(!watch.isRunning());
  poll(timer);
  assert(restarts == resets);
  assert(watch.heartbeat());
  assert(starts == attempts + 1);
  at(1000998);
  poll(timer);
  assert(restarts == resets);
  assert(watch.heartbeat());
  at(1001997);
  poll(timer);
  assert(restarts == resets);
  std::puts("PASS: disable/enable requires a new beat; invalid configuration preserves state");
}
static void failedTimer() {
  {
    cm::LoopWatchdog watch;
    failCreate = true;
    const auto attempts = creates, messages = logs;
    assert(!watch.heartbeat());
    assert(!watch.isRunning());
    for (unsigned i = 0; i < 10000; ++i)
      assert(!watch.heartbeat());
    assert(creates == attempts + 1 && logs == messages + 1);
    failCreate = false;
    assert(watch.configure(30000));
    assert(watch.heartbeat());
    assert(creates == attempts + 2);
  }
  {
    cm::LoopWatchdog watch;
    failStart = true;
    const auto attempts = starts, messages = logs;
    assert(!watch.heartbeat());
    for (unsigned i = 0; i < 10000; ++i)
      assert(!watch.heartbeat());
    assert(starts == attempts + 1 && logs == messages + 1);
    failStart = false;
    assert(watch.configure(30000));
    assert(watch.heartbeat());
    assert(starts == attempts + 2);
  }
  std::puts("PASS: create/start failures do not retry or log every loop; configure permits recovery");
}
static void ownershipAndLateCallbacks() {
  esp_timer_create_args_t oldCallback{};
  const auto resets = restarts;
  callbackDuringSdk = true;
  {
    cm::LoopWatchdog first;
    at(100);
    assert(first.heartbeat());
    oldCallback = timers.back()->args;
    {
      cm::LoopWatchdog second;
      assert(!second.configure(1000));
      assert(!second.heartbeat());
      assert(!second.isRunning());
    }
    assert(first.isRunning());
  }
  assert(timers.back()->deleted);
  oldCallback.callback(oldCallback.arg);
  assert(restarts == resets);
  {
    cm::LoopWatchdog next;
    assert(next.configure(1000));
    at(100);
    assert(next.heartbeat());
    at(5000);
    oldCallback.callback(oldCallback.arg);
    assert(restarts == resets);
    poll(timers.back().get());
    assert(restarts == resets + 1);
  }
  callbackDuringSdk = false;
  assert(cm::LoopWatchdog::consumeRestartMarker());
  const auto before = creates;
  { cm::LoopWatchdog neverArmed; }
  assert(creates == before);
  assert(deletes == timers.size());
  assert(stops >= deletes);
  std::puts("PASS: single owner, teardown, old-generation callbacks and reentrant SDK callbacks");
}
int main() {
  portableState();
  startupExpiry();
  disableAndFailure();
  failedTimer();
  ownershipAndLateCallbacks();
  std::puts("Loop watchdog host checks passed");
}
