#pragma once
#include "../BluetoothTypes.h"
#include <mutex>
namespace cm {
namespace detail {
enum class BleWriteResult { Discarded,
                            Written,
                            Failed };
// Serialize cancellation with the final worker admission, including backend I/O.
class BluetoothCommandFence {
  std::mutex mutex_;

public:
  template <class Scheduler>
  void cancel(Scheduler& scheduler) {
    std::lock_guard<std::mutex> lock(mutex_);
    scheduler.invalidate();
  }
  template <class Scheduler>
  BleWriteResult writeIfCurrent(Scheduler& scheduler, const BleCommand& command, uint32_t now) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (command.generation != scheduler.generation())
      return BleWriteResult::Discarded;
    return scheduler.write(command.data, command.size, now) ? BleWriteResult::Written : BleWriteResult::Failed;
  }
};
} // namespace detail
} // namespace cm
