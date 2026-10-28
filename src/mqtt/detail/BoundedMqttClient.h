#pragma once

#include <WiFiClient.h>
#include <lwip/sockets.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include "BoundedMqttStats.h"

namespace cm {
namespace detail {

// PubSubClient sees a packet only after the complete bounded frame is validated.
// TCP reads/writes bypass WiFiClient's allocating RX buffer and retrying write().
class BoundedMqttClient : public Client {
public:
  static constexpr uint16_t MaxFrameBytes = 2304;
  static constexpr uint16_t PumpByteBudget = 256;
  static constexpr uint32_t PumpTimeBudgetUs = 1000;
  static constexpr uint32_t FrameLifetimeMs = 30000;
  static constexpr uint16_t TxCapacity = 2 * MaxFrameBytes;
  // No positive TCP send for five seconds expires pending bytes. This is not a
  // packet-age or broker-delivery deadline; enqueue and transient errors do not renew it.
  static constexpr uint32_t TxLifetimeMs = 5000;
  static constexpr uint16_t TxCallBudget = 64;
  static constexpr uint32_t TxTimeBudgetUs = 1000;

  void beginLoop() {
    budget_ = PumpByteBudget;
    timeBudgetUs_ = PumpTimeBudgetUs;
    beginTxBudget_();
  }

  void setFrameCapacity(uint16_t bytes) {
    capacity_ = std::max<uint16_t>(8, std::min<uint16_t>(bytes, MaxFrameBytes));
    if (used_ > capacity_ || total_ > capacity_)
      reject_(stats_.rxOversize);
  }

  const MqttBoundedIoStats& stats() const {
    return stats_;
  }
  uint16_t pendingBytes() const {
    return used_;
  }
  void rejectHost() {
    reject_(stats_.rejectedHosts);
  }

  int connect(IPAddress ip, uint16_t port) override {
    stop();
    beginLoop();
    const int result = socket_.connect(ip, port, 100);
    awaitingConnack_ = result != 0;
    if (!result)
      ++stats_.connectFailures;
    return result;
  }

  int connect(const char* host, uint16_t port) override {
    IPAddress ip;
    if (!host || !ip.fromString(host)) {
      rejectHost();
      return 0;
    }
    return connect(ip, port);
  }

  void pump() {
    pumpTx_();
    const uint32_t startedUs = micros();
    pump_(startedUs);
    const uint32_t elapsed = static_cast<uint32_t>(micros() - startedUs);
    timeBudgetUs_ = elapsed >= timeBudgetUs_ ? 0 : timeBudgetUs_ - elapsed;
  }

private:
  void pump_(uint32_t startedUs) {
    if (ready_)
      return;
    if (used_ && static_cast<uint32_t>(millis() - startedMs_) >= FrameLifetimeMs) {
      reject_(stats_.rxTimeouts);
      return;
    }
    const int fd = socket_.fd();
    if (fd < 0)
      return;
    while (budget_ && static_cast<uint32_t>(micros() - startedUs) < timeBudgetUs_) {
      const uint16_t wanted = total_ ? static_cast<uint16_t>(total_ - used_) : 1;
      const uint16_t count = std::min<uint16_t>(budget_, wanted);
      const int received = ::recv(fd, frame_ + used_, count, MSG_DONTWAIT);
      if (received < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
          stop();
        return;
      }
      if (received == 0) {
        stop();
        return;
      }
      if (used_ && static_cast<uint32_t>(millis() - startedMs_) >= FrameLifetimeMs) {
        reject_(stats_.rxTimeouts);
        return;
      }
      if (used_ == 0)
        startedMs_ = millis();
      budget_ -= static_cast<uint16_t>(received);
      used_ += static_cast<uint16_t>(received);
      if (!parse_())
        return;
      if (total_ && used_ == total_) {
        if (!validBody_()) {
          reject_(stats_.rxMalformed);
          return;
        }
        ready_ = true;
        awaitingConnack_ = false;
        ++stats_.rxFrames;
        return;
      }
    }
  }

public:
  int available() override {
    // PubSubClient waits synchronously here (socket timeout: 1 second), so no
    // outer CM loop can replenish the budget for a delayed/fragmented CONNACK.
    // Only this four-byte handshake frame may replenish its per-poll budget.
    if (awaitingConnack_) {
      budget_ = PumpByteBudget;
      timeBudgetUs_ = PumpTimeBudgetUs;
      // TX retries share one real-time epoch; polling cannot manufacture budget.
      if (static_cast<uint32_t>(micros() - txEpochUs_) >= TxTimeBudgetUs)
        beginTxBudget_();
    }
    pump();
    return ready_ ? used_ - cursor_ : 0;
  }
  int peek() override {
    return ready_ ? frame_[cursor_] : -1;
  }
  int read() override {
    if (!ready_)
      return -1;
    const uint8_t value = frame_[cursor_++];
    if (cursor_ == used_)
      clearFrame_();
    return value;
  }
  int read(uint8_t* output, size_t bytes) override {
    if (!ready_ || !output)
      return 0;
    const size_t count = std::min<size_t>(bytes, used_ - cursor_);
    std::memcpy(output, frame_ + cursor_, count);
    cursor_ += static_cast<uint16_t>(count);
    if (cursor_ == used_)
      clearFrame_();
    return static_cast<int>(count);
  }
  size_t write(uint8_t byte) override {
    return write(&byte, 1);
  }
  size_t write(const uint8_t* bytes, size_t count) override {
    if (!count)
      return 0;
    if (!bytes || socket_.fd() < 0)
      return 0;
    pumpTx_();
    if (socket_.fd() < 0)
      return 0;
    if (count > static_cast<size_t>(TxCapacity - txUsed_)) {
      ++stats_.txOverflows;
      reject_(stats_.txFailures);
      return 0;
    }
    if (!txUsed_)
      txProgressMs_ = millis();
    const uint16_t tail = static_cast<uint16_t>((txHead_ + txUsed_) % TxCapacity);
    const size_t first = std::min<size_t>(count, TxCapacity - tail);
    std::memcpy(tx_ + tail, bytes, first);
    std::memcpy(tx_, bytes + first, count - first);
    txUsed_ += static_cast<uint16_t>(count);
    pumpTx_();
    // Success means ownership of the complete chunk, not a broker acknowledgement.
    return socket_.fd() >= 0 ? count : 0;
  }
  // PubSubClient calls flush() before stop(); never wait for pending TCP data.
  void flush() override {}
  void stop() override {
    socket_.stop();
    clearFrame_();
    txUsed_ = txHead_ = 0;
    txProgressMs_ = 0;
    txBlocked_ = false;
    awaitingConnack_ = false;
  }
  uint8_t connected() override {
    return socket_.connected();
  }
  operator bool() override {
    return connected();
  }

private:
  WiFiClient socket_;
  uint8_t frame_[MaxFrameBytes]{};
  uint8_t tx_[TxCapacity]{};
  uint16_t txHead_ = 0;
  uint16_t txUsed_ = 0;
  uint16_t txCallsLeft_ = TxCallBudget;
  uint32_t txTimeLeftUs_ = TxTimeBudgetUs;
  uint32_t txEpochUs_ = 0;
  uint32_t txProgressMs_ = 0;
  bool txBlocked_ = false;
  MqttBoundedIoStats stats_;
  uint32_t startedMs_ = 0;
  uint32_t remaining_ = 0;
  uint32_t timeBudgetUs_ = PumpTimeBudgetUs;
  uint16_t capacity_ = 1024;
  uint16_t used_ = 0;
  uint16_t cursor_ = 0;
  uint16_t total_ = 0;
  uint16_t budget_ = PumpByteBudget;
  uint8_t headerBytes_ = 0;
  bool ready_ = false;
  bool awaitingConnack_ = false;

  void beginTxBudget_() {
    txCallsLeft_ = TxCallBudget;
    txTimeLeftUs_ = TxTimeBudgetUs;
    txEpochUs_ = micros();
    txBlocked_ = false;
  }

  bool expireTx_() {
    if (txUsed_ && static_cast<uint32_t>(millis() - txProgressMs_) >= TxLifetimeMs) {
      ++stats_.txTimeouts;
      reject_(stats_.txFailures);
      return true;
    }
    return false;
  }

  void pumpTx_() {
    if (expireTx_() || !txUsed_ || txBlocked_)
      return;
    const uint32_t startedUs = micros();
    while (txUsed_ && txCallsLeft_ && static_cast<uint32_t>(micros() - startedUs) < txTimeLeftUs_) {
      if (expireTx_())
        break;
      const uint16_t count = std::min<uint16_t>(txUsed_, TxCapacity - txHead_);
      --txCallsLeft_;
      const int sent = ::send(socket_.fd(), tx_ + txHead_, count, MSG_DONTWAIT);
      if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
        ++stats_.txBackpressure;
        txBlocked_ = true;
        break;
      }
      if (sent <= 0 || sent > count) {
        reject_(stats_.txFailures);
        break;
      }
      if (sent < count)
        ++stats_.txPartialWrites;
      txHead_ = static_cast<uint16_t>((txHead_ + sent) % TxCapacity);
      txUsed_ -= static_cast<uint16_t>(sent);
      txProgressMs_ = millis();
      if (!txUsed_) {
        txHead_ = 0;
        txProgressMs_ = 0;
      }
    }
    const uint32_t elapsed = static_cast<uint32_t>(micros() - startedUs);
    txTimeLeftUs_ = elapsed >= txTimeLeftUs_ ? 0 : txTimeLeftUs_ - elapsed;
  }

  void clearFrame_() {
    used_ = cursor_ = total_ = 0;
    remaining_ = 0;
    headerBytes_ = 0;
    startedMs_ = 0;
    ready_ = false;
  }
  void reject_(uint32_t& counter) {
    ++counter;
    stop();
  }

  bool parse_() {
    const uint8_t type = frame_[0] >> 4;
    const uint8_t flags = frame_[0] & 15;
    if (used_ == 1) {
      if (awaitingConnack_ && frame_[0] != 0x20) {
        reject_(stats_.rxMalformed);
        return false;
      }
      // Server-to-client MQTT 3.1.1 packets supported by PubSubClient 2.8.
      const bool allowed = type == 2 || type == 3 || (type >= 4 && type <= 7) ||
                           type == 9 || (type >= 11 && type <= 14);
      const bool flagsOk = type == 3 ? ((flags & 6) <= 2) : flags == (type == 6 ? 2 : 0);
      if (!allowed || !flagsOk) {
        reject_(stats_.rxMalformed);
        return false;
      }
      return true;
    }
    if (!headerBytes_) {
      const uint8_t digit = frame_[used_ - 1];
      remaining_ += static_cast<uint32_t>(digit & 127) << ((used_ - 2) * 7);
      if (digit & 128) {
        if (used_ == 5) {
          reject_(stats_.rxMalformed);
          return false;
        }
        return true;
      }
      if (used_ > 2 && digit == 0) {
        reject_(stats_.rxMalformed);
        return false;
      }
      headerBytes_ = static_cast<uint8_t>(used_);
      if (remaining_ + headerBytes_ > capacity_) {
        reject_(stats_.rxOversize);
        return false;
      }
      total_ = static_cast<uint16_t>(remaining_ + headerBytes_);
      const bool lengthOk = type == 3 ? remaining_ >= 3 : type == 9  ? remaining_ >= 3
                                                        : type >= 12 ? remaining_ == 0
                                                                     : remaining_ == 2;
      if (!lengthOk) {
        reject_(stats_.rxMalformed);
        return false;
      }
    }
    return true;
  }

  bool validBody_() const {
    if ((frame_[0] >> 4) == 2) {
      return frame_[headerBytes_] <= 1 && frame_[headerBytes_ + 1] <= 5;
    }
    if ((frame_[0] >> 4) != 3)
      return true;
    const uint16_t topicBytes = (static_cast<uint16_t>(frame_[headerBytes_]) << 8) |
                                frame_[headerBytes_ + 1];
    const uint8_t qosBytes = (frame_[0] & 6) ? 2 : 0;
    if (!topicBytes || static_cast<uint32_t>(topicBytes) + 2 + qosBytes > remaining_)
      return false;
    for (uint16_t i = 0; i < topicBytes; ++i) {
      const uint8_t c = frame_[headerBytes_ + 2 + i];
      if (!c || c == '#' || c == '+')
        return false;
    }
    if (qosBytes) {
      const uint16_t offset = headerBytes_ + 2 + topicBytes;
      if (!frame_[offset] && !frame_[offset + 1])
        return false;
    }
    return true;
  }
};

} // namespace detail
} // namespace cm
