#pragma once
#include "../BluetoothTypes.h"
#include <atomic>
#include <mutex>
namespace cm { namespace detail {
// Latest configuration is durable state, independent of disposable IO epochs.
class BluetoothDispatch {
public:
  void setEnabled(bool value) { enabled_.store(value); }
  bool enabled() const { return enabled_.load(); }
  void submitConfiguration(const BleCommand& command) {
    std::lock_guard<std::mutex> lock(mutex_);
    desired_=command;
    if(++revision_==0)++revision_;
  }
  bool configurationPending() const {
    std::lock_guard<std::mutex> lock(mutex_);return revision_!=applied_;
  }
  void requestReapply() {
    std::lock_guard<std::mutex> lock(mutex_);
    if(revision_)applied_=revision_-1;
  }
  template<class Scheduler,class Apply> bool dispatchConfiguration(Scheduler& scheduler,uint32_t now,Apply apply) {
    BleCommand command{};uint32_t revision=0;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if(revision_==applied_)return false;
      command=desired_;revision=revision_;
    }
    apply(command);
    scheduler.configure(command.enabled,command.hasAdvertisement,command.hasGatt,now);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      // A concurrently published replacement remains pending until it is applied.
      if(revision_==revision)applied_=revision;
    }
    return true;
  }
  // Delivery identity is application-loop-local; no token is sampled at acknowledgement time.
  bool beginDelivery(const BleEvent& event,uint32_t currentQuery,bool pending) {
    endDelivery();
    if(event.type!=BleEventType::Notification)return true;
    if(!pending || !event.queryId || event.queryId!=currentQuery)return false;
    deliveredEpoch_=event.generation;deliveredQuery_=event.queryId;return true;
  }
  void endDelivery() { deliveredQuery_=0;deliveredEpoch_=0; }
  bool makeCompletion(uint32_t currentEpoch,BleCommand& command) const {
    if(!deliveredQuery_ || deliveredEpoch_!=currentEpoch)return false;
    command={};command.type=BleCommandType::CompleteQuery;
    command.generation=deliveredEpoch_;command.queryId=deliveredQuery_;return true;
  }
private:
  mutable std::mutex mutex_;
  BleCommand desired_{};
  uint32_t revision_=0,applied_=0;
  std::atomic<bool> enabled_{false};
  uint32_t deliveredEpoch_=0,deliveredQuery_=0;
};
}}
