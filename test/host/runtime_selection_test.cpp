#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstdint>
#include "runtime/detail/RuntimeSelection.h"
#include "runtime/detail/RuntimeSnapshot.h"
#include "runtime/detail/RuntimeSubscriptions.h"

using cm::RuntimeSelection;
struct Provider {
  std::string name;
  std::function<void(JsonObject&)> fill;
  int order = 0;
};
struct Control {
  std::string group;
  std::string key;
  std::function<int()> getter;
};
// [MOCKED!] The pinned peer close queues a control message and stays connected.
// Only the network shell is synthetic; maintenance and subscription logic are real.
static uint32_t heartbeatNow = 0;
static uint32_t millis() {
  return heartbeatNow;
}
struct HeartbeatPeer {
  unsigned closeCalls = 0;
  bool connected = true;
  std::function<void()> onClose;
  void close() {
    ++closeCalls;
    if (onClose)
      onClose();
  }
};
struct HeartbeatSocket {
  std::map<uint32_t, HeartbeatPeer> peers;
  std::function<void(uint32_t)> beforeLookup;
  bool availableForWriteAll() const {
    return false;
  }
  void textAll(const char*) {}
  HeartbeatPeer* client(uint32_t id) {
    if (beforeLookup)
      beforeLookup(id);
    const auto found = peers.find(id);
    return found == peers.end() ? nullptr : &found->second;
  }
  void cleanupClients() {}
};
struct HeartbeatHarness {
  bool wsEnabled = true;
  HeartbeatSocket socket;
  HeartbeatSocket* ws = &socket;
  std::mutex wsClientsMutex;
  cm::RuntimeSubscriptions wsSubscriptions;
  unsigned long wsLastHeartbeat = 0;
  uint32_t wsHeartbeatInterval = 30000, wsHeartbeatTimeout = 120000;
  void wsRemove(uint32_t id) {
    std::lock_guard<std::mutex> lock(wsClientsMutex);
    wsSubscriptions.remove(id);
  }
#include "heartbeat-maintenance.inc"
};
static void heartbeatSingleClose() {
  HeartbeatHarness h;
  assert(h.wsSubscriptions.connect(1, "selected", 0));
  h.socket.peers[1] = HeartbeatPeer{};
  heartbeatNow = 120001;
  for (unsigned i = 0; i < 10000; ++i)
    h.wsHeartbeatMaintenance();
  assert(h.socket.peers[1].connected && h.socket.peers[1].closeCalls == 1);
  assert(h.wsSubscriptions.clients.empty());
  h.wsSubscriptions.seen(1, heartbeatNow);
  assert(!h.wsSubscriptions.subscribe(1, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"A\"]}", heartbeatNow));
  assert(h.wsSubscriptions.clients.empty());
}
static void heartbeatLatePong() {
  HeartbeatHarness h;
  assert(h.wsSubscriptions.connect(2, "selected", 1000));
  h.socket.peers[2] = HeartbeatPeer{};
  heartbeatNow = 121000; // Strict greater-than boundary is still healthy.
  h.wsHeartbeatMaintenance();
  assert(h.socket.peers[2].closeCalls == 0);
  bool updated = false;
  h.socket.beforeLookup = [&](uint32_t id) {
    if (updated)
      return;
    updated = true;
    ++heartbeatNow; // Pong after the detached snapshot and outer clock sample.
    std::lock_guard<std::mutex> lock(h.wsClientsMutex);
    h.wsSubscriptions.seen(id, heartbeatNow);
  };
  heartbeatNow = 121001;
  h.wsHeartbeatMaintenance();
  assert(updated && h.socket.peers[2].closeCalls == 0);
  assert(h.wsSubscriptions.clients.size() == 1);
  heartbeatNow = 241002;
  h.wsHeartbeatMaintenance();
  assert(h.socket.peers[2].closeCalls == 0);
  ++heartbeatNow;
  h.wsHeartbeatMaintenance();
  assert(h.socket.peers[2].closeCalls == 1 && h.wsSubscriptions.clients.empty());
}
static void heartbeatDisconnectReconnect() {
  HeartbeatHarness h;
  assert(h.wsSubscriptions.connect(3, "selected", 0));
  h.socket.peers[3] = HeartbeatPeer{};
  h.wsRemove(3);
  h.wsRemove(3);
  heartbeatNow = 500000;
  h.wsHeartbeatMaintenance();
  assert(h.socket.peers[3].closeCalls == 0);
  assert(h.wsSubscriptions.connect(3, "selected", heartbeatNow));
  heartbeatNow += 120001;
  bool reconnected = false;
  h.socket.beforeLookup = [&](uint32_t id) {
    if (reconnected)
      return;
    reconnected = true;
    std::lock_guard<std::mutex> lock(h.wsClientsMutex);
    h.wsSubscriptions.remove(id);
    assert(h.wsSubscriptions.connect(id, "selected", heartbeatNow));
  };
  h.wsHeartbeatMaintenance(); // Old snapshot cannot expire a newly connected entry.
  assert(reconnected && h.socket.peers[3].closeCalls == 0);
  h.socket.peers[3].onClose = [&]() {
    assert(h.wsSubscriptions.clients.empty()); // Claim precedes the external callback.
    h.wsRemove(3);                             // Reentrant disconnect must run without the CM mutex held.
  };
  heartbeatNow += 120001;
  h.wsHeartbeatMaintenance();
  h.wsRemove(3);
  assert(h.socket.peers[3].closeCalls == 1 && h.wsSubscriptions.clients.empty());
  assert(h.wsSubscriptions.connect(4, "none", heartbeatNow));
  h.socket.peers[4] = HeartbeatPeer{};
  h.wsHeartbeatMaintenance();
  assert(h.socket.peers[4].closeCalls == 0);
}
static void heartbeatRollover() {
  HeartbeatHarness h;
  const uint32_t start = 0xffffff00u;
  assert(h.wsSubscriptions.connect(5, "none", start));
  h.socket.peers[5] = HeartbeatPeer{};
  heartbeatNow = uint32_t(start + 120000u);
  h.wsHeartbeatMaintenance();
  assert(h.socket.peers[5].closeCalls == 0);
  ++heartbeatNow;
  h.wsHeartbeatMaintenance();
  assert(h.socket.peers[5].closeCalls == 1 && h.wsSubscriptions.clients.empty());
}
int main(int argc, char** argv) {
  if (argc > 1) {
    const std::string scenario = argv[1];
    if (scenario == "heartbeat-single")
      heartbeatSingleClose();
    else if (scenario == "heartbeat-pong")
      heartbeatLatePong();
    else
      return 2;
    std::cout << scenario << " passed\n";
    return 0;
  }

  RuntimeSelection selected;
  assert(!selected.all && !selected.contains("A"));
  assert(cm::parseRuntimeSelection("[\"A\",\"A\",\"controls\",\"alarms\"]", selected));
  assert(selected.groups.size() == 3 && selected.contains("A") && !selected.contains("a"));
  assert(!cm::parseRuntimeSelection("{}", selected));
  assert(selected.contains("A")); // Invalid replacement preserves last valid selection.
  assert(!cm::parseRuntimeSelection("[null]", selected));
  assert(!cm::parseRuntimeSelection("[\"\"]", selected));
  assert(!cm::parseRuntimeSelection("[\"a\\u0000b\"]", selected));
  assert(!cm::parseRuntimeSelection("[\"" + std::string(65, 'x') + "\"]", selected));
  assert(!cm::parseRuntimeSelection(std::string(2049, ' '), selected));
  JsonDocument tooMany;
  auto array = tooMany.to<JsonArray>();
  for (int i = 0; i < 33; ++i)
    array.add(std::to_string(i));
  assert(!cm::parseRuntimeSelection(array, selected));
  RuntimeSelection empty;
  assert(cm::parseRuntimeSelection("[]", empty) && !empty.all && empty.groups.empty());
  RuntimeSelection full = RuntimeSelection::everything();
  assert(full.contains("anything"));
  RuntimeSelection other;
  assert(cm::parseRuntimeSelection("[\"B\"]", other));
  auto combined = selected;
  combined.merge(other);
  assert(combined.contains("A") && combined.contains("B"));

  cm::RuntimeSubscriptions clients;
  assert(clients.connect(1, "none", 0));
  assert(!clients.connect(2, "invalid", 0));
  assert(clients.connect(2, "selected", 0));
  assert(clients.connect(3, "selected", 0));
  assert(!clients.unionSelection().all && clients.unionSelection().groups.empty());
  assert(clients.subscribe(2, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"A\"]}", 1));
  assert(clients.subscribe(3, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"B\",\"A\"]}", 1));
  assert(clients.unionSelection().groups.size() == 2);
  assert(!clients.subscribe(2, "{\"type\":\"runtimeSubscribe\",\"groups\":null}", 1));
  assert(clients.unionSelection().contains("A"));
  assert(!clients.subscribe(1, "{\"type\":\"runtimeSubscribe\",\"groups\":[\"A\"]}", 1));
  clients.remove(3);
  assert(!clients.unionSelection().contains("B"));
  assert(clients.connect(4, "", 0) && clients.unionSelection().all);
  for (unsigned i = 5; i < 12; ++i)
    assert(clients.connect(i, "selected", 0));
  assert(!clients.connect(12, "selected", 0));
  assert(clients.connect(13, "", 0));
  assert(clients.connect(14, "none", 0));

  int a = 0, b = 0, control = 0, orphan = 0;
  // [MOCKED!] Providers expose synthetic values; production snapshot helpers execute them.
  std::vector<Provider> providers{{"A", [&](JsonObject& o) {++a; o["v"] = 7; }}, {"B", [&](JsonObject& o) {++b; o["bulk"] = std::string(400, 'x'); }}};
  std::vector<Control> controls{{"A", "switch", [&]() {++control;return 1; }}, {"controls", "orphan", [&]() {++orphan;return 2; }}};
  auto run = [&](const RuntimeSelection& s) {
    JsonDocument d;
    auto root = d.to<JsonObject>();
    root["uptime"] = 123;
    auto ps = cm::selectRuntimeEntries(providers, s, [](const auto& p) { return p.name.c_str(); });
    auto cs = cm::selectRuntimeEntries(controls, s, [](const auto& c) { return c.group.c_str(); });
    cm::fillRuntimeProviders(root, ps);
    cm::fillRuntimeControls(root, cs);
    root["system"]["otaActive"] = true;
    root["system"]["otaHasPassword"] = true;
    return d;
  };
  auto small = run(selected);
  assert(a == 1 && b == 0 && control == 1 && orphan == 1);
  assert(small["controls"]["orphan"] == 2);
  auto all = run(full);
  assert(a == 2 && b == 1 && control == 2 && orphan == 2);
  auto none = run(empty);
  assert(a == 2 && b == 1 && control == 2 && orphan == 2);
  assert(none["system"]["otaActive"] == true);
  auto shared = run(combined);
  JsonDocument one, two;
  cm::filterRuntimeSnapshot(one.to<JsonObject>(), shared.as<JsonObjectConst>(), selected);
  cm::filterRuntimeSnapshot(two.to<JsonObject>(), shared.as<JsonObjectConst>(), other);
  assert(a == 3 && b == 2 && control == 3 && orphan == 3);
  assert(one["B"].isNull() && two["A"].isNull());
  assert(two["system"]["otaHasPassword"] == true);
  std::cout << "full_bytes=" << measureJson(all) << " selected_bytes=" << measureJson(small) << " empty_bytes=" << measureJson(none) << '\n';
  assert(measureJson(small) < measureJson(all));
  heartbeatSingleClose();
  heartbeatLatePong();
  heartbeatDisconnectReconnect();
  heartbeatRollover();
  std::cout << "Runtime selection/provider and4 heartbeat lifecycle scenarios passed\n";
}
