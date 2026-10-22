#include "bluetooth/detail/BluetoothCommandFence.h"
#include "bluetooth/detail/BluetoothScheduler.h"
#include "bluetooth/detail/BoundedBleQueue.h"
#include "bluetooth/detail/BleWrite.h"
#include <cassert>
#include <condition_variable>
#include <thread>
#include <future>
#include <chrono>
#include <iostream>

// [MOCKED!] Radio I/O only; queue, scheduler and command fence are production code.
struct FenceBackend {
  int writes = 0;
  bool connected = true, block = false, entered = false, release = false;
  std::mutex mutex;
  std::condition_variable changed;
  void scan() {}
  void stopScan() {}
  void cancel() {
    connected = false;
  }
  bool connect() {
    return connected;
  }
  bool write(const uint8_t*, size_t) {
    std::unique_lock<std::mutex> lock(mutex);
    ++writes;
    entered = true;
    changed.notify_all();
    if (block)
      changed.wait(lock, [this] { return release; });
    return connected;
  }
};
int main() {
  using namespace cm;
  using namespace cm::detail;
  FenceBackend backend;
  BluetoothScheduler<FenceBackend> scheduler(backend);
  scheduler.configure(true, false, true, 0);
  backend.connected = true;
  assert(scheduler.requestConnect(0));
  BluetoothCommandFence fence;
  BoundedBleQueue<BleCommand, 8> queue;
  const uint8_t oldBytes[] = {1, 2}, readBytes[] = {3, 4};
  BleCommand old{};
  assert(makeWriteCommand(old, oldBytes, sizeof(oldBytes), scheduler.generation()));
  assert(queue.push(old));
  fence.cancel(scheduler);
  BleCommand popped{};
  assert(queue.pop(popped));
  assert(fence.writeIfCurrent(scheduler, popped, 1) == BleWriteResult::Discarded && backend.writes == 0);
  // Reproduce the normal worker generation-change reconnect lifecycle, without a settings mutation.
  scheduler.cancel();
  scheduler.disconnected(2);
  assert(!backend.connected);
  backend.connected = true;
  assert(scheduler.requestConnect(3));
  BleCommand read{};
  assert(makeWriteCommand(read, readBytes, sizeof(readBytes), scheduler.generation()));
  assert(fence.writeIfCurrent(scheduler, read, 4) == BleWriteResult::Written && backend.writes == 1);
  assert(scheduler.completeQuery(scheduler.queryId(), scheduler.generation()));
  scheduler.requestPause(BlePauseReason::Ota);
  read.generation = scheduler.generation();
  assert(fence.writeIfCurrent(scheduler, read, 5) == BleWriteResult::Failed && backend.writes == 1);
  scheduler.requestResume(BlePauseReason::Ota);

  // A backend write that already owns the fence completes before cancel returns.
  backend.block = true;
  backend.entered = false;
  read.generation = scheduler.generation();
  std::thread writer([&] { assert(fence.writeIfCurrent(scheduler, read, 6) == BleWriteResult::Written); });
  {
    std::unique_lock<std::mutex> lock(backend.mutex);
    backend.changed.wait(lock, [&] { return backend.entered; });
  }
  const auto before = scheduler.generation();
  auto cancel = std::async(std::launch::async, [&] { fence.cancel(scheduler); });
  assert(cancel.wait_for(std::chrono::milliseconds(30)) == std::future_status::timeout);
  assert(scheduler.generation() == before);
  {
    std::lock_guard<std::mutex> lock(backend.mutex);
    backend.release = true;
    backend.changed.notify_all();
  }
  writer.join();
  cancel.get();
  assert(scheduler.generation() != before && backend.writes == 2);
  assert(fence.writeIfCurrent(scheduler, read, 7) == BleWriteResult::Discarded && backend.writes == 2);
  std::cout << "[MOCKED!] BLE queued-command cancellation, fresh read, pause and in-flight fence passed\n";
}
