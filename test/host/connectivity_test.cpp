#include "wifi/ConnectivityCheck.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
using cm::detail::ConnectivityCheck;
using cm::detail::ConnectivityState;
// [MOCKED!] Only raw socket admission/replies are replaced; production policy runs unchanged.
struct Wire final : cm::detail::ConnectivityTransport {
  struct Probe {
    uint32_t target;
    // Preserve the full emitted ICMP probe identity alongside target and nonce.
    // cppcheck-suppress unusedStructMember
    [[maybe_unused]] uint16_t sequence;
    uint32_t nonce;
  };
  std::vector<Probe> probes;
  bool admitted = true, ready = false, open = false;
  unsigned cancels = 0;
  bool start(uint32_t target, uint16_t sequence, uint32_t nonce) override {
    probes.push_back({target, sequence, nonce});
    open = admitted;
    return admitted;
  }
  bool poll() override {
    bool result = ready;
    ready = false;
    return result;
  }
  void cancel() override {
    open = false;
    ready = false;
    ++cancels;
  }
};
static void targets() {
  for (const char* text : {"off", "gateway", "192.168.2.126", "1.2.3.4", "223.255.255.254"})
    assert(cm::detail::isConnectivityTarget(text));
  for (const char* text : {"", "OFF", " Gateway", "gateway ", "192.168.2", "192.168.2.1x", "01.2.3.4", "1..2.3", "256.1.2.3", "1.2.3.-1", "1.2.3.4.5", "0.0.0.0", "224.1.2.3", "255.255.255.255"})
    assert(!cm::detail::isConnectivityTarget(text));
  assert(!cm::detail::isConnectivityTarget(nullptr));
}
static void offAndMissingTarget() {
  Wire w;
  ConnectivityCheck p;
  p.reset(100);
  assert(p.tick(100, true, false, 0, w));
  assert(w.probes.empty());
  assert(p.tick(60099, true, false, 0, w));
  assert(!p.expired(120098, 60000));
  assert(!p.tick(120099, false, false, 0, w));
  assert(p.expired(120099, 60000));
  assert(p.configure("gateway", 120100));
  assert(!p.tick(120100, true, false, 0, w));
  assert(w.probes.empty());
  assert(!p.tick(180100, true, false, 0, w));
  assert(p.expired(180100, 60000));
  assert(p.state() == ConnectivityState::Unreachable);
}
static void failingTargetRecoveryAndCadence() {
  Wire w;
  ConnectivityCheck p;
  p.reset(0);
  assert(p.configure("192.168.2.1", 0));
  assert(!p.tick(0, true, false, 0, w));
  assert(w.probes.size() == 1 && w.probes[0].target == 0xc0a80201u);
  assert(!p.tick(999, true, false, 0, w));
  w.ready = true;
  assert(!p.tick(1000, true, false, 0, w)); // Deadline wins over a reply first observed too late.
  assert(!p.tick(9999, true, false, 0, w));
  assert(w.probes.size() == 1);
  assert(!p.tick(10000, true, false, 0, w));
  assert(w.probes.size() == 2);
  w.ready = true;
  assert(p.tick(10001, true, false, 0, w));
  assert(p.state() == ConnectivityState::Reachable);
  assert(!p.expired(70000, 60000));
  assert(p.expired(70001, 60000));
  // Merely retaining association and a valid IP cannot renew a failed target's deadline.
  assert(!p.tick(70001, true, false, 0, w));
  assert(p.expired(70001, 60000));
  Wire denied;
  denied.admitted = false;
  ConnectivityCheck q;
  q.configure("1.2.3.4", 0);
  q.tick(0, true, false, 0, denied);
  q.tick(9999, true, false, 0, denied);
  assert(denied.probes.size() == 1);
  q.tick(10000, true, false, 0, denied);
  assert(denied.probes.size() == 2 && q.state() == ConnectivityState::Unreachable);
}
static void targetChangesAndGateway() {
  Wire w;
  ConnectivityCheck p;
  p.configure("gateway", 10);
  p.tick(10, true, false, 0xc0a80201u, w);
  assert(w.probes.size() == 1);
  const auto nonce = w.probes.back().nonce;
  assert(!p.configure("bad", 20));
  assert(p.configure("gateway", 20)); // Equal settings do not renew the health grace.
  assert(p.expired(60010, 60000));
  w.ready = true;
  p.tick(30, true, false, 0xc0a80301u, w); // A DHCP gateway change invalidates the old response.
  assert(w.probes.size() == 2 && w.probes.back().target == 0xc0a80301u && w.probes.back().nonce != nonce);
  assert(p.state() != ConnectivityState::Reachable);
  w.ready = true;
  assert(p.tick(31, true, false, 0xc0a80301u, w));
  assert(p.configure("192.168.2.126", 40));
  w.ready = true;
  assert(!p.tick(40, true, false, 0xc0a80301u, w));
  assert(w.probes.back().target == 0xc0a8027eu);
  assert(!p.expired(60039, 60000));
  assert(p.expired(60040, 60000));
  assert(p.configure("off", 60041));
  assert(p.tick(60041, true, false, 0, w));
  assert(!p.expired(120040, 60000));
}
static void suspensionAndRollover() {
  Wire w;
  ConnectivityCheck p;
  p.configure("1.2.3.4", 100);
  p.tick(100, true, false, 0, w);
  p.tick(200, true, true, 0, w);
  const auto count = w.probes.size();
  assert(!p.expired(1000000, 60000));
  p.tick(1000000, true, true, 0, w);
  assert(w.probes.size() == count && !w.open);
  assert(!p.tick(1000100, true, false, 0, w));
  assert(!p.expired(1060099, 60000));
  assert(p.expired(1060100, 60000));
  // AP has the same suspension contract, including when station association is absent.
  p.tick(1100000, false, true, 0, w);
  p.tick(1200000, false, false, 0, w);
  assert(!p.expired(1259999, 60000));
  assert(p.expired(1260000, 60000));
  ConnectivityCheck wrap;
  Wire ww;
  wrap.configure("1.2.3.4", 0xfffffff0u);
  wrap.tick(0xfffffff0u, true, false, 0, ww);
  wrap.tick(0x3d7u, true, false, 0, ww);
  assert(ww.open);
  wrap.tick(0x3d8u, true, false, 0, ww);
  assert(!ww.open);
  wrap.tick(0x26ffu, true, false, 0, ww);
  assert(ww.probes.size() == 1);
  wrap.tick(0x2700u, true, false, 0, ww);
  assert(ww.probes.size() == 2);
  assert(!wrap.expired(0xea4fu, 60000));
  assert(wrap.expired(0xea50u, 60000));
}
static void put16(uint8_t* b, uint16_t n) {
  b[0] = uint8_t(n >> 8);
  b[1] = uint8_t(n);
}
static void packetValidation() {
  using cm::detail::EchoPacket;
  uint8_t request[24];
  EchoPacket::request(request, 0xc0a80201u, 0x1234, 0x01020304u);
  assert(request[0] == 8 && request[1] == 0 && request[4] == 0x43 && request[5] == 0x4d && request[6] == 0x12 && request[7] == 0x34);
  uint8_t reply[44] = {0x45, 0, 0, 44, 0, 0, 0, 0, 64, 1, 0, 0, 192, 168, 2, 1, 192, 168, 2, 126};
  std::memcpy(reply + 20, request, 24);
  reply[20] = 0;
  reply[22] = reply[23] = 0;
  put16(reply + 22, EchoPacket::checksum(reply + 20, 24));
  put16(reply + 10, EchoPacket::checksum(reply, 20));
  assert(EchoPacket::reply(reply, 44, 0xc0a80201u, 0xc0a80201u, 0x1234, 0x01020304u));
  assert(!EchoPacket::reply(reply, 43, 0xc0a80201u, 0xc0a80201u, 0x1234, 0x01020304u));
  assert(!EchoPacket::reply(reply, 44, 0xc0a80202u, 0xc0a80201u, 0x1234, 0x01020304u));
  assert(!EchoPacket::reply(reply, 44, 0xc0a80201u, 0xc0a80201u, 0x1235, 0x01020304u));
  assert(!EchoPacket::reply(reply, 44, 0xc0a80201u, 0xc0a80201u, 0x1234, 0x01020305u));
  for (unsigned i : {0u, 2u, 6u, 9u, 12u, 20u, 21u, 24u, 26u, 28u, 32u, 40u}) {
    reply[i] ^= 1;
    assert(!EchoPacket::reply(reply, 44, 0xc0a80201u, 0xc0a80201u, 0x1234, 0x01020304u));
    reply[i] ^= 1;
  }
  // A deliberately changed payload remains invalid even with a recomputed ICMP checksum.
  reply[32] ^= 1;
  reply[22] = reply[23] = 0;
  put16(reply + 22, EchoPacket::checksum(reply + 20, 24));
  assert(!EchoPacket::reply(reply, 44, 0xc0a80201u, 0xc0a80201u, 0x1234, 0x01020304u));
}
int main() {
  targets();
  offAndMissingTarget();
  failingTargetRecoveryAndCadence();
  targetChangesAndGateway();
  suspensionAndRollover();
  packetValidation();
  std::puts("Connectivity: targets, failed-target expiry/recovery, cadence, DHCP, configuration, OTA/AP, rollover and ICMP validation passed");
}
