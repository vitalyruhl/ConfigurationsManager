#pragma once
#include <cstddef>
#include <cstdint>
#ifndef CM_BLE_TX_MAX_BYTES
#define CM_BLE_TX_MAX_BYTES 272
#endif
static_assert(CM_BLE_TX_MAX_BYTES >= 1 && CM_BLE_TX_MAX_BYTES <= 272,
              "CM_BLE_TX_MAX_BYTES must be between 1 and 272");
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
// Peer configuration is durable state and never carries IO payload storage.
struct BleConfiguration {
  BleAddress advertisement{}, gatt{};
  uint16_t service = 0, notify = 0, write = 0;
  bool enabled = false, hasAdvertisement = false, hasGatt = false;
};
enum class BleCommandType { Connect, Write, CompleteQuery };
struct BleCommand {
  BleCommandType type = BleCommandType::Connect;
  uint32_t generation = 0, queryId = 0;
  uint16_t size = 0;
  uint8_t data[CM_BLE_TX_MAX_BYTES]{};
};
}
} // namespace cm
