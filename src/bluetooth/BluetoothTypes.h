#pragma once
#include <cstddef>
#include <cstdint>
namespace cm {
struct BleAddress { char text[18]{}; uint8_t type = 0; };
enum class BlePauseReason : uint8_t { Ota = 1, Configuration = 2 };
enum class BleEventType { Advertisement, Connected, Disconnected, Notification, Error };
enum class BleState { Disabled, Idle, Scanning, Connecting, Connected, Paused, Error };
struct BleEvent {
  BleEventType type = BleEventType::Error;
  BleAddress peer{};
  uint32_t receivedMs = 0, generation = 0, queryId = 0;
  int8_t rssi = 0;
  int errorCode = 0;
  uint16_t size = 0;
  uint8_t data[272]{};
};
namespace detail {
enum class BleCommandType { Configure, Connect, Write, CompleteQuery };
struct BleCommand {
  BleCommandType type = BleCommandType::Connect;
  uint32_t generation = 0, queryId = 0;
  BleAddress advertisement{}, gatt{};
  uint16_t service = 0, notify = 0, write = 0, size = 0;
  bool enabled = false, hasAdvertisement = false, hasGatt = false;
  uint8_t data[272]{};
};
}
} // namespace cm
