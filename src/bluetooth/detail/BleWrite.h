#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "../BluetoothTypes.h"
namespace cm {
namespace detail {
// Validate before modifying the command, so rejected writes cannot alter queued work.
inline bool makeWriteCommand(BleCommand& command, const uint8_t* data, size_t size, uint32_t generation) {
  if (!data || !size || size > sizeof(command.data))
    return false;
  command = {};
  command.type = BleCommandType::Write;
  command.generation = generation;
  command.size = static_cast<uint16_t>(size);
  std::memcpy(command.data, data, size);
  return true;
}

template <class Characteristic>
bool writeCharacteristic(Characteristic* target, const uint8_t* data, size_t size) {
  if (!target || !data || !size || (!target->canWrite() && !target->canWriteNoResponse()))
    return false;
  return target->writeValue(data, size, target->canWrite());
}
} // namespace detail
} // namespace cm
