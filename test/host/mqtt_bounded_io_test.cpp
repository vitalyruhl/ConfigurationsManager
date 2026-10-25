// [MOCKED!] Clock, ESP32 Client, and lwIP syscall boundaries; production transport runs unchanged.
#include "mqtt_stubs/WiFiClient.h"
#include "mqtt/detail/BoundedMqttClient.h"
#include <cassert>
#include <cstdio>
#include <vector>

static void feed(cm::detail::BoundedMqttClient& client, std::initializer_list<uint8_t> bytes) {
  mockIncoming.insert(mockIncoming.end(), bytes);
  client.beginLoop();
  client.pump();
}

static void connect(cm::detail::BoundedMqttClient& client) {
  mockIncoming.clear();
  mockNowMs = 0;
  mockSendResult = -2;
  mockRecvCostUs = 0;
  mockRecvMillisCost = 0;
  const int connectResult = client.connect(IPAddress(), 1883);
  assert(connectResult == 1);
  feed(client, {0x20, 2, 0, 0});
  uint8_t connack[4]{};
  assert(client.read(connack, sizeof(connack)) == 4);
}

int main() {
  cm::detail::BoundedMqttClient client;
  connect(client);
  // A fragmented PUBLISH must remain invisible until payload completion.
  feed(client, {0x30, 4, 0, 1, 't'});
  assert(client.available() == 0);
  assert(client.read() == -1);
  feed(client, {'x'});
  assert(client.available() == 6);
  uint8_t packet[6]{};
  assert(client.read(packet, sizeof(packet)) == 6);
  assert(packet[5] == 'x' && client.available() == 0);
  assert(client.stats().rxFrames == 2);

  // Per-byte activity must not renew the total 30-second frame deadline.
  connect(client);
  feed(client, {0x30});
  mockNowMs = 29999;
  feed(client, {4, 0, 1, 't'});
  assert(client.connected());
  mockNowMs = 30000;
  feed(client, {'x'});
  assert(!client.connected() && client.stats().rxTimeouts == 1);
  assert(client.available() == 0);

  // Header-only drip and uint32 clock rollover use the same first-byte deadline.
  connect(client);
  mockNowMs = 0xfffffff0u;
  feed(client, {0x30});
  mockNowMs = static_cast<uint32_t>(0xfffffff0u + 30000u);
  client.beginLoop();
  client.pump();
  assert(!client.connected() && client.stats().rxTimeouts == 2);

  // Crossing the deadline during a pump must reject even an immediately finishing frame.
  connect(client);
  feed(client, {0x30});
  mockNowMs = 29999;
  mockRecvMillisCost = 1;
  feed(client, {4, 0, 1, 't', 'x'});
  assert(!client.connected() && client.stats().rxTimeouts == 3);

  // Repeated available() calls share the same per-loop time budget.
  connect(client);
  mockRecvCostUs = 1000;
  feed(client, {0x30, 4, 0, 1, 't', 'x'});
  assert(client.pendingBytes() == 1 && client.available() == 0);
  client.beginLoop();
  client.pump();
  assert(client.pendingBytes() == 2 && client.available() == 0);
  client.beginLoop();
  client.pump();
  assert(client.available() == 6);

  // Oversized length is rejected before reading or allocating the payload.
  connect(client);
  client.setFrameCapacity(16);
  feed(client, {0x30, 127});
  assert(!client.connected() && client.stats().rxOversize == 1);

  // Malformed lengths, flags, topics and missing QoS packet IDs never reach PubSubClient.
  const std::vector<std::vector<uint8_t>> malformed = {
    {0x30, 0}, {0x30, 2, 0, 99}, {0x32, 3, 0, 1, 't'}, {0x36, 0}, {0x20, 1, 0}, {0xd1, 0}, {0x30, 0x80, 0x80, 0x80, 0x80}, {0x30, 0x80, 0}, {0x30, 3, 0, 1, 0}};
  uint32_t expectedMalformed = 0;
  for (const auto& bytes : malformed) {
    connect(client);
    mockIncoming = bytes;
    client.beginLoop();
    client.pump();
    assert(!client.connected());
    ++expectedMalformed;
    assert(client.stats().rxMalformed == expectedMalformed);
  }

  // The pump handles at most 256 input bytes in one application-loop budget.
  connect(client);
  client.setFrameCapacity(2304);
  mockIncoming = {0x30, 0x82, 0x02, 0, 1, 't'}; // 258-byte body, 261-byte packet
  mockIncoming.resize(261, 'x');
  client.beginLoop();
  client.pump();
  assert(client.available() == 0 && client.pendingBytes() == 256);
  client.beginLoop();
  client.pump();
  assert(client.available() == 261);
  client.stop();
  assert(client.pendingBytes() == 0 && client.available() == 0);

  // Partial/EAGAIN/error TX closes the socket; no frame continuation is possible.
  for (int result : {1, -1, 0}) {
    connect(client);
    mockSendResult = result;
    const uint8_t bytes[] = {0xc0, 0};
    assert(client.write(bytes, sizeof(bytes)) == 0);
    assert(!client.connected());
  }
  assert(client.stats().txFailures == 3);
  connect(client);
  const uint8_t ping[] = {0xc0, 0};
  assert(client.write(ping, sizeof(ping)) == 2);
  assert(mockLastSendFlags == MSG_DONTWAIT);
  assert(mockConnectTimeout == 100);
  assert(client.connect("broker.example", 1883) == 0);
  assert(client.stats().rejectedHosts == 1 && !client.connected());
  std::printf("[MOCKED!] Bounded MQTT transport tests passed (object bytes=%zu).\n", sizeof(client));
}
