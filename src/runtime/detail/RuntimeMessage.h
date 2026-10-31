#pragma once
#include "RuntimeDelivery.h"
#include <ESPAsyncWebServer.h>
#include <new>
#include <vector>

namespace cm {
// Member destruction frees copied bytes before releasing the delivery slot.
struct RuntimeOutgoingBuffer {
  RuntimeDeliveryReservation reservation;
  std::vector<uint8_t> bytes;

  RuntimeOutgoingBuffer(const String& payload, RuntimeDeliveryReservation&& claim)
      : reservation(std::move(claim)), bytes(payload.c_str(), payload.c_str() + payload.length()) {}
};

inline bool sendRuntimeMessage(AsyncWebSocket& socket, uint32_t clientId, const String& payload,
                               RuntimeDeliveryReservation reservation) {
  if (!reservation || !payload.length() || !socket.availableForWrite(clientId))
    return false;
  auto* allocation = new (std::nothrow) RuntimeOutgoingBuffer(payload, std::move(reservation));
  if (!allocation)
    return false;
  std::shared_ptr<RuntimeOutgoingBuffer> owner(allocation);
  // An aliasing shared_ptr lets the transport own bytes and their reservation together.
  AsyncWebSocketSharedBuffer buffer(owner, &owner->bytes);
  // The maintained server holds its client-list lock across lookup and enqueue.
  // A disconnect after availableForWrite() simply makes this return false.
  return socket.text(clientId, std::move(buffer));
}
} // namespace cm
