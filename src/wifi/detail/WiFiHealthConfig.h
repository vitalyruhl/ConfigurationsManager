#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace cm {
namespace detail {
struct WiFiHealthConfig {
  std::array<char, 16> target{};
  uint32_t timeoutMinutes = 0;
  bool targetChanged = false;
  bool timeoutChanged = false;
};

// HTTP callbacks queue updates; only the application loop touches WiFi/socket state.
class WiFiHealthConfigMailbox {
  std::mutex mutex_;
  WiFiHealthConfig pending_;

public:
  bool setTarget(const char* value) {
    if (!value || std::strlen(value) >= pending_.target.size())
      return false;
    std::lock_guard<std::mutex> lock(mutex_);
    std::memcpy(pending_.target.data(), value, std::strlen(value) + 1);
    pending_.targetChanged = true;
    return true;
  }
  bool setTimeoutMinutes(uint32_t minutes) {
    if (minutes > 9999)
      return false;
    std::lock_guard<std::mutex> lock(mutex_);
    pending_.timeoutMinutes = minutes;
    pending_.timeoutChanged = true;
    return true;
  }
  bool take(WiFiHealthConfig& output) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!pending_.targetChanged && !pending_.timeoutChanged)
      return false;
    output = pending_;
    pending_.targetChanged = pending_.timeoutChanged = false;
    return true;
  }
};
} // namespace detail
} // namespace cm
