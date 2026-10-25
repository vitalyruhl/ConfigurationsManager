#pragma once
// [MOCKED!] Minimal Arduino boundary for upstream PubSubClient 2.8 integration.
#include "WiFiClient.h"
#include <cstdlib>
#include <cstring>
using boolean = bool;
inline void yield() {
  ++mockNowMs;
}
#define pgm_read_byte_near(pointer) (*reinterpret_cast<const uint8_t*>(pointer))
