#pragma once
#include <cstdint>

namespace cm {
// Cumulative transport diagnostics. Reconnect/stop only clears the in-flight frame.
struct MqttBoundedIoStats {
  uint32_t rxFrames = 0;
  uint32_t rxTimeouts = 0;
  uint32_t rxOversize = 0;
  uint32_t rxMalformed = 0;
  uint32_t txFailures = 0;
  uint32_t connectFailures = 0;
  uint32_t rejectedHosts = 0;
};
} // namespace cm
