#pragma once
#include <ESPAsyncWebServer.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>

namespace cm {
// One fixed-length sender for immutable static, moved, or shared response data.
// Only transport admission is bounded; provider generation remains the caller's responsibility.
class BoundedResponse : public AsyncWebServerResponse {
public:
  BoundedResponse(const uint8_t* content, size_t length, const String& contentType)
      : content_(content) { initialize(length, contentType); }

  BoundedResponse(String&& content, const String& contentType)
      : owned_(std::move(content)) {
    content_ = reinterpret_cast<const uint8_t*>(owned_.c_str());
    initialize(owned_.length(), contentType);
  }

  BoundedResponse(std::shared_ptr<const String> content, const String& contentType)
      : shared_(std::move(content)) {
    content_ = shared_ ? reinterpret_cast<const uint8_t*>(shared_->c_str()) : nullptr;
    initialize(shared_ ? shared_->length() : 0, contentType);
  }

  BoundedResponse(const BoundedResponse&) = delete;
  BoundedResponse& operator=(const BoundedResponse&) = delete;

  bool _sourceValid() const override { return content_ || _contentLength == 0; }

  void _respond(AsyncWebServerRequest* request) override {
    if (!request || !request->client() || !_sourceValid()) {
      _state = RESPONSE_FAILED;
      if (request && request->client()) request->client()->close();
      return;
    }
    header_ = _assembleHead(request->version());
    if (!header_.length()) {
      _state = RESPONSE_FAILED;
      request->client()->close();
      return;
    }
    _state = RESPONSE_HEADERS;
    admit(request);
  }

  size_t _ack(AsyncWebServerRequest* request, size_t length, uint32_t time) override {
    (void)time;
    if (_state == RESPONSE_END || _state == RESPONSE_FAILED) return 0;
    if (!request || !request->client()) { _state = RESPONSE_FAILED; return 0; }
    _ackedLength += std::min(length, _writtenLength - _ackedLength);
    if (_state == RESPONSE_WAIT_ACK && _ackedLength == _writtenLength) {
      _state = RESPONSE_END;
      // close() can synchronously destroy this response through disconnect.
      // Do not access any members or the request after this call.
      request->client()->close();
      return 0;
    }
    return admit(request);
  }

private:
  static constexpr size_t maxInFlight = 512;
  const uint8_t* content_ = nullptr;
  String owned_;
  std::shared_ptr<const String> shared_;
  String header_;
  size_t headerOffset_ = 0;
  bool flushPending_ = false;

  void initialize(size_t length, const String& contentType) {
    _code = 200;
    _contentType = contentType;
    _contentLength = length;
    addHeader("Connection", "close");
  }

  size_t admit(AsyncWebServerRequest* request) {
    auto* client = request->client();
    if (flushPending_) {
      if (!client->send()) return 0;
      flushPending_ = false;
    }
    if (_state != RESPONSE_HEADERS && _state != RESPONSE_CONTENT) return 0;
    const size_t outstanding = _writtenLength - _ackedLength;
    if (outstanding >= maxInFlight) return 0;
    const size_t allowance = std::min(client->space(), maxInFlight - outstanding);
    if (!allowance) return 0;
    const bool sendingHeader = _state == RESPONSE_HEADERS;
    const size_t remaining = sendingHeader ? header_.length() - headerOffset_ : _contentLength - _sentLength;
    const char* data = sendingHeader ? header_.c_str() + headerOffset_
                                    : reinterpret_cast<const char*>(content_ + _sentLength);
    // AsyncClient::write can return zero after add succeeded but send failed.
    // Count accepted bytes through add; a failed flush must never re-enqueue them.
    const size_t accepted = client->add(data, std::min(remaining, allowance), ASYNC_WRITE_FLAG_COPY);
    if (!accepted) return 0;
    _writtenLength += accepted;
    if (sendingHeader) {
      headerOffset_ += accepted;
      if (headerOffset_ == header_.length()) {
        header_ = String(); // COPY admission owns the queued header bytes now.
        _state = _contentLength ? RESPONSE_CONTENT : RESPONSE_WAIT_ACK;
      }
    } else {
      _sentLength += accepted;
      if (_sentLength == _contentLength) _state = RESPONSE_WAIT_ACK;
    }
    flushPending_ = !client->send();
    return accepted;
  }
};
// The route increments before obtaining metadata. Destruction releases exactly
// that reservation, both after final ACK and after an early peer disconnect.
class RuntimeMetaResponse final : public BoundedResponse {
public:
  RuntimeMetaResponse(std::shared_ptr<const String> payload, std::atomic<uint32_t>* activeRequests)
      : BoundedResponse(std::move(payload), "application/json"), activeRequests_(activeRequests) {}
  ~RuntimeMetaResponse() override { if (activeRequests_) --(*activeRequests_); }
  bool hasPayload() const { return _contentLength != 0 && BoundedResponse::_sourceValid(); }
  bool _sourceValid() const override { return hasPayload(); }
private:
  std::atomic<uint32_t>* activeRequests_;
};
} // namespace cm
