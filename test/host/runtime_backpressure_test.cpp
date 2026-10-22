// [MOCKED!] ACK-stalled public WebSocket shell; snapshot and push production bodies are real.
#include <ArduinoJson.h>
#include <cassert>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "runtime/detail/RuntimeSubscriptions.h"
#include "runtime/detail/RuntimeSnapshot.h"
#include "runtime/detail/RuntimeMessage.h"
namespace {
struct Socket {
  std::map<uint32_t, AsyncWebSocketClient> clients;
  AsyncWebSocketClient* client(uint32_t id) {
    auto i = clients.find(id);
    return i == clients.end() ? nullptr : &i->second;
  }
};
static unsigned long millis() {
  return 100;
}
struct TrackedDocument : JsonDocument {
  inline static std::atomic<unsigned> alive{0}, constructions{0};
  inline static thread_local unsigned ownAlive = 0;
  TrackedDocument() {
    ++alive;
    ++ownAlive;
    ++constructions;
  }
  ~TrackedDocument() {
    --alive;
    --ownAlive;
  }
};
struct ConfigManagerRuntime {
  std::recursive_mutex runtimeGenerationMutex;
#include "runtime-generation.inc"
  unsigned builds = 0;
  cm::RuntimeSelection lastSelection;
  std::function<void()> duringBuild;
  void fillRuntimeSnapshot(JsonObject root, const cm::RuntimeSelection& selection) {
    ++builds;
    lastSelection = selection;
    if (duringBuild)
      duringBuild();
    root["uptime"] = 100;
    if (selection.contains("A")) {
      root["A"]["value"] = builds;
      root["A"]["nested"]["null"] = nullptr;
      root["A"]["system"]["unexpectedNestedGroup"] = true;
    }
    if (selection.contains("B"))
      root["B"]["value"] = builds;
    if (selection.contains("system"))
      root["system"]["heap"] = 12345;
    root["system"]["otaActive"] = true;
    root["system"]["otaHasPassword"] = false;
  }
  String runtimeValuesToJSON(const cm::RuntimeSelection& selection);
};
#define JsonDocument TrackedDocument
#include "runtime-serialization.inc"
#undef JsonDocument
using Runtime = ConfigManagerRuntime;
struct Harness {
  bool wsEnabled = true;
  Socket socket;
  Socket* ws = &socket;
  std::mutex wsClientsMutex;
  cm::RuntimeSubscriptions wsSubscriptions;
  unsigned long wsLastPush = 0;
  Runtime runtimeManager;
  std::function<String()> customPayloadBuilder;
  bool sendWebSocketText(AsyncWebSocketClient* client, const String& payload) {
    if (!client || client->queueIsFull())
      return false;
    client->text(payload);
    return true;
  }
#define JsonDocument TrackedDocument
#include "runtime-push.inc"
#undef JsonDocument
};
static void stalledRuntime() {
  Harness h;
  assert(h.wsSubscriptions.connect(1, "", 0));
  h.socket.clients[1];
  h.pushRuntimeSnapshot();
  for (unsigned i = 0; i < 10000; ++i)
    h.pushRuntimeSnapshot();
  assert(h.runtimeManager.builds == 1 && h.socket.clients[1].queue.size() == 1);
  h.socket.clients[1].queue.clear(); // Actual peer finally ACKs / transport releases message.
  assert(h.pushRuntimeSnapshot());
  assert(h.runtimeManager.builds == 2 && h.socket.clients[1].queue.size() == 1);
  std::cout << "Runtime backpressure: 10000 stalled ticks keep one message; fresh snapshot after release PASS\n";
}

static void customAndSelected() {
  Harness h;
  assert(h.wsSubscriptions.connect(1, "selected", 0));
  assert(h.wsSubscriptions.subscribe(1, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"A\"]}", 0));
  assert(h.wsSubscriptions.connect(2, "none", 0));
  h.socket.clients[1];
  h.socket.clients[2];
  unsigned builds = 0;
  h.customPayloadBuilder = [&] { ++builds; return String("opaque custom"); };
  for (unsigned i = 0; i < 1000; ++i)
    h.pushRuntimeSnapshot();
  assert(builds == 1 && h.runtimeManager.builds == 0);
  assert(h.socket.clients[1].queue.size() == 1 && h.socket.clients[2].queue.empty());
  h.socket.clients[1].queue.clear();
  h.customPayloadBuilder = {};
  assert(h.pushRuntimeSnapshot());
  assert(h.runtimeManager.lastSelection.contains("A"));
  assert(h.socket.clients[1].queue.front()->send(nullptr) > 0);
  h.socket.clients[1].queue.front()->ack(100000, 0);
  assert(h.socket.clients[1].queue.front()->finished());
  // Finished but not yet destroyed: upstream still owns the copied payload.
  assert(!h.pushRuntimeSnapshot() && h.runtimeManager.builds == 1);
  h.socket.clients[1].queue.clear();
  assert(h.pushRuntimeSnapshot() && h.runtimeManager.builds == 2);
}
static void lifetimeAndReject() {
  auto state = std::make_shared<cm::RuntimeDeliveryState>();
  std::weak_ptr<cm::RuntimeDeliveryState> weak = state;
  AsyncWebSocketClient client;
  {
    cm::RuntimeDeliveryReservation token(state);
    assert(token);
    assert(cm::sendRuntimeMessage(&client, String("payload"), std::move(token)));
  }
  assert(state->busy.load());
  AsyncWebSocketBasicMessage::onPayloadFree = [&] { assert(state->busy.load()); };
  client.queue.clear(); // Token is released AFTER the copied payload is freed.
  AsyncWebSocketBasicMessage::onPayloadFree = {};
  assert(!state->busy.load());
  {
    cm::RuntimeDeliveryReservation token(state);
    client.rejectOnMessage = true;
    cm::sendRuntimeMessage(&client, String("payload"), std::move(token));
    assert(!state->busy.load());
  }
  client.rejectOnMessage = false;
  {
    cm::RuntimeDeliveryReservation token(state);
    AsyncWebSocketBasicMessage::failPayload = true;
    assert(!cm::sendRuntimeMessage(&client, String("payload"), std::move(token)));
    AsyncWebSocketBasicMessage::failPayload = false;
    assert(!state->busy.load());
  }
  {
    cm::RuntimeDeliveryReservation token(state);
    assert(!cm::sendRuntimeMessage(nullptr, String("payload"), std::move(token)));
  }
  assert(!state->busy.load());
  for (unsigned i = 0; i < 32; ++i)
    client.queue.emplace_back(new AsyncWebSocketMessage);
  {
    cm::RuntimeDeliveryReservation token(state);
    assert(!cm::sendRuntimeMessage(&client, String("payload"), std::move(token)));
    assert(client.queue.size() == 32);
  }
  assert(!state->busy.load());
  client.queue.clear();
  {
    cm::RuntimeDeliveryReservation token(state);
    assert(!cm::sendRuntimeMessage(&client, String(), std::move(token)));
  }
  assert(!state->busy.load());
  {
    cm::RuntimeDeliveryReservation token(state);
    assert(cm::sendRuntimeMessage(&client, String("payload"), std::move(token)));
  }
  state.reset(); // Subscription state disappears before transport destruction.
  assert(!weak.expired());
  client.queue.clear();
  assert(weak.expired());
}
static void concurrentAndDisconnect() {
  Harness h;
  assert(h.wsSubscriptions.connect(1, "", 0));
  h.socket.clients[1];
  unsigned reentrant = 0;
  h.runtimeManager.duringBuild = [&] { ++reentrant; assert(!h.pushRuntimeSnapshot()); };
  assert(h.pushRuntimeSnapshot());
  assert(reentrant == 1 && h.runtimeManager.builds == 1);
  auto detached = h.wsSubscriptions;
  auto old = detached.clients.front().delivery;
  h.wsSubscriptions.remove(1);
  assert(old->busy.load());
  assert(h.wsSubscriptions.connect(1, "", 0));
  auto fresh = h.wsSubscriptions.clients.front().delivery;
  assert(fresh != old && !fresh->busy.load());
  h.socket.clients[1].queue.clear();
  assert(!old->busy.load() && !fresh->busy.load());
  h.runtimeManager.duringBuild = {};
  assert(h.pushRuntimeSnapshot());
  assert(fresh->busy.load() && !old->busy.load());
}
static void simultaneousClaim() {
  auto state = std::make_shared<cm::RuntimeDeliveryState>();
  std::atomic<unsigned> ready{0}, attempted{0}, winners{0};
  std::atomic<bool> go{false};
  auto contender = [&] {
    ready.fetch_add(1);
    while (!go.load())
      std::this_thread::yield();
    cm::RuntimeDeliveryReservation token(state);
    if (token)
      winners.fetch_add(1);
    attempted.fetch_add(1);
    while (attempted.load() < 2)
      std::this_thread::yield();
  };
  std::thread first(contender), second(contender);
  while (ready.load() < 2)
    std::this_thread::yield();
  go.store(true);
  first.join();
  second.join();
  assert(winners.load() == 1 && !state->busy.load());
}
static void slowAndHealthy() {
  Harness h;
  for (uint32_t id : {1u, 2u}) {
    assert(h.wsSubscriptions.connect(id, "selected", 0));
    const char* subscription = id == 1 ? "{\"type\":\"runtimeSubscribe\",\"groups\":[\"A\"]}" : "{\"type\":\"runtimeSubscribe\",\"groups\":[\"B\"]}";
    assert(h.wsSubscriptions.subscribe(id, subscription, 0));
    h.socket.clients[id];
  }
  assert(h.pushRuntimeSnapshot());
  h.socket.clients[2].queue.clear();
  assert(h.pushRuntimeSnapshot());
  assert(!h.runtimeManager.lastSelection.contains("A") && h.runtimeManager.lastSelection.contains("B"));
  assert(h.socket.clients[1].queue.size() == 1 && h.socket.clients[2].queue.size() == 1);
  // Missing connection after a claim must not strand its local reservation.
  h.socket.clients[2].queue.clear();
  auto healthy = h.wsSubscriptions.clients[1].delivery;
  h.runtimeManager.duringBuild = [&] { h.socket.clients.erase(2); };
  assert(!h.pushRuntimeSnapshot() && !healthy->busy.load());
}
static void singleSelectionEquivalent() {
  for (const char* groups : {"[]", "[\"A\"]", "[\"system\"]", "[\"A\",\"B\"]"}) {
    Harness h;
    assert(h.wsSubscriptions.connect(1, "selected", 0));
    const std::string subscription = std::string("{\"type\":\"runtimeSubscribe\",\"groups\":") + groups + "}";
    assert(h.wsSubscriptions.subscribe(1, subscription.c_str(), 0));
    h.socket.clients[1];
    const unsigned before = TrackedDocument::constructions.load();
    assert(h.pushRuntimeSnapshot());
    assert(TrackedDocument::constructions.load() == before + 1); // No projected document.
    AsyncClient wire;
    h.socket.clients[1].queue.front()->send(&wire);
    JsonDocument actual, source, expected;
    assert(!deserializeJson(actual, wire.received));
    Runtime baseline;
    const auto selection = h.wsSubscriptions.clients[0].selection;
    baseline.fillRuntimeSnapshot(source.to<JsonObject>(), selection);
    cm::filterRuntimeSnapshot(expected.to<JsonObject>(), source.as<JsonObjectConst>(), selection);
    JsonArray announced = expected["_runtimeGroups"].to<JsonArray>();
    for (const auto& group : selection.groups)
      announced.add(group);
    assert(actual.as<JsonVariantConst>() == expected.as<JsonVariantConst>());
    assert(actual["uptime"] == 100 && actual["system"]["otaActive"] == true);
    assert(actual["system"]["otaHasPassword"] == false);
    if (!selection.contains("system"))
      assert(actual["system"]["heap"].isNull());
  }
}
static void generationConcurrency() {
  Harness h;
  assert(h.wsSubscriptions.connect(1, "", 0));
  h.socket.clients[1];
  std::atomic<bool> inBuild{false}, release{false}, beforeCall{false}, completed{false};
  h.runtimeManager.duringBuild = [&] {
    if (!inBuild.exchange(true))
      while (!release.load())
        std::this_thread::yield();
  };
  std::thread http([&] { const auto payload = h.runtimeManager.runtimeValuesToJSON(cm::RuntimeSelection::everything()); assert(payload.length()); });
  while (!inBuild.load())
    std::this_thread::yield();
  assert(!h.pushRuntimeSnapshot()); // Main-loop WS never blocks behind HTTP generation.
  assert(h.runtimeManager.builds == 1 && !h.wsSubscriptions.clients[0].delivery->busy.load());
  release.store(true);
  http.join();
  h.runtimeManager.duringBuild = {};
  assert(h.pushRuntimeSnapshot());
  h.socket.clients[1].queue.clear();
  inBuild.store(false);
  release.store(false);
  h.runtimeManager.duringBuild = [&] {
    if (!inBuild.exchange(true))
      while (!release.load())
        std::this_thread::yield();
  };
  std::thread ws([&] { assert(h.pushRuntimeSnapshot()); });
  while (!inBuild.load())
    std::this_thread::yield();
  const unsigned before = h.runtimeManager.builds;
  std::thread waitingHttp([&] {
    beforeCall.store(true);
    const auto payload = h.runtimeManager.runtimeValuesToJSON(cm::RuntimeSelection::everything());
    assert(payload.length());
    completed.store(true);
  });
  while (!beforeCall.load())
    std::this_thread::yield();
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  assert(!completed.load() && h.runtimeManager.builds == before);
  release.store(true);
  ws.join();
  waitingHttp.join();
  assert(completed.load() && h.runtimeManager.builds == before + 1);
  assert(TrackedDocument::alive.load() == 0);
  h.socket.clients[1].queue.clear();
  h.runtimeManager.duringBuild = {};
  h.customPayloadBuilder = [&] { return h.runtimeManager.runtimeValuesToJSON(cm::RuntimeSelection::everything()); };
  assert(h.pushRuntimeSnapshot()); // Same-thread recursive custom JSON remains supported.
}
static void generationReleasedAtTransport() {
  Harness h;
  assert(h.wsSubscriptions.connect(1, "", 0));
  h.socket.clients[1];
  AsyncWebSocketClient::onMessage = [&] {
    assert(TrackedDocument::ownAlive == 0);
    bool acquired = false;
    std::thread check([&] { auto generation=h.runtimeManager.acquireRuntimeGeneration(false); acquired=generation.owns_lock(); });
    check.join();
    assert(acquired);
  };
  assert(h.pushRuntimeSnapshot());
  AsyncWebSocketClient::onMessage = [] { assert(TrackedDocument::ownAlive == 0); };
}
} // namespace

int main() {
  AsyncWebSocketClient::onMessage = [] { assert(TrackedDocument::ownAlive == 0); };
  stalledRuntime();
  customAndSelected();
  lifetimeAndReject();
  concurrentAndDisconnect();
  simultaneousClaim();
  slowAndHealthy();
  singleSelectionEquivalent();
  generationConcurrency();
  generationReleasedAtTransport();
  std::cout << "Generation: document lifetime/SDK unlock, HTTP wait/WS skip, recursive custom, direct selection equivalence PASS\n";
  std::cout << "Runtime delivery: selected/custom/events, ACK/free, reject/failure, shared lifetime/reentrant+atomic claim, slow/healthy isolation PASS\n";
}
