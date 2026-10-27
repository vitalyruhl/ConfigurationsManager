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
  mockSendError = 0;
  mockSendCostUs = 0;
  mockSendSteps.clear();
  mockSendStepCursor = 0;
  mockRecvCostUs = 0;
  mockRecvMillisCost = 0;
  const int connectResult = client.connect(IPAddress(), 1883);
  assert(connectResult == 1);
  feed(client, {0x20, 2, 0, 0});
  uint8_t connack[4]{};
  assert(client.read(connack, sizeof(connack)) == 4);
}

// [MOCKED!] Continuous forward progress must not expire a nonempty FIFO epoch.
static void progressingQueueDoesNotTimeout() {
  mockResetNetwork();
  cm::detail::BoundedMqttClient client;
  connect(client);
  const int id = mockSockets.back().id;
  std::vector<uint8_t> expected(1024, 'a');
  mockSendResult = -1;
  mockSendError = EAGAIN;
  assert(client.write(expected.data(), expected.size()) == expected.size());
  for (unsigned tick = 1; tick <= 100; ++tick) {
    mockNowMs = tick * 100;
    mockSendSteps = {{64, 0}, {-1, EAGAIN}};
    mockSendStepCursor = 0;
    const size_t callsBefore = mockSendEvents.size();
    client.beginLoop();
    client.pump();
    if (!client.connected()) {
      std::fprintf(stderr, "[MOCKED!] Progressing FIFO closed at %u ms after %zu accepted bytes; txTimeouts=%u\n", mockNowMs, mockSocket(id)->acceptedBytes().size(), client.stats().txTimeouts);
    }
    assert(client.connected());
    const std::vector<uint8_t> next(64, static_cast<uint8_t>(tick));
    assert(client.write(next.data(), next.size()) == next.size());
    expected.insert(expected.end(), next.begin(), next.end());
    assert(mockSendEvents.size() - callsBefore == 2);
    assert(mockSocket(id)->acceptedBytes().size() == tick * 64);
  }
  mockSendResult = -2;
  client.beginLoop();
  client.pump();
  assert(mockSocket(id)->acceptedBytes() == expected);
  assert(client.stats().txTimeouts == 0 && client.stats().txFailures == 0);
  assert(mockCloseEvents.empty());
  std::puts("[MOCKED!] Nonempty FIFO with progress for 10 seconds: exact ordered bytes, no timeout.");
}

// [MOCKED!] Only positive sends renew the deadline, including uint32 rollover.
static void noProgressDeadlineAfterPartialSend() {
  for (uint32_t start : {100u, 0xfffffc00u}) {
    for (int transient : {EAGAIN, EWOULDBLOCK, EINTR}) {
      mockResetNetwork();
      cm::detail::BoundedMqttClient client;
      connect(client);
      const int id = mockSockets.back().id;
      mockNowMs = start;
      mockSendResult = -1;
      mockSendError = transient;
      const uint8_t bytes[] = {1, 2, 3, 4, 5, 6};
      assert(client.write(bytes, sizeof(bytes)) == sizeof(bytes));
      for (uint32_t elapsed : {4000u, 8000u, 12000u}) {
        mockNowMs = start + elapsed;
        mockSendSteps = {{1, 0}, {-1, transient}};
        mockSendStepCursor = 0;
        client.beginLoop();
        client.pump();
        assert(client.connected());
      }
      assert(mockSocket(id)->acceptedBytes() == std::vector<uint8_t>({1, 2, 3}));
      mockNowMs = start + 16999u;
      client.beginLoop();
      client.pump();
      assert(client.connected());
      assert(client.write(bytes, sizeof(bytes)) == sizeof(bytes));
      const size_t callsBefore = mockSendEvents.size();
      mockNowMs = start + 17000u;
      client.beginLoop();
      client.pump();
      assert(!client.connected());
      assert(mockSendEvents.size() == callsBefore);
      assert(mockCloseEvents == std::vector<int>({id}));
      assert(client.stats().txTimeouts == 1 && client.stats().txFailures == 1);
      assert(client.stats().txBackpressure == 5 && client.stats().txPartialWrites == 3);
    }
  }
  std::puts("[MOCKED!] Five seconds without positive send expires; enqueue/transients cannot renew; wrap safe.");
}

// [MOCKED!] These contracts target the real adapter, not a second TX implementation.
static void recoveryBoundaries() {
  mockResetNetwork();
  cm::detail::BoundedMqttClient client;
  connect(client);
  int id = mockSockets.back().id;
  mockSendSteps = {{-1, EAGAIN}};
  const uint8_t header[] = {0x30, 4, 0};
  const uint8_t rest[] = {1, 't', 'x'};
  const size_t headerResult = client.write(header, sizeof(header));
  const size_t restResult = client.write(rest, sizeof(rest));
  assert(headerResult == 3 && restResult == 3 && client.connected());
  assert(mockSendEvents.size() == 1); // Same-loop pressure is not retried by later writes.
  assert(client.stats().txBackpressure == 1 && client.stats().txFailures == 0);
  client.beginLoop();
  client.pump();
  assert(mockSocket(id)->acceptedBytes() == std::vector<uint8_t>({0x30, 4, 0, 1, 't', 'x'}));

  // Wrapped FIFO retains the unsent tail and appends the next chunk exactly once.
  connect(client);
  id = mockSockets.back().id;
  std::vector<uint8_t> first(4000, 'a');
  std::vector<uint8_t> second(3000, 'b');
  mockSendSteps = {{-1, EAGAIN}, {3000, 0}, {-1, EAGAIN}};
  const size_t firstResult = client.write(first.data(), first.size());
  assert(firstResult == first.size());
  client.beginLoop();
  client.pump();
  const size_t secondResult = client.write(second.data(), second.size());
  assert(secondResult == second.size() && client.connected());
  client.beginLoop();
  client.pump();
  first.insert(first.end(), second.begin(), second.end());
  assert(mockSocket(id)->acceptedBytes() == first);
  assert(client.stats().txPartialWrites == 1 && client.stats().txBackpressure == 3);

  // Queue overflow closes once and never leaks old bytes into a new connection.
  connect(client);
  id = mockSockets.back().id;
  std::vector<uint8_t> full(4608, 'q');
  mockSendSteps = {{-1, EAGAIN}};
  const size_t fullResult = client.write(full.data(), full.size());
  const uint8_t one = 1;
  const size_t overflowResult = client.write(&one, 1);
  assert(fullResult == full.size() && overflowResult == 0 && !client.connected());
  assert(mockSocket(id)->acceptedBytes().empty());
  assert(client.stats().txOverflows == 1 && client.stats().txFailures == 1);
  connect(client);
  const int nextId = mockSockets.back().id;
  const uint8_t ping[] = {0xc0, 0};
  const size_t newResult = client.write(ping, sizeof(ping));
  assert(nextId != id && newResult == 2);
  assert(mockSocket(nextId)->acceptedBytes() == std::vector<uint8_t>({0xc0, 0}));

  // Pending bytes with no send progress expire at five seconds, including uint32 wrap.
  for (uint32_t start : {100u, 0xfffffc00u}) {
    connect(client);
    mockNowMs = start;
    mockSendResult = -1;
    mockSendError = EAGAIN;
    const size_t queued = client.write(ping, sizeof(ping));
    assert(queued == 2);
    mockNowMs = start + 4999u;
    client.beginLoop();
    client.pump();
    const size_t appended = client.write(&one, 1);
    assert(appended == 1 && client.connected());
    mockNowMs = start + 5000u;
    client.beginLoop();
    client.pump();
    assert(!client.connected()); // Appending did not renew the no-progress deadline.
  }

  assert(client.stats().txTimeouts == 2 && client.stats().txFailures == 3);

  // Explicit stop drops only locally pending bytes and preserves cumulative counters.
  connect(client);
  id = mockSockets.back().id;
  mockSendSteps = {{-1, EAGAIN}};
  const size_t stoppedWrite = client.write(ping, sizeof(ping));
  assert(stoppedWrite == 2);
  client.stop();
  assert(mockSocket(id)->acceptedBytes().empty());
  connect(client);
  const size_t afterStop = client.write(&one, 1);
  assert(afterStop == 1 && mockSockets.back().acceptedBytes() == std::vector<uint8_t>({1}));
  assert(client.stats().txTimeouts == 2 && client.stats().txFailures == 3);

  // TX backpressure does not consume the independent RX budget.
  connect(client);
  mockSendResult = -1;
  mockSendError = EAGAIN;
  const size_t waitingTx = client.write(ping, sizeof(ping));
  assert(waitingTx == 2);
  feed(client, {0x30, 4, 0, 1, 't', 'r'});
  assert(client.available() == 6 && client.connected());
  uint8_t received[6]{};
  const int readResult = client.read(received, sizeof(received));
  assert(readResult == 6 && received[5] == 'r');

  // Positive short writes cannot busy-loop when the simulated clock stands still.
  connect(client);
  mockSendEvents.clear();
  mockSendResult = 1;
  std::vector<uint8_t> hundred(100, 'p');
  const size_t shortResult = client.write(hundred.data(), hundred.size());
  assert(shortResult == hundred.size() && client.connected());
  assert(mockSendEvents.size() == 64);
  client.pump();
  assert(mockSendEvents.size() == 64); // No budget reset through another pump.
  client.beginLoop();
  client.pump();
  assert(mockSocket(mockSockets.back().id)->acceptedBytes() == hundred);

  // Real elapsed time can exhaust the budget before the attempt limit.
  connect(client);
  mockSendEvents.clear();
  mockMicroNow = 0;
  mockSendResult = 1;
  mockSendCostUs = 250;
  const size_t timedResult = client.write(hundred.data(), hundred.size());
  assert(timedResult == hundred.size() && client.connected());
  assert(mockSendEvents.size() == 4 && mockMicroNow == 1000);
  client.pump();
  assert(mockSendEvents.size() == 4);
  client.stop();
  mockResetNetwork();
  cm::detail::BoundedMqttClient handshake;
  const int handConnected = handshake.connect(IPAddress(), 1883);
  assert(handConnected == 1);
  mockSendResult = -1;
  mockSendError = EAGAIN;
  const size_t handQueued = handshake.write(ping, sizeof(ping));
  assert(handQueued == 2);
  for (unsigned i = 0; i < 100; ++i)
    handshake.available();
  assert(mockSendEvents.size() == 1); // Polls with no elapsed epoch cannot renew TX retries.
  mockMicroNow = 1000;
  handshake.available();
  assert(mockSendEvents.size() == 2 && handshake.stats().txBackpressure == 2);
  mockSendResult = -2;
  mockMicroNow = 2000;
  handshake.available();
  assert(mockSockets.back().acceptedBytes() == std::vector<uint8_t>({0xc0, 0}));
  std::puts("[MOCKED!] TX FIFO chunks/order/wrap/overflow/new-session/age/attempt/time limits passed.");
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

  // Approved recovery change: only definitive error/zero TX closes immediately; transient cases are covered below.
  for (int result : {-1, 0}) {
    connect(client);
    mockSendResult = result;
    mockSendError = ECONNRESET;
    const uint8_t bytes[] = {0xc0, 0};
    assert(client.write(bytes, sizeof(bytes)) == 0);
    assert(!client.connected());
  }
  assert(client.stats().txFailures == 2);
  connect(client);
  const uint8_t ping[] = {0xc0, 0};
  assert(client.write(ping, sizeof(ping)) == 2);
  assert(mockLastSendFlags == MSG_DONTWAIT);
  assert(mockConnectTimeout == 100);
  assert(client.connect("broker.example", 1883) == 0);
  assert(client.stats().rejectedHosts == 1 && !client.connected());
  std::printf("[MOCKED!] Bounded MQTT transport tests passed (object bytes=%zu).\n", sizeof(client));
  recoveryBoundaries();
  progressingQueueDoesNotTimeout();
  noProgressDeadlineAfterPartialSend();
}
