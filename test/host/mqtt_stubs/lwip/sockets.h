#pragma once
// [MOCKED!] Nonblocking lwIP socket boundary.
#include "WiFiClient.h"
#include <cerrno>
#define MSG_DONTWAIT 0x40
inline int recv(int, void* output, size_t size, int flags) {
  mockMicroNow += mockRecvCostUs;
  mockNowMs += mockRecvMillisCost;
  if (mockIncoming.empty() && !mockDelayedIncoming.empty() && mockNowMs >= mockDeliverAtMs) {
    mockIncoming.swap(mockDelayedIncoming);
  }
  if (flags != MSG_DONTWAIT)
    return -1;
  if (mockIncoming.empty()) {
    errno = EAGAIN;
    return -1;
  }
  const size_t count = std::min(size, mockIncoming.size());
  std::memcpy(output, mockIncoming.data(), count);
  mockIncoming.erase(mockIncoming.begin(), mockIncoming.begin() + count);
  return static_cast<int>(count);
}
inline int send(int, const void*, size_t size, int flags) {
  mockLastSendFlags = flags;
  return mockSendResult == -2 ? static_cast<int>(size) : mockSendResult;
}
