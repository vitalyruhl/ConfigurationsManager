// [MOCKED!] Deterministic connection closure; no invalid memory is accessed.
#include "runtime/detail/RuntimeMessage.h"
#include <iostream>

int main() {
  AsyncWebSocket socket;
  auto& client = socket.clients[7];
  auto state = std::make_shared<cm::RuntimeDeliveryState>();
  client.disconnectOnQueueCheck = true;
  cm::RuntimeDeliveryReservation reservation(state);
  const bool sent = cm::sendRuntimeMessage(socket, 7, String("runtime payload"), std::move(reservation));
  if (sent || client.closedQueueCalls != 0 || state->busy.load()) {
    std::cerr << "Connection closed before handoff: sent=" << sent
              << " closed_queue_calls=" << client.closedQueueCalls
              << " busy=" << state->busy.load() << '\n';
    return 1;
  }
  std::cout << "Connection closure before handoff leaves no queued frame or reservation PASS\n";
}
