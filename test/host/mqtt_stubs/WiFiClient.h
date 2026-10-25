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
inline int mockSendError = 0;
inline int mockLastSendFlags = 0;
inline uint32_t mockRecvCostUs = 0;
inline uint32_t mockRecvMillisCost = 0;
inline std::vector<uint8_t> mockIncoming;
inline std::vector<uint8_t> mockDelayedIncoming;
inline uint32_t mockDeliverAtMs = 0;
inline size_t mockDeliverAfterSentBytes = 0;
// [MOCKED!] Kernel acceptance is recorded separately from local socket lifetime.
// Accepted bytes remain observable after stop; this is not an lwIP cancellation model.
struct MockSendStep {
  int result = 0;
  int error = 0;
};
struct MockSocketRecord {
  int id = -1;
  std::vector<uint8_t> sentBytes;
  bool closed = false;
  std::vector<uint8_t>& acceptedBytes() {
    return sentBytes;
  }
};
struct MockSendEvent {
  int connection = -1;
  size_t requested = 0;
  int result = 0;
  int error = 0;
};
inline std::vector<MockSendStep> mockSendSteps;
inline size_t mockSendStepCursor = 0;
inline std::vector<MockSocketRecord> mockSockets;
inline std::vector<MockSendEvent> mockSendEvents;
inline std::vector<int> mockCloseEvents;
inline int mockNextConnectionId = 7;
inline uint32_t mockSendCostUs = 0;
inline MockSocketRecord* mockSocket(int id) {
  const auto found = std::find_if(mockSockets.begin(), mockSockets.end(), [id](const MockSocketRecord& socket) { return socket.id == id; });
  return found == mockSockets.end() ? nullptr : &*found;
}
inline void mockResetNetwork() {
  mockNowMs = mockMillisAdvance = mockMicroNow = 0;
  mockRecvCostUs = mockRecvMillisCost = mockSendCostUs = 0;
  mockIncoming.clear();
  mockDelayedIncoming.clear();
  mockDeliverAtMs = 0;
  mockDeliverAfterSentBytes = 0;
  mockSendError = 0;
  mockSendResult = -2;
  mockSendSteps.clear();
  mockSendStepCursor = 0;
  mockSockets.clear();
  mockSendEvents.clear();
  mockCloseEvents.clear();
  mockNextConnectionId = 7;
}
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
  int connectionId_ = -1;

public:
  int connect(IPAddress, uint16_t, int timeout) {
    mockConnectTimeout = timeout;
    connectionId_ = mockNextConnectionId++;
    mockSockets.push_back({connectionId_, {}, false});
    live_ = true;
    return 1;
  }
  int fd() const {
    return live_ ? connectionId_ : -1;
  }
  uint8_t connected() {
    return live_;
  }
  void stop() {
    if (live_) {
      mockCloseEvents.push_back(connectionId_);
      if (auto* socket = mockSocket(connectionId_))
        socket->closed = true;
    }
    live_ = false;
  }
};
