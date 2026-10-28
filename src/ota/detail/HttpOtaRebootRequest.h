#pragma once
#include <atomic>
#include <cstdint>

namespace cm {
namespace detail {
// A committed HTTP update can request one reboot; only the application loop consumes it.
class HttpOtaRebootRequest {
public:
  bool request(uint32_t now) {
    uint32_t expected = Idle;
    if (!state_.compare_exchange_strong(expected, Publishing, std::memory_order_relaxed))
      return false;
    requestedAt_ = now;
    state_.store(Pending, std::memory_order_release);
    return true;
  }
  bool takeIfDue(uint32_t now) {
    if (state_.load(std::memory_order_acquire) != Pending)
      return false;
    const uint32_t elapsed = now - requestedAt_;
    // Also reject a loop timestamp captured just before a concurrent request.
    if (elapsed < 500UL || elapsed >= 0x80000000UL)
      return false;
    uint32_t expected = Pending;
    return state_.compare_exchange_strong(expected, Consumed, std::memory_order_acq_rel);
  }

private:
  enum : uint32_t { Idle,
                    Publishing,
                    Pending,
                    Consumed };
  std::atomic<uint32_t> state_{Idle};
  uint32_t requestedAt_ = 0; // Published once by the release/acquire state handoff.
};
} // namespace detail
} // namespace cm
