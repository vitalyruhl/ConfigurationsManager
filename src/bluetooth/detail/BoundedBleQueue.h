#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
namespace cm {
namespace detail {
// Callback producers never wait; contention is counted exactly like capacity loss.
template <class T, size_t Capacity>
class BoundedBleQueue {
public:
  bool push(const T& value) {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock() || count_ == Capacity) {
      ++drops_;
      return false;
    }
    values_[tail_] = value;
    tail_ = (tail_ + 1) % Capacity;
    ++count_;
    return true;
  }
  bool pop(T& value) {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock() || !count_)
      return false;
    value = values_[head_];
    head_ = (head_ + 1) % Capacity;
    --count_;
    return true;
  }
  void clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    head_ = tail_ = count_ = 0;
  }
  uint32_t drops() const {
    return drops_.load();
  }

private:
  std::array<T, Capacity> values_{};
  size_t head_ = 0, tail_ = 0, count_ = 0;
  std::mutex mutex_;
  std::atomic<uint32_t> drops_{0};
};
} // namespace detail
} // namespace cm
