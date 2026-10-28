#pragma once
#include <cstdint>

namespace cm {
// Cumulative transport diagnostics. Reconnect/stop clears in-flight RX/TX bytes, not counters.
struct MqttBoundedIoStats {
  uint32_t rxFrames = 0;
  uint32_t rxTimeouts = 0;
  uint32_t rxOversize = 0;
  uint32_t rxMalformed = 0;
  uint32_t txFailures = 0;
  uint32_t txBackpressure = 0;
  uint32_t txPartialWrites = 0;
  uint32_t txTimeouts = 0;
  uint32_t txOverflows = 0;
  uint32_t connectFailures = 0;
  uint32_t rejectedHosts = 0;
};
} // namespace cm
