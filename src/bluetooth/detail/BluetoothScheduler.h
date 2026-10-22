#pragma once
#include "../BluetoothTypes.h"
#include <atomic>
#include <functional>
namespace cm {
namespace detail {
// Only pause/resume cross task boundaries. Other mutations belong to the worker.
template <class Backend>
class BluetoothScheduler {
public:
  explicit BluetoothScheduler(Backend& backend)
      : backend_(backend) {}
  bool attachOnce() {
    return !attached_.exchange(true);
  }
  void configure(bool enabled, bool advertisements, bool gatt, uint32_t now) {
    cancel();
    enabled_ = enabled;
    advertisements_ = advertisements;
    gatt_ = gatt;
    retryPending_ = false;
    retryStep_ = 0;
    scanStarted_ = false;
    lastScan_ = now;
    state_ = enabled ? BleState::Idle : BleState::Disabled;
  }
  uint32_t invalidate() {
    return ++generation_;
  }
  uint32_t generation() const {
    return generation_.load();
  }
  bool accepts(uint32_t id) const {
    return id == generation() && !isPaused();
  }
  void requestPause(BlePauseReason reason) {
    const auto bit = static_cast<uint8_t>(reason);
    if (!(pause_.fetch_or(bit) & bit))
      invalidate();
  }
  void requestResume(BlePauseReason reason) {
    pause_.fetch_and(~static_cast<uint8_t>(reason));
  }
  bool isPaused() const {
    return pause_.load() != 0;
  }
  bool connected() const {
    return connected_.load();
  }
  bool scanning() const {
    return scanning_.load();
  }
  BleState state() const {
    return state_.load();
  }
  bool queryPending() const {
    return queryPending_.load();
  }
  uint32_t queryId() const {
    return queryId_.load();
  }
  bool completeQuery(uint32_t id, uint32_t epoch) {
    if (!accepts(epoch) || !queryPending_ || !id || id != queryId())
      return false;
    queryPending_ = false;
    return true;
  }
  void cancel() {
    if (scanning_.exchange(false))
      backend_.stopScan();
    backend_.cancel();
    connected_ = false;
    queryPending_ = false;
  }
  bool requestConnect(uint32_t now) {
    if (!enabled_ || !gatt_ || isPaused() || connected_)
      return false;
    if (scanning_.exchange(false))
      backend_.stopScan();
    state_ = BleState::Connecting;
    const auto epoch = generation();
    const bool ok = backend_.connect();
    if (!accepts(epoch)) {
      cancel();
      state_ = BleState::Paused;
      return false;
    }
    connected_ = ok;
    if (ok) {
      retryPending_ = false;
      retryStep_ = 0;
      state_ = BleState::Connected;
    } else {
      state_ = BleState::Error;
      scheduleRetry(now);
    }
    return true;
  }
  bool write(const uint8_t* data, size_t size, uint32_t now) {
    if (!data || !size || size > 272 || !connected_ || isPaused() || queryPending_)
      return false;
    if (++queryId_ == 0)
      ++queryId_;
    queryPending_ = true;
    queryAt_ = now;
    if (!backend_.write(data, size)) {
      queryPending_ = false;
      return false;
    }
    return true;
  }
  void disconnected(uint32_t now) {
    connected_ = false;
    queryPending_ = false;
    if (enabled_ && gatt_)
      scheduleRetry(now);
    state_ = enabled_ ? BleState::Idle : BleState::Disabled;
  }
  void tick(uint32_t now) {
    if (isPaused()) {
      if (!pauseApplied_) {
        cancel();
        invalidate();
        retryPending_ = false;
        pauseApplied_ = true;
      }
      state_ = BleState::Paused;
      return;
    }
    if (pauseApplied_) {
      pauseApplied_ = false;
      scanStarted_ = false;
      if (gatt_)
        scheduleRetry(now);
    }
    if (!enabled_) {
      state_ = BleState::Disabled;
      return;
    }
    if (queryPending_ && uint32_t(now - queryAt_) >= 2000) {
      cancel();
      invalidate();
      scheduleRetry(now);
      state_ = BleState::Error;
    }
    if (retryPending_ && uint32_t(now - retryAt_) >= retryDelay_)
      requestConnect(now);
    if (isPaused())
      return;
    if (scanning_ && uint32_t(now - lastScan_) >= 1000) {
      backend_.stopScan();
      scanning_ = false;
    }
    if (advertisements_ && !scanning_ && (!scanStarted_ || uint32_t(now - lastScan_) >= 5000)) {
      lastScan_ = now;
      scanStarted_ = true;
      backend_.scan();
      scanning_ = true;
    }
    if (state_ != BleState::Error)
      state_ = connected_ ? BleState::Connected : (scanning_ ? BleState::Scanning : BleState::Idle);
  }

private:
  void scheduleRetry(uint32_t now) {
    retryAt_ = now;
    retryDelay_ = retryStep_ < 5 ? (1000u << retryStep_) : 30000u;
    if (retryStep_ < 5)
      ++retryStep_;
    retryPending_ = true;
  }
  Backend& backend_;
  std::atomic<uint32_t> generation_{1}, queryId_{0};
  std::atomic<bool> queryPending_{false};
  std::atomic<uint8_t> pause_{0};
  std::atomic<bool> attached_{false}, connected_{false}, scanning_{false};
  std::atomic<BleState> state_{BleState::Disabled};
  bool enabled_ = false, advertisements_ = false, gatt_ = false, pauseApplied_ = false;
  bool retryPending_ = false, scanStarted_ = false;
  uint8_t retryStep_ = 0;
  uint32_t lastScan_ = 0, retryAt_ = 0, retryDelay_ = 1000, queryAt_ = 0;
};
} // namespace detail
} // namespace cm
