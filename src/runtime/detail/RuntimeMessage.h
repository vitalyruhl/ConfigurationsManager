#pragma once
#include "RuntimeDelivery.h"
#include <ESPAsyncWebServer.h>
#include <new>

namespace cm {
// Only CM runtime snapshots use this wrapper. Transport owns it after message().
class RuntimeMessage final : public AsyncWebSocketMessage {
  RuntimeDeliveryReservation reservation_;
  std::unique_ptr<AsyncWebSocketBasicMessage> message_;
public:
  RuntimeMessage(const String& payload, RuntimeDeliveryReservation&& reservation)
      : reservation_(std::move(reservation)),
        message_(new (std::nothrow) AsyncWebSocketBasicMessage(payload.c_str(), payload.length())) {}
  ~RuntimeMessage() override {
    // Free copied payload before another tick may acquire this client's slot.
    message_.reset();
  }
  bool valid() const { return message_ && !message_->finished(); }
  void ack(size_t len, uint32_t time) override { if (message_) message_->ack(len, time); }
  size_t send(AsyncClient* client) override { return message_ ? message_->send(client) : 0; }
  bool finished() override { return !message_ || message_->finished(); }
  bool betweenFrames() const override { return message_ && message_->betweenFrames(); }
};
inline bool sendRuntimeMessage(AsyncWebSocketClient* client, const String& payload,
                               RuntimeDeliveryReservation&& reservation) {
  if (!client || client->queueIsFull() || !payload.length()) return false;
  std::unique_ptr<RuntimeMessage> message(new (std::nothrow) RuntimeMessage(payload, std::move(reservation)));
  if (!message || !message->valid()) return false;
  client->message(message.release()); // Includes synchronous destruction on queue reject.
  return true;
}
} // namespace cm
