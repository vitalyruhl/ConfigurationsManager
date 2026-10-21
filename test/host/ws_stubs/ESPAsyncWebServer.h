#pragma once
// [MOCKED!] Public pinned WebSocket message ownership / ACK shell.
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
using String=std::string;
struct AsyncClient { std::string received; };
struct AsyncWebSocketMessage {
  virtual ~AsyncWebSocketMessage()=default;
  virtual void ack(size_t,uint32_t) {}
  virtual size_t send(AsyncClient*) { return 0; }
  virtual bool finished() { return true; }
  virtual bool betweenFrames() const { return false; }
};
struct AsyncWebSocketBasicMessage: AsyncWebSocketMessage {
  inline static bool failPayload=false;
  inline static std::function<void()> onPayloadFree;
  std::string payload;
  bool done=false;
  AsyncWebSocketBasicMessage(const char* data,size_t len):payload(failPayload?std::string():std::string(data,len)),done(failPayload) {}
  ~AsyncWebSocketBasicMessage() override { std::string().swap(payload); if(onPayloadFree) onPayloadFree(); }
  void ack(size_t len,uint32_t) override { if(len>=payload.size()) done=true; }
  size_t send(AsyncClient* client) override { if(client) client->received=payload; return payload.size(); }
  bool finished() override { return done; }
  bool betweenFrames() const override { return true; }
};
struct AsyncWebSocketClient {
  std::vector<std::unique_ptr<AsyncWebSocketMessage>> queue;
  bool rejectOnMessage=false;
  inline static std::function<void()> onMessage;
  bool queueIsFull() const { return queue.size()>=32; }
  void message(AsyncWebSocketMessage* msg) {
    if(onMessage) onMessage();
    if(rejectOnMessage || queueIsFull()) delete msg;
    else queue.emplace_back(msg);
  }
  void text(const String& payload) { message(new AsyncWebSocketBasicMessage(payload.c_str(),payload.size())); }
};
