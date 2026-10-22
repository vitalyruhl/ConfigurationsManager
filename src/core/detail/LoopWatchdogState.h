#pragma once
#include <atomic>
#include <cstdint>
namespace cm {
namespace detail {
// Configure/heartbeat belong to the loop owner. The observer never waits for a writer.
class LoopWatchdogState {
  std::atomic<uint32_t> timeoutMs_{30000}, lastMs_{0};
  std::atomic<bool> armed_{false};
  mutable std::atomic_flag busy_ = ATOMIC_FLAG_INIT;
  void lock() {
    while (busy_.test_and_set(std::memory_order_acquire)) {}
  }
  void unlock() const {
    busy_.clear(std::memory_order_release);
  }

public:
  static bool validTimeout(uint32_t ms) {
    return ms == 0 || (ms >= 1000 && ms <= 3600000);
  }
  bool configure(uint32_t ms) {
    if (!validTimeout(ms))
      return false;
    lock();
    timeoutMs_.store(ms, std::memory_order_relaxed);
    if (!ms)
      armed_.store(false, std::memory_order_relaxed);
    unlock();
    return true;
  }
  void heartbeat(uint32_t now) {
    lock();
    if (timeoutMs_.load(std::memory_order_relaxed)) {
      lastMs_.store(now, std::memory_order_relaxed);
      armed_.store(true, std::memory_order_relaxed);
    }
    unlock();
  }
  uint32_t timeoutMs() const {
    return timeoutMs_.load(std::memory_order_relaxed);
  }
  bool expired(uint32_t now) const {
    if (busy_.test_and_set(std::memory_order_acquire))
      return false;
    const uint32_t elapsed = now - lastMs_.load(std::memory_order_relaxed);
    const uint32_t timeout = timeoutMs_.load(std::memory_order_relaxed);
    // A sampled observer clock can precede a concurrently delivered heartbeat.
    const bool due = armed_.load(std::memory_order_relaxed) && timeout && elapsed < 0x80000000u && elapsed >= timeout;
    unlock();
    return due;
  }
};
} // namespace detail
} // namespace cm
