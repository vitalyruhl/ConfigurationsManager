#pragma once
#include <cstdint>
#include <functional>
#include <mutex>

enum class OtaStage { Starting, Succeeded, Failed, Aborted, TimedOut };
enum class OtaTransport { Arduino, Http };
struct OtaLifecycleEvent {
  OtaStage stage;
  OtaTransport transport;
  uint32_t sessionId;
  int errorCode;
};
namespace cm { namespace detail {
// Shared by the real upload paths; the observer must only queue nonblocking intent.
class OtaLifecycleState {
public:
  using Observer = std::function<void(const OtaLifecycleEvent&)>;
  explicit OtaLifecycleState(Observer observer = {}) : observer_(std::move(observer)) {}
  void setObserver(Observer observer) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    observer_ = std::move(observer);
  }
  bool tryStart(OtaTransport transport) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (active_ || succeeded_) return false;
    if (++next_ == 0) ++next_;
    active_ = next_;
    transport_ = transport;
    emit(OtaStage::Starting, active_, 0);
    return true;
  }
  void finish(uint32_t sessionId, OtaStage stage, int errorCode) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!sessionId || sessionId != active_ || stage == OtaStage::Starting) return;
    active_ = 0;
    succeeded_ = stage == OtaStage::Succeeded;
    if(succeeded_)succeededSession_=sessionId;
    emit(stage, sessionId, errorCode);
  }
  bool expire(uint32_t sessionId, uint32_t now, uint32_t activity, uint32_t timeout) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!sessionId || sessionId != active_ || uint32_t(now-activity) < timeout) return false;
    finish(sessionId,OtaStage::TimedOut,0);return true;
  }
  bool isSucceededOwner(uint32_t sessionId,OtaTransport transport) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return succeeded_ && sessionId && sessionId==succeededSession_ && transport_==transport;
  }
  bool isActive(OtaTransport transport) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return active_ && transport_==transport;
  }
  uint32_t activeSessionId() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return active_;
  }
private:
  void emit(OtaStage stage, uint32_t id, int error) {
    if (observer_) observer_({stage, transport_, id, error});
  }
  mutable std::recursive_mutex mutex_;
  Observer observer_;
  uint32_t next_ = 0, active_ = 0, succeededSession_ = 0;
  OtaTransport transport_ = OtaTransport::Arduino;
  bool succeeded_ = false;
};
}} // namespace cm::detail
