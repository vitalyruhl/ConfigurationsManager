#pragma once
#include "OtaLifecycleState.h"
#include <mutex>
namespace cm {
namespace detail {
// Polling yields ownership to an admitted HTTP upload, so ordinary servicing
// cannot spuriously reject its chunks or completion on another task.
class OtaUploadGate {
public:
  explicit OtaUploadGate(OtaLifecycleState& state)
      : state_(state) {}
  std::unique_lock<std::recursive_mutex> tryRequest() {
    return std::unique_lock<std::recursive_mutex>(mutex_, std::try_to_lock);
  }
  // Response/reboot handoff after committed success touches no flash state.
  // It stays bound to the accepted owner and does not compete for the flash lock.
  template <class Completion>
  bool completeHttp(uint32_t sessionId, Completion complete) {
    if (!state_.isSucceededOwner(sessionId, OtaTransport::Http))
      return false;
    complete();
    return true;
  }
  std::unique_lock<std::recursive_mutex> tryService(bool maintenance) {
    std::unique_lock<std::recursive_mutex> lock(mutex_, std::defer_lock);
    if (!maintenance && state_.isActive(OtaTransport::Http))
      return lock;
    if (!lock.try_lock())
      return lock;
    if (!maintenance && state_.isActive(OtaTransport::Http))
      lock.unlock();
    return lock;
  }

private:
  OtaLifecycleState& state_;
  std::recursive_mutex mutex_;
};
} // namespace detail
} // namespace cm
