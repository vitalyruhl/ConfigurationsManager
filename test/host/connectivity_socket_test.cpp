#include "wifi/ConnectivitySocket.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <vector>
#include <algorithm>
// [MOCKED!] The lwIP C syscall boundary only; actual adapter and packet validation run unchanged.
using socklen_t = unsigned;
struct sockaddr {
  uint16_t sin_family;
};
struct in_addr {
  uint32_t s_addr;
};
struct sockaddr_in {
  uint16_t sin_family;
  in_addr sin_addr;
};
enum { AF_INET = 2,
       SOCK_RAW = 3,
       IPPROTO_ICMP = 1,
       F_SETFL = 4,
       O_NONBLOCK = 1,
       MSG_DONTWAIT = 8 };
static uint32_t htonl(uint32_t n) {
  return ((n & 255u) << 24) | ((n & 0xff00u) << 8) | ((n >> 8) & 0xff00u) | (n >> 24);
}
static uint32_t ntohl(uint32_t n) {
  return htonl(n);
}
static bool failOpen = false, failFlags = false, failSend = false;
static unsigned opened = 0, closed = 0, received = 0;
static uint32_t destination = 0;
static std::vector<uint8_t> sent;
static std::vector<std::vector<uint8_t>> incoming;
static int lwip_socket(int family, int type, int protocol) {
  assert(family == AF_INET && type == SOCK_RAW && protocol == IPPROTO_ICMP);
  if (failOpen)
    return -1;
  ++opened;
  return 7;
}
static int lwip_fcntl(int fd, int operation, int flags) {
  assert(fd == 7 && operation == F_SETFL && flags == O_NONBLOCK);
  return failFlags ? -1 : 0;
}
static int lwip_close(int fd) {
  assert(fd == 7);
  ++closed;
  return 0;
}
static int lwip_sendto(int fd, const void* bytes, std::size_t n, int flags, const sockaddr* to, unsigned size) {
  assert(fd == 7 && flags == MSG_DONTWAIT && size == sizeof(sockaddr_in));
  destination = ntohl(reinterpret_cast<const sockaddr_in*>(to)->sin_addr.s_addr);
  const auto* b = static_cast<const uint8_t*>(bytes);
  sent.assign(b, b + n);
  return failSend ? -1 : static_cast<int>(n);
}
static int lwip_recvfrom(int fd, void* bytes, std::size_t n, int flags, sockaddr* from, socklen_t* size) {
  assert(fd == 7 && flags == MSG_DONTWAIT && *size == sizeof(sockaddr_in));
  ++received;
  if (incoming.empty())
    return -1;
  auto packet = std::move(incoming.front());
  incoming.erase(incoming.begin());
  const auto count = std::min(n, packet.size());
  std::memcpy(bytes, packet.data(), count);
  auto* source = reinterpret_cast<sockaddr_in*>(from);
  source->sin_family = AF_INET;
  source->sin_addr.s_addr = htonl(0xc0a80201u);
  return static_cast<int>(count);
}
#include "connectivity-socket.inc"
int main() {
  cm::detail::ConnectivitySocket io;
  failOpen = true;
  assert(!io.start(0xc0a80201u, 1, 1));
  assert(opened == 0 && closed == 0);
  failOpen = false;
  failFlags = true;
  assert(!io.start(0xc0a80201u, 1, 1));
  assert(opened == 1 && closed == 1);
  failFlags = false;
  failSend = true;
  assert(!io.start(0xc0a80201u, 1, 1));
  assert(opened == 2 && closed == 2);
  failSend = false;
  assert(io.start(0xc0a80201u, 0x1234, 0x01020304u));
  assert(destination == 0xc0a80201u && sent.size() == 24 && cm::detail::EchoPacket::checksum(sent.data(), sent.size()) == 0);
  std::vector<uint8_t> reply = {0x45, 0, 0, 44, 0, 0, 0, 0, 64, 1, 0, 0, 192, 168, 2, 1, 192, 168, 2, 126};
  reply.insert(reply.end(), sent.begin(), sent.end());
  reply[20] = reply[22] = reply[23] = 0;
  auto check = cm::detail::EchoPacket::checksum(reply.data() + 20, 24);
  reply[22] = uint8_t(check >> 8);
  reply[23] = uint8_t(check);
  check = cm::detail::EchoPacket::checksum(reply.data(), 20);
  reply[10] = uint8_t(check >> 8);
  reply[11] = uint8_t(check);
  incoming = {{0}, {1}, reply};
  assert(!io.poll() && received == 2);
  assert(io.poll() && received == 3);
  io.cancel();
  assert(closed == 3);
  const auto before = received;
  assert(!io.poll() && received == before);
  io.cancel();
  assert(closed == 3);
  assert(io.start(0xc0a80201u, 0x1235, 0x01020305u));
  incoming = {reply};
  assert(!io.poll()); // Previous probe cannot satisfy a new socket.
  io.cancel();
  assert(opened == closed);
  std::puts("Connectivity actual lwIP adapter: nonblocking calls, bounded receive, stale rejection and failed-admission cleanup passed");
}
