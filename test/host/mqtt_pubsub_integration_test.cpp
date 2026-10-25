// [MOCKED!] Arduino clock/client and lwIP boundary. Actual PubSubClient 2.8 and CM adapter.
#include "mqtt/detail/BoundedMqttClient.h"
#include <PubSubClient.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>

// [MOCKED!] Recovery contracts were observed RED before the approved production fix.
// Fatal-error characterization stays separate from transient recovery regressions.
static void requireCase(bool condition, const char* label) {
  if (!condition) {
    std::fprintf(stderr, "[MOCKED!] Characterization failed: %s\n", label);
    std::abort();
  }
}

static int establish(cm::detail::BoundedMqttClient& transport, PubSubClient& mqtt) {
  mqtt.setServer(IPAddress(), 1883);
  mqtt.setSocketTimeout(1);
  mockIncoming = {0x20, 2, 0, 0};
  const bool connected = mqtt.connect("mock-recovery");
  requireCase(connected && transport.connected(), "initial CONNACK");
  const int id = mockSockets.back().id;
  mockSocket(id)->acceptedBytes().clear();
  mockSendEvents.clear();
  mockCloseEvents.clear();
  return id;
}

static bool serviceAt(uint32_t now, cm::detail::BoundedMqttClient& transport, PubSubClient& mqtt) {
  mockNowMs = now;
  transport.beginLoop();
  transport.pump();
  return mqtt.loop();
}

static unsigned characterizeTx(bool recoveryContract) {
  struct Case {
    const char* name;
    int result;
    int error;
    bool transient;
  };
  const Case cases[] = {
    {"EAGAIN-before-bytes", -1, EAGAIN, true},
    {"EWOULDBLOCK-before-bytes", -1, EWOULDBLOCK, true},
    {"EINTR-before-bytes", -1, EINTR, true},
    {"positive-short-write", 1, 0, true},
    {"definitive-ECONNRESET", -1, ECONNRESET, false}};
  unsigned failures = 0;
  for (const auto& test : cases) {
    mockResetNetwork();
    cm::detail::BoundedMqttClient transport;
    PubSubClient mqtt(transport);
    const int id = establish(transport, mqtt);
    mockSendSteps = {{test.result, test.error}, {-2, 0}};
    mockSendCostUs = 100;
    const bool first = mqtt.publish("t", "x");
    const bool second = mqtt.publish("t", "y");
    const size_t initialCalls = mockSendEvents.size();
    bool bounded = initialCalls <= 10 && mockMicroNow <= 1000;
    for (unsigned i = 1; i <= 8; ++i) {
      const size_t callsBefore = mockSendEvents.size();
      const uint32_t clockBefore = mockMicroNow;
      serviceAt(i, transport, mqtt);
      bounded = bounded && mockSendEvents.size() - callsBefore <= 10 &&
                mockMicroNow - clockBefore <= 1000;
    }
    // Literal MQTT PUBLISH packets: a retry must neither duplicate nor reorder bytes.
    const std::vector<uint8_t> expected{0x30, 4, 0, 1, 't', 'x', 0x30, 4, 0, 1, 't', 'y'};
    const bool alive = mqtt.connected();
    const auto& accepted = mockSocket(id)->acceptedBytes();
    const bool recovered = first && second && alive && accepted == expected &&
                           mockCloseEvents.empty() && transport.stats().txFailures == 0 && bounded;
    const bool closedCorrectly = !alive && mockCloseEvents.size() == 1 &&
                                 mockCloseEvents.front() == id && transport.stats().txFailures == 1 && bounded;
    if (!test.transient) {
      requireCase(!first && !second && closedCorrectly, test.name);
      requireCase(accepted.size() == (test.result > 0 ? 1u : 0u), "accepted prefix after close");
      requireCase(mockSendEvents.size() == 1, "no hidden immediate retry loop");
    }
    requireCase(!mockSendEvents.empty(), "publish reached socket boundary");
    const auto& firstSend = mockSendEvents.front();
    requireCase(firstSend.connection == id && firstSend.requested == 6 &&
                  firstSend.result == test.result && firstSend.error == test.error,
                "scripted error belongs to the original complete PUBLISH write");
    requireCase(transport.stats().txBackpressure == (test.error == EAGAIN || test.error == EWOULDBLOCK || test.error == EINTR ? 1u : 0u), "transient counter signature");
    requireCase(transport.stats().txPartialWrites == (test.result > 0 ? 1u : 0u), "partial-write counter signature");
    const bool contractPassed = test.transient ? recovered : closedCorrectly;
    if (recoveryContract && !contractPassed)
      ++failures;
    std::printf("[MOCKED!] TX %s: publish=%d/%d live=%d bytes=%zu calls=%zu close=%zu txFailures=%u recovery=%s bounded=%d\n",
                test.name,
                first,
                second,
                alive,
                accepted.size(),
                mockSendEvents.size(),
                mockCloseEvents.size(),
                transport.stats().txFailures,
                contractPassed ? "GREEN" : "RED",
                bounded);
  }
  return failures;
}

static void characterizeKeepalive() {
  for (unsigned scenario = 0; scenario != 4; ++scenario) {
    mockResetNetwork();
    cm::detail::BoundedMqttClient transport;
    PubSubClient mqtt(transport);
    const int id = establish(transport, mqtt);
    const bool pingSent = serviceAt(15001, transport, mqtt);
    requireCase(pingSent && mockSocket(id)->acceptedBytes() == std::vector<uint8_t>({0xc0, 0}), "real PubSub PINGREQ");
    if (scenario == 0)
      mockIncoming = {0xd0, 0};
    if (scenario == 1 || scenario == 3)
      mockIncoming = {0xd0};
    const uint32_t before = mockNowMs;
    const bool partialLoop = serviceAt(15002, transport, mqtt);
    requireCase(partialLoop && mockNowMs == before + 1, "PINGRESP path never waits for missing bytes");
    if (scenario == 1) {
      mockIncoming = {0};
      const bool completed = serviceAt(15003, transport, mqtt);
      requireCase(completed, "fragmented PINGRESP complete");
    }
    const bool alive = serviceAt(30004, transport, mqtt);
    const bool expectedAlive = scenario < 2;
    requireCase(alive == expectedAlive, "PINGRESP controls next keepalive deadline");
    requireCase(transport.stats().txFailures == 0 && transport.stats().rxTimeouts == 0, "keepalive is not adapter TX/RX timeout");
    if (!expectedAlive)
      requireCase(mqtt.state() == MQTT_CONNECTION_TIMEOUT && mockCloseEvents.size() == 1, "missing PINGRESP closes once");
    const char* names[] = {"complete", "fragmented", "absent", "header-only"};
    std::printf("[MOCKED!] PINGRESP %s: live=%d state=%d close=%zu txFailures=%u rxTimeouts=%u GREEN\n",
                names[scenario],
                alive,
                mqtt.state(),
                mockCloseEvents.size(),
                transport.stats().txFailures,
                transport.stats().rxTimeouts);
  }
}

static void characterizeLateConnackAndCounters() {
  mockResetNetwork();
  cm::detail::BoundedMqttClient transport;
  PubSubClient mqtt(transport);
  mqtt.setServer(IPAddress(), 1883);
  mqtt.setSocketTimeout(1);
  mockMillisAdvance = 1;
  mockRecvCostUs = 100;
  mockDelayedIncoming = {0x20, 2, 0, 0};
  mockDeliverAtMs = 1100;
  const bool result = mqtt.connect("mock-recovery");
  const uint32_t elapsed = mockNowMs;
  requireCase(!result && elapsed >= 1000 && elapsed <= 1010, "CONNACK beyond one second times out");
  requireCase(mqtt.state() == MQTT_CONNECTION_TIMEOUT && mockCloseEvents.size() == 1, "CONNACK timeout closes once");
  const int oldId = mockSockets.front().id;
  const std::vector<uint8_t> oldAccepted = mockSocket(oldId)->acceptedBytes();
  requireCase(!oldAccepted.empty() && oldAccepted.front() == 0x10, "CONNECT was accepted before timeout");
  requireCase(transport.stats().txFailures == 0 && transport.stats().rxTimeouts == 0, "CONNACK timeout counter signature");
  mockMillisAdvance = 0;
  mockDelayedIncoming.clear();
  const int newId = establish(transport, mqtt);
  requireCase(newId != oldId && mockSocket(oldId)->closed && mockSocket(oldId)->acceptedBytes() == oldAccepted, "reconnect preserves accepted-byte evidence for abandoned socket");
  // Evidence retention is not proof of broker delivery, FIN ordering, or actual lwIP cancellation.
  std::printf("[MOCKED!] CONNACK 1100ms: state=timeout elapsed=%u oldConnection=%d newConnection=%d acceptedConnectBytes=%zu GREEN; late broker delivery UNPROVEN\n",
              elapsed,
              oldId,
              newId,
              oldAccepted.size());

  for (unsigned expectedFailures = 1; expectedFailures <= 2; ++expectedFailures) {
    mockSendSteps = {{-1, ECONNRESET}};
    mockSendStepCursor = 0;
    const bool published = mqtt.publish("t", "x");
    const bool live = mqtt.connected();
    requireCase(!published && !live && transport.stats().txFailures == expectedFailures, "cumulative definitive TX failures");
    mockSendSteps.clear();
    mockSendStepCursor = 0;
    establish(transport, mqtt);
  }
  requireCase(transport.stats().txFailures == 2, "reconnect preserves counters");
  std::puts("[MOCKED!] Cumulative txFailures survive stop/reconnect: GREEN");
}

static void handshakeAndHealthyBurst() {
  for (bool partial : {false, true}) {
    mockResetNetwork();
    cm::detail::BoundedMqttClient transport;
    PubSubClient mqtt(transport);
    mqtt.setServer(IPAddress(), 1883);
    mqtt.setSocketTimeout(1);
    mockSendSteps = partial ? std::vector<MockSendStep>{{1, 0}, {-1, EAGAIN}, {-2, 0}} : std::vector<MockSendStep>{{-1, EAGAIN}, {-2, 0}};
    mockMillisAdvance = 1;
    mockRecvCostUs = 100;
    mockDelayedIncoming = {0x20, 2, 0, 0};
    mockDeliverAtMs = 50;
    mockDeliverAfterSentBytes = 27; // CONNECT for literal client ID "mock-recovery".
    const bool connected = mqtt.connect("mock-recovery");
    requireCase(connected && mockNowMs < 1000, "synchronous CONNECT drains after transient TX");
    requireCase(mockSockets.back().acceptedBytes().size() == 27 && mockCloseEvents.empty(), "CONNECT sent exactly once before CONNACK");
    requireCase(mockSendEvents.size() == (partial ? 3u : 2u), "handshake retry count");
  }
  mockResetNetwork();
  cm::detail::BoundedMqttClient transport;
  PubSubClient mqtt(transport);
  const int id = establish(transport, mqtt);
  const bool bufferSet = mqtt.setBufferSize(1024);
  requireCase(bufferSet, "burst buffer");
  mockSendCostUs = 10;
  const std::string payload(700, 'v');
  std::vector<uint8_t> frame{0x30, 0xc3, 5, 0, 5, 'b', 'u', 'r', 's', 't'};
  frame.insert(frame.end(), 700, 'v');
  std::vector<uint8_t> expected;
  for (unsigned i = 0; i < 32; ++i) {
    const bool published = mqtt.publish("burst", payload.c_str());
    requireCase(published, "healthy reconnect-style burst must not fill an artificial 256-byte TX limit");
    expected.insert(expected.end(), frame.begin(), frame.end());
  }
  requireCase(expected.size() > 16384 && mockSocket(id)->acceptedBytes() == expected, "healthy burst exact bytes/order");
  requireCase(transport.connected() && mockCloseEvents.empty() && mockMicroNow <= 1000, "healthy burst within modeled time budget");
  std::printf("[MOCKED!] Healthy burst: %zu bytes, %zu sends, %u us; CONNECT transient/short-write recovered\n",
              expected.size(),
              mockSendEvents.size(),
              mockMicroNow);
}

static int runRecoveryMatrix(bool recoveryContract) {
  const unsigned failures = characterizeTx(recoveryContract);
  characterizeKeepalive();
  characterizeLateConnackAndCounters();
  handshakeAndHealthyBurst();
  std::printf("[MOCKED!] Recovery contract mode=%d failures=%u adapterBytes=%zu; no heap/TCP-stack/HIL claim\n",
              recoveryContract,
              failures,
              sizeof(cm::detail::BoundedMqttClient));
  return failures ? 1 : 0;
}

int main(int argc, char** argv) {
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
  if (argc > 1 && (argc != 2 || std::strcmp(argv[1], "--recovery-contract") != 0))
    return 2;
  return runRecoveryMatrix(true);
}
