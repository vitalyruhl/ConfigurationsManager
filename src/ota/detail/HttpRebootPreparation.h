#pragma once
#include <atomic>
#include <cstdint>

namespace cm {
namespace detail {
// Main-loop-owned preparation with a thread-safe terminal diagnostic.
class HttpRebootPreparation {
public:
  enum class Result { Idle,
                      Waiting,
                      Ready,
                      TimedOut };
  void begin(uint32_t now) {
    if (state_.load() != State::Idle)
      return;
    startedAt_ = now;
    state_.store(State::Waiting);
  }
  template <class Prepare>
  Result poll(uint32_t now, Prepare prepare) {
    if (state_.load() != State::Waiting)
      return Result::Idle;
    const uint32_t elapsed = now - startedAt_;
    if (elapsed >= 0x80000000UL)
      return Result::Waiting;
    if (elapsed >= 10000UL) {
      state_.store(State::Blocked);
      return Result::TimedOut;
    }
    if (!prepare())
      return Result::Waiting;
    state_.store(State::Done);
    return Result::Ready;
  }
  bool blocked() const {
    return state_.load() == State::Blocked;
  }

private:
  enum class State { Idle,
                     Waiting,
                     Done,
                     Blocked };
  std::atomic<State> state_{State::Idle};
  uint32_t startedAt_ = 0;
};
} // namespace detail
} // namespace cm
