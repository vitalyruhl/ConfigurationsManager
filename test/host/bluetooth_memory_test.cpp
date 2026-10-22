#ifndef CM_BLE_TX_MAX_BYTES
#define TEST_BLE_EXPECTED_TX_BYTES 272
#else
#define TEST_BLE_EXPECTED_TX_BYTES CM_BLE_TX_MAX_BYTES
#endif
#include "bluetooth/BluetoothTypes.h"
#include "bluetooth/detail/BoundedBleQueue.h"
#include "bluetooth/detail/BleWrite.h"
#include <cassert>
#include <cstring>
#include <iostream>
int main() {
  using namespace cm;
  // IO slots must not carry retained peer configuration beside the TX bytes.
  assert(sizeof(detail::BleCommand) <= sizeof(detail::BleCommand::data) + 16);
  // Retaining configuration must never reserve any RX/TX payload array.
  static_assert(sizeof(detail::BleConfiguration) <= 48, "Configuration must remain payload-free");
  uint8_t bytes[273]{};
  for (size_t i = 0; i < sizeof(bytes); ++i)
    bytes[i] = static_cast<uint8_t>(i);
  detail::BleCommand command{};
  const size_t capacity = sizeof(command.data);
  assert(detail::makeWriteCommand(command, bytes, capacity, 42));
  assert(command.type == detail::BleCommandType::Write && command.generation == 42);
  assert(command.size == capacity && std::memcmp(command.data, bytes, capacity) == 0);
  command.queryId = 73;
  const auto before = command;
  assert(!detail::makeWriteCommand(command, bytes, capacity + 1, 99));
  assert(!detail::makeWriteCommand(command, nullptr, 1, 99));
  assert(!detail::makeWriteCommand(command, bytes, 0, 99));
  assert(command.type == before.type && command.generation == 42 && command.queryId == 73 && command.size == capacity);
  assert(std::memcmp(command.data, before.data, capacity) == 0);
  detail::BoundedBleQueue<detail::BleCommand, 8> commands;
  for (uint32_t id = 1; id <= 8; ++id) {
    command.queryId = id;
    assert(commands.push(command));
  }
  command.queryId = 9;
  assert(!commands.push(command));
  for (uint32_t id = 1; id <= 8; ++id) {
    assert(commands.pop(command));
    assert(command.queryId == id);
  }
  assert(!commands.pop(command));
  BleEvent incoming{};
  incoming.size = 272;
  incoming.data[271] = 0xa5;
  detail::BoundedBleQueue<BleEvent, 16> events;
  for (int i = 0; i < 16; ++i)
    assert(events.push(incoming));
  assert(!events.push(incoming));
  BleEvent received{};
  assert(events.pop(received));
  assert(received.size == 272 && received.data[271] == 0xa5);
  static_assert(sizeof(detail::BleCommand::data) == TEST_BLE_EXPECTED_TX_BYTES, "Requested/default TX capacity must be preserved");
  std::cout << "BLE memory layout checks passed: command=" << sizeof(detail::BleCommand) << ", event=" << sizeof(BleEvent) << '\n';
}
