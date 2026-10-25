// [MOCKED!] Arduino clock/client and lwIP boundary. Actual PubSubClient 2.8 and CM adapter.
#include "mqtt/detail/BoundedMqttClient.h"
#include <PubSubClient.h>
#include <cassert>
#include <cstdio>
#include <string>

int main() {
  cm::detail::BoundedMqttClient transport;
  PubSubClient mqtt(transport);
  mqtt.setServer(IPAddress(), 1883);
  mqtt.setSocketTimeout(1);
  unsigned callbacks = 0;
  std::string payload;
  mqtt.setCallback([&](char* topic, uint8_t* bytes, unsigned length) {
    ++callbacks;
    assert(std::string(topic) == "t");
    payload.assign(reinterpret_cast<const char*>(bytes), length);
  });
  mockIncoming = {0x20, 2, 0, 0};
  assert(mqtt.connect("mock-device"));
  mockIncoming = {0x30, 4, 0, 1, 't'};
  transport.beginLoop();
  const auto before = mockNowMs;
  assert(mqtt.loop());
  assert(mockNowMs == before && callbacks == 0); // no per-byte PubSubClient read wait
  mockIncoming = {'x'};
  transport.beginLoop();
  assert(mqtt.loop());
  assert(callbacks == 1 && payload == "x");

  // Adjacent TCP packets remain separate MQTT frames and consume one per MQTT loop.
  mockIncoming = {0x30, 4, 0, 1, 't', 'y', 0x30, 4, 0, 1, 't', 'z'};
  transport.beginLoop();
  assert(mqtt.loop());
  assert(callbacks == 2 && payload == "y");
  transport.beginLoop();
  assert(mqtt.loop());
  assert(callbacks == 3 && payload == "z");

  mockIncoming = {0x32, 5, 0, 1, 't', 0, 1}; // valid QoS1 empty payload + PUBACK
  transport.beginLoop();
  assert(mqtt.loop());
  assert(callbacks == 4 && payload.empty());

  mockIncoming = {0x30, 2, 255, 255}; // malformed topic length
  transport.beginLoop();
  mqtt.loop();
  assert(!mqtt.connected() && callbacks == 4);
  assert(transport.stats().rxMalformed == 1);

  // PubSubClient's synchronous CONNACK path remains bounded with a dripped header.
  mockNowMs = 0;
  mockMillisAdvance = 1;
  mockIncoming = {0x20};
  assert(!mqtt.connect("mock-device"));
  assert(mockNowMs <= 1010 && !transport.connected());
  assert(transport.pendingBytes() == 0);

  // The CONNACK wait must still receive a real LAN response after the first 1ms budget.
  mockNowMs = 0;
  mockRecvCostUs = 100;
  mockIncoming.clear();
  mockDelayedIncoming = {0x20, 2, 0, 0};
  mockDeliverAtMs = 50;
  assert(mqtt.connect("mock-device"));
  assert(mockNowMs >= 50 && mockNowMs < 1000);

  mqtt.disconnect();
  mockNowMs = 0;
  mockIncoming = {0x20};
  mockDelayedIncoming = {2, 0, 0};
  mockDeliverAtMs = 50;
  assert(mqtt.connect("mock-device"));
  assert(mockNowMs >= 50 && mockNowMs < 1000);

  // PubSubClient accepts any four-byte packet with byte3=0 as CONNACK unless gated.
  mqtt.disconnect();
  mockNowMs = 0;
  mockIncoming = {0x40, 2, 0, 0};
  assert(!mqtt.connect("mock-device"));
  assert(!transport.connected() && transport.stats().rxMalformed == 2);
  std::puts("[MOCKED!] Real PubSubClient 2.8 bounded transport integration passed.");
}
