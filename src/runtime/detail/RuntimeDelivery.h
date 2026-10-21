#pragma once
#include <atomic>
#include <memory>
#include <utility>

namespace cm {
struct RuntimeDeliveryState { std::atomic<bool> busy{false}; };
// Shared state survives detached subscription copies and disconnect callbacks.
class RuntimeDeliveryReservation {
  std::shared_ptr<RuntimeDeliveryState> state_;
public:
  explicit RuntimeDeliveryReservation(const std::shared_ptr<RuntimeDeliveryState>& state) {
    bool expected = false;
    if (state && state->busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) state_ = state;
  }
  RuntimeDeliveryReservation(const RuntimeDeliveryReservation&) = delete;
  RuntimeDeliveryReservation& operator=(const RuntimeDeliveryReservation&) = delete;
  RuntimeDeliveryReservation(RuntimeDeliveryReservation&&) noexcept = default;
  RuntimeDeliveryReservation& operator=(RuntimeDeliveryReservation&& other) noexcept {
    if (this != &other) { release(); state_ = std::move(other.state_); }
    return *this;
  }
  ~RuntimeDeliveryReservation() { release(); }
  explicit operator bool() const { return bool(state_); }
private:
  void release() {
    if (state_) { state_->busy.store(false, std::memory_order_release); state_.reset(); }
  }
};
} // namespace cm
