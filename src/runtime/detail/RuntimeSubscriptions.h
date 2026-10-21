#pragma once
#include "RuntimeSelection.h"
#include "RuntimeDelivery.h"
#include <cstdint>

namespace cm {
enum class RuntimeMode { Legacy, Selected, Events };
struct RuntimeClient {
  uint32_t id;
  unsigned long lastSeen;
  RuntimeMode mode;
  RuntimeSelection selection;
  std::shared_ptr<RuntimeDeliveryState> delivery = std::make_shared<RuntimeDeliveryState>();
};
// Callers synchronize access; callbacks and network sends use a detached snapshot.
class RuntimeSubscriptions {
public:
  std::vector<RuntimeClient> clients;
  bool connect(uint32_t id, const char* mode, unsigned long now) {
    RuntimeMode kind;
    if (!mode || !*mode) kind = RuntimeMode::Legacy;
    else if (std::strcmp(mode,"selected") == 0) kind = RuntimeMode::Selected;
    else if (std::strcmp(mode,"none") == 0) kind = RuntimeMode::Events;
    else return false;
    if (kind == RuntimeMode::Selected && std::count_if(clients.begin(), clients.end(), [](const auto& client) { return client.mode == RuntimeMode::Selected; }) >= static_cast<int>(RuntimeSelection::maxClients)) return false;
    clients.push_back({id,now,kind,kind == RuntimeMode::Legacy ? RuntimeSelection::everything() : RuntimeSelection{}});
    return true;
  }
  void seen(uint32_t id, unsigned long now) {
    for (auto& client : clients) if (client.id == id) { client.lastSeen = now; return; }
  }
  // Caller holds its subscription mutex; claim once before external close/callbacks.
  bool removeIfExpired(uint32_t id, uint32_t now, uint32_t timeout) {
    const auto client = std::find_if(clients.begin(),clients.end(),[id](const auto& c){return c.id==id;});
    if (client == clients.end() || uint32_t(now - uint32_t(client->lastSeen)) <= timeout) return false;
    remove(id);
    return true;
  }
  void remove(uint32_t id) {
    clients.erase(std::remove_if(clients.begin(),clients.end(),[id](const auto& client){return client.id==id;}),clients.end());
  }
  bool subscribe(uint32_t id, const char* data, size_t length, unsigned long now) {
    if (!data || length > RuntimeSelection::maxPayloadBytes + 64) return false;
    auto client = std::find_if(clients.begin(),clients.end(),[id](const auto& c){return c.id==id && c.mode==RuntimeMode::Selected;});
    if (client == clients.end()) return false;
    JsonDocument document;
    if (deserializeJson(document,data,length,DeserializationOption::NestingLimit(3))) return false;
    if (document["type"] != "runtimeSubscribe") return false;
    if (!parseRuntimeSelection(document["groups"].as<JsonVariantConst>(),client->selection)) return false;
    client->lastSeen=now;
    return true;
  }
  bool subscribe(uint32_t id, const char* data, unsigned long now) { return subscribe(id,data,std::strlen(data),now); }
  RuntimeSelection unionSelection() const {
    RuntimeSelection selection;
    for (const auto& client : clients) if (client.mode != RuntimeMode::Events) selection.merge(client.selection);
    return selection;
  }
};
} // namespace cm
