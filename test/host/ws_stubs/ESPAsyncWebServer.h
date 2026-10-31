#pragma once
// [MOCKED!] Public WebSocket queue ownership shell; TCP acknowledgements are separate.
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>
using String = std::string;
using AsyncWebSocketSharedBuffer = std::shared_ptr<std::vector<uint8_t>>;
struct AsyncClient {
  std::string received;
};
struct AsyncWebSocketMessage {
  virtual ~AsyncWebSocketMessage() = default;
  virtual void ack(size_t, uint32_t) {}
  virtual size_t send(AsyncClient*) {
    return 0;
  }
  virtual bool finished() {
    return true;
  }
  virtual bool betweenFrames() const {
    return false;
  }
};
struct AsyncWebSocketBasicMessage : AsyncWebSocketMessage {
  inline static bool failPayload = false;
  inline static std::function<void()> onPayloadFree;
  std::string payload;
  bool done = false;
  AsyncWebSocketBasicMessage(const char* data, size_t len)
      : payload(failPayload ? std::string() : std::string(data, len)), done(failPayload) {}
  ~AsyncWebSocketBasicMessage() override {
    std::string().swap(payload);
    if (onPayloadFree)
      onPayloadFree();
  }
  void ack(size_t len, uint32_t) override {
    if (len >= payload.size())
      done = true;
  }
  size_t send(AsyncClient* client) override {
    if (client)
      client->received = payload;
    return payload.size();
  }
  bool finished() override {
    return done;
  }
  bool betweenFrames() const override {
    return true;
  }
};
struct AsyncWebSocket;
struct AsyncWebSocketClient {
  AsyncWebSocket* owner = nullptr;
  AsyncWebSocket* server() { return owner; }
  uint32_t clientId = 0;
  uint32_t id() const { return clientId; }
  std::vector<std::unique_ptr<AsyncWebSocketMessage>> queue;
  bool rejectOnMessage = false;
  mutable bool connected = true;
  mutable bool disconnectOnQueueCheck = false;
  unsigned closedQueueCalls = 0;
  inline static std::function<void()> onMessage;
  inline static std::function<void()> beforeBufferRelease;
  bool queueIsFull() const {
    if (disconnectOnQueueCheck) {
      disconnectOnQueueCheck = false;
      connected = false;
    }
    return queue.size() >= 32;
  }
  void message(AsyncWebSocketMessage* msg) {
    if (!connected) {
      ++closedQueueCalls;
      delete msg;
      return;
    }
    if (onMessage)
      onMessage();
    if (rejectOnMessage || queueIsFull())
      delete msg;
    else
      queue.emplace_back(msg);
  }
  void text(const String& payload) {
    message(new AsyncWebSocketBasicMessage(payload.c_str(), payload.size()));
  }
  struct SharedMessage : AsyncWebSocketMessage {
    AsyncWebSocketSharedBuffer buffer;
    bool done = false;
    explicit SharedMessage(AsyncWebSocketSharedBuffer value) : buffer(std::move(value)) {}
    ~SharedMessage() override {
      if (beforeBufferRelease)
        beforeBufferRelease();
      buffer.reset();
    }
    void ack(size_t, uint32_t) override {}
    size_t send(AsyncClient* client) override {
      if (client) client->received.assign(buffer->begin(), buffer->end());
      done = true; // Maintained transport releases its buffer after copying to TCP.
      return buffer->size();
    }
    bool finished() override { return done; }
    bool betweenFrames() const override { return true; }
  };
  bool text(AsyncWebSocketSharedBuffer buffer) {
    if (!connected) { ++closedQueueCalls; return false; }
    if (onMessage) onMessage();
    if (rejectOnMessage || queueIsFull()) return false;
    queue.emplace_back(new SharedMessage(std::move(buffer)));
    return true;
  }
};

// [MOCKED!] Guarded server-by-ID interface; closed connections are never borrowed.
struct AsyncWebSocket {
  std::map<uint32_t, AsyncWebSocketClient> clients;
  AsyncWebSocketClient* client(uint32_t id) {
    auto i = clients.find(id);
    if (i != clients.end()) { i->second.owner = this; i->second.clientId = id; }
    return i == clients.end() || !i->second.connected ? nullptr : &i->second;
  }
  bool hasClient(uint32_t id) { return client(id) != nullptr; }
  bool availableForWrite(uint32_t id) {
    auto* value = client(id);
    return value && !value->queueIsFull();
  }
  bool text(uint32_t id, AsyncWebSocketSharedBuffer buffer) {
    auto* value = client(id);
    return value && value->text(std::move(buffer));
  }
  bool text(uint32_t id, const String& payload) {
    return text(id, std::make_shared<std::vector<uint8_t>>(payload.begin(), payload.end()));
  }
};
