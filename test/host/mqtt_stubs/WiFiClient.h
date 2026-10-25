#pragma once
// [MOCKED!] ESP32 client and clock boundary for the real bounded MQTT adapter.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
inline uint32_t mockNowMs = 0;
inline uint32_t mockMillisAdvance = 0;
inline uint32_t mockMicroNow = 0;
inline uint32_t millis() {
  const uint32_t result = mockNowMs;
  mockNowMs += mockMillisAdvance;
  return result;
}
inline uint32_t micros() {
  return mockMicroNow;
}
inline int mockConnectTimeout = 0;
inline int mockSendResult = -2;
inline int mockLastSendFlags = 0;
inline uint32_t mockRecvCostUs = 0;
inline uint32_t mockRecvMillisCost = 0;
inline std::vector<uint8_t> mockIncoming;
inline std::vector<uint8_t> mockDelayedIncoming;
inline uint32_t mockDeliverAtMs = 0;
class IPAddress {
public:
  IPAddress() = default;
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
  explicit IPAddress(const uint8_t*) {}
  uint8_t operator[](int) const {
    return 192;
  }
  static bool fromString(const char* text) {
    return text && std::strcmp(text, "192.0.2.1") == 0;
  }
};
class Print {
public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t*, size_t) = 0;
};
class Stream : public Print {
public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
};
class Client : public Stream {
public:
  virtual int connect(IPAddress, uint16_t) = 0;
  virtual int connect(const char*, uint16_t) = 0;
  int read() override = 0;
  virtual int read(uint8_t*, size_t) = 0;
  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
  virtual operator bool() = 0;
};
class WiFiClient {
  bool live_ = false;

public:
  int connect(IPAddress, uint16_t, int timeout) {
    mockConnectTimeout = timeout;
    live_ = true;
    return 1;
  }
  int fd() const {
    return live_ ? 7 : -1;
  }
  uint8_t connected() {
    return live_;
  }
  void stop() {
    live_ = false;
  }
};
