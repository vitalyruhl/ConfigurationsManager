#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cm {
namespace detail {
enum class ConnectivityTarget { Off,
                                Gateway,
                                Address };
enum class ConnectivityState { Off,
                               Waiting,
                               Reachable,
                               Unreachable,
                               Suspended };
inline bool parseConnectivityTarget(const char* text, ConnectivityTarget& mode, uint32_t& address) {
  if (!text)
    return false;
  if (std::strcmp(text, "off") == 0) {
    mode = ConnectivityTarget::Off;
    address = 0;
    return true;
  }
  if (std::strcmp(text, "gateway") == 0) {
    mode = ConnectivityTarget::Gateway;
    address = 0;
    return true;
  }
  const char* p = text;
  uint32_t parsed = 0;
  for (unsigned part = 0; part < 4; ++part) {
    const char* first = p;
    unsigned octet = 0, digits = 0;
    while (*p >= '0' && *p <= '9') {
      octet = octet * 10 + static_cast<unsigned>(*p++ - '0');
      if (++digits > 3 || octet > 255)
        return false;
    }
    if (!digits || (digits > 1 && *first == '0'))
      return false;
    parsed = (parsed << 8) | octet;
    if (part < 3) {
      if (*p++ != '.')
        return false;
    } else if (*p != '\0')
      return false;
  }
  // A zero, unspecified-network, multicast or broadcast address is not a remote unicast target.
  if ((parsed >> 24) == 0 || (parsed >> 24) >= 224)
    return false;
  mode = ConnectivityTarget::Address;
  address = parsed;
  return true;
}
inline bool isConnectivityTarget(const char* text) {
  ConnectivityTarget mode;
  uint32_t address;
  return parseConnectivityTarget(text, mode, address);
}

// Canonical address integers use network byte significance, independently of CPU byte order.
struct EchoPacket {
  static constexpr std::size_t size = 24;
  static uint16_t read16(const uint8_t* b) {
    return static_cast<uint16_t>((uint16_t(b[0]) << 8) | b[1]);
  }
  static uint32_t read32(const uint8_t* b) {
    return (uint32_t(b[0]) << 24) | (uint32_t(b[1]) << 16) | (uint32_t(b[2]) << 8) | b[3];
  }
  static void write32(uint8_t* b, uint32_t n) {
    b[0] = uint8_t(n >> 24);
    b[1] = uint8_t(n >> 16);
    b[2] = uint8_t(n >> 8);
    b[3] = uint8_t(n);
  }
  static uint16_t checksum(const uint8_t* b, std::size_t n) {
    uint32_t sum = 0;
    while (n > 1) {
      sum += read16(b);
      b += 2;
      n -= 2;
    }
    if (n)
      sum += uint16_t(*b) << 8;
    while (sum >> 16)
      sum = (sum & 0xffffu) + (sum >> 16);
    return static_cast<uint16_t>(~sum);
  }
  static void request(uint8_t* b, uint32_t target, uint16_t sequence, uint32_t nonce) {
    std::memset(b, 0, size);
    b[0] = 8;
    b[4] = 0x43;
    b[5] = 0x4d;
    b[6] = uint8_t(sequence >> 8);
    b[7] = uint8_t(sequence);
    write32(b + 8, 0x434d4e50u);
    write32(b + 12, nonce);
    write32(b + 16, ~nonce);
    write32(b + 20, target);
    const auto check = checksum(b, size);
    b[2] = uint8_t(check >> 8);
    b[3] = uint8_t(check);
  }
  static bool reply(const uint8_t* b, std::size_t n, uint32_t source, uint32_t target, uint16_t sequence, uint32_t nonce) {
    if (n < 20 || (b[0] >> 4) != 4)
      return false;
    const std::size_t ihl = (b[0] & 15u) * 4u;
    if (ihl < 20 || ihl > 60 || n < ihl + size || read16(b + 2) != ihl + size || b[9] != 1 ||
        (read16(b + 6) & 0x3fffu) != 0 || checksum(b, ihl) != 0 || source != target || read32(b + 12) != target)
      return false;
    const uint8_t* icmp = b + ihl;
    uint8_t expected[size];
    request(expected, target, sequence, nonce);
    return icmp[0] == 0 && icmp[1] == 0 && checksum(icmp, size) == 0 && std::memcmp(icmp + 4, expected + 4, size - 4) == 0;
  }
};

struct ConnectivityTransport {
  virtual ~ConnectivityTransport() = default;
  virtual bool start(uint32_t target, uint16_t sequence, uint32_t nonce) = 0;
  virtual bool poll() = 0;
  virtual void cancel() = 0;
};

// Loop-owned policy: no tasks, callbacks, heap storage or wall-clock dependency.
class ConnectivityCheck {
  ConnectivityTarget mode_ = ConnectivityTarget::Off;
  ConnectivityState state_ = ConnectivityState::Off;
  uint32_t configuredAddress_ = 0, activeTarget_ = 0, referenceAt_ = 0, lastAttempt_ = 0, probeAt_ = 0, nonce_ = 0;
  uint16_t sequence_ = 0;
  bool pending_ = false, scheduled_ = false, suspended_ = false, configurationChanged_ = false;

public:
  void seed(uint32_t nonce) {
    nonce_ = nonce;
  }
  void reset(uint32_t now) {
    referenceAt_ = now;
    activeTarget_ = 0;
    pending_ = false;
    scheduled_ = false;
    suspended_ = false;
    configurationChanged_ = true;
    state_ = enabled() ? ConnectivityState::Waiting : ConnectivityState::Off;
  }
  bool matchesTarget(const char* text) const {
    ConnectivityTarget mode;
    uint32_t address;
    return parseConnectivityTarget(text, mode, address) && mode_ == mode && configuredAddress_ == address;
  }
  bool configure(const char* text, uint32_t now) {
    ConnectivityTarget mode;
    uint32_t address;
    if (!parseConnectivityTarget(text, mode, address))
      return false;
    if (mode_ == mode && configuredAddress_ == address)
      return true;
    mode_ = mode;
    configuredAddress_ = address;
    reset(now);
    return true;
  }
  bool enabled() const {
    return mode_ != ConnectivityTarget::Off;
  }
  ConnectivityState state() const {
    return state_;
  }
  bool expired(uint32_t now, uint32_t timeout) const {
    return !suspended_ && timeout && uint32_t(now - referenceAt_) >= timeout;
  }
  uint32_t elapsed(uint32_t now) const {
    return uint32_t(now - referenceAt_);
  }
  bool tick(uint32_t now, bool stationHealthy, bool suspended, uint32_t gateway, ConnectivityTransport& wire) {
    if (configurationChanged_) {
      wire.cancel();
      configurationChanged_ = false;
    }
    if (suspended) {
      if (pending_)
        wire.cancel();
      pending_ = false;
      scheduled_ = false;
      suspended_ = true;
      state_ = ConnectivityState::Suspended;
      return false;
    }
    if (suspended_) {
      referenceAt_ = now;
      suspended_ = false;
      activeTarget_ = 0;
    }
    if (!enabled()) {
      state_ = ConnectivityState::Off;
      if (stationHealthy)
        referenceAt_ = now;
      return stationHealthy;
    }
    if (!stationHealthy) {
      if (pending_)
        wire.cancel();
      pending_ = false;
      scheduled_ = false;
      activeTarget_ = 0;
      state_ = ConnectivityState::Unreachable;
      return false;
    }
    const uint32_t target = mode_ == ConnectivityTarget::Gateway ? gateway : configuredAddress_;
    if (target != activeTarget_) {
      wire.cancel();
      pending_ = false;
      scheduled_ = false;
      activeTarget_ = target;
      state_ = ConnectivityState::Waiting;
    }
    if ((target >> 24) == 0 || (target >> 24) >= 224) {
      state_ = ConnectivityState::Unreachable;
      return false;
    }
    if (pending_) {
      if (uint32_t(now - probeAt_) >= 1000) {
        wire.cancel();
        pending_ = false;
        state_ = ConnectivityState::Unreachable;
      } else if (wire.poll()) {
        wire.cancel();
        pending_ = false;
        referenceAt_ = now;
        state_ = ConnectivityState::Reachable;
        return true;
      }
    }
    if (!pending_ && (!scheduled_ || uint32_t(now - lastAttempt_) >= 10000)) {
      scheduled_ = true;
      lastAttempt_ = now;
      probeAt_ = now;
      ++sequence_;
      ++nonce_;
      pending_ = wire.start(target, sequence_, nonce_);
      if (!pending_)
        state_ = ConnectivityState::Unreachable;
    }
    return false;
  }
};
} // namespace detail
} // namespace cm
