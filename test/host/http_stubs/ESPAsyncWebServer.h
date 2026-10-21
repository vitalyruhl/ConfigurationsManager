#pragma once
// [MOCKED!] Minimal pinned-platform shell; the production response subclass is unchanged.
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <utility>
#include <vector>
using String = std::string;
constexpr uint8_t ASYNC_WRITE_FLAG_COPY = 1;
enum WebResponseState { RESPONSE_SETUP, RESPONSE_HEADERS, RESPONSE_CONTENT, RESPONSE_WAIT_ACK, RESPONSE_END, RESPONSE_FAILED };
class AsyncClient {
public:
  size_t available = 16384, outstanding = 0, maximumOutstanding = 0, maximumOffer = 0, sends = 0;
  uint16_t mss = 512;
  size_t transmitted = 0, acknowledged = 0, timeoutEvents = 0;
  uint32_t nowMs = 1, lastTxMs = 0, lastAckMs = 0, ackTimeoutMs = 5000;
  std::function<void()> onTimeout;
  std::deque<size_t> acceptLimits;
  std::deque<bool> flushResults;
  std::string accepted, received;
  std::vector<const char*> offeredPointers;
  bool closed = false;
  std::function<void()> onClose;
  uint16_t getMss() const { return mss; }
  void setAckTimeout(uint32_t timeout) { ackTimeoutMs = timeout; }
  bool canSend() const { return space() > 0; }
  size_t space() const { return closed || outstanding >= available ? 0 : available - outstanding; }
  size_t add(const char* data, size_t length, uint8_t flags) {
    assert(flags == ASYNC_WRITE_FLAG_COPY);
    maximumOffer = std::max(maximumOffer, length);
    offeredPointers.push_back(data);
    size_t count = std::min(length, space());
    if (!acceptLimits.empty()) { count = std::min(count, acceptLimits.front()); acceptLimits.pop_front(); }
    accepted.append(data, count);
    outstanding += count;
    maximumOutstanding = std::max(maximumOutstanding, outstanding);
    return count;
  }
  bool send() {
    ++sends;
    bool result = true;
    if (!flushResults.empty()) { result = flushResults.front(); flushResults.pop_front(); }
    if (result) { transmitted = accepted.size(); lastTxMs = nowMs; }
    return result;
  }
  // Peer delivery and ACK feedback are independent events: payload can arrive
  // completely while its ACK is delayed or absent, as in the device trace.
  void receiveTransmitted() {
    received.append(accepted.data() + received.size(), transmitted - received.size());
  }
  // Only bytes handed to a successful output operation can generate TCP ACKs.
  size_t acknowledge(size_t length) {
    if (closed) return 0;
    receiveTransmitted();
    const size_t count = std::min(length, transmitted - acknowledged);
    acknowledged += count; outstanding -= count;
    if (count) lastAckMs = nowMs;
    return count;
  }
  void advance(uint32_t elapsed) {
    nowMs += elapsed;
    // Mirror the pinned AsyncTCP poll condition; its HTTP timeout callback closes.
    const uint32_t oneDay = 86400000;
    if (!closed && ackTimeoutMs && (lastAckMs - lastTxMs + oneDay) < oneDay
        && uint32_t(nowMs - lastTxMs) >= ackTimeoutMs) {
      ++timeoutEvents;
      if (onTimeout) onTimeout();
    }
  }
  void close() { if (closed) return; closed = true; if (onClose) onClose(); }
};
class AsyncWebServerRequest {
  AsyncClient* client_;
public:
  explicit AsyncWebServerRequest(AsyncClient* client) : client_(client) {
    client_->onTimeout = [client]() { client->close(); };
  }
  AsyncClient* client() { return client_; }
  uint8_t version() const { return 1; }
};
class AsyncWebServerResponse {
protected:
  int _code = 0;
  String _contentType;
  size_t _contentLength = 0, _headLength = 0, _sentLength = 0, _ackedLength = 0, _writtenLength = 0;
  bool _sendContentLength = true, _chunked = false;
  WebResponseState _state = RESPONSE_SETUP;
  std::vector<std::pair<String,String>> headers_;
public:
  virtual ~AsyncWebServerResponse() = default;
  void addHeader(const String& name, const String& value) { headers_.push_back({name,value}); }
  String _assembleHead(uint8_t) {
    String header = "HTTP/1.1 " + std::to_string(_code) + " OK\r\nContent-Length: " + std::to_string(_contentLength) + "\r\nContent-Type: " + _contentType + "\r\n";
    for (const auto& field : headers_) header += field.first + ": " + field.second + "\r\n";
    header += "\r\n"; _headLength = header.length(); return header;
  }
  virtual bool _sourceValid() const { return false; }
  virtual void _respond(AsyncWebServerRequest*) {}
  virtual size_t _ack(AsyncWebServerRequest*, size_t, uint32_t) { return 0; }
  bool _finished() const { return _state > RESPONSE_WAIT_ACK; }
  size_t bodyAccepted() const { return _sentLength; }
  size_t totalAccepted() const { return _writtenLength; }
  size_t totalAcknowledged() const { return _ackedLength; }
  WebResponseState state() const { return _state; }
};
