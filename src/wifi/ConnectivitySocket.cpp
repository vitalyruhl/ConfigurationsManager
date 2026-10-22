#include "ConnectivitySocket.h"
#if CM_ENABLE_WIFI
#include <lwip/sockets.h>
#include <lwip/inet.h>
namespace cm {
namespace detail {
void ConnectivitySocket::cancel() {
  if (fd_ >= 0) {
    lwip_close(fd_);
    fd_ = -1;
  }
}
bool ConnectivitySocket::start(uint32_t target, uint16_t sequence, uint32_t nonce) {
  cancel();
  fd_ = lwip_socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  if (fd_ < 0)
    return false;
  if (lwip_fcntl(fd_, F_SETFL, O_NONBLOCK) < 0) {
    cancel();
    return false;
  }
  target_ = target;
  sequence_ = sequence;
  nonce_ = nonce;
  sockaddr_in destination{};
  destination.sin_family = AF_INET;
  destination.sin_addr.s_addr = htonl(target);
  uint8_t packet[EchoPacket::size];
  EchoPacket::request(packet, target, sequence, nonce);
  if (lwip_sendto(fd_, packet, sizeof(packet), MSG_DONTWAIT, reinterpret_cast<const sockaddr*>(&destination), sizeof(destination)) != sizeof(packet)) {
    cancel();
    return false;
  }
  return true;
}
bool ConnectivitySocket::poll() {
  if (fd_ < 0)
    return false;
  // Raw ICMP sockets can also receive unrelated packets; bound CPU and stack work per loop.
  for (unsigned budget = 0; budget < 2; ++budget) {
    uint8_t packet[128];
    sockaddr_in source{};
    socklen_t sourceSize = sizeof(source);
    const int count = lwip_recvfrom(fd_, packet, sizeof(packet), MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&source), &sourceSize);
    if (count < 0)
      break;
    if (source.sin_family == AF_INET && EchoPacket::reply(packet, static_cast<std::size_t>(count), ntohl(source.sin_addr.s_addr), target_, sequence_, nonce_))
      return true;
  }
  return false;
}
} // namespace detail
} // namespace cm
#endif
