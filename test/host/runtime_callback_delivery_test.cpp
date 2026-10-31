// [MOCKED!] Exercise the actual public callback sender with periodic pushes paused.
#include <ESPAsyncWebServer.h>
#include <cassert>
#include <iostream>

struct Harness {
  bool wsEnabled = false;
  AsyncWebSocket* ws = nullptr;
#include "callback-delivery.inc"
};

int main() {
  AsyncWebSocket callbackServer, unrelatedServer;
  callbackServer.clients[3];
  auto* client = callbackServer.client(3);
  Harness h;
  h.ws = &unrelatedServer;
  assert(h.sendWebSocketText(client, String("callback response")));
  assert(callbackServer.clients[3].queue.size() == 1);
  assert(unrelatedServer.clients.empty());
  callbackServer.clients[3].connected = false;
  assert(!h.sendWebSocketText(client, String("closed callback")));
  assert(callbackServer.clients[3].closedQueueCalls == 0);
  assert(!h.sendWebSocketText(nullptr, String("missing callback")));
  std::cout << "Callback replies use their owning server with periodic pushes paused PASS\n";
}
