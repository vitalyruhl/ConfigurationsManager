#pragma once
// [MOCKED!] Nonblocking lwIP socket boundary.
#include "WiFiClient.h"
#include <cerrno>
#define MSG_DONTWAIT 0x40
inline int recv(int fd, void* output, size_t size, int flags) {
  mockMicroNow += mockRecvCostUs;
  mockNowMs += mockRecvMillisCost;
  const auto* socket = mockSocket(fd);
  const bool sentEnough = !mockDeliverAfterSentBytes || (socket && socket->sentBytes.size() >= mockDeliverAfterSentBytes);
  if (mockIncoming.empty() && !mockDelayedIncoming.empty() && mockNowMs >= mockDeliverAtMs && sentEnough) {
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
inline int send(int fd, const void* bytes, size_t size, int flags) {
  mockLastSendFlags = flags;
  mockMicroNow += mockSendCostUs;
  MockSendStep step{mockSendResult, mockSendError};
  if (mockSendStepCursor < mockSendSteps.size())
    step = mockSendSteps[mockSendStepCursor++];
  const int result = step.result == -2 ? static_cast<int>(size) : step.result;
  errno = step.error;
  mockSendEvents.push_back({fd, size, result, step.error});
  if (result > 0) {
    if (auto* socket = mockSocket(fd)) {
      const auto* input = static_cast<const uint8_t*>(bytes);
      const size_t accepted = std::min(size, static_cast<size_t>(result));
      socket->acceptedBytes().insert(socket->acceptedBytes().end(), input, input + accepted);
    }
  }
  return result;
}
