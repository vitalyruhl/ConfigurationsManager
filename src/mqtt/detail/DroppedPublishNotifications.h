#pragma once

#include <cstdint>
#include <functional>
#include <utility>

namespace cm {
namespace detail {

class DroppedPublishNotifications {
public:
  using Callback = std::function<void(uint32_t, uint32_t)>;

  void setCallback(Callback callback, uint32_t messages, uint32_t bytes) {
    callback_ = std::move(callback);
    messages_ = messages;
    bytes_ = bytes;
  }

  void dispatch(uint32_t messages, uint32_t bytes) {
    if (dispatching_ || !callback_)
      return;
    const uint32_t messageDelta = messages - messages_;
    const uint32_t byteDelta = bytes - bytes_;
    if (!messageDelta)
      return;
    messages_ = messages;
    bytes_ = bytes;
    // A copy remains valid if the callback changes its own registration.
    Callback callback = callback_;
    dispatching_ = true;
    callback(messageDelta, byteDelta);
    dispatching_ = false;
  }

private:
  Callback callback_;
  uint32_t messages_ = 0;
  uint32_t bytes_ = 0;
  bool dispatching_ = false;
};

} // namespace detail
} // namespace cm
